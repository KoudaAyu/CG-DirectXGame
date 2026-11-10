#pragma once
#include "Vector.h"
#include "Application/UI/TacticalUICanvas.h"
#include <cstdint>

/// <summary>
/// ゲームオーバー演出コントローラー用 定数・メッシュ・カメラ・UIパレット定義 (GameOverSequenceTuning)
/// </summary>
namespace GameOverSequenceTuning
{
	// --- 共通算術・射影定数 (Common & Projection) ---
	inline constexpr float kEaseCubicFirstHalfScale = 4.0f, kLookAtMinXzLen = 0.001f, kFullCircleDeg = 360.0f, kMinDirectionDist = 0.01f;
	inline constexpr uint32_t kFallbackTextureIndex = 0u;

	inline constexpr int kAxisX = 0, kAxisY = 1, kAxisZ = 2, kAxisW = 3;
	inline constexpr float kProjMinClipW = 0.001f, kProjNdcCullLimit = 1.4f;

	// --- タイムライン・バレットタイム・シーン遷移定数 (Timeline & Transition) ---
	inline constexpr float kBulletTimeSlowEnd       = 0.22f; // バレットタイム20%減速終了時刻 (sec)
	inline constexpr float kGroundImpactTime        = 0.38f; // 地面激突・等倍速復帰時刻 (sec)
	inline constexpr float kBulletTimeRecoverSpan   = 0.16f; // 0.22s -> 0.38s 加速区間 (sec)
	inline constexpr float kBulletTimeMinScale = 0.20f, kBulletTimeAccelGain = 0.80f;

	inline constexpr float kSkipEnableTime          = 0.35f; // スキップ入力受付開始時刻 (sec)
	inline constexpr float kNormalShutterDuration   = 1.70f; // 通常完走時シャッター時間 (sec)
	inline constexpr float kSkipShutterDuration     = 1.00f; // スキップ時シャッター時間 (sec)
	inline constexpr float kSkipClickVolume = 0.60f;

	// --- 手続き型ボックスメッシュ生成テーブル（軍用担架モデル） (Stretcher Mesh) ---
	struct BoxSpec { Vector3 center; Vector3 size; };

	inline constexpr int kBoxCornerCount = 8, kBoxFaceCount = 6, kFaceVertCount = 4, kFaceIndexCount = 6;

	inline constexpr Vector3 kBoxCornerSigns[kBoxCornerCount] = {
		{ -1.0f, -1.0f, -1.0f }, {  1.0f, -1.0f, -1.0f },
		{  1.0f,  1.0f, -1.0f }, { -1.0f,  1.0f, -1.0f },
		{ -1.0f, -1.0f,  1.0f }, {  1.0f, -1.0f,  1.0f },
		{  1.0f,  1.0f,  1.0f }, { -1.0f,  1.0f,  1.0f },
	};

	inline constexpr int kBoxFaceVertices[kBoxFaceCount][kFaceVertCount] = {
		{ 0, 1, 2, 3 }, { 5, 4, 7, 6 }, { 4, 0, 3, 7 },
		{ 1, 5, 6, 2 }, { 3, 2, 6, 7 }, { 4, 5, 1, 0 },
	};

	inline constexpr Vector3 kBoxFaceNormals[kBoxFaceCount] = {
		{  0.0f,  0.0f, -1.0f }, {  0.0f,  0.0f,  1.0f },
		{ -1.0f,  0.0f,  0.0f }, {  1.0f,  0.0f,  0.0f },
		{  0.0f,  1.0f,  0.0f }, {  0.0f, -1.0f,  0.0f },
	};

	inline constexpr Vector2 kQuadCornerUVs[kFaceVertCount] = {
		{ 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f },
	};

	inline constexpr uint32_t kQuadTriIndices[kFaceIndexCount] = { 0u, 1u, 2u, 0u, 2u, 3u };

	// アルミ担架フレーム・グリップ・キャンバス生地・折りたたみ脚・救急枕 (StretcherBoxes)
	inline constexpr BoxSpec kStretcherBoxes[] = {
		{ {  0.0f,   0.0f,  -0.60f }, { 2.40f, 0.06f, 0.06f } },
		{ {  0.0f,   0.0f,   0.60f }, { 2.40f, 0.06f, 0.06f } },
		{ { -1.15f,  0.0f,  -0.60f }, { 0.16f, 0.08f, 0.08f } },
		{ { -1.15f,  0.0f,   0.60f }, { 0.16f, 0.08f, 0.08f } },
		{ {  1.15f,  0.0f,  -0.60f }, { 0.16f, 0.08f, 0.08f } },
		{ {  1.15f,  0.0f,   0.60f }, { 0.16f, 0.08f, 0.08f } },
		{ {  0.0f,  -0.015f, 0.0f  }, { 1.85f, 0.03f, 1.15f } },
		{ { -0.75f, -0.07f, -0.60f }, { 0.05f, 0.12f, 0.05f } },
		{ { -0.75f, -0.07f,  0.60f }, { 0.05f, 0.12f, 0.05f } },
		{ {  0.75f, -0.07f, -0.60f }, { 0.05f, 0.12f, 0.05f } },
		{ {  0.75f, -0.07f,  0.60f }, { 0.05f, 0.12f, 0.05f } },
		{ { -0.60f,  0.04f,  0.0f  }, { 0.45f, 0.08f, 0.85f } },
	};
	inline constexpr float kStretcherBoundingRadius = 4.0f;

	// --- 3Dオブジェクト外装・初期セットアップ定数 (Object Appearance) ---
	inline constexpr Vector4 kDroppedRifleColor = { 0.20f, 0.20f, 0.22f, 1.0f };
	inline constexpr float kMedicBoundingRadius = 10.0f;
	inline constexpr Vector3 kMedicModelScale = { 0.95f, 0.95f, 0.95f }, kStretcherModelScale = { 1.25f, 1.25f, 1.25f };
	inline constexpr Vector3 kUnitScale = { 1.0f, 1.0f, 1.0f };
	inline constexpr Vector4 kUnitWhiteColor = { 1.0f, 1.0f, 1.0f, 1.0f }, kWoundedDuckTint = { 0.95f, 0.95f, 0.98f, 1.0f };

	// --- 心電図（ECG）波形シミュレーション定数（視覚更新専用） (ECG Waveform) ---
	struct EcgWaveSpec { float freqHz; float peakEnd; float dipEnd; float dipSpan; float peakAmp; float dipAmp; };

	inline constexpr size_t kEcgHistoryCapacity = 120, kEcgHistoryMaxSamples = 80;
	inline constexpr float kEcgTachyEndTime = 0.35f;
	inline constexpr EcgWaveSpec kEcgTachySpec = { 5.0f, 0.14f, 0.26f, 0.12f, 1.35f, 0.50f };
	inline constexpr EcgWaveSpec kEcgBradySpec = { 2.0f, 0.16f, 0.28f, 0.12f, 1.0f, 0.35f };

	// --- 放射ダストバースト共通テーブル定義 (Radial Dust Bursts) ---
	struct RadialDustBurstSpec
	{
		int     count;
		float   originOffsetY;
		float   posJitterXZ;
		float   angleJitter;
		Vector2 speedRange;
		Vector2 upVelRange;
		float   scale;
		Vector4 color;
		float   life;
	};

	// --- 致命弾着弾・初動物理＆パーティクル定数 (Start Impact) ---
	inline constexpr float kInitialVignetteAlpha = 0.65f, kMinKillerDistance = 0.1f, kDuckFinalSlideDist = 1.55f;

	inline constexpr Vector3 kFallbackRifleOffset = { 0.0f, 0.68f, 0.0f };
	inline constexpr float kInitialRiflePitch = 0.2f, kRifleFlingForwardSpd = 2.4f, kRifleFlingSideSpd = 1.1f, kRifleFlingUpSpd = 4.0f;
	inline constexpr Vector2 kRifleSpinRangeX = { 14.0f, 18.0f }, kRifleSpinRangeY = { 8.0f, 12.0f }, kRifleSpinRangeZ = { 12.0f, 16.0f };

	inline constexpr float kMedicTargetSideDist = 1.80f, kMedicStartBackDist = 6.5f, kMedicStartSideDist = 0.6f;

	inline constexpr float kHitShakeDuration = 0.70f, kHitShakeIntensity = 3.5f, kHitStopDuration = 0.16f;
	inline constexpr float kKiaImpactVolume = 0.82f, kDuckVoiceVolume = 0.55f;

	inline constexpr int kHitStreakCount = 10;
	inline constexpr Vector2 kHitStreakSpeedRange = { 10.0f, 18.0f };
	inline constexpr float kHitStreakJitterXZ = 0.25f, kHitStreakJitterY = 0.15f, kChestBurstHeight = 0.36f, kHitStreakScale = 0.22f;
	inline constexpr Vector4 kHitStreakColor = { 0.12f, 0.11f, 0.10f, 0.35f };
	inline constexpr float kHitStreakLife = 0.18f;

	inline constexpr int kHitFeatherCount = 16;
	inline constexpr Vector4 kHitFeatherColor = { 0.22f, 0.19f, 0.14f, 0.45f };

	// 足元衝撃波ダストリング (HitFootRing)
	inline constexpr RadialDustBurstSpec kHitFootRingBurst = {
		12, 0.02f, 0.0f, 0.0f, { 2.6f, 2.6f }, { 0.10f, 0.10f }, 0.40f, { 0.11f, 0.10f, 0.08f, 0.30f }, 0.32f };

	// --- アヒル兵士ラグドール運動・地面激突・呼吸定数 (UpdateMotion) ---
	inline constexpr float kAirJumpArcHeight = 0.26f, kParabolaPeakFactor = 4.0f, kAirKnockbackDist = 1.10f;
	inline constexpr float kAirBaseLiftY = 0.08f, kAirPitchTimeScale = 1.25f, kPronePitchRad = 1.22f;
	inline constexpr float kAirYawSpinAmp = 0.30f, kAirRollAmp = 0.35f;

	inline constexpr float kAirSquashEndT = 0.25f, kAirStretchSpanT = 0.75f, kAirSquashBaseXZ = 1.08f;
	inline constexpr float kAirSquashGainXZ = 0.08f, kAirSquashBaseY = 0.90f, kAirSquashLossY = 0.10f;
	inline constexpr float kAirStretchBaseXZ = 0.96f, kAirStretchBaseY = 1.05f, kAirStretchGainY = 0.06f;

	inline constexpr float kSkidPhaseSpan = 0.47f, kBodyCollapseVolume = 0.78f, kGroundShakeDuration = 0.65f, kGroundShakeIntensity = 3.8f;

	// 地面激突ダスト3層：衝撃波リング(16)・泥煙雲(8)・砂利破片(14) (GroundImpactBursts)
	inline constexpr RadialDustBurstSpec kGroundImpactDustBursts[] = {
		{ 16, 0.03f, 0.0f,  0.0f,  { 4.6f, 4.6f }, { 0.08f, 0.22f }, 0.80f, { 0.14f, 0.12f, 0.10f, 0.38f }, 0.50f },
		{  8, 0.06f, 0.0f,  0.2f,  { 1.5f, 1.5f }, { 0.6f,  1.4f  }, 1.10f, { 0.09f, 0.08f, 0.07f, 0.45f }, 0.65f },
		{ 14, 0.04f, 0.25f, 0.25f, { 2.2f, 4.6f }, { 0.6f,  2.2f  }, 0.38f, { 0.10f, 0.08f, 0.07f, 0.45f }, 0.45f },
	};

	inline constexpr int kImpactFeatherCount = 14;
	inline constexpr float kImpactFeatherSpreadXZ = 0.50f;
	inline constexpr Vector2 kImpactFeatherYRange = { 0.12f, 0.40f };
	inline constexpr Vector4 kImpactFeatherColor = { 0.20f, 0.18f, 0.14f, 0.40f };

	inline constexpr float kMicroBounceEndT = 0.35f, kMicroBounceHeight = 0.05f, kSkidExtraDist = 0.45f, kGroundRestY = 0.14f;

	inline constexpr float kSkidDustEndT = 0.70f, kSkidDustInterval = 0.08f, kSkidDustBackVel = -0.6f, kSkidDustJitterXZ = 0.2f;
	inline constexpr Vector2 kSkidDustUpRange = { 0.1f, 0.35f };
	inline constexpr float kSkidDustY = 0.04f, kSkidDustScale = 0.45f;
	inline constexpr Vector4 kSkidDustColor = { 0.12f, 0.10f, 0.09f, 0.32f };
	inline constexpr float kSkidDustLife = 0.35f;

	inline constexpr float kNeckWhipEndT = 0.40f, kNeckWhipAmp = 0.08f, kSkidYawRelaxRatio = 0.15f, kSkidRollSettle = 0.05f;

	inline constexpr float kGroundSquashEndT = 0.20f, kGroundSquashBaseXZ = 1.12f, kGroundSquashGainXZ = 0.08f;
	inline constexpr float kGroundSquashBaseY = 0.84f, kGroundSquashLossY = 0.10f;

	inline constexpr float kProneYawOffset = 0.26f, kProneRollRad = 0.30f, kProneBreathFreq = 1.8f;
	inline constexpr float kProneBreathAmp = 0.010f, kProneScaleXZ = 1.04f, kProneScaleY = 0.96f;

	// --- 衛生兵レスキューチーム・落下ライフル・環境煙定数 (Medic, Rifle & Atmosphere) ---
	inline constexpr float kMedicAppearTime = 1.60f, kMedicRunEndTime = 2.80f, kMedicRunDuration = 1.20f;
	inline constexpr float kMedicWalkSpeed = 7.5f, kMedicArmOffset = 1.48f, kMedicBobAmp = 0.06f;
	inline constexpr float kMedicWaddleAmp = 0.14f, kMedicStepVolume = 0.36f;
	inline constexpr uint32_t kMedicStepCooldownMs = 90u;
	inline constexpr float kStretcherCarryY = 0.38f, kStretcherGroundY = 0.08f, kMedicKneelOffsetY = -0.06f, kMedicKneelPitch = 0.25f;

	inline constexpr float kRifleGravity = 12.0f, kRifleGroundY = 0.04f, kRifleRestitutionY = 0.28f;
	inline constexpr float kRifleFrictionXZ = 0.55f, kRifleSpinDamping = 0.35f, kRifleBounceVolume = 0.62f;
	inline constexpr int kRifleDustCount = 4;
	inline constexpr float kRifleDustJitterXZ = 0.8f;
	inline constexpr Vector2 kRifleDustUpRange = { 0.2f, 0.5f };
	inline constexpr float kRifleDustScale = 0.40f;
	inline constexpr Vector4 kRifleDustColor = { 0.12f, 0.10f, 0.09f, 0.30f };
	inline constexpr float kRifleDustLife = 0.30f, kRifleRestVelLimit = 0.2f;

	inline constexpr float kWoundSmokeStartTime = 0.45f, kWoundSmokeInterval = 0.09f, kWoundSmokeBackOff = 0.18f;
	inline constexpr float kWoundSmokeSideOff = 0.12f, kWoundSmokePosJitXZ = 0.06f, kWoundSmokePosY = 0.14f;
	inline constexpr float kWoundSmokeVelFling = 0.22f, kWoundSmokeVelSide = 0.35f, kWoundSmokeVelJitXZ = 0.10f;
	inline constexpr Vector2 kWoundSmokeUpRange = { 0.8f, 1.4f };
	inline constexpr float kWoundSmokeScale = 0.42f;
	inline constexpr Vector4 kWoundSmokeColor = { 0.08f, 0.07f, 0.07f, 0.25f };
	inline constexpr float kWoundSmokeLife = 0.40f;

	inline constexpr float kDownwashStartTime = 1.0f, kDownwashEndTime = 2.90f, kDownwashInterval = 0.12f;
	inline constexpr float kDownwashSpreadXZ = 1.6f, kDownwashPosY = 0.08f;
	inline constexpr Vector2 kDownwashVelXRange = { 1.8f, 3.2f }, kDownwashVelYRange = { 0.1f, 0.3f }, kDownwashVelZRange = { 0.8f, 2.0f };
	inline constexpr float kDownwashScale = 0.75f;
	inline constexpr Vector4 kDownwashColor = { 0.14f, 0.12f, 0.10f, 0.28f };
	inline constexpr float kDownwashLife = 0.35f;

	inline constexpr float kDriftFeatherInterval = 0.25f, kDriftFeatherSpreadXZ = 0.8f;
	inline constexpr Vector2 kDriftFeatherYRange = { 0.3f, 1.2f };
	inline constexpr Vector4 kDriftFeatherColor = { 0.18f, 0.16f, 0.13f, 0.35f };

	// --- シネマティック5フェーズカメラ設定 (UpdateCamera) ---
	struct CamLocalOffset { float fling; float height; float perp; };

	inline constexpr CamLocalOffset kCamPhase1Target = { -1.90f, 1.05f, -2.90f };
	inline constexpr float kCamMicroShakeFreqX = 50.0f, kCamMicroShakeAmpX = 0.022f, kCamMicroShakeFreqY = 40.0f;
	inline constexpr float kCamMicroShakeAmpY = 0.016f, kCamPhase1LookY = 0.32f, kCamDutchRollRad = -0.070f, kCamPhase1TargetFov = 0.48f;

	inline constexpr CamLocalOffset kCamPhase2Drift = { -0.22f, 0.10f, -0.16f };
	inline constexpr float kCamSlamDecayRate = 2.5f, kCamSlamFreqFactor = 4.5f, kCamSlamAmpY = 0.038f;
	inline constexpr float kCamPhase2LookY = 0.28f, kCamPhase2FovDrop = 0.02f;

	inline constexpr CamLocalOffset kCamPhase3Start = { -2.05f, 1.15f, -2.95f }, kCamPhase3End = { -0.65f, 1.35f, -3.35f };
	inline constexpr float kCamBreathFreq = 2.5f, kCamBreathAmpY = 0.008f, kCamPhase3LookY = 0.26f;
	inline constexpr float kCamPhase3StartFov = 0.46f, kCamPhase3FovDrop = 0.02f;

	inline constexpr CamLocalOffset kCamPhase4End = { -1.95f, 1.80f, -3.75f };
	inline constexpr float kCamTeamLookY = 0.30f, kCamPhase4LookBlend = 0.60f, kCamPhase4StartFov = 0.44f;
	inline constexpr float kCamPhase4FovGain = 0.04f, kCamPhase5Fov = 0.48f;

	// --- シネマスコープ黒帯・ダークヴィネット・無線字幕UIレイアウト＆カラーパレット (HUD & Palette) ---
	struct RgbColor { uint8_t r; uint8_t g; uint8_t b; };

	struct GameOverRadioCue { float startTime; float endTime; float fadeInDuration; RgbColor color; const char8_t* text; };

	inline constexpr int kAlphaMin = 0, kAlphaMax = 255;
	inline constexpr float kAlphaMaxF = 255.0f;

	inline constexpr uint32_t kColLetterboxSolid = UI_COL32(0, 0, 0, 255);
	inline constexpr RgbColor kColBlackRgb = { 0, 0, 0 }, kColSkipPromptRgb = { 203, 213, 225 };
	inline constexpr RgbColor kColRadioMaydayRgb = { 245, 215, 120 }, kColRadioMedicRgb = { 245, 225, 130 };

	inline constexpr float kVignetteMinAlpha = 0.05f, kVignettePulseBase = 0.85f, kVignettePulseAmp = 0.15f;
	inline constexpr float kVignettePulseFreq = 4.5f, kVignetteAlphaScale = 180.0f, kVignetteRectAlphaRatio = 0.5f;
	inline constexpr float kVignetteStrokeWidth = 48.0f;

	inline constexpr float kLetterboxHeightRatio = 0.11f, kLetterboxOpenDuration = 0.28f, kLetterboxMinHeightPx = 0.5f;

	inline constexpr float kSkipPulseBase = 0.55f, kSkipPulseAmp = 0.45f, kSkipPulseFreq = 5.0f;
	inline constexpr float kSkipAlphaScale = 210.0f, kSkipFontSize = 14.0f;

	inline constexpr float kSubtitleFontSize = 20.0f, kSubtitleShadowOffset = 1.5f, kSubtitlePosYRatio = 0.80f;
	inline constexpr float kSubtitleAlphaScale = 240.0f;

	// 救難無線字幕タイムラインテーブル (GameOverRadioCues: Act1=0.85, Act2=1.70, Act3=2.80)
	inline constexpr GameOverRadioCue kGameOverRadioCues[] = {
		{ 0.85f, 1.70f, 0.35f, kColRadioMaydayRgb, u8"「メーデー！ 味方が負傷、救難信号を発信！」" },
		{ 1.70f, 2.80f, 0.30f, kColRadioMedicRgb,  u8"「救難ヘリ了解！ 衛生班が到着、担架を展開！」" },
	};

	inline constexpr float kBlackoutMinAlpha = 0.01f, kBlackoutSubThreshold = 0.20f, kBlackoutSubSpan = 0.80f;
	inline constexpr float kBlackoutSubPosYRatio = 0.5f;
}
