#include "CombatSystem.h"
#include "GamePlayScene.h"
#include "../../Player/Player.h"
#include "../../Enemy/Enemy.h"
#include "../../Enemy/MovingEnemy.h"
#include "Application/Particle/AppParticleManager.h"
#include "ParticleManager.h"
#include <cmath>
#include <algorithm>

// コンストラクタ
CombatSystem::CombatSystem(GamePlayScene* scene)
	: scene_(scene)
{
}

// 毎フレームの戦闘・弾丸更新・削除のメインアップデート
void CombatSystem::Update(float deltaTime)
{
	UpdateCombat(deltaTime);      // プレイヤーと敵の射撃処理
	UpdateBullets(deltaTime);     // アクティブな弾丸の位置更新と軌跡エフェクト
	RemoveDeadBullets();          // 不要になった弾丸のクリーンアップ
}

// 新規の弾丸オブジェクトをシステムに追加
void CombatSystem::AddBullet(std::unique_ptr<Bullet> bullet)
{
	if (bullet)
	{
		bullets_.emplace_back(std::move(bullet));
	}
}

// プレイヤーおよび敵キャラクターの射撃トリガーと弾丸生成の解決
void CombatSystem::UpdateCombat(float deltaTime)
{
	// --- プレイヤーの射撃判定 ---
	if (scene_->player_)
	{
		// 入力と経過時間に基づいて射撃を試行し、発射された弾丸リストを取得
		std::vector<std::unique_ptr<Bullet>> fired = scene_->player_->TryShoot(&scene_->mouseInput, deltaTime);
		if (!fired.empty())
		{
			// ショットガンかどうか（一度に複数弾発射されるか）判定
			bool isShotgun = (fired.size() > 1);
			float maxRad = isShotgun ? 18.0f : 14.0f;
			scene_->playerSoundMaxRadius_ = maxRad;
			scene_->playerSoundRadius_ = 0.0f;
			scene_->playerSoundTimer_ = 0.6f; // 音波リングの持続時間 (0.6秒)
			
			// 周囲の敵への銃声通知シミュレーション
			Vector3 playerPos = scene_->player_->GetPosition();
			if (scene_->enemy_ && !scene_->enemy_->IsDead())
			{
				float dx = scene_->enemy_->GetPosition().x - playerPos.x;
				float dz = scene_->enemy_->GetPosition().z - playerPos.z;
				float dist = std::sqrt(dx * dx + dz * dz);
				if (dist <= maxRad)
				{
					scene_->enemy_->HearNoise(playerPos);
				}
			}
			if (scene_->movingEnemy_ && !scene_->movingEnemy_->IsDead())
			{
				float dx = scene_->movingEnemy_->GetPosition().x - playerPos.x;
				float dz = scene_->movingEnemy_->GetPosition().z - playerPos.z;
				float dist = std::sqrt(dx * dx + dz * dz);
				if (dist <= maxRad)
				{
					scene_->movingEnemy_->HearNoise(playerPos);
				}
			}

			// マズルフラッシュ用のライト点滅時間設定
			scene_->lightFlashTimer_ = isShotgun ? 0.08f : 0.04f;

			// 射撃タイプに応じたカメラシェイク演出
			if (isShotgun)
			{
				scene_->TriggerCameraShake(0.18f, 0.45f);
			}
			else
			{
				scene_->TriggerCameraShake(0.08f, 0.12f);
			}

			// 銃口マズルエフェクト・煙のパーティクル放出
			if (scene_->particleManager && scene_->appParticleManager_)
			{
				Vector3 bulletPos = fired[0]->GetPosition();
				Vector3 dir = fired[0]->GetDirection();
				Vector3 right = { dir.z, 0.0f, -dir.x };
				Vector3 up = { 0.0f, 1.0f, 0.0f };

				int particleCount = isShotgun ? 24 : 8;
				float speedMultiplier = isShotgun ? 1.5f : 1.0f;

				for (int i = 0; i < particleCount; ++i)
				{
					scene_->appParticleManager_->EmitMuzzleFlash(
						scene_->particleManager->GetRandomEngine(),
						bulletPos,
						dir,
						right,
						up,
						{ 1.0f, 0.8f, 0.2f, 1.0f },
						speedMultiplier,
						scene_->particleTextureB
					);
				}

				scene_->appParticleManager_->EmitMuzzleFlare(
					scene_->particleManager->GetRandomEngine(),
					bulletPos,
					isShotgun ? 0.9f : 0.65f,
					{ 1.0f, 0.85f, 0.3f, 1.0f },
					0.05f,
					scene_->particleTextureB
				);

				for (int i = 0; i < (isShotgun ? 8 : 4); ++i)
				{
					std::uniform_real_distribution<float> velXZ(-0.5f, 0.5f);
					std::uniform_real_distribution<float> velY(0.1f, 0.4f);
					std::uniform_real_distribution<float> forwardMult(0.5f, 1.5f);
					Vector3 smokeVel = dir * forwardMult(scene_->particleManager->GetRandomEngine()) + right * velXZ(scene_->particleManager->GetRandomEngine()) + up * velY(scene_->particleManager->GetRandomEngine());
					
					scene_->appParticleManager_->EmitDustWithVelocity(
						scene_->particleManager->GetRandomEngine(),
						bulletPos,
						isShotgun ? 0.9f : 0.5f,
						{ 0.8f, 0.8f, 0.8f, 0.35f },
						smokeVel,
						0.45f,
						scene_->particleTextureB
					);
				}
			}

			// 生成された弾丸をリストに登録
			for (auto& b : fired)
			{
				AddBullet(std::move(b));
			}
		}
	}

	// --- 固定敵の射撃判定 ---
	if (scene_->enemy_ && scene_->player_ && !scene_->enemy_->IsDead() && !scene_->player_->IsDead())
	{
		std::unique_ptr<Bullet> bullet = scene_->enemy_->TryShoot(scene_->player_->GetPosition());
		if (bullet)
		{
			scene_->TriggerCameraShake(0.06f, 0.08f);
			if (scene_->particleManager && scene_->appParticleManager_)
			{
				Vector3 bulletPos = bullet->GetPosition();
				Vector3 dir = bullet->GetDirection();
				Vector3 right = { dir.z, 0.0f, -dir.x };
				Vector3 up = { 0.0f, 1.0f, 0.0f };

				for (int i = 0; i < 6; ++i)
				{
					scene_->appParticleManager_->EmitMuzzleFlash(
						scene_->particleManager->GetRandomEngine(),
						bulletPos,
						dir,
						right,
						up,
						{ 1.0f, 0.2f, 0.2f, 1.0f },
						1.0f,
						scene_->particleTextureB
					);
				}

				scene_->appParticleManager_->EmitMuzzleFlare(
					scene_->particleManager->GetRandomEngine(),
					bulletPos,
					0.4f,
					{ 1.0f, 0.3f, 0.3f, 1.0f },
					0.08f,
					scene_->particleTextureB
				);

				for (int i = 0; i < 3; ++i)
				{
					std::uniform_real_distribution<float> velXZ(-0.4f, 0.4f);
					std::uniform_real_distribution<float> velY(0.1f, 0.3f);
					std::uniform_real_distribution<float> forwardMult(0.8f, 2.0f);
					Vector3 smokeVel = dir * forwardMult(scene_->particleManager->GetRandomEngine()) + right * velXZ(scene_->particleManager->GetRandomEngine()) + up * velY(scene_->particleManager->GetRandomEngine());
					
					scene_->appParticleManager_->EmitDustWithVelocity(
						scene_->particleManager->GetRandomEngine(),
						bulletPos,
						0.45f,
						{ 0.85f, 0.75f, 0.75f, 0.3f },
						smokeVel,
						0.4f,
						scene_->particleTextureB
					);
				}
			}
			AddBullet(std::move(bullet));
		}
	}

	// --- 巡回敵の射撃判定 ---
	if (scene_->movingEnemy_ && scene_->player_ && !scene_->movingEnemy_->IsDead() && !scene_->player_->IsDead())
	{
		std::unique_ptr<Bullet> bullet = scene_->movingEnemy_->TryShoot(scene_->player_->GetPosition());
		if (bullet)
		{
			scene_->TriggerCameraShake(0.06f, 0.08f);
			if (scene_->particleManager && scene_->appParticleManager_)
			{
				Vector3 bulletPos = bullet->GetPosition();
				Vector3 dir = bullet->GetDirection();
				Vector3 right = { dir.z, 0.0f, -dir.x };
				Vector3 up = { 0.0f, 1.0f, 0.0f };

				for (int i = 0; i < 6; ++i)
				{
					scene_->appParticleManager_->EmitMuzzleFlash(
						scene_->particleManager->GetRandomEngine(),
						bulletPos,
						dir,
						right,
						up,
						{ 0.8f, 0.4f, 1.0f, 1.0f },
						1.0f,
						scene_->particleTextureB
					);
				}

				scene_->appParticleManager_->EmitMuzzleFlare(
					scene_->particleManager->GetRandomEngine(),
					bulletPos,
					0.4f,
					{ 0.9f, 0.4f, 1.0f, 1.0f },
					0.08f,
					scene_->particleTextureB
				);

				for (int i = 0; i < 3; ++i)
				{
					std::uniform_real_distribution<float> velXZ(-0.4f, 0.4f);
					std::uniform_real_distribution<float> velY(0.1f, 0.3f);
					std::uniform_real_distribution<float> forwardMult(0.8f, 2.0f);
					Vector3 smokeVel = dir * forwardMult(scene_->particleManager->GetRandomEngine()) + right * velXZ(scene_->particleManager->GetRandomEngine()) + up * velY(scene_->particleManager->GetRandomEngine());
					
					scene_->appParticleManager_->EmitDustWithVelocity(
						scene_->particleManager->GetRandomEngine(),
						bulletPos,
						0.45f,
						{ 0.8f, 0.75f, 0.85f, 0.3f },
						smokeVel,
						0.4f,
						scene_->particleTextureB
					);
				}
			}
			AddBullet(std::move(bullet));
		}
	}
}

// 弾丸の座標更新と、プレイヤーかすり判定(Graze)、飛翔軌跡（トレイル煙）エフェクトの更新
void CombatSystem::UpdateBullets(float deltaTime)
{
	for (auto& bullet : bullets_)
	{
		if (bullet && !bullet->IsDead())
		{
			bullet->Update(deltaTime);

			// 敵の弾丸とプレイヤーアヒルの「かすり判定(Graze/NearMiss)」
			if (bullet->GetOwner() == BulletOwner::Enemy && scene_->player_ && !scene_->player_->IsDead() && !bullet->IsNearMissTriggered())
			{
				Vector3 bPos = bullet->GetPosition();
				Vector3 pPos = scene_->player_->GetPosition();
				float dx = bPos.x - pPos.x;
				float dz = bPos.z - pPos.z;
				float dist = std::sqrt(dx * dx + dz * dz);
				
				float minHitDist = scene_->playerHitRadius_ + scene_->bulletHitRadius_;
				float nearMissDist = minHitDist + 1.20f; // 当たり判定の少し外側をかすり判定とする

				if (dist > minHitDist && dist <= nearMissDist)
				{
					bullet->TriggerNearMiss(); // 多重判定を防止するためフラグをオンにする

					// かすりエフェクト（白い火花/煙）の生成
					if (scene_->particleManager && scene_->appParticleManager_)
					{
						Vector3 dir = bullet->GetDirection();
						for (int i = 0; i < 4; ++i)
						{
							float offset = (i - 1.5f) * 0.15f;
							Vector3 spawnPos = bPos + dir * offset;
							scene_->appParticleManager_->EmitDustWithVelocity(
								scene_->particleManager->GetRandomEngine(),
								spawnPos,
								0.25f,
								{ 1.0f, 1.0f, 1.0f, 0.6f },
								dir * 1.8f,
								0.12f,
								scene_->particleTextureB
							);
						}
					}

					// スクリーン上に「GRAZE」というテキストをポップアップ表示
					scene_->AddFloatingText(pPos + Vector3{ 0.0f, 1.3f, 0.0f }, "GRAZE", { 0.5f, 0.9f, 1.0f, 1.0f }, false);
				}
			}

			// 弾丸の後方に放出される煙（軌跡トレイル）エフェクト
			if (scene_->particleManager && scene_->appParticleManager_)
			{
				Vector3 pos = bullet->GetPosition();
				Vector4 color = { 0.7f, 0.9f, 1.0f, 0.35f }; // プレイヤー弾は青白いトレイル
				if (bullet->GetOwner() == BulletOwner::Enemy)
				{
					color = { 1.0f, 0.2f, 0.2f, 0.35f };     // 敵弾は赤いトレイル
				}

				scene_->appParticleManager_->EmitDust(
					scene_->particleManager->GetRandomEngine(),
					pos,
					0.5f,
					color,
					scene_->particleTextureB
				);
			}
		}
	}
}

// 死亡判定が立った不要な弾丸の削除（クリーンアップ）
void CombatSystem::RemoveDeadBullets()
{
	bullets_.erase(
		std::remove_if(bullets_.begin(), bullets_.end(), [](const std::unique_ptr<Bullet>& bullet)
			{
				return !bullet || bullet->IsDead();
			}),
		bullets_.end());
}
