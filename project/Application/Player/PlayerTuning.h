#pragma once
#include "DuckPartRig.h"

/// <summary>
/// プレイヤー挙動・プロシージャル武器メッシュ・アヒル関節アニメーション・照準物理定数定義
/// </summary>
namespace PlayerTuning
{
    inline constexpr int kDuckLimbCount = 4, kMatW = 3;
    inline constexpr float kSmoothStepCubicBias = 3.0f, kReferenceFps = 60.0f, kMaxPhysicsStepDt = 0.05f;
    inline constexpr float kRayParallelEpsilon = 1e-6f, kSmallLenEpsilon = 1e-4f, kReticleScreenMarginPx = 4.0f;

    // 軍用アサルトライフル 3Dメッシュ・保持オフセット定数
    namespace Weapon
    {
        inline constexpr DuckPartRig::BoxBounds kRifleBaseBoxes[] = {
            { { -0.038f, -0.032f, -0.20f  }, {  0.038f,  0.058f,  0.16f  } }, // アッパー＆ロアレシーバー
            { { -0.024f,  0.058f, -0.18f  }, {  0.024f,  0.070f,  0.44f  } }, // フルレングス・ピカティニーレール
            { { -0.032f, -0.068f,  0.015f }, { -0.022f, -0.028f,  0.125f } }, // フレアマガジンウェル左壁
            { {  0.022f, -0.068f,  0.015f }, {  0.032f, -0.028f,  0.125f } }, // フレアマガジンウェル右壁
            { { -0.032f, -0.068f,  0.015f }, {  0.032f, -0.028f,  0.027f } }, // フレアマガジンウェル後壁
            { { -0.032f, -0.068f,  0.113f }, {  0.032f, -0.028f,  0.125f } }, // フレアマガジンウェル前壁
            { { -0.022f, -0.155f, -0.155f }, {  0.022f, -0.030f, -0.085f } }, // ピストルグリップ
            { { -0.012f, -0.075f, -0.085f }, {  0.012f, -0.062f,  0.018f } }, // トリガーガード
            { { -0.020f,  0.002f, -0.49f  }, {  0.020f,  0.042f, -0.20f  } }, // バッファチューブ
            { { -0.036f, -0.105f, -0.50f  }, {  0.036f,  0.046f, -0.35f  } }, // SOPMOD クレーンストック
            { { -0.031f, -0.026f,  0.16f  }, {  0.031f,  0.054f,  0.45f  } }, // M-LOK ハンドガード
            { { -0.016f,  0.042f,  0.47f  }, {  0.016f,  0.082f,  0.51f  } }, // ガスブロック / フロントサイト
            { { -0.016f,  0.012f,  0.45f  }, {  0.016f,  0.042f,  0.66f  } }, // ヘビーバレル
            { { -0.023f,  0.006f,  0.66f  }, {  0.023f,  0.048f,  0.73f  } }, // 2チャンバー・マズルブレーキ
            { { -0.025f,  0.070f, -0.09f  }, {  0.025f,  0.122f,  0.07f  } }, // ホログラフィックサイト
        };
        inline constexpr DuckPartRig::BoxBounds kRifleMagazineBoxes[] = {
            { { -0.022f, -0.115f,  0.045f }, {  0.022f, -0.040f,  0.115f } }, // 3段湾曲マガジン上段
            { { -0.022f, -0.185f,  0.060f }, {  0.022f, -0.110f,  0.130f } }, // 3段湾曲マガジン中段
            { { -0.025f, -0.245f,  0.082f }, {  0.025f, -0.180f,  0.154f } }, // 3段湾曲マガジン下段
        };
        inline constexpr float kRifleBoundingRadius = 0.95f, kMagazineBoundingRadius = 0.85f;
        inline constexpr float kMagazineReflection = 0.14f, kMagazineFresnelF0 = 0.28f;
        inline constexpr Vector4 kRifleGunmetalColor = { 0.21f, 0.22f, 0.24f, 1.0f };
        inline constexpr Vector3 kHipAimOffset = { 0.42f, 0.68f, 0.25f };
        inline constexpr Vector3 kHolsterSlingOffset = { -0.10f, 0.72f, -0.28f }, kHolsterSlingEuler = { 0.52f, 0.35f, 0.88f };
        inline constexpr float kRifleRecoilDecayRate = 3.5f, kRifleRecoilFwdKick = 0.08f, kRifleRecoilPitchKick = 0.06f;
        inline constexpr float kMagwellLocalY = -0.09f, kMagwellLocalZ = 0.075f;
    }

    // 効果音ボリューム・クールダウン ＆ 被弾カラー演出定数
    namespace Audio
    {
        inline constexpr float kHealStartVolume = 0.68f, kHealCompleteVolume = 0.72f;
        inline constexpr float kHolsterToggleVolume = 0.62f, kUnholsterAimVolume = 0.48f, kDryFireVolume = 0.68f;
        inline constexpr int kHolsterToggleCooldownMs = 90, kUnholsterAimCooldownMs = 120, kDryFireCooldownMs = 110;
    }

    namespace Visual
    {
        inline constexpr Vector4 kDefaultBodyColor = { 1.0f, 1.0f, 1.0f, 1.0f }, kHitInitialFlashColor = { 5.0f, 5.0f, 5.0f, 1.0f };
        inline constexpr Vector4 kHitActiveFlashColor = { 4.5f, 0.35f, 0.35f, 1.0f }, kInvincibleBlinkColor = { 1.0f, 0.3f, 0.3f, 0.5f };
        inline constexpr Vector4 kDeadBodyDimColor = { 0.2f, 0.2f, 0.2f, 1.0f }, kDeadLimbTint = { 0.95f, 0.95f, 0.98f, 1.0f };
        inline constexpr int kInvincibleBlinkFrames = 5;
        inline constexpr float kHitShakeIntensity = 0.40f, kHitShakeLiftY = 0.05f, kHitFlinchPitch = -0.45f;
    }

    // 照準レイ補正・レティクル物理リコイル定数
    namespace Aim
    {
        inline constexpr float kCloseBlendStartDist = 0.50f, kCloseBlendRange = 0.85f;
        inline constexpr float kMaxRightOffsetDistRatio = 0.92f, kMaxSinArg = 0.98f, kAimRayMinDist = 0.18f, kAimRayMaxDist = 60.0f;
        inline constexpr float kRecoilSubstepThresholdDt = 0.018f, kRecoilBoundaryVelDamping = 0.85f, kRecoilRestOffsetPx = 0.04f;
        inline constexpr float kRecoilCoolReadyTime = 0.005f, kRecoilHeatCoolRate = 2.2f;
        inline constexpr float kRecoilKickVisualDecay = 5.2f, kRecoilVisualNormOffsetPx = 36.0f;
        inline constexpr float kClimbAimFollowX = 0.45f, kClimbBaseScreenUpY = -0.85f, kClimbAimFollowY = 0.20f;
        inline constexpr float kSideFlipGuaranteedHeat = 0.30f, kSideKeepProb = 0.28f, kRecoilMovingMultiplier = 1.25f;
        inline constexpr float kRecoilJitterMin = 0.76f, kRecoilJitterMax = 1.24f, kRecoilHeatPerShot = 0.22f;
        inline constexpr float kHeatClimbSnapAdd = 4.0f, kHeatLatSnapAdd = 7.8f, kHeatClimbVelAdd = 95.0f, kHeatLatVelAdd = 220.0f;
    }

    // アヒル歩行・ダッシュ・ポーズプリセット定数
    namespace Anim
    {
        struct LimbPoseSpec { Vector3 pivot; Vector3 extraOffset; Vector3 jointRot; };

        inline constexpr LimbPoseSpec kDeadLimbPoses[kDuckLimbCount] = {
            { DuckPartRig::kLeftFootPivot,  { 0.0f, 0.02f, 0.0f }, {  0.35f, -0.22f,  0.12f } },
            { DuckPartRig::kRightFootPivot, { 0.0f, 0.02f, 0.0f }, {  0.28f,  0.24f, -0.10f } },
            { DuckPartRig::kLeftWingPivot,  { 0.0f, 0.0f,  0.0f }, {  0.18f,  0.0f,  -0.48f } },
            { DuckPartRig::kRightWingPivot, { 0.0f, 0.0f,  0.0f }, {  0.15f,  0.0f,   0.42f } },
        };
        inline constexpr LimbPoseSpec kIntroLimbPoses[kDuckLimbCount] = {
            { DuckPartRig::kLeftFootPivot,  { 0.0f, 0.0f,  0.0f }, {  0.0f,  -0.05f,  0.0f  } },
            { DuckPartRig::kRightFootPivot, { 0.0f, 0.0f,  0.0f }, {  0.0f,   0.05f,  0.0f  } },
            { DuckPartRig::kLeftWingPivot,  { 0.0f, 0.0f,  0.0f }, {  0.0f,   0.0f,  -0.06f } },
            { DuckPartRig::kRightWingPivot, { 0.0f, 0.0f,  0.0f }, { -0.14f,  0.0f,   0.05f } },
        };
        inline constexpr float kInitialWalkPhase = 0.15f, kIdleAnimWrapSec = 1000.0f;
        inline constexpr float kWalkBlendInRate = 15.0f, kWalkBlendOutRate = 11.5f, kSprintBlendRate = 12.0f, kBlendSnapEpsilon = 0.002f;
        inline constexpr float kMoveDirMinLenSq = 0.01f, kHolsterRunTurnRate = 14.5f, kLocalMoveDirBlendRate = 18.0f;
        inline constexpr float kFootstepMinWalkBlend = 0.02f, kWalkCadenceBase = 12.8f, kWalkCadenceSprintAdd = 4.4f;
        inline constexpr float kWalkCadenceMinFactor = 0.45f, kWalkCadenceBlendFactor = 0.55f;
        inline constexpr float kDustStrideBase = 0.225f, kVisualStrideBase = 0.230f, kStrideSprintAdd = 0.095f;
        inline constexpr float kFootLiftBase = 0.122f, kFootLiftSprintAdd = 0.050f;
        inline constexpr float kDodgeLandFlareStart = 0.72f, kDodgeLandFlareSpan = 0.28f;
        inline constexpr float kDodgeFootTuckY = 0.11f, kDodgeFootTuckZ = -0.05f;
        inline constexpr float kDodgeWingTuckFlare = 0.08f, kDodgeWingLandFlare = 0.56f;
        inline constexpr float kDodgeFootPitch = 0.52f, kDodgeFootToeYaw = 0.05f, kDodgeWingPitch = 0.22f;
        inline constexpr float kBreathFreq = 2.75f, kIdleBobAmp = 0.015f, kIdleSquashAmp = 0.011f;
        inline constexpr float kHopAmpBase = 0.070f, kHopAmpSprintAdd = 0.030f, kWaddleAmpBase = 0.122f, kWaddleAmpSprintAdd = 0.036f;
        inline constexpr float kLeanAmpBase = 0.090f, kLeanAmpSprintAdd = 0.068f;
        inline constexpr float kStepSquashBase = 0.033f, kStepSquashSprintAdd = 0.012f;
        inline constexpr float kScaleXZStepSquashRate = 0.72f, kScaleYStepSquashRate = 0.96f, kScaleYIdleSquashRate = 0.60f;
        inline constexpr float kBodyRecoilPitchBack = 0.045f, kBodyStrafeRollRate = 0.072f, kHealingBodyPitchAdd = 0.10f;
        inline constexpr float kLeftIdleFlapBase = 0.045f, kLeftIdleFlapBreath = 0.025f, kLeftWalkFlapBase = 0.18f, kLeftWalkFlapHop = 0.15f;
        inline constexpr float kRightIdleFlapBase = 0.040f, kRightIdleFlapBreath = 0.020f, kRightWalkFlapBase = 0.055f, kRightWalkFlapHop = 0.045f;
        inline constexpr float kSprintFlapBase = 0.40f, kSprintFlapWave = 0.30f, kSprintFlapFreqMult = 1.5f;
        inline constexpr float kLeftWalkPitchAmp = 0.28f, kLeftWalkSprintDamp = 0.35f, kSprintWingPitchAmp = 0.34f, kRightHoldRiflePitch = -0.18f;
        inline constexpr float kReloadLeftWingPitch = -0.65f, kReloadLeftWingYaw = -0.32f, kReloadLeftWingMinFlap = 0.24f;
        inline constexpr float kHealPatFreq = 16.0f, kHealPatAmp = 0.14f, kHealWingPitchBase = -0.48f, kHealWingYawAbs = 0.25f, kHealWingFlapOut = 0.18f;
    }

    // 3段階タクティカル／ドライ・リロードアニメーション定数
    namespace ReloadAnim
    {
        inline constexpr float kCantInSpan = 0.14f, kCantOutStart = 0.78f, kCantOutSpan = 0.22f;
        inline constexpr float kCantYawOffset = 0.34f, kCantPitchOffset = -0.38f, kCantLiftY = 0.055f, kBodyBobFollow = 0.35f;
        inline constexpr float kCantRightPull = -0.075f, kCantFwdPull = -0.030f;
        inline constexpr float kTapStart = 0.66f, kTapEnd = 0.76f, kTapSpan = 0.10f, kTapLiftY = 0.032f, kTapPitchDown = 0.065f;
        inline constexpr float kDryRackStart = 0.76f, kDrySlamStart = 0.86f, kDryRackEnd = 0.93f, kDryRackSpan = 0.10f, kDrySlamSpan = 0.07f;
        inline constexpr float kDryRackFwdPull = 0.075f, kDryRackPitchUp = 0.050f, kDrySlamOvertravel = 0.024f;
        inline constexpr float kTacSnapStart = 0.80f, kTacSnapEnd = 0.90f, kTacSnapSpan = 0.10f, kTacSnapFwdPush = 0.020f;
        inline constexpr float kMagReleaseEnd = 0.05f, kMagDropEnd = 0.16f, kMagDropSpan = 0.11f;
        inline constexpr float kMagDropSlideDist = 0.24f, kMagDropFwdDrift = 0.03f, kMagDropPitchTilt = 0.18f;
        inline constexpr float kMagEmptyEnd = 0.34f, kMagInsertEnd = 0.67f, kMagInsertSpan = 0.33f;
        inline constexpr float kMagPouchDown = 0.27f, kMagPouchLeft = 0.20f, kMagPouchBack = 0.12f;
        inline constexpr float kMagInsertPitch = 0.36f, kMagInsertYaw = 0.22f;
    }
}
