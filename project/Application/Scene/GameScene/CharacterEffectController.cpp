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
	constexpr int     kDodgeChipCount = 4;
	constexpr float   kDodgeSpreadRange = 0.38f, kDodgeBackOffset = 0.34f, kDodgePosJitterScale = 0.28f, kDodgeChipSpawnY = 0.05f;
	constexpr float   kDodgeChipBaseSc = 0.026f, kDodgeChipScStep = 0.008f, kDodgeChipTargetMul = 0.68f;
	constexpr float   kDodgeChipBackSpeed = 2.6f, kDodgeChipBaseUpVel = 1.20f, kDodgeChipUpVelStep = 0.48f;
	constexpr float   kDodgeChipBaseLife = 0.34f, kDodgeChipLifeStep = 0.09f, kDodgeChipBounce = 0.40f, kDodgeChipSpinSpeed = 14.0f;
	constexpr Vector4 kDodgeChipColor = { 0.48f, 0.40f, 0.30f, 0.92f }, kDodgeChipEndColor = { 0.38f, 0.32f, 0.24f, 0.0f };

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
		{ 0.035f, { 0.63f, 0.55f, 0.43f }, 0.30f, 0.42f, { 0.67f, 0.60f, 0.50f, 0.0f } }, // Dirt
		{ 0.078f, { 0.58f, 0.47f, 0.34f }, 0.24f, 0.34f, { 0.62f, 0.52f, 0.40f, 0.0f } }, // Bridge
		{ 0.035f, { 0.74f, 0.74f, 0.72f }, 0.22f, 0.30f, { 0.78f, 0.78f, 0.76f, 0.0f } }, // Helipad
	} };

	struct GaitDustSpec { int puffCount; float baseScale, targetScaleMul, horizSpeedMul, baseVelY, baseLife; };
	constexpr GaitDustSpec kWalkGaitSpec   = { 2, 0.115f, 2.20f, 2.2f, 0.16f, 0.34f };
	constexpr GaitDustSpec kSprintGaitSpec = { 4, 0.160f, 2.65f, 3.5f, 0.28f, 0.44f };

	// 全力スプリント時の蹴り上げ土塊・芝片チップ定数
	constexpr int     kSprintChipCount = 3;
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
	healFxTimer_ = 0.0f;
	prevHpRatio_ = 1.0f;
	wasHealingPrev_ = false;
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
				const Vector3 vel = { (-dodgeDir.x + spread(rng)) * kDodgeChipBackSpeed, kDodgeChipBaseUpVel + (i % 2) * kDodgeChipUpVelStep, (-dodgeDir.z + spread(rng)) * kDodgeChipBackSpeed };
				EmitEffectParticle(
					smokeMgr, pos, rotZ, kDodgeChipBaseSc + (i % 2) * kDodgeChipScStep, kDodgeChipStyle.targetScaleMul,
					vel, kDodgeChipColor, kDodgeChipEndColor, kDodgeChipBaseLife + (i % 2) * kDodgeChipLifeStep,
					chipTex, kDodgeChipStyle, ((i % 2) == 0) ? kDodgeChipSpinSpeed : -kDodgeChipSpinSpeed);
			}

			// 回避ローリングの勢いで軌跡上にふわりと舞うアヒルの綿羽
			if (scene_->featherTextureIndex_ != TextureManager::kInvalidTextureIndex)
			{
				AppParticle fp{};
				fp.transform.Initialize();
				fp.transform.SetTranslate(pPos - dodgeDir * 0.25f + Vector3{ spread(rng) * 0.45f, 0.42f, spread(rng) * 0.45f });
				fp.transform.SetRotate({ 0.0f, 0.0f, rotDist(rng) });
				const float sc = 0.095f;
				fp.scaleAspect = { 0.76f, 1.28f };
				fp.initialScale = sc;
				fp.targetScale = sc * 0.88f;
				fp.transform.SetScale({ sc * 0.76f, sc * 1.28f, 1.0f });
				fp.velocity = dodgeDir * (-1.1f) + Vector3{ spread(rng) * 1.4f, 1.15f, spread(rng) * 1.4f };
				fp.color = { 1.0f, 0.91f, 0.34f, 0.92f };
				fp.endColor = { 1.0f, 0.91f, 0.34f, 0.0f };
				fp.hasColorShift = true;
				fp.lifeTime = 1.25f;
				fp.textureIndex = scene_->featherTextureIndex_;
				fp.gravity = 1.55f;
				fp.drag = 2.5f;
				fp.wobbleFreq = 8.0f;
				fp.wobbleAmp = 0.48f;
				fp.wobbleAxis = { 1.0f, 0.15f, 0.65f };
				fp.angularVelocity = spread(rng) * 14.0f;
				fp.slotLayer = 2;
				smokeMgr->AddParticle(fp);
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

	// --- [Q] 救急キット使用中の螺旋リカバリー粒子 ＆ 治療完了エメラルドリング ---
	const bool isHealing = scene_->player_->IsHealing();
	const float curHpRatio = scene_->player_->GetHPRatio();
	const uint32_t glintTex = (scene_->sparkStreakTextureIndex_ != TextureManager::kInvalidTextureIndex)
		? scene_->sparkStreakTextureIndex_ : scene_->particleTextureB;

	if (isHealing)
	{
		healFxTimer_ += deltaTime;
		if (healFxTimer_ >= 0.052f)
		{
			healFxTimer_ = 0.0f;
			const float prog = std::clamp(scene_->player_->GetHealProgress(), 0.0f, 1.0f);
			const float baseAng = prog * GameConfig::Math::kTwoPi * 3.0f;
			const float rad = 0.56f - 0.18f * prog;

			for (int s = 0; s < 2; ++s)
			{
				const float a = baseAng + static_cast<float>(s) * GameConfig::Math::kPi;
				AppParticle hp{};
				hp.transform.Initialize();
				hp.transform.SetTranslate(pPos + Vector3{ std::cos(a) * rad, 0.18f + prog * 0.55f, std::sin(a) * rad });
				hp.scaleAspect = { 0.65f, 1.55f };
				hp.initialScale = 0.048f;
				hp.targetScale = 0.018f;
				hp.transform.SetScale({ 0.048f * 0.65f, 0.048f * 1.55f, 1.0f });
				hp.velocity = { -std::sin(a) * 0.85f, 1.35f, std::cos(a) * 0.85f };
				hp.color = (s == 0) ? Vector4{ 0.25f, 1.0f, 0.68f, 0.90f } : Vector4{ 0.78f, 1.0f, 0.90f, 0.85f };
				hp.endColor = { 0.10f, 0.75f, 0.45f, 0.0f };
				hp.hasColorShift = true;
				hp.lifeTime = 0.58f;
				hp.textureIndex = glintTex;
				hp.drag = 1.6f;
				hp.fadeInRatio = 0.15f;
				hp.twinkleSpeed = 14.0f;
				hp.twinklePhase = a;
				hp.slotLayer = 2;
				scene_->appParticleManager_->AddParticle(hp);
			}
		}
	}
	else
	{
		if (wasHealingPrev_ && curHpRatio > prevHpRatio_ + 0.04f)
		{
			// 治療完了の瞬間：周囲へ爽快に広がるエメラルド回復バースト
			for (int k = 0; k < 16; ++k)
			{
				const float a = static_cast<float>(k) * (GameConfig::Math::kTwoPi / 16.0f);
				AppParticle burst{};
				burst.transform.Initialize();
				burst.transform.SetTranslate(pPos + Vector3{ std::cos(a) * 0.32f, 0.52f, std::sin(a) * 0.32f });
				burst.scaleAspect = { 0.70f, 1.60f };
				burst.initialScale = 0.058f;
				burst.targetScale = 0.018f;
				burst.transform.SetScale({ 0.058f * 0.70f, 0.058f * 1.60f, 1.0f });
				burst.velocity = { std::cos(a) * 2.2f, 1.85f, std::sin(a) * 2.2f };
				burst.color = { 0.32f, 1.0f, 0.72f, 0.95f };
				burst.endColor = { 0.12f, 0.80f, 0.48f, 0.0f };
				burst.hasColorShift = true;
				burst.lifeTime = 0.62f;
				burst.textureIndex = glintTex;
				burst.drag = 2.4f;
				burst.twinkleSpeed = 16.0f;
				burst.twinklePhase = a;
				burst.slotLayer = 2;
				scene_->appParticleManager_->AddParticle(burst);
			}
		}
		healFxTimer_ = 0.0f;
	}
	wasHealingPrev_ = isHealing;
	prevHpRatio_ = curHpRatio;
}

void CharacterEffectController::UpdateEnemyEffects(float deltaTime)
{
	if (!scene_ || !scene_->particleManager || !scene_->appParticleManager_) return;

	const uint32_t glintTex = (scene_->sparkStreakTextureIndex_ != TextureManager::kInvalidTextureIndex) ? scene_->sparkStreakTextureIndex_ : scene_->particleTextureB;
	const uint32_t smokeTex = (scene_->wispySmokeTextureIndex_ != TextureManager::kInvalidTextureIndex) ? scene_->wispySmokeTextureIndex_ : scene_->smokeTextureIndex_;
	const uint32_t featherTex = (scene_->featherTextureIndex_ != TextureManager::kInvalidTextureIndex) ? scene_->featherTextureIndex_ : glintTex;
	AppParticleManager* addMgr = scene_->appParticleManager_.get();
	AppParticleManager* smokeMgr = scene_->smokeParticleManager_ ? scene_->smokeParticleManager_.get() : nullptr;

	auto& rng = scene_->particleManager->GetRandomEngine();
	std::uniform_real_distribution<float> offsetDist(-kGlintOffsetRange, kGlintOffsetRange);
	std::uniform_real_distribution<float> rotDist(0.0f, GameConfig::Math::kTwoPi);
	std::uniform_real_distribution<float> unitDist(-1.0f, 1.0f);

	enemyFxTimer_ += deltaTime;
	const bool emitContinuousTick = (enemyFxTimer_ >= 0.055f);
	if (emitContinuousTick)
	{
		enemyFxTimer_ = std::fmod(enemyFxTimer_, 0.055f);
	}

	const Vector3 playerPos = scene_->player_ ? scene_->player_->GetPosition() : Vector3{ 0.0f, 0.0f, 0.0f };
	constexpr float kMaxEnemyFxDistSq = 44.0f * 44.0f;

	for (auto& enemy : scene_->GetEnemies())
	{
		if (!enemy || enemy->IsDead()) continue;

		const Vector3 ePos = enemy->GetPosition();
		const float dx = ePos.x - playerPos.x;
		const float dz = ePos.z - playerPos.z;
		if (dx * dx + dz * dz > kMaxEnemyFxDistSq) continue;

		// 1. 索敵中の頭上戦術警戒グリント微粒子
		if (enemy->GetDetectionMeter() > 0.0f && enemy->GetAIState() != Enemy::AIState::Chase)
		{
			const float meter = enemy->GetDetectionMeter();
			if (std::rand() % kPercentModulo < static_cast<int>(kGlintBaseChance + meter * kGlintMeterChanceMul))
			{
				const Vector3 pos = ePos + Vector3{ offsetDist(rng), kGlintHeadOffsetY, offsetDist(rng) };
				const float rotZ = rotDist(rng);
				const Vector3 vel = { offsetDist(rng) * kGlintHorizVelMul, kGlintBaseUpVel + meter * kGlintMeterUpVelMul, offsetDist(rng) * kGlintHorizVelMul };
				const Vector4 col = { 1.0f, kGlintColorGreenBase - meter * kGlintColorGreenDrop, kGlintColorBlue, kGlintColorAlpha };
				EmitEffectParticle(
					addMgr, pos, rotZ, kGlintBaseScale + meter * kGlintMeterScaleMul, kGlintStyle.targetScaleMul,
					vel, col, kGlintEndColor, kGlintLifeTime, glintTex, kGlintStyle);
			}
		}

		if (!emitContinuousTick) continue;

		// 2. 被弾ダメージ蓄積に応じた装甲黒煙・ショート火花・抜け落ちる羽毛リーク演出
		const float hpRatio = std::clamp(enemy->GetHPRatio(), 0.0f, 1.0f);
		if (hpRatio < 0.99f)
		{
			const float dmg = 1.0f - hpRatio;
			if (smokeMgr && smokeTex != TextureManager::kInvalidTextureIndex && (dmg > 0.35f || (std::rand() & 1) == 0))
			{
				AppParticle sm{};
				sm.transform.Initialize();
				sm.transform.SetTranslate(ePos + Vector3{ unitDist(rng) * 0.22f, 0.52f + unitDist(rng) * 0.14f, unitDist(rng) * 0.22f });
				sm.transform.SetRotate({ 0.0f, 0.0f, rotDist(rng) });
				sm.scaleAspect = { 1.05f, 1.25f };
				sm.initialScale = 0.10f + dmg * 0.09f;
				sm.targetScale = sm.initialScale * 2.55f;
				sm.transform.SetScale({ sm.initialScale * sm.scaleAspect.x, sm.initialScale * sm.scaleAspect.y, 1.0f });
				sm.velocity = { unitDist(rng) * 0.32f, 0.85f + dmg * 0.95f, unitDist(rng) * 0.32f };
				const float shade = 0.34f - dmg * 0.18f;
				sm.color = { shade, shade * 0.94f, shade * 0.90f, 0.26f + dmg * 0.30f };
				sm.endColor = { 0.14f, 0.13f, 0.12f, 0.0f };
				sm.hasColorShift = true;
				sm.drag = 1.7f;
				sm.curlFreq = 2.8f;
				sm.curlAmp = 0.24f;
				sm.lifeTime = 0.58f + dmg * 0.38f;
				sm.fadeInRatio = 0.16f;
				sm.textureIndex = smokeTex;
				sm.slotLayer = 1;
				smokeMgr->AddParticle(sm);
			}

			// 重傷時（HP60%以下）：損傷アーマーから散る火花＆舞い落ちる黄色い羽毛
			if (dmg >= 0.40f && (std::rand() % 3 == 0))
			{
				AppParticle sp{};
				sp.transform.Initialize();
				sp.transform.SetTranslate(ePos + Vector3{ unitDist(rng) * 0.24f, 0.55f, unitDist(rng) * 0.24f });
				sp.scaleAspect = { 0.55f, 1.85f };
				sp.initialScale = 0.036f;
				sp.targetScale = 0.012f;
				sp.transform.SetScale({ 0.036f * 0.55f, 0.036f * 1.85f, 1.0f });
				sp.velocity = { unitDist(rng) * 2.2f, 1.35f + std::abs(unitDist(rng)) * 1.4f, unitDist(rng) * 2.2f };
				sp.color = { 1.0f, 0.78f, 0.26f, 0.94f };
				sp.endColor = { 1.0f, 0.28f, 0.05f, 0.0f };
				sp.hasColorShift = true;
				sp.gravity = 13.5f;
				sp.bounceElasticity = 0.38f;
				sp.drag = 1.2f;
				sp.lifeTime = 0.42f;
				sp.textureIndex = glintTex;
				sp.slotLayer = 2;
				addMgr->AddParticle(sp);

				if (smokeMgr)
				{
					AppParticle ft{};
					ft.transform.Initialize();
					ft.transform.SetTranslate(ePos + Vector3{ unitDist(rng) * 0.28f, 0.62f, unitDist(rng) * 0.28f });
					ft.transform.SetRotate({ 0.0f, 0.0f, rotDist(rng) });
					ft.scaleAspect = { 0.75f, 1.35f };
					ft.initialScale = 0.075f;
					ft.targetScale = 0.055f;
					ft.transform.SetScale({ 0.075f * 0.75f, 0.075f * 1.35f, 1.0f });
					ft.velocity = { unitDist(rng) * 1.05f, 0.65f, unitDist(rng) * 1.05f };
					ft.color = { 0.98f, 0.86f, 0.28f, 0.90f };
					ft.endColor = { 0.92f, 0.74f, 0.20f, 0.0f };
					ft.hasColorShift = true;
					ft.gravity = 1.85f;
					ft.drag = 2.6f;
					ft.curlFreq = 3.6f;
					ft.curlAmp = 0.34f;
					ft.lifeTime = 0.85f;
					ft.textureIndex = featherTex;
					ft.slotLayer = 2;
					smokeMgr->AddParticle(ft);
				}
			}
		}

		// 3. 強敵ボス「看守長 (Warden)」専用の威圧クリムゾン・エンバー（火の粉オーラ）
		if (enemy->GetTacticalRole() == Enemy::TacticalRole::Warden)
		{
			const float a = rotDist(rng);
			const float r = 0.46f * enemy->GetRoleScale();
			AppParticle bossP{};
			bossP.transform.Initialize();
			bossP.transform.SetTranslate(ePos + Vector3{ std::cos(a) * r, 0.22f + std::abs(unitDist(rng)) * 0.65f, std::sin(a) * r });
			bossP.scaleAspect = { 0.68f, 1.62f };
			bossP.initialScale = 0.052f;
			bossP.targetScale = 0.016f;
			bossP.transform.SetScale({ 0.052f * 0.68f, 0.052f * 1.62f, 1.0f });
			bossP.velocity = { -std::sin(a) * 0.85f, 1.45f + std::abs(unitDist(rng)) * 0.85f, std::cos(a) * 0.85f };
			bossP.color = { 1.0f, 0.28f, 0.12f, 0.92f };
			bossP.endColor = { 1.0f, 0.72f, 0.16f, 0.0f };
			bossP.hasColorShift = true;
			bossP.drag = 1.4f;
			bossP.curlFreq = 3.0f;
			bossP.curlAmp = 0.24f;
			bossP.lifeTime = 0.68f;
			bossP.fadeInRatio = 0.14f;
			bossP.twinkleSpeed = 16.0f;
			bossP.twinklePhase = a;
			bossP.textureIndex = glintTex;
			bossP.slotLayer = 2;
			addMgr->AddParticle(bossP);
		}

		// 4. 射撃直前の銃口レーザーロックオン予兆グリント粒子
		const float aimProg = enemy->GetAimTelegraphProgress();
		if (aimProg > 0.25f)
		{
			const Vector3 muzPos = enemy->GetMuzzleWorldPosition();
			AppParticle mz{};
			mz.transform.Initialize();
			mz.transform.SetTranslate(muzPos + Vector3{ unitDist(rng) * 0.04f, unitDist(rng) * 0.04f, unitDist(rng) * 0.04f });
			mz.scaleAspect = { 0.70f, 1.50f };
			mz.initialScale = 0.032f + aimProg * 0.032f;
			mz.targetScale = mz.initialScale * 0.25f;
			mz.transform.SetScale({ mz.initialScale * mz.scaleAspect.x, mz.initialScale * mz.scaleAspect.y, 1.0f });
			mz.velocity = { unitDist(rng) * 0.55f, 0.35f + aimProg * 0.55f, unitDist(rng) * 0.55f };
			mz.color = { 1.0f, 0.22f + (1.0f - aimProg) * 0.45f, 0.12f, 0.88f };
			mz.endColor = { 1.0f, 0.08f, 0.04f, 0.0f };
			mz.hasColorShift = true;
			mz.drag = 2.4f;
			mz.lifeTime = 0.24f;
			mz.twinkleSpeed = 22.0f;
			mz.twinklePhase = rotDist(rng);
			mz.textureIndex = glintTex;
			mz.slotLayer = 2;
			addMgr->AddParticle(mz);
		}
	}
}
