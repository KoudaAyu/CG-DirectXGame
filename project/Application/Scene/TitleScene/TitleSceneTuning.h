#pragma once

#include "Application/Config/GameConfig.h"
#include "Vector.h"
#include <cmath>
#include <cstddef>
#include <cstdint>

// --- TitleScene 専用チューニング定数・ジオラマ配置テーブル定義 ---
namespace TitleSceneTuning
{
	struct RangeF { float minVal; float maxVal; };
	struct BoxRange3D { RangeF x; RangeF y; RangeF z; };

	struct ParticleEmitSpec
	{
		Vector4 startColor;
		Vector4 endColor;
		bool hasEndColor;
		RangeF alpha;
		RangeF startScale;
		RangeF endScale;
		bool endScaleRelative;
		RangeF lifeTime;
		float gravity;
		float drag;
		float curl;
	};

	// 共通算術・プロシージャルメッシュ生成定数 
	namespace Common
	{
		inline constexpr size_t kQuadVertCount = 4, kIndicesPerQuad = 6;
		inline constexpr uint32_t kQuadTriIndices[kIndicesPerQuad] = { 0u, 1u, 2u, 1u, 3u, 2u };
		inline constexpr int kCoinFlipMod = 2, kMatAxisX = 0, kMatAxisY = 1, kMatAxisZ = 2, kMatAxisW = 3;

		inline constexpr float kCubicInScale = 4.0f, kBackOvershoot = 1.70158f;
		inline constexpr float kBackOvershootPlusOne = kBackOvershoot + 1.0f, kSmoothstepCubicA = 3.0f;
		inline constexpr float kSmoothstepCubicB = 2.0f, kSmoothstepDerivScale = 6.0f;

		inline constexpr uint32_t kLcgMultiplier = 1664525u, kLcgIncrement = 1013904223u, kHashShiftBits = 8u;
		inline constexpr float kHashInvMax = 1.0f / 16777216.0f;

		inline constexpr float kMinNormalLen = 1e-7f, kTriVertCountF = 3.0f;
		inline constexpr int kBlobRings = 4, kBlobSegments = 7, kBladeSides = 3;
		inline constexpr float kBladeSidesF = 3.0f, kBladeCenterRatio = 0.3f;

		inline constexpr float kMinParticleScale = 0.001f, kDefaultCurlFreq = 1.6f;

		[[nodiscard]] constexpr float Smoothstep01(float t) noexcept
		{
			return t * t * (kSmoothstepCubicA - kSmoothstepCubicB * t);
		}

		[[nodiscard]] constexpr float SmoothstepDeriv01(float t) noexcept
		{
			return kSmoothstepDerivScale * t * (1.0f - t);
		}
	}

	// フレーム時間・イントロ演出・遷移・オーディオ設定 
	namespace Timing
	{
		inline constexpr float kMinDeltaTime = 0.0001f, kMaxDeltaTime = 1.0f / 30.0f, kIntroDuration = 3.0f;
		inline constexpr float kLogoSlamTime = 2.40f, kLogoSlideTime = 0.36f, kStartTransitionDelay = 0.30f;
		inline constexpr float kGameplayTransitionDuration = 0.60f;

		inline constexpr float kBgmLoopDuration = 162.0f, kBgmVolume = 0.35f, kStartDuckSeVolume = 0.50f;
		inline constexpr float kStartUiClickVolume = 0.72f, kStartRifleShotVolume = 0.44f, kLogoSlamSeVolume = 0.64f;
		inline constexpr float kClickHopDuckSeVolume = 0.52f;
		inline constexpr int kLeftMouseButton = 0;
	}

	// カメラ構図・シネマティック導入・手ブレ・パララックス設定 
	namespace CameraChoreo
	{
		inline constexpr float kFovY = 0.70f;
		inline constexpr Vector3 kStartPos = { 3.50f, 8.50f, -9.00f }, kStartRot = { 0.80f, -0.30f, 0.0f };
		inline constexpr Vector3 kFinalPos = { 4.20f, 4.00f, -5.20f }, kFinalRot = { 0.42f, -0.38f, 0.0f };

		inline constexpr float kLogoSlamShakeDuration = 0.20f, kLogoSlamShakePower = 0.035f, kIdleBlendFadeDuration = 1.0f;
		inline constexpr float kIdleSwayFreqX = 0.35f, kIdleSwayAmpX = 0.04f, kIdleSwayFreqY = 0.50f, kIdleSwayAmpY = 0.02f;

		inline constexpr float kParallaxPosX = 0.12f, kParallaxPosY = 0.06f, kParallaxRotY = 0.015f, kParallaxRotX = 0.008f;
	}

	// ライティング設定 (澄んだ午後の陽光 + 主役ポイントライト) 
	namespace Lighting
	{
		inline constexpr Vector3 kSunRawDirection = { -0.42f, -0.78f, 0.46f };
		inline constexpr Vector4 kSunColor = { 0.98f, 0.94f, 0.86f, 1.0f };
		inline constexpr float kSunIntensity = 1.10f;

		inline constexpr Vector4 kHeroLightColor = { 1.0f, 0.95f, 0.82f, 1.0f };
		inline constexpr Vector3 kHeroLightOffset = { 0.2f, 0.8f, -0.5f };
		inline constexpr float kHeroLightBaseIntensity = 0.75f, kHeroLightRadius = 5.5f, kHeroLightDecay = 1.6f;
		inline constexpr float kMuzzleFlashNormDuration = 0.14f, kMuzzleFlashIntensityBoost = 1.8f;
	}

	// 地形・川岸ライン設定 (手前岸 / 対岸) 
	namespace Terrain
	{
		inline constexpr float kLandY = -0.40f, kFarLandY = -0.38f, kWaterY = -0.65f, kRiverBedY = -0.75f;

		inline constexpr float kNearShoreBaseZ = 2.6f, kNearShoreWaveAmp = 0.30f, kNearShoreWaveFreq = 0.30f;
		inline constexpr float kFarShoreBaseZ = 7.4f, kFarShoreWaveAmp = 0.25f, kFarShoreWaveFreq = 0.28f;

		inline constexpr float kSpanLinearX = 22.0f, kSpanCubicX = 158.0f;
		inline constexpr int kGroundCols = 128;
		inline constexpr float kGroundBoundingRadius = 420.0f, kGroundUvScale = 0.30f;

		inline constexpr int kNearRows = 14, kNearFlatRows = 8;
		inline constexpr float kNearFlatRowDiv = 7.0f, kNearSlopeRowDiv = 5.0f, kNearEdgeInset = 0.05f;
		inline constexpr float kNearLinearZ = 12.0f, kNearCubicZ = 38.0f, kNearSlopeSpanZ = 0.50f;
		inline constexpr Vector4 kNearGroundColor = { 0.88f, 0.90f, 0.88f, 1.0f };

		inline constexpr int kFarRows = 18, kFarSlopeRows = 6;
		inline constexpr float kFarSlopeRowDiv = 5.0f, kFarFlatRowDiv = 11.0f, kFarSlopeStartOffset = 0.45f;
		inline constexpr float kFarSlopeSpanZ = 0.50f, kFarFlatInset = 0.05f, kFarLinearZ = 28.0f, kFarCubicZ = 232.0f;
		inline constexpr Vector4 kFarGroundColor = { 0.86f, 0.88f, 0.86f, 1.0f };

		[[nodiscard]] inline float NearShoreZ(float x) noexcept
		{
			return kNearShoreBaseZ + kNearShoreWaveAmp * std::sin(x * kNearShoreWaveFreq);
		}

		[[nodiscard]] inline float FarShoreZ(float x) noexcept
		{
			return kFarShoreBaseZ + kFarShoreWaveAmp * std::cos(x * kFarShoreWaveFreq);
		}

		[[nodiscard]] constexpr float MapDioramaSpanX(int col, int totalCols) noexcept
		{
			const float u = (static_cast<float>(col) / static_cast<float>(totalCols - 1)) * 2.0f - 1.0f;
			return kSpanLinearX * u + kSpanCubicX * u * u * u;
		}
	}

	// 川の水面メッシュ・ゲルストナー波・マウス3D波紋設定 
	namespace Water
	{
		inline constexpr float kBoundingRadius = 280.0f, kStartZ = 0.5f, kSpanZ = 10.5f, kUvScale = 0.22f;
		inline constexpr Vector4 kWaterColor = { 0.42f, 0.82f, 0.94f, 0.90f };
		inline constexpr float kReflectionFactor = 0.58f, kFresnelF0 = 0.04f, kFlowUvSpeed = 0.08f, kRayDownMinY = -1e-5f;
		inline constexpr float kHoverMinX = -12.0f, kHoverMaxX = 12.0f;

		struct GerstnerWaveSpec { float dirX; float dirZ; float amp; float len; float speed; };
		inline constexpr GerstnerWaveSpec kGerstnerWaves[] = {
			{ 0.97f,  0.24f, 0.020f, 6.5f, 1.10f },
			{ 0.89f, -0.45f, 0.010f, 4.2f, 1.30f },
			{ 0.99f,  0.14f, 0.004f, 2.4f, 1.60f },
		};

		inline constexpr size_t kMaxRippleSources = 16;
		inline constexpr float kRippleEmitCooldown = 0.08f, kRippleBaseRadius = 0.4f, kRippleExpandSpeed = 2.2f;
		inline constexpr float kRippleBandWidth = 1.1f, kRippleMinCenterDist = 0.05f, kRippleAmpScale = 0.022f;
	}

	// 主役アヒル配置・呼吸・クリックホップ・出撃発砲演出 
	namespace HeroDuck
	{
		inline constexpr float kBoundingRadius = 10.0f;
		inline constexpr Vector3 kBasePos = { 3.30f, Terrain::kLandY, 0.90f };
		inline constexpr float kScale = 0.75f, kFacingYaw = -1.25f;

		inline constexpr float kStartRecoilDuration = 0.25f, kRecoilNormDiv = 0.14f, kRecoilBackZ = 0.04f, kRecoilPitch = 0.06f;

		inline constexpr Vector3 kMuzzleOffset = { 0.15f, 0.42f, 0.35f };
		inline constexpr Vector4 kMuzzleFlashColor = { 1.0f, 0.85f, 0.35f, 1.0f };
		inline constexpr float kMuzzleFlashScale = 2.0f, kMuzzleFlareScale = 0.70f;
		inline constexpr Vector4 kMuzzleFlareColor = { 1.0f, 0.95f, 0.80f, 1.0f };
		inline constexpr float kMuzzleFlareLife = 0.12f;

		inline constexpr float kBreatheFreq = 2.0f, kBreatheAmp = 0.008f, kIdleYawFreq = 0.8f, kIdleYawAmp = 0.04f, kAimFollowYaw = 0.15f;

		inline constexpr float kHitCenterOffsetY = 0.3f, kMinClipW = 0.01f, kClickHitRadiusPx = 95.0f, kHopDuration = 0.35f;
		inline constexpr float kHopHeight = 0.18f, kHopPitchAmp = 0.08f;

		inline constexpr int kHopDustCount = 6;
		inline constexpr float kHopDustRadius = 0.15f, kHopDustOffsetY = 0.02f;
		inline constexpr RangeF kHopDustHorizSpd = { 0.4f, 0.8f }, kHopDustUpSpd = { 0.2f, 0.5f };
		inline constexpr ParticleEmitSpec kHopDustSpec = { { 0.82f, 0.76f, 0.65f, 0.45f }, { 0.76f, 0.70f, 0.58f, 0.0f }, true,
			{ 0.45f, 0.45f }, { 0.035f, 0.035f }, { 0.10f, 0.10f }, false, { 0.40f, 0.40f }, 0.0f, 0.0f, 0.04f };

		inline constexpr float kBattleCryDuration = 3.12f, kBattleCryFreq = 14.0f, kBattleCryPitchAmp = 0.08f, kBattleCryHopAmp = 0.04f;
	}

	// 手前岸物資・対岸ゾーンプロップ配置テーブル・脱出パッド設定 
	namespace Props
	{
		inline constexpr float kCrateBoundingRadius = 6.0f;
		inline constexpr Vector3 kCrate1Scale = { 0.58f, 0.58f, 0.58f }, kCrate1Pos = { 4.40f, Terrain::kLandY, 1.60f };
		inline constexpr Vector3 kCrate1Rot = { 0.0f, 0.35f, 0.0f }, kCrate2Scale = { 0.42f, 0.42f, 0.42f };
		inline constexpr Vector3 kCrate2Pos = { 4.45f, Terrain::kLandY + 0.70f, 1.63f }, kCrate2Rot = { 0.0f, -0.15f, 0.0f };

		inline constexpr float kSandbagBoundingRadius = 10.0f;
		inline constexpr Vector3 kSandbagScale = { 0.46f, 0.46f, 0.46f }, kSandbagPos = { 2.95f, Terrain::kLandY, 1.85f };
		inline constexpr Vector3 kSandbagRot = { 0.0f, 0.15f, 0.0f };

		inline constexpr float kExtractionPadBoundingRadius = 15.0f;
		inline constexpr Vector3 kExtractionPadScale = { 0.80f, 0.80f, 0.80f }, kExtractionPadPos = { 2.70f, Terrain::kFarLandY, 15.70f };
		inline constexpr Vector3 kExtractionPadFallbackPos = { 0.50f, Terrain::kFarLandY, 15.50f };

		enum class EnemyPropKind : uint8_t
		{
			Bridge = 0, Sandbag, Signpost, Barrel, Container, Crate, Watchtower, Target, Count
		};

		struct EnemyPropSpec { EnemyPropKind kind; Vector3 position; Vector3 scale; Vector3 rotation; };

		inline constexpr EnemyPropSpec kEnemyPropSpecs[] = {
			// 中央木造橋梁 
			{ EnemyPropKind::Bridge,     {  0.60f, Terrain::kFarLandY + 0.06f,  5.00f }, { 0.62f, 0.55f, 0.72f }, { 0.0f,  0.00f, 0.0f } },
			// Zone 1: 渡河橋頭堡検問所 
			{ EnemyPropKind::Sandbag,    { -0.50f, Terrain::kFarLandY,          8.70f }, { 0.56f, 0.56f, 0.56f }, { 0.0f,  0.00f, 0.0f } },
			{ EnemyPropKind::Signpost,   { -0.75f, Terrain::kFarLandY,          8.10f }, { 0.45f, 0.45f, 0.45f }, { 0.0f,  0.25f, 0.0f } },
			{ EnemyPropKind::Barrel,     {  2.30f, Terrain::kFarLandY,          8.50f }, { 0.55f, 0.55f, 0.55f }, { 0.0f,  0.20f, 0.0f } },
			// Zone 2: 左翼軍用コンテナ物流拠点 
			{ EnemyPropKind::Container,  { -4.80f, Terrain::kFarLandY,         12.40f }, { 0.80f, 0.80f, 0.80f }, { 0.0f,  0.38f, 0.0f } },
			{ EnemyPropKind::Container,  { -4.75f, Terrain::kFarLandY + 1.77f, 12.45f }, { 0.76f, 0.76f, 0.76f }, { 0.0f,  0.36f, 0.0f } },
			{ EnemyPropKind::Container,  { -2.00f, Terrain::kFarLandY,         13.20f }, { 0.78f, 0.78f, 0.78f }, { 0.0f, -0.22f, 0.0f } },
			{ EnemyPropKind::Crate,      { -1.60f, Terrain::kFarLandY,         10.40f }, { 0.65f, 0.65f, 0.65f }, { 0.0f,  0.15f, 0.0f } },
			{ EnemyPropKind::Crate,      { -1.55f, Terrain::kFarLandY + 0.79f, 10.50f }, { 0.50f, 0.50f, 0.50f }, { 0.0f, -0.20f, 0.0f } },
			// Zone 3: 右翼高台の木造監視塔 
			{ EnemyPropKind::Watchtower, {  4.20f, Terrain::kFarLandY,         12.00f }, { 0.85f, 0.85f, 0.85f }, { 0.0f, -0.30f, 0.0f } },
			{ EnemyPropKind::Crate,      {  6.30f, Terrain::kFarLandY,         11.20f }, { 0.65f, 0.65f, 0.65f }, { 0.0f, -0.10f, 0.0f } },
			{ EnemyPropKind::Barrel,     {  6.20f, Terrain::kFarLandY,         12.70f }, { 0.60f, 0.60f, 0.60f }, { 0.0f,  0.30f, 0.0f } },
			// Zone 4: 最奥の開けた脱出地点 & 射撃場 
			{ EnemyPropKind::Target,     { -1.60f, Terrain::kFarLandY,         16.10f }, { 0.55f, 0.55f, 0.55f }, { 0.0f, -0.15f, 0.0f } },
			{ EnemyPropKind::Target,     { -0.10f, Terrain::kFarLandY,         16.30f }, { 0.55f, 0.55f, 0.55f }, { 0.0f,  0.20f, 0.0f } },
		};
	}

	// 敵兵アヒル小隊の配置テーブル & 警戒・哨戒AI設定 
	namespace EnemySquad
	{
		enum class BehaviorType : uint8_t
		{
			SentryLookout = 0, OvalPatrol, StandGuard, ScoutScan, PatrolAcross
		};

		inline constexpr Vector3 kEnemyDuckScale = { 0.46f, 0.46f, 0.46f };

		struct EnemyDuckSpec
		{
			Vector3 basePos;
			float baseFacing;
			BehaviorType behavior;
			float radiusX;
			float radiusZ;
			float speed;
			float phaseOffset;
		};

		inline constexpr EnemyDuckSpec kEnemyDuckSpecs[] = {
			{ { 0.90f, Terrain::kFarLandY,  9.40f }, GameConfig::Math::kPi * 0.96f, BehaviorType::SentryLookout, 0.00f, 0.00f, 0.55f, 0.0f },
			{ { 0.05f, Terrain::kFarLandY, 10.85f }, GameConfig::Math::kPi,         BehaviorType::OvalPatrol,    0.42f, 0.16f, 0.35f, 1.2f },
			{ { 2.40f, Terrain::kFarLandY,  9.90f }, GameConfig::Math::kPi - 0.35f, BehaviorType::StandGuard,    0.00f, 0.00f, 0.70f, 2.4f },
			{ { 0.60f, Terrain::kFarLandY, 13.20f }, GameConfig::Math::kPi * 0.90f, BehaviorType::ScoutScan,     0.00f, 0.00f, 0.45f, 3.6f },
			{ { 0.00f, Terrain::kFarLandY, 11.20f }, GameConfig::Math::kPi * 0.50f, BehaviorType::PatrolAcross,  4.20f, 0.00f, 0.32f, 0.0f },
		};

		inline constexpr float kBreatheFreq = 1.8f, kBreatheAmp = 0.005f, kSentryScanAmp1 = 0.22f;
		inline constexpr float kSentrySubFreqMul = 0.37f, kSentryScanAmp2 = 0.10f, kOvalWalkCycleMul = 4.0f;
		inline constexpr float kWaddleHopAmp = 0.0035f, kWaddleRollAmp = 0.015f, kStandGuardScanAmp = 0.14f;
		inline constexpr float kStandGuardPitch = 0.035f, kScoutTurnGain = 2.2f, kScoutYawAmp = 0.70f;
		inline constexpr float kPatrolTurnGain = 3.5f, kPatrolWalkCycleMul = 6.0f;
	}

	// 接地影デカール一括配置テーブル & 敵追従影設定 
	namespace Shadows
	{
		inline constexpr float kHeightOffset = 0.012f, kStaticBoundingRadius = 60.0f;
		inline constexpr Vector4 kStaticShadowColor = { 0.05f, 0.06f, 0.08f, 0.52f };
		inline constexpr float kEnemyShadowBoundingRadius = 5.0f, kEnemyShadowRadius = 0.32f;
		inline constexpr Vector4 kEnemyShadowColor = { 0.05f, 0.06f, 0.08f, 0.50f };

		struct ShadowDiscSpec { float cx; float cy; float cz; float rx; float rz; };

		inline constexpr ShadowDiscSpec kStaticShadowDiscs[] = {
			// 手前岸 (主役アヒル・物資・木造橋手前) 
			{  3.30f, Terrain::kLandY + kHeightOffset,     0.90f, 0.48f, 0.48f },
			{  4.40f, Terrain::kLandY + kHeightOffset,     1.60f, 0.45f, 0.45f },
			{  2.95f, Terrain::kLandY + kHeightOffset,     1.85f, 0.55f, 0.35f },
			{  0.60f, Terrain::kLandY + kHeightOffset,     2.30f, 1.05f, 0.30f },
			// 対岸 (木造橋奥・検問所・コンテナヤード・監視塔・脱出地点) 
			{  0.60f, Terrain::kFarLandY + kHeightOffset,  7.75f, 1.05f, 0.30f },
			{ -0.50f, Terrain::kFarLandY + kHeightOffset,  8.70f, 0.60f, 0.30f },
			{ -0.75f, Terrain::kFarLandY + kHeightOffset,  8.10f, 0.28f, 0.28f },
			{  2.30f, Terrain::kFarLandY + kHeightOffset,  8.50f, 0.48f, 0.40f },
			{ -4.80f, Terrain::kFarLandY + kHeightOffset, 12.40f, 1.10f, 1.40f },
			{ -2.00f, Terrain::kFarLandY + kHeightOffset, 13.20f, 1.10f, 1.30f },
			{ -1.60f, Terrain::kFarLandY + kHeightOffset, 10.40f, 0.55f, 0.55f },
			{  3.60f, Terrain::kFarLandY + kHeightOffset, 11.40f, 0.30f, 0.30f },
			{  4.80f, Terrain::kFarLandY + kHeightOffset, 11.40f, 0.30f, 0.30f },
			{  3.60f, Terrain::kFarLandY + kHeightOffset, 12.60f, 0.30f, 0.30f },
			{  4.80f, Terrain::kFarLandY + kHeightOffset, 12.60f, 0.30f, 0.30f },
			{  6.30f, Terrain::kFarLandY + kHeightOffset, 11.20f, 0.48f, 0.48f },
			{  6.20f, Terrain::kFarLandY + kHeightOffset, 12.70f, 0.48f, 0.40f },
			{  2.70f, Terrain::kFarLandY + kHeightOffset, 15.70f, 1.35f, 1.35f },
			{ -1.60f, Terrain::kFarLandY + kHeightOffset, 16.10f, 0.35f, 0.35f },
			{ -0.10f, Terrain::kFarLandY + kHeightOffset, 16.30f, 0.35f, 0.35f },
		};
	}

	// 風シミュレーション設定 (木々・茂み・葦・草の動的しなり) 
	namespace Wind
	{
		inline constexpr float kMinSwayEpsilon = 0.001f, kSpatialFreqX = 0.28f, kSpatialFreqZ = 0.40f, kWaveSpeed = 1.30f;
		inline constexpr float kGustBase = 0.60f, kGustAmp = 0.40f, kGustTimeFreq = 0.55f, kGustSpatialFreqX = 0.15f;
		inline constexpr float kSubWaveFreqMul = 1.9f, kSubWavePhase = 1.2f, kSubWaveAmp = 0.30f, kFlutterTimeFreq = 3.2f;
		inline constexpr float kFlutterFreqX = 1.8f, kFlutterFreqZ = 1.2f, kFlutterAmp = 0.014f, kMainDispScale = 0.090f;
		inline constexpr float kWindDirX = 0.92f, kWindDirZ = 0.38f, kWindSagY = 0.10f;
	}

	// 環境装飾バッチ・植生配置テーブル・草葦生成・小道デカール設定 
	namespace Scenery
	{
		enum BatchId : int
		{
			kTrunk = 0, kLeaf, kPine, kBush, kRock, kReed, kGrass, kLeafLight, kBatchCount
		};

		inline constexpr float kBatchBoundingRadius = 260.0f;
		inline constexpr uint32_t kInitialSeed = 20261006u;

		// 小道ウェイポイント (手前岸 / 対岸) 
		inline constexpr Vector2 kNearPathWaypoints[] = {
			{ 4.6f, -2.0f }, { 3.9f, -0.6f }, { 2.9f, 0.5f },
			{ 1.9f,  1.3f }, { 1.0f,  1.9f }, { 0.6f, 2.3f },
		};
		inline constexpr Vector2 kFarPathWaypoints[] = {
			{ 0.6f, 7.8f }, { 1.2f, 10.0f }, { 2.0f, 12.2f }, { 2.6f, 13.8f },
		};
		inline constexpr float kDistInitMax = 1e9f;

		// 配置済みプロップの占有領域 (植生めり込み防止) 
		struct Footprint { float x; float z; float hw; float hd; float r; };

		inline constexpr Footprint kPropFootprints[] = {
			{  3.30f,  0.90f, 0.00f, 0.00f, 0.50f }, // 主役アヒル 
			{  4.40f,  1.60f, 0.00f, 0.00f, 0.55f }, // 木箱 
			{  2.95f,  1.85f, 0.85f, 0.20f, 0.05f }, // 土嚢 
			{  0.60f,  5.00f, 0.95f, 2.90f, 0.05f }, // 木造橋 
			{ -0.50f,  8.70f, 1.00f, 0.25f, 0.05f }, // 土嚢 
			{ -0.75f,  8.10f, 0.45f, 0.15f, 0.05f }, // 標識 
			{  2.30f,  8.50f, 0.47f, 0.25f, 0.05f }, // ドラム缶 
			{  0.90f,  9.40f, 0.35f, 0.00f, 0.45f }, // 歩哨アヒル 
			{ -4.80f, 12.40f, 1.55f, 2.20f, 0.05f }, // コンテナ上下段 
			{ -2.00f, 13.20f, 1.27f, 2.10f, 0.05f }, // コンテナ 
			{ -1.60f, 10.40f, 0.00f, 0.00f, 0.55f }, // 木箱 
			{  0.05f, 10.85f, 0.55f, 0.00f, 0.45f }, // 巡回アヒル 
			{  4.20f, 12.00f, 1.55f, 1.55f, 0.05f }, // 監視塔 
			{  6.30f, 11.20f, 0.00f, 0.00f, 0.55f }, // 木箱 
			{  6.20f, 12.70f, 0.51f, 0.25f, 0.05f }, // ドラム缶 
			{  2.40f,  9.90f, 0.25f, 0.00f, 0.45f }, // 警戒アヒル 
			{  2.70f, 15.70f, 0.00f, 0.00f, 2.40f }, // 脱出パッド 
			{ -1.60f, 16.10f, 0.42f, 0.12f, 0.10f }, // 標的1 
			{ -0.10f, 16.30f, 0.42f, 0.12f, 0.10f }, // 標的2 
			{  0.60f, 13.20f, 0.40f, 0.00f, 0.45f }, // 斥候アヒル 
		};

		inline constexpr float kFootprintEps = 1e-4f, kNearSideSplitZ = 5.0f, kShoreMargin = 0.10f;
		inline constexpr int kMaxPushIters = 8;
		inline constexpr float kPushExtraMargin = 0.02f, kFinalBlockCheckScale = 0.95f;

		inline constexpr float kCampMinX = -3.4f, kCampMaxX = 5.9f, kCampMinZ = 7.3f, kCampMaxZ = 15.8f;

		// 樹木・茂み・岩・草・葦の形状パラメータ 
		inline constexpr float kBroadleafRadius = 0.85f, kBroadleafScaleXZ = 0.42f, kBroadleafScaleY = 0.43f;
		inline constexpr float kPineRadius = 0.62f, kTallPineProb = 0.34f, kPineScaleXZ = 0.36f, kPineScaleY = 0.37f;

		inline constexpr float kBushRadius = 0.60f;
		inline constexpr int kBushFronds = 5;
		inline constexpr float kBushHalfWidth = 0.42f, kBushHeight = 0.54f, kBushTiltOut = 0.24f, kBushSinkY = 0.02f;
		inline constexpr float kBushTopWidthMul = 1.15f, kBushNormXZ = 0.45f, kBushNormY = 0.89f, kBushUvMinU = 0.642f;
		inline constexpr float kBushUvMaxU = 0.985f, kBushUvMinV = 0.515f, kBushUvMaxV = 0.985f;
		inline constexpr uint32_t kBushQuadIndices[Common::kIndicesPerQuad] = { 0u, 2u, 1u, 1u, 2u, 3u };

		inline constexpr float kRockRadius = 0.38f, kRockMainOffsetY = 0.06f;
		inline constexpr Vector3 kRockMainRadii = { 0.28f, 0.18f, 0.24f };
		inline constexpr float kRockMainJitter = 0.22f;
		inline constexpr Vector3 kRockSubOffset = { 0.22f, 0.02f, 0.12f }, kRockSubRadii = { 0.13f, 0.09f, 0.12f };
		inline constexpr float kRockSubJitter = 0.25f, kRiverRockSubmergeY = 0.12f;

		inline constexpr int kTuftBaseBlades = 4;
		inline constexpr float kTuftExtraBlades = 3.0f;
		inline constexpr RangeF kTuftOffsetR = { 0.0f, 0.07f }, kTuftHeight = { 0.14f, 0.26f };
		inline constexpr float kTuftSinkY = 0.01f, kTuftBladeWidth = 0.018f, kTuftLeanRatio = 0.35f;

		inline constexpr int kReedBaseBlades = 6;
		inline constexpr float kReedExtraBlades = 5.0f, kReedSpreadX = 0.22f, kReedSpreadZ = 0.12f;
		inline constexpr RangeF kReedHeight = { 0.45f, 0.85f };
		inline constexpr float kReedBladeWidth = 0.016f, kReedLeanX = 0.10f, kReedLeanZ = 0.08f;

		enum class ItemKind : uint8_t { Broadleaf = 0, Pine, Bush, Rock };
		struct PlacementSpec { ItemKind kind; float x; float z; float groundY; float scale; };

		inline constexpr PlacementSpec kForegroundPlacements[] = {
			{ ItemKind::Broadleaf, -1.80f, -2.00f, Terrain::kLandY, 2.7f },
			{ ItemKind::Broadleaf, -2.20f, -1.20f, Terrain::kLandY, 2.3f },
			{ ItemKind::Broadleaf, -2.80f, -1.80f, Terrain::kLandY, 2.4f },
			{ ItemKind::Broadleaf, -2.60f,  0.20f, Terrain::kLandY, 1.7f },
			{ ItemKind::Broadleaf, -4.20f,  0.50f, Terrain::kLandY, 1.8f },
			{ ItemKind::Pine,      -1.10f,  1.50f, Terrain::kLandY, 1.1f },
			{ ItemKind::Pine,      -3.00f, -0.60f, Terrain::kLandY, 1.6f },
			{ ItemKind::Broadleaf, -3.60f,  2.00f, Terrain::kLandY, 1.3f },
			{ ItemKind::Pine,       6.00f,  2.00f, Terrain::kLandY, 1.3f },
			{ ItemKind::Pine,       6.80f,  0.60f, Terrain::kLandY, 1.0f },
			{ ItemKind::Bush,      -0.50f,  2.15f, Terrain::kLandY, 1.0f },
			{ ItemKind::Bush,       5.35f,  1.20f, Terrain::kLandY, 1.1f },
			{ ItemKind::Bush,      -1.90f, -0.60f, Terrain::kLandY, 1.2f },
			{ ItemKind::Bush,      -3.30f,  1.00f, Terrain::kLandY, 0.9f },
			{ ItemKind::Bush,      -2.50f, -1.00f, Terrain::kLandY, 1.1f },
			{ ItemKind::Rock,       1.45f,  0.45f, Terrain::kLandY, 0.8f },
			{ ItemKind::Rock,       5.25f,  0.30f, Terrain::kLandY, 1.0f },
			{ ItemKind::Rock,      -1.55f,  2.30f, Terrain::kLandY, 0.9f },
			{ ItemKind::Rock,      -0.40f,  0.20f, Terrain::kLandY, 0.6f },
			{ ItemKind::Rock,      -2.20f,  4.20f, Terrain::kWaterY - kRiverRockSubmergeY, 1.3f },
			{ ItemKind::Rock,       2.60f,  5.60f, Terrain::kWaterY - kRiverRockSubmergeY, 1.0f },
			{ ItemKind::Rock,       4.40f,  3.40f, Terrain::kWaterY - kRiverRockSubmergeY, 1.1f },
			{ ItemKind::Rock,      -4.40f,  6.10f, Terrain::kWaterY - kRiverRockSubmergeY, 0.9f },
		};

		inline constexpr PlacementSpec kFarShorePlacements[] = {
			{ ItemKind::Pine,      -8.0f,  9.60f, Terrain::kFarLandY, 2.0f },
			{ ItemKind::Pine,      -8.6f, 11.80f, Terrain::kFarLandY, 2.4f },
			{ ItemKind::Pine,      -8.2f, 13.90f, Terrain::kFarLandY, 2.6f },
			{ ItemKind::Pine,      -9.8f,  9.90f, Terrain::kFarLandY, 2.2f },
			{ ItemKind::Pine,     -10.6f, 13.00f, Terrain::kFarLandY, 2.6f },
			{ ItemKind::Broadleaf, -9.2f, 15.00f, Terrain::kFarLandY, 2.0f },
			{ ItemKind::Pine,     -11.8f, 11.20f, Terrain::kFarLandY, 2.5f },
			{ ItemKind::Pine,     -12.2f, 14.40f, Terrain::kFarLandY, 2.8f },
			{ ItemKind::Broadleaf, -6.2f, 16.60f, Terrain::kFarLandY, 1.9f },
			{ ItemKind::Bush,      -6.2f,  7.95f, Terrain::kFarLandY, 1.0f },
			{ ItemKind::Bush,      -6.8f,  8.60f, Terrain::kFarLandY, 1.2f },
			{ ItemKind::Bush,      -8.6f,  8.40f, Terrain::kFarLandY, 1.1f },
			{ ItemKind::Rock,      -0.9f,  9.60f, Terrain::kFarLandY, 1.0f },
			{ ItemKind::Rock,      -6.9f,  9.90f, Terrain::kFarLandY, 1.0f },
			{ ItemKind::Rock,      -7.9f,  8.90f, Terrain::kFarLandY, 0.9f },
			{ ItemKind::Bush,       2.9f,  8.60f, Terrain::kFarLandY, 1.0f },
			{ ItemKind::Bush,       5.8f,  9.20f, Terrain::kFarLandY, 1.3f },
			{ ItemKind::Broadleaf,  8.0f,  9.60f, Terrain::kFarLandY, 1.4f },
			{ ItemKind::Pine,       7.9f, 14.00f, Terrain::kFarLandY, 1.8f },
		};

		// 両岸の葦スイープ・草むら散布設定 
		inline constexpr float kReedSweepMinX = -8.5f, kReedSweepMaxX = 8.5f, kReedSweepStepX = 0.55f, kReedJitterX = 0.2f;
		inline constexpr float kReedBridgeGapMinX = -0.5f, kReedBridgeGapMaxX = 1.7f, kReedNearProb = 0.65f;
		inline constexpr float kReedFarProb = 0.55f, kReedShoreOffsetZ = 0.18f, kReedWaterSinkY = 0.06f;

		struct AvoidCircle { float x; float z; float r; };
		inline constexpr AvoidCircle kNearGrassAvoidCircles[] = {
			{  3.30f, 0.90f, 0.65f },
			{  4.40f, 1.60f, 0.55f },
			{  2.95f, 1.85f, 0.60f },
			{ -2.60f, 0.20f, 0.50f },
			{ -1.10f, 1.50f, 0.30f },
		};

		inline constexpr int kGrassMaxTries = 450;
		inline constexpr float kGrassBlockRadius = 0.12f;
		inline constexpr int kNearGrassTargetCount = 95;
		inline constexpr RangeF kNearGrassRangeX = { -5.5f, 7.5f };
		inline constexpr float kNearGrassMinZ = -2.5f, kNearGrassShoreMarginZ = 0.15f, kNearGrassMinPathDist = 0.55f;
		inline constexpr RangeF kNearGrassScale = { 0.8f, 1.4f };
		inline constexpr int kNearReedMixModulo = 3;

		inline constexpr int kFarGrassTargetCount = 75;
		inline constexpr RangeF kFarGrassRangeX = { -9.0f, 8.5f };
		inline constexpr float kFarGrassShoreMarginZ = 0.2f, kFarGrassMaxZ = 15.0f, kFarGrassMinPathDist = 0.6f;
		inline constexpr RangeF kFarGrassScale = { 1.0f, 1.6f };

		// 対岸の草むら除外ゾーン 
		inline constexpr float kExclContainerMinX = -6.4f, kExclContainerMaxX = -0.7f, kExclContainerMinZ = 10.2f;
		inline constexpr float kExclContainerMaxZ = 15.3f, kExclTowerMinX = 2.9f, kExclTowerMaxX = 5.5f;
		inline constexpr float kExclTowerMinZ = 10.7f, kExclTowerMaxZ = 13.3f, kExclPadCenterX = 2.7f;
		inline constexpr float kExclPadCenterZ = 15.7f, kExclPadHalfExtent = 2.0f, kExclCheckpointMaxZ = 9.0f;
		inline constexpr float kExclCheckpointMinX = -1.2f, kExclCheckpointMaxX = 2.9f;

		struct ForestBandSpec
		{
			int rows; float xMin; float xMax; RangeF stepX;
			float zBase; float zRowStep; float zJitter;
			RangeF scale; float scaleRowStep; float broadleafProb; float broadleafScaleMul;
		};

		inline constexpr int kWingForestRows = 3;
		inline constexpr ForestBandSpec kCenterBackForestBand = {
			3, -16.0f, 12.0f, { 1.0f, 1.5f }, 16.5f, 2.4f, 0.5f, { 2.0f, 3.0f }, 0.40f, 0.28f, 0.85f };
		inline constexpr ForestBandSpec kLeftWingForestBand = {
			3, -52.0f, -16.6f, { 1.3f, 2.1f }, 11.5f, 4.2f, 0.8f, { 2.1f, 3.2f }, 0.35f, 0.30f, 0.85f };
		inline constexpr ForestBandSpec kRightWingForestBand = {
			3, 12.6f, 36.0f, { 1.4f, 2.2f }, 12.5f, 4.2f, 0.8f, { 2.0f, 3.1f }, 0.35f, 0.28f, 0.85f };
		inline constexpr ForestBandSpec kSkylineForestBand = {
			2, -56.0f, 38.0f, { 1.8f, 2.8f }, 24.5f, 4.6f, 0.9f, { 2.8f, 4.1f }, 0.45f, 0.24f, 0.85f };

		inline constexpr Vector4 kBatchColors[kBatchCount] = {
			{ 0.42f, 0.30f, 0.20f, 1.0f }, // 幹 
			{ 0.96f, 0.98f, 0.94f, 6.0f }, // シラカバ広葉樹 
			{ 0.95f, 0.98f, 0.93f, 6.0f }, // トウヒ・アカマツ 
			{ 0.94f, 0.97f, 0.92f, 6.0f }, // 森林低木茂み 
			{ 0.88f, 0.90f, 0.88f, 1.0f }, // 岩 
			{ 0.60f, 0.62f, 0.32f, 1.0f }, // 葦 
			{ 0.40f, 0.62f, 0.25f, 1.0f }, // 草 
			{ 0.46f, 0.66f, 0.30f, 1.0f }, // 明るい葉 
		};

		// 頂点ごとの風揺れウェイト係数 & 土の小道デカール設定 
		inline constexpr float kSwaySideSplitZ = 4.0f, kTreeTrunkMaxU = 0.245f, kTreeTrunkRigidHeight = 0.45f;
		inline constexpr float kReedFullHeight = 0.8f, kReedMaxSway = 0.75f, kGrassFullHeight = 0.35f;
		inline constexpr float kGrassMaxSway = 0.60f, kBushFullHeight = 0.55f, kBushMaxSway = 0.32f;
		inline constexpr float kBroadleafSpanHeight = 3.2f, kBroadleafMaxSway = 0.55f, kPineSpanHeight = 3.5f, kPineMaxSway = 0.44f;

		inline constexpr float kPathBoundingRadius = 60.0f, kPathStepDist = 0.25f;
		inline constexpr RangeF kPathRadius = { 0.40f, 0.52f };
		inline constexpr float kPathJitter = 0.05f, kPathHeightOffset = 0.015f;
		inline constexpr Vector4 kPathColor = { 0.50f, 0.38f, 0.24f, 0.45f };
	}

	// 川面マウス波しぶきインタラクション設定 
	namespace RiverSpray
	{
		inline constexpr float kMinDeltaTimeForSpeed = 1e-4f, kSpeedNormDiv = 1.5f, kSpeedFactorMin = 0.5f;
		inline constexpr float kSpeedFactorMax = 3.5f, kRiverDriftSpeedX = 0.364f;

		// A. クリック時の大波しぶき 
		inline constexpr int kClickDropCount = 34;
		inline constexpr RangeF kClickRadialSpd = { 0.8f, 2.8f }, kClickUpSpd = { 2.2f, 4.4f };
		inline constexpr float kClickPosJitter = 0.06f, kClickDropOffsetY = 0.015f;
		inline constexpr ParticleEmitSpec kClickDropSpec = { { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f, 0.0f }, false,
			{ 0.85f, 1.0f }, { 0.024f, 0.054f }, { 0.25f, 0.25f }, true, { 0.32f, 0.55f }, 16.0f, 0.20f, 0.0f };

		inline constexpr int kClickMistCount = 6;
		inline constexpr RangeF kClickMistRadial = { 0.3f, 1.0f }, kClickMistUp = { 1.2f, 2.2f };
		inline constexpr float kClickMistOffsetY = 0.03f;
		inline constexpr ParticleEmitSpec kClickMistSpec = { { 0.96f, 0.98f, 1.0f, 1.0f }, { 0.80f, 0.94f, 1.0f, 0.0f }, true,
			{ 0.40f, 0.65f }, { 0.08f, 0.14f }, { 0.24f, 0.40f }, false, { 0.40f, 0.65f }, 1.8f, 2.2f, 0.20f };

		inline constexpr int kClickFoamCount = 3;
		inline constexpr float kClickFoamOffsetY = 0.008f, kClickFoamVelJitter = 0.15f;
		inline constexpr ParticleEmitSpec kClickFoamSpec = { { 0.96f, 0.99f, 1.0f, 1.0f }, { 0.65f, 0.88f, 1.0f, 0.0f }, true,
			{ 0.60f, 0.85f }, { 0.06f, 0.10f }, { 0.22f, 0.38f }, false, { 0.60f, 1.10f }, 0.0f, 0.0f, 0.05f };

		inline constexpr float kClickRippleLife = 1.6f, kClickRipplePower = 0.50f, kClickEmitCooldown = 0.06f;

		// B. なぞり・ドラッグ時の扇状波しぶき 
		inline constexpr float kDragMinMoveDist = 0.003f, kDragBaseInterval = 0.045f, kDragMinInterval = 0.018f, kDragMaxInterval = 0.050f;

		inline constexpr int kDragDropBase = 8;
		inline constexpr float kDragDropSpeedMul = 5.0f;
		inline constexpr int kDragDropHoldBonus = 8, kDragDropMax = 26;
		inline constexpr RangeF kDragSideSpread = { 0.5f, 1.8f }, kDragForwardSpread = { 0.1f, 1.2f }, kDragUpSpd = { 1.4f, 3.4f };
		inline constexpr float kDragUpBaseMul = 0.75f, kDragUpSpeedMul = 0.25f, kDragVelJitterXZ = 0.25f;
		inline constexpr ParticleEmitSpec kDragDropSpec = { { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f, 0.0f }, false,
			{ 0.80f, 1.0f }, { 0.020f, 0.048f }, { 0.30f, 0.30f }, true, { 0.28f, 0.48f }, 15.5f, 0.22f, 0.0f };

		inline constexpr float kDragMistFastThreshold = 1.2f;
		inline constexpr int kDragMistCountFast = 3, kDragMistCountSlow = 1;
		inline constexpr float kDragMistDeltaVelScale = 0.8f, kDragMistVelJitter = 0.35f;
		inline constexpr RangeF kDragMistUp = { 0.8f, 1.6f };
		inline constexpr float kDragMistPosJitter = 0.04f, kDragMistOffsetY = 0.02f;
		inline constexpr ParticleEmitSpec kDragMistSpec = { { 0.94f, 0.98f, 1.0f, 1.0f }, { 0.78f, 0.92f, 1.0f, 0.0f }, true,
			{ 0.35f, 0.55f }, { 0.06f, 0.11f }, { 0.18f, 0.32f }, false, { 0.32f, 0.50f }, 2.0f, 1.8f, 0.15f };

		inline constexpr float kDragFoamDeltaVelScale = 0.2f;
		inline constexpr ParticleEmitSpec kDragFoamSpec = { { 0.94f, 0.98f, 1.0f, 1.0f }, { 0.65f, 0.88f, 1.0f, 0.0f }, true,
			{ 0.50f, 0.75f }, { 0.05f, 0.09f }, { 0.12f, 0.22f }, false, { 0.50f, 0.90f }, 0.0f, 0.0f, 0.05f };
		inline constexpr float kDragRippleLife = 1.1f, kDragRipplePowerScale = 0.16f;

		// C. 静止ホバー時の水流飛沫 
		inline constexpr float kHoverEmitInterval = 0.060f;
		inline constexpr int kHoverDropCount = 3;
		inline constexpr RangeF kHoverVelX = { 0.25f, 0.65f }, kHoverVelY = { 0.9f, 1.7f };
		inline constexpr float kHoverVelJitterZ = 0.35f, kHoverPosJitter = 0.03f, kHoverOffsetY = 0.01f;
		inline constexpr ParticleEmitSpec kHoverDropSpec = { { 0.98f, 1.0f, 1.0f, 0.80f }, { 0.0f, 0.0f, 0.0f, 0.0f }, false,
			{ 0.80f, 0.80f }, { 0.016f, 0.032f }, { 0.010f, 0.010f }, false, { 0.24f, 0.40f }, 14.0f, 0.2f, 0.0f };

		inline constexpr RangeF kHoverFoamVelX = { 0.32f, 0.45f };
		inline constexpr float kHoverFoamVelJitterZ = 0.08f;
		inline constexpr ParticleEmitSpec kHoverFoamSpec = { { 0.92f, 0.97f, 1.0f, 1.0f }, { 0.70f, 0.90f, 1.0f, 0.0f }, true,
			{ 0.40f, 0.60f }, { 0.035f, 0.060f }, { 0.08f, 0.14f }, false, { 0.6f, 1.1f }, 0.0f, 0.0f, 0.04f };
	}

	// 環境パーティクル設定 (脱出煙・ビーコン・川流・きらめき・ダスト・砂埃) 
	namespace Particles
	{
		// (A) 脱出地点エメラルドグリーン信号煙 
		inline constexpr float kSmokeInterval = 0.032f, kSmokeSpreadXZ = 0.20f, kSmokeOffsetY = 0.15f;
		inline constexpr RangeF kSmokeVelX = { 0.12f, 0.28f }, kSmokeVelY = { 1.1f, 1.8f };
		inline constexpr float kSmokeVelJitterZ = 0.06f;
		inline constexpr ParticleEmitSpec kSmokeSpec = { { 0.25f, 0.95f, 0.42f, 0.65f }, { 0.22f, 0.55f, 0.32f, 1.0f }, true,
			{ 0.65f, 0.65f }, { 0.40f, 0.60f }, { 2.0f, 3.2f }, false, { 3.2f, 4.5f }, 0.0f, 0.12f, 0.35f };

		// (B) 脱出ビーコン外周の光粒 
		inline constexpr float kBeaconInterval = 0.04f, kBeaconRadius = 1.5f, kBeaconOffsetY = 0.1f;
		inline constexpr RangeF kBeaconVelY = { 1.0f, 2.2f };
		inline constexpr ParticleEmitSpec kBeaconSpec = { { 0.55f, 1.0f, 0.55f, 1.0f }, { 0.0f, 0.0f, 0.0f, 0.0f }, false,
			{ 1.0f, 1.0f }, { 0.04f, 0.08f }, { 0.02f, 0.02f }, false, { 1.2f, 2.0f }, 0.0f, 0.0f, 0.35f };

		// (C-1) 川面を流れる微細な白泡 
		inline constexpr float kRiverWaveInterval = 0.055f;
		inline constexpr RangeF kRiverWaveRangeX = { -7.5f, 6.5f };
		inline constexpr float kRiverWaveShoreMargin = 0.50f, kRiverSurfaceOffsetY = 0.008f;
		inline constexpr RangeF kRiverWaveSpeed = { 0.32f, 0.42f };
		inline constexpr float kRiverWaveJitterZ = 0.02f;
		inline constexpr ParticleEmitSpec kRiverWaveSpec = { { 0.90f, 0.96f, 1.0f, 1.0f }, { 0.65f, 0.88f, 1.0f, 0.0f }, true,
			{ 0.22f, 0.38f }, { 0.035f, 0.065f }, { 1.4f, 1.9f }, true, { 1.6f, 2.8f }, 0.0f, 0.0f, 0.04f };

		// (C-2) 川中の岩・橋脚ウェイク 
		inline constexpr float kRiverWakeInterval = 0.040f;
		struct RiverObstacle { float x; float z; float r; };
		inline constexpr int kRiverObstacleCount = 6;
		inline constexpr RiverObstacle kRiverObstacles[kRiverObstacleCount] = {
			{ -2.2f, 4.2f, 0.55f }, {  2.6f, 5.6f, 0.48f }, {  4.4f, 3.4f, 0.50f },
			{ -4.4f, 6.1f, 0.42f }, { -0.2f, 3.6f, 0.30f }, { -0.2f, 6.4f, 0.30f },
		};
		inline constexpr RangeF kWakeUpstreamRatio = { 0.45f, 0.70f };
		inline constexpr int kWakeDropBaseCount = 3, kWakeDropRandSpan = 3;
		inline constexpr RangeF kWakeDropVelX = { -0.10f, 0.45f }, kWakeDropVelY = { 0.9f, 2.1f };
		inline constexpr float kWakeDropVelJitterZ = 0.65f, kWakeDropPosJitter = 0.04f, kWakeDropOffsetY = 0.012f;
		inline constexpr ParticleEmitSpec kWakeDropSpec = { { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f, 0.0f }, false,
			{ 0.75f, 0.95f }, { 0.018f, 0.038f }, { 0.010f, 0.010f }, false, { 0.24f, 0.44f }, 14.0f, 0.20f, 0.0f };

		inline constexpr int kWakeMistChanceMod = 3;
		inline constexpr float kWakeMistOffsetY = 0.02f;
		inline constexpr RangeF kWakeMistVelX = { 0.10f, 0.35f }, kWakeMistVelY = { 0.6f, 1.2f };
		inline constexpr float kWakeMistVelJitterZ = 0.25f;
		inline constexpr ParticleEmitSpec kWakeMistSpec = { { 0.96f, 0.98f, 1.0f, 0.40f }, { 0.80f, 0.94f, 1.0f, 0.0f }, true,
			{ 0.40f, 0.40f }, { 0.05f, 0.09f }, { 0.15f, 0.26f }, false, { 0.30f, 0.50f }, 2.2f, 1.8f, 0.12f };

		inline constexpr RangeF kWakeFoamVelX = { 0.35f, 0.50f }, kWakeFoamSideZ = { 0.08f, 0.24f };
		inline constexpr ParticleEmitSpec kWakeFoamSpec = { { 0.94f, 0.98f, 1.0f, 1.0f }, { 0.70f, 0.90f, 1.0f, 0.0f }, true,
			{ 0.40f, 0.65f }, { 0.035f, 0.065f }, { 0.070f, 0.120f }, false, { 0.8f, 1.5f }, 0.0f, 0.0f, 0.05f };

		// (C-3) 両岸さざ波 
		inline constexpr float kShoreFoamInterval = 0.075f;
		inline constexpr RangeF kShoreFoamRangeX = { -7.5f, 7.0f }, kShoreFoamOffsetZ = { 0.10f, 0.25f }, kShoreFoamVelX = { 0.25f, 0.35f };
		inline constexpr ParticleEmitSpec kShoreFoamSpec = { { 0.90f, 0.96f, 1.0f, 1.0f }, { 0.70f, 0.90f, 1.0f, 0.0f }, true,
			{ 0.25f, 0.45f }, { 0.025f, 0.045f }, { 0.050f, 0.085f }, false, { 0.8f, 1.5f }, 0.0f, 0.0f, 0.03f };

		// (C-4) 水面の陽光きらめき 
		inline constexpr float kGlintInterval = 0.040f;
		inline constexpr RangeF kGlintRangeX = { -7.0f, 6.0f };
		inline constexpr float kGlintShoreMargin = 0.6f;
		inline constexpr RangeF kGlintVelX = { 0.32f, 0.40f }, kGlintVelY = { 0.05f, 0.15f };
		inline constexpr ParticleEmitSpec kGlintSpec = { { 1.0f, 0.98f, 0.92f, 1.0f }, { 0.0f, 0.0f, 0.0f, 0.0f }, false,
			{ 0.50f, 0.80f }, { 0.015f, 0.028f }, { 0.005f, 0.005f }, false, { 0.18f, 0.32f }, 2.0f, 0.1f, 0.0f };

		// (D) 陽光ダストモート 
		inline constexpr float kAmbientDustInterval = 0.045f;
		inline constexpr BoxRange3D kAmbientDustBox = { { -6.0f, 6.0f }, { -0.2f, 3.0f }, { 0.0f, 14.0f } };
		inline constexpr RangeF kAmbientDustVelX = { 0.08f, 0.18f }, kAmbientDustVelY = { -0.02f, 0.05f };
		inline constexpr float kAmbientDustVelJitterZ = 0.04f;
		inline constexpr ParticleEmitSpec kAmbientDustSpec = { { 1.0f, 0.94f, 0.75f, 0.40f }, { 0.0f, 0.0f, 0.0f, 0.0f }, false,
			{ 0.40f, 0.40f }, { 0.020f, 0.045f }, { 0.015f, 0.015f }, false, { 3.0f, 5.0f }, 0.0f, 0.0f, 0.20f };

		// (E) 地面の微小土煙・砂埃 
		inline constexpr float kGroundDustInterval = 0.14f, kGroundDustOffsetY = 0.02f;
		inline constexpr RangeF kGroundDustNearX = { 0.8f, 5.2f }, kGroundDustNearZ = { -1.6f, 2.2f };
		inline constexpr RangeF kGroundDustFarX = { -3.5f, 4.5f }, kGroundDustFarZ = { 8.5f, 15.0f };
		inline constexpr RangeF kGroundDustVelX = { 0.18f, 0.42f }, kGroundDustVelY = { 0.06f, 0.16f };
		inline constexpr float kGroundDustVelJitterZ = 0.06f;
		inline constexpr ParticleEmitSpec kGroundDustSpec = { { 0.76f, 0.70f, 0.58f, 1.0f }, { 0.72f, 0.66f, 0.54f, 0.0f }, true,
			{ 0.16f, 0.30f }, { 0.04f, 0.08f }, { 0.14f, 0.26f }, false, { 0.9f, 1.7f }, 0.0f, 0.0f, 0.05f };
	}

	// 3D舞い散る木の葉メッシュ・発生源ボックス・パレット・物理設定 
	namespace Leaves
	{
		inline constexpr size_t kMaxFallingLeaves = 24;
		inline constexpr float kBoundingRadius = 1.0f;
		inline constexpr Vector3 kDefaultWobbleDir = { -0.38f, 0.0f, 0.92f };
		inline constexpr float kDefaultScale = 0.32f, kDefaultMaxLife = 8.0f, kDefaultWobbleFreq = 3.0f, kDefaultWobbleAmp = 0.6f;

		struct LeafVertexSpec { Vector4 position; Vector2 texcoord; Vector3 normal; };
		inline constexpr LeafVertexSpec kDoubleSidedQuadVertices[] = {
			// 表側ポリゴン (+Z法線) 
			{ { -0.5f,  0.5f, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f,  1.0f } },
			{ {  0.5f,  0.5f, 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f,  1.0f } },
			{ { -0.5f, -0.5f, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f,  1.0f } },
			{ {  0.5f, -0.5f, 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f,  1.0f } },
			// 裏側ポリゴン (-Z法線) 
			{ { -0.5f,  0.5f, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
			{ {  0.5f,  0.5f, 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
			{ { -0.5f, -0.5f, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } },
			{ {  0.5f, -0.5f, 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } },
		};
		inline constexpr uint32_t kDoubleSidedQuadIndices[] = { 0u, 1u, 2u, 1u, 3u, 2u, 5u, 4u, 7u, 4u, 6u, 7u };

		inline constexpr int kSpawnBoxCount = 4;
		inline constexpr BoxRange3D kSpawnBoxes[kSpawnBoxCount] = {
			{ { -7.5f, -4.5f }, { 2.2f, 3.8f }, { 3.5f,  7.5f } }, // 左手手前の広葉樹 
			{ { -5.0f, -2.0f }, { 1.6f, 2.8f }, { 1.0f,  4.5f } }, // 左手近景の茂み 
			{ { -6.5f, -2.0f }, { 2.2f, 4.0f }, { 5.0f,  9.0f } }, // 渡河橋上流側の木立 
			{ { -9.0f, -3.5f }, { 2.6f, 4.8f }, { 8.5f, 13.0f } }, // 左手奥の樹冠 
		};

		inline constexpr int kPaletteCount = 5;
		inline constexpr Vector4 kColorPalettes[kPaletteCount] = {
			{ 1.00f, 1.00f, 1.00f, 1.0f }, // 基準グリーン 
			{ 1.06f, 1.14f, 0.90f, 1.0f }, // 若葉・ライムグリーン 
			{ 0.78f, 0.90f, 0.72f, 1.0f }, // 深緑・フォレストグリーン 
			{ 0.88f, 1.08f, 0.84f, 1.0f }, // エメラルドグリーン 
			{ 1.08f, 1.06f, 0.76f, 1.0f }, // 日向のオリーブグリーン 
		};

		inline constexpr float kGustThreshold = 0.68f, kIntervalGusting = 0.45f, kIntervalCalm = 1.35f;
		inline constexpr RangeF kLife = { 7.5f, 11.0f }, kWindSpeed = { 1.6f, 2.6f };
		inline constexpr float kWindGustBaseMul = 0.82f, kWindGustExtraMul = 0.32f, kWindVelJitterXZ = 0.15f;
		inline constexpr RangeF kFallVelY = { -0.32f, -0.16f };
		inline constexpr float kFallGustExtraY = 0.06f;
		inline constexpr RangeF kWobbleFreq = { 2.2f, 3.8f }, kWobbleAmp = { 0.40f, 0.85f };
		inline constexpr float kInitRollRange = 0.4f;
		inline constexpr RangeF kRotPitch = { 1.4f, 3.2f }, kRotYaw = { 0.8f, 2.0f };
		inline constexpr float kRotRollRange = 0.3f;
		inline constexpr RangeF kScale = { 0.28f, 0.40f };
		inline constexpr float kDespawnWaterMarginY = 0.02f, kDespawnMaxX = 8.0f, kSwayLiftVelAmp = 0.08f, kSwayRollGain = 0.70f;
	}

	// 2DタイトルスプライトUIレイアウト・アニメーション・カラーパレット 
	namespace UI
	{
		inline constexpr float kDefaultScreenW = 1280.0f, kDefaultScreenH = 720.0f;
		inline constexpr Vector2 kTopLeftAnchor = { 0.0f, 0.0f }, kCenterAnchor = { 0.5f, 0.5f };

		inline constexpr Vector4 kVignetteColor = { 0.10f, 0.12f, 0.15f, 0.20f }, kLogoGhostRgb = { 1.0f, 0.8f, 0.35f, 0.0f };
		inline constexpr Vector4 kTransparentWhite = { 1.0f, 1.0f, 1.0f, 0.0f }, kOpaqueBlack = { 0.0f, 0.0f, 0.0f, 1.0f };

		// グリントスプライトシート (4x4 セル) 
		inline constexpr int kShineCols = 4, kShineRows = 4, kShineFlashCell = kShineCols * kShineRows - 1;
		inline constexpr int kShineSweepFrames = kShineFlashCell;
		inline constexpr float kLogoTexW = 1000.0f, kLogoTexH = 250.0f, kPromptTexW = 700.0f, kPromptTexH = 100.0f;

		// タイトルロゴ & ダッシュ残像 
		inline constexpr int kLogoGhostCount = 2;
		inline constexpr Vector2 kLogoFinalPos = { 48.0f, 34.0f }, kLogoSize = { 580.0f, 145.0f };
		inline constexpr float kLogoInitOffscreenMul = 2.0f, kLogoDashExtraOffsetX = 60.0f, kLogoFloatFreq = 1.8f;
		inline constexpr float kLogoFloatAmpY = 3.0f, kLogoFloatFadeTime = 0.6f, kLogoStretchX = 0.18f;
		inline constexpr float kLogoSquashY = 0.10f, kLogoSpringDecay = 7.0f, kLogoSpringFreq = 22.0f;
		inline constexpr float kLogoSpringAmpX = 0.07f, kLogoSpringAmpY = 0.09f, kLogoAlphaFadeSpeed = 4.0f;

		inline constexpr float kGhostLagStep = 0.12f;
		inline constexpr float kGhostBaseAlpha[kLogoGhostCount] = { 0.50f, 0.28f };
		inline constexpr float kGhostFadeInSpeed = 5.0f, kGhostFadeOutStart = 0.55f, kGhostFadeOutSpan = 0.45f;
		inline constexpr float kGhostScaleX = 1.22f, kGhostScaleY = 0.94f;

		inline constexpr float kLogoStartFlashDuration = 0.30f, kLogoLandFlashDuration = 0.28f, kLogoShineDelay = 0.9f;
		inline constexpr float kLogoShinePeriod = 3.8f, kLogoShineDuration = 0.55f;

		// PRESS SPACE プロンプト & 開幕フェード 
		inline constexpr float kPromptCenterOffsetX = 235.0f, kPromptBaseX = kLogoFinalPos.x + kPromptCenterOffsetX, kPromptPosYRatio = 0.88f;
		inline constexpr Vector2 kPromptSize = { 470.0f, 66.0f };
		inline constexpr float kPromptAppearDuration = 0.45f, kPromptPulseFreq = 3.2f, kPromptPulseScaleAmp = 0.025f;
		inline constexpr float kPromptSlideOffsetX = 80.0f, kPromptAlphaBase = 0.82f, kPromptAlphaPulse = 0.18f;
		inline constexpr float kPromptShineDelay = 0.6f, kPromptShinePeriod = 2.6f, kPromptShineDuration = 0.42f;
		inline constexpr float kPromptPopScaleAmp = 0.16f, kPromptPopDuration = 0.18f, kPromptFadeOutDelay = 0.15f;
		inline constexpr float kPromptFadeOutDuration = 0.35f, kPromptStartFlashDuration = 0.30f, kIntroFadeDuration = 0.8f;
	}
}
