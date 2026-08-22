#include "CollisionSystem.h"
#include "GamePlayScene.h"
#include "../../Player/Player.h"
#include "../../Enemy/Enemy.h"
#include "../../Enemy/MovingEnemy.h"
#include "Obstacle.h"

#include "Application/Particle/AppParticleManager.h"
#include "ParticleManager.h"
#include "Baziru3_Engine/Framework/Effect/HitEffect.h"
#include "Baziru3_Engine/Framework/Collision/CollisionManager.h"
#include "Baziru3_Engine/Framework/Collision/SphereCollider.h"
#include "Baziru3_Engine/Framework/Collision/BoxCollider.h"
#include "Baziru3_Engine/Framework/Collision/CapsuleCollider.h"
#include "CombatSystem.h"
#include <cmath>
#include <algorithm>

// 弾丸の移動線分 (start -> end, 半径 bulletRadius) とキャラクターの直立円柱 (center, height, radius) との精密連続衝突判定 (CCD)
static bool CheckBulletCapsuleHit(const Vector3& start, const Vector3& end, float bulletRadius, const Vector3& charPos, float charHeight, float charRadius, Vector3& outHitPoint)
{
	float dx = end.x - start.x;
	float dz = end.z - start.z;
	float lenSq = dx * dx + dz * dz;

	float t = 0.0f;
	if (lenSq > 1e-6f)
	{
		float vx = charPos.x - start.x;
		float vz = charPos.z - start.z;
		t = (vx * dx + vz * dz) / lenSq;
		t = (std::max)(0.0f, (std::min)(1.0f, t));
	}

	// 線分上の最近接 3D 座標
	Vector3 closestPoint = {
		start.x + dx * t,
		start.y + (end.y - start.y) * t,
		start.z + dz * t
	};

	// Y軸（高さ）範囲チェック (足元 charPos.y 〜 頭上 charPos.y + charHeight)
	if (closestPoint.y < charPos.y - bulletRadius || closestPoint.y > charPos.y + charHeight + bulletRadius)
	{
		return false;
	}

	// XZ平面での距離チェック
	float distX = closestPoint.x - charPos.x;
	float distZ = closestPoint.z - charPos.z;
	float distSq = distX * distX + distZ * distZ;

	float totalRadius = charRadius + bulletRadius;
	if (distSq <= totalRadius * totalRadius)
	{
		outHitPoint = closestPoint;
		return true;
	}
	return false;
}

// コンストラクタ
CollisionSystem::CollisionSystem(GamePlayScene* scene)
	: scene_(scene)
{
}

// 衝突判定とキャラクターの位置補正を毎フレーム実行
void CollisionSystem::Update()
{
	CollisionManager::GetInstance()->Update(); // エンジンの衝突判定更新
	ResolveBulletCollisions();      // 弾丸とキャラクターの衝突 (CCD精密判定)
	ResolveObstacleCollisions();    // 弾丸と障害物の衝突 (Raycastによる精密判定)
	ResolveCharacterObstacleCollisions(); // キャラクターと障害物の衝突
	ResolveContactDamage();        // プレイヤーと敵の直接接触によるダメージ
}

// 弾丸とプレイヤー・敵の精密衝突判定 (CCD + 直立円柱ヒットボックス)
void CollisionSystem::ResolveBulletCollisions()
{
	if (!scene_->combatSystem_) return;

	auto& bullets = scene_->combatSystem_->GetBullets();

	const float kBulletRadius = 0.12f;      // 弾丸の有効半径 (12cm)
	const float kDuckHeight = 1.35f;        // ダックキャラクターの身長
	const float kDuckRadius = 0.42f;        // ダックキャラクターの胴体半径

	for (auto& bullet : bullets)
	{
		if (!bullet || bullet->IsDead())
		{
			continue;
		}

		const Vector3 bPosPrev = bullet->GetPrevPosition();
		const Vector3 bPosCurrent = bullet->GetPosition();

		if (bullet->GetOwner() == BulletOwner::Player)
		{
			// --- プレイヤーの弾丸と固定敵の精密衝突判定 ---
			if (scene_->enemy_ && !scene_->enemy_->IsDead())
			{
				const Vector3 enemyPos = scene_->enemy_->GetPosition();
				Vector3 hitPoint;
				if (CheckBulletCapsuleHit(bPosPrev, bPosCurrent, kBulletRadius, enemyPos, kDuckHeight, kDuckRadius, hitPoint))
				{
					int prevHp = scene_->enemy_->GetHP();
					scene_->enemy_->OnHit(scene_->player_ ? scene_->player_->GetPosition() : Vector3{0.0f,0.0f,0.0f});
					int dmg = prevHp - scene_->enemy_->GetHP();
					if (dmg > 0)
					{
						bool isCritical = (rand() % 100 < 30);
						std::string text = std::to_string(dmg);
						Vector4 color = isCritical ? Vector4{ 1.0f, 0.9f, 0.0f, 1.0f } : Vector4{ 1.0f, 1.0f, 1.0f, 1.0f };
						if (isCritical) text += "!";
						scene_->AddFloatingText(hitPoint + Vector3{ 0.0f, 0.8f, 0.0f }, text, color, isCritical);

						// 🩸 リアル＆バイオレントな被弾指向性血しぶきスプラッター
						if (scene_->particleManager && scene_->appParticleManager_)
						{
							Vector3 bulletDir = bPosCurrent - bPosPrev;
							float blen = std::sqrt(bulletDir.x * bulletDir.x + bulletDir.y * bulletDir.y + bulletDir.z * bulletDir.z);
							if (blen > 1e-4f) bulletDir = { bulletDir.x / blen, bulletDir.y / blen, bulletDir.z / blen };

							scene_->appParticleManager_->EmitViolentBloodSpray(
								scene_->particleManager->GetRandomEngine(),
								hitPoint,
								bulletDir,
								isCritical,
								scene_->bloodTextureIndex_,
								scene_->smokeTextureIndex_
							);
						}

						if (scene_->enemy_->IsDead())
						{
							scene_->TriggerHitStop(0.14f);
						}
						else if (isCritical)
						{
							scene_->TriggerHitStop(0.06f);
						}
					}

					if (scene_->enemy_->IsDead())
					{
						scene_->TriggerCameraShake(0.6f, 1.4f);
						if (scene_->hitEffect_)
						{
							scene_->hitEffect_->Play(enemyPos);
							scene_->hitEffect_->SpawnPlaneParticles(enemyPos);
						}
						// 🩸 敵死亡時のバイオレントな大スプラッター＆血煙大爆発 (Violent Blood Burst)
						if (scene_->particleManager && scene_->appParticleManager_)
						{
							Vector3 hitDir = bPosCurrent - bPosPrev;
							float hlen = std::sqrt(hitDir.x * hitDir.x + hitDir.y * hitDir.y + hitDir.z * hitDir.z);
							if (hlen > 1e-4f) hitDir = { hitDir.x / hlen, hitDir.y / hlen, hitDir.z / hlen };

							scene_->appParticleManager_->EmitViolentBloodBurst(
								scene_->particleManager->GetRandomEngine(),
								enemyPos,
								hitDir,
								scene_->bloodTextureIndex_,
								scene_->smokeTextureIndex_,
								scene_->starburstTextureIndex_
							);
						}
					}
					bullet->Finalize();
					continue;
				}
			}

			// --- プレイヤーの弾丸と移動敵の精密衝突判定 ---
			if (scene_->movingEnemy_ && !scene_->movingEnemy_->IsDead())
			{
				const Vector3 enemyPos = scene_->movingEnemy_->GetPosition();
				Vector3 hitPoint;
				if (CheckBulletCapsuleHit(bPosPrev, bPosCurrent, kBulletRadius, enemyPos, kDuckHeight, kDuckRadius, hitPoint))
				{
					int prevHp = scene_->movingEnemy_->GetHP();
					scene_->movingEnemy_->OnHit(scene_->player_ ? scene_->player_->GetPosition() : Vector3{0.0f,0.0f,0.0f});
					int dmg = prevHp - scene_->movingEnemy_->GetHP();
					if (dmg > 0)
					{
						bool isCritical = (rand() % 100 < 30);
						std::string text = std::to_string(dmg);
						Vector4 color = isCritical ? Vector4{ 1.0f, 0.9f, 0.0f, 1.0f } : Vector4{ 1.0f, 1.0f, 1.0f, 1.0f };
						if (isCritical) text += "!";
						scene_->AddFloatingText(hitPoint + Vector3{ 0.0f, 0.8f, 0.0f }, text, color, isCritical);

						// 🩸 リアル＆バイオレントな被弾指向性血しぶきスプラッター
						if (scene_->particleManager && scene_->appParticleManager_)
						{
							Vector3 bulletDir = bPosCurrent - bPosPrev;
							float blen = std::sqrt(bulletDir.x * bulletDir.x + bulletDir.y * bulletDir.y + bulletDir.z * bulletDir.z);
							if (blen > 1e-4f) bulletDir = { bulletDir.x / blen, bulletDir.y / blen, bulletDir.z / blen };

							scene_->appParticleManager_->EmitViolentBloodSpray(
								scene_->particleManager->GetRandomEngine(),
								hitPoint,
								bulletDir,
								isCritical,
								scene_->bloodTextureIndex_,
								scene_->smokeTextureIndex_
							);
						}

						if (scene_->movingEnemy_->IsDead())
						{
							scene_->TriggerHitStop(0.14f);
						}
						else if (isCritical)
						{
							scene_->TriggerHitStop(0.06f);
						}
					}

					if (scene_->movingEnemy_->IsDead())
					{
						scene_->TriggerCameraShake(0.6f, 1.4f);
						if (scene_->hitEffect_)
						{
							scene_->hitEffect_->Play(enemyPos);
							scene_->hitEffect_->SpawnPlaneParticles(enemyPos);
						}
						// 🩸 敵死亡時のバイオレントな大スプラッター＆血煙大爆発 (Violent Blood Burst)
						if (scene_->particleManager && scene_->appParticleManager_)
						{
							Vector3 hitDir = bPosCurrent - bPosPrev;
							float hlen = std::sqrt(hitDir.x * hitDir.x + hitDir.y * hitDir.y + hitDir.z * hitDir.z);
							if (hlen > 1e-4f) hitDir = { hitDir.x / hlen, hitDir.y / hlen, hitDir.z / hlen };

							scene_->appParticleManager_->EmitViolentBloodBurst(
								scene_->particleManager->GetRandomEngine(),
								enemyPos,
								hitDir,
								scene_->bloodTextureIndex_,
								scene_->smokeTextureIndex_,
								scene_->starburstTextureIndex_
							);
						}
					}
					bullet->Finalize();
					continue;
				}
			}

			// --- プレイヤーの弾丸と的（Target）の精密衝突判定 ---
			for (auto& target : scene_->GetTargets())
			{
				if (target && !target->IsDead())
				{
					const Vector3 targetPos = target->GetPosition();
					Vector3 hitPoint;
					if (CheckBulletCapsuleHit(bPosPrev, bPosCurrent, kBulletRadius, targetPos, 1.5f, target->GetRadius(), hitPoint))
					{
						int prevHp = target->GetHP();
						target->OnHit(1);
						int dmg = prevHp - target->GetHP();
						if (dmg > 0)
						{
							bool isCritical = (rand() % 100 < 30);
							std::string text = std::to_string(dmg);
							Vector4 color = isCritical ? Vector4{ 1.0f, 0.9f, 0.0f, 1.0f } : Vector4{ 1.0f, 1.0f, 1.0f, 1.0f };
							if (isCritical) text += "!";
							scene_->AddFloatingText(hitPoint + Vector3{ 0.0f, 0.8f, 0.0f }, text, color, isCritical);

							scene_->TriggerHitStop(0.04f);
						}

						if (target->IsDead())
						{
							scene_->TriggerCameraShake(0.35f, 0.6f);
							if (scene_->hitEffect_)
							{
								scene_->hitEffect_->Play(targetPos);
								scene_->hitEffect_->SpawnPlaneParticles(targetPos);
							}
							if (scene_->particleManager && scene_->appParticleManager_)
							{
								scene_->appParticleManager_->EmitDeathFlash(scene_->particleManager->GetRandomEngine(), targetPos, 4.0f, { 1.0f, 0.8f, 0.2f, 1.0f }, 0.35f, scene_->starburstTextureIndex_);

								for (int i = 0; i < 40; ++i)
								{
									scene_->appParticleManager_->EmitFeather(scene_->particleManager->GetRandomEngine(), targetPos, { 1.0f, 0.9f, 0.6f, 1.0f }, scene_->particleTextureB);
								}
								for (int i = 0; i < 20; ++i)
								{
									scene_->appParticleManager_->EmitSpark(scene_->particleManager->GetRandomEngine(), targetPos, {0,0,0}, { 1.0f, 0.6f, 0.2f, 1.0f }, 0.15f, 1.2f, scene_->particleTextureB);
								}
							}
							scene_->AddFloatingText(targetPos + Vector3{ 0.0f, 1.5f, 0.0f }, "TARGET DESTROYED", { 0.0f, 1.0f, 0.5f, 1.0f }, true);
						}

						bullet->Finalize();
						break;
					}
				}
			}
			if (bullet->IsDead()) continue;
		}
		else if (bullet->GetOwner() == BulletOwner::Enemy)
		{
			// --- 敵の弾丸とプレイヤーの精密衝突判定 ---
			if (scene_->player_ && !scene_->player_->IsDead())
			{
				const Vector3 playerPos = scene_->player_->GetPosition();
				Vector3 hitPoint;
				if (CheckBulletCapsuleHit(bPosPrev, bPosCurrent, kBulletRadius, playerPos, kDuckHeight, kDuckRadius, hitPoint))
				{
					float prevHp = scene_->player_->GetHP();
					scene_->player_->TakeDamage(20.0f);
					if (scene_->player_->GetHP() < prevHp)
					{
						scene_->TriggerCameraShake(0.25f, 0.5f); // 被弾カメラシェイク
						scene_->TriggerHitStop(0.04f);           // 被弾時の短いスローモーション
						scene_->AddFloatingText(playerPos + Vector3{ 0.0f, 1.0f, 0.0f }, "WARNING -20", { 1.0f, 0.1f, 0.1f, 1.0f }, true);

						if (scene_->particleManager && scene_->appParticleManager_)
						{
							for (int i = 0; i < 20; ++i)
							{
								scene_->appParticleManager_->EmitFeather(scene_->particleManager->GetRandomEngine(), playerPos, { 1.0f, 1.0f, 1.0f, 1.0f }, scene_->particleTextureB);
							}
						}
					}
					bullet->Finalize();
					continue;
				}
			}
		}
	}
}

// 弾丸と障害物（Obstacle）の精密衝突判定 (MeshColliderを使用したポリゴン精密判定)
void CollisionSystem::ResolveObstacleCollisions()
{
	if (!scene_->combatSystem_) return;
	auto& bullets = scene_->combatSystem_->GetBullets();

	for (auto& bullet : bullets)
	{
		if (!bullet || bullet->IsDead()) continue;
		Vector3 bPosPrev = bullet->GetPrevPosition();
		Vector3 bPos = bullet->GetPosition();
		Vector3 diff = bPos - bPosPrev;
		float len = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
		if (len < 1e-4f) continue;
		Vector3 dir = { diff.x / len, diff.y / len, diff.z / len };

		Collider* hitCollider = nullptr;
		float hitDist = len;
		const uint32_t kObstacleMask = (1 << static_cast<uint32_t>(CollisionAttribute::Obstacle));

		if (CollisionManager::GetInstance()->Raycast(bPosPrev, dir, len, hitCollider, hitDist, kObstacleMask))
		{
			// 着弾交点
			Vector3 hitWorldPos = bPosPrev + dir * hitDist;
			bPos = hitWorldPos; // エフェクト発生座標を交点に設定

			// 木製の障害物に着弾した際のおがくず（飛散）パーティクル演出
			if (scene_->particleManager && scene_->appParticleManager_)
			{
				scene_->appParticleManager_->EmitMuzzleFlare(
					scene_->particleManager->GetRandomEngine(),
					bPos,
					0.32f,
					{ 1.0f, 0.9f, 0.6f, 0.9f },
					0.06f,
					scene_->particleTextureB
				);

				// 木片（フェンス素材テクスチャ）のバースト
				for (int i = 0; i < 8; ++i)
				{
					std::uniform_real_distribution<float> colorDist(0.0f, 0.15f);
					float r = 0.5f + colorDist(scene_->particleManager->GetRandomEngine());
					float g = 0.35f + colorDist(scene_->particleManager->GetRandomEngine());
					float b = 0.15f + colorDist(scene_->particleManager->GetRandomEngine());
					
					std::uniform_real_distribution<float> chipScale(0.1f, 0.25f);
					scene_->appParticleManager_->EmitSpark(
						scene_->particleManager->GetRandomEngine(),
						bPos,
						{0, 0, 0},
						{ r, g, b, 1.0f },
						chipScale(scene_->particleManager->GetRandomEngine()),
						0.5f,
						scene_->fenceTextureIndex_
					);
				}
				
				// 細かいおがくずの浮遊煙
				for (int i = 0; i < 6; ++i)
				{
					std::uniform_real_distribution<float> colorDist(0.0f, 0.1f);
					float r = 0.7f + colorDist(scene_->particleManager->GetRandomEngine());
					float g = 0.55f + colorDist(scene_->particleManager->GetRandomEngine());
					float b = 0.35f + colorDist(scene_->particleManager->GetRandomEngine());
					
					std::uniform_real_distribution<float> velDist(-2.0f, 2.0f);
					std::uniform_real_distribution<float> velUp(1.0f, 3.0f);
					Vector3 vel = { velDist(scene_->particleManager->GetRandomEngine()), velUp(scene_->particleManager->GetRandomEngine()), velDist(scene_->particleManager->GetRandomEngine()) };

					scene_->appParticleManager_->EmitDustWithVelocity(
						scene_->particleManager->GetRandomEngine(),
						bPos,
						0.6f,
						{ r, g, b, 0.8f },
						vel,
						1.3f,
						scene_->fenceTextureIndex_
					);
				}
			}

			bullet->Finalize();
		}
	}
}

// プレイヤーと敵の直接接触時の接触ダメージ判定と適用
void CollisionSystem::ResolveContactDamage()
{
	if (!scene_->player_ || !scene_->enemy_ || scene_->enemy_->IsDead() || scene_->player_->IsDead())
	{
		return;
	}

	// 固定敵との接触ダメージ
	if (scene_->IsWithinRadius(scene_->player_->GetPosition(), scene_->enemy_->GetPosition(), scene_->playerHitRadius_ + scene_->enemyHitRadius_))
	{
		float prevHp = scene_->player_->GetHP();
		scene_->player_->TakeDamage(scene_->kContactDamage);
		if (scene_->player_->GetHP() < prevHp)
		{
			scene_->vignetteAlpha_ = 0.6f;
			scene_->TriggerCameraShake(0.25f, 0.5f);
			scene_->TriggerHitStop(0.06f); // 接触被弾時のスローモーション
			scene_->AddFloatingText(scene_->player_->GetPosition() + Vector3{0.0f, 1.0f, 0.0f}, "WARNING -20", {1.0f, 0.1f, 0.1f, 1.0f}, true);
		}
	}

	// 移動敵との接触ダメージ
	if (scene_->movingEnemy_ && !scene_->movingEnemy_->IsDead())
	{
		if (scene_->IsWithinRadius(scene_->player_->GetPosition(), scene_->movingEnemy_->GetPosition(), scene_->playerHitRadius_ + scene_->enemyHitRadius_))
		{
			float prevHp = scene_->player_->GetHP();
			scene_->player_->TakeDamage(scene_->kContactDamage);
			if (scene_->player_->GetHP() < prevHp)
			{
				scene_->vignetteAlpha_ = 0.6f;
				scene_->TriggerCameraShake(0.25f, 0.5f);
				scene_->TriggerHitStop(0.06f); // 接触被弾時のスローモーション
				scene_->AddFloatingText(scene_->player_->GetPosition() + Vector3{0.0f, 1.0f, 0.0f}, "WARNING -20", {1.0f, 0.1f, 0.1f, 1.0f}, true);
			}
		}
	}
}

// プレイヤーおよび移動敵と障害物(BoxCollider)との手動めり込み解決（押し出し補正）
void CollisionSystem::ResolveCharacterObstacleCollisions()
{
	// 最適化: キャラクターと障害物の衝突（押し出し解決）は、
	// エンジン側の自動物理システム（CollisionManager::Update）に完全に移譲されたため、
	// 二重処理防止のためにゲーム側の手動ループは廃止されました。
}
