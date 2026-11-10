#pragma once
#include <cstddef>
#include <cstdint>
#include "Application/Config/GameConfig.h"
#include "Object3d.h"

namespace EnemyTuning
{
    // 数学定数
    inline constexpr float kPi = GameConfig::Math::kPi, kTwoPi = GameConfig::Math::kTwoPi, kDegToRad = GameConfig::Math::kDegToRad;

    // ステージ・NavMesh歩行可能境界
    inline constexpr float kArenaMinX = -32.5f, kArenaMaxX = 32.5f, kArenaMinZ = -10.5f, kArenaMaxZ = 67.5f;
    inline constexpr float kBridgeCenterAbsX = 14.0f, kBridgeHalfWidth = 1.65f, kRiverMinZ = 27.3f, kRiverMaxZ = 32.7f;
    inline constexpr float kStepWalkableRadius = 0.62f, kQueryWalkableRadius = 0.65f;
    inline constexpr float kDefaultSearchRadius = 8.0f, kWaypointSnapRadius = 4.0f, kWalkableGridStep = 0.5f, kLargeInitDist = 999999.0f;

    // Enemy 初期パラメータ・コライダー・基本ステータス
    inline constexpr float kMovingHorizSpeedThreshold = 0.35f, kDefaultFaceDeltaTime = 0.016f, kDefaultHitFlashDuration = 0.10f;
    inline constexpr int kDefaultMaxHp = 5, kDefaultMagAmmo = 6;
    inline constexpr float kDefaultRespawnDuration = 30.0f, kDefaultReloadDuration = 1.65f;
    inline constexpr Vector3 kDefaultBulletSpawnOffset = { 0.36f, 0.62f, 0.72f };
    inline constexpr float kDefaultBulletSpeed = 0.44f, kDefaultBulletLifeTime = 2.2f, kDefaultShotCooldown = 1.45f, kDefaultAimTelegraphDur = 0.40f;
    inline constexpr float kDefaultFovDegrees = 72.0f, kDefaultFovRadians = kDefaultFovDegrees * kDegToRad, kDefaultMaxSightRange = 12.5f;
    inline constexpr float kPartBoundingRadius = 4.0f, kInitialSpawnZ = 18.0f, kIdleWaddlePhase = 0.15f;
    inline constexpr float kColliderRadius = 0.50f, kColliderOffsetY = 0.50f, kInitialShotCooldown = 0.75f;

    // SetPosition 初期位相・十字砲火角度
    inline constexpr float kSideLaneThresholdX = 2.2f, kInwardCrossfireAngle = 0.28f;
    inline constexpr float kScanPhaseSeedX = 1.73f, kScanPhaseSeedZ = 0.91f, kStrafeSignSeedX = 2.3f, kStrafeSignSeedZ = 1.1f;
    inline constexpr float kShotCdSeedBase = 0.45f, kShotCdSeedX = 0.37f, kShotCdSeedZ = 0.23f, kShotCdSeedMod = 0.75f;

    // 3Dライフルプロシージャルメッシュ定義
    struct BoxBoundsSpec { Vector3 minP; Vector3 maxP; };
    inline constexpr size_t kBoxCornerCount = 8, kBoxFaceCount = 6, kBoxFaceVertCount = 4;
    inline constexpr uint32_t kBoxLastCorner = 3;

    inline constexpr BoxBoundsSpec kEnemyRifleBoxes[] = {
        { { -0.038f, -0.040f, -0.20f }, { 0.038f,  0.055f,  0.20f } }, // 機関部レシーバー
        { { -0.018f,  0.008f,  0.20f }, { 0.018f,  0.038f,  0.68f } }, // ロングバレル
        { { -0.024f, -0.230f,  0.03f }, { 0.024f, -0.040f,  0.14f } }, // 30連マガジン
        { { -0.032f, -0.075f, -0.46f }, { 0.032f,  0.035f, -0.20f } }, // ストック
        { { -0.028f, -0.026f,  0.20f }, { 0.028f,  0.048f,  0.44f } }, // ハンドガード
        { { -0.016f,  0.055f,  0.34f }, { 0.016f,  0.088f,  0.42f } }, // タクティカルレーザーモジュール
        { { -0.022f,  0.002f,  0.68f }, { 0.022f,  0.044f,  0.75f } }, // マズルブレーキ
    };
    inline constexpr float kRifleBoundingRadius = 0.95f, kRifleTelegraphColorThresh = 0.65f;
    inline constexpr Vector4 kRifleDefaultColor = { 0.24f, 0.22f, 0.21f, 1.0f }, kRifleTelegraphColor = { 0.42f, 0.20f, 0.20f, 1.0f };

    // 戦術ロール別ステータステーブル（Garrison, Patrol, Flanker, Overwatch, Rookie, Warden）
    struct RoleStatsSpec
    {
        int maxHp; float roleScale, maxSightRange, fovAngle, aimTelegraphDuration, bulletSpeed, shotCooldown;
        int burstCount, maxMagAmmo; Vector4 baseBodyTint;
    };
    inline constexpr size_t kRoleCount = 6;
    inline constexpr RoleStatsSpec kRoleStats[kRoleCount] = {
        {  5, 1.00f, 11.0f, 1.30f, 0.48f, 0.42f, 1.55f, 2, 6, { 1.00f, 1.00f, 1.00f, 1.0f } }, // Garrison
        {  4, 1.00f, 10.5f, 1.28f, 0.50f, 0.40f, 1.50f, 2, 6, { 0.96f, 0.98f, 1.00f, 1.0f } }, // Patrol
        {  4, 1.00f, 12.0f, 1.42f, 0.45f, 0.43f, 1.35f, 2, 6, { 1.00f, 0.86f, 0.82f, 1.0f } }, // Flanker
        {  4, 1.02f, 13.5f, 0.96f, 0.62f, 0.50f, 1.85f, 1, 4, { 0.90f, 0.95f, 0.82f, 1.0f } }, // Overwatch
        {  3, 0.96f, 10.0f, 1.22f, 0.62f, 0.36f, 1.90f, 1, 5, { 0.92f, 0.97f, 1.00f, 1.0f } }, // Rookie
        { 10, 1.25f,  8.6f, 1.45f, 0.48f, 0.45f, 1.30f, 3, 9, { 1.00f, 0.54f, 0.48f, 1.0f } }, // Warden
    };

    // 銃口・旋回・武器状態定数
    inline constexpr float kMinTelegraphDurEps = 0.01f, kMuzzleRightOffset = 0.38f, kMuzzleUpOffset = 0.64f, kMuzzleFwdOffset = 0.86f;
    inline constexpr float kFaceTargetMinLenSq = 1e-6f, kPatrolTurnSpeed = 3.2f, kCombatTurnSpeed = 11.5f, kFaceAlignedTolRad = 0.28f;
    inline constexpr float kPostReloadShotDelay = 0.30f, kRifleRecoilDecayRate = 5.5f, kBurstAbortShotCooldown = 0.65f, kAimTelegraphDecayMult = 2.2f;

    // 歩行・転倒・被弾ポーズアニメーション定数
    inline constexpr float kPoseMinDeltaTime = 1e-5f, kPoseMinMoveDist = 1e-5f, kLocalMoveNormEps = 1e-4f;
    inline constexpr float kHorizSpeedKeepWeight = 0.72f, kHorizSpeedNewWeight = 0.28f;
    inline constexpr float kLocalMoveKeepWeight = 0.68f, kLocalMoveNewWeight = 0.32f;
    inline constexpr float kWalkFullSpeedRef = 2.2f, kWalkActiveRatioMin = 0.04f, kWaddleBaseRate = 11.2f, kWaddleSpeedRate = 5.2f;
    inline constexpr Vector4 kHitFlashColor = { 3.8f, 3.8f, 3.8f, 1.0f };
    inline constexpr size_t kLimbCount = 4;

    inline constexpr float kDeathHopArcHeight = 0.16f, kDeathDropOffset = 0.12f, kDeathPitchMax = -1.38f, kDeathRollMax = 0.42f;
    inline constexpr float kDeathShrinkStartRatio = 0.72f, kDeathShrinkSpan = 0.28f, kDeathFootOffsetY = 0.02f;
    inline constexpr float kDeathLeftFootPitch = 0.35f, kDeathRightFootPitch = 0.30f, kDeathFootYaw = 0.18f, kDeathFootRoll = 0.10f;
    inline constexpr float kDeathWingPitch = 0.15f, kDeathWingFlap = 0.46f;
    inline constexpr Vector3 kRifleBaseLocalOffset = { 0.38f, 0.62f, 0.22f };

    inline constexpr float kBobHeightAmp = 0.072f, kStepSquashAmp = 0.028f, kMoveFwdPitchScale = 0.088f, kRecoilBodyPitchScale = 0.04f;
    inline constexpr float kWaddleRollAmp = 0.118f, kMoveRightRollScale = 0.065f;
    inline constexpr float kBaseFlinchDuration = 0.18f, kWardenFlinchDuration = 0.09f, kFlinchPitchScale = 0.24f, kFlinchRollScale = 0.16f;
    inline constexpr float kSquashXZScale = 0.70f, kSquashYScale = 0.92f, kFootStrideLength = 0.225f, kFootLiftHeight = 0.118f;
    inline constexpr float kLeftWingSwingAmp = 0.26f, kLeftWingBaseFlap = 0.04f, kLeftWingWalkFlap = 0.16f, kLeftWingHopFlap = 0.14f;
    inline constexpr float kReloadLeftWingPitch = -0.58f, kReloadLeftWingYaw = -0.28f, kReloadLeftWingFlap = 0.22f;
    inline constexpr float kRightWingHoldPitch = -0.16f, kRightWingBaseFlap = 0.05f, kRightWingHopFlap = 0.04f;
    inline constexpr float kReloadRifleTiltPitch = 0.48f, kRifleRecoilPitchScale = 0.16f, kRifleRecoilBackScale = 0.09f, kRifleBobFollowFactor = 0.35f;

    // AI索敵・物理減衰・戦闘判断定数
    inline constexpr float kHpLagDecayRate = 1.35f, kDeadKnockbackDamp = 8.0f, kLiveKnockbackMinVel = 0.01f, kLiveKnockbackDamp = 10.0f;
    inline constexpr float kInitFarPlayerDist = 9999.0f, kCoverSightMultiplier = 0.60f, kStaticChaseSightMult = 1.35f, kMovingChaseSightMult = 1.45f;
    inline constexpr float kMinSightDistEps = 1e-4f, kDetectSpeedInCover = 0.72f, kDetectSpeedOpen = 1.15f;
    inline constexpr float kCloseDetectDistance = 5.5f, kCloseDetectMultiplier = 1.6f;
    inline constexpr float kStaticChaseAlertDur = 0.45f, kMovingChaseAlertDur = 0.42f, kSpotMaxShotCooldown = 0.35f;
    inline constexpr float kStaticSearchDuration = 4.2f, kMovingSearchDuration = 4.5f, kLostSightAlertDur = 0.8f;
    inline constexpr float kLostSightMeterRetain = 0.65f, kDetectionDecayRate = 0.50f;

    // 定点歩哨 Patrol / Investigate / Chase 定数
    inline constexpr float kOverwatchScanArc = 0.36f, kDefaultScanArc = 0.56f, kPatrolScanFreq = 1.15f, kPatrolMaxTurnRate = 2.6f;
    inline constexpr float kIdleShiftFreq = 0.55f, kOverwatchIdleShift = 0.30f, kDefaultIdleShift = 0.65f;
    inline constexpr float kHomeReturnDeadzone = 0.12f, kHomeReturnMaxSpeed = 1.35f, kHomeReturnGain = 2.5f;
    inline constexpr float kInvestigateScanFreq = 2.8f, kInvestigateScanRadius = 1.5f;
    inline constexpr float kMinCombatTargetDist = 0.2f, kDuckToAnchorShotCd = 0.85f, kAnchorReturnDeadzone = 0.20f, kAnchorReturnSpeed = 2.15f;
    inline constexpr float kPeekOutFlipPeriod = 1.35f, kPeekOutSpeed = 2.10f, kPeekOutMaxRadiusSq = 2.65f * 2.65f;
    inline constexpr float kCombatStrafeFlipPeriod = 1.75f, kOverwatchAimStrafeSpd = 0.15f, kDefaultAimStrafeSpd = 0.45f;
    inline constexpr float kFreeCombatStrafeSpd = 1.25f, kCombatStrafeMaxRadSq = 2.40f * 2.40f;

    // 視線判定・聴覚・無線・射撃・被弾リアクション定数
    inline constexpr float kEyeHeightBase = 0.55f, kLosMinDistSq = 1e-8f, kEarHeightY = 1.65f, kHearingMaxDistSq = 11.5f * 11.5f;
    inline constexpr size_t kLosSampleCount = 3;
    inline constexpr int kLosRequiredHits = 2;
    inline constexpr float  kLosTargetYOffsets[kLosSampleCount] = { 0.75f, 0.42f, 0.18f };
    inline constexpr float kNoiseSearchDuration = 4.0f, kNoiseAlertDuration = 0.9f, kNoiseMinDetectionMeter = 0.45f;
    inline constexpr float kSquadAlertDuration = 0.55f, kSquadMinShotCooldown = 0.45f;
    inline constexpr float kPreShotFaceDeltaTime = 0.05f, kOverwatchSpreadRad = 0.015f, kDefaultSpreadRad = 0.042f;
    inline constexpr float kMuzzleWallCheckDist = 1.1f, kMuzzleBlockedCooldown = 0.75f, kWardenBurstInterval = 0.11f, kDefaultBurstInterval = 0.15f;
    inline constexpr float kWardenKnockbackMult = 0.45f, kHitKnockbackSpeed = 2.8f, kDeathKnockbackSpeed = 4.2f;
    inline constexpr float kWardenAimSuppress = 0.06f, kDefaultAimSuppress = 0.18f;
    inline constexpr float kEvadeCooldownDuration = 2.2f, kEvadeStepDuration = 0.24f, kEvadeStepSpeed = 4.2f;
    inline constexpr float kHitDirectAlertMaxDist = 26.0f, kHitMaxAlertPopupTime = 0.22f;
    inline constexpr float kFarHitSearchDuration = 4.5f, kFarHitAlertDuration = 0.6f, kFarHitDetectionMeter = 0.85f;

    // MovingEnemy 専用巡回・経路追従・側面機動定数
    inline constexpr float kDefaultSplineSpeed = 2.2f, kDefaultSimpleSpeed = 0.035f, kDefaultMoveSpeed = 0.042f, kFlankerMoveSpeed = 0.054f;
    inline constexpr Vector3 kDefaultPatrolPointA = { -5.0f, 0.0f, 26.0f }, kDefaultPatrolPointB = { 5.0f, 0.0f, 26.0f };
    inline constexpr int kSplineDistTableSamples = 20, kLowHpCoverThreshold = 2;
    inline constexpr float kReferenceFps = 60.0f, kNavRepathInterval = 0.35f, kWaypointReachRadiusSq = 0.48f * 0.48f, kMinNavStepDist = 0.05f;
    inline constexpr float kRejoinSpeedMult = 0.90f, kRejoinDirectDist = 0.75f, kPauseSweepFreq = 3.4f, kPauseSweepAmp = 0.65f;
    inline constexpr float kSplineDriftMaxDistSq = 0.85f * 0.85f, kEndpointMinSplineLen = 1.5f, kEndpointDistMargin = 0.22f;
    inline constexpr float kEndpointPauseDuration = 0.85f, kEndpointPauseCooldown = 2.8f;
    inline constexpr float kSimpleDestReachDist = 0.35f, kSimplePatrolSpeedMult = 0.85f, kInvestigateStopDist = 1.35f;
    inline constexpr float kSplineNearestSampleStep = 0.25f, kMinTargetDistClamp = 0.001f;
    inline constexpr float kCoverAdvanceSpeedMult = 1.08f, kInvalidCoverIgnoreDur = 4.0f;
    inline constexpr float kFlankerStrafePeriod = 1.65f, kPatrolStrafePeriod = 2.15f;
    inline constexpr float kFlankerMaxOptDist = 6.6f, kPatrolMaxOptDist = 7.8f, kFlankerMinOptDist = 3.1f, kPatrolMinOptDist = 3.8f;
    inline constexpr float kFlankerLateralSpread = 4.2f, kPatrolLateralSpread = 2.5f, kFlankerStandOffDist = 2.4f, kPatrolStandOffDist = 4.5f;
    inline constexpr float kFlankerAdvanceMult = 1.12f, kPatrolAdvanceMult = 0.96f;
    inline constexpr float kRetreatSpeedMult = 0.85f, kRetreatBackWeight = 0.8f, kRetreatSideWeight = 0.6f;
    inline constexpr float kAimingMoveStrafeMult = 0.38f, kFlankerCircleMult = 0.95f, kPatrolCircleMult = 0.82f;
}
