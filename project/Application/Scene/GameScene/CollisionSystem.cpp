#include "CollisionSystem.h"
#include "GamePlayScene.h"
#include "../../Player/Player.h"
#include "../../Enemy/Enemy.h"
#include "../../Enemy/MovingEnemy.h"
#include "../../Obstacle.h"
#include "Application/Particle/AppParticleManager.h"
#include "ParticleManager.h"
#include "Baziru3_Engine/Effect/HitEffect.h"
#include "Baziru3_Engine/Collision/CollisionManager.h"
#include "Baziru3_Engine/Collision/SphereCollider.h"
#include "Baziru3_Engine/Collision/BoxCollider.h"
#include "Baziru3_Engine/Collision/CapsuleCollider.h"
#include "CombatSystem.h"
#include <cmath>
#include <algorithm>

// コンストラクタ
CollisionSystem::CollisionSystem(GamePlayScene* scene)
	: scene_(scene)
{
}

// 衝突判定とキャラクターの位置補正を毎フレーム実行
void CollisionSystem::Update()
{
	CollisionManager::GetInstance()->Update(); // エンジンの衝突判定更新
	ResolveBulletCollisions();      // 弾丸とキャラクターの衝突
	ResolveObstacleCollisions();    // 弾丸と障害物の衝突 (MeshColliderによる精密判定)
	ResolveCharacterObstacleCollisions(); // キャラクターと障害物の衝突 (手動押し出し解決)
	ResolveContactDamage();        // プレイヤーと敵の直接接触によるダメージ
}

// 弾丸とプレイヤー・敵の衝突判定およびダメージ適用
void CollisionSystem::ResolveBulletCollisions()
{
	if (!scene_->combatSystem_) return;

	auto& bullets = scene_->combatSystem_->GetBullets();

	for (auto& bullet : bullets)
	{
		if (!bullet || bullet->IsDead())
		{
			continue;
		}

		const Vector3 bulletPos = bullet->GetPosition();
		if (bullet->GetOwner() == BulletOwner::Player)
		{
			// --- プレイヤーの弾丸と固定敵の衝突判定 ---
			if (scene_->enemy_ && !scene_->enemy_->IsDead())
			{
				const Vector3 enemyPos = scene_->enemy_->GetPosition();
				if (scene_->IsWithinRadius(bulletPos, enemyPos, scene_->bulletHitRadius_ + scene_->enemyHitRadius_))
				{
					int prevHp = scene_->enemy_->GetHP();
					scene_->enemy_->OnHit(scene_->player_ ? scene_->player_->GetPosition() : Vector3{0.0f,0.0f,0.0f});
					int dmg = prevHp - scene_->enemy_->GetHP();
					if (dmg > 0)
					{
						// クリティカル判定（30%の確率）とダメージ数表示（浮遊テキスト）の追加
						bool isCritical = (rand() % 100 < 30);
						std::string text = std::to_string(dmg);
						Vector4 color = isCritical ? Vector4{ 1.0f, 0.9f, 0.0f, 1.0f } : Vector4{ 1.0f, 1.0f, 1.0f, 1.0f };
						if (isCritical) text += "!";
						scene_->AddFloatingText(enemyPos + Vector3{ 0.0f, 1.2f, 0.0f }, text, color, isCritical);

						// ヒットストップをトリガー（通常ヒットはカクつき防止のため行わず、クリティカル・死亡時のみに限定）
						if (scene_->enemy_->IsDead())
						{
							scene_->TriggerHitStop(0.12f); // 撃破時は長めのスロー
						}
						else if (isCritical)
						{
							scene_->TriggerHitStop(0.05f); // クリティカル時は一瞬のスロー
						}
					}

					// 敵死亡時の派手な羽・フラッシュ・煙のバーストエフェクトとカメラ揺らし
					if (scene_->enemy_->IsDead())
					{
						scene_->TriggerCameraShake(0.45f, 0.9f);
						if (scene_->hitEffect_)
						{
							scene_->hitEffect_->Play(enemyPos);
							scene_->hitEffect_->SpawnPlaneParticles(enemyPos);
						}
						if (scene_->particleManager && scene_->appParticleManager_)
						{
							scene_->appParticleManager_->EmitDeathFlash(scene_->particleManager->GetRandomEngine(), enemyPos, 6.0f, { 1.0f, 0.8f, 0.2f, 1.0f }, 0.45f, scene_->starburstTextureIndex_);

							for (int i = 0; i < 90; ++i)
							{
								scene_->appParticleManager_->EmitFeather(scene_->particleManager->GetRandomEngine(), enemyPos, { 1.0f, 0.9f, 0.2f, 1.0f }, scene_->particleTextureB);
							}
							for (int i = 0; i < 30; ++i)
							{
								scene_->appParticleManager_->EmitSpark(scene_->particleManager->GetRandomEngine(), enemyPos, {0,0,0}, { 1.0f, 0.6f, 0.0f, 1.0f }, 0.15f, 1.5f, scene_->particleTextureB);
							}
							for (int i = 0; i < 40; ++i)
							{
								float angle = i * (6.2831853f / 40.0f);
								Vector3 vel = { std::cos(angle) * 3.5f, 0.2f, std::sin(angle) * 3.5f };
								scene_->appParticleManager_->EmitDustWithVelocity(
									scene_->particleManager->GetRandomEngine(),
									enemyPos,
									2.0f,
									{ 1.0f, 1.0f, 1.0f, 0.6f },
									vel,
									1.2f,
									scene_->particleTextureB
								);
							}
						}
					}
					bullet->Finalize(); // 着弾した弾丸を削除マーク
					continue;
				}
			}

			// --- プレイヤーの弾丸と移動敵の衝突判定 ---
			if (scene_->movingEnemy_ && !scene_->movingEnemy_->IsDead())
			{
				const Vector3 enemyPos = scene_->movingEnemy_->GetPosition();
				if (scene_->IsWithinRadius(bulletPos, enemyPos, scene_->bulletHitRadius_ + scene_->enemyHitRadius_))
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
						scene_->AddFloatingText(enemyPos + Vector3{ 0.0f, 1.2f, 0.0f }, text, color, isCritical);

						// ヒットストップをトリガー（通常ヒットはカクつき防止のため行わず、クリティカル・死亡時のみに限定）
						if (scene_->movingEnemy_->IsDead())
						{
							scene_->TriggerHitStop(0.12f); // 撃破時は長めのスロー
						}
						else if (isCritical)
						{
							scene_->TriggerHitStop(0.05f); // クリティカル時は一瞬のスロー
						}
					}

					if (scene_->movingEnemy_->IsDead())
					{
						scene_->TriggerCameraShake(0.45f, 0.9f);
						if (scene_->hitEffect_)
						{
							scene_->hitEffect_->Play(enemyPos);
							scene_->hitEffect_->SpawnPlaneParticles(enemyPos);
						}
						if (scene_->particleManager && scene_->appParticleManager_)
						{
							scene_->appParticleManager_->EmitDeathFlash(scene_->particleManager->GetRandomEngine(), enemyPos, 6.0f, { 1.0f, 0.1f, 0.75f, 1.0f }, 0.45f, scene_->starburstTextureIndex_);

							for (int i = 0; i < 90; ++i)
							{
								scene_->appParticleManager_->EmitFeather(scene_->particleManager->GetRandomEngine(), enemyPos, { 1.0f, 0.1f, 0.75f, 1.0f }, scene_->particleTextureB);
							}
							for (int i = 0; i < 30; ++i)
							{
								scene_->appParticleManager_->EmitSpark(scene_->particleManager->GetRandomEngine(), enemyPos, {0,0,0}, { 1.0f, 0.1f, 0.75f, 1.0f }, 0.15f, 1.5f, scene_->particleTextureB);
							}
							for (int i = 0; i < 40; ++i)
							{
								float angle = i * (6.2831853f / 40.0f);
								Vector3 vel = { std::cos(angle) * 3.5f, 0.2f, std::sin(angle) * 3.5f };
								scene_->appParticleManager_->EmitDustWithVelocity(
									scene_->particleManager->GetRandomEngine(),
									enemyPos,
									2.0f,
									{ 1.0f, 0.1f, 0.75f, 0.6f },
									vel,
									1.2f,
									scene_->particleTextureB
								);
							}
						}
					}
					bullet->Finalize();
					continue;
				}
			}

			// --- プレイヤーの弾丸と的（Target）の衝突判定 ---
			for (auto& target : scene_->GetTargets())
			{
				if (target && !target->IsDead())
				{
					const Vector3 targetPos = target->GetPosition();
					if (scene_->IsWithinRadius(bulletPos, targetPos, scene_->bulletHitRadius_ + target->GetRadius()))
					{
						int prevHp = target->GetHP();
						target->OnHit(1); // 1ダメージ
						int dmg = prevHp - target->GetHP();
						if (dmg > 0)
						{
							bool isCritical = (rand() % 100 < 30);
							std::string text = std::to_string(dmg);
							Vector4 color = isCritical ? Vector4{ 1.0f, 0.9f, 0.0f, 1.0f } : Vector4{ 1.0f, 1.0f, 1.0f, 1.0f };
							if (isCritical) text += "!";
							scene_->AddFloatingText(targetPos + Vector3{ 0.0f, 1.0f, 0.0f }, text, color, isCritical);

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
			// --- 敵の弾丸とプレイヤーの衝突判定 ---
			if (scene_->player_ && !scene_->player_->IsDead())
			{
				const Vector3 playerPos = scene_->player_->GetPosition();
				if (scene_->IsWithinRadius(bulletPos, playerPos, scene_->bulletHitRadius_ + scene_->playerHitRadius_))
				{
					float prevHp = scene_->player_->GetHP();
					scene_->player_->TakeDamage(20.0f); // 弾丸ダメージ
					if (scene_->player_->GetHP() < prevHp)
					{
						scene_->vignetteAlpha_ = 0.6f;          // 画面周囲に赤い被弾ビネット効果
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

		bool hitObstacle = false;
		float closestDist = len;

		auto& colliders = CollisionManager::GetInstance()->GetColliders();
		for (Collider* col : colliders)
		{
			if (!col || !col->IsEnabled() || col->GetAttribute() != CollisionAttribute::Obstacle)
			{
				continue;
			}

			CollisionData data;
			data.originalCollider = col;
			data.type = col->GetType();
			data.attribute = col->GetAttribute();
			data.worldPosition = col->GetWorldPosition();
			data.isTrigger = col->IsTrigger();

			if (data.type == ColliderType::Sphere)
			{
				SphereCollider* sphere = static_cast<SphereCollider*>(col);
				data.shape.radius = sphere->GetRadius();
			}
			else if (data.type == ColliderType::Box)
			{
				BoxCollider* box = static_cast<BoxCollider*>(col);
				data.shape.size = box->GetSize();
				data.shape.rotation = box->GetWorldRotation();
			}
			else if (data.type == ColliderType::Capsule)
			{
				CapsuleCollider* capsule = static_cast<CapsuleCollider*>(col);
				data.shape.radius = capsule->GetRadius();
				data.shape.height = capsule->GetHeight();
			}

			float dist = 0.0f;
			if (CollisionManager::CheckRayCollider(bPosPrev, dir, closestDist, data, dist))
			{
				closestDist = dist;
				hitObstacle = true;
			}
		}

		if (hitObstacle)
		{
			// 着弾交点
			Vector3 hitWorldPos = bPosPrev + dir * closestDist;
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
	// 1. プレイヤーと障害物の押し出し解決
	if (scene_->player_ && !scene_->player_->IsDead())
	{
		Vector3 playerPos = scene_->player_->GetPosition();
		SphereCollider* playerCol = scene_->player_->GetCollider();
		if (playerCol)
		{
			CollisionData playerColData;
			playerColData.originalCollider = playerCol;
			playerColData.type = ColliderType::Sphere;
			playerColData.attribute = CollisionAttribute::Player;
			playerColData.worldPosition = playerPos;
			playerColData.isTrigger = false;
			playerColData.shape.radius = playerCol->GetRadius();

			for (auto& obs : scene_->obstacles_)
			{
				if (!obs) continue;
				BoxCollider* colliders[2] = { obs->GetCollider(), obs->GetCollider2() };
				for (int c = 0; c < 2; ++c)
				{
					BoxCollider* boxCol = colliders[c];
					if (!boxCol) continue;

					CollisionData boxColData;
					boxColData.originalCollider = boxCol;
					boxColData.type = ColliderType::Box;
					boxColData.attribute = CollisionAttribute::Obstacle;
					boxColData.worldPosition = boxCol->GetWorldPosition();
					boxColData.isTrigger = false;
					boxColData.shape.size = boxCol->GetSize();
					boxColData.shape.rotation = boxCol->GetWorldRotation();

					Vector3 pushDir = { 0.0f, 0.0f, 0.0f };
					float pushLen = 0.0f;
					if (CollisionManager::CheckSphereBox(playerColData, boxColData, pushDir, pushLen))
					{
						// 箱から球へ押し戻すため、-pushDir * pushLen を減算
						playerPos = playerPos - pushDir * pushLen;
						scene_->player_->SetPosition(playerPos);
						playerColData.worldPosition = playerPos;
					}
				}
			}
		}
	}

	// 2. 移動敵と障害物の押し出し解決
	if (scene_->movingEnemy_ && !scene_->movingEnemy_->IsDead())
	{
		Vector3 enemyPos = scene_->movingEnemy_->GetPosition();
		SphereCollider* enemyCol = scene_->movingEnemy_->GetCollider();
		if (enemyCol)
		{
			CollisionData enemyColData;
			enemyColData.originalCollider = enemyCol;
			enemyColData.type = ColliderType::Sphere;
			enemyColData.attribute = CollisionAttribute::Enemy;
			enemyColData.worldPosition = enemyPos;
			enemyColData.isTrigger = false;
			enemyColData.shape.radius = enemyCol->GetRadius();

			for (auto& obs : scene_->obstacles_)
			{
				if (!obs) continue;
				BoxCollider* colliders[2] = { obs->GetCollider(), obs->GetCollider2() };
				for (int c = 0; c < 2; ++c)
				{
					BoxCollider* boxCol = colliders[c];
					if (!boxCol) continue;

					CollisionData boxColData;
					boxColData.originalCollider = boxCol;
					boxColData.type = ColliderType::Box;
					boxColData.attribute = CollisionAttribute::Obstacle;
					boxColData.worldPosition = boxCol->GetWorldPosition();
					boxColData.isTrigger = false;
					boxColData.shape.size = boxCol->GetSize();
					boxColData.shape.rotation = boxCol->GetWorldRotation();

					Vector3 pushDir = { 0.0f, 0.0f, 0.0f };
					float pushLen = 0.0f;
					if (CollisionManager::CheckSphereBox(enemyColData, boxColData, pushDir, pushLen))
					{
						enemyPos = enemyPos - pushDir * pushLen;
						scene_->movingEnemy_->SetPosition(enemyPos);
						enemyColData.worldPosition = enemyPos;
					}
				}
			}
		}
	}
}
