#pragma once
#include "Vector.h"
#include "Application/Config/GameConfig.h"
#include <cstdint>

/// <summary>
/// 衝突判定・材質別着弾エフェクト・被弾リアクションの調整定数定義
/// </summary>
namespace CollisionTuning
{
	namespace Geometry
	{
		constexpr float kRotationEps = 1e-5f, kParallelSlabEps = 1e-6f, kVecNormalizeEps = 1e-4f;
		constexpr float kFallbackSlabNormalY = 0.25f, kFallbackImpactNormalY = 0.35f, kMinReflectedDirY = 0.15f;
		constexpr float kReflectedUpBiasY = 0.22f, kFenceConcreteMaxY = 0.54f, kRiverSkipMinStartY = 0.14f, kRiverSkipMinDirY = -0.15f;

		constexpr float kBulletRadius          = GameConfig::Combat::kBulletRadius; // 0.12f
		constexpr float kDuckHeight = 1.35f, kDuckRadius = 0.42f, kTargetHeight = 1.5f;
	}

	namespace Fragment
	{
		constexpr float kDefaultEndScaleRatio = 0.28f, kAngularVelocityRange = 14.0f;
		constexpr uint16_t kDefaultSlotLayer = 2;
	}

	namespace Impact
	{
		constexpr const char* kMuzzleFlashTexPath = "Resources/duckov_muzzle_flash.png";
		constexpr const char* kSeMetalPath        = "Resources/Audio/SE_Impact_Metal.wav";
		constexpr const char* kSeConcretePath     = "Resources/Audio/SE_Impact_Concrete.wav";
		constexpr const char* kSeDirtPath         = "Resources/Audio/SE_Impact_Dirt.wav";
		constexpr const char* kSeWoodPath         = "Resources/Audio/SE_Impact_Wood.wav";

		constexpr float kAudioVolume = 0.58f, kAudioMinDist = 4.0f, kAudioMaxDist = 38.0f;
		constexpr uint32_t kAudioCooldownMs = 35;

		// 着弾煙パフ共通設定
		constexpr float kSmokeNormalOffset = 0.04f, kSmokeInitScaleMinRatio = 0.38f, kSmokeInitScaleMaxRatio = 0.65f;
		constexpr float kSmokeAspectXMin = 0.92f, kSmokeAspectXMax = 1.32f, kSmokeAspectYMin = 0.85f;
		constexpr float kSmokeAspectYMax = 1.18f, kSmokeNormalSpeedMinRatio = 0.48f, kSmokeHorizJitter = 0.85f;
		constexpr float kSmokeUpMin = 0.30f, kSmokeUpMax = 1.25f, kSmokeEndColorDim = 0.88f, kSmokeDrag = 3.0f;
		constexpr float kSmokeCurlFreq = 1.6f, kSmokeCurlAmp = 0.22f, kSmokeLifeMinRatio = 0.78f;
		constexpr float kSmokeLifeMaxRatio = 1.18f, kSmokeAngularVel = 1.4f, kSmokeFadeInRatio = 0.08f;

		struct SmokePuffPreset { int count; Vector4 color; float minScale; float maxScale; float speed; float lifeTime; };

		constexpr SmokePuffPreset kMetalSmoke    = {  5, { 0.46f, 0.47f, 0.50f, 0.36f }, 0.42f, 0.78f, 2.6f, 0.62f };
		constexpr SmokePuffPreset kConcreteSmoke = {  9, { 0.80f, 0.79f, 0.76f, 0.48f }, 0.48f, 1.04f, 3.3f, 0.82f };
		constexpr SmokePuffPreset kSandDirtSmoke = { 10, { 0.70f, 0.60f, 0.44f, 0.52f }, 0.50f, 1.12f, 3.8f, 0.86f };
		constexpr SmokePuffPreset kWoodSmoke     = {  7, { 0.72f, 0.61f, 0.44f, 0.44f }, 0.42f, 0.86f, 2.8f, 0.72f };

		// 鋼鉄 (Metal) 着弾設定
		constexpr float kFlashNormalOffset = 0.03f, kSparkNormalOffset = 0.025f, kChipNormalOffset = 0.035f;

		constexpr Vector4 kMetalFlashStart = { 1.0f, 0.97f, 0.82f, 0.98f }, kMetalFlashEnd = { 1.0f, 0.44f, 0.08f, 0.0f };
		constexpr float kMetalFlashSize = 0.42f, kMetalFlashLife = 0.056f;

		constexpr int kMetalSparkCount = 38, kMetalBrightSparkPeriod = 3;
		constexpr float kMetalSparkJitterXZ = 0.58f, kMetalSparkJitterYMin = -0.20f, kMetalSparkJitterYMax = 0.72f;
		constexpr float kMetalSparkSpeedMin = 5.4f, kMetalSparkSpeedMax = 15.2f;
		constexpr Vector4 kMetalSparkBrightCol = { 1.0f, 0.98f, 0.88f, 1.0f }, kMetalSparkGoldCol = { 1.0f, 0.78f, 0.30f, 0.98f };
		constexpr Vector4 kMetalSparkEndCol = { 1.0f, 0.24f, 0.04f, 0.0f };
		constexpr float kMetalSparkWMin = 0.019f, kMetalSparkWMax = 0.036f, kMetalSparkLMin = 0.15f;
		constexpr float kMetalSparkLMax = 0.36f, kMetalSparkLifeMin = 0.22f, kMetalSparkLifeMax = 0.64f;
		constexpr float kMetalSparkGravity = 12.4f, kMetalSparkDrag = 1.4f, kMetalSparkBounce = 0.58f, kMetalSparkEndScale = 0.24f;

		constexpr int kMetalChipCount = 7;
		constexpr float kMetalChipNormalSpdMin = 2.0f, kMetalChipNormalSpdMax = 4.8f, kMetalChipHorizJitter = 1.6f;
		constexpr float kMetalChipUpMin = 1.0f, kMetalChipUpMax = 3.0f;
		constexpr Vector4 kMetalChipStartCol = { 0.32f, 0.35f, 0.38f, 0.94f }, kMetalChipEndCol = { 0.22f, 0.24f, 0.26f, 0.0f };
		constexpr float kMetalChipWMin = 0.024f, kMetalChipWMax = 0.042f, kMetalChipLMin = 0.032f, kMetalChipLMax = 0.058f;
		constexpr float kMetalChipLifeMin = 0.32f, kMetalChipLifeMax = 0.56f, kMetalChipGravity = 15.5f;
		constexpr float kMetalChipBounce = 0.46f, kMetalChipEndScale = 0.80f;

		// コンクリート (Concrete) 着弾設定
		constexpr Vector4 kConcFlashStart = { 1.0f, 0.92f, 0.70f, 0.86f }, kConcFlashEnd = { 0.85f, 0.58f, 0.32f, 0.0f };
		constexpr float kConcFlashSize = 0.30f, kConcFlashLife = 0.048f;

		constexpr int kConcSparkCount = 14;
		constexpr float kConcSparkReflSpdMin = 4.0f, kConcSparkReflSpdMax = 9.6f, kConcSparkHorizJitter = 2.0f;
		constexpr float kConcSparkUpMin = 0.9f, kConcSparkUpMax = 3.2f;
		constexpr Vector4 kConcSparkStartCol = { 1.0f, 0.86f, 0.48f, 0.92f }, kConcSparkEndCol = { 1.0f, 0.34f, 0.06f, 0.0f };
		constexpr float kConcSparkWidth = 0.019f, kConcSparkLength = 0.135f, kConcSparkLifeMin = 0.14f;
		constexpr float kConcSparkLifeMax = 0.32f, kConcSparkGravity = 13.0f, kConcSparkDrag = 2.0f;
		constexpr float kConcSparkBounce = 0.50f, kConcSparkEndScale = 0.24f;

		constexpr int kConcChipCount = 18;
		constexpr float kConcChipNormalSpdMin = 2.6f, kConcChipNormalSpdMax = 6.8f, kConcChipHorizJitter = 2.2f;
		constexpr float kConcChipUpMin = 1.4f, kConcChipUpMax = 4.2f, kConcToneMin = 0.58f, kConcToneMax = 0.80f;
		constexpr float kConcGreenFactor = 0.99f, kConcBlueFactor = 0.96f, kConcStartAlpha = 0.96f, kConcEndRedDim = 0.80f;
		constexpr float kConcEndGreenDim = 0.79f, kConcEndBlueDim = 0.76f, kConcChipWMin = 0.030f, kConcChipWMax = 0.062f;
		constexpr float kConcChipLMin = 0.038f, kConcChipLMax = 0.078f, kConcChipLifeMin = 0.40f, kConcChipLifeMax = 0.78f;
		constexpr float kConcChipGravity = 16.0f, kConcChipBounce = 0.46f, kConcChipEndScale = 0.82f;

		// 土嚢・砂塵 (SandDirt) 着弾設定
		constexpr int kSandClodCount = 24;
		constexpr float kSandNormalSpdMin = 2.5f, kSandNormalSpdMax = 6.4f, kSandHorizJitter = 2.1f, kSandUpMin = 1.6f;
		constexpr float kSandUpMax = 4.9f, kSandToneMin = 0.84f, kSandToneMax = 1.16f;
		constexpr Vector4 kSandBaseStart = { 0.60f, 0.50f, 0.34f, 0.95f };
		constexpr Vector3 kSandBaseEndRGB = { 0.46f, 0.38f, 0.25f };
		constexpr float kSandWMin = 0.030f, kSandWMax = 0.060f, kSandLMin = 0.040f, kSandLMax = 0.082f;
		constexpr float kSandLifeMin = 0.38f, kSandLifeMax = 0.74f, kSandGravity = 16.6f, kSandDrag = 1.1f;
		constexpr float kSandBounce = 0.20f, kSandEndScale = 0.78f;

		// 木材・樹木 (Wood) 着弾設定
		constexpr int kWoodSplinterCount = 22, kWoodBarkPeriod = 3;
		constexpr float kWoodNormalSpdMin = 2.6f, kWoodNormalSpdMax = 6.8f, kWoodHorizJitter = 2.3f, kWoodUpMin = 1.5f;
		constexpr float kWoodUpMax = 4.8f, kWoodToneMin = 0.82f, kWoodToneMax = 1.18f;
		constexpr Vector4 kWoodBarkBase = { 0.38f, 0.27f, 0.17f, 0.96f }, kWoodHeartBase = { 0.74f, 0.55f, 0.32f, 0.96f };
		constexpr float kWoodEndDim = 0.80f, kWoodWMin = 0.024f, kWoodWMax = 0.046f, kWoodLMin = 0.095f, kWoodLMax = 0.220f;
		constexpr float kWoodLifeMin = 0.40f, kWoodLifeMax = 0.82f, kWoodGravity = 14.6f, kWoodDrag = 1.25f;
		constexpr float kWoodBounce = 0.36f, kWoodEndScale = 0.84f;

		constexpr int kTreeLeafCount = 11;
		constexpr float kLeafSpawnJitterXZ = 0.45f, kLeafSpawnOffsetYMin = 0.18f, kLeafSpawnOffsetYMax = 0.85f;
		constexpr float kLeafScaleMin = 0.080f, kLeafScaleMax = 0.145f;
		constexpr Vector2 kLeafAspect = { 0.82f, 1.30f };
		constexpr float kLeafTargetScaleRatio = 0.92f, kLeafNormalSpdMin = 0.6f, kLeafNormalSpdMax = 1.7f;
		constexpr float kLeafHorizJitter = 1.1f, kLeafUpMin = 0.35f, kLeafUpMax = 1.35f, kLeafToneMin = 0.85f, kLeafToneMax = 1.15f;
		constexpr Vector4 kLeafStartBase = { 0.34f, 0.58f, 0.25f, 0.94f };
		constexpr Vector3 kLeafEndBaseRGB = { 0.28f, 0.48f, 0.20f };
		constexpr float kLeafLifeMin = 1.35f, kLeafLifeMax = 2.25f, kLeafGravity = 1.85f, kLeafDrag = 2.3f;
		constexpr float kLeafWobbleFreq = 7.8f, kLeafWobbleAmp = 0.52f;
		constexpr Vector3 kLeafWobbleAxis = { 1.0f, 0.15f, 0.7f };
		constexpr float kLeafAngularVel = 4.8f;

		constexpr int kWoodSparkCount = 6;
		constexpr float kWoodSparkReflSpdMin = 3.0f, kWoodSparkReflSpdMax = 6.8f, kWoodSparkHorizJitter = 1.4f;
		constexpr float kWoodSparkUpMin = 0.6f, kWoodSparkUpMax = 2.2f;
		constexpr Vector4 kWoodSparkStartCol = { 1.0f, 0.78f, 0.34f, 0.86f }, kWoodSparkEndCol = { 0.95f, 0.28f, 0.05f, 0.0f };
		constexpr float kWoodSparkWidth = 0.018f, kWoodSparkLength = 0.095f, kWoodSparkLifeMin = 0.12f;
		constexpr float kWoodSparkLifeMax = 0.26f, kWoodSparkGravity = 10.5f, kWoodSparkDrag = 2.4f;
		constexpr float kWoodSparkBounce = 0.40f, kWoodSparkEndScale = 0.25f;
	}

	namespace CombatHit
	{
		// クリティカル＆ポップアップ共通
		constexpr int kPercentModulus = 100, kCritChancePercent = 30;
		constexpr Vector4 kCritPopupColor = { 1.0f, 0.9f, 0.0f, 1.0f }, kNormalPopupColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		constexpr float kDamagePopupOffsetY = 0.8f;

		// 敵被弾・撃破演出
		constexpr const char* kSeKillConfirm      = "Resources/Audio/SE_Kill_Confirm.wav";
		constexpr const char* kSeDuckQuack        = "Resources/Audio/SE_Duck.mp3";
		constexpr const char* kSeHitCrit          = "Resources/Audio/SE_Hit_Critical.wav";
		constexpr const char* kSeHitEnemy         = "Resources/Audio/SE_Hit_Enemy.wav";

		constexpr float kKillConfirmVol = 0.78f;
		constexpr uint32_t kKillConfirmCooldownMs = 40;
		constexpr float kEnemyQuackVol = 0.55f, kEnemyQuackMinDist = 5.0f, kEnemyQuackMaxDist = 38.0f;
		constexpr uint32_t kEnemyQuackCooldownMs = 120;

		constexpr float kWardenKillHitStop = 0.14f, kEnemyKillHitStop = 0.08f, kCritHitStop = 0.04f, kKillBannerOffsetY = 1.45f;
		constexpr Vector4 kWardenBannerColor = { 1.0f, 0.84f, 0.24f, 1.0f }, kHostileBannerColor = { 1.0f, 0.36f, 0.32f, 1.0f };

		constexpr float kCritHitVol = 0.72f, kNormalHitVol = 0.66f;
		constexpr uint32_t kHitSoundCooldownMs = 30;
		constexpr float kEnemyDeathShakeDur = 0.28f, kEnemyDeathShakeAmp = 0.55f;

		// 射撃標的 (Target) 着弾・破壊演出
		constexpr int kTargetHitDamage = 1, kTargetPaintChipCount = 9;
		constexpr float kTargetPaintBackUpY = 0.38f, kTargetPaintSpdMin = 2.2f, kTargetPaintSpdMax = 5.4f;
		constexpr float kTargetPaintHorizJit = 2.0f, kTargetPaintUpMin = 1.2f, kTargetPaintUpMax = 3.4f;
		constexpr Vector4 kTargetRedPaintCol = { 0.92f, 0.20f, 0.18f, 0.96f }, kTargetWhitePaintCol = { 0.96f, 0.95f, 0.92f, 0.96f };
		constexpr float kTargetPaintEndDim = 0.8f, kTargetPaintWMin = 0.028f, kTargetPaintWMax = 0.054f;
		constexpr float kTargetPaintLMin = 0.042f, kTargetPaintLMax = 0.082f, kTargetPaintLifeMin = 0.38f;
		constexpr float kTargetPaintLifeMax = 0.68f, kTargetPaintGravity = 15.2f, kTargetPaintDrag = 1.1f;
		constexpr float kTargetPaintBounce = 0.42f, kTargetPaintEndScale = 0.82f;

		constexpr const char* kSeTargetHit        = "Resources/Audio/SE_Target_Hit.wav";
		constexpr const char* kSeTargetBreak      = "Resources/Audio/SE_Target_Break.wav";
		constexpr float kTargetHitVol = 0.68f;
		constexpr uint32_t kTargetHitCooldownMs = 30;
		constexpr float kTargetBreakVol = 0.80f;
		constexpr uint32_t kTargetBreakCooldownMs = 40;
		constexpr float kTargetBreakShakeDur = 0.4f, kTargetBreakShakeAmp = 0.8f, kTargetBurstOffsetY = 0.65f;

		constexpr int kTargetDebrisCount = 36, kTargetDebrisKindMod = 3;
		constexpr float kTargetDebrisSpdMin = 3.0f, kTargetDebrisSpdMax = 8.4f, kTargetDebrisUpMin = 2.0f;
		constexpr float kTargetDebrisUpMax = 6.4f, kTargetPlateWMin = 0.038f, kTargetPlateWMax = 0.075f;
		constexpr float kTargetPlateLMin = 0.058f, kTargetPlateLMax = 0.115f, kTargetWoodWMin = 0.028f;
		constexpr float kTargetWoodWMax = 0.050f, kTargetWoodLMin = 0.110f, kTargetWoodLMax = 0.240f;
		constexpr Vector4 kTargetBullseyeRed = { 0.90f, 0.18f, 0.16f, 0.96f }, kTargetRingWhite = { 0.96f, 0.95f, 0.92f, 0.96f };
		constexpr Vector4 kTargetStandWood = { 0.72f, 0.52f, 0.30f, 0.96f };
		constexpr float kTargetDebrisEndDim = 0.78f, kTargetDebrisJitXZ = 0.22f, kTargetDebrisJitY = 0.24f;
		constexpr float kTargetDebrisLifeMin = 0.62f, kTargetDebrisLifeMax = 1.25f, kTargetDebrisGravity = 14.8f;
		constexpr float kTargetDebrisDrag = 1.05f, kTargetDebrisBounce = 0.42f, kTargetDebrisEndScale = 0.85f;

		constexpr int kTargetDustCount = 18;
		constexpr float kTargetDustAngleJit = 0.14f, kTargetDustRingRad = 0.20f, kTargetDustJitY = 0.22f;
		constexpr float kTargetDustSpdMin = 2.2f, kTargetDustSpdMax = 4.6f, kTargetDustInitScMin = 0.26f, kTargetDustInitScMax = 0.42f;
		constexpr Vector2 kTargetDustAspect = { 1.30f, 0.92f };
		constexpr float kTargetDustExpandRatio = 3.1f, kTargetDustUpMin = 0.40f, kTargetDustUpMax = 1.65f;
		constexpr Vector4 kTargetDustStartCol = { 0.74f, 0.67f, 0.56f, 0.50f }, kTargetDustEndCol = { 0.68f, 0.63f, 0.56f, 0.0f };
		constexpr float kTargetDustLifeMin = 0.68f, kTargetDustLifeMax = 1.12f, kTargetDustDrag = 2.7f;
		constexpr float kTargetDustCurlFreq = 1.7f, kTargetDustCurlAmp = 0.28f, kTargetDustFadeIn = 0.09f;

		constexpr float kTargetBannerOffsetY = 1.5f;
		constexpr Vector4 kTargetBannerColor = { 0.0f, 1.0f, 0.5f, 1.0f };

		// プレイヤー被弾演出
		constexpr float kFallbackAttackerDist = 16.0f, kMinAttackerDot = 0.5f, kMinEnemySearchDist = 0.1f, kEnemyRifleBulletDmg = 20.0f;

		constexpr const char* kSePlayerHit        = "Resources/Audio/SE_Player_Hit.wav";
		constexpr float kPlayerHitVol = 0.80f;
		constexpr uint32_t kPlayerHitCooldownMs = 80;
		constexpr float kPlayerQuackVol = 0.62f;
		constexpr uint32_t kPlayerQuackCooldownMs = 180;
		constexpr float kPlayerHitVignette = 0.85f, kPlayerHitShakeDur = 0.35f, kPlayerHitShakeAmp = 0.9f;
		constexpr float kPlayerHitStop = 0.06f, kPlayerPopupOffsetY = 1.2f;
		constexpr Vector4 kPlayerPopupColor = { 1.0f, 0.22f, 0.20f, 1.0f };

		constexpr int kVestSparkCount = 8;
		constexpr float kVestSparkDirMin = -2.5f, kVestSparkDirMax = 3.5f, kVestSparkHorizJit = 2.6f;
		constexpr float kVestSparkUpMin = 0.8f, kVestSparkUpMax = 3.0f;
		constexpr Vector4 kVestSparkStartCol = { 1.0f, 0.90f, 0.52f, 0.92f }, kVestSparkEndCol = { 1.0f, 0.35f, 0.08f, 0.0f };
		constexpr float kVestSparkWidth = 0.016f, kVestSparkLength = 0.095f, kVestSparkLifeMin = 0.12f;
		constexpr float kVestSparkLifeMax = 0.24f, kVestSparkGravity = 11.5f, kVestSparkDrag = 2.0f;
		constexpr float kVestSparkBounce = 0.42f, kVestSparkEndScale = 0.25f;

		constexpr int kFeatherCount = 14, kFeatherWhitePeriod = 3;
		constexpr float kFeatherJitXZ = 0.18f, kFeatherJitYMin = -0.10f, kFeatherJitYMax = 0.22f, kFeatherScaleMin = 0.085f;
		constexpr float kFeatherScaleMax = 0.155f;
		constexpr Vector2 kFeatherAspect = { 0.76f, 1.28f };
		constexpr float kFeatherEndScaleRatio = 0.90f, kFeatherFwdSpdMin = 0.8f, kFeatherFwdSpdMax = 2.8f;
		constexpr float kFeatherHorizJit = 2.2f, kFeatherUpMin = 1.2f, kFeatherUpMax = 3.4f;
		constexpr Vector4 kFeatherYellowCol = { 1.0f, 0.89f, 0.30f, 0.95f }, kFeatherWhiteCol = { 0.98f, 0.97f, 0.93f, 0.94f };
		constexpr float kFeatherLifeMin = 1.05f, kFeatherLifeMax = 1.85f, kFeatherGravity = 1.65f, kFeatherDrag = 2.5f;
		constexpr float kFeatherWobbleFreq = 8.4f, kFeatherWobbleAmp = 0.48f;
		constexpr Vector3 kFeatherWobbleAxis = { 1.0f, 0.15f, 0.65f };
		constexpr float kFeatherAngularVel = 5.2f;

		constexpr int kImpactDustCount = 3;
		constexpr float kImpactDustInitSc = 0.14f, kImpactDustEndSc = 0.42f, kImpactDustFwdSpdMin = 1.0f;
		constexpr float kImpactDustFwdSpdMax = 2.2f, kImpactDustHorizJit = 0.8f, kImpactDustUpMin = 0.3f, kImpactDustUpMax = 0.9f;
		constexpr Vector4 kImpactDustStartCol = { 0.76f, 0.72f, 0.64f, 0.30f }, kImpactDustEndCol = { 0.70f, 0.68f, 0.62f, 0.0f };
		constexpr float kImpactDustLife = 0.42f, kImpactDustDrag = 2.8f, kImpactDustFadeIn = 0.10f;

		// 接触ダメージ演出
		constexpr float kContactHitVol = 0.78f, kContactQuackVol = 0.60f, kContactVignette = 0.6f, kContactShakeDur = 0.25f;
		constexpr float kContactShakeAmp = 0.5f, kContactHitStop = 0.06f, kContactPopupOffsetY = 1.0f;
		constexpr Vector4 kContactPopupColor = { 1.0f, 0.1f, 0.1f, 1.0f };
	}
}
