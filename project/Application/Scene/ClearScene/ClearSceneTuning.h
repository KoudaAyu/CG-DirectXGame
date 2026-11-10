#pragma once

#include "Application/Config/GameConfig.h"
#include "Application/UI/TacticalUICanvas.h"
#include "Vector.h"
#include <cstddef>
#include <cstdint>

namespace ClearSceneTuning
{
	// --- 汎用数学・補間・文字列整形定数 ---
	namespace Common
	{
		inline constexpr float kFixedDeltaTime = 1.0f / 60.0f, kExpoSnapThreshold = 0.999f, kExpoDecayExponent = -10.0f;
		inline constexpr float kCubicInOutScale = 4.0f, kSmoothstepC1 = 3.0f, kNormalMinLen = 0.0001f;
		inline constexpr float kCylinderNormalMinLen = 0.001f, kGridClampMax = 0.99999f;

		inline constexpr size_t kRawUintBufSize = 32, kCommaTempBufSize = 48;
		inline constexpr int kCommaMaxWriteIndex = 44, kCommaGroupDigits = 3;
		inline constexpr size_t kStatTextBufSize = 24, kCommaOutBufSize = 32, kAmountTextBufSize = 40, kLootCountBufSize = 16;
		inline constexpr int kSecondsPerMinute = 60;
		inline constexpr uint32_t kByteMask = 0xFFu;
		inline constexpr size_t kAxisX = 0, kAxisY = 1, kAxisZ = 2;

		inline constexpr Vector4 kWhiteColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	}

	// --- 演出タイムライン ＆ サウンドミキシング定数 ---
	namespace Timeline
	{
		inline constexpr float kSkipInputGuardSec     = 0.30f; // 冒頭誤スキップ防止秒数 (sec)
		inline constexpr float kInboundDoorOpenRatio  = 0.72f; // 飛来中ドア開放SEトリガー比率 (0..1)
		inline constexpr float kInboundTouchdownRatio = 0.92f; // 飛来中接地SEトリガー比率 (0..1)
		inline constexpr float kCutFlashDecayRate     = 2.8f;  // リザルト転換フラッシュ減衰速度 (1/s)

		inline constexpr float kCountUpStartDelaySec  = 0.12f; // 数値カウントアップ開始遅延 (sec)
		inline constexpr float kCountUpDurationSec    = 0.92f; // 数値カウントアップ所要秒数 (sec)
		inline constexpr float kCountTickEndRatio     = 0.98f; // カウントティックSE終了進捗 (0..1)
		inline constexpr float kGradeRevealSec        = 0.34f; // グレード文字が確定する秒数 (sec)
		inline constexpr float kFullRevealSec         = 1.15f; // 全数値・内訳の展開が完了する秒数 (sec)
	}

	namespace Audio
	{
		inline constexpr float kVolTransitionClick = 0.70f, kVolSkipClick = 0.52f, kVolRevealClick = 0.58f, kVolUiHover = 0.38f;
		inline constexpr int kHoverCooldownMs = 50;

		inline constexpr float kVolHeliDoor = 0.62f, kVolTouchdown = 0.55f, kVolDuckRun = 0.50f, kVolDuckBoard = 0.52f;
		inline constexpr float kVolDuckGrade = 0.48f, kVolBoardRoll = 0.50f, kVolBoardHolster = 0.56f;
		inline constexpr float kVolTakeoffExfil = 0.58f, kVolCountTick = 0.42f;
		inline constexpr int kCountTickCooldown = 48;
		inline constexpr float kVolGradeStamp = 0.78f, kVolRunStep = 0.42f;
		inline constexpr int kRunStepCooldownMs = 65;

		inline constexpr float kRotorLoopSec = 4.00f, kRotorVolDefault = 0.44f, kRotorVolInboundMin = 0.34f;
		inline constexpr float kRotorVolInboundAdd = 0.16f, kRotorVolTakeoff = 0.56f, kRotorVolCruise = 0.22f;

		inline constexpr float kFlareLoopSec = 3.00f, kFlareVolGround = 0.34f, kFlareVolTakeoff = 0.22f;

		inline constexpr float kBgmLoopSec = 24.00f, kBgmVolResult = 0.38f;
	}

	// --- 単独起動時のフォールバック戦績データ ---
	namespace FallbackStats
	{
		inline constexpr float kEmptyRaidTimeThreshold = 0.1f, kRaidTime = 94.5f, kMaxRaidTime = 300.0f;
		inline constexpr int kEnemiesKilled = 4, kTargetsDestroyed = 4, kTotalTargets = 4, kShotsFired = 28, kShotsHit = 22;
		inline constexpr float kRemainingHp = 82.0f, kMaxHp = GameConfig::Player::kDefaultMaxHp;
		inline constexpr int kDamageTakenCount = 1, kLootedGoldDucks = 1, kLootedCashCount = 2, kLootedCashValue = 18500;
		inline constexpr int kLootedMedkits = 1, kLootedAmmoBoxes = 1, kTotalLootValue = 76500;
	}

	// --- プロシージャルメッシュ定義 (Procedural Box & Cylinder Specs) ---
	namespace Meshes
	{
		struct ProceduralBoxSpec { Vector3 center; Vector3 size; Vector2 uvMin; Vector2 uvMax; };

		struct CylinderBandSpec { float y0; float y1; float radiusScale; Vector2 uv; };

		inline constexpr size_t kBoxCornerCount = 8, kBoxFaceCount = 6, kBoxFaceVerts = 4, kQuadIndexCount = 6, kCapTriVerts = 3;

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

		inline constexpr int kBoxFaces[kBoxFaceCount][kBoxFaceVerts] = {
			{ 0, 1, 2, 3 }, { 5, 4, 7, 6 }, { 4, 0, 3, 7 },
			{ 1, 5, 6, 2 }, { 3, 2, 6, 7 }, { 4, 5, 1, 0 }
		};

		inline constexpr Vector3 kBoxNormals[kBoxFaceCount] = {
			{ 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f,  1.0f }, { -1.0f, 0.0f, 0.0f },
			{ 1.0f, 0.0f,  0.0f }, { 0.0f, 1.0f,  0.0f }, {  0.0f, -1.0f, 0.0f }
		};

		inline constexpr uint32_t kQuadTriIndices[kQuadIndexCount]        = { 0, 1, 2, 0, 2, 3 };
		inline constexpr uint32_t kQuadReverseTriIndices[kQuadIndexCount] = { 0, 2, 1, 0, 3, 2 };
		inline constexpr uint32_t kCapBottomTriIndices[kCapTriVerts]      = { 0, 2, 1 };
		inline constexpr uint32_t kCapTopTriIndices[kCapTriVerts]         = { 0, 1, 2 };
		inline constexpr bool kQuadUseMaxU[kBoxFaceVerts] = { false, true, true, false };
		inline constexpr bool kQuadUseMaxV[kBoxFaceVerts] = { false, false, true, true };

		// 共通UV領域 (Common UV Regions)
		inline constexpr Vector2 kUvOliveMin = { 0.25f, 0.05f }, kUvOliveMax = { 0.35f, 0.15f };
		inline constexpr Vector2 kUvMetalMin = { 0.65f, 0.05f }, kUvMetalMax = { 0.75f, 0.15f };
		inline constexpr Vector2 kUvGlassMin = { 0.10f, 0.75f }, kUvGlassMax = { 0.25f, 0.85f };
		inline constexpr Vector2 kUvWarnMin = { 0.85f, 0.05f }, kUvWarnMax = { 0.95f, 0.15f };

		// M18シグナル発煙筒メッシュ
		inline constexpr Vector2 kUvFlareMetal = { 0.15f, 0.15f }, kUvFlareGreen = { 0.45f, 0.45f }, kUvFlareStripe = { 0.85f, 0.35f };
		inline constexpr int kFlareCylinderSegs = 12;
		inline constexpr float kFlareCylinderRadius = 0.042f, kFlareCylinderTopY = 0.155f, kFlareBoundingRadius = 0.5f;

		inline constexpr CylinderBandSpec kFlareCylinderBands[] = {
			{ 0.000f, 0.015f, 1.05f, kUvFlareMetal  },
			{ 0.015f, 0.075f, 1.00f, kUvFlareGreen  },
			{ 0.075f, 0.115f, 1.02f, kUvFlareStripe },
			{ 0.115f, 0.145f, 1.00f, kUvFlareGreen  },
			{ 0.145f, 0.155f, 1.05f, kUvFlareMetal  },
		};

		inline constexpr ProceduralBoxSpec kFlareCanisterBoxes[] = {
			{ {  0.000f, 0.168f, 0.0f }, { 0.024f, 0.026f, 0.024f }, kUvFlareMetal, kUvFlareMetal },
			{ {  0.026f, 0.115f, 0.0f }, { 0.006f, 0.095f, 0.016f }, kUvFlareMetal, kUvFlareMetal },
			{ {  0.016f, 0.168f, 0.0f }, { 0.022f, 0.008f, 0.016f }, kUvFlareMetal, kUvFlareMetal },
			{ { -0.018f, 0.170f, 0.0f }, { 0.014f, 0.014f, 0.004f }, kUvFlareMetal, kUvFlareMetal },
		};

		// 回収ヘリ スライドカーゴドア
		inline constexpr float kHeliDoorBoundingRadius = 4.0f;
		inline constexpr ProceduralBoxSpec kHeliDoorBoxes[] = {
			{ { 0.000f,  0.00f, 0.00f }, { 0.045f, 2.120f, 2.72f }, kUvOliveMin, kUvOliveMax },
			{ { 0.014f,  1.08f, 0.00f }, { 0.068f, 0.060f, 2.76f }, kUvMetalMin, kUvMetalMax },
			{ { 0.014f, -1.08f, 0.00f }, { 0.068f, 0.060f, 2.76f }, kUvMetalMin, kUvMetalMax },
			{ { 0.026f, -0.45f, 0.00f }, { 0.018f, 0.045f, 2.62f }, kUvOliveMin, kUvOliveMax },
			{ { 0.026f,  0.80f, 0.00f }, { 0.018f, 0.045f, 2.62f }, kUvOliveMin, kUvOliveMax },
			{ { 0.025f,  0.35f, 0.45f }, { 0.014f, 0.650f, 0.75f }, kUvGlassMin, kUvGlassMax },
			{ { 0.030f,  0.70f, 0.45f }, { 0.022f, 0.050f, 0.85f }, kUvMetalMin, kUvMetalMax },
			{ { 0.030f,  0.00f, 0.45f }, { 0.022f, 0.050f, 0.85f }, kUvMetalMin, kUvMetalMax },
			{ { 0.030f,  0.35f, 0.05f }, { 0.022f, 0.650f, 0.05f }, kUvMetalMin, kUvMetalMax },
			{ { 0.030f,  0.35f, 0.85f }, { 0.022f, 0.650f, 0.05f }, kUvMetalMin, kUvMetalMax },
			{ { 0.030f, -0.05f, 1.18f }, { 0.022f, 0.340f, 0.07f }, kUvMetalMin, kUvMetalMax },
			{ { 0.044f, -0.05f, 1.18f }, { 0.020f, 0.260f, 0.03f }, kUvWarnMin,  kUvWarnMax  },
		};

		// 回収ヘリ キャビン内装フレーム・ベンチシート (Cabin Interior)
		inline constexpr float kCabinInteriorBoundingRadius = 5.0f;
		inline constexpr ProceduralBoxSpec kCabinInteriorBoxes[] = {
			{ {  0.18f, 0.095f,  0.75f }, { 1.650f, 0.025f, 2.400f }, kUvMetalMin, kUvMetalMax },
			{ { -0.46f, 0.440f,  0.72f }, { 0.380f, 0.055f, 2.200f }, kUvOliveMin, kUvOliveMax },
			{ { -0.66f, 0.940f,  0.72f }, { 0.045f, 0.480f, 2.200f }, kUvOliveMin, kUvOliveMax },
			{ { -0.64f, 1.060f, -0.25f }, { 0.045f, 1.920f, 0.045f }, kUvMetalMin, kUvMetalMax },
			{ { -0.64f, 1.060f,  0.72f }, { 0.045f, 1.920f, 0.045f }, kUvMetalMin, kUvMetalMax },
			{ { -0.64f, 1.060f,  1.68f }, { 0.045f, 1.920f, 0.045f }, kUvMetalMin, kUvMetalMax },
			{ {  0.22f, 2.040f, -0.15f }, { 1.750f, 0.065f, 0.075f }, kUvMetalMin, kUvMetalMax },
			{ {  0.22f, 2.040f,  0.75f }, { 1.750f, 0.065f, 0.075f }, kUvMetalMin, kUvMetalMax },
			{ {  0.22f, 2.040f,  1.65f }, { 1.750f, 0.065f, 0.075f }, kUvMetalMin, kUvMetalMax },
			{ {  1.12f, 1.980f,  0.75f }, { 0.035f, 0.035f, 2.350f }, kUvMetalMin, kUvMetalMax },
			{ { -0.22f, 0.280f, -0.08f }, { 0.440f, 0.340f, 0.560f }, kUvOliveMin, kUvOliveMax },
			{ { -0.22f, 0.460f, -0.08f }, { 0.460f, 0.030f, 0.580f }, kUvMetalMin, kUvMetalMax },
		};

		// 携行アサルトライフル
		inline constexpr float kRifleBoundingRadius = 2.0f;
		inline constexpr ProceduralBoxSpec kRifleBoxes[] = {
			{ { 0.0f,  0.010f,  0.00f }, { 0.075f, 0.100f, 0.400f }, kUvMetalMin, kUvMetalMax },
			{ { 0.0f,  0.012f,  0.31f }, { 0.062f, 0.080f, 0.240f }, kUvOliveMin, kUvOliveMax },
			{ { 0.0f,  0.024f,  0.50f }, { 0.034f, 0.034f, 0.320f }, kUvMetalMin, kUvMetalMax },
			{ { 0.0f,  0.024f,  0.69f }, { 0.048f, 0.048f, 0.100f }, kUvMetalMin, kUvMetalMax },
			{ { 0.0f, -0.130f,  0.07f }, { 0.046f, 0.185f, 0.095f }, kUvOliveMin, kUvOliveMax },
			{ { 0.0f, -0.095f, -0.12f }, { 0.044f, 0.130f, 0.068f }, kUvMetalMin, kUvMetalMax },
			{ { 0.0f, -0.018f, -0.34f }, { 0.062f, 0.120f, 0.280f }, kUvOliveMin, kUvOliveMax },
			{ { 0.0f,  0.092f,  0.01f }, { 0.048f, 0.060f, 0.160f }, kUvMetalMin, kUvMetalMax },
		};
	}

	// --- 3層山岳リング・渓谷・蛇行河川・森林・空撮拠点の生成パラメータ ---
	namespace Terrain
	{
		// 360度連峰リング波形ハーモニクス
		inline constexpr float kMtnFreq1 = 5.0f, kMtnFreq2 = 11.0f, kMtnPhaseMul2 = 1.7f, kMtnFreq3 = 23.0f;
		inline constexpr float kMtnPhaseMul3 = 2.3f, kMtnFreq4 = 41.0f, kMtnPhaseMul4 = 3.1f, kMtnFreq5 = 3.0f, kMtnPhaseMul5 = 0.6f;

		inline constexpr float kMtnWeight1 = 0.28f, kMtnWeight2 = 0.32f, kMtnWeight3 = 0.18f, kMtnWeight4 = 0.10f, kMtnWeight5 = 0.12f;

		inline constexpr float kMtnInnerRadiusRatio = 0.90f, kMtnOuterRadiusRatio = 1.10f, kMtnSlopeNormalXZ = 0.42f;
		inline constexpr float kMtnSlopeNormalY = 0.90f, kMtnBoundingMargin = 120.0f, kGroundMtnCenterY = -2.0f;

		struct MountainLayerSpec
		{
			float   radius;
			float   minPeakH;
			float   maxPeakH;
			float   bottomY;
			int     segments;
			float   phaseSeed;
			Vector4 color;
			bool    enableLighting;
		};

		inline constexpr MountainLayerSpec kMountainFarSpec  = { 710.0f, 56.0f, 112.0f, -18.0f, 180, 0.45f, { 0.45f, 0.54f, 0.66f, 1.0f }, false };
		inline constexpr MountainLayerSpec kMountainMidSpec  = { 560.0f, 38.0f,  78.0f, -16.0f, 180, 2.15f, { 0.27f, 0.36f, 0.46f, 1.0f }, false };
		inline constexpr MountainLayerSpec kMountainNearSpec = { 430.0f, 22.0f,  48.0f, -14.0f, 180, 4.10f, { 0.19f, 0.27f, 0.23f, 1.0f }, true  };

		// 上空巡航渓谷タイル・蛇行河川ハイトマップ (Aerial Valley & River)
		inline constexpr float kAerialTileLen = 110.0f, kAerialTileWidth = 260.0f, kValleyBaseY = 14.0f, kCruiseFlightYaw = 2.54f;
		inline constexpr int kValleyGridNX = 72, kValleyGridNZ = 48;

		inline constexpr float kRiverMeanderAmp1 = 14.5f, kRiverMeanderFreq2 = 2.0f, kRiverMeanderPhase2 = 0.65f, kRiverMeanderAmp2 = 5.2f;

		inline constexpr float kRiverBedHalfWidth = 7.8f, kRiverBedDepth = -1.15f, kRiverBankBlendWidth = 6.4f;
		inline constexpr float kTerraceStartDist = 14.2f, kTerraceSpan = 65.0f, kTerraceRise = 1.45f;
		inline constexpr float kOuterSlopeStartDist = 68.0f, kOuterSlopeSpan = 65.0f, kOuterSlopeRise = 3.6f;
		inline constexpr float kGentleWaveFreq1 = 0.032f, kGentleWaveAmp1 = 0.16f, kGentleWaveFreq2 = 0.045f, kGentleWaveOuterAmp = 3.8f;

		inline constexpr float kValleyNormalEps = 1.2f, kValleyUvScale = 0.085f, kValleyBoundingRadius = 260.0f;

		inline constexpr size_t kMtnVertsPerSeg = 8, kMtnIndicesPerSeg = 18;
		inline constexpr int kRiverRibbonSegs = 48, kRiverVertsPerRow = 2;
		inline constexpr float kRiverHalfWidth = 11.6f, kRiverSurfaceY = -0.36f, kRiverUvRepeatV = 6.0f, kRiverBoundingRadius = 220.0f;

		// 渓谷スクロールベルトとタイル色調
		inline constexpr Vector3 kAerialBeltAnchor = { -4.0f, kValleyBaseY, 112.0f };
		inline constexpr float kAerialInitialScroll = 38.0f, kAerialScrollSpeed = 22.0f, kResultPrewarmDt = 0.05f;
		inline constexpr int kResultPrewarmSteps = 96;

		inline constexpr Vector4 kValleyTileTint = { 0.62f, 0.71f, 0.61f, 1.0f }, kRiverTileTint = { 0.78f, 0.89f, 0.98f, 1.0f };
		inline constexpr Vector4 kForestTileTint = { 0.74f, 0.83f, 0.78f, 1.0f }, kOutpostTileTint = { 0.78f, 0.82f, 0.80f, 1.0f };

		// 渓谷バッチ針葉樹林の接地配置
		inline constexpr int kAerialTreeCount = 78, kAlternateBankDivisor = 2;
		inline constexpr Vector2 kTreeFootSampleDirs[] = {
			{ 0.0f, 0.0f }, { 1.0f, 0.0f }, { -1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, -1.0f }
		};
		inline constexpr float kTreeZHalfSpanRatio = 0.47f, kTreeZFullSpanRatio = 0.94f, kTreeJitterFreq = 1.73f;
		inline constexpr float kTreeJitterAmp = 4.5f, kTreeBankDistBase = 15.5f, kTreeBankDistStep = 14.3f;
		inline constexpr float kTreeBankDistMod = 82.0f, kTreeBankDistMin = 15.0f, kTreeBankDistMax = 102.0f;
		inline constexpr float kOutpostClearingHalfExtent = 11.5f, kTreeScaleBase = 0.68f, kTreeScaleStep = 0.21f;
		inline constexpr float kTreeScaleMod = 0.38f, kTreeScaleYMul = 1.15f, kTreeFootRadiusMul = 1.65f;
		inline constexpr float kTreeMaxSlopeDeltaY = 0.32f, kTreeTrunkLiftRatio = 0.19f, kTreeYawStep = 2.13f;
		inline constexpr float kForestBoundingRadius = 260.0f, kOutpostBoundingRadius = 260.0f;

		struct AerialOutpostSiteSpec { float localZ; float riverOffsetX; float yawBias; };

		inline constexpr AerialOutpostSiteSpec kAerialOutpostSites[] = {
			{ -28.0f,  21.0f, 0.00f },
			{  27.0f, -19.0f, 0.85f },
		};
	}

	// --- 地上LZ前哨拠点（FOB）小道具 ＆ 針葉樹林配置テーブル ---
	namespace Props
	{
		enum class LzPropKind : uint8_t
		{
			Pine      = 0,
			Sandbag   = 1,
			Barrel    = 2,
			Crate     = 3,
			Container = 4,
			Tower     = 5,
			Fence     = 6,
			Signpost  = 7,
			Count     = 8
		};

		struct LzPropAssetSpec { const char* objFile; const char* texPath; float boundingRadius; };

		inline constexpr LzPropAssetSpec kLzPropAssets[] = {
			{ "duckov_pine_tree.obj",    "Resources/duckov_pine_tree.png",   35.0f }, // Pine
			{ "duckov_sandbag_wall.obj", "Resources/duckov_sandbag.png",     25.0f }, // Sandbag
			{ "duckov_barrel_stack.obj", "Resources/duckov_barrel.png",      25.0f }, // Barrel
			{ "duckov_crate.obj",        "Resources/duckov_crate.png",       25.0f }, // Crate
			{ "container.obj",           "Resources/container_military.png", 35.0f }, // Container
			{ "watchtower.obj",          "Resources/duckov_crate.png",       45.0f }, // Tower
			{ "duckov_fence.obj",        "Resources/duckov_fence.png",       25.0f }, // Fence
			{ "signpost.obj",            "Resources/signpost.png",           20.0f }, // Signpost
		};

		inline constexpr float kGroundBoundingRadius = 240.0f;
		inline constexpr Vector3 kGroundTranslate = { 0.0f, -0.02f, 0.0f }, kGroundScale = { 95.0f, 1.0f, 95.0f };
		inline constexpr float kHelipadBoundingRadius = 15.0f;
		inline constexpr Vector3 kHelipadScale = { 8.0f, 8.0f, 1.0f };
		inline constexpr float kHelipadSurfaceY = 0.02f;

		struct LzPropPlacementSpec { LzPropKind kind; Vector3 pos; Vector3 rot; Vector3 scale; Vector4 tint; };

		inline constexpr LzPropPlacementSpec kLzFobProps[] = {
			// 監視塔・2段積み軍用コンテナ (Towers & Containers)
			{ LzPropKind::Tower,     { -10.8f, 0.00f, 13.8f }, { 0.0f,  0.32f, 0.0f }, { 1.15f, 1.15f, 1.15f }, Common::kWhiteColor },
			{ LzPropKind::Tower,     {  13.6f, 0.00f, 16.5f }, { 0.0f, -0.48f, 0.0f }, { 1.10f, 1.15f, 1.10f }, Common::kWhiteColor },
			{ LzPropKind::Container, {  -7.4f, 0.00f,  7.6f }, { 0.0f,  0.36f, 0.0f }, { 1.05f, 1.05f, 1.05f }, Common::kWhiteColor },
			{ LzPropKind::Container, {  -7.5f, 2.30f,  7.7f }, { 0.0f,  0.42f, 0.0f }, { 1.05f, 1.05f, 1.05f }, { 0.86f, 0.88f, 0.84f, 1.0f } },
			{ LzPropKind::Container, { -12.2f, 0.00f,  9.8f }, { 0.0f,  1.15f, 0.0f }, { 1.05f, 1.05f, 1.05f }, Common::kWhiteColor },
			{ LzPropKind::Container, {  10.4f, 0.00f,  9.2f }, { 0.0f, -0.52f, 0.0f }, { 1.05f, 1.05f, 1.05f }, Common::kWhiteColor },
			// 外周防護フェンス
			{ LzPropKind::Fence,     { -13.5f, 0.00f, 13.0f }, { 0.0f,  0.45f, 0.0f }, { 1.05f, 1.05f, 1.05f }, Common::kWhiteColor },
			{ LzPropKind::Fence,     {  -8.4f, 0.00f, 16.2f }, { 0.0f,  0.16f, 0.0f }, { 1.05f, 1.05f, 1.05f }, Common::kWhiteColor },
			{ LzPropKind::Fence,     {  -3.6f, 0.00f, 17.0f }, { 0.0f,  0.04f, 0.0f }, { 1.05f, 1.05f, 1.05f }, Common::kWhiteColor },
			{ LzPropKind::Fence,     {   1.4f, 0.00f, 17.2f }, { 0.0f, -0.05f, 0.0f }, { 1.05f, 1.05f, 1.05f }, Common::kWhiteColor },
			{ LzPropKind::Fence,     {   6.4f, 0.00f, 16.8f }, { 0.0f, -0.18f, 0.0f }, { 1.05f, 1.05f, 1.05f }, Common::kWhiteColor },
			{ LzPropKind::Fence,     {  11.4f, 0.00f, 15.2f }, { 0.0f, -0.42f, 0.0f }, { 1.05f, 1.05f, 1.05f }, Common::kWhiteColor },
			// 土嚢掩体・ドラム缶集積・物資木箱・標識 (Sandbags, Barrels, Crates & Sign)
			{ LzPropKind::Sandbag,   {  -4.3f, 0.00f,  4.4f }, { 0.0f,  0.24f, 0.0f }, { 1.05f, 1.05f, 1.05f }, Common::kWhiteColor },
			{ LzPropKind::Sandbag,   {   6.9f, 0.00f,  5.2f }, { 0.0f, -0.42f, 0.0f }, { 1.05f, 1.05f, 1.05f }, Common::kWhiteColor },
			{ LzPropKind::Sandbag,   {  -1.8f, 0.00f, 12.2f }, { 0.0f,  0.08f, 0.0f }, { 1.10f, 1.10f, 1.10f }, Common::kWhiteColor },
			{ LzPropKind::Barrel,    {  -5.3f, 0.00f,  3.6f }, { 0.0f,  0.40f, 0.0f }, { 1.00f, 1.00f, 1.00f }, Common::kWhiteColor },
			{ LzPropKind::Barrel,    {   8.4f, 0.00f,  7.0f }, { 0.0f, -0.75f, 0.0f }, { 1.05f, 1.05f, 1.05f }, Common::kWhiteColor },
			{ LzPropKind::Barrel,    {  -8.8f, 0.00f, 11.4f }, { 0.0f,  1.05f, 0.0f }, { 1.05f, 1.05f, 1.05f }, Common::kWhiteColor },
			{ LzPropKind::Crate,     {   6.8f, 0.00f,  3.8f }, { 0.0f, -0.30f, 0.0f }, { 1.00f, 1.00f, 1.00f }, Common::kWhiteColor },
			{ LzPropKind::Crate,     {  -3.4f, 0.00f, 11.6f }, { 0.0f,  0.52f, 0.0f }, { 1.10f, 1.10f, 1.10f }, Common::kWhiteColor },
			{ LzPropKind::Crate,     {   4.2f, 0.00f, 13.0f }, { 0.0f, -0.22f, 0.0f }, { 1.15f, 1.15f, 1.15f }, Common::kWhiteColor },
			{ LzPropKind::Signpost,  {  -3.9f, 0.00f,  2.2f }, { 0.0f,  0.48f, 0.0f }, { 0.90f, 0.90f, 0.90f }, Common::kWhiteColor },
		};

		struct LzTreePlacementSpec { Vector3 pos; float yaw; Vector3 scale; Vector4 tint; };

		inline constexpr float kLzTreeGroundLiftRatio = 0.18f;

		inline constexpr LzTreePlacementSpec kLzTrees[] = {
			{ {  -8.8f, 0.0f,  8.6f },  0.50f, { 1.25f, 1.45f, 1.25f }, { 0.96f, 0.98f, 0.95f, 1.0f } },
			{ {   9.4f, 0.0f,  7.4f }, -0.80f, { 1.30f, 1.52f, 1.30f }, { 0.96f, 0.98f, 0.95f, 1.0f } },
			{ {  -7.2f, 0.0f, -4.0f },  1.20f, { 1.10f, 1.25f, 1.10f }, { 0.95f, 0.97f, 0.94f, 1.0f } },
			{ { -14.8f, 0.0f, 10.5f },  1.85f, { 1.45f, 1.70f, 1.45f }, { 0.90f, 0.94f, 0.90f, 1.0f } },
			{ { -12.6f, 0.0f, 18.8f },  0.25f, { 1.60f, 1.90f, 1.60f }, { 0.84f, 0.90f, 0.88f, 1.0f } },
			{ {  -7.8f, 0.0f, 20.5f },  2.10f, { 1.55f, 1.85f, 1.55f }, { 0.84f, 0.90f, 0.88f, 1.0f } },
			{ {  -3.2f, 0.0f, 21.8f }, -1.10f, { 1.70f, 2.05f, 1.70f }, { 0.82f, 0.88f, 0.87f, 1.0f } },
			{ {   1.6f, 0.0f, 22.5f },  0.75f, { 1.65f, 1.95f, 1.65f }, { 0.82f, 0.88f, 0.87f, 1.0f } },
			{ {   6.2f, 0.0f, 21.2f }, -0.45f, { 1.55f, 1.85f, 1.55f }, { 0.84f, 0.90f, 0.88f, 1.0f } },
			{ {  11.0f, 0.0f, 19.6f },  1.45f, { 1.65f, 1.98f, 1.65f }, { 0.84f, 0.90f, 0.88f, 1.0f } },
			{ {  15.8f, 0.0f, 13.5f }, -1.65f, { 1.45f, 1.72f, 1.45f }, { 0.90f, 0.94f, 0.90f, 1.0f } },
			{ { -19.5f, 0.0f, 26.0f },  0.95f, { 1.90f, 2.25f, 1.90f }, { 0.74f, 0.83f, 0.84f, 1.0f } },
			{ { -10.5f, 0.0f, 29.5f }, -2.05f, { 2.05f, 2.40f, 2.05f }, { 0.72f, 0.82f, 0.84f, 1.0f } },
			{ {  -1.2f, 0.0f, 31.0f },  0.35f, { 2.15f, 2.50f, 2.15f }, { 0.70f, 0.80f, 0.83f, 1.0f } },
			{ {   7.8f, 0.0f, 30.2f },  1.75f, { 2.00f, 2.35f, 2.00f }, { 0.72f, 0.82f, 0.84f, 1.0f } },
			{ {  17.5f, 0.0f, 27.5f }, -0.85f, { 1.95f, 2.30f, 1.95f }, { 0.74f, 0.83f, 0.84f, 1.0f } },
			{ {  23.0f, 0.0f, 20.5f },  2.40f, { 1.80f, 2.10f, 1.80f }, { 0.78f, 0.86f, 0.85f, 1.0f } },
			{ { -22.5f, 0.0f, 17.5f }, -1.35f, { 1.75f, 2.05f, 1.75f }, { 0.78f, 0.86f, 0.85f, 1.0f } },
		};

		// 空撮渓谷の前哨拠点クラスタ構成ピース
		struct OutpostClusterPieceSpec { LzPropKind kind; float dx; float dz; float yaw; float scale; };

		inline constexpr OutpostClusterPieceSpec kAerialOutpostPieces[] = {
			{ LzPropKind::Tower,      0.0f,  0.0f,  0.35f, 1.15f },
			{ LzPropKind::Container,  4.4f, -2.4f, -0.45f, 1.20f },
			{ LzPropKind::Container, -3.8f,  3.0f,  1.10f, 1.20f },
			{ LzPropKind::Crate,      2.0f,  2.3f,  0.20f, 1.25f },
		};

		inline constexpr float kLzPrewarmDt = 0.04f;
		inline constexpr int kLzPrewarmSteps = 75;
	}

	// --- ヘリコプター・護衛編隊・オペレーター・シネマティックカメラ・照明キーフレーム ---
	namespace Choreography
	{
		// 主役機 Dustoff-1 構造・アタッチメント (Hero Helicopter Structure)
		inline constexpr float kHeliBoundingRadius = 80.0f, kHeliScale = 0.65f;
		inline constexpr Vector4 kCabinInteriorTint = { 0.56f, 0.60f, 0.58f, 1.0f };
		inline constexpr Vector3 kMainRotorLocalOffset = { 0.0f, 4.15f, 0.75f }, kTailRotorLocalOffset = { 0.20f, 3.35f, -8.60f };
		inline constexpr float kRotorBlurOffsetFast = 0.095f, kRotorBlurOffsetGround = 0.055f, kRotorBlurSecondMul = 2.0f;

		inline constexpr float kCargoDoorBaseX = 1.470f, kCargoDoorSlideDeltaX = 0.015f, kCargoDoorLocalY = 1.14f;
		inline constexpr float kCargoDoorOpenZ = -1.90f, kCargoDoorClosedZ = 0.75f;
		inline constexpr Vector3 kTurbineNozzleLocal = { 0.40f, 3.22f, -1.25f };

		// ステート別フライトキーフレーム
		inline constexpr float kMainRotorSpeedBase = 50.0f, kTailRotorSpeedBase = 68.0f;

		inline constexpr Vector3 kHeliInboundStartPos = { 20.5f, 15.2f, 18.0f };
		inline constexpr Vector3 kHeliInboundStartRot = { 0.08f, 3.00f, 0.22f }, kHeliLandPos = { 2.8f, 0.435f, 2.0f };
		inline constexpr float kHeliLandYaw = 2.65f, kInboundFlareStartT = 0.25f, kInboundFlareSpanT = 0.75f;
		inline constexpr float kInboundFlarePitch = -0.18f, kInboundBankPitch = 0.08f, kInboundBankYaw = 0.35f;
		inline constexpr float kInboundBankRoll = 0.22f, kInboundDoorStartT = 0.72f, kInboundDoorSpanT = 0.28f;
		inline constexpr float kInboundDoorOpenAmount = 0.92f, kMainRotorSpeedInbound = 54.0f, kTailRotorSpeedInbound = 72.0f;

		inline constexpr float kIdleSwayFreq = 3.5f, kIdleSwayAmpZ = 0.008f, kRunDoorMinSlide = 0.04f;
		inline constexpr float kRunDoorOpenSpeed = 2.5f, kMainRotorSpeedRun = 48.0f, kTailRotorSpeedRun = 64.0f;

		inline constexpr float kBoardSettleAmpY = -0.025f, kBoardSettleRollMul = 0.4f, kBoardDoorSlide = 0.06f;
		inline constexpr float kMainRotorSpeedBoard = 52.0f, kTailRotorSpeedBoard = 70.0f;

		inline constexpr float kTakeoffSpoolSpanT = 0.28f, kTakeoffMainSpoolGain = 18.0f, kTakeoffTailSpoolGain = 22.0f;
		inline constexpr float kTakeoffDoorSlide = 0.08f, kTakeoffLiftStartT = 0.22f, kTakeoffLiftSpanT = 0.78f;
		inline constexpr float kTakeoffVibFreq = 46.0f, kTakeoffVibAmp = 0.012f, kTakeoffHoverClimbY = 0.18f;
		inline constexpr float kTakeoffHoverPitch = -0.02f, kTakeoffCurveExp = 1.25f, kTakeoffClimbStartY = 0.615f;
		inline constexpr float kTakeoffDriftX = 6.2f, kTakeoffClimbY = 13.5f, kTakeoffDriftZ = 4.4f;
		inline constexpr float kTakeoffNosePitch = 0.11f, kTakeoffPitchSinMul = 0.85f, kTakeoffYawDelta = 0.11f;
		inline constexpr float kTakeoffBankRoll = -0.14f, kTakeoffRollSinMul = 0.80f;

		inline constexpr Vector3 kCruiseBasePos = { 2.48f, 42.30f, 1.70f };
		inline constexpr float kCruiseBasePitch = 0.065f, kCruiseBaseRoll = -0.085f, kCruiseCutInDuration = 0.65f;
		inline constexpr Vector3 kCruiseCutInOffset = { 0.85f, -0.95f, 1.15f };
		inline constexpr float kCruiseBobFreqX = 1.15f, kCruiseBobAmpX = 0.09f, kCruiseBobFreqY1 = 1.65f;
		inline constexpr float kCruiseBobAmpY1 = 0.14f, kCruiseBobFreqY2 = 0.75f, kCruiseBobAmpY2 = 0.06f;
		inline constexpr float kCruiseBobFreqZ = 0.95f, kCruiseBobAmpZ = 0.10f, kCruisePitchFreq = 1.35f;
		inline constexpr float kCruisePitchAmp = 0.018f, kCruiseRollFreq = 1.05f, kCruiseRollAmp = 0.024f;
		inline constexpr float kCruiseDoorSlide = 0.08f, kMainRotorSpeedCruise = 68.0f, kTailRotorSpeedCruise = 88.0f;

		// 護衛編隊ヘリ (Dustoff-2 / Dustoff-3)
		struct EscortHeliSpec { float scale; Vector4 tint; Vector3 basePos; Vector3 prewarmRot; };

		inline constexpr EscortHeliSpec kEscortSpecs[] = {
			{ 0.48f, { 0.84f, 0.88f, 0.90f, 1.0f }, {  0.55f, 38.80f, 28.50f }, { 0.075f, Terrain::kCruiseFlightYaw, -0.105f } },
			{ 0.30f, { 0.72f, 0.79f, 0.86f, 1.0f }, { -4.80f, 44.60f, 52.00f }, { 0.055f, Terrain::kCruiseFlightYaw, -0.070f } },
		};

		inline constexpr float kEscortPhaseStep = 1.9f, kEscortTimeRateBase = 1.05f, kEscortTimeRateDecay = 0.18f;
		inline constexpr float kEscortSwayFreqX = 0.85f, kEscortSwayAmpX = 0.32f, kEscortSwayFreqY = 1.25f;
		inline constexpr float kEscortSwayAmpY = 0.28f, kEscortSwayFreqZ = 0.70f, kEscortSwayAmpZ = 0.30f;
		inline constexpr float kEscortBasePitch = 0.072f, kEscortPitchFreq = 1.1f, kEscortPitchAmp = 0.020f;
		inline constexpr float kEscortYawFreq = 0.6f, kEscortYawAmp = 0.025f, kEscortBaseRoll = -0.105f;
		inline constexpr float kEscortRollFreq = 0.95f, kEscortRollAmp = 0.032f, kEscortRotorSpeedMul = 1.04f, kEscortRotorBlurOffset = 0.10f;

		// プレイヤーアヒル ＆ ライフル姿勢
		inline constexpr float kDuckBoundingRadius = 10.0f, kDuckScale = 0.38f, kRifleScale = 0.48f;
		inline constexpr Vector4 kRifleTint = { 0.72f, 0.75f, 0.78f, 1.0f };

		inline constexpr Vector3 kDoorGroundSpot = { 1.58f, 0.0f, 1.38f }, kCabinHeroLocal = { 0.80f, 0.105f, 0.75f };

		inline constexpr float kDuckCrouchFreq = 4.5f, kDuckCrouchAmp = 0.015f, kDuckIdlePitch = 0.05f;
		inline constexpr float kDuckIdleYaw = 1.35f, kDuckIdleSwayFreq = 3.2f, kDuckIdleSwayAmp = 0.03f;
		inline constexpr Vector3 kRifleOffInbound = { 0.84f, 0.56f, 0.16f }, kRifleRotInbound = { 0.22f, -0.06f, 0.08f };

		inline constexpr float kRunPhaseSpeed = 15.0f, kRunHopAmp = 0.11f, kRunWaddleAmp = 0.14f, kRunLeanPitch = 0.10f;
		inline constexpr Vector3 kRifleOffRun = { 0.84f, 0.56f, 0.16f }, kRifleRotRun = { 0.28f, -0.08f, 0.10f };

		inline constexpr float kBoardJumpSpanT = 0.72f, kBoardTurnStartT = 0.35f, kBoardTurnSpanT = 0.65f;
		inline constexpr float kBoardArcHeight = 0.42f, kCabinHeroYawBias = -0.16f;
		inline constexpr Vector3 kRifleOffBoard = { 0.84f, 0.56f, 0.14f }, kRifleRotBoard = { 0.36f, -0.08f, 0.12f };

		inline constexpr float kHeroBreathFreq = 2.8f, kHeroBreathAmp = 0.010f, kHeroSurveyFreq = 0.85f;
		inline constexpr float kHeroSurveyAmp = 0.055f, kHeroTiltFollow = 0.65f;
		inline constexpr Vector3 kRifleOffHero = { 0.84f, 0.55f, 0.12f }, kRifleRotHero = { 0.42f, -0.10f, 0.12f };

		// シネマティックカメラ
		inline constexpr float kCameraFarZ = 1600.0f;
		inline constexpr Vector3 kCameraInitPos = { -0.5f, 1.8f, -6.8f }, kCameraInitRot = { 0.08f, 0.10f, 0.0f };
		inline constexpr float kCameraDefaultFov = 0.55f;

		struct CameraCutKeyframe { Vector3 posStart; Vector3 posEnd; Vector3 rotStart; Vector3 rotEnd; };

		inline constexpr CameraCutKeyframe kCamKeyInbound = { { -1.60f, 1.45f, -7.60f }, { -0.45f, 1.72f, -6.75f },
			{ -0.12f, 0.34f, 0.00f }, { 0.06f, 0.11f, 0.00f } };
		inline constexpr CameraCutKeyframe kCamKeyRun = { { -0.45f, 1.72f, -6.75f }, { 0.35f, 1.58f, -5.35f },
			{ 0.06f, 0.11f, 0.00f }, { 0.03f, 0.18f, 0.00f } };
		inline constexpr CameraCutKeyframe kCamKeyBoard = { { 0.35f, 1.58f, -5.35f }, { 0.55f, 1.68f, -4.85f },
			{ 0.03f, 0.18f, 0.00f }, { -0.02f, 0.22f, 0.00f } };
		inline constexpr CameraCutKeyframe kCamKeyTakeoff = { { 0.55f, 1.68f, -4.85f }, { 0.10f, 2.85f, -6.10f },
			{ -0.02f, 0.22f, 0.00f }, { -0.34f, 0.30f, -0.03f } };

		inline constexpr float kCamTakeoffLookStartT = 0.12f, kCamTakeoffLookSpanT = 0.88f, kCamTakeoffShakeDecay = 0.70f;
		inline constexpr float kCamTakeoffShakeAmp = 0.018f, kCamTakeoffShakeFreqX = 50.0f, kCamTakeoffShakeFreqY = 62.0f;
		inline constexpr float kCamTakeoffShakeYRatio = 0.8f;

		inline constexpr Vector3 kCamResultBasePos = { -0.15f, 43.52f, -6.45f }, kCamResultCutOffset = { -0.35f, 0.28f, -0.55f };
		inline constexpr float kCamResultSwayFreqX = 0.72f, kCamResultSwayAmpX = 0.06f, kCamResultSwayFreqY = 0.94f;
		inline constexpr float kCamResultSwayAmpY = 0.05f;
		inline constexpr Vector3 kCamResultBaseRot = { 0.125f, 0.062f, -0.015f };
		inline constexpr float kCamResultRollFreq = 0.92f, kCamResultRollAmp = 0.008f;

		// ライティング設定 (Scene Lighting)
		inline constexpr Vector4 kInitSunColor = { 1.0f, 0.96f, 0.88f, 1.0f };
		inline constexpr Vector3 kInitSunDir = { 0.3817f, -0.6227f, 0.6830f };
		inline constexpr float kInitSunIntensity = 1.25f;

		inline constexpr Vector4 kLzSunColor = { 1.0f, 0.95f, 0.86f, 1.0f };
		inline constexpr Vector3 kLzSunDir = { 0.38f, -0.60f, 0.70f };
		inline constexpr float kLzSunIntensity = 1.25f;
		inline constexpr Vector4 kFlarePointColor = { 0.35f, 1.0f, 0.58f, 1.0f };
		inline constexpr Vector3 kFlareLightOffset = { 0.0f, 0.45f, 0.0f };
		inline constexpr float kFlarePointIntensity = 2.2f, kFlarePointRadius = 8.5f, kFlarePointDecay = 1.6f;

		inline constexpr Vector4 kCruiseSunColor = { 1.0f, 0.94f, 0.84f, 1.0f };
		inline constexpr Vector3 kCruiseSunDir = { 0.40f, -0.52f, 0.75f };
		inline constexpr float kCruiseSunIntensity = 1.28f;
		inline constexpr Vector3 kCabinRimLocalOffset = { 1.75f, 1.35f, 0.55f };
		inline constexpr Vector4 kCabinRimColor = { 1.0f, 0.84f, 0.56f, 1.0f };
		inline constexpr float kCabinRimIntensity = 2.15f, kCabinRimRadius = 6.8f, kCabinRimDecay = 1.35f;
	}

	// --- ボリューメトリック煙・砂塵・雲海パーティクル仕様 (Particle Emission Specs) ---
	namespace Particles
	{
		struct Range1D { float minVal; float maxVal; };

		struct Range3D { Vector3 minVal; Vector3 maxVal; };

		struct ParticleEmitSpec
		{
			Range3D  posJitter;
			Range1D  scale;
			Range1D  aspectX;
			Range1D  aspectY;
			Range1D  dirSpeed;
			Range3D  velJitter;
			Vector3  colorRgb;
			Range1D  alpha;
			Vector4  endColor;
			Range1D  lifeTime;
			float    drag;
			float    gravity;
			float    bounceElasticity;
			float    initScaleMul;
			Range1D  targetScale;
			bool     targetRelativeToSc;
			Range1D  curlFreq;
			Range1D  curlAmp;
			Range1D  wobbleFreq;
			Range1D  wobbleAmp;
			Vector3  wobbleAxis;
			float    fadeInRatio;
			uint16_t slotLayer;
		};

		inline constexpr uint16_t kLayerBackground = 0, kLayerMidground = 1, kLayerForeground = 2;

		inline constexpr Range1D kZero1D = { 0.0f, 0.0f }, kUnit1D = { 1.0f, 1.0f };
		inline constexpr Range3D kZero3D = { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
		inline constexpr Vector3 kUpAxis = { 0.0f, 1.0f, 0.0f };

		// エミッター発生間隔・環境定数
		inline constexpr float kFlareEmitInterval = 0.026f;
		inline constexpr Vector3 kFlareNozzleOffset = { -0.018f, 0.172f, 0.010f };
		inline constexpr int kFlareVeilStepStride = 2, kSparkChanceDivisor = 4, kTwinEngineCount = 2;
		inline constexpr int kHighCloudChanceDivisor = 2, kWingmanExhaustChanceDivisor = 2, kWispBandCount = 2;

		inline constexpr float kLzBgPlumeInterval = 0.11f;
		inline constexpr Vector3 kLzBgSmokeSites[] = {
			{ -15.5f, 2.2f, 35.0f },
			{  17.5f, 2.6f, 43.0f },
		};

		inline constexpr float kRotorWashMaxHeliY = 11.5f, kRotorWashRefHeliY = 0.42f, kRotorWashSpanY = 11.0f;
		inline constexpr float kRotorWashBaseStrength = 2.0f, kRotorWashGainStrength = 8.8f;
		inline constexpr Vector3 kRotorWashCenterOffset = { 0.0f, 1.5f, 0.0f };
		inline constexpr float kRotorWashRadius = 7.2f;

		inline constexpr float kRotorDustInterval = 0.028f, kRotorDustMinWashRatio = 0.08f;
		inline constexpr Range1D kRotorDustSpawnRadius = { 1.8f, 4.4f };
		inline constexpr float kRotorDustOutSpeedBase = 3.8f, kRotorDustOutSpeedGain = 5.4f;
		inline constexpr Range1D kRotorDustSpeedMul = { 0.82f, 1.18f }, kRotorDustUpSpeed = { 0.28f, 0.95f };

		inline constexpr float kGroundExhaustInterval = 0.038f, kGroundExhaustAftY = -0.22f;

		inline constexpr float kAerialPlumeInterval = 0.075f, kAerialOutpostMinZ = 20.0f, kAerialOutpostMaxZ = 265.0f;
		inline constexpr int kEmberChanceDivisor = 4;

		inline constexpr float kAerialCloudInterval = 0.085f, kMidCloudLayerSplitZ = 52.0f;

		inline constexpr float kCruiseExhaustInterval = 0.030f;
		inline constexpr Vector3 kWingmanExhaustOffset = { -0.32f, 1.42f, 0.52f };

		inline constexpr float kAerialWindInterval = 0.048f;
		inline constexpr Range1D kWispBandLowY = { -4.2f, -1.75f }, kWispBandHighY = { 2.35f, 4.2f };
		inline constexpr Range1D kWispForwardDist = { 5.5f, 9.5f }, kWispOffsetX = { -2.5f, 2.5f }, kWispOffsetZ = { 0.5f, 4.5f };
		inline constexpr int kSunMoteChanceDivisor = 3;
		inline constexpr Range1D kMoteForwardDist = { 4.5f, 8.5f };
		inline constexpr Range3D kMoteSpawnOffset = { { -3.0f, -2.8f, -1.5f }, { 2.5f, 2.8f, 4.5f } };

		// 1. M18発煙筒 Stage A: 高速コアジェット (Core Jet)
		inline constexpr ParticleEmitSpec kSpecFlareJet = { { { -0.015f, 0.0f, -0.015f }, { 0.015f, 0.0f, 0.015f } },
			{ 0.07f, 0.11f }, { 0.85f, 0.85f }, { 1.28f, 1.28f }, kZero1D, { { -0.52f, 2.80f, -0.08f }, { -0.12f, 3.85f, 0.34f } },
			{ 0.09f, 0.65f, 0.29f }, { 0.78f, 0.78f }, { 0.15f, 0.61f, 0.33f, 0.0f }, { 0.42f, 0.62f }, 2.85f, 0.0f, 0.0f,
			1.0f, { 0.38f, 0.52f }, false, kZero1D, kZero1D, kZero1D, kZero1D, kUpAxis, 0.12f, kLayerForeground };

		// 2. M18発煙筒 Stage B: カール乱流メイン煙柱 (Main Plume)
		inline constexpr ParticleEmitSpec kSpecFlarePlume = { { { -0.06f, 0.14f, -0.05f }, { 0.05f, 0.28f, 0.06f } },
			{ 0.28f, 0.42f }, { 0.92f, 1.14f }, { 0.92f, 1.10f }, kZero1D, { { -0.62f, 1.32f, 0.08f }, { -0.06f, 2.12f, 0.56f } },
			{ 0.14f, 0.70f, 0.35f }, { 0.56f, 0.56f }, { 0.48f, 0.70f, 0.54f, 0.0f }, { 2.25f, 3.30f }, 0.52f, 0.0f, 0.0f,
			1.0f, { 1.95f, 2.75f }, false, { 1.55f, 2.15f }, { 0.55f, 0.82f }, { 2.0f, 2.8f }, { 0.24f, 0.38f }, { 0.707f, 0.15f, 0.707f },
			0.20f, kLayerMidground };

		// 3. M18発煙筒 Stage C: 外周ベール煙 (Outer Veil)
		inline constexpr ParticleEmitSpec kSpecFlareVeil = { { { -0.14f, 0.30f, -0.12f }, { 0.12f, 0.55f, 0.14f } },
			{ 0.45f, 0.65f }, { 1.38f, 1.38f }, { 0.86f, 0.86f }, kZero1D, { { -0.85f, 0.95f, 0.10f }, { 0.10f, 1.55f, 0.75f } },
			{ 0.22f, 0.66f, 0.39f }, { 0.28f, 0.28f }, { 0.56f, 0.72f, 0.60f, 0.0f }, { 2.60f, 3.65f }, 0.60f, 0.0f, 0.0f,
			1.0f, { 2.65f, 3.35f }, false, { 1.35f, 1.35f }, { 0.65f, 0.65f }, kZero1D, kZero1D, kUpAxis, 0.26f, kLayerMidground };

		// 4. 発煙筒イグナイター火花 (Igniter Spark)
		inline constexpr ParticleEmitSpec kSpecFlareSpark = { kZero3D, { 0.015f, 0.024f }, kUnit1D, kUnit1D,
			kZero1D, { { -0.9f, 1.8f, -0.9f }, { 0.9f, 3.2f, 0.9f } },
			{ 1.0f, 0.80f, 0.36f }, { 0.65f, 0.65f }, { 1.0f, 0.45f, 0.12f, 0.0f }, { 0.28f, 0.48f }, 0.0f, 7.5f, 0.35f,
			1.0f, { 0.4f, 0.4f }, true, kZero1D, kZero1D, kZero1D, kZero1D, kUpAxis, 0.0f, kLayerForeground };

		// 5. 地上LZ遠景戦場黒煙 (LZ Background Smoke)
		inline constexpr ParticleEmitSpec kSpecLzBgSmoke = { { { -0.9f, 0.0f, -0.9f }, { 0.9f, 0.0f, 0.9f } },
			{ 1.35f, 1.95f }, { 0.95f, 1.25f }, { 0.95f, 1.15f }, kZero1D, { { -0.55f, 1.65f, -0.25f }, { 0.15f, 2.45f, 0.35f } },
			{ 0.20f, 0.22f, 0.26f }, { 0.46f, 0.46f }, { 0.46f, 0.49f, 0.55f, 0.0f }, { 4.4f, 6.0f }, 0.12f, 0.0f, 0.0f,
			1.0f, { 4.4f, 6.0f }, false, { 0.52f, 0.52f }, { 0.58f, 0.58f }, kZero1D, kZero1D, kUpAxis, 0.24f, kLayerBackground };

		// 6. ローターダウンウォッシュ砂塵リング (Rotor Wash Dust)
		inline constexpr ParticleEmitSpec kSpecRotorDust = { { { 0.0f, 0.14f, 0.0f }, { 0.0f, 0.36f, 0.0f } },
			{ 0.45f, 0.72f }, { 2.15f, 2.15f }, { 0.58f, 0.58f }, kZero1D, kZero3D,
			{ 0.63f, 0.57f, 0.47f }, { 0.36f, 0.36f }, { 0.72f, 0.68f, 0.60f, 0.0f }, { 0.75f, 1.25f }, 1.65f, 0.0f, 0.0f,
			1.0f, { 1.85f, 2.55f }, false, kZero1D, kZero1D, kZero1D, kZero1D, kUpAxis, 0.18f, kLayerForeground };

		// 7. 地上ヘリ双発タービン排気 (Ground Turbine Exhaust)
		inline constexpr ParticleEmitSpec kSpecGroundExhaust = { kZero3D, { 0.16f, 0.24f }, { 1.35f, 1.35f }, { 0.82f, 0.82f },
			{ 4.8f, 6.8f }, { { -0.5f, -0.4f, -0.5f }, { 0.5f, 0.3f, 0.5f } },
			{ 0.27f, 0.29f, 0.33f }, { 0.24f, 0.24f }, { 0.50f, 0.52f, 0.56f, 0.0f }, { 0.55f, 0.85f }, 1.25f, 0.0f, 0.0f,
			1.0f, { 0.85f, 1.25f }, false, kZero1D, kZero1D, kZero1D, kZero1D, kUpAxis, 0.16f, kLayerForeground };

		// 8. 渓谷前哨拠点の戦場スモーク柱 (Valley Outpost Plume)
		inline constexpr ParticleEmitSpec kSpecValleyPlume = { { { -1.4f, 2.2f, -1.4f }, { 1.4f, 2.2f, 1.4f } },
			{ 2.1f, 3.3f }, { 0.95f, 1.28f }, { 0.95f, 1.22f }, { 22.0f, 22.0f }, { { -0.9f, 3.15f, -0.9f }, { 0.9f, 4.65f, 0.9f } },
			{ 0.16f, 0.18f, 0.22f }, { 0.66f, 0.66f }, { 0.46f, 0.50f, 0.56f, 0.0f }, { 4.2f, 5.8f }, 0.0f, 0.0f, 0.0f,
			1.0f, { 7.4f, 11.2f }, false, { 0.44f, 0.44f }, { 0.95f, 0.95f }, kZero1D, kZero1D, kUpAxis, 0.20f, kLayerBackground };

		// 9. 渓谷前哨拠点の残り火コア (Valley Outpost Ember)
		inline constexpr ParticleEmitSpec kSpecValleyEmber = { { { -0.9f, 1.4f, -0.9f }, { 0.9f, 1.4f, 0.9f } },
			{ 1.25f, 1.95f }, { 1.15f, 1.15f }, { 1.05f, 1.05f }, { 22.0f, 22.0f }, { { 0.0f, 2.2f, 0.0f }, { 0.0f, 3.4f, 0.0f } },
			{ 0.96f, 0.48f, 0.16f }, { 0.52f, 0.52f }, { 0.20f, 0.20f, 0.24f, 0.0f }, { 0.85f, 1.35f }, 0.0f, 0.0f, 0.0f,
			1.0f, { 2.1f, 2.1f }, true, kZero1D, kZero1D, kZero1D, kZero1D, kUpAxis, 0.16f, kLayerBackground };

		// 10. 中層ボリューメトリック積雲バンク (Mid Cumulus Cloud)
		inline constexpr ParticleEmitSpec kSpecMidCloud = { { { -22.0f, 25.2f, 24.0f }, { 46.0f, 34.8f, 128.0f } },
			{ 5.4f, 8.4f }, { 2.35f, 3.05f }, { 0.58f, 0.74f }, { 13.5f, 17.5f }, kZero3D,
			{ 0.98f, 0.94f, 0.88f }, { 0.36f, 0.50f }, { 0.87f, 0.88f, 0.93f, 0.0f }, { 6.5f, 9.0f }, 0.0f, 0.0f, 0.0f,
			0.88f, { 1.30f, 1.30f }, true, kZero1D, kZero1D, kZero1D, kZero1D, kUpAxis, 0.25f, kLayerMidground };

		// 11. 連峰稜線の高高度黄金雲 (High Ridge Cloud)
		inline constexpr ParticleEmitSpec kSpecHighCloud = { { { -42.0f, 46.5f, 68.0f }, { 54.0f, 56.5f, 175.0f } },
			{ 7.2f, 11.0f }, { 3.05f, 3.05f }, { 0.48f, 0.48f }, { 9.0f, 12.0f }, kZero3D,
			{ 0.99f, 0.94f, 0.87f }, { 0.34f, 0.34f }, { 0.90f, 0.89f, 0.94f, 0.0f }, { 7.5f, 10.5f }, 0.0f, 0.0f, 0.0f,
			0.90f, { 1.22f, 1.22f }, true, kZero1D, kZero1D, kZero1D, kZero1D, kUpAxis, 0.28f, kLayerBackground };

		// 12. 上空巡航 Dustoff-1 タービン排気 (Cruise Exhaust)
		inline constexpr ParticleEmitSpec kSpecCruiseExhaust = { kZero3D, { 0.16f, 0.26f }, { 1.95f, 1.95f }, { 0.62f, 0.62f },
			{ 23.0f, 29.0f }, { { -0.6f, -0.25f, -0.6f }, { 0.6f, 0.35f, 0.6f } },
			{ 0.26f, 0.28f, 0.33f }, { 0.26f, 0.26f }, { 0.54f, 0.56f, 0.62f, 0.0f }, { 0.42f, 0.64f }, 0.0f, 0.0f, 0.0f,
			1.0f, { 0.88f, 1.35f }, false, kZero1D, kZero1D, kZero1D, kZero1D, kUpAxis, 0.16f, kLayerForeground };

		// 13. ウイングマン Dustoff-2 タービン排気 (Wingman Exhaust)
		inline constexpr ParticleEmitSpec kSpecWingmanExhaust = { { kWingmanExhaustOffset, kWingmanExhaustOffset },
			{ 0.32f, 0.48f }, { 2.05f, 2.05f }, { 0.66f, 0.66f }, { 19.0f, 23.0f }, { { 0.0f, -0.2f, 0.0f }, { 0.0f, 0.3f, 0.0f } },
			{ 0.30f, 0.33f, 0.38f }, { 0.34f, 0.34f }, { 0.56f, 0.59f, 0.65f, 0.0f }, { 0.75f, 1.05f }, 0.0f, 0.0f, 0.0f,
			1.0f, { 1.40f, 2.05f }, false, kZero1D, kZero1D, kZero1D, kZero1D, kUpAxis, 0.18f, kLayerMidground };

		// 14. 近景スリップストリーム水蒸気ウィスプ (Slipstream Wisp)
		inline constexpr ParticleEmitSpec kSpecSlipstreamWisp = { kZero3D, { 0.55f, 1.05f }, { 2.65f, 2.65f }, { 0.40f, 0.40f },
			{ 27.0f, 34.0f }, { { 0.0f, -0.4f, 0.0f }, { 0.0f, 0.2f, 0.0f } },
			{ 0.94f, 0.95f, 0.98f }, { 0.14f, 0.14f }, { 0.88f, 0.91f, 0.96f, 0.0f }, { 0.65f, 0.95f }, 0.0f, 0.0f, 0.0f,
			0.85f, { 1.45f, 1.45f }, true, kZero1D, kZero1D, kZero1D, kZero1D, kUpAxis, 0.20f, kLayerForeground };

		// 15. 陽光の微細ダスト粒子 (Sunlight Dust Mote)
		inline constexpr ParticleEmitSpec kSpecSunMote = { kZero3D, { 0.016f, 0.028f }, kUnit1D, kUnit1D,
			{ 18.0f, 24.0f }, kZero3D, { 1.0f, 0.88f, 0.64f }, { 0.20f, 0.20f }, { 1.0f, 0.82f, 0.52f, 0.0f },
			{ 0.65f, 1.05f }, 0.0f, 0.0f, 0.0f, 1.0f, { 0.5f, 0.5f }, true, kZero1D, kZero1D, kZero1D, kZero1D, kUpAxis,
			0.18f, kLayerForeground };
	}

	// --- 報酬計算 ＆ グレード査定閾値 ---
	namespace Scoring
	{
		inline constexpr int kBaseExfilBonus = 15000, kKillBonusPerEnemy = 5000, kTargetBonusPerUnit = 4000;
		inline constexpr float kFallbackMaxRaidSec = 300.0f, kTimeBonusPerSec = 80.0f;
		inline constexpr int kPercentScale = 100;
		inline constexpr float kPercentScaleF = 100.0f;
		inline constexpr int kAccuracyBonusPerPct = 60;
		inline constexpr float kHpBonusSteps = 49.0f;
		inline constexpr int kHpBonusPerStep = 100;

		inline constexpr int kKillFullThreshold = 3, kKillFullPoints = 25, kKillPointsEach = 8;

		inline constexpr int kTargetAllPoints = 25, kTargetPointsEach = 6;

		inline constexpr float kRemTimeFastSec = 150.0f, kRemTimeMidSec = 90.0f;
		inline constexpr int kRemTimeFastPoints = 20, kRemTimeMidPoints = 12, kRemTimeBasePoints = 5;

		inline constexpr int kAccHighPct = 70, kAccMidPct = 45, kAccHighPoints = 15, kAccMidPoints = 8;

		inline constexpr float kHpHighRatio = 0.70f, kHpMidRatio = 0.40f;
		inline constexpr int kHpHighPoints = 15, kHpMidPoints = 8;

		inline constexpr int kGoldDuckBonusPoints = 15;

		inline constexpr int kGradeThresholdS = 85, kGradeThresholdA = 65, kGradeThresholdB = 42, kGradeIndexS = 0;
		inline constexpr int kGradeIndexA = 1, kGradeIndexB = 2, kGradeIndexC = 3;
	}

	// --- デブリーフィングUI パレット・グリフ・レイアウト定数 ---
	namespace UI
	{
		// アトラス上の数字・英字グリフ幅定義
		struct DigitGlyphSpec { char ch; int row; int col; float advanceRatio; };

		inline constexpr float kDigitNumericAdvance = 0.348f, kDigitFallbackAdvance = 0.210f;

		inline constexpr DigitGlyphSpec kDigitGlyphTable[] = {
			{ ',', 0, 10, 0.165f },
			{ '.', 0, 11, 0.165f },
			{ '+', 0, 12, 0.345f },
			{ '-', 0, 13, 0.312f },
			{ '%', 0, 14, 0.440f },
			{ ':', 0, 15, 0.175f },
			{ '/', 1,  0, 0.255f },
			{ 'x', 1,  1, 0.330f },
			{ 'R', 1,  2, 0.422f },
			{ 'U', 1,  3, 0.424f },
			{ 'B', 1,  4, 0.418f },
			{ 'H', 1,  5, 0.436f },
			{ 'P', 1,  6, 0.402f },
			{ 'K', 1,  7, 0.414f },
			{ 'I', 1,  8, 0.205f },
			{ 'L', 1,  9, 0.372f },
			{ 'S', 1, 10, 0.398f },
			{ 'M', 1, 11, 0.495f },
			{ 'T', 1, 12, 0.320f },
			{ 'A', 1, 13, 0.418f },
			{ 'C', 1, 14, 0.402f },
			{ 'E', 1, 15, 0.388f },
		};

		inline constexpr int kAlignLeft = 0, kAlignCenter = 1, kAlignRight = 2;
		inline constexpr float kDigitCellAspect = 0.5f, kDigitShadowAlphaMul = 0.32f, kDigitShadowOffsetY = 1.2f;
		inline constexpr float kDigitAtlasCols = 16.0f, kDigitAtlasRows = 2.0f, kDigitUvInsetMin = 0.02f, kDigitUvInsetMax = 0.98f;

		// カラーパレット (UI Color Palette)
		inline constexpr uint32_t kColCutsceneTopBar = UI_COL32(5, 7, 10, 228), kColCutsceneBotBar = UI_COL32(5, 7, 10, 236);
		inline constexpr uint32_t kColCutsceneRule = UI_COL32(255, 255, 255, 22), kColSkipHint = UI_COL32(255, 255, 255, 195);
		inline constexpr float kSubMaxAlpha = 245.0f;

		inline constexpr uint32_t kColScrimLeft = UI_COL32(7, 10, 15, 224), kColScrimMid = UI_COL32(8, 12, 18, 188);
		inline constexpr uint32_t kColScrimRight = UI_COL32(8, 12, 18, 0), kColVignetteTint = UI_COL32(255, 255, 255, 88);
		inline constexpr int kCutFlashR = 245, kCutFlashG = 240, kCutFlashB = 228;
		inline constexpr float kCutFlashMaxAlpha = 68.0f, kAnimVisibleEps = 0.001f;

		inline constexpr uint32_t kColOffWhite = UI_COL32(243, 246, 250, 255), kColMuted = UI_COL32(182, 192, 206, 255);
		inline constexpr uint32_t kColMicroGray = UI_COL32(138, 150, 165, 255), kColBrass = UI_COL32(214, 182, 120, 255);
		inline constexpr uint32_t kColChampagne = UI_COL32(232, 198, 130, 255);

		inline constexpr uint32_t kColHeaderDivider = UI_COL32(255, 255, 255, 44), kColHeaderAccent = UI_COL32(214, 182, 120, 215);
		inline constexpr uint32_t kColTelemetryVertSep = UI_COL32(255, 255, 255, 20), kColSectionHairline = UI_COL32(255, 255, 255, 42);
		inline constexpr uint32_t kColLedgerRowLine = UI_COL32(255, 255, 255, 18), kColTotalAccent = UI_COL32(214, 182, 120, 210);

		inline constexpr uint32_t kColHoverNextFill = UI_COL32(214, 182, 120, 26), kColHoverTitleFill = UI_COL32(220, 230, 242, 20);
		inline constexpr uint32_t kColTitleBandIdle = UI_COL32(215, 220, 228, 230), kColNextLineIdle = UI_COL32(214, 182, 120, 205);
		inline constexpr uint32_t kColNextLineHover = UI_COL32(245, 218, 155, 255), kColTitleLineHover = UI_COL32(226, 232, 242, 210);

		// 仮想スクリーン ＆ カットシーン字幕レイアウト (Cutscene Subtitles Layout)
		inline constexpr float kVirtualWidth = 1280.0f, kVirtualHeight = 720.0f;

		inline constexpr float kCutsceneTopBarH = 38.0f, kCutsceneBotBarH = 54.0f;
		inline constexpr int kSubIdxInbound = 0, kSubIdxRunning = 1, kSubIdxBoarding = 2, kSubIdxTakeoff = 3;
		inline constexpr float kSubFadeDuration = 0.22f, kSubsAtlasW = 2048.0f, kSubsAtlasH = 512.0f, kSubsRowH = 96.0f;
		inline constexpr float kSubsDstLeftX = 128.0f, kSubsDstRightX = 1152.0f, kSubsDstTopOffset = 3.0f, kSubsDstBotOffset = 51.0f;

		inline constexpr float kSkipSrcX0 = 850.0f, kSkipSrcY0 = 408.0f, kSkipSrcX1 = 1130.0f, kSkipSrcY1 = 468.0f;
		inline constexpr float kSkipRightMargin1 = 168.0f, kSkipRightMargin0 = 28.0f, kSkipTopOffset = 12.0f, kSkipBotOffset = 42.0f;

		// リザルト画面共通アトラス・スクリム (Result Atlas & Scrim)
		inline constexpr float kUiAtlasSize = 2048.0f, kLeftBandAtlasOriginX = 60.0f, kLeftBandAtlasScale = 2.0f;
		inline constexpr float kPanelFadeDuration = 0.35f, kScrimMidX = 465.0f, kScrimRightX = 760.0f;
		inline constexpr float kContentLeftX = 76.0f, kContentRightX = 536.0f;

		// Band 1: ヘッダー ＆ グレードスタンプ (Header & Grade Stamp)
		inline constexpr float kHeaderFadeDuration = 0.30f, kHeaderSlideOffset = -14.0f, kHeaderBandX0 = 60.0f;
		inline constexpr float kHeaderBandY0 = 52.0f, kHeaderBandX1 = 560.0f, kHeaderBandY1 = 146.0f, kHeaderDivX = 456.0f;
		inline constexpr float kHeaderDivY0 = 64.0f, kHeaderDivY1 = 138.0f, kHeaderLineDelay = 0.04f;
		inline constexpr float kHeaderLineDuration = 0.38f, kHeaderLineMaxW = 460.0f, kHeaderAccentEndX = 122.0f, kHeaderLineY = 150.0f;

		inline constexpr int kGradeGridCols = 2;
		inline constexpr float kGradeAnimDelay = 0.22f, kGradeAnimDuration = 0.28f, kGradeBaseHalfSize = 64.0f;
		inline constexpr float kGradeStartScale = 1.12f, kGradeScaleDelta = 0.12f, kGradeCenterX = 498.0f;
		inline constexpr float kGradeCenterY = 110.0f, kGradeAtlasBaseX = 1024.0f, kGradeAtlasCellSize = 256.0f;

		// Band 2: テレメトリストリップ (Telemetry Strip)
		inline constexpr size_t kTelemetryColCount = 3, kTelColTime = 0, kTelColKills = 1, kTelColAccuracy = 2;
		inline constexpr float kTelemetryDelay = 0.08f, kTelemetryDuration = 0.30f, kTelemetrySlideOffset = -10.0f;
		inline constexpr float kTelemetryBandY0 = 160.0f, kTelemetryBandY1 = 186.0f;
		inline constexpr float  kTelemetrySepXs[]       = { 228.0f, 392.0f };
		inline constexpr float kTelemetrySepY0 = 170.0f, kTelemetrySepY1 = 214.0f, kTelemetryBottomLineY = 228.0f;
		inline constexpr float kTelemetryDigitH = 44.0f, kTelemetryDigitCenterY = 200.0f;
		inline constexpr float  kTelemetryColXs[kTelemetryColCount] = { 76.0f, 246.0f, 412.0f };

		// Band 3..9: 報酬内訳レッジャー ＆ 戦利品ステンシル
		inline constexpr float kRewardHdrDelay = 0.14f, kRewardHdrDuration = 0.28f, kRewardHdrSlideOffset = -8.0f;
		inline constexpr float kRewardHdrY0 = 238.0f, kRewardHdrX1 = 340.0f, kRewardHdrY1 = 266.0f;

		inline constexpr int kRewardRowCount = 6, kLootBonusRowIndex = 5;
		inline constexpr float kRewardRowBaseDelay = 0.17f, kRewardRowDelayStep = 0.045f, kRewardRowDuration = 0.26f;
		inline constexpr float kRewardRowSlideOffset = -10.0f, kRewardRowStartY = 268.0f, kRewardRowHeight = 32.0f;
		inline constexpr float kRewardRowLabelX1 = 320.0f, kRewardRowCenterOffsetY = 16.0f, kRewardAmtRightX = 498.0f;
		inline constexpr float kRewardAmtDigitH = 28.0f, kRewardUnitOffsetY = 1.5f, kRewardUnitDigitH = 19.5f;

		inline constexpr int kLootIconCount = 4;
		inline constexpr float kLootStripDelay = 0.44f, kLootStripDuration = 0.28f, kLootIconStepX = 92.0f;
		inline constexpr float kLootEmptyAlphaMul = 0.28f, kLootAtlasBaseX = 1024.0f, kLootAtlasY0 = 512.0f;
		inline constexpr float kLootAtlasY1 = 640.0f, kLootAtlasCellW = 128.0f, kLootIconY0 = 469.0f, kLootIconY1 = 487.0f;
		inline constexpr float kLootIconSize = 18.0f, kLootCountOffsetX = 24.0f, kLootCountCenterY = 478.0f, kLootCountDigitH = 23.0f;

		// Band 10: 総獲得報酬
		inline constexpr float kTotalDelay = 0.48f, kTotalDuration = 0.30f, kTotalSlideOffset = -10.0f;
		inline constexpr float kTotalLineY = 502.0f, kTotalAccentEndX = 114.0f, kTotalBandY0 = 508.0f;
		inline constexpr float kTotalBandX1 = 280.0f, kTotalBandY1 = 568.0f, kTotalAmtRightX = 494.0f;
		inline constexpr float kTotalAmtCenterY = 538.0f, kTotalAmtDigitH = 55.0f, kTotalUnitCenterY = 543.5f, kTotalUnitDigitH = 24.0f;

		// Band 11: アクションプロンプトボタン
		inline constexpr float kButtonsDelay = 0.56f, kButtonsDuration = 0.30f, kBtnHitY0 = 600.0f, kBtnHitY1 = 642.0f;
		inline constexpr float kNextHitX0 = 72.0f, kNextHitX1 = 228.0f, kTitleHitX0 = 252.0f, kTitleHitX1 = 438.0f;

		inline constexpr float kHoverAnimRate = 8.0f, kHoverDrawThreshold = 0.01f, kHoverFillY0 = 603.0f;
		inline constexpr float kHoverFillY1 = 637.0f, kNextFillX1 = 212.0f, kTitleFillX1 = 428.0f;

		inline constexpr float kBtnBandY0 = 602.0f, kBtnBandY1 = 644.0f, kNextBandX1 = 240.0f, kTitleBandX0 = 248.0f, kTitleBandX1 = 456.0f;

		inline constexpr float kBtnUnderlineY = 636.0f, kBtnUnderlineThickness = 1.5f, kNextUnderlineBaseEndX = 200.0f;
		inline constexpr float kNextUnderlineHoverGainX = 10.0f, kTitleUnderlineStartX = 256.0f, kTitleUnderlineSpanX = 166.0f;
	}
}
