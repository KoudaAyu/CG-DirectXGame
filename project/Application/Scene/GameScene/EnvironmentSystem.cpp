#include "EnvironmentSystem.h"
#include "GamePlayScene.h"
#include "Application/Audio/TacticalAudio.h"
#include "Application/Config/GameConfig.h"
#include "Application/Particle/AppParticleManager.h"
#include "ParticleManager.h"
#include "ParticleEmitter.h"
#include "TextureManager.h"
#include "../../Player/Player.h"
#include <cmath>
#include <numbers>
#include <random>
#include <algorithm>
#include <filesystem>
#include <unordered_map>

namespace
{
	constexpr float kPi    = GameConfig::Math::kPi;
	constexpr float kTwoPi = GameConfig::Math::kTwoPi;
	static std::unordered_map<std::string, Object3d::ModelData> sNatureModelCache;

	// --- 3D自然環境レイヤー構成テーブル ---
	constexpr float   kNatureLayerBoundingRadius = 2600.0f;
	constexpr Vector3 kNatureLightDirection      = { 0.36f, -0.82f, 0.44f };

	struct NatureLayerSpec
	{
		const char *objFile, *texFile;
		Vector3 translate;
		Vector4 color;
		bool enableLighting;
		float dirLightIntensity, reflectionFactor;
		bool setFresnel;
		float fresnelF0;
	};

	constexpr NatureLayerSpec kNatureLayerSpecs[] = {
		{ "duckov_outer_ground.obj",        "grass.png",                  { 0.0f, -0.020f,  0.0f }, {  1.0f,  1.0f,  1.0f, 1.0f  }, false, 0.0f,  0.0f,   false, 0.0f  },
		{ "duckov_ground_realism.obj",      "duckov_ground_realism.png",  { 0.0f,  0.0012f, 0.0f }, {  1.0f,  1.0f,  1.0f, 0.94f }, false, 0.0f,  0.0f,   false, 0.0f  },
		{ "duckov_compound_paths.obj",      "duckov_dirt_road.png",       { 0.0f,  0.0055f, 0.0f }, { 0.96f, 0.94f, 0.90f, 0.95f }, false, 0.0f,  0.0f,   false, 0.0f  },
		{ "duckov_river_banks.obj",         "duckov_river_bank_rock.png", { 0.0f,  0.0f,    0.0f }, { 24.0f, 24.5f, 23.5f, 1.0f  }, true,  0.04f, 0.0f,   true,  0.02f },
		{ "duckov_river_water.obj",         "duckov_river_water.png",     { 0.0f,  0.022f, 30.0f }, { 7.50f, 8.15f, 8.30f, 0.74f }, true,  0.12f, 0.016f, true,  0.02f },
		{ "duckov_river_foam.obj",          "duckov_river_foam.png",      { 0.0f,  0.033f, 30.0f }, { 0.95f, 0.99f,  1.0f, 0.48f }, false, 0.0f,  0.0f,   false, 0.0f  },
		{ "duckov_bridge_wakes.obj",        "duckov_bridge_wake.png",     { 0.0f,  0.038f, 30.0f }, { 0.96f, 0.99f,  1.0f, 0.66f }, false, 0.0f,  0.0f,   false, 0.0f  },
		{ "duckov_grass_foliage_a.obj",     "duckov_grass_atlas.png",     { 0.0f,  0.0f,    0.0f }, { 0.96f, 0.98f, 0.93f, 4.0f  }, false, 0.0f,  0.0f,   false, 0.0f  },
		{ "duckov_grass_foliage_b.obj",     "duckov_grass_atlas.png",     { 0.0f,  0.0f,    0.0f }, { 0.96f, 0.98f, 0.93f, 4.0f  }, false, 0.0f,  0.0f,   false, 0.0f  },
		{ "duckov_woodland_understory.obj", "duckov_pine_tree.png",       { 0.0f,  0.0f,    0.0f }, { 24.0f, 24.2f, 23.5f, 6.0f  }, true,  0.04f, 0.0f,   false, 0.0f  },
		{ "duckov_woodland_sway.obj",       "duckov_pine_tree.png",       { 0.0f,  0.0f,    0.0f }, { 23.8f, 24.5f, 23.2f, 6.0f  }, true,  0.04f, 0.0f,   false, 0.0f  },
	};

	// --- 水流スクロール & 風揺れ植生アニメーション定数 ---
	struct GrassSwaySpec { float freqX1, phaseX1, freqX2, phaseX2, freqZ, phaseZ; };
	constexpr GrassSwaySpec kGrassSwayA = { 1.85f, 0.0f,  3.90f, 0.0f, 1.45f, 0.0f  };
	constexpr GrassSwaySpec kGrassSwayB = { 1.85f, 1.95f, 4.15f, 0.8f, 1.45f, 1.60f };

	constexpr struct
	{
		float riverZ, meshPeriodX, mainFlowSpd, mainBaseY, mainSwellAmp, mainSwellFreq;
		float foamFlowSpd, foamBaseY, foamLapAmp, foamLapFreq;
		float wakeBaseY, wakeShiftAmp, wakeShiftFreq, wakePulseAmp, wakePulseFreq, wakeAlphaBase, wakeAlphaAmp, wakeAlphaFreq, wakeAlphaPhase;
		Vector3 wakeRgb;
		float grassAmpX1, grassAmpX2, grassAmpZ, grassBreathAmpY;
		float woodAmpX1, woodFreqX1, woodPhaseX1, woodAmpX2, woodFreqX2, woodAmpZ, woodFreqZ, woodPhaseZ;
	} kAnim = {
		GameConfig::Environment::kRiverPosition.z, 8.0f, 2.35f, 0.022f, 0.0018f, 1.65f,
		3.75f, 0.033f, 0.016f, 2.15f,
		0.038f, -0.07f, 6.2f, 0.030f, 7.4f, 0.72f, 0.10f, 5.6f, 0.8f, { 0.96f, 0.99f, 1.0f },
		0.028f, 0.011f, 0.016f, 0.016f,
		0.022f, 1.65f, 0.9f, 0.009f, 3.4f, 0.015f, 1.35f, 0.7f
	};

	// --- 共通パーティクルプリセット構造体 ---
	struct ParticlePreset
	{
		Vector2 aspect;
		float initMult, targetMult;
		Vector4 startColor, endColor;
		Vector2 lifeRange;
		float gravity, drag, bounce, fadeIn;
		uint16_t slotLayer;
	};

	constexpr ParticlePreset kPresetExfilBurst = { { 0.018f, 0.095f }, 1.0f,  0.25f, { 1.0f,  0.88f, 0.42f, 0.92f }, { 1.0f,  0.35f, 0.08f, 0.0f }, { 0.28f, 0.52f }, 10.5f,  1.6f,  0.40f, 0.0f,  2 };
	constexpr ParticlePreset kPresetFlareCore  = { { 1.0f,   1.0f   }, 0.0f,  0.0f,  { 0.28f, 0.95f, 0.52f, 0.58f }, { 0.10f, 0.80f, 0.35f, 0.0f }, { 0.08f, 0.08f }, 0.0f,   0.0f,  0.0f,  0.0f,  0 };
	constexpr ParticlePreset kPresetVentSpark  = { { 1.0f,   3.4f   }, 1.0f,  0.35f, { 1.0f,  0.84f, 0.38f, 0.85f }, { 1.0f,  0.36f, 0.08f, 0.0f }, { 0.24f, 0.42f }, 9.5f,   0.0f,  0.38f, 0.0f,  2 };
	constexpr ParticlePreset kPresetFlareSmoke = { { 1.0f,   1.0f   }, 1.0f,  1.0f,  { 0.0f,  0.0f,  0.0f,  0.0f  }, { 0.14f, 0.52f, 0.27f, 0.0f }, { 0.0f,  0.0f  }, 0.015f, 0.24f, 0.0f,  0.20f, 1 };
	constexpr ParticlePreset kPresetRiverFoam  = { { 2.55f,  0.36f  }, 0.85f, 1.35f, { 0.93f, 0.98f, 1.0f,  0.28f }, { 0.78f, 0.92f, 0.98f, 0.0f }, { 1.25f, 2.15f }, 0.0f,   0.0f,  0.0f,  0.25f, 0 };
	constexpr ParticlePreset kPresetRiverGlint = { { 1.65f,  0.55f  }, 1.0f,  0.35f, { 1.0f,  0.97f, 0.88f, 0.62f }, { 0.72f, 0.92f, 1.0f,  0.0f }, { 0.65f, 1.15f }, 0.0f,   0.0f,  0.0f,  0.25f, 1 };
	constexpr ParticlePreset kPresetRiverMist  = { { 2.4f,   0.52f  }, 1.0f,  2.15f, { 0.84f, 0.94f, 0.98f, 0.13f }, { 0.78f, 0.92f, 0.98f, 0.0f }, { 2.2f,  3.4f  }, 0.0f,   0.18f, 0.0f,  0.30f, 0 };

	// --- 脱出ヘリパッド・M18発煙筒・バリア球定数 ---
	constexpr struct
	{
		float ringRotStep;
		Vector3 flareGroundOffset, flareVentOffset, disabledGpuPos;
		float tickVol, completeVol, shakeDur, shakeIntensity, disabledGpuFreq;
		int burstCount, beaconChanceDiv, ventSparkChanceDiv, smokeLayersUnlocked, smokeLayersLocked;
		Vector2 burstSpd, burstVelY, ventSparkScale, ventSparkVelY;
		float coreScaleUnlocked, coreScaleLocked, coreVelY, coreAlphaUnlocked, coreAlphaLocked, ventSparkVelH;
		float smokeIntervalUnlocked, smokeIntervalLocked, smokeJitter, smokeAngVel, smokeWobbleFreq, smokeWobbleAmp, smokeWobbleAxisY, smokeCurlFreq, smokeCurlAmp;
		Vector2 smokeRadialSpd, smokeLifeUnlocked, smokeLifeLocked, smokeWindUnlocked, smokeWindLocked;
	} kFlare = {
		0.02f, { 1.8f, 0.026f, 1.2f }, { 0.0f, 0.18f, 0.0f }, { 0.0f, -200.0f, 0.0f },
		0.66f, 0.82f, 0.32f, 0.38f, 999.0f,
		22, 3, 4, 3, 2,
		{ 1.0f, 3.2f }, { 2.2f, 5.2f }, { 0.015f, 0.024f }, { 1.8f, 3.4f },
		0.060f, 0.042f, 2.2f, 0.58f, 0.35f, 0.95f,
		0.026f, 0.045f, 0.04f, 0.35f, 0.95f, 0.16f, 0.1f, 0.68f, 0.36f,
		{ 0.14f, 0.34f }, { 3.6f, 4.8f }, { 2.8f, 3.6f }, { 0.52f, 0.62f }, { 0.36f, 0.42f }
	};

	struct SmokePuffLayerSpec
	{
		Vector2 initUnlocked, initLocked, targetUnlocked, targetLocked, riseUnlocked, riseLocked;
		Vector3 colorRgb;
		float alphaUnlocked, alphaLocked;
		bool randomAspect;
		Vector2 aspectX, aspectY;
		float drag;
		uint16_t slotLayer;
	};

	constexpr SmokePuffLayerSpec kSmokePuffLayers[] = {
		{ { 0.16f, 0.22f }, { 0.12f, 0.16f }, { 1.45f, 2.05f }, { 0.95f, 1.35f }, { 2.55f, 3.25f }, { 1.85f, 2.35f }, { 0.07f, 0.74f, 0.34f }, 0.60f, 0.40f, true,  { 0.88f, 1.15f }, { 1.05f, 1.32f }, 0.24f, 1 },
		{ { 0.14f, 0.19f }, { 0.10f, 0.14f }, { 1.25f, 1.75f }, { 0.85f, 1.15f }, { 1.95f, 2.55f }, { 1.45f, 1.85f }, { 0.10f, 0.68f, 0.32f }, 0.46f, 0.30f, true,  { 1.10f, 1.42f }, { 0.85f, 1.10f }, 0.24f, 1 },
		{ { 0.18f, 0.25f }, { 0.18f, 0.25f }, { 1.30f, 1.80f }, { 1.30f, 1.80f }, { 0.55f, 1.05f }, { 0.55f, 1.05f }, { 0.09f, 0.64f, 0.30f }, 0.34f, 0.34f, false, { 1.75f, 1.75f }, { 0.68f, 0.68f }, 0.42f, 0 },
	};

	constexpr struct { float rotStep, centerY, baseScale, pulseSpeedMult, pulseAmp; Vector3 hiddenPos; } kBarrier = {
		0.05f, 0.5f, 1.8f, 4.0f, 0.05f, { 0.0f, -100.0f, 0.0f }
	};

	// --- 河川エフェクト定数 ---
	constexpr float kBridgePillarUpstreamX[] = { -12.42f, 15.58f };
	constexpr struct
	{
		float bridgeAbsX, bridgeWaveClearance, bridgeMistClearance;
		Vector2 waveXRange, foamScale, glintScale;
		int foamCount, pillarRollMod, pillarRollChance, glintCount;
		float foamVelZ, pillarSpraySpd, glintYOffset, glintTwinkleSpd, splashXRange, mistInterval, mistY, mistRotZ, mistVelZ;
		Vector2 mistX, mistZ, mistScale, mistVelX, mistVelY;
	} kRiverFx = {
		14.0f, 1.85f, 2.2f,
		{ -34.0f, 36.0f }, { 0.11f, 0.20f }, { 0.032f, 0.062f },
		4, 100, 60, 3,
		0.10f, 2.6f, 0.005f, 16.0f, 32.0f, 0.24f, 0.14f, 0.25f, 0.12f,
		{ -30.0f, 34.0f }, { 27.8f, 32.2f }, { 0.42f, 0.68f }, { -1.65f, -1.05f }, { 0.03f, 0.09f }
	};

	inline float Rand01() { return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX); }
	inline float RandRange(float a, float b) { return a + (b - a) * Rand01(); }
	inline bool IsNearBridgeDeckX(float x, float clearance) { return std::abs(std::abs(x) - kRiverFx.bridgeAbsX) < clearance; }

	AppParticle BuildPresetParticle(
		const Vector3& pos, float rotZ, float baseScale, Vector2 aspectOverride,
		const Vector3& vel, float lifeTime, uint32_t texIdx, const ParticlePreset& preset)
	{
		AppParticle p{};
		p.transform.Initialize();
		p.transform.SetTranslate(pos);
		if (rotZ != 0.0f) p.transform.SetRotate({ 0.0f, 0.0f, rotZ });
		p.scaleAspect = aspectOverride;
		p.initialScale = baseScale * preset.initMult;
		p.targetScale = baseScale * preset.targetMult;
		const float drawSc = (p.initialScale > 0.0f) ? p.initialScale : baseScale;
		p.transform.SetScale({ drawSc * p.scaleAspect.x, drawSc * p.scaleAspect.y, 1.0f });
		p.velocity = vel;
		p.color = preset.startColor;
		p.endColor = preset.endColor;
		p.hasColorShift = true;
		p.lifeTime = lifeTime;
		p.currentTime = 0.0f;
		p.textureIndex = texIdx;
		p.gravity = preset.gravity;
		p.drag = preset.drag;
		p.bounceElasticity = preset.bounce;
		p.fadeInRatio = preset.fadeIn;
		p.slotLayer = preset.slotLayer;
		return p;
	}

	void UpdateGrassBatch(Object3d* obj, float animTime, const GrassSwaySpec& spec)
	{
		if (!obj) return;
		const float swayX = kAnim.grassAmpX1 * std::sin(animTime * spec.freqX1 + spec.phaseX1)
			+ kAnim.grassAmpX2 * std::sin(animTime * spec.freqX2 + spec.phaseX2);
		const float swayZ = kAnim.grassAmpZ * std::cos(animTime * spec.freqZ + spec.phaseZ);
		const float breathY = 1.0f - kAnim.grassBreathAmpY * std::abs(std::sin(animTime * spec.freqX1 + spec.phaseX1));
		obj->SetTranslate({ swayX, 0.0f, swayZ });
		obj->SetScale({ 1.0f, breathY, 1.0f });
		obj->Update();
	}
}

EnvironmentSystem::EnvironmentSystem(GamePlayScene* scene) : scene_(scene) {}

void EnvironmentSystem::Initialize()
{
	riverWaveTimer_ = 0.0f;
	riverSplashTimer_ = 0.0f;
	riverMistTimer_ = 0.0f;
	barrierTimer_ = 0.0f;
	natureAnimTime_ = 0.0f;
	InitializeNatureLayers();
}

void EnvironmentSystem::Finalize()
{
	std::unique_ptr<Object3d>* layers[] = {
		&woodlandSwayObj_, &woodlandUnderstoryObj_, &grassFoliageObjB_, &grassFoliageObjA_,
		&riverBridgeWakesObj_, &riverWaterFoamObj_, &riverWaterMainObj_, &riverBanksObj_,
		&compoundPathsObj_, &groundRealismObj_, &outerGroundObj_
	};
	for (auto* layer : layers) layer->reset();
}

std::unique_ptr<Object3d> EnvironmentSystem::CreateNatureLayerObject(const std::string& objFilename, const std::string& texFilename)
{
	if (!scene_ || !scene_->object3dCom || !scene_->camera_) return nullptr;
	if (!std::filesystem::exists("Resources/" + objFilename) && !std::filesystem::exists("project/Resources/" + objFilename)) return nullptr;

	auto it = sNatureModelCache.find(objFilename);
	if (it == sNatureModelCache.end())
	{
		Object3d::ModelData loaded = Object3d::LoadObjFile("Resources", objFilename);
		loaded.material.textureFilePath = "Resources/" + texFilename;
		loaded.material.textureIndex = TextureManager::GetInstance()->Load(loaded.material.textureFilePath);
		// 2400m全域をカバーする広域地面・河川・自然レイヤーが視錐台カリングで欠けないよう十分な境界球半径を確保
		loaded.boundingRadius = kNatureLayerBoundingRadius;
		sNatureModelCache[objFilename] = std::move(loaded);
		it = sNatureModelCache.find(objFilename);
	}

	auto obj = std::make_unique<Object3d>();
	obj->Initialize(scene_->object3dCom, it->second);
	obj->SetCamera(scene_->camera_);
	obj->SetScale({ 1.0f, 1.0f, 1.0f });
	obj->SetRotate({ 0.0f, 0.0f, 0.0f });
	obj->SetAllowWireframeOverlay(false);
	return obj;
}

void EnvironmentSystem::InitializeNatureLayers()
{
	// 外周芝生・接地AO＆腐植土・ダート道・苔岩護岸・多層3D水面・白泡・橋脚引き波・風揺れ草木を一括セットアップ
	std::unique_ptr<Object3d>* targets[] = {
		&outerGroundObj_, &groundRealismObj_, &compoundPathsObj_, &riverBanksObj_,
		&riverWaterMainObj_, &riverWaterFoamObj_, &riverBridgeWakesObj_,
		&grassFoliageObjA_, &grassFoliageObjB_, &woodlandUnderstoryObj_, &woodlandSwayObj_
	};
	static_assert(std::size(targets) == std::size(kNatureLayerSpecs));

	for (size_t i = 0; i < std::size(kNatureLayerSpecs); ++i)
	{
		const NatureLayerSpec& spec = kNatureLayerSpecs[i];
		auto obj = CreateNatureLayerObject(spec.objFile, spec.texFile);
		if (obj)
		{
			if (spec.enableLighting)
			{
				auto& lightData = obj->GetLightData();
				lightData.directionalLight.direction = kNatureLightDirection;
				lightData.directionalLight.intensity = spec.dirLightIntensity;
				lightData.pointLight.intensity = 0.0f;
			}
			obj->SetTranslate(spec.translate);
			obj->SetColor(spec.color);
			obj->SetEnableLighting(spec.enableLighting);
			obj->SetReflectionFactor(spec.reflectionFactor);
			if (spec.setFresnel) obj->SetFresnelF0(spec.fresnelF0);
			obj->Update();
		}
		*targets[i] = std::move(obj);
	}
}

void EnvironmentSystem::UpdateNatureAnimation(float deltaTime)
{
	natureAnimTime_ += deltaTime;

	for (Object3d* staticObj : { outerGroundObj_.get(), groundRealismObj_.get(), compoundPathsObj_.get(), riverBanksObj_.get() })
	{
		if (staticObj) staticObj->Update();
	}

	// 8.000m厳密周期メッシュのシームレス水流スクロール（+X上流 -> -X下流へ連続流動・折り返しポップ完全ゼロ）
	if (riverWaterMainObj_)
	{
		const float flowMainX = -std::fmod(natureAnimTime_ * kAnim.mainFlowSpd, kAnim.meshPeriodX);
		const float swellY = kAnim.mainBaseY + kAnim.mainSwellAmp * std::sin(natureAnimTime_ * kAnim.mainSwellFreq);
		riverWaterMainObj_->SetTranslate({ flowMainX, swellY, kAnim.riverZ });
		riverWaterMainObj_->Update();
	}

	if (riverWaterFoamObj_)
	{
		const float flowFoamX = -std::fmod(natureAnimTime_ * kAnim.foamFlowSpd, kAnim.meshPeriodX);
		const float lapScaleZ = 1.0f + kAnim.foamLapAmp * std::sin(natureAnimTime_ * kAnim.foamLapFreq);
		riverWaterFoamObj_->SetTranslate({ flowFoamX, kAnim.foamBaseY, kAnim.riverZ });
		riverWaterFoamObj_->SetScale({ 1.0f, 1.0f, lapScaleZ });
		riverWaterFoamObj_->Update();
	}

	if (riverBridgeWakesObj_)
	{
		const float wakeShiftX = kAnim.wakeShiftAmp * std::sin(natureAnimTime_ * kAnim.wakeShiftFreq);
		const float wakePulseX = 1.0f + kAnim.wakePulseAmp * std::sin(natureAnimTime_ * kAnim.wakePulseFreq);
		const float wakeAlpha = kAnim.wakeAlphaBase + kAnim.wakeAlphaAmp * std::sin(natureAnimTime_ * kAnim.wakeAlphaFreq + kAnim.wakeAlphaPhase);
		riverBridgeWakesObj_->SetTranslate({ wakeShiftX, kAnim.wakeBaseY, kAnim.riverZ });
		riverBridgeWakesObj_->SetScale({ wakePulseX, 1.0f, 1.0f });
		riverBridgeWakesObj_->SetColor({ kAnim.wakeRgb.x, kAnim.wakeRgb.y, kAnim.wakeRgb.z, wakeAlpha });
		riverBridgeWakesObj_->Update();
	}

	// 根元浮きを一切起こさない2バッチ逆位相の自然な微風スウェイ演出
	UpdateGrassBatch(grassFoliageObjA_.get(), natureAnimTime_, kGrassSwayA);
	UpdateGrassBatch(grassFoliageObjB_.get(), natureAnimTime_, kGrassSwayB);

	if (woodlandUnderstoryObj_) woodlandUnderstoryObj_->Update();
	if (woodlandSwayObj_)
	{
		const float swayX = kAnim.woodAmpX1 * std::sin(natureAnimTime_ * kAnim.woodFreqX1 + kAnim.woodPhaseX1)
			+ kAnim.woodAmpX2 * std::sin(natureAnimTime_ * kAnim.woodFreqX2);
		const float swayZ = kAnim.woodAmpZ * std::cos(natureAnimTime_ * kAnim.woodFreqZ + kAnim.woodPhaseZ);
		woodlandSwayObj_->SetTranslate({ swayX, 0.0f, swayZ });
		woodlandSwayObj_->Update();
	}
}

void EnvironmentSystem::DrawOuterGround(const RenderContext& ctx)
{
	if (outerGroundObj_) outerGroundObj_->Draw(ctx);
}

void EnvironmentSystem::DrawNatureLayers(const RenderContext& ctx)
{
	// 接地AO＆腐植土 -> ダート道 -> 苔岩護岸・切り株 -> 多層3D水面・白泡・橋脚引き波 -> 風揺れ草木
	Object3d* orderedLayers[] = {
		groundRealismObj_.get(), compoundPathsObj_.get(), riverBanksObj_.get(), woodlandUnderstoryObj_.get(),
		riverWaterMainObj_.get(), riverWaterFoamObj_.get(), riverBridgeWakesObj_.get(),
		woodlandSwayObj_.get(), grassFoliageObjA_.get(), grassFoliageObjB_.get()
	};
	for (Object3d* layer : orderedLayers) { if (layer) layer->Draw(ctx); }
}

void EnvironmentSystem::Update(float deltaTime)
{
	if (!scene_) return;
	UpdateExtractionGoal(deltaTime);
	UpdateBarrier();
	UpdateNatureAnimation(deltaTime);
	UpdateRiverEffects(deltaTime);
	UpdateHitEffect(deltaTime);
}

void EnvironmentSystem::UpdateExtractionGoal(float deltaTime)
{
	if (!scene_) return;

	if (scene_->goalRing_)
	{
		scene_->goalRingTransform_.rotate.y += kFlare.ringRotStep;
		scene_->goalRing_->SetTransform(scene_->goalRingTransform_);
		scene_->goalRing_->Update();
	}

	const Vector3 goalPos = scene_->goalRingTransform_.translate;
	const Vector3 ventPos = goalPos + kFlare.flareGroundOffset + kFlare.flareVentOffset;

	if (scene_->player_ && !scene_->isGameIntroActive_)
	{
		const Vector3 playerPos = scene_->player_->GetPosition();
		const float dx = playerPos.x - goalPos.x;
		const float dz = playerPos.z - goalPos.z;
		scene_->isPlayerInExtractionZone_ =
			(dx * dx + dz * dz <= GameConfig::Math::Square(GameConfig::Environment::kExtractionRadius));

		// 脱出ゾーン内かつ全ての的を破壊済みの場合、脱出カウントダウン進行
		if (scene_->isPlayerInExtractionZone_ && scene_->allTargetsDestroyed_)
		{
			if (!scene_->isGameCleared_)
			{
				scene_->extractionTimer_ -= deltaTime;
				const int secRem = static_cast<int>(std::ceil(scene_->extractionTimer_));
				if (secRem >= 1 && secRem != lastExfilCountdownSec_)
				{
					lastExfilCountdownSec_ = secRem;
					TacticalAudio::Play2D("Resources/Audio/SE_Exfil_Tick.wav", kFlare.tickVol);
				}
				if (scene_->extractionTimer_ <= 0.0f)
				{
					scene_->extractionTimer_ = 0.0f;
					scene_->isGameCleared_ = true;
					scene_->clearCelebrateTimer_ = 0.0f;
					scene_->TriggerCameraShake(kFlare.shakeDur, kFlare.shakeIntensity);
					TacticalAudio::Play2D("Resources/Audio/SE_Exfil_Complete.wav", kFlare.completeVol);

					// 脱出確定瞬間：ミリタリー発煙筒の高圧エメラルドブースト＆信管スパーク
					if (scene_->particleManager && scene_->appParticleManager_)
					{
						const uint32_t spkTex = (scene_->sparkStreakTextureIndex_ != TextureManager::kInvalidTextureIndex)
							? scene_->sparkStreakTextureIndex_ : scene_->particleTextureB;
						for (int i = 0; i < kFlare.burstCount; ++i)
						{
							const float a = RandRange(0.0f, 2.0f * kPi);
							const float spd = RandRange(kFlare.burstSpd.x, kFlare.burstSpd.y);
							const Vector3 vel = { std::cos(a) * spd, RandRange(kFlare.burstVelY.x, kFlare.burstVelY.y), std::sin(a) * spd };
							const float life = RandRange(kPresetExfilBurst.lifeRange.x, kPresetExfilBurst.lifeRange.y);
							scene_->appParticleManager_->AddParticle(
								BuildPresetParticle(ventPos, a, 1.0f, kPresetExfilBurst.aspect, vel, life, spkTex, kPresetExfilBurst));
						}
					}
				}
			}
		}
		else if (!scene_->isGameCleared_)
		{
			scene_->extractionTimer_ = GameConfig::Environment::kExtractionMaxTime;
			lastExfilCountdownSec_ = -1;
		}
	}

	// 脱出ヘリパッド・地上設置型スモークフレア缶（M18 Military Green Smoke Flare）
	if (!scene_->particleManager) return;

	if (ParticleEmitter* gpuEmitter = scene_->particleManager->GetGPUEmitter())
	{
		if (EmitterSphere* emitterData = gpuEmitter->GetEmitterData())
		{
			emitterData->translate = kFlare.disabledGpuPos;
			emitterData->count = 0;
			emitterData->frequency = kFlare.disabledGpuFreq;
		}
	}

	const bool unlocked = scene_->allTargetsDestroyed_;
	const uint32_t flareSparkTex = (scene_->sparkStreakTextureIndex_ != TextureManager::kInvalidTextureIndex)
		? scene_->sparkStreakTextureIndex_ : scene_->particleTextureB;
	if (unlocked && scene_->appParticleManager_ && (std::rand() % kFlare.beaconChanceDiv == 0))
	{
		scene_->appParticleManager_->EmitHelipadBeaconMotes(scene_->particleManager->GetRandomEngine(), goalPos, flareSparkTex);
	}

	// A. 信管ノズルの微細な噴出ジェット輝点 ＆ 火花
	if (scene_->appParticleManager_)
	{
		const float coreSc = unlocked ? kFlare.coreScaleUnlocked : kFlare.coreScaleLocked;
		AppParticle coreP = BuildPresetParticle(
			ventPos, 0.0f, coreSc, kPresetFlareCore.aspect,
			{ 0.0f, kFlare.coreVelY, 0.0f }, kPresetFlareCore.lifeRange.x, scene_->particleTextureB, kPresetFlareCore);
		coreP.color.w = unlocked ? kFlare.coreAlphaUnlocked : kFlare.coreAlphaLocked;
		scene_->appParticleManager_->AddParticle(coreP);

		if (unlocked && (std::rand() % kFlare.ventSparkChanceDiv == 0))
		{
			const float sc = RandRange(kFlare.ventSparkScale.x, kFlare.ventSparkScale.y);
			const Vector3 vel = {
				RandRange(-kFlare.ventSparkVelH, kFlare.ventSparkVelH),
				RandRange(kFlare.ventSparkVelY.x, kFlare.ventSparkVelY.y),
				RandRange(-kFlare.ventSparkVelH, kFlare.ventSparkVelH)
			};
			const float life = RandRange(kPresetVentSpark.lifeRange.x, kPresetVentSpark.lifeRange.y);
			const Vector2 aspect = { sc * kPresetVentSpark.aspect.x, sc * kPresetVentSpark.aspect.y };
			scene_->appParticleManager_->AddParticle(BuildPresetParticle(ventPos, 0.0f, 1.0f, aspect, vel, life, flareSparkTex, kPresetVentSpark));
		}
	}

	// B. リアルなM18エメラルド誘導煙柱（ストレートアルファ clear_smoke_wispy 積層煙）
	AppParticleManager* smokeMgr = scene_->smokeParticleManager_ ? scene_->smokeParticleManager_.get() : scene_->appParticleManager_.get();
	const uint32_t wispyTex = (scene_->wispySmokeTextureIndex_ != TextureManager::kInvalidTextureIndex)
		? scene_->wispySmokeTextureIndex_
		: ((scene_->escapeSmokeTextureIndex_ != TextureManager::kInvalidTextureIndex) ? scene_->escapeSmokeTextureIndex_ : scene_->smokeTextureIndex_);
	if (!smokeMgr || wispyTex == TextureManager::kInvalidTextureIndex) return;

	const float emitInterval = unlocked ? kFlare.smokeIntervalUnlocked : kFlare.smokeIntervalLocked;
	const int puffLayers = unlocked ? kFlare.smokeLayersUnlocked : kFlare.smokeLayersLocked;
	const Vector2 wind = unlocked ? kFlare.smokeWindUnlocked : kFlare.smokeWindLocked;
	const Vector2 lifeRange = unlocked ? kFlare.smokeLifeUnlocked : kFlare.smokeLifeLocked;
	scene_->escapeSmokeTimer_ += deltaTime;

	while (scene_->escapeSmokeTimer_ >= emitInterval)
	{
		scene_->escapeSmokeTimer_ -= emitInterval;
		for (int k = 0; k < puffLayers; ++k)
		{
			const SmokePuffLayerSpec& layer = kSmokePuffLayers[k];
			const Vector3 pos = ventPos + Vector3{
				RandRange(-kFlare.smokeJitter, kFlare.smokeJitter),
				RandRange(0.0f, kFlare.smokeJitter),
				RandRange(-kFlare.smokeJitter, kFlare.smokeJitter)
			};
			const Vector2 initR   = unlocked ? layer.initUnlocked : layer.initLocked;
			const Vector2 targetR = unlocked ? layer.targetUnlocked : layer.targetLocked;
			const Vector2 riseR   = unlocked ? layer.riseUnlocked : layer.riseLocked;
			const float initScale   = RandRange(initR.x, initR.y);
			const float targetScale = RandRange(targetR.x, targetR.y);
			const float riseY       = RandRange(riseR.x, riseR.y);
			const Vector2 aspect    = layer.randomAspect
				? Vector2{ RandRange(layer.aspectX.x, layer.aspectX.y), RandRange(layer.aspectY.x, layer.aspectY.y) }
				: Vector2{ layer.aspectX.x, layer.aspectY.x };
			const float rotZ   = RandRange(0.0f, 2.0f * kPi);
			const float rAngle = RandRange(0.0f, 2.0f * kPi);
			const float rSpeed = RandRange(kFlare.smokeRadialSpd.x, kFlare.smokeRadialSpd.y);
			const float life   = RandRange(lifeRange.x, lifeRange.y);
			const float angVel = RandRange(-kFlare.smokeAngVel, kFlare.smokeAngVel);
			const Vector3 vel  = { std::cos(rAngle) * rSpeed + wind.x, riseY, std::sin(rAngle) * rSpeed + wind.y };

			AppParticle smokeP = BuildPresetParticle(pos, rotZ, initScale, aspect, vel, life, wispyTex, kPresetFlareSmoke);
			smokeP.targetScale = targetScale;
			smokeP.color = { layer.colorRgb.x, layer.colorRgb.y, layer.colorRgb.z, unlocked ? layer.alphaUnlocked : layer.alphaLocked };
			smokeP.drag = layer.drag;
			smokeP.slotLayer = layer.slotLayer;
			smokeP.angularVelocity = angVel;
			smokeP.wobbleFreq = kFlare.smokeWobbleFreq;
			smokeP.wobbleAmp = kFlare.smokeWobbleAmp;
			smokeP.wobbleAxis = { std::sin(rAngle), kFlare.smokeWobbleAxisY, std::cos(rAngle) };
			smokeP.curlFreq = kFlare.smokeCurlFreq;
			smokeP.curlAmp = kFlare.smokeCurlAmp;
			smokeMgr->AddParticle(smokeP);
		}
	}
}

void EnvironmentSystem::UpdateBarrier()
{
	if (!scene_ || !scene_->sphere_) return;

	Sprite::Transform transformSphere = scene_->sphere_->GetTransform();
	transformSphere.rotate.y += kBarrier.rotStep;
	if (!scene_->allTargetsDestroyed_)
	{
		transformSphere.translate = scene_->goalRingTransform_.translate;
		transformSphere.translate.y = kBarrier.centerY;
		barrierTimer_ += GamePlayScene::kFixedDeltaTime * kBarrier.pulseSpeedMult;
		const float scaled = kBarrier.baseScale * (1.0f + kBarrier.pulseAmp * std::sin(barrierTimer_));
		transformSphere.scale = { scaled, scaled, scaled };
	}
	else
	{
		transformSphere.translate = kBarrier.hiddenPos;
		transformSphere.scale = { 0.0f, 0.0f, 0.0f };
	}
	scene_->sphere_->SetTransform(transformSphere);
	scene_->sphere_->Update();
}

void EnvironmentSystem::UpdateRiverEffects(float deltaTime)
{
	if (!scene_ || !scene_->particleManager || !scene_->appParticleManager_) return;

	AppParticleManager* smokeMgr = scene_->smokeParticleManager_ ? scene_->smokeParticleManager_.get() : scene_->appParticleManager_.get();
	const uint32_t foamTex = (scene_->waterFoamTextureIndex_ != TextureManager::kInvalidTextureIndex) ? scene_->waterFoamTextureIndex_ : scene_->particleTextureB;
	const uint32_t wispyTex = (scene_->wispySmokeTextureIndex_ != TextureManager::kInvalidTextureIndex) ? scene_->wispySmokeTextureIndex_ : scene_->escapeSmokeTextureIndex_;
	const uint32_t glintTex = (scene_->sparkStreakTextureIndex_ != TextureManager::kInvalidTextureIndex) ? scene_->sparkStreakTextureIndex_ : scene_->particleTextureB;
	auto& rng = scene_->particleManager->GetRandomEngine();

	riverWaveTimer_ += deltaTime;
	if (riverWaveTimer_ >= GameConfig::Environment::kRiverWaveInterval)
	{
		std::uniform_real_distribution<float> zDist(GameConfig::Environment::kRiverZMin, GameConfig::Environment::kRiverZMax);
		std::uniform_real_distribution<float> xDist(kRiverFx.waveXRange.x, kRiverFx.waveXRange.y);
		std::uniform_real_distribution<float> speedDist(GameConfig::Environment::kRiverWaveSpeedMin, GameConfig::Environment::kRiverWaveSpeedMax);

		// 1. ストレートアルファ水面白泡フィラメント ＆ 橋脚スプレー
		if (smokeMgr)
		{
			for (int i = 0; i < kRiverFx.foamCount; ++i)
			{
				const float sx = xDist(rng);
				if (IsNearBridgeDeckX(sx, kRiverFx.bridgeWaveClearance)) continue;
				const Vector3 pos = { sx, GameConfig::Environment::kRiverWaveParticleY, zDist(rng) };
				const float fs = RandRange(kRiverFx.foamScale.x, kRiverFx.foamScale.y);
				const Vector3 vel = { -speedDist(rng), 0.0f, RandRange(-kRiverFx.foamVelZ, kRiverFx.foamVelZ) };
				const float life = RandRange(kPresetRiverFoam.lifeRange.x, kPresetRiverFoam.lifeRange.y);
				smokeMgr->AddParticle(BuildPresetParticle(pos, 0.0f, fs, kPresetRiverFoam.aspect, vel, life, foamTex, kPresetRiverFoam));
			}

			const uint32_t sprayWispyTex = (wispyTex != TextureManager::kInvalidTextureIndex) ? wispyTex : foamTex;
			for (float pillarX : kBridgePillarUpstreamX)
			{
				if (std::rand() % kRiverFx.pillarRollMod < kRiverFx.pillarRollChance)
				{
					const Vector3 pPos = { pillarX, GameConfig::Environment::kRiverSplashY, zDist(rng) };
					smokeMgr->EmitRealisticRiverSpray(rng, pPos, { -1.0f, 0.0f, 0.0f }, kRiverFx.pillarSpraySpd, foamTex, sprayWispyTex);
				}
			}
		}

		// 2. 午後の陽光を反射してキラッと瞬く極小の波頭サン・グリント（加算合成）
		for (int i = 0; i < kRiverFx.glintCount; ++i)
		{
			const float sx = xDist(rng);
			if (IsNearBridgeDeckX(sx, kRiverFx.bridgeWaveClearance)) continue;
			const Vector3 pos = { sx, GameConfig::Environment::kRiverWaveParticleY + kRiverFx.glintYOffset, zDist(rng) };
			const float gs = RandRange(kRiverFx.glintScale.x, kRiverFx.glintScale.y);
			const Vector3 vel = { -speedDist(rng), 0.0f, 0.0f };
			const float life = RandRange(kPresetRiverGlint.lifeRange.x, kPresetRiverGlint.lifeRange.y);
			AppParticle gl = BuildPresetParticle(pos, 0.0f, gs, kPresetRiverGlint.aspect, vel, life, glintTex, kPresetRiverGlint);
			gl.twinkleSpeed = kRiverFx.glintTwinkleSpd;
			gl.twinklePhase = RandRange(0.0f, kTwoPi);
			scene_->appParticleManager_->AddParticle(gl);
		}
		riverWaveTimer_ = 0.0f;
	}

	riverSplashTimer_ += deltaTime;
	if (riverSplashTimer_ >= GameConfig::Environment::kRiverSplashInterval)
	{
		std::uniform_real_distribution<float> rxDist(-kRiverFx.splashXRange, kRiverFx.splashXRange);
		std::uniform_real_distribution<float> rzDist(GameConfig::Environment::kRiverZMin, GameConfig::Environment::kRiverZMax);
		const float sx = rxDist(rng);
		if (!IsNearBridgeDeckX(sx, kRiverFx.bridgeWaveClearance) && smokeMgr)
		{
			smokeMgr->EmitRiverSplashDroplet(rng, { sx, GameConfig::Environment::kRiverSplashY, rzDist(rng) }, foamTex);
		}
		riverSplashTimer_ = 0.0f;
	}

	// 水面直上を下流 (-X) へたなびく薄く涼やかな川霧
	riverMistTimer_ += deltaTime;
	if (riverMistTimer_ >= kRiverFx.mistInterval)
	{
		riverMistTimer_ = 0.0f;
		if (smokeMgr && wispyTex != TextureManager::kInvalidTextureIndex)
		{
			const float mx = RandRange(kRiverFx.mistX.x, kRiverFx.mistX.y);
			if (!IsNearBridgeDeckX(mx, kRiverFx.bridgeMistClearance))
			{
				const Vector3 pos = { mx, kRiverFx.mistY, RandRange(kRiverFx.mistZ.x, kRiverFx.mistZ.y) };
				const float initSc = RandRange(kRiverFx.mistScale.x, kRiverFx.mistScale.y);
				const float rotZ = RandRange(-kRiverFx.mistRotZ, kRiverFx.mistRotZ);
				const Vector3 vel = { RandRange(kRiverFx.mistVelX.x, kRiverFx.mistVelX.y), RandRange(kRiverFx.mistVelY.x, kRiverFx.mistVelY.y), RandRange(-kRiverFx.mistVelZ, kRiverFx.mistVelZ) };
				const float life = RandRange(kPresetRiverMist.lifeRange.x, kPresetRiverMist.lifeRange.y);
				smokeMgr->AddParticle(BuildPresetParticle(pos, rotZ, initSc, kPresetRiverMist.aspect, vel, life, wispyTex, kPresetRiverMist));
			}
		}
	}
}

void EnvironmentSystem::UpdateHitEffect(float deltaTime)
{
	if (!scene_ || !scene_->hitEffect_) return;
	scene_->hitEffect_->SetPlaneParticleCount(scene_->emitter.count);
	scene_->hitEffect_->Update(deltaTime);
}
