#pragma once
#include "Vector.h"
#include "Application/UI/TacticalUICanvas.h"
#include <cstdint>

/// <summary>
/// 強襲ヘリ降下 ＆ 脱出回収ヘリ演出コントローラー用 定数・メッシュ・軌道・パーティクル・UI定義 (HelicopterInsertionTuning)
/// </summary>
namespace HelicopterInsertionTuning
{
	// --- 共通定数・列挙・行列軸インデックス (Common & Math) ---
	enum CutIndex : int
	{
		kCutAct1 = 1,
		kCutAct2 = 2,
		kCutAct3 = 3,
		kCutAct4 = 4,
		kCutAct5 = 5,
	};

	enum TextAlign : int
	{
		kAlignLeft   = 0,
		kAlignCenter = 1,
		kAlignRight  = 2,
	};

	inline constexpr int kAxisX = 0, kAxisY = 1, kAxisZ = 2;

	inline constexpr float kSmoothStepLinear = 3.0f, kLookAtMinXzLen = 0.001f;
	inline constexpr int kAsyncKeyDownMask = 0x8000;
	inline constexpr uint32_t kByteMask = 0xFFu;
	inline constexpr uint16_t kParticleLayerMid = 1u, kParticleLayerFront = 2u;

	inline constexpr Vector3 kZeroVec3 = { 0.0f, 0.0f, 0.0f }, kUnitScale = { 1.0f, 1.0f, 1.0f };
	inline constexpr Vector4 kUnitWhiteColor = { 1.0f, 1.0f, 1.0f, 1.0f };

	// --- 手続き型ボックスメッシュ ＆ シャドウ円盤生成テーブル (Procedural Meshes) ---
	struct UvBoxSpec { Vector3 center; Vector3 size; Vector2 uvMin; Vector2 uvMax; };

	inline constexpr int kBoxCornerCount = 8, kBoxFaceCount = 6, kFaceVertCount = 4, kFaceIndexCount = 6;

	inline constexpr Vector3 kBoxCornerSigns[kBoxCornerCount] = {
		{ -1.0f, -1.0f, -1.0f },
		{  1.0f, -1.0f, -1.0f },
		{  1.0f,  1.0f, -1.0f },
		{ -1.0f,  1.0f, -1.0f },
		{ -1.0f, -1.0f,  1.0f },
		{  1.0f, -1.0f,  1.0f },
		{  1.0f,  1.0f,  1.0f },
		{ -1.0f,  1.0f,  1.0f },
	};

	inline constexpr int kBoxFaceVertices[kBoxFaceCount][kFaceVertCount] = {
		{ 0, 1, 2, 3 },
		{ 5, 4, 7, 6 },
		{ 4, 0, 3, 7 },
		{ 1, 5, 6, 2 },
		{ 3, 2, 6, 7 },
		{ 4, 5, 1, 0 },
	};

	inline constexpr Vector3 kBoxFaceNormals[kBoxFaceCount] = {
		{  0.0f,  0.0f, -1.0f },
		{  0.0f,  0.0f,  1.0f },
		{ -1.0f,  0.0f,  0.0f },
		{  1.0f,  0.0f,  0.0f },
		{  0.0f,  1.0f,  0.0f },
		{  0.0f, -1.0f,  0.0f },
	};

	inline constexpr bool     kFaceUseUvMaxX[kFaceVertCount] = { false, true, true, false };
	inline constexpr bool     kFaceUseUvMaxY[kFaceVertCount] = { false, false, true, true };
	inline constexpr uint32_t kQuadTriIndices[kFaceIndexCount] = { 0u, 1u, 2u, 0u, 2u, 3u };

	// ヘリテクスチャUVアトラス領域 (Atlas UV Regions)
	inline constexpr Vector2 kUvMetalMin = { 0.65f, 0.05f }, kUvMetalMax = { 0.75f, 0.15f };
	inline constexpr Vector2 kUvOliveMin = { 0.25f, 0.05f }, kUvOliveMax = { 0.35f, 0.15f };
	inline constexpr Vector2 kUvGlassMin = { 0.10f, 0.75f }, kUvGlassMax = { 0.25f, 0.85f };
	inline constexpr Vector2 kUvWarnMin = { 0.85f, 0.05f }, kUvWarnMax = { 0.95f, 0.15f };

	// キャビン内装モデル：縞鋼板フロア・兵員ベンチ・天井リブ・グラブレール・弾薬箱 (CabinInteriorBoxes)
	inline constexpr UvBoxSpec kCabinInteriorBoxes[] = {
		{ {  0.18f, 0.095f,  0.75f }, { 1.65f,  0.025f, 2.40f  }, kUvMetalMin, kUvMetalMax },
		{ { -0.46f, 0.44f,   0.72f }, { 0.38f,  0.055f, 2.20f  }, kUvOliveMin, kUvOliveMax },
		{ { -0.66f, 0.94f,   0.72f }, { 0.045f, 0.48f,  2.20f  }, kUvOliveMin, kUvOliveMax },
		{ { -0.64f, 1.06f,  -0.25f }, { 0.045f, 1.92f,  0.045f }, kUvMetalMin, kUvMetalMax },
		{ { -0.64f, 1.06f,   0.72f }, { 0.045f, 1.92f,  0.045f }, kUvMetalMin, kUvMetalMax },
		{ { -0.64f, 1.06f,   1.68f }, { 0.045f, 1.92f,  0.045f }, kUvMetalMin, kUvMetalMax },
		{ {  0.22f, 2.04f,  -0.15f }, { 1.75f,  0.065f, 0.075f }, kUvMetalMin, kUvMetalMax },
		{ {  0.22f, 2.04f,   0.75f }, { 1.75f,  0.065f, 0.075f }, kUvMetalMin, kUvMetalMax },
		{ {  0.22f, 2.04f,   1.65f }, { 1.75f,  0.065f, 0.075f }, kUvMetalMin, kUvMetalMax },
		{ {  1.12f, 1.98f,   0.75f }, { 0.035f, 0.035f, 2.35f  }, kUvMetalMin, kUvMetalMax },
		{ { -0.22f, 0.28f,  -0.08f }, { 0.44f,  0.34f,  0.56f  }, kUvOliveMin, kUvOliveMax },
		{ { -0.22f, 0.46f,  -0.08f }, { 0.46f,  0.03f,  0.58f  }, kUvMetalMin, kUvMetalMax },
	};
	inline constexpr float kCabinInteriorBoundingRadius = 8.0f;

	// 右側面スライドカーゴドアモデル：窓枠・警戒ハンドル・ガイドレール (CargoDoorBoxes)
	inline constexpr UvBoxSpec kCargoDoorBoxes[] = {
		{ { 0.0f,    0.0f,  0.0f  }, { 0.045f, 2.12f,  2.72f }, kUvOliveMin, kUvOliveMax },
		{ { 0.014f,  1.08f, 0.0f  }, { 0.068f, 0.06f,  2.76f }, kUvMetalMin, kUvMetalMax },
		{ { 0.014f, -1.08f, 0.0f  }, { 0.068f, 0.06f,  2.76f }, kUvMetalMin, kUvMetalMax },
		{ { 0.026f, -0.45f, 0.0f  }, { 0.018f, 0.045f, 2.62f }, kUvOliveMin, kUvOliveMax },
		{ { 0.026f,  0.80f, 0.0f  }, { 0.018f, 0.045f, 2.62f }, kUvOliveMin, kUvOliveMax },
		{ { 0.025f,  0.35f, 0.45f }, { 0.014f, 0.65f,  0.75f }, kUvGlassMin, kUvGlassMax },
		{ { 0.030f,  0.70f, 0.45f }, { 0.022f, 0.05f,  0.85f }, kUvMetalMin, kUvMetalMax },
		{ { 0.030f,  0.00f, 0.45f }, { 0.022f, 0.05f,  0.85f }, kUvMetalMin, kUvMetalMax },
		{ { 0.030f,  0.35f, 0.05f }, { 0.022f, 0.65f,  0.05f }, kUvMetalMin, kUvMetalMax },
		{ { 0.030f,  0.35f, 0.85f }, { 0.022f, 0.65f,  0.05f }, kUvMetalMin, kUvMetalMax },
		{ { 0.030f, -0.05f, 1.18f }, { 0.022f, 0.34f,  0.07f }, kUvMetalMin, kUvMetalMax },
		{ { 0.044f, -0.05f, 1.18f }, { 0.020f, 0.26f,  0.03f }, kUvWarnMin,  kUvWarnMax  },
	};
	inline constexpr float kCargoDoorBoundingRadius = 6.0f;

	// 地上シャドウ投影クアッド定数 (ShadowDisc)
	inline constexpr int kShadowVertCount = 4, kShadowIndexCount = 6;
	inline constexpr Vector2  kShadowCornerSigns[kShadowVertCount] = {
		{ -1.0f, -1.0f }, { 1.0f, -1.0f }, { -1.0f, 1.0f }, { 1.0f, 1.0f }
	};
	inline constexpr Vector2  kShadowCornerUVs[kShadowVertCount] = {
		{ 0.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f }
	};
	inline constexpr uint32_t kShadowTriIndices[kShadowIndexCount] = { 0u, 1u, 2u, 1u, 3u, 2u };

	// --- 3Dモデル初期化・機体パーツローカル座標・影投影定数 (Model & Rig) ---
	inline constexpr float kHeliModelBoundingRadius = 220.0f, kFastRopeBoundingRadius = 60.0f, kShadowDiscRadiusX = 4.2f;
	inline constexpr float kShadowDiscRadiusZ = 6.8f, kShadowBoundingRadius = 35.0f;

	inline constexpr Vector4 kCabinInteriorColor = { 0.56f, 0.60f, 0.58f, 1.0f }, kMainRotorBlur1Color = { 1.0f, 1.0f, 1.0f, 0.34f };
	inline constexpr Vector4 kMainRotorBlur2Color = { 1.0f, 1.0f, 1.0f, 0.18f }, kInitialShadowColor = { 0.04f, 0.06f, 0.08f, 0.65f };

	inline constexpr Vector3 kInitialCabinDuckOffset = { 0.0f, 0.105f, 0.75f }, kParkedHiddenPos = { 0.0f, -200.0f, 0.0f };

	inline constexpr float kInitRotorBlurOffset = 0.065f, kHighSpeedRotorBlurOff = 0.092f, kHoverRotorBlurOff = 0.068f;
	inline constexpr float kPickupRotorBlurOff = 0.085f;

	// 機体ローカル基準パーツ座標 (Helicopter Local Mount Offsets)
	inline constexpr float kDoorClosedX = 1.470f, kDoorSlideDeltaX = 0.015f, kDoorLocalY = 1.14f, kDoorOpenZ = -1.90f;
	inline constexpr float kDoorClosedZ = 0.75f;

	inline constexpr Vector3 kMainRotorMastLocal = { 0.0f, 4.15f, 0.75f }, kTailRotorHubLocal = { 0.20f, 3.35f, -8.60f };
	inline constexpr Vector3 kFastRopeAnchorLocal = { 1.65f, 2.38f, 0.75f };
	inline constexpr float kFastRopeVisibleExtraSec = 0.15f, kFastRopeDeployDuration = 0.55f;
	inline constexpr float kFastRopeMeshBaseLen = 25.0f, kFastRopeMinScaleY = 0.01f;

	// 地上ソフトシャドウ投影計算 (Ground Shadow Projection)
	inline constexpr float kShadowMinAlphaScale = 0.01f, kShadowSunOffsetXPerY = 0.10f, kShadowSunOffsetZPerY = 0.15f;
	inline constexpr float kShadowGroundY = 0.035f, kShadowRefHeight = 24.0f, kShadowHeightRatioMin = 0.35f;
	inline constexpr float kShadowHeightRatioMax = 2.5f, kShadowBaseScale = 2.25f, kShadowHeightScaleGain = 0.35f;
	inline constexpr float kShadowBaseAlpha = 0.68f, kShadowAlphaDropPerY = 0.010f, kShadowAlphaMin = 0.12f;
	inline constexpr float kShadowAlphaMax = 0.65f, kShadowAspectZ = 1.55f;
	inline constexpr Vector3 kShadowRuntimeRgb = { 0.03f, 0.05f, 0.07f };

	// フラッシュ減衰 ＆ ローター空間音響ループ (Flash & Rotor Audio)
	inline constexpr float kCutSwitchFlashAlpha = 0.08f, kCutFlashDecayRate = 1.2f, kImpactFlashDecayRate = 1.6f;

	inline constexpr float kInsertAudioMinDist = 14.0f, kInsertAudioMaxDist = 145.0f, kInsertFallbackAtten = 0.72f;
	inline constexpr float kRotorLoopDurationSec = 4.00f, kInsertRotorVolume = 0.56f;

	// --- 開幕インサーション5幕 機体＆アヒル兵士モーション定数 (Acts 1-5 Motion) ---
	inline constexpr float kMainRotorAngularSpeed = 62.0f, kTailRotorAngularSpeed = 88.0f;
	inline constexpr Vector3 kHoverAnchorPos = { 0.0f, 18.0f, 0.0f };

	// Act 1: 超低空急襲アプローチ ＆ フレア制動 (Act 1)
	inline constexpr float kAct1BrakeExponent = 2.6f, kAct1SCurveAmpX = 6.5f, kAct1YawInitial = 0.05f;
	inline constexpr float kAct1YawSettleStartT = 0.65f, kAct1YawSettleSpanT = 0.35f, kAct1FlareStartT = 0.60f;
	inline constexpr float kAct1FlareSpanT = 0.40f, kAct1CruisePitchStart = -0.16f, kAct1CruisePitchGain = 0.10f;
	inline constexpr float kAct1CruiseRollStart = -0.32f, kAct1CruiseRollDecayRat = 0.5f, kAct1FlarePitchAmp = 0.28f;
	inline constexpr float kAct1FlareRollStart = -0.16f, kAct1DoorOpenStartT = 0.56f, kAct1DoorOpenSpanT = 0.38f;
	inline constexpr float kDoorOpenSoundVolume = 0.65f, kDoorOpenTravel = 0.96f, kDoorOpenRestProgress = 0.04f;
	inline constexpr Vector3 kAct1CabinDuckOffset = { 0.05f, 0.105f, 0.75f };

	// Act 2: キャビン前待機・RED/GREEN LIGHT・ロープ飛び移り (Act 2)
	inline constexpr float kAct2HoverOscFreqX = 2.2f, kAct2HoverOscAmpX = 0.04f, kAct2HoverOscFreqY = 1.8f;
	inline constexpr float kAct2HoverOscAmpY = 0.05f, kAct2RotOscFreqX = 1.5f, kAct2RotOscAmp = 0.012f, kAct2RotOscFreqZ = 2.0f;

	inline constexpr float kRopeDeployDampRate = 2.2f, kRopeDeployFreqZ = 11.0f, kRopeDeployAmpZ = 0.055f;
	inline constexpr float kRopeWindFreqZ = 4.2f, kRopeWindAmpZ = 0.012f, kRopeDeployFreqX = 8.5f, kRopeDeployAmpX = 0.035f;

	inline constexpr float kAct2WalkEndPhase = 0.32f, kAct2WalkBobAmp = 0.028f, kAct2WalkDeltaX = 1.35f, kAct2WalkTurnYaw = 1.25f;

	inline constexpr float kAct2PrepEndPhase = 0.68f, kAct2PrepSpanPhase = 0.36f, kAct2GreenBeepPhase = 0.50f;
	inline constexpr float kGreenLightBeepVolume = 0.52f, kAct2NodStartT = 0.50f, kAct2NodEndT = 0.85f;
	inline constexpr float kAct2NodSpanT = 0.35f, kAct2NodAmp = 0.18f, kAct2WindBobFreq = 6.5f, kAct2WindBobAmp = 0.012f;
	inline constexpr float kAct2StandLocalX = 1.40f, kAct2StandPitch = 0.08f, kAct2StandYaw = -1.30f, kAct2StandRoll = 0.04f;

	inline constexpr float kAct2LeapSpanPhase = 0.32f, kJumpSoundVolume = 0.66f, kAct2LeapEndX = 1.34f;
	inline constexpr float kAct2LeapEndZ = -0.25f, kAct2LeapEndYaw = 0.30f, kAct2LeapHopAmp = 0.10f;
	inline constexpr float kAct2LeapPitchGain = 0.06f, kAct2LeapStretchAmp = 0.05f, kAct2RopeTensionDamp = 0.7f;

	// Act 3: ファストロープ高速滑降 ＆ コンバットブレーキ (Act 3)
	inline constexpr float kRopeSlideSoundVolume = 0.70f, kAct3HoverOscFreqX = 2.4f, kAct3HoverOscFreqY = 2.0f;
	inline constexpr float kAct3HoverOscAmp = 0.03f, kAct3BrakeStartPhase = 0.68f, kAct3BrakeSpanPhase = 0.32f;
	inline constexpr float kAct3RopeVibFreq = 42.0f, kAct3RopeVibAmp = 0.004f, kAct3FreefallRatio = 0.82f;
	inline constexpr float kAct3BrakeRatio = 0.18f, kAct3DropTopExtraY = 0.10f;

	inline constexpr float kRopeWorldX = 1.65f, kRopeWorldZ = 0.75f, kRopeFacingYaw = 0.30f, kRopeFrontBodyOffset = 1.05f;
	inline constexpr Vector3 kAct3FreefallScale = { 0.94f, 1.06f, 0.94f };
	inline constexpr float kAct3FreefallPitch = 0.06f, kAct3BrakeScaleGainXZ = 0.16f, kAct3BrakeScaleLossY = 0.14f;
	inline constexpr float kAct3BrakePitchGain = 0.08f;

	// Act 4: 着地激突・360クリアリング・ボルトコッキング (Act 4)
	inline constexpr float kAct4RopeSwayFreq = 14.0f, kAct4RopeSwayAmp = 0.03f, kLandingImpactFlash = 0.16f;
	inline constexpr float kLandingShakeDuration = 0.42f, kLandingShakeIntensity = 0.68f, kLandingBootVolume = 0.82f;
	inline constexpr Vector3 kLandingImpactPoint = { 1.34f, 0.06f, -0.25f };
	inline constexpr float kDuckVoiceVolume = 0.52f;

	inline constexpr float kAct4SkidEndPhase = 0.14f, kAct4SquatEndPhase = 0.12f, kAct4SquatDrop = 0.35f, kAct4SquatFloor = 0.65f;
	inline constexpr Vector3 kAct4SkidStartPos = { 1.34f, 0.0f, -0.25f }, kAct4SkidEndPos = { 1.10f, 0.0f, -0.20f };
	inline constexpr float kAct4SkidSinkY = -0.26f, kAct4SkidScaleBaseX = 1.42f, kAct4SkidScaleBaseY = 0.52f;
	inline constexpr float kAct4SkidScaleBaseZ = 1.38f, kAct4SkidScaleSquatDelta = 0.08f, kAct4SkidPitchScale = 0.44f;
	inline constexpr float kAct4SkidYaw = 0.28f, kAct4SkidRollStart = -0.09f;

	inline constexpr float kAct4SnapEndPhase = 0.22f, kAct4SnapSpanPhase = 0.08f, kAct4SnapScaleStartX = 1.25f;
	inline constexpr float kAct4SnapScaleStartY = 0.70f, kAct4SnapScaleStartZ = 1.22f, kAct4SnapScaleDelta = 0.18f;
	inline constexpr float kAct4SnapStartY = -0.16f, kAct4SnapPitchStart = 0.36f, kAct4SnapPitchEnd = -0.08f;
	inline constexpr float kHeadSnapShakeDuration = 0.14f, kHeadSnapShakeIntensity = 0.14f, kEyeGlintPhase = 0.16f;
	inline constexpr Vector3 kEyeGlintOffset = { -0.06f, 0.44f, 0.30f };
	inline constexpr float kEyeGlintScale = 0.36f;
	inline constexpr Vector4 kEyeGlintColor = { 0.90f, 0.98f, 1.0f, 0.92f };
	inline constexpr float kEyeGlintLife = 0.13f;

	inline constexpr float kAct4RuffleEndPhase = 0.36f, kAct4RuffleSpanPhase = 0.14f, kAct4RuffleCycles = 4.0f;
	inline constexpr float kAct4RuffleRollAmp = 0.20f, kAct4RufflePitchGain = 0.6f, kAct4RuffleScaleLossXZ = 0.10f;
	inline constexpr float kAct4RuffleScaleGainY = 0.16f, kAct4RuffleHopY = 0.03f, kAct4RuffleYawDecay = 0.6f, kFeatherShakePhase = 0.25f;

	inline constexpr float kAct4StrideEndPhase = 0.70f, kAct4StrideSpanPhase = 0.34f, kAct4StrideBobAmp = 0.045f;
	inline constexpr float kFootstep1Phase = 0.44f, kFootstep2Phase = 0.60f, kFootstepVolume = 0.54f;
	inline constexpr float kAct4ScanMidPhase = 0.53f, kAct4ScanHalfSpan = 0.17f, kAct4ScanSnapPortion = 0.36f;
	inline constexpr float kAct4ScanPitch = 0.03f, kAct4ScanStartYaw = 0.12f, kAct4ScanLeftYaw = -0.78f;
	inline constexpr float kAct4ScanRightYaw = 0.68f, kAct4ScanBankRoll = 0.06f;

	inline constexpr float kAct4FrontReturnEnd = 0.75f, kAct4RackPullEnd = 0.80f, kAct4SubStepSpan05 = 0.05f;
	inline constexpr float kAct4FrontReturnPitch = 0.02f, kAct4RackLiftY = 0.015f, kAct4RackPullZ = -0.10f;
	inline constexpr float kAct4RackPitch = -0.18f, kAct4RackRoll = 0.06f, kAct4BoltSlamEnd = 0.86f;
	inline constexpr float kAct4BoltSlamSpan = 0.06f, kAct4SlamReboundZ = 0.05f, kAct4SlamPitch = 0.07f;
	inline constexpr float kAct4SlamScaleDelta = 0.09f, kAct4IdleSpan = 0.14f, kAct4IdleBreathAmp = 0.012f;

	inline constexpr float kCockShakeDuration = 0.24f, kCockShakeIntensity = 0.28f, kBoltRackVolume = 0.80f, kCasingBounceVolume = 0.46f;
	inline constexpr Vector3 kChamberEjectPos = { 0.22f, 0.50f, 0.25f }, kUnitForwardZ = { 0.0f, 0.0f, 1.0f };
	inline constexpr Vector4 kBrassCasingColor = { 1.0f, 0.84f, 0.32f, 1.0f };
	inline constexpr Vector3 kBrassCasingScale = { 0.08f, 0.18f, 0.08f }, kCockGlintPos = { 0.28f, 0.52f, 0.52f };
	inline constexpr float kCockGlintScale = 0.48f;
	inline constexpr Vector4 kCockGlintColor = { 1.0f, 0.97f, 0.85f, 0.92f };
	inline constexpr float kCockGlintLife = 0.14f;

	inline constexpr float kAct4HeliHoverFreq = 2.0f, kAct4HeliHoverAmpY = 0.03f, kAct4HeliNosePitch = 0.14f;

	// Act 5: ヘリ急上昇離脱 ＆ ドア閉鎖 (Act 5)
	inline constexpr float kDoorCloseSoundVolume = 0.52f, kAct5DoorCloseSpan = 0.75f, kAct5ClimbBankAmpX = 3.5f;
	inline constexpr float kAct5ClimbGainY = 24.0f, kAct5ForwardDashZ = 120.0f, kAct5BreakPitchStart = 0.32f;
	inline constexpr float kAct5BreakPitchDecay = 0.4f, kAct5BreakRollEnd = -0.15f;

	// --- 開幕5幕シネマティックカメラ設定 (Acts 1-5 Camera) ---
	inline constexpr Vector3 kAct1CamPosStart = { -22.0f, 7.5f, -32.0f }, kAct1CamPosEnd = { -15.0f, 13.0f, -24.0f };
	inline constexpr float kAct1CamVibFreq = 28.0f, kAct1CamVibBase = 0.008f, kAct1CamVibGain = 0.020f;
	inline constexpr Vector3 kAct1CamLookOffset = { 0.0f, 0.4f, 0.0f };
	inline constexpr float kAct1CamRollStart = -0.06f;

	inline constexpr Vector3 kAct2CamStartOff = { 3.3f, 2.9f, -2.6f }, kAct2CamHoldEndOff = { 2.5f, 2.3f, -1.9f };
	inline constexpr Vector3 kAct2CamLeapEndOff = { 2.0f, 2.0f, -1.5f };
	inline constexpr float kAct2CamTargetLiftY = 0.35f, kAct2CamRoll = 0.035f;

	inline constexpr float kAct3HighCamX = 1.8f, kAct3HighCamLiftY = 2.8f, kAct3HighCamZ = -2.2f;
	inline constexpr float kAct3HighTargetDropY = -0.3f, kAct3HighTargetLeadZ = 0.5f;
	inline constexpr Vector3 kAct3LowCamPos = { -0.5f, 1.25f, 3.4f };
	inline constexpr float kAct3LowTargetY = 0.45f, kAct3CamBlendStartPhase = 0.45f, kAct3CamBlendSpanPhase = 0.55f;
	inline constexpr float kAct3CamRollStart = 0.03f;

	inline constexpr Vector3 kAct4CamPosEnd = { 0.0f, 1.35f, 4.4f };
	inline constexpr float kAct4CamDipEndPhase = 0.18f, kAct4CamDipAmpY = 0.12f, kAct4CamDipRoll = -0.05f;
	inline constexpr float kAct4CamScanRollAmp = 0.028f, kAct4CamPunchStartPhase = 0.78f, kAct4CamPunchEndPhase = 0.86f;
	inline constexpr float kAct4CamPunchSpanPhase = 0.08f, kAct4CamPunchAmpZ = 0.25f, kAct4CamPunchYRatio = 0.15f;
	inline constexpr float kAct4CamFollowTrackRatio = 0.6f, kAct4CamScanLeadX = 0.15f, kAct4CamTargetY = 0.65f;

	inline constexpr float kAct5CamSwerveAmpX = 1.2f, kAct5CamLookTargetY = 0.70f, kAct5CamRotBlendStartT = 0.65f;
	inline constexpr float kAct5CamRotBlendSpanT = 0.35f;

	// --- パーティクルプリセット ＆ 空力エフェクト定数 (Particle Presets) ---
	struct ParticleStyleSpec
	{
		Vector2  scaleAspect;
		Vector4  color;
		Vector4  endColor;
		float    gravity;
		float    drag;
		float    bounceElasticity;
		float    fadeInRatio;
		float    wobbleFreq;
		float    wobbleAmp;
		Vector3  wobbleAxis;
		uint16_t slotLayer;
	};

	inline constexpr Vector3 kDefaultWobbleAxis = { 0.0f, 1.0f, 0.0f };

	// ロープ接触摩擦スパーク (RopeSpark)
	inline constexpr float kRopeSparkStartPhase = 0.05f, kRopeSparkEndPhase = 0.96f, kRopeSparkIntervalBrk = 0.016f;
	inline constexpr float kRopeSparkIntervalSld = 0.036f, kRopeSparkOffsetY = 0.65f, kRopeSparkSpeedBrake = 2.4f;
	inline constexpr float kRopeSparkSpeedSlide = 1.1f, kRopeSparkAspectX = 0.018f, kRopeSparkAspectYBrake = 0.11f;
	inline constexpr float kRopeSparkAspectYSlide = 0.075f, kRopeSparkTargetScale = 0.25f;
	inline constexpr Vector2 kRopeSparkUpVelRange = { 1.2f, 3.0f };
	inline constexpr float kRopeSparkLifeBrake = 0.26f, kRopeSparkLifeSlide = 0.17f;
	inline constexpr ParticleStyleSpec kRopeSparkSlideStyle = { { 0.018f, 0.075f }, { 1.0f, 0.90f, 0.45f, 0.92f },
		{ 1.0f, 0.32f, 0.06f, 0.0f }, 11.5f, 1.8f, 0.42f, 0.0f, 0.0f, 0.0f, kDefaultWobbleAxis, kParticleLayerFront };
	inline constexpr ParticleStyleSpec kRopeSparkBrakeStyle = { { 0.018f, 0.11f }, { 1.0f, 0.90f, 0.45f, 0.92f },
		{ 1.0f, 0.32f, 0.06f, 0.0f }, 11.5f, 1.8f, 0.42f, 0.0f, 0.0f, 0.0f, kDefaultWobbleAxis, kParticleLayerFront };

	// ロープブレーキ摩擦ウィスプ煙 (RopeBrakeSmoke)
	inline constexpr float kRopeSmokeStartPhase = 0.65f, kRopeSmokeJitterXZ = 0.07f, kRopeSmokeOffsetY = -0.08f;
	inline constexpr Vector2 kRopeSmokeScaleRange = { 0.22f, 0.34f };
	inline constexpr float kRopeSmokeTargetMul = 2.4f, kRopeSmokeVelSpreadXZ = 0.55f;
	inline constexpr Vector2 kRopeSmokeUpVelRange = { 0.8f, 1.8f }, kRopeSmokeLifeRange = { 0.42f, 0.68f };
	inline constexpr ParticleStyleSpec kRopeBrakeSmokeStyle = { { 0.85f, 1.35f }, { 0.86f, 0.84f, 0.80f, 0.38f },
		{ 0.72f, 0.72f, 0.74f, 0.0f }, 0.0f, 1.4f, 0.0f, 0.16f, 0.0f, 0.0f, kDefaultWobbleAxis, kParticleLayerFront };

	// 着地360度ドーナツダストリング (LandingDustRing)
	inline constexpr int kLandingDustCount = 20;
	inline constexpr float kLandingDustAngleJit = 0.08f;
	inline constexpr Vector2 kLandingDustSpdRange = { 3.6f, 5.4f };
	inline constexpr float kLandingDustSpawnRad = 0.25f, kLandingDustSpawnY = 0.06f;
	inline constexpr Vector2 kLandingDustScaleRange = { 0.32f, 0.48f };
	inline constexpr float kLandingDustTargetMul = 2.5f;
	inline constexpr Vector2 kLandingDustUpRange = { 0.20f, 0.58f }, kLandingDustLifeRange = { 0.55f, 0.90f };
	inline constexpr ParticleStyleSpec kLandingDustStyle = { { 1.85f, 0.58f }, { 0.66f, 0.59f, 0.48f, 0.46f },
		{ 0.72f, 0.67f, 0.58f, 0.0f }, 0.0f, 2.5f, 0.0f, 0.14f, 0.0f, 0.0f, kDefaultWobbleAxis, kParticleLayerMid };

	// 着地砂利チップ (LandingDebrisChip)
	inline constexpr int kLandingChipCount = 8;
	inline constexpr float kLandingChipSpawnRad = 0.15f, kLandingChipSpawnY = 0.05f;
	inline constexpr Vector2 kLandingChipScaleRange = { 0.024f, 0.042f };
	inline constexpr float kLandingChipTargetMul = 0.75f;
	inline constexpr Vector2 kLandingChipSpdRange = { 2.2f, 4.4f }, kLandingChipUpRange = { 1.2f, 2.6f };
	inline constexpr Vector2 kLandingChipLifeRange = { 0.35f, 0.58f };
	inline constexpr float kLandingChipSpinMax = 14.0f;
	inline constexpr ParticleStyleSpec kLandingChipStyle = { { 1.15f, 0.85f }, { 0.48f, 0.44f, 0.38f, 0.92f },
		{ 0.40f, 0.36f, 0.30f, 0.0f }, 15.2f, 0.0f, 0.38f, 0.0f, 0.0f, 0.0f, kDefaultWobbleAxis, kParticleLayerFront };

	// 着地ブーツ金具スパーク (LandingBootSpark)
	inline constexpr int kLandingSparkCount = 14;
	inline constexpr float kLandingSparkTargetScl = 0.25f;
	inline constexpr Vector2 kLandingSparkSpdRange = { 2.4f, 4.8f }, kLandingSparkUpRange = { 0.75f, 2.1f };
	inline constexpr Vector2 kLandingSparkLifeRange = { 0.18f, 0.32f };
	inline constexpr ParticleStyleSpec kLandingSparkStyle = { { 0.018f, 0.095f }, { 1.0f, 0.88f, 0.48f, 0.88f },
		{ 1.0f, 0.34f, 0.08f, 0.0f }, 12.2f, 1.8f, 0.42f, 0.0f, 0.0f, 0.0f, kDefaultWobbleAxis, kParticleLayerFront };

	// 着地スキッド煙 (LandingSkidSmoke)
	inline constexpr float kSkidSmokeJitterX = 0.10f, kSkidSmokeOffsetY = 0.12f;
	inline constexpr Vector2 kSkidSmokeOffsetZRange = { 0.05f, 0.18f }, kSkidSmokeScaleRange = { 0.28f, 0.40f };
	inline constexpr float kSkidSmokeTargetMul = 2.0f, kSkidSmokeVelJitterX = 0.4f;
	inline constexpr Vector2 kSkidSmokeVelYRange = { 0.2f, 0.5f }, kSkidSmokeVelZRange = { 0.9f, 1.8f };
	inline constexpr float kSkidSmokeLife = 0.42f;
	inline constexpr ParticleStyleSpec kLandingSkidSmokeStyle = { { 1.6f, 0.65f }, { 0.68f, 0.62f, 0.50f, 0.40f },
		{ 0.74f, 0.69f, 0.60f, 0.0f }, 0.0f, 2.0f, 0.0f, 0.15f, 0.0f, 0.0f, kDefaultWobbleAxis, kParticleLayerMid };

	// 羽毛ブルブルシェイク (FeatherShake)
	inline constexpr int kFeatherShakeCount = 6;
	inline constexpr float kFeatherPosJitterX = 0.22f;
	inline constexpr Vector2 kFeatherPosYRange = { 0.24f, 0.56f };
	inline constexpr float kFeatherPosJitterZ = 0.18f;
	inline constexpr Vector2 kFeatherScaleRange = { 0.085f, 0.135f };
	inline constexpr float kFeatherTargetMul = 0.90f, kFeatherVelJitterXZ = 1.4f;
	inline constexpr Vector2 kFeatherVelYRange = { 0.95f, 2.1f };
	inline constexpr Vector4 kFeatherGoldColor = { 1.0f, 0.90f, 0.34f, 0.94f }, kFeatherWhiteColor = { 0.98f, 0.97f, 0.93f, 0.92f };
	inline constexpr Vector4           kFeatherAlternateCols[] = { kFeatherGoldColor, kFeatherWhiteColor };
	inline constexpr Vector2 kFeatherLifeRange = { 1.05f, 1.65f };
	inline constexpr float kFeatherSpinMax = 4.5f;
	inline constexpr ParticleStyleSpec kFeatherShakeStyle = { { 0.76f, 1.28f }, kFeatherGoldColor,
		{ 1.0f, 0.90f, 0.34f, 0.0f }, 1.55f, 2.4f, 0.0f, 0.0f, 8.2f, 0.44f, { 1.0f, 0.15f, 0.6f }, kParticleLayerFront };

	// コッキング放射スパーク (CockSpark)
	inline constexpr int kCockSparkCount = 10;
	inline constexpr float kCockSparkTargetScale = 0.25f, kCockSparkRadialSpeed = 2.4f, kCockSparkUpBias = 0.4f;
	inline constexpr float kCockSparkForwardVel = 0.8f, kCockSparkLife = 0.20f;
	inline constexpr ParticleStyleSpec kCockSparkStyle = { { 0.016f, 0.085f }, { 1.0f, 0.94f, 0.72f, 0.92f },
		{ 1.0f, 0.42f, 0.10f, 0.0f }, 9.5f, 2.2f, 0.0f, 0.0f, 0.0f, 0.0f, kDefaultWobbleAxis, kParticleLayerFront };

	// ヘリ飛行中エフェクト（翼端渦・排気・衝突防止灯・ダウンウォッシュ） (Flight Particles)
	inline constexpr float kAct1HighGStartSec = 2.0f, kAct1ThrottleEndSec = 1.8f, kInsertWashStartSec = 1.3f;
	inline constexpr float kInsertWashExtraEndSec = 0.25f, kInsertWashHeightLimit = 22.0f;

	inline constexpr float kRotorVortexInterval = 0.026f;
	inline constexpr int kRotorBladeCount = 4;
	inline constexpr float kRotorTipRadius = 3.25f, kRotorTipDropY = -0.06f, kRotorTipTangentSpd = 4.4f;
	inline constexpr float kRotorTipDownwashVelY = -2.4f, kVortexAlphaHighG = 0.24f, kVortexAlphaNormal = 0.13f;
	inline constexpr float kVortexScaleHighG = 0.34f, kVortexScaleNormal = 0.24f, kVortexLifeHighG = 0.24f;
	inline constexpr float kVortexLifeNormal = 0.17f, kVortexTargetMul = 1.8f;
	inline constexpr ParticleStyleSpec kRotorVortexStyle = { { 1.85f, 0.52f }, { 0.94f, 0.97f, 1.0f, 0.24f },
		{ 0.90f, 0.94f, 0.98f, 0.0f }, 0.0f, 0.0f, 0.0f, 0.15f, 0.0f, 0.0f, kDefaultWobbleAxis, kParticleLayerFront };

	inline constexpr float kExhaustInterval = 0.030f, kExhaustSpeedFull = 7.2f, kExhaustSpeedCruise = 4.8f;
	inline constexpr float kExhaustScaleFull = 0.36f, kExhaustScaleCruise = 0.25f, kExhaustAlphaFull = 0.30f, kExhaustAlphaCruise = 0.18f;
	inline constexpr float             kExhaustNozzleSides[]  = { -1.0f, 1.0f };
	inline constexpr float kExhaustNozzleSideX = 0.72f, kExhaustNozzleY = 3.42f, kExhaustNozzleZ = -0.55f;
	inline constexpr float kExhaustVelSideX = 0.45f, kExhaustVelDownY = -0.25f, kExhaustJitterXZ = 0.25f;
	inline constexpr Vector2 kExhaustJitterYRange = { -0.15f, 0.20f }, kExhaustLifeRange = { 0.36f, 0.56f };
	inline constexpr float kExhaustTargetMul = 2.8f;
	inline constexpr ParticleStyleSpec kEngineExhaustStyle = { { 1.75f, 0.68f }, { 0.22f, 0.24f, 0.28f, 0.30f },
		{ 0.48f, 0.50f, 0.55f, 0.0f }, 0.0f, 0.9f, 0.0f, 0.18f, 0.0f, 0.0f, kDefaultWobbleAxis, kParticleLayerFront };

	inline constexpr float kTailBeaconPeriod = 0.80f, kBeaconGateDtMul = 1.25f;
	inline constexpr Vector3 kTailBeaconLocal = { 0.05f, 4.35f, -8.70f };
	inline constexpr float kTailBeaconScale = 1.20f;
	inline constexpr Vector4 kTailBeaconColor = { 1.0f, 0.12f, 0.08f, 0.95f };
	inline constexpr float kTailBeaconLife = 0.08f;

	inline constexpr float kBellyStrobePeriod = 1.20f, kBellySecondPulseSec = 0.15f;
	inline constexpr Vector3 kBellyStrobeLocal = { 0.0f, 0.15f, 0.50f };
	inline constexpr float kBellyStrobeScale = 1.35f;
	inline constexpr Vector4 kBellyStrobeColor = { 1.0f, 1.0f, 1.0f, 0.92f };
	inline constexpr float kBellyStrobeLife = 0.06f;

	inline constexpr float kWashBaseHeightMin = 2.0f, kWashRatioMin = 0.15f, kWashCenterMaxY = 5.5f;
	inline constexpr float kWashEffectRadius = 10.5f, kWashStrengthBase = 2.5f, kWashStrengthGain = 7.5f;
	inline constexpr float kDownwashDustInterval = 0.032f, kDownwashGroundY = 0.10f;
	inline constexpr int kDownwashDustCount = 4;
	inline constexpr Vector2 kDownwashSpawnRadRange = { 1.6f, 4.6f };
	inline constexpr float kDownwashOutSpdBase = 3.4f, kDownwashOutSpdGain = 4.8f;
	inline constexpr Vector2 kDownwashSpdMulRange = { 0.85f, 1.15f }, kDownwashPosYRange = { 0.04f, 0.26f };
	inline constexpr Vector2 kDownwashScaleRange = { 0.48f, 0.76f }, kDownwashTargetRange = { 1.85f, 2.60f };
	inline constexpr Vector2 kDownwashUpVelRange = { 0.22f, 0.72f };
	inline constexpr float kDownwashBaseAlpha = 0.34f;
	inline constexpr Vector2 kDownwashLifeRange = { 0.68f, 1.10f };
	inline constexpr ParticleStyleSpec kDownwashDustStyle = { { 2.15f, 0.56f }, { 0.65f, 0.59f, 0.48f, 0.34f },
		{ 0.74f, 0.69f, 0.60f, 0.0f }, 0.0f, 1.65f, 0.0f, 0.20f, 0.0f, 0.0f, kDefaultWobbleAxis, kParticleLayerMid };

	inline constexpr float kDownwashLeafChance = 0.55f, kDownwashLeafAltChance = 0.5f, kDownwashLeafSpawnRad = 3.2f;
	inline constexpr Vector2 kDownwashLeafPosYRange = { 0.15f, 0.75f }, kDownwashLeafSpdRange = { 3.2f, 5.6f };
	inline constexpr Vector2 kDownwashLeafUpRange = { 1.3f, 3.0f };
	inline constexpr Vector4 kLeafBrightGreen = { 0.24f, 0.58f, 0.16f, 0.92f }, kLeafOliveGreen = { 0.35f, 0.48f, 0.14f, 0.92f };
	inline constexpr float kDownwashLeafScale = 0.24f, kDownwashLeafLife = 0.72f;

	// --- 終盤エクストラクション回収機 (Dustoff-1) 軌道・音響定数 (Extraction Pickup) ---
	inline constexpr float kPickupMainRotorSpeed = 64.0f, kPickupTailRotorSpeed = 90.0f, kPickupApproachDuration = 4.2f;
	inline constexpr Vector3 kPickupSkyEntryOffset = { 34.0f, 35.0f, 60.0f }, kPickupHoldBaseOffset = { 8.2f, 14.2f, 14.8f };
	inline constexpr Vector3 kPickupHoldOscFreq = { 0.85f, 1.15f, 0.65f }, kPickupHoldOscAmp = { 1.3f, 0.35f, 1.0f };

	inline constexpr float kPickupZoneBaseDescent = 0.32f, kPickupZoneGainDescent = 0.53f;
	inline constexpr float kPickupBlendSpeedCleared = 2.8f, kPickupBlendSpeedZone = 1.7f;

	inline constexpr Vector3 kPickupHoverBaseOffset = { 3.15f, 5.20f, 4.40f }, kPickupClimaxDeltaOffset = { -0.35f, -2.35f, -1.95f };
	inline constexpr float kPickupHoverBobFreq = 2.5f, kPickupHoverBobAmpY = 0.08f;

	inline constexpr float kPickupFlarePulseAmp = 0.14f, kPickupApproachPitch = -0.12f, kPickupClimaxPitchDelta = -0.04f;
	inline constexpr float kPickupOrbitYaw = 3.55f, kPickupPadFacingYaw = 2.65f, kPickupApproachRoll = 0.24f;
	inline constexpr float kPickupRollOscFreq = 1.4f, kPickupRollOscAmp = 0.015f;

	inline constexpr float kPickupDoorOpenStartT = 0.55f, kPickupDoorOpenSpanT = 0.45f, kPickupDoorTravel = 0.95f;
	inline constexpr float kPickupDoorSoundTrigger = 0.10f, kPickupDoorSoundVolume = 0.62f;
	inline constexpr float kPickupDoorAudioMinDist = 8.0f, kPickupDoorAudioMaxDist = 58.0f;

	inline constexpr float kPickupRotorAudioMinDist = 10.0f, kPickupRotorAudioMaxDist = 95.0f, kPickupRotorVolume = 0.54f;

	inline constexpr float kPickupHighGDescent = 0.35f, kPickupFullThrottleT = 0.7f, kPickupWashHeightLimit = 16.5f;

	// --- シネマスコープHUDレイアウト・カラーパレット・無線字幕テーブル (DrawUI) ---
	struct RgbColor { uint8_t r; uint8_t g; uint8_t b; };

	struct RadioSubtitleCue
	{
		float         startTime;
		float         endTime;
		float         fadeInDuration;
		float         fadeOutDuration;
		const char8_t* text;
	};

	inline constexpr int kAlphaMin = 0, kAlphaMax = 255;
	inline constexpr float kAlphaMaxF = 255.0f, kCineFontMinPx = 12.0f, kCineFontMaxPx = 96.0f;
	inline constexpr float kCineShadowAlphaRatio = 0.88f, kCineShadowOffsetPx = 1.0f;

	inline constexpr float kUiRefWidth = 1280.0f, kUiScaleMin = 0.75f, kUiScaleMax = 1.65f;

	inline constexpr float kMaxTotalFlashAlpha = 0.35f, kMinVisibleFlashAlpha = 0.008f;

	inline constexpr float kLetterboxHeightRatio = 0.088f, kLetterboxExitSpeed = 1.25f, kLetterboxContentMinRat = 0.35f;
	inline constexpr float kTopLineThickness = 1.2f;

	inline constexpr float kHeaderFontSize = 18.0f;
	inline constexpr int kHeaderMaxAlpha = 248;

	inline constexpr float kSkipPulseBase = 0.72f, kSkipPulseAmp = 0.28f, kSkipPulseFreq = 4.5f;
	inline constexpr int kSkipMaxAlpha = 240;
	inline constexpr float kSkipFontSize = 15.0f, kSkipPadX = 12.0f, kSkipPadY = 8.0f, kRightEdgeMargin = 26.0f;
	inline constexpr int kSkipBgMaxAlpha = 210;
	inline constexpr float kBoxCornerRadius = 4.0f, kBoxBorderThickness = 1.1f;

	inline constexpr float kLeftEdgeMargin = 34.0f, kRecBlinkDutyRatio = 0.65f, kRecDotRadius = 4.5f;
	inline constexpr int kRecDotMaxAlpha = 245;
	inline constexpr float kRecFontSize = 14.0f, kRecTextOffsetX = 12.0f;
	inline constexpr int kRecTextMaxAlpha = 232;

	inline constexpr float kTelemetryFadeTailSec = 0.3f;
	inline constexpr size_t kTelemetryBufSize = 64;
	inline constexpr int kTelemetryMaxSingleSec = 9;
	inline constexpr float kTelemetryFontSize = 15.0f, kTelemetryOffsetY = 20.0f;
	inline constexpr int kTelemetryMaxAlpha = 235;

	inline constexpr float kLampExtraShowSec = 0.75f, kLampSlideInSec = 0.25f, kLampFadeStartOffsetSec = 0.35f;
	inline constexpr float kLampFadeDurationSec = 0.40f, kLampFontSize = 15.0f, kLampWidthPad = 42.0f;
	inline constexpr float kLampHeight = 34.0f, kLampSlideOffsetX = 60.0f, kLampOffsetY = 14.0f;
	inline constexpr int kLampMaxAlpha = 245;
	inline constexpr float kLampBorderThickness = 1.4f, kLampDotOffsetX = 16.0f, kLampDotRadius = 5.5f;
	inline constexpr int kLampDotSegments = 14;
	inline constexpr float kLampTextOffsetX = 28.0f;

	inline constexpr float kSubtitleEndExtraSec = 0.35f, kSubtitleMinAlpha = 0.01f, kSubtitleFontSize = 18.0f;
	inline constexpr float kSubtitlePadW = 34.0f, kSubtitlePadH = 14.0f, kSubtitleMarginBottom = 14.0f;
	inline constexpr int kSubtitleBgMaxAlpha = 224, kSubtitleBorderMaxAlpha = 185, kSubtitleTextMaxAlpha = 252;

	// UIカラーパレット (HUD Palette)
	inline constexpr uint32_t kColLetterboxFill = UI_COL32(5, 7, 10, 248);
	inline constexpr RgbColor kColBlackRgb = { 0, 0, 0 }, kColWarmFlashRgb = { 255, 246, 224 }, kColTopAmberLineRgb = { 255, 196, 56 };
	inline constexpr int kTopAmberLineAlpha = 95;
	inline constexpr RgbColor kColBottomSteelLineRgb = { 82, 118, 138 };
	inline constexpr int kBottomSteelLineAlpha = 120;
	inline constexpr RgbColor kColHeaderGoldRgb = { 255, 214, 92 }, kColSkipBgRgb = { 18, 28, 38 };
	inline constexpr RgbColor kColSkipBorderRgb = { 255, 206, 78 }, kColSkipTextRgb = { 248, 252, 255 };
	inline constexpr RgbColor kColRecDotRgb = { 255, 58, 58 }, kColRecTextRgb = { 198, 224, 242 }, kColTelemetryRgb = { 204, 236, 250 };

	inline constexpr RgbColor kColGreenLampBgRgb = { 10, 38, 24 }, kColGreenLampBorderRgb = { 62, 252, 152 };
	inline constexpr RgbColor kColGreenLampTextRgb = { 165, 255, 206 }, kColRedLampBgRgb = { 38, 12, 14 };
	inline constexpr RgbColor kColRedLampBorderRgb = { 255, 82, 74 }, kColRedLampTextRgb = { 255, 168, 158 };

	inline constexpr RgbColor kColSubtitleBgRgb = { 8, 13, 20 }, kColSubtitleBorderRgb = { 82, 128, 156 };
	inline constexpr RgbColor kColSubtitleTextRgb = { 246, 250, 255 };

	// タクティカル無線字幕タイムラインテーブル (RadioSubtitles: Cut1=3.40, Cut3=7.10, Cut4=8.80)
	inline constexpr RadioSubtitleCue kInsertionSubtitles[] = {
		{ 0.25f,        3.40f - 0.10f, 0.25f, 0.25f, u8"[DUSTOFF-1] 降下地点へ超低空進入！ 衝撃に備えろ！" },
		{ 3.40f + 0.05f, 3.40f + 1.45f, 0.20f, 0.20f, u8"[CREW CHIEF] ドア開放、ファストロープ投下！ 待機せよ…" },
		{ 3.40f + 1.48f, 7.10f - 0.15f, 0.18f, 0.25f, u8"[DUSTOFF-1] グリーンライト！ 一気に滑り降りろ！" },
		{ 7.10f + 0.20f, 8.80f + 0.25f, 0.22f, 0.30f, u8"[OPERATOR] 着地完了、周辺クリア！ 作戦を開始する！" },
	};
}
