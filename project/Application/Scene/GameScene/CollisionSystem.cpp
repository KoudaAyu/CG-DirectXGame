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
#include "RaidStats.h"
#include "Matrix4x4.h"
#include <cmath>
#include <algorithm>

// 線分/レイ [rayStart, rayDir * maxDist] と OBB (中心 boxCenter, サイズ boxSize, 回転 boxRot) との精密交差判定
static bool CheckRayBoxIntersection(
	const Vector3& rayStart,
	const Vector3& rayDir,
	float maxDist,
	const Vector3& boxCenter,
	const Vector3& boxSize,
	const Vector3& boxRot,
	float& outDist)
{
	Vector3 extents = { boxSize.x * 0.5f, boxSize.y * 0.5f, boxSize.z * 0.5f };

	// 回転行列 R を計算する
	Matrix4x4 R = Multiply(MakeRotateXMatrix(boxRot.x), Multiply(MakeRotateYMatrix(boxRot.y), MakeRotateZMatrix(boxRot.z)));

	// ボックスのローカル軸（X, Y, Z）
	Vector3 axisX = { R.m[0][0], R.m[0][1], R.m[0][2] };
	Vector3 axisY = { R.m[1][0], R.m[1][1], R.m[1][2] };
	Vector3 axisZ = { R.m[2][0], R.m[2][1], R.m[2][2] };

	// レイの始点と方向をローカル空間に変換
	Vector3 offset = { rayStart.x - boxCenter.x, rayStart.y - boxCenter.y, rayStart.z - boxCenter.z };
	Vector3 localStart = {
		offset.x * axisX.x + offset.y * axisX.y + offset.z * axisX.z,
		offset.x * axisY.x + offset.y * axisY.y + offset.z * axisY.z,
		offset.x * axisZ.x + offset.y * axisZ.y + offset.z * axisZ.z
	};
	Vector3 localDir = {
		rayDir.x * axisX.x + rayDir.y * axisX.y + rayDir.z * axisX.z,
		rayDir.x * axisY.x + rayDir.y * axisY.y + rayDir.z * axisY.z,
		rayDir.x * axisZ.x + rayDir.y * axisZ.y + rayDir.z * axisZ.z
	};

	float tmin = 0.0f;
	float tmax = maxDist;

	// X軸
	if (std::abs(localDir.x) < 1e-6f)
	{
		if (localStart.x < -extents.x || localStart.x > extents.x) return false;
	}
	else
	{
		float ood = 1.0f / localDir.x;
		float t1 = (-extents.x - localStart.x) * ood;
		float t2 = (extents.x - localStart.x) * ood;
		if (t1 > t2) std::swap(t1, t2);
		tmin = (std::max)(tmin, t1);
		tmax = (std::min)(tmax, t2);
		if (tmin > tmax) return false;
	}

	// Y軸
	if (std::abs(localDir.y) < 1e-6f)
	{
		if (localStart.y < -extents.y || localStart.y > extents.y) return false;
	}
	else
	{
		float ood = 1.0f / localDir.y;
		float t1 = (-extents.y - localStart.y) * ood;
		float t2 = (extents.y - localStart.y) * ood;
		if (t1 > t2) std::swap(t1, t2);
		tmin = (std::max)(tmin, t1);
		tmax = (std::min)(tmax, t2);
		if (tmin > tmax) return false;
	}

	// Z軸
	if (std::abs(localDir.z) < 1e-6f)
	{
		if (localStart.z < -extents.z || localStart.z > extents.z) return false;
	}
	else
	{
		float ood = 1.0f / localDir.z;
		float t1 = (-extents.z - localStart.z) * ood;
		float t2 = (extents.z - localStart.z) * ood;
		if (t1 > t2) std::swap(t1, t2);
		tmin = (std::max)(tmin, t1);
		tmax = (std::min)(tmax, t2);
		if (tmin > tmax) return false;
	}

	outDist = tmin;
	return true;
}

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
	ResolveObstacleCollisions();    // ① 弾丸と障害物の衝突を最優先判定 (障害物に着弾した弾丸は即座にFinalize)
	ResolveBulletCollisions();      // ② 生き残った弾丸とキャラクターの衝突 (CCD精密判定 + LOS遮蔽チェック)
	ResolveCharacterObstacleCollisions(); // キャラクターと障害物の衝突
	ResolveContactDamage();        // プレイヤーと敵の直接接触によるダメージ
}

// 障害物に着弾した際の跳弾火花・木片・おがくず煙の共通演出処理
void CollisionSystem::TriggerObstacleHitEffect(const Vector3& hitPos, const Vector3& bulletDir)
{
	if (!scene_ || !scene_->particleManager || !scene_->appParticleManager_) return;

	Vector3 bDir = bulletDir;
	float blen = std::sqrt(bDir.x * bDir.x + bDir.y * bDir.y + bDir.z * bDir.z);
	if (blen > 1e-4f) bDir = { bDir.x / blen, bDir.y / blen, bDir.z / blen };
	else bDir = { 0.0f, 0.0f, 1.0f };

	Vector3 hitNormal = { -bDir.x, 0.4f, -bDir.z };
	float nlen = std::sqrt(hitNormal.x * hitNormal.x + hitNormal.y * hitNormal.y + hitNormal.z * hitNormal.z);
	if (nlen > 1e-4f) hitNormal = { hitNormal.x / nlen, hitNormal.y / nlen, hitNormal.z / nlen };

	// 障壁・遮蔽物への着弾時の跳弾火花 ＆ 着弾ダストスモーク (GPU Particle Burst)
	scene_->appParticleManager_->EmitRicochetSparks(
		scene_->particleManager->GetRandomEngine(),
		hitPos,
		hitNormal,
		scene_->particleTextureB,
		scene_->smokeTextureIndex_
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
			hitPos,
			{ 0.0f, 0.0f, 0.0f },
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
			hitPos,
			0.6f,
			{ r, g, b, 0.8f },
			vel,
			1.3f,
			scene_->fenceTextureIndex_
		);
	}
}

// シーン内の全障害物（Obstacle）とレイの精密交差判定（直接OBBレイキャスト + エンジン側判定の二重ハイブリッド）
bool CollisionSystem::RaycastObstacles(const Vector3& rayStart, const Vector3& rayDir, float maxDist, float& outHitDist, Vector3& outHitPoint)
{
	if (!scene_) return false;

	bool hit = false;
	float closestDist = maxDist;

	// 1. アプリケーション層で管理する全障害物（Obstacle）のBoxColliderと直接OBB交差判定
	for (const auto& obs : scene_->GetObstacles())
	{
		if (!obs) continue;

		// メインコライダー
		BoxCollider* col = obs->GetCollider();
		if (col && col->IsEnabled())
		{
			float dist = 0.0f;
			if (CheckRayBoxIntersection(rayStart, rayDir, closestDist, col->GetWorldPosition(), col->GetSize(), col->GetWorldRotation(), dist))
			{
				closestDist = dist;
				hit = true;
			}
		}

		// 追加コライダー（フェンス2枚目や脚・欄干など）
		for (const auto& extra : obs->GetExtraColliders())
		{
			if (extra && extra->IsEnabled())
			{
				float dist = 0.0f;
				if (CheckRayBoxIntersection(rayStart, rayDir, closestDist, extra->GetWorldPosition(), extra->GetSize(), extra->GetWorldRotation(), dist))
				{
					closestDist = dist;
					hit = true;
				}
			}
		}
	}

	// 2. エンジン側の CollisionManager::Raycast もフォールバックとして併用（マスク 4: Obstacle）
	{
		Collider* engHitCol = nullptr;
		float engHitDist = closestDist;
		const uint32_t kObstacleAttr = static_cast<uint32_t>(CollisionAttribute::Obstacle); // 値 4
		if (CollisionManager::GetInstance()->Raycast(rayStart, rayDir, closestDist, engHitCol, engHitDist, kObstacleAttr))
		{
			if (engHitCol && engHitCol->GetAttribute() == CollisionAttribute::Obstacle)
			{
				closestDist = engHitDist;
				hit = true;
			}
		}
	}

	if (hit)
	{
		outHitDist = closestDist;
		outHitPoint = { rayStart.x + rayDir.x * closestDist, rayStart.y + rayDir.y * closestDist, rayStart.z + rayDir.z * closestDist };
	}
	return hit;
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
					// ★ 障害物遮蔽チェック (Line-of-Sight): 弾丸の移動軌跡上に障害物がないか検証
					Vector3 toHit = { hitPoint.x - bPosPrev.x, hitPoint.y - bPosPrev.y, hitPoint.z - bPosPrev.z };
					float hitDist = std::sqrt(toHit.x * toHit.x + toHit.y * toHit.y + toHit.z * toHit.z);
					if (hitDist > 1e-4f)
					{
						Vector3 hitDir = { toHit.x / hitDist, toHit.y / hitDist, toHit.z / hitDist };
						float obsDist = hitDist;
						Vector3 obsHitPos{};
						if (RaycastObstacles(bPosPrev, hitDir, hitDist, obsDist, obsHitPos))
						{
							TriggerObstacleHitEffect(obsHitPos, bullet->GetDirection());
							bullet->Finalize();
							continue;
						}
					}

					int prevHp = scene_->enemy_->GetHP();
					scene_->enemy_->OnHit(scene_->player_ ? scene_->player_->GetPosition() : Vector3{0.0f,0.0f,0.0f});
					int dmg = prevHp - scene_->enemy_->GetHP();
					RaidStats::GetInstance().shotsHit++;
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
							RaidStats::GetInstance().enemiesKilled++;
							scene_->TriggerHitStop(0.14f);
						}
						else if (isCritical)
						{
							scene_->TriggerHitStop(0.06f);
						}
					}

					if (scene_->enemy_->IsDead())
					{
						scene_->TriggerCameraShake(0.65f, 1.6f);
						// 💥 敵撃破時の大迫力GPUインスタンシング爆散＆スパークスプラッター
						if (scene_->particleManager && scene_->appParticleManager_)
						{
							Vector3 hitDir = bPosCurrent - bPosPrev;
							float hlen = std::sqrt(hitDir.x * hitDir.x + hitDir.y * hitDir.y + hitDir.z * hitDir.z);
							if (hlen > 1e-4f) hitDir = { hitDir.x / hlen, hitDir.y / hlen, hitDir.z / hlen };

							scene_->appParticleManager_->EmitEnemyDestroyGPUBurst(
								scene_->particleManager->GetRandomEngine(),
								enemyPos,
								hitDir,
								scene_->particleTextureB,
								scene_->starburstTextureIndex_,
								scene_->smokeTextureIndex_
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
					// ★ 障害物遮蔽チェック (Line-of-Sight): 弾丸の移動軌跡上に障害物がないか検証
					Vector3 toHit = { hitPoint.x - bPosPrev.x, hitPoint.y - bPosPrev.y, hitPoint.z - bPosPrev.z };
					float hitDist = std::sqrt(toHit.x * toHit.x + toHit.y * toHit.y + toHit.z * toHit.z);
					if (hitDist > 1e-4f)
					{
						Vector3 hitDir = { toHit.x / hitDist, toHit.y / hitDist, toHit.z / hitDist };
						float obsDist = hitDist;
						Vector3 obsHitPos{};
						if (RaycastObstacles(bPosPrev, hitDir, hitDist, obsDist, obsHitPos))
						{
							TriggerObstacleHitEffect(obsHitPos, bullet->GetDirection());
							bullet->Finalize();
							continue;
						}
					}

					int prevHp = scene_->movingEnemy_->GetHP();
					scene_->movingEnemy_->OnHit(scene_->player_ ? scene_->player_->GetPosition() : Vector3{0.0f,0.0f,0.0f});
					int dmg = prevHp - scene_->movingEnemy_->GetHP();
					RaidStats::GetInstance().shotsHit++;
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
							RaidStats::GetInstance().enemiesKilled++;
							scene_->TriggerHitStop(0.14f);
						}
						else if (isCritical)
						{
							scene_->TriggerHitStop(0.06f);
						}
					}

					if (scene_->movingEnemy_->IsDead())
					{
						scene_->TriggerCameraShake(0.65f, 1.6f);
						// 💥 移動敵撃破時の大迫力GPUインスタンシング爆散＆スパークスプラッター
						if (scene_->particleManager && scene_->appParticleManager_)
						{
							Vector3 hitDir = bPosCurrent - bPosPrev;
							float hlen = std::sqrt(hitDir.x * hitDir.x + hitDir.y * hitDir.y + hitDir.z * hitDir.z);
							if (hlen > 1e-4f) hitDir = { hitDir.x / hlen, hitDir.y / hlen, hitDir.z / hlen };

							scene_->appParticleManager_->EmitEnemyDestroyGPUBurst(
								scene_->particleManager->GetRandomEngine(),
								enemyPos,
								hitDir,
								scene_->particleTextureB,
								scene_->starburstTextureIndex_,
								scene_->smokeTextureIndex_
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
						// ★ 障害物遮蔽チェック (Line-of-Sight): 弾丸の移動軌跡上に障害物がないか検証
						Vector3 toHit = { hitPoint.x - bPosPrev.x, hitPoint.y - bPosPrev.y, hitPoint.z - bPosPrev.z };
						float hitDist = std::sqrt(toHit.x * toHit.x + toHit.y * toHit.y + toHit.z * toHit.z);
						if (hitDist > 1e-4f)
						{
							Vector3 hitDir = { toHit.x / hitDist, toHit.y / hitDist, toHit.z / hitDist };
							float obsDist = hitDist;
							Vector3 obsHitPos{};
							if (RaycastObstacles(bPosPrev, hitDir, hitDist, obsDist, obsHitPos))
							{
								TriggerObstacleHitEffect(obsHitPos, bullet->GetDirection());
								bullet->Finalize();
								break;
							}
						}

						int prevHp = target->GetHP();
						target->OnHit(1);
						int dmg = prevHp - target->GetHP();
						RaidStats::GetInstance().shotsHit++;
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
							RaidStats::GetInstance().targetsDestroyed++;
							scene_->TriggerCameraShake(0.4f, 0.8f);
							// 💥 標的破壊時の超高速GPU破砕スパーク＆ダストリング爆散
							if (scene_->particleManager && scene_->appParticleManager_)
							{
								scene_->appParticleManager_->EmitTargetDestroyGPUBurst(
									scene_->particleManager->GetRandomEngine(),
									targetPos + Vector3{ 0.0f, 0.6f, 0.0f },
									scene_->particleTextureB,
									scene_->starburstTextureIndex_,
									scene_->smokeTextureIndex_
								);
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
					// ★ 障害物遮蔽チェック (Line-of-Sight): 弾丸の移動軌跡上に障害物がないか検証
					Vector3 toHit = { hitPoint.x - bPosPrev.x, hitPoint.y - bPosPrev.y, hitPoint.z - bPosPrev.z };
					float hitDist = std::sqrt(toHit.x * toHit.x + toHit.y * toHit.y + toHit.z * toHit.z);
					if (hitDist > 1e-4f)
					{
						Vector3 hitDir = { toHit.x / hitDist, toHit.y / hitDist, toHit.z / hitDist };
						float obsDist = hitDist;
						Vector3 obsHitPos{};
						if (RaycastObstacles(bPosPrev, hitDir, hitDist, obsDist, obsHitPos))
						{
							// プレイヤーの手前に障害物がある！弾丸は障害物に着弾して消滅（プレイヤー貫通を100%遮断）
							TriggerObstacleHitEffect(obsHitPos, bullet->GetDirection());
							bullet->Finalize();
							continue;
						}
					}

					float prevHp = scene_->player_->GetHP();
					scene_->player_->TakeDamage(20.0f, "HOSTILE SENTRY (AK-74M)", "5.45x39mm PS CARTRIDGE (CHEST PENETRATION)");
					if (scene_->player_->GetHP() < prevHp)
					{
						// 💥 被弾リアクション演出の全発動
						scene_->vignetteAlpha_ = 0.85f;          // 画面全体がピカッと赤く染まるフラッシュ
						scene_->TriggerCameraShake(0.35f, 0.9f); // 重い直撃カメラシェイク
						scene_->TriggerHitStop(0.06f);           // 一瞬の被弾ヒットストップ

						// どの方向から撃たれたかを計算して被弾方向インジケーターをトリガー
						Vector3 bDir = bullet->GetDirection();
						float hitAngle = std::atan2(-bDir.x, -bDir.z); // 弾丸が飛んできた方角
						scene_->TriggerDamageIndicator(hitAngle);

						// 3Dダメージポップアップ
						scene_->AddFloatingText(playerPos + Vector3{ 0.0f, 1.2f, 0.0f }, "💥 -20 HP", { 1.0f, 0.15f, 0.15f, 1.0f }, true);

						if (scene_->particleManager && scene_->appParticleManager_)
						{
							for (int i = 0; i < 25; ++i)
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

// 弾丸と障害物（Obstacle）の精密衝突判定 (RaycastObstaclesによる確実な判定)
void CollisionSystem::ResolveObstacleCollisions()
{
	if (!scene_->combatSystem_) return;
	auto& bullets = scene_->combatSystem_->GetBullets();

	const float kBulletRadius = 0.12f;

	for (auto& bullet : bullets)
	{
		if (!bullet || bullet->IsDead()) continue;
		Vector3 bPosPrev = bullet->GetPrevPosition();
		Vector3 bPos = bullet->GetPosition();
		Vector3 diff = { bPos.x - bPosPrev.x, bPos.y - bPosPrev.y, bPos.z - bPosPrev.z };
		float len = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
		if (len < 1e-4f) continue;
		Vector3 dir = { diff.x / len, diff.y / len, diff.z / len };

		// 浮動小数点数誤差やすき間によるすり抜けを防ぐため、マージン (弾丸半径分) を加味した長さを検査
		float checkLen = len + kBulletRadius;

		float hitDist = checkLen;
		Vector3 hitPos{};
		bool hasHit = false;

		// 1. 中心線レイキャスト
		if (RaycastObstacles(bPosPrev, dir, checkLen, hitDist, hitPos))
		{
			hasHit = true;
		}
		else
		{
			// 2. 弾丸の太さ（半径 0.12m）を考慮したオフセットレイキャスト（左右・上下）
			// 障害物の角や薄いフェンスのすり抜けを完全防止
			Vector3 right = { dir.z, 0.0f, -dir.x };
			float rlen = std::sqrt(right.x * right.x + right.z * right.z);
			if (rlen > 1e-4f) { right.x /= rlen; right.z /= rlen; }
			Vector3 up = { 0.0f, 1.0f, 0.0f };

			const Vector3 offsets[4] = {
				{ right.x * kBulletRadius, right.y * kBulletRadius, right.z * kBulletRadius },
				{ -right.x * kBulletRadius, -right.y * kBulletRadius, -right.z * kBulletRadius },
				{ up.x * kBulletRadius, up.y * kBulletRadius, up.z * kBulletRadius },
				{ -up.x * kBulletRadius, -up.y * kBulletRadius, -up.z * kBulletRadius }
			};

			for (const auto& off : offsets)
			{
				float subDist = checkLen;
				Vector3 subHitPos{};
				Vector3 startOff = { bPosPrev.x + off.x, bPosPrev.y + off.y, bPosPrev.z + off.z };
				if (RaycastObstacles(startOff, dir, checkLen, subDist, subHitPos))
				{
					hasHit = true;
					hitDist = subDist;
					hitPos = subHitPos;
					break;
				}
			}
		}

		if (hasHit)
		{
			// 着弾エフェクト発生
			TriggerObstacleHitEffect(hitPos, bullet->GetDirection());
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
	if (scene_->IsWithinRadius(scene_->player_->GetPosition(), scene_->enemy_->GetPosition(), GamePlayScene::kPlayerHitRadius + GamePlayScene::kEnemyHitRadius))
	{
		float prevHp = scene_->player_->GetHP();
		scene_->player_->TakeDamage(GamePlayScene::kContactDamage, "SENTRY GUARD", "BLUNT FORCE TRAUMA (CLOSE COMBAT)");
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
		if (scene_->IsWithinRadius(scene_->player_->GetPosition(), scene_->movingEnemy_->GetPosition(), GamePlayScene::kPlayerHitRadius + GamePlayScene::kEnemyHitRadius))
		{
			float prevHp = scene_->player_->GetHP();
			scene_->player_->TakeDamage(GamePlayScene::kContactDamage, "PATROL ENFORCER", "CQC MELEE ENGAGEMENT (FATAL IMPACT)");
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
