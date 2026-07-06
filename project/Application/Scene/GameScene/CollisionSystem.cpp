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
	ResolveObstacleCollisions();    // キャラクター・弾丸と障害物の衝突
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

// キャラクターおよび弾丸と障害物（Obstacle）の衝突判定とめり込み補正
void CollisionSystem::ResolveObstacleCollisions()
{
	// --- 弾丸と障害物の衝突判定 ---
	if (!scene_->combatSystem_) return;
	auto& bullets = scene_->combatSystem_->GetBullets();

	for (auto& bullet : bullets)
	{
		if (!bullet || bullet->IsDead()) continue;
		Vector3 bPos = bullet->GetPosition();
		for (auto& obs : scene_->obstacles_)
		{
			if (!obs) continue;
			Vector3 oPos = obs->GetPosition();
			float dx = bPos.x - oPos.x;
			float dz = bPos.z - oPos.z;
			float dist = std::sqrt(dx * dx + dz * dz);
			float minDist = scene_->bulletHitRadius_ + obs->GetRadius();
			if (dist < minDist)
			{
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
				break;
			}
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
