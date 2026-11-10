#pragma once
#include "Vector.h"
#include "Application/Config/GameConfig.h"
#include <cstddef>
#include <cstdint>

/// <summary>
/// 戦闘システム（射撃・排莢・落下マガジン・マズルブラスト・気流トレイル）の調整定数定義
/// </summary>
namespace CombatTuning
{
	namespace Pool
	{
		constexpr size_t kInitialBulletCapacity = 64, kMaxActiveCasings = 56, kMaxDroppedMagazines = 24;
	}

	namespace Assets
	{
		constexpr const char*  kResourceDir          = "Resources";
		constexpr const char*  kProjectResourceDir   = "project/Resources/";
		constexpr const char*  kBallisticsAtlasPath  = "Resources/duckov_ballistics_atlas.png";
		constexpr const char*  kMuzzleFlashTexPath   = "Resources/duckov_muzzle_flash.png";
		constexpr const char*  kMagazineTexPath      = "Resources/duckov_magazine_tex.png";
		constexpr const char*  kCasingModelFile      = "duckov_shell_casing.obj";
		constexpr const char*  kMagFullModelFile     = "duckov_magazine_full.obj";
		constexpr const char*  kMagEmptyModelFile    = "duckov_magazine_empty.obj";
		constexpr const char*  kFallbackSphereFile   = "sphere.obj";
		constexpr const wchar_t* kObjectVSPath       = L"Resources/shaders/Object3D.VS.hlsl";
		constexpr const wchar_t* kObjectPSPath       = L"Resources/shaders/Object3D.PS.hlsl";
		constexpr const wchar_t* kVSProfile          = L"vs_6_0";
		constexpr const wchar_t* kPSProfile          = L"ps_6_0";

		constexpr float kCasingBoundingRadius = 0.8f, kCasingReflection = 0.26f, kCasingFresnelF0 = 0.62f;
		constexpr Vector4 kCasingMasterColor = { 1.04f, 0.98f, 0.88f, 1.0f };

		constexpr float kMagazineBoundingRadius = 1.0f, kMagFullReflection = 0.15f, kMagFullFresnelF0 = 0.30f;
		constexpr float kMagEmptyReflection = 0.12f, kMagEmptyFresnelF0 = 0.24f;
		constexpr Vector4 kWhiteColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	}

	namespace Spark
	{
		constexpr float kDirNormalizeEps = 1e-4f, kTargetScaleRatio = 0.30f, kBounceElasticity = 0.45f, kAngularVelocityRange = 8.0f;
	}

	namespace BridgeDeck
	{
		constexpr float kMinZ = 26.0f, kMaxZ = 34.0f, kBridgeCenterX = 14.0f, kDeckHalfWidth = 1.45f;
		constexpr float kExtractionHardPadRad = 3.6f, kOriginHardPadRad = 3.2f;
	}

	namespace Casing
	{
		constexpr float kDefaultInstanceLife = 3.4f, kEnemyLifeTime = 2.8f, kPlayerLifeTime = 3.5f;
		constexpr Vector4 kEnemyColor             = { 0.94f, 0.84f, 0.70f, 1.0f }; // 東側ラッカー塗装・真鍮薬莢
		constexpr Vector4 kPlayerColor            = { 1.06f, 0.98f, 0.86f, 1.0f }; // NATO C260 ポリッシュ真鍮薬莢

		constexpr float kSpawnRightOffset = 0.12f, kSpawnUpOffset = 0.05f, kPlayerBreechBackOffset = 0.48f, kEnemyBreechBackOffset = 0.44f;

		constexpr float kRightVelMin = 2.15f, kRightVelMax = 3.45f, kUpVelMin = 2.10f, kUpVelMax = 3.30f;
		constexpr float kBackVelMin = 0.35f, kBackVelMax = 0.95f, kYawJitter = 0.5f, kAngVelPitchRange = 24.0f;
		constexpr float kAngVelYawRange = 18.0f, kAngVelRollMin = 26.0f, kAngVelRollMax = 44.0f;

		constexpr float kGravity = 11.8f, kBridgeSurfaceY = 0.068f, kGroundSurfaceY = 0.022f;

		constexpr const char* kBounceSePath       = "Resources/Audio/SE_Casing_Bounce.wav";
		constexpr float kBounceSeVolume = 0.36f, kBounceSeMinDist = 3.0f, kBounceSeMaxDist = 18.0f;
		constexpr uint32_t kBounceSeCooldownMs = 45;

		constexpr float kBounceFxOffsetY = 0.015f, kSparkHorizVel = 0.8f, kSparkUpVelMin = 0.45f, kSparkUpVelMax = 1.05f;
		constexpr Vector4 kSparkStartColor = { 1.0f, 0.90f, 0.55f, 0.72f }, kSparkEndColor = { 1.0f, 0.42f, 0.10f, 0.0f };
		constexpr float kSparkWidth = 0.014f, kSparkLength = 0.055f, kSparkLifeTime = 0.10f, kSparkGravity = 10.0f, kSparkDrag = 2.0f;

		constexpr Vector2 kDustAspect = { 1.35f, 0.72f };
		constexpr float kDustInitScale = 0.055f, kDustTargetScale = 0.145f, kDustHorizVel = 0.35f, kDustUpVelMin = 0.08f;
		constexpr float kDustUpVelMax = 0.22f;
		constexpr Vector4 kDustStartColor = { 0.62f, 0.54f, 0.42f, 0.22f }, kDustEndColor = { 0.68f, 0.62f, 0.52f, 0.0f };
		constexpr float kDustDrag = 3.0f, kDustLifeTime = 0.24f, kDustFadeInRatio = 0.12f;

		constexpr int kMaxBounceCount = 3;
		constexpr float kSettleVelY = 0.65f, kSettledRotX = 0.03f, kRestitutionY = 0.42f, kFrictionXZ = 0.58f;
		constexpr float kScatterVelXZ = 0.35f, kAngularDampingXZ = 0.65f;

		constexpr float kFadeDuration = 0.60f, kMinFadeAlpha = 0.12f;
		constexpr Vector4 kFadeTint = { 1.0f, 0.94f, 0.82f, 1.0f };
	}

	namespace Magazine
	{
		constexpr float kDefaultInstanceLife = 16.0f, kEnemyLifeTime = 12.5f, kPlayerLifeTime = 16.5f;
		constexpr float kFullDropReflection = 0.16f, kEmptyDropReflection = 0.12f, kDropFresnelF0 = 0.28f;
		constexpr Vector4 kEnemyColor             = { 0.78f, 0.68f, 0.64f, 1.0f }; // ダークプラム系ポリマー弾倉
		constexpr Vector4 kPlayerColor            = { 1.02f, 0.99f, 0.95f, 1.0f }; // コヨーテFDE・窓付き湾曲PMAG
		constexpr Vector3 kEnemyScale = { 0.92f, 0.92f, 0.92f }, kPlayerScale = { 1.0f, 1.0f, 1.0f };

		constexpr float kRightVelMin = 0.26f, kRightVelMax = 0.62f, kFwdVelMin = 0.32f, kFwdVelMax = 0.75f;
		constexpr float kDownVelMin = -0.55f, kDownVelMax = -0.15f, kYawJitter = 0.45f, kInitialPitch = -1.15f;
		constexpr float kAngVelPitchMin = 5.8f, kAngVelPitchMax = 8.8f, kAngVelYawRange = 4.2f;

		constexpr float kGravity = 11.2f, kBridgeSurfaceY = 0.092f, kGroundSurfaceY = 0.036f;

		constexpr const char* kDropSePath         = "Resources/Audio/SE_Mag_Drop.wav";
		constexpr float kDropSeVolume = 0.54f, kDropSeMinDist = 4.0f, kDropSeMaxDist = 22.0f;
		constexpr uint32_t kDropSeCooldownMs = 70;

		constexpr int kDustPuffCount = 2;
		constexpr float kDustJitterXZ = 0.08f, kDustOffsetY = 0.02f;
		constexpr Vector2 kDustAspect = { 1.40f, 0.72f };
		constexpr float kDustInitScale = 0.085f, kDustTargetScale = 0.24f, kDustHorizVel = 0.48f, kDustUpVelMin = 0.10f;
		constexpr float kDustUpVelMax = 0.26f;
		constexpr Vector4 kBridgeDustColor = { 0.56f, 0.45f, 0.32f, 0.24f };
		constexpr Vector4 kGroundDustColor = { 0.63f, 0.55f, 0.43f, 0.28f }, kDustEndColor = { 0.68f, 0.62f, 0.54f, 0.0f };
		constexpr float kDustDrag = 3.2f, kDustLifeTime = 0.30f, kDustFadeInRatio = 0.14f;

		constexpr int kMaxBounceCount = 2;
		constexpr float kSettleVelY = 0.55f, kSettledRotX = 0.02f, kRestitutionY = 0.28f, kFrictionXZ = 0.48f;
		constexpr float kScatterVelXZ = 0.18f, kAngularDampingX = 0.45f, kAngularDampingY = 0.50f;

		constexpr float kFadeDuration = 1.20f, kMinFadeAlpha = 0.12f;

		// 敵リロード時のマガジン排出パラメータ
		constexpr float kEnemyDropReloadRatio = 0.12f, kEnemyMagRightOffset = 0.34f, kEnemyMagFwdOffset = 0.28f, kEnemyMagUpOffset = 0.56f;
		constexpr const char* kEnemyMagOutSePath  = "Resources/Audio/SE_Reload_MagOut.wav";
		constexpr float kEnemyMagOutVol = 0.48f, kEnemyMagOutMinDist = 4.0f, kEnemyMagOutMaxDist = 24.0f;
		constexpr uint32_t kEnemyMagOutCooldownMs = 120;
	}

	namespace MuzzleBlast
	{
		// 1. クラウン直前の白熱マッハディスク閃光
		constexpr float kCoreFwdOffset = 0.06f, kCoreScaleShotgun = 0.56f, kCoreScaleEnemy = 0.34f;
		constexpr float kCoreScalePlayer = 0.40f, kCoreFwdSpeed = 1.5f;
		constexpr Vector4 kCoreColorEnemy = { 1.0f, 0.72f, 0.36f, 0.95f }, kCoreColorPlayer = { 1.0f, 0.92f, 0.68f, 0.96f };
		constexpr Vector4 kCoreEndColor = { 1.0f, 0.42f, 0.10f, 0.0f };
		constexpr float kCoreLifeShotgun = 0.050f, kCoreLifeRifle = 0.034f, kCoreTargetScaleRatio = 0.35f;

		// 2. 前方軸方向ジェット炎
		constexpr int kJetCountShotgun = 7, kJetCountRifle = 3;
		constexpr float kJetBaseFwdOffset = 0.10f, kJetStepFwdOffset = 0.07f, kJetBaseScaleShotgun = 0.38f;
		constexpr float kJetBaseScaleRifle = 0.24f, kJetScaleJitterMin = 0.8f, kJetScaleJitterMax = 1.2f;
		constexpr Vector2 kJetAspect = { 1.55f, 0.75f };
		constexpr float kJetFwdSpeedMin = 6.5f, kJetFwdSpeedMax = 12.5f, kJetRightJitter = 0.9f, kJetUpJitter = 0.6f;
		constexpr Vector4 kJetColorEnemy = { 1.0f, 0.58f, 0.22f, 0.86f }, kJetColorPlayer = { 1.0f, 0.80f, 0.36f, 0.88f };
		constexpr Vector4 kJetEndColor = { 1.0f, 0.28f, 0.05f, 0.0f };
		constexpr float kJetDrag = 8.0f, kJetLifeMin = 0.032f, kJetLifeMax = 0.055f, kJetTargetScaleRatio = 0.45f;

		// 3. 左右サイドベント噴出炎
		constexpr float kVentSideOffset = 0.05f, kVentScaleShotgun = 0.24f, kVentScaleRifle = 0.16f;
		constexpr float kVentSideSpeedMin = 2.8f, kVentSideSpeedMax = 4.4f, kVentFwdSpeed = 1.6f;
		constexpr Vector4 kVentStartColor = { 1.0f, 0.74f, 0.32f, 0.68f }, kVentEndColor = { 1.0f, 0.35f, 0.08f, 0.0f };
		constexpr float kVentDrag = 9.0f, kVentLifeTime = 0.036f, kVentTargetScaleRatio = 0.4f;

		// 4. 未燃焼装薬の高速火の粉スパーク
		constexpr int kSparkCountShotgun = 12, kSparkCountRifle = 5;
		constexpr float kSparkFwdOffset = 0.08f, kSparkFwdSpeedMin = 10.0f, kSparkFwdSpeedMax = 18.5f;
		constexpr float kSparkRightJitter = 2.4f, kSparkUpMin = -0.7f, kSparkUpMax = 1.9f;
		constexpr Vector4 kSparkBrightColor = { 1.0f, 0.96f, 0.76f, 0.98f };
		constexpr Vector4 kSparkAmberColor = { 1.0f, 0.80f, 0.34f, 0.94f }, kSparkEndColor = { 1.0f, 0.26f, 0.04f, 0.0f };
		constexpr float kSparkWidthMin = 0.016f, kSparkWidthMax = 0.028f, kSparkLenMin = 0.09f, kSparkLenMax = 0.19f;
		constexpr float kSparkLifeMin = 0.08f, kSparkLifeMax = 0.17f, kSparkGravity = 7.8f, kSparkDrag = 4.2f;

		// 5. 銃口コルダイト硝煙＆薬室薄煙
		constexpr int kSmokePuffShotgun = 5, kSmokePuffRifle = 3;
		constexpr float kSmokeBaseFwdOffset = 0.10f, kSmokeStepFwdOffset = 0.11f, kSmokeInitMinShotgun = 0.13f;
		constexpr float kSmokeInitMaxShotgun = 0.20f, kSmokeInitMinRifle = 0.08f, kSmokeInitMaxRifle = 0.13f;
		constexpr float kSmokeEndMinShotgun = 0.66f, kSmokeEndMaxShotgun = 0.94f, kSmokeEndMinRifle = 0.40f, kSmokeEndMaxRifle = 0.62f;
		constexpr Vector2 kSmokeAspect = { 1.32f, 0.90f };
		constexpr float kSmokeFwdSpeedMin = 2.6f, kSmokeFwdSpeedMax = 5.2f, kSmokeRightJitter = 0.48f, kSmokeUpMin = 0.22f;
		constexpr float kSmokeUpMax = 0.62f;
		constexpr Vector3 kSmokeStartRGB = { 0.76f, 0.75f, 0.72f };
		constexpr float kSmokeAlphaShotgun = 0.30f, kSmokeAlphaRifle = 0.21f;
		constexpr Vector4 kSmokeEndColor = { 0.66f, 0.66f, 0.65f, 0.0f };
		constexpr float kSmokeDrag = 3.5f, kSmokeCurlFreq = 1.6f, kSmokeCurlAmp = 0.20f, kSmokeLifeMin = 0.50f;
		constexpr float kSmokeLifeMax = 0.82f, kSmokeAngularVel = 1.1f, kSmokeFadeInRatio = 0.10f;

		constexpr float kChamberBackOffset = 0.48f, kChamberRightOffset = 0.10f, kChamberInitScale = 0.055f, kChamberTargetScale = 0.28f;
		constexpr Vector2 kChamberAspect = { 0.92f, 1.35f };
		constexpr float kChamberRightSpeed = 0.52f, kChamberUpSpeed = 0.82f;
		constexpr Vector4 kChamberStartColor = { 0.80f, 0.79f, 0.76f, 0.20f }, kChamberEndColor = { 0.70f, 0.70f, 0.69f, 0.0f };
		constexpr float kChamberDrag = 2.4f, kChamberCurlFreq = 2.0f, kChamberCurlAmp = 0.14f, kChamberLifeTime = 0.44f;
		constexpr float kChamberFadeInRatio = 0.12f;
	}

	namespace Firing
	{
		constexpr float kShotgunSoundRadius = 18.0f, kRifleSoundRadius = 14.0f, kSoundRingDuration = 0.6f;
		constexpr float kShotgunLightFlashTime = 0.055f, kRifleLightFlashTime = 0.030f;

		constexpr float kShotgunShakeDuration = 0.16f, kShotgunShakeAmplitude = 0.38f;
		constexpr const char* kShotgunSePath      = "Resources/Audio/SE_Gun_Shotgun.wav";
		constexpr float kShotgunSeVol = 0.80f;
		constexpr uint32_t kShotgunSeCooldownMs = 35;

		constexpr float kRifleShakeDuration = 0.065f, kRifleShakeAmplitude = 0.10f;
		constexpr const char* kRifleSePath1       = "Resources/Audio/SE_Gun_Rifle_1.wav";
		constexpr const char* kRifleSePath2       = "Resources/Audio/SE_Gun_Rifle_2.wav";
		constexpr const char* kRifleSePath3       = "Resources/Audio/SE_Gun_Rifle_3.wav";
		constexpr float kRifleSeVol = 0.72f;
		constexpr uint32_t kRifleSeCooldownMs = 28;

		constexpr float kEnemyShakeDuration = 0.055f, kEnemyShakeAmplitude = 0.075f;
		constexpr const char* kEnemyShotSePath1   = "Resources/Audio/SE_Enemy_Shot_1.wav";
		constexpr const char* kEnemyShotSePath2   = "Resources/Audio/SE_Enemy_Shot_2.wav";
		constexpr float kEnemyShotSeVol = 0.72f, kEnemyShotMinDist = 6.0f, kEnemyShotMaxDist = 48.0f;
		constexpr uint32_t kEnemyShotCooldownMs = 35;
	}

	namespace GrazeAndWake
	{
		constexpr float kNearMissMargin = 1.20f;
		constexpr const char* kWhizSePath         = "Resources/Audio/SE_Bullet_Whiz.wav";
		constexpr float kWhizSeVol = 0.72f;
		constexpr uint32_t kWhizSeCooldownMs = 95;

		constexpr int kGrazeSparkCount = 5;
		constexpr float kGrazeSparkCenterIdx = 2.0f, kGrazeSparkSpacing = 0.14f, kGrazeSparkFwdSpeed = 3.2f;
		constexpr float kGrazeSparkHorizJitter = 0.6f, kGrazeSparkUpMin = 0.2f, kGrazeSparkUpMax = 0.8f;
		constexpr Vector4 kGrazeSparkStartColor = { 1.0f, 0.94f, 0.78f, 0.88f }, kGrazeSparkEndColor = { 0.7f, 0.88f, 1.0f, 0.0f };
		constexpr float kGrazeSparkWidth = 0.020f, kGrazeSparkLength = 0.13f, kGrazeSparkLife = 0.10f, kGrazeSparkDrag = 3.0f;

		constexpr float kGrazeTextOffsetY = 1.3f;
		constexpr const char* kGrazeTextLabel     = "GRAZE";
		constexpr Vector4 kGrazeTextColor = { 0.5f, 0.9f, 1.0f, 1.0f };

		constexpr float kWakeBackOffset = 0.45f, kWakeInitScale = 0.055f, kWakeTargetScale = 0.19f;
		constexpr Vector2 kWakeAspect = { 1.45f, 0.65f };
		constexpr float kWakeHorizJitter = 0.12f, kWakeUpMin = 0.06f, kWakeUpMax = 0.18f, kWakeBackSpeed = 0.35f;
		constexpr Vector4 kWakeColorEnemy = { 0.90f, 0.74f, 0.70f, 0.13f };
		constexpr Vector4 kWakeColorPlayer = { 0.88f, 0.87f, 0.84f, 0.13f }, kWakeEndColor = { 0.78f, 0.78f, 0.78f, 0.0f };
		constexpr float kWakeDrag = 2.0f, kWakeLifeTime = 0.22f, kWakeFadeInRatio = 0.08f;

		constexpr int kEmberChanceModulus = 3;
		constexpr float kEmberBackOffset = 0.20f, kEmberHorizJitter = 0.65f, kEmberUpMin = -0.2f, kEmberUpMax = 0.45f;
		constexpr Vector4 kEmberColorEnemy = { 1.0f, 0.42f, 0.16f, 0.76f };
		constexpr Vector4 kEmberColorPlayer = { 1.0f, 0.88f, 0.46f, 0.76f }, kEmberEndColor = { 1.0f, 0.25f, 0.05f, 0.0f };
		constexpr float kEmberWidth = 0.015f, kEmberLength = 0.048f, kEmberLifeTime = 0.11f, kEmberGravity = 4.5f, kEmberDrag = 3.5f;
	}
}
