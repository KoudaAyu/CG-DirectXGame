#pragma once

#include "Application/Config/GameConfig.h"
#include "Application/UI/TacticalUICanvas.h"
#include "Vector.h"
#include <cstdint>

namespace GameOverSceneTuning
{
	// --- 共通数学・時間・オーディオ定数 ---
	constexpr float kPi = GameConfig::Math::kPi, kHalfPi = GameConfig::Math::kHalfPi, kCubicInScale = 4.0f;
	constexpr float kTargetFrameRate = 60.0f, kFixedDeltaTime = 1.0f / kTargetFrameRate;
	constexpr uint32_t kAlphaByteMask = 0xFFu;

	constexpr float kEarlySkipEnableTime = 0.50f, kTakeoffSkipMinTime = 0.25f, kSceneTransitionDuration = 0.5f;

	constexpr float kUiClickVolume = 0.68f, kHeliDoorVolume = 0.64f, kDuckQuackVolume = 0.52f;
	constexpr float kHolsterLoadVolume = 0.56f, kBgmLoopEndSec = 20.00f, kBgmVolume = 0.34f, kRotorLoopEndSec = 4.00f;
	constexpr float kRotorVolApproaching = 0.28f, kRotorVolLoading = 0.34f, kRotorVolReady = 0.38f;
	constexpr float kRotorVolTakeoffPeak = 0.54f, kRotorVolTakeoffFade = 0.55f, kWalkStepVolume = 0.32f;
	constexpr int kWalkStepCooldownMs = 100;

	// --- ライティング・カメラ定数 ---
	constexpr Vector4 kDirLightColor{ 0.38f, 0.44f, 0.58f, 1.0f };
	constexpr Vector3 kDirLightDirection{ -0.35f, -0.85f, 0.40f };
	constexpr float kDirLightIntensity = 0.85f;

	constexpr Vector4 kPointLightInitColor{ 1.0f, 0.88f, 0.60f, 1.0f }, kPointLightPulseColor{ 1.0f, 0.88f, 0.62f, 1.0f };
	constexpr Vector3 kPointLightPosition{ 3.2f, 4.5f, 2.2f };
	constexpr float kPointLightInitIntensity = 4.5f, kPointLightPulseBase = 4.4f, kPointLightPulseFreq = 5.0f;
	constexpr float kPointLightPulseAmp = 0.18f, kPointLightRadius = 32.0f, kPointLightDecay = 1.0f;

	constexpr Vector3 kBaseCameraPos{ -0.2f, 2.0f, -7.2f }, kBaseCameraRot{ 0.10f, 0.12f, 0.0f };
	constexpr float kBaseCameraFovY = 0.55f, kCamLookUpPitchDelta = 0.32f, kCamLookUpYawDelta = 0.08f;
	constexpr float kCamLookUpLiftY = 1.8f, kCamShakeDecayScale = 0.75f, kCamShakeFreqX = 52.0f, kCamShakeAmpX = 0.022f;
	constexpr float kCamShakeFreqY = 64.0f, kCamShakeAmpY = 0.016f;

	// --- プロシージャルボックスメッシュ定義（担架 ＆ ヘリコプターカーゴドア） ---
	constexpr int kBoxCornerCount = 8, kBoxFaceCount = 6, kFaceVertexCount = 4, kFaceIndexCount = 6;
	constexpr float kProceduralBoundingRadius = 4.0f;

	constexpr int kBoxFaces[kBoxFaceCount][kFaceVertexCount] = {
		{ 0, 1, 2, 3 }, // Front (-Z)
		{ 5, 4, 7, 6 }, // Back (+Z)
		{ 4, 0, 3, 7 }, // Left (-X)
		{ 1, 5, 6, 2 }, // Right (+X)
		{ 3, 2, 6, 7 }, // Top (+Y)
		{ 4, 5, 1, 0 }, // Bottom (-Y)
	};

	constexpr Vector3 kBoxNormals[kBoxFaceCount] = {
		{  0.0f,  0.0f, -1.0f },
		{  0.0f,  0.0f,  1.0f },
		{ -1.0f,  0.0f,  0.0f },
		{  1.0f,  0.0f,  0.0f },
		{  0.0f,  1.0f,  0.0f },
		{  0.0f, -1.0f,  0.0f },
	};

	constexpr bool kQuadUseMaxU[kFaceVertexCount] = { false, true, true, false };
	constexpr bool kQuadUseMaxV[kFaceVertexCount] = { false, false, true, true };
	constexpr uint32_t kQuadTriOffsets[kFaceIndexCount] = { 0, 1, 2, 0, 2, 3 };

	struct BoxPartSpec { Vector3 center; Vector3 size; };

	// アルミニウム担架棒・グリップ・キャンバス生地・折りたたみ脚・救急枕
	constexpr BoxPartSpec kStretcherBoxes[] = {
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

	enum class DoorUvZone : uint8_t
	{
		Olive = 0, // オリーブドラブ装甲（機体同色）
		Metal = 1, // 黒染めスチール・レール・ベゼル
		Glass = 2, // 濃色スモークキャビンウィンドウ
		Warn  = 3, // レスキューイエロー（緊急解除ハンドル）
	};

	struct UvRectSpec { Vector2 uvMin; Vector2 uvMax; };

	constexpr UvRectSpec kDoorUvRects[] = {
		{ { 0.25f, 0.05f }, { 0.35f, 0.15f } }, // Olive
		{ { 0.65f, 0.05f }, { 0.75f, 0.15f } }, // Metal
		{ { 0.10f, 0.75f }, { 0.25f, 0.85f } }, // Glass
		{ { 0.85f, 0.05f }, { 0.95f, 0.15f } }, // Warn
	};

	struct DoorBoxPartSpec { Vector3 center; Vector3 size; DoorUvZone uvZone; };

	// スライドドアパネル・上下レール・補強リブ・強化ガラス窓・枠ベゼル・緊急ラッチ
	constexpr DoorBoxPartSpec kHeliDoorBoxes[] = {
		{ { 0.0f,   0.0f,  0.0f  }, { 0.04f,  2.12f, 2.72f }, DoorUvZone::Olive },
		{ { 0.012f,  1.08f, 0.0f }, { 0.065f, 0.06f, 2.76f }, DoorUvZone::Metal },
		{ { 0.012f, -1.08f, 0.0f }, { 0.065f, 0.06f, 2.76f }, DoorUvZone::Metal },
		{ { 0.022f, -0.45f, 0.0f }, { 0.015f, 0.04f, 2.62f }, DoorUvZone::Olive },
		{ { 0.022f,  0.80f, 0.0f }, { 0.015f, 0.04f, 2.62f }, DoorUvZone::Olive },
		{ { 0.022f,  0.35f, 0.45f }, { 0.012f, 0.65f, 0.75f }, DoorUvZone::Glass },
		{ { 0.026f,  0.70f, 0.45f }, { 0.020f, 0.05f, 0.85f }, DoorUvZone::Metal },
		{ { 0.026f,  0.00f, 0.45f }, { 0.020f, 0.05f, 0.85f }, DoorUvZone::Metal },
		{ { 0.026f,  0.35f, 0.05f }, { 0.020f, 0.65f, 0.05f }, DoorUvZone::Metal },
		{ { 0.026f,  0.35f, 0.85f }, { 0.020f, 0.65f, 0.05f }, DoorUvZone::Metal },
		{ { 0.026f, -0.05f, 1.18f }, { 0.020f, 0.34f, 0.07f }, DoorUvZone::Metal },
		{ { 0.042f, -0.05f, 1.18f }, { 0.020f, 0.26f, 0.03f }, DoorUvZone::Warn  },
	};

	// --- 3D ジオラマ配置・モデルスケール・小道具テーブル ---
	constexpr float kHeliScale = 0.65f;
	constexpr Vector3 kHeliScaleVec{ kHeliScale, kHeliScale, kHeliScale };
	constexpr float kHeliBoundingRadius = 80.0f;

	constexpr Vector3 kHelipadScale{ 8.0f, 8.0f, 1.0f }, kHelipadPos{ 3.2f, 0.02f, 2.2f };
	constexpr float kHelipadBoundingRadius = 15.0f;

	constexpr Vector3 kDuckScaleVec{ 0.38f, 0.38f, 0.38f };
	constexpr float kDuckBoundingRadius = 10.0f;
	constexpr Vector3 kStretcherScaleVec{ 0.65f, 0.65f, 0.65f };

	constexpr Vector3 kGroundScale{ 60.0f, 1.0f, 60.0f };
	constexpr float kGroundBoundingRadius = 150.0f, kPropBoundingRadius = 35.0f;

	enum class PropKind : uint8_t
	{
		PineTree = 0,
		Watchtower,
		Fence,
		Container,
		SandbagWall,
		BarrelStack,
		Crate,
		Count
	};

	struct PropPlacementSpec { PropKind kind; Vector3 pos; Vector3 rot; Vector3 scale; };

	constexpr PropPlacementSpec kDioramaProps[] = {
		// (A) 最奥の針葉樹林帯（鬱蒼としたタイガの森・多層シルエット）
		{ PropKind::PineTree, { -16.5f, 0.0f, 19.0f }, { 0.0f, 0.4f, 0.0f }, { 2.0f, 2.3f, 2.0f } },
		{ PropKind::PineTree, { -13.0f, 0.0f, 16.5f }, { 0.0f, 1.1f, 0.0f }, { 1.7f, 1.9f, 1.7f } },
		{ PropKind::PineTree, { -10.0f, 0.0f, 18.0f }, { 0.0f, 2.3f, 0.0f }, { 1.9f, 2.2f, 1.9f } },
		{ PropKind::PineTree, {  -6.5f, 0.0f, 15.0f }, { 0.0f, 0.8f, 0.0f }, { 1.6f, 1.8f, 1.6f } },
		{ PropKind::PineTree, {  -3.0f, 0.0f, 17.5f }, { 0.0f, 1.7f, 0.0f }, { 1.8f, 2.1f, 1.8f } },
		{ PropKind::PineTree, {   0.5f, 0.0f, 19.5f }, { 0.0f, 2.9f, 0.0f }, { 2.1f, 2.4f, 2.1f } },
		{ PropKind::PineTree, {   4.0f, 0.0f, 16.5f }, { 0.0f, 0.5f, 0.0f }, { 1.7f, 2.0f, 1.7f } },
		{ PropKind::PineTree, {   7.5f, 0.0f, 18.5f }, { 0.0f, 1.9f, 0.0f }, { 1.9f, 2.2f, 1.9f } },
		{ PropKind::PineTree, {  11.0f, 0.0f, 15.5f }, { 0.0f, 0.2f, 0.0f }, { 1.6f, 1.8f, 1.6f } },
		{ PropKind::PineTree, {  14.5f, 0.0f, 17.5f }, { 0.0f, 2.1f, 0.0f }, { 1.8f, 2.1f, 1.8f } },
		{ PropKind::PineTree, {  18.0f, 0.0f, 20.0f }, { 0.0f, 1.3f, 0.0f }, { 2.0f, 2.3f, 2.0f } },
		// (B) 敵勢力監視・前哨見張り塔（森林境界左翼）
		{ PropKind::Watchtower, { -8.5f, 0.0f, 12.5f }, { 0.0f, 0.25f, 0.0f }, { 1.0f, 1.0f, 1.0f } },
		// (C) 救難基地境界の有刺鉄線・防護フェンス（Z=8.8m, 5.0mピッチ隙間なし連結）
		{ PropKind::Fence, { -5.0f, 0.0f, 8.8f }, { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f } },
		{ PropKind::Fence, {  0.0f, 0.0f, 8.8f }, { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f } },
		{ PropKind::Fence, {  5.0f, 0.0f, 8.8f }, { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f } },
		{ PropKind::Fence, { 10.0f, 0.0f, 8.8f }, { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f } },
		// (D) 前線司令・救護所ミリタリーコンテナ（左奥）
		{ PropKind::Container, { -6.5f, 0.0f, 5.5f }, { 0.0f, 0.35f, 0.0f }, { 1.0f, 1.0f, 1.0f } },
		// (E) 土嚢陣地バリア（左側アプローチ警備 ＆ 右側外縁防衛）
		{ PropKind::SandbagWall, { -3.8f, 0.0f, 4.6f }, { 0.0f,  0.15f, 0.0f }, { 1.0f, 1.0f, 1.0f } },
		{ PropKind::SandbagWall, {  8.0f, 0.0f, 5.2f }, { 0.0f, -0.20f, 0.0f }, { 1.0f, 1.0f, 1.0f } },
		// (F) 軍用ドラム缶・燃料補給スタック（左右基地施設）
		{ PropKind::BarrelStack, { -5.2f, 0.0f, 3.8f }, { 0.0f,  0.40f, 0.0f }, { 1.0f, 1.0f, 1.0f } },
		{ PropKind::BarrelStack, {  6.2f, 0.0f, 4.8f }, { 0.0f, -0.35f, 0.0f }, { 1.0f, 1.0f, 1.0f } },
		// (G) 軍用弾薬・補給物資木箱スタック（段積み・配置）
		{ PropKind::Crate, { -4.4f, 0.0f,  4.0f }, { 0.0f,  0.20f, 0.0f }, { 0.95f, 0.95f, 0.95f } },
		{ PropKind::Crate, { -4.4f, 1.12f, 4.0f }, { 0.0f,  0.50f, 0.0f }, { 0.72f, 0.72f, 0.72f } },
		{ PropKind::Crate, {  7.0f, 0.0f,  4.2f }, { 0.0f, -0.15f, 0.0f }, { 1.0f,  1.0f,  1.0f  } },
		{ PropKind::Crate, {  8.2f, 0.0f,  4.0f }, { 0.0f,  0.30f, 0.0f }, { 0.90f, 0.90f, 0.90f } },
		// (H) ヘリパッド外縁の境界標識マーカー（進入・着陸誘導）
		{ PropKind::Crate, { -1.0f, 0.0f, -2.0f }, { 0.0f,  0.10f, 0.0f }, { 0.45f, 0.45f, 0.45f } },
		{ PropKind::Crate, {  7.4f, 0.0f, -2.0f }, { 0.0f, -0.10f, 0.0f }, { 0.45f, 0.45f, 0.45f } },
	};

	// --- ヘリコプター・担架チームアニメーション定数 ---
	constexpr float kMainRotorSpeedApproaching = 9.0f, kTailRotorSpeedApproaching = 24.0f;
	constexpr float kMainRotorSpeedLoading = 14.5f, kTailRotorSpeedLoading = 30.0f, kMainRotorSpeedReady = 20.0f;
	constexpr float kTailRotorSpeedReady = 36.0f, kTakeoffSpoolDuration = 0.45f, kMainRotorTakeoffBoost = 20.5f;
	constexpr float kTailRotorTakeoffBoost = 46.0f, kTakeoffAscendStartRatio = 0.15f, kTakeoffAscendSpanRatio = 0.85f;
	constexpr float kTakeoffLiftCurveExp = 1.3f;
	constexpr Vector3 kTakeoffLiftOffset{ 6.5f, 9.5f, 4.5f };
	constexpr float kTakeoffPitchAmp = 0.10f, kTakeoffPitchFreqScale = 0.9f, kTakeoffRollAmp = -0.15f, kTakeoffRollFreqScale = 0.8f;

	constexpr Vector3 kMastLocalOffset{ 0.0f, 4.15f, 0.75f }, kTailRotorLocalOffset{ 0.20f, 3.35f, -8.60f };
	constexpr float kRotorBlurMaxOffset = 0.095f;

	constexpr float kDoorOpenLocalX = 1.470f, kDoorSlideDeltaX = 0.015f, kDoorLocalY = 1.14f, kDoorOpenLocalZ = -1.90f;
	constexpr float kDoorClosedLocalZ = 0.75f;

	constexpr Vector3 kStretcherCabinLocal{ -0.35f, 0.19f, 0.25f }, kWoundedCabinLocal{ -0.35f, 0.30f, 0.25f };
	constexpr Vector3 kFrontMedicCabinLocal{ 0.35f, 0.105f, 0.55f }, kRearMedicCabinLocal{ 0.35f, 0.105f, -0.15f };

	constexpr float kWoundedPitchTilt = -0.10f, kWoundedSupineRoll = -1.55f, kBreathFreq = 2.8f, kBreathAmp = 0.015f;

	constexpr float kWalkPhaseSpeed = 6.5f;
	constexpr Vector3 kApproachTargetPos{ 1.50f, 0.0f, 1.45f };
	constexpr float kWalkWaddleAmp = 0.14f, kWalkBobAmp = 0.07f, kMedicCarryOffsetX = 0.95f;
	constexpr float kStretcherCarryHeightY = 0.24f, kWoundedCarryHeightY = 0.35f;

	constexpr float kFrontMedicLoadEndRatio = 0.65f, kBedLoadStartRatio = 0.10f, kBedLoadSpanRatio = 0.80f;
	constexpr float kBedLoadLiftArcHeight = 0.14f, kRearMedicLoadStartRatio = 0.30f, kRearMedicLoadSpanRatio = 0.70f;

	// --- 2D 映画的HUD ＆ カラーパレット定数 ---
	constexpr float kHudEnterDelay = 0.20f, kHudEnterDuration = 0.80f, kLetterboxHeightRatio = 0.065f, kRuleThickness = 1.0f;

	constexpr float kTitlePosYRatio = 0.38f, kTitleFontSize = 56.0f, kTitleShadowDropY = 3.0f;
	constexpr float kTitleShadowDiagonal = 1.5f, kCrimsonRuleHalfWidth = 170.0f, kCrimsonRuleOffsetY = 68.0f;
	constexpr float kSubStatusFontSize = 16.0f, kSubStatusOffsetY = 76.0f;

	constexpr float kPromptOffsetUpY = 8.0f, kPromptFontSize = 16.0f, kApproachBlinkBase = 0.75f;
	constexpr float kApproachBlinkAmp = 0.25f, kApproachBlinkFreq = 4.5f, kReadyBlinkBase = 0.70f;
	constexpr float kReadyBlinkAmp = 0.30f, kReadyBlinkFreq = 6.0f, kTakeoffDotsSpeed = 5.0f;
	constexpr int kTakeoffDotsCount = 4, kPromptBufferSize = 128;
	constexpr float kPercentScale = 100.0f;

	constexpr float kExitFadeStartRatio = 0.55f, kExitFadeSpanRatio = 0.45f;

	constexpr uint32_t kColVignetteTint = UI_COL32(255, 255, 255, 220), kColLetterboxFill = UI_COL32(8, 10, 14, 255);
	constexpr uint32_t kColLetterboxRule = UI_COL32(35, 48, 64, 180), kColTitleShadowDeep = UI_COL32(0, 0, 0, 240);
	constexpr uint32_t kColTitleShadowSide = UI_COL32(0, 0, 0, 220), kColTitleMain = UI_COL32(248, 250, 252, 255);
	constexpr uint32_t kColCrimsonRule = UI_COL32(185, 28, 28, 175), kColSubStatus = UI_COL32(180, 195, 215, 230);
	constexpr uint32_t kColPromptApproaching = UI_COL32(215, 225, 235, 230);
	constexpr uint32_t kColPromptLoading = UI_COL32(251, 191, 36, 240), kColPromptReady = UI_COL32(74, 222, 128, 255);
	constexpr uint32_t kColPromptTakeoff = UI_COL32(203, 213, 225, 240);
}
