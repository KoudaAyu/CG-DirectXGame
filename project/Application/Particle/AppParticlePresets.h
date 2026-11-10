#pragma once
#include <array>
#include <cstdint>
#include "Vector.h"
#include "Application/Config/GameConfig.h"

namespace AppParticlePresets
{
	// 汎用1次元レンジ (Min-Max Range)
	struct RangeF { float min = 0.0f; float max = 0.0f; };

	// 色補間設定 (Color & Shift Spec)
	struct ColorShiftSpec
	{
		Vector4 color      = { 1.0f, 1.0f, 1.0f, 1.0f };
		Vector4 endColor   = { 0.0f, 0.0f, 0.0f, 0.0f };
		bool hasColorShift = false;
	};

	// スケール補間・アスペクト比設定 (Scale & Aspect Spec)
	struct ScaleSpec
	{
		Vector2 aspect       = { 1.0f, 1.0f };
		float initScaleMul   = 0.0f;
		float targetScaleMul = 0.0f;
	};

	// 物理・光学ダイナミクス設定 (Physics & Optical Dynamics Spec)
	struct DynamicsSpec
	{
		float gravity          = 0.0f;
		float drag             = 0.0f;
		float bounceElasticity = 0.0f;
		float curlFreq         = 0.0f;
		float curlAmp          = 0.0f;
		float fadeInRatio      = 0.0f;
		float twinkleSpeed     = 0.0f;
		uint16_t slotLayer     = 0;
	};

	// ボックス拡散放出プリセット (Box-Jittered Burst Preset: Dust / Feather / BloodMist / Smoke)
	struct JitterEmitPreset
	{
		float posJitter       = 0.0f;
		RangeF scaleX         = {};
		RangeF scaleY         = {};
		bool uniformScale     = false;
		RangeF velXZ          = {};
		RangeF velY           = {};
		RangeF life           = {};
		RangeF spin           = {};
		DynamicsSpec dynamics = {};
	};

	// 静止スタンプフレア層設定 (MuzzleFlare / DeathFlash Stamp Layers)
	struct MuzzleFlareLayerSpec
	{
		float rotZ       = 0.0f;
		Vector2 scaleMul = { 1.0f, 1.0f };
	};

	struct DeathFlashLayerSpec
	{
		float rotZ          = 0.0f;
		bool randomRot      = false;
		float scaleMul      = 1.0f;
		float lifeMul       = 1.0f;
		float angularVel    = 0.0f;
		bool forceWhiteCore = false;
	};

	// --- 1. コア・バッチ描画・物理フィールド定数 (Core / Batch / Force Fields) ---
	namespace Core
	{
		constexpr uint32_t kQuadVertexCount = 6;
		constexpr size_t kMatPaddingFloats = 3, kMatrixTranslationRow = 3, kMaxActiveParticles = 7600;
		constexpr float kBounceHorizDamping = 0.7f, kCurlTimeFreqY = 4.2f, kCurlTimeFreqX = 3.8f;
		constexpr float kTwinkleBase = 0.70f, kTwinkleAmplitude = 0.30f, kFadeInEpsilon = 0.001f, kSmoothstepCubicC1 = 3.0f;
		constexpr float kFixedFrameDeltaTime = 1.0f / 60.0f;

		// マウス水面攪乱 & ヘリローター吹き下ろし (Disturbance & Rotor Downwash)
		constexpr float kDisturbMinDistSq = 0.0001f, kDisturbPushForceScale = 1.8f, kDisturbVelInherit = 0.35f, kDisturbMinWobbleAmp = 0.35f;
		constexpr float kDownwashMinEpsilon = 0.001f, kDownwashRadiusMul = 2.2f, kDownwashMinRelY = -0.5f, kDownwashMaxRelY = 9.0f;
		constexpr float kDownwashMinDistSq = 0.0001f, kDownwashMinHeightFactor = 0.15f;
		constexpr float kDownwashVertForceScale = 1.8f, kOutflowMaxHeightY = 2.5f, kOutflowBaseSpeedMul = 0.6f, kOutflowProfileSpeedMul= 1.8f;
		constexpr float kOutflowWobbleScale = 0.28f, kGroundSlideClampY = 0.02f;
	}

	// --- 2. 火花・薬莢・マズル・着弾・破砕プリセット (Ballistics & Sparks) ---
	namespace Ballistics
	{
		// EmitSpark / EmitSparkWithVelocity
		constexpr float kGpuEmitMinScaleThreshold = 0.1f, kGpuEmitFallbackRadius = 0.4f, kSparkScaleRef = 0.12f;
		constexpr uint32_t kGpuEmitSparkCount = 6, kGpuEmitSparkVelCount = 4;
		constexpr RangeF kSparkScaleX = { 0.02f, 0.06f }, kSparkScaleY = { 0.15f, 0.45f };
		constexpr RangeF kSparkVelXZ = { -4.0f, 4.0f }, kSparkVelY = { 2.0f, 6.0f }, kSparkSpin = { -10.0f, 10.0f };
		constexpr DynamicsSpec kSparkDyn = { 9.8f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0 };

		// EmitShellCasing & EmitMuzzleFlash
		constexpr float kShellRightOffset = 0.15f, kShellUpOffset = 0.1f, kShellLifeTime = 2.0f;
		constexpr RangeF kShellForceRight = { 1.8f, 3.2f }, kShellForceUp = { 1.5f, 3.0f }, kShellForceBack = { -1.2f, -0.4f }, kShellSpin = { 15.0f, 30.0f };
		constexpr DynamicsSpec kShellDyn = { 9.8f, 0.0f, 0.4f, 0.0f, 0.0f, 0.0f, 0.0f, 0 };

		constexpr float kMuzzleFlashScale = 0.3f, kMuzzleRightSpread = 1.8f, kMuzzleUpSpread = 1.2f;
		constexpr RangeF kMuzzleForwardSpeed = { 4.0f, 8.0f }, kMuzzleLifeTime = { 0.05f, 0.15f };

		// EmitMuzzleFlare / EmitDeathFlash
		constexpr std::array<MuzzleFlareLayerSpec, 3> kMuzzleFlareLayers = { {
			{ 0.0f,                         { 1.5f, 0.3f } },
			{ GameConfig::Math::kHalfPi,    { 1.5f, 0.3f } },
			{ GameConfig::Math::kQuarterPi, { 0.7f, 0.7f } },
		} };

		constexpr std::array<DeathFlashLayerSpec, 3> kDeathFlashLayers = { {
			{ 0.0f,                         false, 1.5f, 1.0f,  0.5f, false },
			{ GameConfig::Math::kQuarterPi, false, 1.1f, 0.8f, -0.8f, false },
			{ 0.0f,                         true,  0.7f, 0.6f,  1.2f, true  },
		} };

		// EmitRicochetSparks
		constexpr int kRicochetCount = 20;
		constexpr RangeF kRicochetSpread = { -0.65f, 0.65f }, kRicochetSpeed = { 3.5f, 9.5f }, kRicochetLife = { 0.18f, 0.48f };
		constexpr float kRicochetUpBias = 0.25f, kRicochetUpSpreadMul = 0.45f, kRicochetRotScale = 4.8f;
		constexpr ScaleSpec kRicochetScale = { { 0.020f, 0.12f }, 1.0f, 0.25f };
		constexpr Vector4 kRicochetColorEven = { 1.0f, 0.92f, 0.55f, 0.96f }, kRicochetColorOdd = { 1.0f, 0.62f, 0.18f, 0.92f }, kRicochetEndColor = { 1.0f, 0.25f, 0.04f, 0.0f };
		constexpr DynamicsSpec kRicochetDyn = { 12.0f, 1.6f, 0.45f, 0.0f, 0.0f, 0.0f, 0.0f, 2 };

		// EmitTargetDestroyGPUBurst & EmitLandingGPUBurst
		constexpr float kTargetCoreInitScale = 0.42f, kTargetCoreTargetScale = 0.14f, kTargetCoreLife = 0.055f;
		constexpr ColorShiftSpec kTargetCoreColor = { { 1.0f, 0.94f, 0.72f, 0.92f }, { 1.0f, 0.42f, 0.10f, 0.0f }, true };
		constexpr int kTargetSparkCount = 38, kTargetHotColorStride = 3;
		constexpr RangeF kTargetPitch = { -0.35f, 1.15f }, kTargetSpeed = { 4.2f, 11.5f }, kTargetLife = { 0.22f, 0.62f };
		constexpr RangeF kTargetWidth = { 0.018f, 0.036f }, kTargetLen = { 0.09f, 0.24f };
		constexpr float kTargetUpBoost = 1.6f, kTargetSpawnDirOffset = 0.08f, kTargetTargetScale = 0.25f, kTargetSpinSpeed = 9.0f;
		constexpr Vector4 kTargetColorHot = { 1.0f, 0.97f, 0.84f, 1.0f }, kTargetColorWarm = { 1.0f, 0.76f, 0.28f, 0.96f }, kTargetEndColor = { 1.0f, 0.26f, 0.04f, 0.0f };
		constexpr DynamicsSpec kTargetSparkDyn = { 12.8f, 1.6f, 0.48f, 0.0f, 0.0f, 0.0f, 0.0f, 2 };

		constexpr int kLandingSparkCount = 14;
		constexpr RangeF kLandingSpeed = { 2.2f, 5.0f }, kLandingLife = { 0.16f, 0.34f }, kLandingUpVel = { 0.65f, 2.05f };
		constexpr float kLandingSpawnRadius = 0.14f, kLandingSpawnY = 0.05f;
		constexpr ScaleSpec kLandingScale = { { 0.020f, 0.095f }, 1.0f, 0.28f };
		constexpr ColorShiftSpec kLandingColor = { { 1.0f, 0.86f, 0.46f, 0.88f }, { 1.0f, 0.35f, 0.08f, 0.0f }, true };
		constexpr DynamicsSpec kLandingDyn = { 12.0f, 2.0f, 0.40f, 0.0f, 0.0f, 0.0f, 0.0f, 2 };
	}

	// --- 3. 血液・羽毛・被弾撃破バーストプリセット (Blood, Feathers & Gore) ---
	namespace Gore
	{
		constexpr JitterEmitPreset kFeatherPreset = {
			0.2f, { 0.05f, 0.12f }, { 0.15f, 0.35f }, false, { -2.0f, 2.0f }, { 1.5f, 4.0f }, { 1.2f, 2.2f }, { -5.0f, 5.0f },
			{ 3.5f, 0.0f, 0.3f, 0.0f, 0.0f, 0.0f, 0.0f, 0 } };
		constexpr JitterEmitPreset kBloodDropPreset = {
			0.1f, { 0.04f, 0.14f }, { 0.08f, 0.28f }, false, { -2.5f, 2.5f }, { 1.0f, 5.5f }, { 0.8f, 1.8f }, { -8.0f, 8.0f },
			{ 14.0f, 0.0f, 0.08f, 0.0f, 0.0f, 0.0f, 0.0f, 0 } };
		constexpr JitterEmitPreset kBloodMistPreset = {
			0.2f, { 0.3f, 0.8f }, {}, true, { -0.8f, 0.8f }, { 0.2f, 1.2f }, { 0.6f, 1.2f }, { 0.5f, 0.5f },
			{ 0.8f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0 } };
		constexpr JitterEmitPreset kDarkSmokePreset = {
			0.25f, { 0.4f, 0.9f }, {}, true, { -0.6f, 0.6f }, { 0.3f, 1.5f }, { 0.7f, 1.4f }, { 0.4f, 0.4f },
			{ -0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0 } };

		constexpr RangeF kBloodDropRedTone = { 0.65f, 0.95f }, kBloodDropDarkTone = { 0.02f, 0.08f }, kDarkSmokeAlpha = { 0.35f, 0.65f };
		constexpr float kBloodDropAlpha = 0.95f;
		constexpr Vector4 kBloodMistColor = { 0.85f, 0.05f, 0.05f, 0.65f };
		constexpr Vector3 kDarkSmokeRgb = { 0.75f, 0.75f, 0.78f };

		// EmitBloodBurst
		constexpr float kBloodBurstFlashScale = 5.0f, kBloodBurstFlashLife = 0.35f, kBloodBurstDropSpeed = 2.0f, kBloodBurstMistScale = 1.8f;
		constexpr int kBloodBurstDropCount = 60, kBloodBurstRingCount = 30, kBloodBurstMistCount = 25, kBloodBurstFeatherCount = 40;
		constexpr RangeF kBloodBurstRingSpeed = { 2.5f, 6.0f }, kBloodBurstRingUp = { 1.5f, 5.5f };
		constexpr Vector4 kBloodBurstFlashColor = { 0.95f, 0.05f, 0.05f, 1.0f }, kBloodFeatherRed = { 0.9f, 0.1f, 0.1f, 1.0f }, kBloodFeatherYellow = { 1.0f, 0.85f, 0.2f, 1.0f };

		// EmitViolentBloodSpray
		constexpr int kSprayCritCount = 24, kSprayNormalCount = 12, kSprayCritSmoke = 4, kSprayNormSmoke = 2;
		constexpr float kSprayPosJitter = 0.08f, kSprayMinFwdSpeed = 3.0f, kSprayCritMaxFwd = 8.0f, kSprayNormMaxFwd = 5.5f, kSpraySpin = 6.0f, kSpraySmokeScale = 0.7f;
		constexpr RangeF kSprayScaleX = { 0.06f, 0.18f }, kSprayScaleY = { 0.12f, 0.40f }, kSprayConeSpread = { -1.2f, 1.2f }, kSprayUpSpeed = { 0.8f, 3.5f }, kSprayLife = { 0.35f, 0.75f };
		constexpr Vector4 kSpraySparkColor = { 1.0f, 0.88f, 0.35f, 0.95f }, kSprayDebrisColor = { 0.98f, 0.96f, 0.90f, 0.90f };
		constexpr DynamicsSpec kSprayDyn = { 12.0f, 0.0f, 0.05f, 0.0f, 0.0f, 0.0f, 0.0f, 0 };

		// EmitViolentBloodBurst
		constexpr float kViolentFlashScale = 6.0f, kViolentFlashLife = 0.40f, kViolentDirJitter = 0.15f, kViolentDirOffsetY = 0.4f;
		constexpr float kViolentDirLife = 0.8f, kViolentDirSpin = 8.0f, kViolentRingLife = 1.0f, kViolentRingSpin = 10.0f, kViolentSmokeOffsetY = 0.5f, kViolentSmokeScale = 1.5f;
		constexpr int kViolentDirCount = 50, kViolentRingCount = 40, kViolentSmokeCount = 20, kViolentFeatherCount = 35;
		constexpr RangeF kViolentDirScaleX = { 0.08f, 0.22f }, kViolentDirScaleY = { 0.15f, 0.50f }, kViolentDirSpreadXZ = { -3.5f, 3.5f }, kViolentDirSpreadY = { 2.0f, 7.0f }, kViolentDirFwd = { 2.0f, 6.5f };
		constexpr RangeF kViolentRingSpeed = { 3.0f, 7.5f }, kViolentRingUp = { 1.8f, 6.8f }, kViolentRingScale = { 0.08f, 0.20f };
		constexpr Vector4 kViolentFlashColor = { 1.0f, 0.92f, 0.45f, 1.0f };
		constexpr Vector4 kViolentDirColorEven = { 1.0f, 0.85f, 0.25f, 1.0f }, kViolentDirColorOdd = { 0.98f, 0.98f, 1.0f, 0.95f };
		constexpr Vector4 kViolentRingColorEven = { 1.0f, 0.85f, 0.30f, 1.0f }, kViolentRingColorOdd = { 0.95f, 0.95f, 1.0f, 0.95f };
		constexpr Vector4 kViolentFeatherYellow = { 1.0f, 0.88f, 0.22f, 1.0f }, kViolentFeatherWhite = { 0.95f, 0.95f, 0.98f, 1.0f };
		constexpr DynamicsSpec kViolentDirDyn = { 12.0f, 0.0f, 0.05f, 0.0f, 0.0f, 0.0f, 0.0f, 0 };
		constexpr DynamicsSpec kViolentRingDyn = { 14.0f, 0.0f, 0.05f, 0.0f, 0.0f, 0.0f, 0.0f, 0 };

		// EmitEnemyDestroyGPUBurst
		constexpr float kEnemyDestroyFlashScale = 4.8f, kEnemyDestroyFlashLife = 0.28f, kEnemyDestroyHitBias = 0.8f, kEnemyDestroyUpBoost = 1.5f, kEnemyDestroyDustScale = 0.9f, kEnemyDestroyDustLife = 0.8f;
		constexpr int kEnemyDestroySparkCount = 80, kEnemyDestroyGoldStride = 3, kEnemyDestroyDustCount = 30, kEnemyDestroyFeatherCount = 40;
		constexpr RangeF kEnemyDestroyPitch = { -0.8f, 1.2f }, kEnemyDestroySpeed = { 3.5f, 9.5f }, kEnemyDestroyScale = { 0.08f, 0.22f }, kEnemyDestroyLife = { 0.5f, 1.4f };
		constexpr RangeF kEnemyDestroyDustSpeed = { 1.5f, 3.5f }, kEnemyDestroyDustUp = { 0.3f, 1.1f };
		constexpr Vector4 kEnemyDestroyFlashColor = { 1.0f, 0.85f, 0.3f, 1.0f }, kEnemyDestroyDustColor = { 0.4f, 0.35f, 0.3f, 0.55f };
		constexpr Vector4 kEnemyDestroySparkGold = { 1.0f, 0.95f, 0.5f, 1.0f }, kEnemyDestroySparkOrange = { 1.0f, 0.45f, 0.1f, 1.0f };
		constexpr Vector4 kEnemyDestroyFeatherEven = { 1.0f, 0.88f, 0.25f, 1.0f }, kEnemyDestroyFeatherOdd = { 0.95f, 0.2f, 0.1f, 1.0f };
	}

	// --- 4. 土埃・移動アクションプリセット (Dust & Locomotion) ---
	namespace Dust
	{
		constexpr JitterEmitPreset kGenericDustPreset = {
			0.15f, { 0.12f, 0.28f }, {}, true, { -0.4f, 0.4f }, { 0.1f, 0.3f }, { 0.3f, 0.55f }, {},
			{ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0 } };

		constexpr int kDodgeCount = 4, kStepCount = 2;
		constexpr RangeF kDodgeSpread = { -0.32f, 0.32f }, kDodgeSpeed = { 1.3f, 2.6f }, kDodgeInitScale = { 0.18f, 0.28f }, kDodgeLife = { 0.36f, 0.62f }, kDodgeUpVel = { 0.22f, 0.60f };
		constexpr float kDodgeBackOffset = 0.36f, kDodgeBaseLiftY = -0.05f, kDodgeJitterScale = 0.25f, kDodgeSpawnY = 0.03f, kDodgeSpinScale = 3.5f;
		constexpr ScaleSpec kDodgeScaleSpec = { { 1.45f, 0.78f }, 1.0f, 2.45f };
		constexpr ColorShiftSpec kDodgeColor = { { 0.62f, 0.54f, 0.42f, 0.38f }, { 0.68f, 0.62f, 0.52f, 0.0f }, true };
		constexpr DynamicsSpec kDodgeDyn = { -0.10f, 2.8f, 0.0f, 1.8f, 0.22f, 0.14f, 0.0f, 1 };

		constexpr RangeF kStepSpread = { -0.14f, 0.14f }, kStepInitScale = { 0.11f, 0.16f }, kStepUpVel = { 0.16f, 0.40f }, kStepLife = { 0.34f, 0.48f };
		constexpr float kStepSpawnY = 0.035f, kStepHorizVelMul = 2.4f, kStepSpinScale = 4.0f;
		constexpr ScaleSpec kStepScaleSpec = { { 1.38f, 0.72f }, 1.0f, 2.25f };
		constexpr ColorShiftSpec kStepColor = { { 0.60f, 0.52f, 0.40f, 0.30f }, { 0.66f, 0.60f, 0.50f, 0.0f }, true };
		constexpr DynamicsSpec kStepDyn = { 0.0f, 2.6f, 0.0f, 0.0f, 0.0f, 0.15f, 0.0f, 1 };
	}

	// --- 5. 水面・河川・飛沫・マウス波紋プリセット (Water, River & Wake) ---
	namespace Water
	{
		// EmitWaterSplash
		constexpr int kSplashCount = 22;
		constexpr RangeF kSplashSpeed = { 0.9f, 2.8f }, kSplashInitScale = { 0.045f, 0.100f }, kSplashUpVel = { 2.4f, 5.6f }, kSplashLife = { 0.36f, 0.60f };
		constexpr float kSplashRadius = 0.06f, kSplashSpawnY = 0.02f;
		constexpr ScaleSpec kSplashScale = { { 0.82f, 1.35f }, 1.0f, 0.55f };
		constexpr ColorShiftSpec kSplashColor = { { 0.92f, 0.97f, 1.0f, 0.85f }, { 0.72f, 0.90f, 0.98f, 0.0f }, true };
		constexpr DynamicsSpec kSplashDyn = { 13.8f, 0.6f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 2 };

		// EmitRiverWaveRipples
		constexpr int kRippleCount = 7, kRippleFoamStride = 3, kPillarSprayChance = 55, kPercentModulo = 100;
		constexpr RangeF kRippleXRange = { -34.0f, 36.0f }, kRippleGlintScale = { 0.055f, 0.125f }, kRippleLife = { 1.1f, 2.2f };
		constexpr float kBridgeCenterAbsX = 14.0f, kBridgeExcludeHalf = 1.75f, kRippleDriftZ = 0.22f, kRippleFadeIn = 0.22f;
		constexpr float kRippleGlintLifeMul = 0.75f, kRippleGlintTwinkle = 14.0f, kRipplePhaseStep = 1.1f, kPillarFlowSpeed = 2.6f;
		constexpr ScaleSpec kRippleGlintSpec = { { 1.45f, 0.75f }, 1.0f, 0.35f }, kRippleFoamSpec = { { 2.35f, 0.42f }, 0.85f, 1.15f };
		constexpr ColorShiftSpec kRippleGlintColor = { { 0.92f, 0.99f, 1.0f, 0.72f }, { 0.45f, 0.88f, 1.0f, 0.0f }, true };
		constexpr ColorShiftSpec kRippleFoamColor = { { 0.88f, 0.97f, 1.0f, 0.32f }, { 0.58f, 0.86f, 0.96f, 0.0f }, true };
		constexpr std::array<float, 2> kBridgePillarXs = { -12.42f, 15.58f };

		// EmitRiverSplashDroplet
		constexpr int kRiverDropCount = 8;
		constexpr RangeF kRiverDropSpeed = { 0.8f, 2.2f }, kRiverDropScale = { 0.025f, 0.055f }, kRiverDropUpVel = { 1.2f, 2.8f }, kRiverDropLife = { 0.38f, 0.60f };
		constexpr float kRiverDropJitterXZ = 0.1f, kRiverDropSpawnY = 0.01f;
		constexpr ScaleSpec kRiverDropScSpec = { { 1.0f, 1.0f }, 0.8f, 0.4f };
		constexpr ColorShiftSpec kRiverDropColor = { { 0.92f, 0.97f, 1.0f, 0.90f }, { 0.65f, 0.88f, 1.0f, 0.15f }, true };
		constexpr DynamicsSpec kRiverDropDyn = { 13.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0 };

		// EmitRealisticRiverSpray (Droplets / Mist / Spume)
		constexpr RangeF kSprayLatJitter = { -0.35f, 0.35f }, kSpraySpeedJitter = { 0.7f, 1.4f }, kSprayUpVel = { 0.75f, 1.9f };
		constexpr RangeF kSprayDropScale = { 0.022f, 0.048f }, kSprayDropLife = { 0.30f, 0.50f };
		constexpr int kSprayDropCount = 8, kSprayMistCount = 3, kSpraySpumeCount = 2;
		constexpr float kSprayDropSpawnY = 0.015f, kSprayDropLatVelMul = 0.8f, kSprayMistLatMul = 0.8f, kSprayMistSpawnY = 0.03f;
		constexpr float kSprayMistFwdMul = 0.65f, kSprayMistUpVel = 0.20f, kSpraySpumeLatMul = 0.6f, kSpraySpumeFwdMul = 0.72f;
		constexpr RangeF kSprayMistScale = { 0.055f, 0.100f }, kSprayMistLife = { 0.60f, 0.95f }, kSpraySpumeScale = { 0.040f, 0.075f }, kSpraySpumeLife = { 1.0f, 1.5f };
		constexpr ScaleSpec kSprayDropScSpec = { { 1.0f, 1.0f }, 0.8f, 0.35f }, kSprayMistScSpec = { { 1.0f, 1.0f }, 1.0f, 2.8f }, kSpraySpumeScSpec = { { 1.0f, 1.0f }, 0.9f, 1.5f };
		constexpr ColorShiftSpec kSprayDropColor = { { 0.92f, 0.97f, 1.0f, 0.95f }, { 0.65f, 0.88f, 1.0f, 0.15f }, true };
		constexpr ColorShiftSpec kSprayMistColor = { { 0.96f, 0.98f, 1.0f, 0.28f }, { 0.82f, 0.92f, 1.0f, 0.0f }, true };
		constexpr ColorShiftSpec kSpraySpumeColor = { { 1.0f, 1.0f, 1.0f, 0.60f }, { 0.75f, 0.92f, 1.0f, 0.0f }, true };
		constexpr DynamicsSpec kSprayDropDyn = { 14.0f, 0.6f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0 };
		constexpr DynamicsSpec kSprayMistDyn = { 0.05f, 3.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0 };
		constexpr DynamicsSpec kSpraySpumeDyn = { 0.0f, 0.4f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0 };

		// EmitDuckHullSplash & EmitMouseWaterSplashFling & EmitShorelineWave
		constexpr int kDuckBowCount = 3;
		constexpr float kDuckBowFlankSpan = 0.28f, kDuckBowFwdOffset = 0.16f, kDuckBowSpawnY = 0.015f, kDuckBowLatVelMul = 3.0f, kDuckBowFwdVel = 0.20f;
		constexpr float kDuckWakeBackSpan = 0.22f, kDuckWakeSideOff = 0.20f, kDuckWakeFwdVel = 0.70f, kDuckWakeSideVel = 0.12f;
		constexpr RangeF kDuckBowScale = { 0.016f, 0.038f }, kDuckBowUpVel = { 0.75f, 1.50f }, kDuckBowLife = { 0.25f, 0.40f };
		constexpr RangeF kDuckWakeScale = { 0.030f, 0.055f }, kDuckWakeLife = { 0.80f, 1.20f };
		constexpr ScaleSpec kDuckWakeScSpec = { { 1.0f, 1.0f }, 1.0f, 1.5f };
		constexpr ColorShiftSpec kDuckBowColor = { { 0.94f, 0.98f, 1.0f, 0.92f }, { 0.70f, 0.90f, 1.0f, 0.15f }, true };
		constexpr ColorShiftSpec kDuckWakeColor = { { 0.96f, 0.99f, 1.0f, 0.55f }, { 0.65f, 0.88f, 1.0f, 0.0f }, true };
		constexpr DynamicsSpec kDuckBowDyn = { 14.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0 };
		constexpr DynamicsSpec kDuckWakeDyn = { 0.0f, 0.8f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0 };

		constexpr int kFlingDropCount = 10, kFlingMistCount = 2;
		constexpr float kFlingSpdScale = 0.25f, kFlingPosJitter = 0.10f, kFlingDropSpawnY = 0.015f, kFlingLatSpread = 1.6f;
		constexpr float kFlingMistSpawnY = 0.025f, kFlingMistFwdMul = 0.35f, kFlingMistUpVel = 0.40f;
		constexpr RangeF kFlingSpdClamp = { 1.2f, 4.5f }, kFlingDropScale = { 0.022f, 0.052f }, kFlingDropUpVel = { 1.2f, 3.2f }, kFlingDropLife = { 0.32f, 0.54f };
		constexpr RangeF kFlingMistScale = { 0.06f, 0.11f }, kFlingMistLife = { 0.55f, 0.80f };
		constexpr ScaleSpec kFlingMistScSpec = { { 1.0f, 1.0f }, 1.0f, 2.4f };
		constexpr ColorShiftSpec kFlingDropColor = { { 0.94f, 0.98f, 1.0f, 0.95f }, { 0.55f, 0.85f, 1.0f, 0.15f }, true };
		constexpr ColorShiftSpec kFlingMistColor = { { 0.95f, 0.98f, 1.0f, 0.30f }, { 0.80f, 0.92f, 1.0f, 0.0f }, true };
		constexpr DynamicsSpec kFlingDropDyn = { 14.5f, 0.4f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0 };
		constexpr DynamicsSpec kFlingMistDyn = { 0.1f, 3.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0 };

		constexpr RangeF kShoreSpeedJitter = { 0.35f, 0.75f }, kShoreLatJitter = { -0.04f, 0.04f }, kShoreLife = { 0.60f, 0.85f };
		constexpr float kShoreSpawnY = 0.003f, kShoreBaseSpeed = 0.42f, kShoreInitScale = 0.025f, kShoreTargetScale = 0.035f;
		constexpr Vector2 kShoreQuadSize = { 0.007f, 0.035f };
		constexpr ColorShiftSpec kShoreColor = { { 0.75f, 0.95f, 1.0f, 0.10f }, { 0.50f, 0.85f, 1.0f, 0.0f }, true };
		constexpr DynamicsSpec kShoreDyn = { 0.0f, 0.4f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0 };

		// EmitMouseWaveWake & EmitMouseRippleRing
		constexpr int kWakeBandCount = 2, kRingCount = 20;
		constexpr float kWakeDirEpsilon = 0.001f, kWakePowerSpeedMul = 0.22f, kWakeSideBaseOff = 0.05f, kWakeSideStepOff = 0.07f;
		constexpr float kWakeBackStepOff = 0.035f, kWakeBackVelMul = 0.20f, kWakeUpVel = 0.08f, kWakeScalePowerMul = 0.15f;
		constexpr RangeF kWakeJitter = { -0.025f, 0.025f }, kWakeSpread = { 0.35f, 0.85f }, kWakeLife = { 0.7f, 1.3f }, kWakeScale = { 0.032f, 0.058f }, kWakeSpin = { -1.2f, 1.2f }, kWakePowerClamp = { 0.4f, 2.5f };
		constexpr ScaleSpec kWakeScaleSpec = { { 1.0f, 1.0f }, 0.7f, 1.6f };
		constexpr ColorShiftSpec kWakeColorInner = { { 0.55f, 0.90f, 1.0f, 0.45f }, { 0.18f, 0.65f, 0.92f, 0.12f }, true };
		constexpr ColorShiftSpec kWakeColorOuter = { { 0.25f, 0.80f, 0.98f, 0.35f }, { 0.12f, 0.48f, 0.88f, 0.06f }, true };
		constexpr DynamicsSpec kWakeDyn = { -0.03f, 2.2f, 0.0f, 2.0f, 0.04f, 0.0f, 0.0f, 0 };

		constexpr RangeF kRingJitter = { -0.02f, 0.02f }, kRingLife = { 0.7f, 1.2f }, kRingScale = { 0.032f, 0.055f };
		constexpr float kRingExpSpeedMul = 1.1f, kRingSpawnRadius = 0.05f;
		constexpr ScaleSpec kRingScaleSpec = { { 1.0f, 1.0f }, 0.6f, 1.7f };
		constexpr ColorShiftSpec kRingColor = { { 0.50f, 0.88f, 1.0f, 0.40f }, { 0.15f, 0.58f, 0.90f, 0.08f }, true };
		constexpr DynamicsSpec kRingDyn = { 0.0f, 2.4f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0 };
	}

	// --- 6. ヘリパッド誘導灯・ミノフスキー粒子プリセット (Beacon & Minovsky) ---
	namespace Minovsky
	{
		// EmitHelipadBeaconMotes
		constexpr int kBeaconCount = 2;
		constexpr RangeF kBeaconRadius = { 0.2f, 2.4f }, kBeaconInitScale = { 0.022f, 0.040f }, kBeaconUpVel = { 1.1f, 2.3f }, kBeaconLife = { 1.05f, 1.60f };
		constexpr float kBeaconSpawnY = 0.08f, kBeaconDriftVelX = 0.28f, kBeaconDriftVelZ = 0.34f, kBeaconRadialVel = 0.18f;
		constexpr ScaleSpec kBeaconScale = { { 0.85f, 1.45f }, 1.0f, 0.35f };
		constexpr ColorShiftSpec kBeaconColor = { { 0.35f, 1.0f, 0.62f, 0.68f }, { 0.12f, 0.75f, 0.42f, 0.0f }, true };
		constexpr DynamicsSpec kBeaconDyn = { 0.0f, 0.45f, 0.0f, 1.4f, 0.26f, 0.20f, 12.0f, 2 };

		// EmitMinovskySwirl & Stream
		constexpr RangeF kSwirlJitter = { -0.04f, 0.04f }, kSwirlScaleJitter = { 0.8f, 1.25f }, kSwirlLife = { 1.2f, 2.2f }, kSwirlSpin = { -6.0f, 6.0f }, kSwirlTwinkleSpeed = { 8.0f, 18.0f };
		constexpr float kSwirlTangentY = 0.38f, kSwirlTangentSpd = 0.85f, kSwirlUpVel = 0.55f, kSwirlWobbleFreq = 4.0f, kSwirlWobbleJitter = 10.0f, kSwirlWobbleAmp = 0.22f, kSwirlAxisAngleMul = 1.5f, kSwirlAxisY = 0.2f;
		constexpr ScaleSpec kSwirlScale = { { 1.0f, 1.0f }, 0.4f, 1.15f };
		constexpr ColorShiftSpec kSwirlMagenta = { { 1.0f, 0.22f, 0.90f, 0.95f }, { 0.25f, 0.85f, 1.0f, 0.95f }, true };
		constexpr ColorShiftSpec kSwirlCyan = { { 0.12f, 0.98f, 1.0f, 0.95f }, { 0.95f, 0.20f, 0.85f, 0.95f }, true };
		constexpr DynamicsSpec kSwirlDyn = { -0.18f, 0.75f, 0.0f, 3.2f, 0.18f, 0.0f, 0.0f, 0 };

		constexpr RangeF kStreamJitter = { -0.08f, 0.08f }, kStreamLife = { 0.6f, 1.2f }, kStreamSpin = { -5.0f, 5.0f };
		constexpr Vector4 kStreamMagenta = { 1.0f, 0.25f, 0.90f, 0.95f }, kStreamCyan = { 0.15f, 0.95f, 1.0f, 0.95f };
		constexpr ScaleSpec kStreamScale = { { 1.0f, 1.0f }, 1.0f, 0.2f };
		constexpr DynamicsSpec kStreamDyn = { -0.2f, 1.8f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0 };

		// EmitMinovskyBurst & Bokeh & DashTrail
		constexpr float kBurstMinSpdMul = 0.4f, kBurstMaxSpdMul = 1.2f, kBurstDirEpsilon = 0.0001f, kBurstStarScaleMul = 1.3f;
		constexpr int kBurstColorMaxRoll = 10, kBurstStarRollMax = 1, kBurstCyanStarRoll = 2, kBurstCyanRollMax = 5, kBurstMagStarRoll = 6;
		constexpr RangeF kBurstScale = { 0.15f, 0.35f }, kBurstLife = { 0.8f, 1.6f }, kBurstSpin = { -8.0f, 8.0f };
		constexpr Vector4 kBurstCyanColor = { 0.15f, 0.95f, 1.0f, 0.95f }, kBurstMagentaColor = { 1.0f, 0.18f, 0.88f, 0.95f };
		constexpr ScaleSpec kBurstScaleSpec = { { 1.0f, 1.0f }, 0.5f, 1.4f };
		constexpr DynamicsSpec kBurstDyn = { -0.1f, 2.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0 };

		constexpr RangeF kBokehDriftVel = { -0.15f, 0.15f }, kBokehUpVel = { 0.1f, 0.35f }, kBokehLife = { 3.0f, 6.0f }, kBokehSpin = { -1.5f, 1.5f };
		constexpr Vector4 kBokehMagenta = { 0.95f, 0.22f, 0.80f, 0.65f }, kBokehCyan = { 0.15f, 0.88f, 1.0f, 0.65f };
		constexpr float kBokehWobbleFreq = 1.8f, kBokehWobbleAmp = 0.2f;
		constexpr Vector3 kBokehWobbleAxis = { 1.0f, 0.0f, 0.5f };
		constexpr ScaleSpec kBokehScale = { { 1.0f, 1.0f }, 0.7f, 1.25f };
		constexpr DynamicsSpec kBokehDyn = { -0.05f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0 };

		constexpr int kDashTrailCount = 4;
		constexpr RangeF kDashJitter = { -0.25f, 0.25f }, kDashScale = { 0.18f, 0.32f };
		constexpr float kDashOffsetY = 0.2f, kDashBackSpeed = 3.0f, kDashUpVel = 0.8f;

		// EmitMinovskyFlightAura & Glitter
		constexpr RangeF kAuraRadius = { 0.25f, 0.95f }, kAuraHeight = { -0.45f, 0.65f }, kAuraScale = { 0.022f, 0.045f }, kAuraLife = { 1.4f, 2.6f }, kAuraSpin = { -7.0f, 7.0f }, kAuraTwinkleSpeed = { 12.0f, 22.0f };
		constexpr int kAuraColorMaxRoll = 9, kAuraStarRollMax = 1, kAuraCyanRollMax = 5;
		constexpr float kAuraTangentSpd = 0.45f, kAuraUpVel = 0.38f, kAuraStarScaleMul = 1.25f;
		constexpr ColorShiftSpec kAuraStarColor = { { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.3f, 0.95f, 1.0f, 0.8f }, true };
		constexpr ColorShiftSpec kAuraCyanColor = { { 0.18f, 0.96f, 1.0f, 0.95f }, { 0.95f, 0.22f, 0.88f, 0.95f }, true };
		constexpr ColorShiftSpec kAuraMagentaColor = { { 1.0f, 0.25f, 0.90f, 0.95f }, { 0.15f, 0.92f, 1.0f, 0.95f }, true };
		constexpr ScaleSpec kAuraScaleSpec = { { 1.0f, 1.0f }, 0.35f, 1.2f };
		constexpr DynamicsSpec kAuraDyn = { -0.15f, 0.5f, 0.0f, 3.5f, 0.2f, 0.0f, 0.0f, 0 };

		constexpr RangeF kGlitterJitter = { -0.06f, 0.06f }, kGlitterLife = { 0.5f, 1.1f }, kGlitterSpin = { -12.0f, 12.0f };
		constexpr float kGlitterHorizMul = 0.8f, kGlitterUpVel = 0.35f;
		constexpr ScaleSpec kGlitterScale = { { 1.0f, 1.0f }, 0.2f, 1.5f };
		constexpr ColorShiftSpec kGlitterColor = { { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.4f, 1.0f, 1.0f, 0.85f }, true };
		constexpr DynamicsSpec kGlitterDyn = { -0.1f, 0.3f, 0.0f, 0.0f, 0.0f, 0.0f, 24.0f, 0 };
	}
}
