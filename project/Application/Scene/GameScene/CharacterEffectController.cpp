#include "CharacterEffectController.h"
#include "GamePlayScene.h"
#include "Application/Config/GameConfig.h"
#include "Application/Particle/AppParticleManager.h"
#include "ParticleManager.h"
#include "../../Player/Player.h"
#include "../../Enemy/Enemy.h"
#include "../../Enemy/MovingEnemy.h"
#include <cmath>
#include <random>

CharacterEffectController::CharacterEffectController(GamePlayScene* scene)
	: scene_(scene)
{
}

void CharacterEffectController::Initialize()
{
	dodgeDustTimer_ = 0.0f;
	stepTimer_ = 0.0f;
}

void CharacterEffectController::Update(float deltaTime)
{
	if (!scene_) return;

	UpdatePlayerEffects(deltaTime);
	UpdateEnemyEffects(deltaTime);

	// AppParticleManager 全体の更新
	if (scene_->appParticleManager_)
	{
		Vector3 pPos = scene_->player_ ? scene_->player_->GetPosition() : Vector3{ 0.0f, 0.0f, 0.0f };
		scene_->appParticleManager_->Update(deltaTime, pPos);
	}
}

void CharacterEffectController::UpdatePlayerEffects(float deltaTime)
{
	if (!scene_ || !scene_->particleManager || !scene_->appParticleManager_ || !scene_->player_ || scene_->player_->IsDead())
	{
		return;
	}

	Vector3 pPos = scene_->player_->GetPosition();

	if (scene_->player_->IsDodging())
	{
		// ローリング回避中: 進行方向の逆へ足元から吹き出す土煙
		dodgeDustTimer_ += deltaTime;
		if (dodgeDustTimer_ >= GameConfig::Player::kDodgeDustInterval)
		{
			Vector3 pRot = scene_->player_->GetRotation();
			Vector3 dodgeDir = { std::sin(pRot.y), 0.0f, std::cos(pRot.y) };
			scene_->appParticleManager_->EmitDodgeRollDust(
				scene_->particleManager->GetRandomEngine(),
				pPos,
				dodgeDir,
				scene_->smokeTextureIndex_
			);
			dodgeDustTimer_ = 0.0f;
		}
	}
	else if (scene_->player_->IsMoving())
	{
		// 走り移動中: 足元から舞い上がる土埃
		stepTimer_ += deltaTime;
		if (stepTimer_ >= GameConfig::Player::kStepDustInterval)
		{
			scene_->appParticleManager_->EmitFootstepDust(
				scene_->particleManager->GetRandomEngine(),
				pPos,
				scene_->smokeTextureIndex_
			);
			stepTimer_ = 0.0f;
		}
	}
}

void CharacterEffectController::UpdateEnemyEffects(float /*deltaTime*/)
{
	if (!scene_ || !scene_->particleManager || !scene_->appParticleManager_)
	{
		return;
	}

	auto emitSuspicionAura = [&](const Vector3& pos, float meter) {
		// 警戒度に応じて発生頻度を高める (15% 〜 45%)
		int chance = static_cast<int>(15.0f + meter * 30.0f);
		if (rand() % 100 < chance)
		{
			std::uniform_real_distribution<float> offsetDist(-0.35f, 0.35f);
			Vector3 spawnPos = pos + Vector3{
				offsetDist(scene_->particleManager->GetRandomEngine()),
				1.3f,
				offsetDist(scene_->particleManager->GetRandomEngine())
			};

			std::uniform_real_distribution<float> velY(0.5f, 1.3f);
			std::uniform_real_distribution<float> velXZ(-0.2f, 0.2f);
			Vector3 vel = {
				velXZ(scene_->particleManager->GetRandomEngine()),
				velY(scene_->particleManager->GetRandomEngine()),
				velXZ(scene_->particleManager->GetRandomEngine())
			};

			// 警戒度に応じた色（黄色〜オレンジ）
			Vector4 color = { 1.0f, 0.9f - (meter * 0.3f), 0.15f, 0.75f };

			scene_->appParticleManager_->EmitDustWithVelocity(
				scene_->particleManager->GetRandomEngine(),
				spawnPos,
				0.85f, // はっきり見えるサイズ
				color,
				vel,
				1.15f, // 寿命を長くして頭上へ立ち上らせる
				scene_->particleTextureB
			);
		}
	};

	if (scene_->enemy_ && !scene_->enemy_->IsDead() &&
		scene_->enemy_->GetDetectionMeter() > 0.0f &&
		scene_->enemy_->GetAIState() != Enemy::AIState::Chase)
	{
		emitSuspicionAura(scene_->enemy_->GetPosition(), scene_->enemy_->GetDetectionMeter());
	}

	if (scene_->movingEnemy_ && !scene_->movingEnemy_->IsDead() &&
		scene_->movingEnemy_->GetDetectionMeter() > 0.0f &&
		scene_->movingEnemy_->GetAIState() != MovingEnemy::AIState::Chase)
	{
		emitSuspicionAura(scene_->movingEnemy_->GetPosition(), scene_->movingEnemy_->GetDetectionMeter());
	}
}
