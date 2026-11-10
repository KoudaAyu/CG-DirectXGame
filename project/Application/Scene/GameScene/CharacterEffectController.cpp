#include "CharacterEffectController.h"
#include "GamePlayScene.h"
#include "Application/Audio/TacticalAudio.h"
#include "Application/Config/GameConfig.h"
#include "Application/Particle/AppParticleManager.h"
#include "ParticleManager.h"
#include "../../Player/Player.h"
#include "../../Enemy/Enemy.h"
#include "../../Enemy/MovingEnemy.h"
#include <array>
#include <cmath>
#include <cstdlib>
#include <random>

namespace
{
	// 共通レイヤー・小石チップ物理定数
	constexpr uint16_t kDustSlotLayer   = 1, kChipSlotLayer = 2, kGlintSlotLayer = 2;
	constexpr Vector2  kChipScaleAspect = { 1.15f, 0.85f };
	constexpr float    kChipGravity     = 15.0f;

	// 回避ローリング小石パラメータ
	constexpr int     kDodgeChipCount = 2;
	constexpr float   kDodgeSpreadRange = 0.35f, kDodgeBackOffset = 0.32f, kDodgePosJitterScale = 0.25f, kDodgeChipSpawnY = 0.05f;
	constexpr float   kDodgeChipBaseSc = 0.022f, kDodgeChipScStep = 0.008f, kDodgeChipTargetMul = 0.65f;
	constexpr float   kDodgeChipBackSpeed = 2.2f, kDodgeChipBaseUpVel = 1.05f, kDodgeChipUpVelStep = 0.45f;
	constexpr float   kDodgeChipBaseLife = 0.30f, kDodgeChipLifeStep = 0.08f, kDodgeChipBounce = 0.36f, kDodgeChipSpinSpeed = 13.0f;
	constexpr Vector4 kDodgeChipColor = { 0.45f, 0.38f, 0.28f, 0.88f }, kDodgeChipEndColor = { 0.38f, 0.32f, 0.24f, 0.0f };

	// 地表材質判定範囲（木造橋・脱出ヘリパッド・初期コンクリートパッド）
	constexpr float kBridgeZMin = 26.0f, kBridgeZMax = 34.0f, kBridgeCenterAbsX = 14.0f, kBridgeHalfWidth = 1.55f;
	constexpr float kExtractionPadRadiusSq = GameConfig::Math::Square(3.6f), kSpawnPadRadiusSq = GameConfig::Math::Square(3.2f);

	// 足音SE・足元土埃パフ定数
	constexpr float   kWalkStepVolume = 0.35f, kSprintStepVolume = 0.50f, kFootstepSpreadRange = 0.12f;
	constexpr int     kStepAudioCooldownMs = 55;
	constexpr Vector2 kFootstepPuffAspect  = { 1.42f, 0.70f };
	constexpr float   kFootstepScaleStep = 0.022f, kFootstepVelYStep = 0.06f, kFootstepLifeStep = 0.06f;
	constexpr float   kFootstepDrag = 2.7f, kFootstepSpinScale = 5.0f, kFootstepFadeInRatio = 0.15f;

	enum class GroundSurface : size_t { Dirt = 0, Bridge = 1, Helipad = 2 };
	struct SurfaceDustSpec { float puffSpawnY; Vector3 baseRgb; float walkAlpha, sprintAlpha; Vector4 endColor; };
	constexpr std::array<SurfaceDustSpec, 3> kSurfaceDustSpecs = { {
		{ 0.032f, { 0.61f, 0.53f, 0.41f }, 0.24f, 0.34f, { 0.67f, 0.60f, 0.50f, 0.0f } }, // Dirt
		{ 0.075f, { 0.56f, 0.45f, 0.32f }, 0.18f, 0.26f, { 0.62f, 0.52f, 0.40f, 0.0f } }, // Bridge
		{ 0.032f, { 0.72f, 0.72f, 0.70f }, 0.16f, 0.24f, { 0.78f, 0.78f, 0.76f, 0.0f } }, // Helipad
	} };

	struct GaitDustSpec { int puffCount; float baseScale, targetScaleMul, horizSpeedMul, baseVelY, baseLife; };
	constexpr GaitDustSpec kWalkGaitSpec   = { 2, 0.095f, 2.05f, 2.0f, 0.14f, 0.30f };
	constexpr GaitDustSpec kSprintGaitSpec = { 3, 0.135f, 2.45f, 3.2f, 0.24f, 0.38f };

	// 全力スプリント時の蹴り上げ土塊・芝片チップ定数
	constexpr int     kSprintChipCount = 2;
	constexpr float   kSprintChipBridgeY = 0.08f, kSprintChipDirtY = 0.04f, kSprintChipBaseSc = 0.020f, kSprintChipScStep = 0.006f, kSprintChipTargetMul = 0.7f;
	constexpr float   kSprintChipBackSpeed = 1.25f, kSprintChipBackStep = 0.55f, kSprintChipHorizJitter = 2.5f, kSprintChipBaseUpVel = 0.95f, kSprintChipUpVelStep = 0.40f;
	constexpr float   kSprintChipEndRgbFactor = 0.85f, kSprintChipBaseLife = 0.28f, kSprintChipLifeStep = 0.08f, kSprintChipBounce = 0.35f, kSprintChipSpinSpeed = 12.0f;
	constexpr Vector4 kBridgeSprintChipColor = { 0.46f, 0.34f, 0.20f, 0.90f };
	constexpr Vector4 kDirtSprintChipColor0  = { 0.44f, 0.37f, 0.27f, 0.90f }, kDirtSprintChipColor1 = { 0.28f, 0.42f, 0.22f, 0.88f };

	// 敵の警戒予兆グリント粒子定数
	constexpr int     kPercentModulo = 100;
	constexpr float   kGlintBaseChance = 8.0f, kGlintMeterChanceMul = 18.0f, kGlintOffsetRange = 0.18f, kGlintHeadOffsetY = 1.18f;
	constexpr float   kGlintBaseScale = 0.032f, kGlintMeterScaleMul = 0.022f, kGlintTargetScaleMul = 0.35f;
	constexpr float   kGlintHorizVelMul = 0.4f, kGlintBaseUpVel = 0.45f, kGlintMeterUpVelMul = 0.35f;
	constexpr float   kGlintColorGreenBase = 0.86f, kGlintColorGreenDrop = 0.28f, kGlintColorBlue = 0.22f, kGlintColorAlpha = 0.72f;
	constexpr float   kGlintLifeTime = 0.32f, kGlintDrag = 2.0f, kGlintFadeInRatio = 0.18f;
	constexpr Vector2 kGlintScaleAspect = { 0.65f, 1.45f };
	constexpr Vector4 kGlintEndColor    = { 1.0f, 0.42f, 0.08f, 0.0f };

	GroundSurface ClassifyGroundSurface(const Vector3& pos)
	{
		if (pos.z >= kBridgeZMin && pos.z <= kBridgeZMax &&
			(std::abs(pos.x + kBridgeCenterAbsX) <= kBridgeHalfWidth || std::abs(pos.x - kBridgeCenterAbsX) <= kBridgeHalfWidth))
			return GroundSurface::Bridge;
		const float dxPad = pos.x - GameConfig::Environment::kExtractionPadPosition.x;
		const float dzPad = pos.z - GameConfig::Environment::kExtractionPadPosition.z;
		if ((dxPad * dxPad + dzPad * dzPad <= kExtractionPadRadiusSq) || (pos.x * pos.x + pos.z * pos.z <= kSpawnPadRadiusSq))
			return GroundSurface::Helipad;
		return GroundSurface::Dirt;
	}

	struct EffectParticleStyle { Vector2 aspect; float targetScaleMul, gravity, drag, bounce, fadeInRatio; uint16_t slotLayer; };
	constexpr EffectParticleStyle kDodgeChipStyle  = { kChipScaleAspect,  kDodgeChipTargetMul,  kChipGravity, 0.0f,       kDodgeChipBounce,  0.0f,              kChipSlotLayer };
	constexpr EffectParticleStyle kSprintChipStyle = { kChipScaleAspect,  kSprintChipTargetMul, kChipGravity, 0.0f,       kSprintChipBounce, 0.0f,              kChipSlotLayer };
	constexpr EffectParticleStyle kGlintStyle      = { kGlintScaleAspect, kGlintTargetScaleMul, 0.0f,         kGlintDrag, 0.0f,              kGlintFadeInRatio, kGlintSlotLayer };

	void EmitEffectParticle(
		AppParticleManager* mgr, const Vector3& pos, float rotZ, float initScale, float targetScaleMul,
		const Vector3& velocity, const Vector4& color, const Vector4& endColor, float lifeTime,
		uint32_t tex, const EffectParticleStyle& style, float angularVel = 0.0f)
	{
		AppParticle p{};
		p.transform.Initialize();
		p.transform.SetTranslate(pos);
		p.transform.SetRotate({ 0.0f, 0.0f, rotZ });
		p.scaleAspect = style.aspect;
		p.initialScale = initScale;
		p.targetScale = initScale * targetScaleMul;
		p.transform.SetScale({ initScale * style.aspect.x, initScale * style.aspect.y, 1.0f });
		p.velocity = velocity;
		p.color = color;
		p.endColor = endColor;
		p.hasColorShift = true;
		p.lifeTime = lifeTime;
		p.currentTime = 0.0f;
		p.textureIndex = tex;
		p.gravity = style.gravity;
		p.drag = style.drag;
		p.bounceElasticity = style.bounce;
		p.angularVelocity = angularVel;
		p.fadeInRatio = style.fadeInRatio;
		p.slotLayer = style.slotLayer;
		mgr->AddParticle(p);
	}
}

CharacterEffectController::CharacterEffectController(GamePlayScene* scene) : scene_(scene) {}

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

	if (scene_->appParticleManager_)
	{
		const Vector3 pPos = scene_->player_ ? scene_->player_->GetPosition() : Vector3{ 0.0f, 0.0f, 0.0f };
		scene_->appParticleManager_->Update(deltaTime, pPos);
	}
}

void CharacterEffectController::UpdatePlayerEffects(float deltaTime)
{
	if (!scene_ || !scene_->particleManager || !scene_->appParticleManager_ || !scene_->player_ || scene_->player_->IsDead()) return;

	const Vector3 pPos = scene_->player_->GetPosition();
	AppParticleManager* smokeMgr = scene_->smokeParticleManager_ ? scene_->smokeParticleManager_.get() : scene_->appParticleManager_.get();
	const uint32_t dustTex = (scene_->soilDustTextureIndex_ != TextureManager::kInvalidTextureIndex) ? scene_->soilDustTextureIndex_ : scene_->wispySmokeTextureIndex_;
	const uint32_t chipTex = (scene_->debrisChipTextureIndex_ != TextureManager::kInvalidTextureIndex) ? scene_->debrisChipTextureIndex_ : scene_->particleTextureB;

	if (scene_->player_->IsDodging())
	{
		// ローリング回避中: 進行方向の逆へ足元から巻き上がるリアルな砂埃＆小石飛沫（SF発光トレイルは廃止）
		dodgeDustTimer_ += deltaTime;
		if (dodgeDustTimer_ >= GameConfig::Player::kDodgeDustInterval)
		{
			const Vector3 pRot = scene_->player_->GetRotation();
			const Vector3 dodgeDir = { std::sin(pRot.y), 0.0f, std::cos(pRot.y) };
			auto& rng = scene_->particleManager->GetRandomEngine();

			smokeMgr->EmitDodgeRollDust(rng, pPos, dodgeDir, dustTex);

			std::uniform_real_distribution<float> spread(-kDodgeSpreadRange, kDodgeSpreadRange);
			std::uniform_real_distribution<float> rotDist(0.0f, GameConfig::Math::kTwoPi);
			for (int i = 0; i < kDodgeChipCount; ++i)
			{
				const Vector3 pos = pPos - dodgeDir * kDodgeBackOffset + Vector3{ spread(rng) * kDodgePosJitterScale, kDodgeChipSpawnY, spread(rng) * kDodgePosJitterScale };
				const float rotZ = rotDist(rng);
				const Vector3 vel = { (-dodgeDir.x + spread(rng)) * kDodgeChipBackSpeed, kDodgeChipBaseUpVel + i * kDodgeChipUpVelStep, (-dodgeDir.z + spread(rng)) * kDodgeChipBackSpeed };
				EmitEffectParticle(
					smokeMgr, pos, rotZ, kDodgeChipBaseSc + i * kDodgeChipScStep, kDodgeChipStyle.targetScaleMul,
					vel, kDodgeChipColor, kDodgeChipEndColor, kDodgeChipBaseLife + i * kDodgeChipLifeStep,
					chipTex, kDodgeChipStyle, (i == 0) ? kDodgeChipSpinSpeed : -kDodgeChipSpinSpeed);
			}
			dodgeDustTimer_ = 0.0f;
		}
	}
	else if (scene_->player_->IsMoving())
	{
		// 歩行・ダッシュ中: 左右の水かき足が地面に着地した瞬間、地表材質に応じた土埃・小石を発生
		Vector3 footstepPos{};
		if (scene_->player_->ConsumeFootstepDust(footstepPos))
		{
			auto& rng = scene_->particleManager->GetRandomEngine();
			const bool isSprint = scene_->player_->IsWeaponHolstered();
			const GroundSurface surface = ClassifyGroundSurface(footstepPos);
			const float stepVol = isSprint ? kSprintStepVolume : kWalkStepVolume;

			if (surface == GroundSurface::Bridge)
				TacticalAudio::PlayRandom2D({ "Resources/Audio/SE_Step_Wood_1.wav", "Resources/Audio/SE_Step_Wood_2.wav" }, stepVol, kStepAudioCooldownMs, "player_step");
			else if (surface == GroundSurface::Helipad)
				TacticalAudio::Play2D("Resources/Audio/SE_Step_Concrete.wav", stepVol, kStepAudioCooldownMs, "player_step");
			else
				TacticalAudio::PlayRandom2D({ "Resources/Audio/SE_Step_Dirt_1.wav", "Resources/Audio/SE_Step_Dirt_2.wav" }, stepVol, kStepAudioCooldownMs, "player_step");

			std::uniform_real_distribution<float> spread(-kFootstepSpreadRange, kFootstepSpreadRange);
			std::uniform_real_distribution<float> rotDist(0.0f, GameConfig::Math::kTwoPi);
			const SurfaceDustSpec& surfSpec = kSurfaceDustSpecs[static_cast<size_t>(surface)];
			const GaitDustSpec& gaitSpec = isSprint ? kSprintGaitSpec : kWalkGaitSpec;
			const Vector4 puffColor = { surfSpec.baseRgb.x, surfSpec.baseRgb.y, surfSpec.baseRgb.z, isSprint ? surfSpec.sprintAlpha : surfSpec.walkAlpha };
			const EffectParticleStyle puffStyle = { kFootstepPuffAspect, gaitSpec.targetScaleMul, 0.0f, kFootstepDrag, 0.0f, kFootstepFadeInRatio, kDustSlotLayer };

			for (int i = 0; i < gaitSpec.puffCount; ++i)
			{
				const Vector3 pos = footstepPos + Vector3{ spread(rng), surfSpec.puffSpawnY, spread(rng) };
				const float rotZ = rotDist(rng);
				const Vector3 vel = { spread(rng) * gaitSpec.horizSpeedMul, gaitSpec.baseVelY + i * kFootstepVelYStep, spread(rng) * gaitSpec.horizSpeedMul };
				EmitEffectParticle(
					smokeMgr, pos, rotZ, gaitSpec.baseScale + i * kFootstepScaleStep, puffStyle.targetScaleMul,
					vel, puffColor, surfSpec.endColor, gaitSpec.baseLife + i * kFootstepLifeStep,
					dustTex, puffStyle, spread(rng) * kFootstepSpinScale);
			}

			// 全力ダッシュ（[C] 武器収納スプリント）時は足裏で蹴り上げた土塊・芝片がリアルに跳ねる
			if (isSprint && surface != GroundSurface::Helipad)
			{
				const bool onBridge = (surface == GroundSurface::Bridge);
				const Vector3 pRot = scene_->player_->GetRotation();
				const Vector3 backDir = { -std::sin(pRot.y), 0.0f, -std::cos(pRot.y) };
				for (int i = 0; i < kSprintChipCount; ++i)
				{
					const Vector3 pos = footstepPos + Vector3{ spread(rng) * 0.5f, onBridge ? kSprintChipBridgeY : kSprintChipDirtY, spread(rng) * 0.5f };
					const float rotZ = rotDist(rng);
					const Vector3 vel = backDir * (kSprintChipBackSpeed + i * kSprintChipBackStep)
						+ Vector3{ spread(rng) * kSprintChipHorizJitter, kSprintChipBaseUpVel + i * kSprintChipUpVelStep, spread(rng) * kSprintChipHorizJitter };
					const Vector4 col = onBridge ? kBridgeSprintChipColor : ((i == 0) ? kDirtSprintChipColor0 : kDirtSprintChipColor1);
					const Vector4 endCol = { col.x * kSprintChipEndRgbFactor, col.y * kSprintChipEndRgbFactor, col.z * kSprintChipEndRgbFactor, 0.0f };
					EmitEffectParticle(
						smokeMgr, pos, rotZ, kSprintChipBaseSc + i * kSprintChipScStep, kSprintChipStyle.targetScaleMul,
						vel, col, endCol, kSprintChipBaseLife + i * kSprintChipLifeStep,
						chipTex, kSprintChipStyle, (i == 0) ? kSprintChipSpinSpeed : -kSprintChipSpinSpeed);
				}
			}
			stepTimer_ = 0.0f;
		}
		else
		{
			stepTimer_ += deltaTime;
		}
	}
	else
	{
		Vector3 dummyPos{};
		scene_->player_->ConsumeFootstepDust(dummyPos);
		stepTimer_ = 0.0f;
	}
}

void CharacterEffectController::UpdateEnemyEffects(float /*deltaTime*/)
{
	if (!scene_ || !scene_->particleManager || !scene_->appParticleManager_) return;

	const uint32_t glintTex = (scene_->sparkStreakTextureIndex_ != TextureManager::kInvalidTextureIndex) ? scene_->sparkStreakTextureIndex_ : scene_->particleTextureB;
	auto& rng = scene_->particleManager->GetRandomEngine();
	std::uniform_real_distribution<float> offsetDist(-kGlintOffsetRange, kGlintOffsetRange);
	std::uniform_real_distribution<float> rotDist(0.0f, GameConfig::Math::kTwoPi);

	for (auto& enemy : scene_->GetEnemies())
	{
		if (!enemy || enemy->IsDead() || enemy->GetDetectionMeter() <= 0.0f || enemy->GetAIState() == Enemy::AIState::Chase) continue;

		// 旧来の巨大黄色発光球（0.85m）を廃止し、頭上に控えめな戦術警戒グリント微粒子のみを瞬かせる
		const float meter = enemy->GetDetectionMeter();
		if (std::rand() % kPercentModulo < static_cast<int>(kGlintBaseChance + meter * kGlintMeterChanceMul))
		{
			const Vector3 pos = enemy->GetPosition() + Vector3{ offsetDist(rng), kGlintHeadOffsetY, offsetDist(rng) };
			const float rotZ = rotDist(rng);
			const Vector3 vel = { offsetDist(rng) * kGlintHorizVelMul, kGlintBaseUpVel + meter * kGlintMeterUpVelMul, offsetDist(rng) * kGlintHorizVelMul };
			const Vector4 col = { 1.0f, kGlintColorGreenBase - meter * kGlintColorGreenDrop, kGlintColorBlue, kGlintColorAlpha };
			EmitEffectParticle(
				scene_->appParticleManager_.get(), pos, rotZ, kGlintBaseScale + meter * kGlintMeterScaleMul, kGlintStyle.targetScaleMul,
				vel, col, kGlintEndColor, kGlintLifeTime, glintTex, kGlintStyle);
		}
	}
}
