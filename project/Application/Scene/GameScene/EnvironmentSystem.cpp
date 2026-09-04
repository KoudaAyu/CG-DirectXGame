#include "EnvironmentSystem.h"
#include "GamePlayScene.h"
#include "Application/Config/GameConfig.h"
#include "Application/Particle/AppParticleManager.h"
#include "ParticleManager.h"
#include "../../Player/Player.h"
#include <cmath>
#include <random>

EnvironmentSystem::EnvironmentSystem(GamePlayScene* scene)
	: scene_(scene)
{
}

void EnvironmentSystem::Initialize()
{
	riverWaveTimer_ = 0.0f;
	riverSplashTimer_ = 0.0f;
	barrierTimer_ = 0.0f;
}

void EnvironmentSystem::Update(float deltaTime)
{
	if (!scene_) return;

	UpdateExtractionGoal(deltaTime);
	UpdateBarrier();
	UpdateRiverEffects(deltaTime);
	UpdateHitEffect(deltaTime);
}

void EnvironmentSystem::UpdateExtractionGoal(float deltaTime)
{
	if (!scene_) return;

	// 脱出リングの回転アニメーション
	if (scene_->goalRing_)
	{
		scene_->goalRingTransform_.rotate.y += 0.02f;
		scene_->goalRing_->SetTransform(scene_->goalRingTransform_);
		scene_->goalRing_->Update();
	}

	if (!scene_->player_)
	{
		return;
	}

	const Vector3 playerPos = scene_->player_->GetPosition();
	const Vector3 goalPos = scene_->goalRingTransform_.translate;
	const float dx = playerPos.x - goalPos.x;
	const float dz = playerPos.z - goalPos.z;
	const float dist = std::sqrt(dx * dx + dz * dz);

	scene_->isPlayerInExtractionZone_ = (dist <= GameConfig::Environment::kExtractionRadius);

	// 脱出ゾーン内かつ全ての的を破壊済みの場合、脱出カウントダウン進行
	if (scene_->isPlayerInExtractionZone_ && scene_->allTargetsDestroyed_)
	{
		if (!scene_->isGameCleared_)
		{
			scene_->extractionTimer_ -= deltaTime;
			if (scene_->extractionTimer_ <= 0.0f)
			{
				scene_->extractionTimer_ = 0.0f;
				scene_->isGameCleared_ = true;
				scene_->clearCelebrateTimer_ = 0.0f;

				// 生還クリア時の特大祝砲 Confetti (150 particles)
				if (scene_->particleManager && scene_->appParticleManager_)
				{
					for (int i = 0; i < 150; ++i)
					{
						float angle = (static_cast<float>(rand()) / RAND_MAX) * 6.2831853f;
						float speedXZ = 1.5f + (static_cast<float>(rand()) / RAND_MAX) * 4.5f;
						Vector3 velocity = {
							std::cos(angle) * speedXZ,
							9.0f + (static_cast<float>(rand()) / RAND_MAX) * 11.0f,
							std::sin(angle) * speedXZ
						};

						Vector4 color;
						int colorType = rand() % 4;
						if (colorType == 0) color = { 1.0f, 0.85f, 0.15f, 1.0f };
						else if (colorType == 1) color = { 0.1f, 0.9f, 1.0f, 1.0f };
						else if (colorType == 2) color = { 1.0f, 0.2f, 0.8f, 1.0f };
						else color = { 0.2f, 1.0f, 0.5f, 1.0f };

						float scale = 0.2f + (static_cast<float>(rand()) / RAND_MAX) * 0.15f;
						float lifeTime = 1.2f + (static_cast<float>(rand()) / RAND_MAX) * 0.8f;

						scene_->appParticleManager_->EmitSparkWithVelocity(
							scene_->particleManager->GetRandomEngine(),
							goalPos,
							velocity,
							color,
							scale,
							lifeTime,
							scene_->particleTextureB
						);
					}
				}
			}
		}
	}
	else if (!scene_->isGameCleared_)
	{
		scene_->extractionTimer_ = GameConfig::Environment::kExtractionMaxTime;
	}

	// 生還クリア後の連続祝砲ファウンテン演出
	if (scene_->isGameCleared_ && scene_->particleManager && scene_->appParticleManager_)
	{
		scene_->clearCelebrateTimer_ += deltaTime;
		if (scene_->clearCelebrateTimer_ >= 0.009f)
		{
			for (int i = 0; i < 10; ++i)
			{
				float angle = (static_cast<float>(rand()) / RAND_MAX) * 6.2831853f;
				float speedXZ = 1.0f + (static_cast<float>(rand()) / RAND_MAX) * 3.5f;
				Vector3 velocity = {
					std::cos(angle) * speedXZ,
					7.0f + (static_cast<float>(rand()) / RAND_MAX) * 9.0f,
					std::sin(angle) * speedXZ
				};

				Vector4 color;
				int colorType = rand() % 4;
				if (colorType == 0) color = { 1.0f, 0.85f, 0.15f, 1.0f };
				else if (colorType == 1) color = { 0.1f, 0.9f, 1.0f, 1.0f };
				else if (colorType == 2) color = { 1.0f, 0.2f, 0.8f, 1.0f };
				else color = { 0.2f, 1.0f, 0.5f, 1.0f };

				float scale = 0.18f + (static_cast<float>(rand()) / RAND_MAX) * 0.12f;
				float lifeTime = 1.0f + (static_cast<float>(rand()) / RAND_MAX) * 0.8f;

				scene_->appParticleManager_->EmitSparkWithVelocity(
					scene_->particleManager->GetRandomEngine(),
					goalPos,
					velocity,
					color,
					scale,
					lifeTime,
					scene_->particleTextureB
				);
			}
			scene_->clearCelebrateTimer_ = 0.0f;
		}
	}

	// 脱出ヘリパッド稼働時の上昇光粒子流 ＆ 風圧ダスト
	if (scene_->particleManager && scene_->appParticleManager_ && scene_->allTargetsDestroyed_)
	{
		scene_->appParticleManager_->EmitHelipadBeaconMotes(
			scene_->particleManager->GetRandomEngine(),
			scene_->goalRingTransform_.translate,
			scene_->particleTextureB
		);

		scene_->escapeSmokeTimer_ += deltaTime;
		if (scene_->escapeSmokeTimer_ >= 0.06f)
		{
			for (int i = 0; i < 2; ++i)
			{
				scene_->appParticleManager_->EmitDust(
					scene_->particleManager->GetRandomEngine(),
					scene_->goalRingTransform_.translate,
					1.6f,
					{ 0.1f, 0.9f, 0.5f, 0.45f },
					scene_->particleTextureB
				);
			}
			scene_->escapeSmokeTimer_ = 0.0f;
		}
	}
}

void EnvironmentSystem::UpdateBarrier()
{
	if (!scene_ || !scene_->sphere_) return;

	Sprite::Transform transformSphere = scene_->sphere_->GetTransform();
	transformSphere.rotate.y += 0.05f; // バリア感を出すために少し速めに回転

	if (!scene_->allTargetsDestroyed_)
	{
		// 的が残っている間は脱出ゲートを保護する赤いバリアとして表示
		transformSphere.translate = scene_->goalRingTransform_.translate;
		transformSphere.translate.y = 0.5f;
		transformSphere.scale = { 1.8f, 1.8f, 1.8f };

		// バリアの鼓動パルス
		barrierTimer_ += GamePlayScene::kFixedDeltaTime * 4.0f;
		float pulse = 1.0f + 0.05f * std::sin(barrierTimer_);
		transformSphere.scale.x *= pulse;
		transformSphere.scale.y *= pulse;
		transformSphere.scale.z *= pulse;
	}
	else
	{
		// 的全滅後は画面外遠くかスケール0にして非表示
		transformSphere.translate = { 0.0f, -100.0f, 0.0f };
		transformSphere.scale = { 0.0f, 0.0f, 0.0f };
	}

	scene_->sphere_->SetTransform(transformSphere);
	scene_->sphere_->Update();
}

void EnvironmentSystem::UpdateRiverEffects(float deltaTime)
{
	if (!scene_ || !scene_->particleManager || !scene_->appParticleManager_) return;

	// (A) 川の流れに沿って流れる上品なさざ波
	riverWaveTimer_ += deltaTime;
	if (riverWaveTimer_ >= GameConfig::Environment::kRiverWaveInterval)
	{
		scene_->appParticleManager_->EmitRiverWaveRipples(
			scene_->particleManager->GetRandomEngine(),
			scene_->particleTextureB
		);
		riverWaveTimer_ = 0.0f;
	}

	// (B) 川面のパチパチ跳ねる水滴
	riverSplashTimer_ += deltaTime;
	if (riverSplashTimer_ >= GameConfig::Environment::kRiverSplashInterval)
	{
		std::uniform_real_distribution<float> rxDist(-18.0f, 18.0f);
		std::uniform_real_distribution<float> rzDist(GameConfig::Environment::kRiverZMin, GameConfig::Environment::kRiverZMax);
		Vector3 sPos = {
			rxDist(scene_->particleManager->GetRandomEngine()),
			GameConfig::Environment::kRiverSplashY,
			rzDist(scene_->particleManager->GetRandomEngine())
		};
		scene_->appParticleManager_->EmitRiverSplashDroplet(
			scene_->particleManager->GetRandomEngine(),
			sPos,
			scene_->particleTextureB
		);
		riverSplashTimer_ = 0.0f;
	}
}

void EnvironmentSystem::UpdateHitEffect(float deltaTime)
{
	if (!scene_ || !scene_->hitEffect_) return;

	scene_->hitEffect_->SetPlaneParticleCount(scene_->emitter.count);
	scene_->hitEffect_->Update(deltaTime);
}
