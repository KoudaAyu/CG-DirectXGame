#include "SceneManager.h"
#include "SkyBox.h"
#include "MaterialManager.h"
#include "Light.h"
#include "ParticleManager.h"
#include "RootParam.h"
#include "RenderContext.h"
#include "Baziru3_Engine\Graphics\Graphics\SceneRenderRequests.h"
#include "SpriteManager.h"
#include "TextureManager.h"
#include "AudioManager.h"
#include "../../Player/Player.h"
#include "../../Enemy/Enemy.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <initializer_list>
#include <numbers>
#include <Windows.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include "Baziru3_Engine/Framework/IO/JsonSafeLoader.h"
#include "Baziru3_Engine/Framework/AI/NavMesh.h"

#include "GamePlayScene.h"
#include "GamePlayLevelLayout.h"
#include "Application/Config/GameConfig.h"
#include "Application/Config/AppDebugConfig.h"
#include "Application/Transition/StingerTransition.h"
#include "Application/Transition/DeathTransition.h"
#include "RaidStats.h"
#include "Baziru3_Engine/Core/Base/Allocator/ConstantBufferAllocator.h"
#include "Application/UI/TacticalUICanvas.h"

#include "CombatSystem.h"
#include "Bullet.h"
#include "CollisionSystem.h"
#include "EnvironmentSystem.h"
#include "CharacterEffectController.h"
#include "Baziru3_Engine/Framework/Collision/CollisionManager.h"
#include "Baziru3_Engine/Framework/Collision/SphereCollider.h"
#include "Baziru3_Engine/Framework/Collision/BoxCollider.h"
#include "Baziru3_Engine/Framework/Collision/CapsuleCollider.h"

namespace
{
	constexpr float kPi     = GameConfig::Math::kPi;
	constexpr float kTwoPi  = GameConfig::Math::kTwoPi;
	constexpr float kHalfPi = GameConfig::Math::kHalfPi;

	// --- シーン全体・ライティング・カメラ基本定数 ---
	constexpr float   kMaxFrameDeltaTime = 0.1f;
	constexpr float   kSmokeGroundY      = 0.025f;
	constexpr Vector3 kSunRawDirection   = { -0.42f, -0.78f, 0.46f };
	constexpr Vector4 kSunLightColor     = { 1.0f, 0.94f, 0.82f, 1.0f };
	constexpr float   kSunBaseIntensity  = 1.35f;
	constexpr float   kSunFlashIntensity = 1.48f;
	constexpr float   kCameraFarZ        = 1800.0f;
	constexpr float   kSmoothStepC1      = 3.0f;
	constexpr float   kSmoothStepC2      = 2.0f;
	constexpr Vector4 kPadUnlockedColor  = { 1.08f, 1.10f, 1.08f, 1.0f };
	constexpr Vector4 kPadLockedColor    = { 0.86f, 0.87f, 0.90f, 1.0f };

	// --- HitEffect・ヘリパッド・エミッター初期設定 ---
	constexpr struct
	{
		uint32_t cylSegs, ringSegs, emitterCount;
		float cylHeight, cylRadius, ringInner, ringOuter, ringScale, duration, padBoundingRadius, emitterFreq;
	} kEnvInit = { 64, 32, 3, 1.0f, 0.2f, 1.0f, 1.0f, 3.0f, 0.35f, 4.0f, 0.5f };

	// --- 増援・スプライトUI・ビネット・速度線定数 ---
	constexpr struct { float shakeDur, shakeInt, bannerTime, radioVol; } kReinforceCfg = { 0.42f, 0.65f, 4.2f, 0.78f };

	constexpr struct
	{
		Vector2 hpPos, hpSize, staminaPos, staminaSize, reloadBarSize, vignetteSize;
		float reloadHeadOffsetY;
		Vector4 barBgColor, hpFgColor, staminaFgColor, reloadFgColor;
		int speedLineCount;
		float speedFadeIn, speedMaxAlpha, speedFadeOut, speedDistBase, speedDistAmp, speedWidthBase, speedWidthAmp, speedPhaseMult, speedHeightRatio;
		float lowHpThreshold, heartPulseBase, heartPulseGain, heartIntBase, heartIntGain, heartVolBase, heartVolGain, vignetteFadeOut;
		uint32_t heartCooldownMs;
	} kHudSpriteCfg = {
		{ 20.0f, 20.0f }, { 200.0f, 16.0f }, { 20.0f, 40.0f }, { 200.0f, 10.0f }, { 60.0f, 6.0f }, { 1280.0f, 720.0f }, 1.6f,
		{ 0.1f, 0.1f, 0.1f, 0.8f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.8f, 1.0f, 1.0f }, { 0.0f, 1.0f, 0.5f, 1.0f }, 8,
		10.0f, 0.6f, 4.0f, 0.8f, 0.2f, 8.0f, 4.0f, 10.0f, 0.7f,
		0.3f, 6.0f, 6.0f, 0.2f, 0.3f, 0.44f, 0.30f, 2.0f, 220
	};
	constexpr int kMatRowTrans = 3;
	constexpr int kMatColW     = 3;

	// --- デバッグ入力 & カメラ操作定数 ---
	constexpr struct
	{
		float debugFxOffsetY, debugParticleLife, minForwardLen, debugAttackerDist, lethalDamage;
		float uiClickVol, uiEscClickVol, zoomKeyStep, wheelDeadzone, wheelZoomScale;
		uint32_t uiClickCooldownMs;
	} kInputCfg = { 1.5f, 0.35f, 0.1f, 14.0f, 9999.0f, 0.56f, 0.50f, 0.015f, 0.01f, 0.08f, 60 };

	// --- プレイヤーリロード・回避・足音パーティクルプリセット ---
	struct SceneParticleSpec { Vector2 aspect; float gravity, drag, bounce, fadeIn; uint16_t slotLayer; };
	constexpr SceneParticleSpec kSpecBoltWisp    = { { 1.0f,  1.0f  }, 0.0f,  2.8f, 0.0f,  0.0f,  0 };
	constexpr SceneParticleSpec kSpecKickDust    = { { 1.55f, 0.72f }, 0.0f,  2.8f, 0.0f,  0.14f, 1 };
	constexpr SceneParticleSpec kSpecKickChip    = { { 1.15f, 0.85f }, 15.2f, 0.8f, 0.38f, 0.0f,  2 };
	constexpr SceneParticleSpec kSpecKickFeather = { { 0.78f, 1.25f }, 1.45f, 2.6f, 0.0f,  0.0f,  2 };
	constexpr SceneParticleSpec kSpecLandDust    = { { 1.60f, 0.70f }, 0.0f,  2.9f, 0.0f,  0.14f, 1 };
	constexpr SceneParticleSpec kSpecLandChip    = { { 1.0f,  1.0f  }, 15.0f, 0.0f, 0.36f, 0.0f,  2 };

	constexpr struct
	{
		float magOutVol, stage1Prog, stage2Prog, stage2ShakeDur, stage2ShakeInt, magInVol;
		float stage3Prog, dryShakeDur, dryShakeInt, tacShakeDur, tacShakeInt, dryBoltVol, tacBoltVol;
		float wispOffsetY, wispRotZ, wispInitSc, wispTargetSc, wispSideSpd, wispUpSpd, wispLife;
		Vector4 wispStartCol, wispEndCol;
	} kReloadFx = {
		0.72f, 0.15f, 0.67f, 0.032f, 0.038f, 0.76f,
		0.86f, 0.048f, 0.058f, 0.028f, 0.032f, 0.80f, 0.65f,
		0.04f, 1.2f, 0.06f, 0.18f, 0.55f, 0.22f, 0.26f,
		{ 0.86f, 0.88f, 0.90f, 0.22f }, { 0.78f, 0.80f, 0.82f, 0.0f }
	};

	constexpr struct
	{
		float startShakeDur, startShakeInt, startVol, soundMaxRad, soundDuration, bridgeMinZ, bridgeMaxZ, bridgeAbsX, bridgeHalfWidth, jitterRange;
		int kickDustCount, kickChipCount, kickFeatherCount, landDustCount, landChipCount, modTwo, modThree;
		float kdRadius, kdBridgeY, kdGroundY, kdBaseSc, kdStepSc, kdTargetMult, kdBaseSpd, kdStepSpd, kdBaseVy, kdStepVy, kdBaseLife, kdStepLife, kdAngMult;
		Vector4 kdBridgeCol, kdGroundCol, kdEndCol;
		float kcBridgeY, kcGroundY, kcBaseSc, kcStepSc, kcTargetMult, kcBaseSpd, kcStepSpd, kcBaseVy, kcStepVy, kcBaseLife, kcStepLife, kcAngVel;
		Vector4 kcBridgeCol, kcGroundCol, kcEndCol;
		float kfJitterPosMult, kfBaseY, kfStepY, kfBaseSc, kfStepSc, kfTargetMult, kfJitterVelMult, kfBaseVy, kfStepVy, kfBaseLife, kfStepLife, kfWobbleFreq, kfWobbleAmp, kfAngVel;
		Vector3 kfWobbleAxis;
		Vector4 kfCol0, kfCol1;
		float landShakeDur, landShakeInt, landVol, ldRadius, ldY, ldBaseSc, ldStepSc, ldTargetMult, ldSpd, ldVy, ldBaseLife, ldStepLife;
		Vector4 ldCol;
		float lcY, lcBaseSc, lcStepSc, lcSpd, lcBaseVy, lcStepVy, lcLife, lcAngVel;
		Vector2 lcAspect;
		Vector4 lcCol;
		float stepNoiseInterval, stepNoiseMaxRad, stepNoiseDuration, soundExpandSpeed;
	} kMoveFx = {
		0.12f, 0.22f, 0.68f, 10.0f, 0.4f, 26.0f, 34.0f, 14.0f, 1.55f, 0.12f,
		8, 6, 2, 8, 5, 2, 3,
		0.16f, 0.08f, 0.04f, 0.16f, 0.04f, 2.4f, 1.85f, 0.55f, 0.18f, 0.08f, 0.36f, 0.07f, 8.0f,
		{ 0.54f, 0.43f, 0.30f, 0.28f }, { 0.62f, 0.54f, 0.42f, 0.36f }, { 0.68f, 0.62f, 0.52f, 0.0f },
		0.09f, 0.05f, 0.024f, 0.008f, 0.7f, 1.6f, 0.85f, 1.25f, 0.55f, 0.34f, 0.12f, 14.0f,
		{ 0.46f, 0.34f, 0.20f, 0.92f }, { 0.44f, 0.38f, 0.28f, 0.92f }, { 0.38f, 0.33f, 0.24f, 0.0f },
		1.5f, 0.38f, 0.12f, 0.09f, 0.025f, 0.92f, 3.0f, 0.85f, 0.35f, 0.95f, 0.25f, 8.5f, 0.45f, 3.8f,
		{ 1.0f, 0.15f, 0.6f }, { 1.0f, 0.90f, 0.36f, 0.92f }, { 0.98f, 0.97f, 0.92f, 0.90f },
		0.12f, 0.25f, 0.62f, 0.18f, 0.04f, 0.18f, 0.05f, 2.5f, 2.15f, 0.22f, 0.40f, 0.07f,
		{ 0.62f, 0.54f, 0.42f, 0.38f },
		0.05f, 0.025f, 0.008f, 2.2f, 1.15f, 0.45f, 0.34f, 12.0f, { 1.2f, 0.85f }, { 0.44f, 0.38f, 0.28f, 0.90f },
		0.35f, 6.0f, 0.25f, 30.0f
	};

	// --- 遮蔽判定・敵AI連携・リスポーン演出定数 ---
	struct PortalRingSpec { int count; float radius, scale, velY, life; };
	constexpr PortalRingSpec kRespawnRings[] = { { 24, 0.7f, 0.65f, 2.2f, 0.8f }, { 16, 0.4f, 0.55f, 2.8f, 0.7f } };

	constexpr struct
	{
		float minCoverExtentY, coverReachDist, coverRotEps;
		float alertVol, alertMinDist, alertMaxDist, questionVol, questionMinDist, questionMaxDist;
		uint32_t aiChirpCooldownMs, stepAudioCooldownMs;
		float squadAlertRadius, squadNoiseRadius, minEnemySpacing, minSepDist, sepAngStepI, sepAngStepJ, sepPushRate;
		float stepDustChanceRate, stepAudioVol, stepAudioMinDist, stepAudioMaxDist;
		int coreCount;
		float coreOffsetH, coreOffsetV, coreScale, coreVelY, coreLife, respawnScaleMult;
		Vector4 coreColor, respawnPortalColor;
	} kAiCfg = {
		0.35f, 0.88f, 1e-5f,
		0.70f, 6.0f, 38.0f, 0.62f, 5.0f, 32.0f, 300, 95,
		9.0f, 14.0f, 1.65f, 0.001f, 1.7f, 0.9f, 3.8f,
		4.5f, 0.34f, 3.0f, 20.0f,
		8, 0.1f, 0.4f, 0.75f, 3.2f, 0.6f, 1.25f,
		{ 1.0f, 1.0f, 1.0f, 0.95f }, { 1.0f, 0.7f, 0.0f, 0.95f }
	};

	// --- シネマティック進行・BGM・カメラ・ライト・浮遊テキスト定数 ---
	constexpr struct
	{
		float defaultKillerOffsetZ, maxSearchDistSq;
		Vector4 introHeliLightColor, flareLightColor;
		Vector3 introHeliLightOffset, flareAudioOffset, flareLightOffset;
		float introHeliLightInt, introHeliLightRad, introHeliLightDecay;
		float bgmTensionPeriod, bgmAlertPeriod, bgmIntroVol, bgmClearVol, bgmAlertVol, bgmNormalVol;
		float flareAudioMinDist, flareAudioMaxDist, flareAudioPeriod, flareAudioBaseVol;
		float slowTimeScale, clearClimaxShakeDur, clearClimaxShakeInt, clearTimeScale, clearSmokeDtScale, clearFadeSec;
		Vector3 heroEyeBase, heroEyeDelta, heroTargetBase, heroTargetDelta;
		float minXzLen, heroRollScale, heroCamBlendRate, heroBreathAmp, heroPitchScale, heroYawScale;
		float overheadBlendSpeed, cursorClamp, cursorDeadzone, cursorActiveRange, overheadLookScale, lookDampSpeed;
		float flarePulseFreqRaid, flarePulseFreqSlow, flarePulseBase, flarePulseAmp;
		float flareIntClear, flareIntUnlocked, flareIntLocked, flareRadUnlocked, flareRadLocked, flareDecay;
		float floatTextRiseSpd, floatTextCritFreq, floatTextCritAmp, floatTextJitterXZ, floatTextCritLife, floatTextNormLife;
	} kCineCfg = {
		15.0f, 9999.0f * 9999.0f,
		{ 1.0f, 0.90f, 0.72f, 1.0f }, { 0.18f, 1.0f, 0.48f, 1.0f },
		{ 0.0f, -2.4f, 0.0f }, { 1.8f, 0.18f, 1.2f }, { 1.50f, 0.95f, 1.10f },
		1.35f, 18.0f, 1.6f,
		24.00f, 16.00f, 0.20f, 0.22f, 0.34f, 0.30f,
		3.5f, 24.0f, 3.00f, 0.40f,
		0.15f, 0.32f, 0.22f, 0.35f, 0.72f, 0.5f,
		{ -2.40f, 1.52f, -5.75f }, { 0.55f, -0.14f, 0.65f }, { 0.0f, 1.05f, 0.0f }, { 1.15f, 0.95f, 1.95f },
		0.001f, -0.025f, 3.8f, 0.015f, -0.07f, 0.55f,
		5.5f, 1.2f, 0.08f, 0.92f, 0.35f, 6.5f,
		7.5f, 9.0f, 0.85f, 0.15f,
		2.45f, 1.95f, 0.65f, 11.5f, 5.0f, 1.5f,
		1.5f, 20.0f, 0.15f, 0.3f, 1.0f, 0.75f
	};

	constexpr size_t kJsonErrorBufferSize = 256;

	enum class KeyEdgeId { Debug9, Debug8, DebugF1, DebugF3, DebugSuicide, ToggleCamY, ToggleMapM };
	template <KeyEdgeId Id>
	bool ConsumeKeyEdge(bool curDown) noexcept
	{
		static bool prev = false;
		const bool triggered = curDown && !prev;
		prev = curDown;
		return triggered;
	}

	constexpr float SmoothStep01(float t) noexcept { return t * t * (kSmoothStepC1 - kSmoothStepC2 * t); }

	uint32_t LoadTexWithFallback(const char* path, uint32_t fallback = TextureManager::kInvalidTextureIndex)
	{
		const uint32_t idx = TextureManager::GetInstance()->Load(path);
		return (idx != TextureManager::kInvalidTextureIndex) ? idx : fallback;
	}

	void HideSpriteBarPair(Sprite* bg, Sprite* fg)
	{
		if (bg) { bg->SetSize({ 0.0f, 0.0f }); bg->Update(); }
		if (fg) { fg->SetSize({ 0.0f, 0.0f }); fg->Update(); }
	}

	void HideSpriteAtIndex(std::vector<std::unique_ptr<Sprite>>& pool, int idx)
	{
		if (idx >= 0 && idx < static_cast<int>(pool.size()))
		{
			if (Sprite* s = pool[idx].get()) { s->SetSize({ 0.0f, 0.0f }); s->Update(); }
		}
	}

	RenderContext MakeTexturedContext(const RenderContext& baseCtx, uint32_t texIdx)
	{
		RenderContext out = baseCtx;
		if (texIdx != TextureManager::kInvalidTextureIndex) out.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(texIdx);
		return out;
	}

	bool CreateWhiteSpritePair(
		SpriteCom* sc, std::vector<std::unique_ptr<Sprite>>& pool,
		Vector2 anchorBg, Vector2 anchorFg, Sprite*& outBg, Sprite*& outFg)
	{
		if (!sc) return false;
		const Sprite::Transform defaultTf = { { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
		auto bg = Sprite::Create(sc, defaultTf, "Resources/CG4/human/white.png");
		auto fg = Sprite::Create(sc, defaultTf, "Resources/CG4/human/white.png");
		if (!bg || !fg) return false;
		bg->SetAnchorPoint(anchorBg);
		fg->SetAnchorPoint(anchorFg);
		pool.emplace_back(std::move(bg));
		pool.emplace_back(std::move(fg));
		outBg = pool[pool.size() - 2].get();
		outFg = pool[pool.size() - 1].get();
		return true;
	}

	AppParticle MakeSceneParticle(
		const Vector3& pos, float rotZ, float drawScale, Vector2 drawAspect, float initSc, float targetSc,
		const Vector3& vel, const Vector4& startCol, const Vector4& endCol,
		float life, uint32_t texIdx, float angVel, const SceneParticleSpec& spec)
	{
		AppParticle p{};
		p.transform.Initialize();
		p.transform.SetTranslate(pos);
		p.transform.SetRotate({ 0.0f, 0.0f, rotZ });
		p.scaleAspect = spec.aspect;
		p.initialScale = initSc;
		p.targetScale = targetSc;
		p.transform.SetScale({ drawScale * drawAspect.x, drawScale * drawAspect.y, 1.0f });
		p.velocity = vel;
		p.color = startCol;
		p.endColor = endCol;
		p.hasColorShift = true;
		p.lifeTime = life;
		p.currentTime = 0.0f;
		p.textureIndex = texIdx;
		p.gravity = spec.gravity;
		p.drag = spec.drag;
		p.bounceElasticity = spec.bounce;
		p.angularVelocity = angVel;
		p.fadeInRatio = spec.fadeIn;
		p.slotLayer = spec.slotLayer;
		return p;
	}

	Vector3 ReadJsonVec3(const nlohmann::json& node)
	{
		return { node["x"].get<float>(), node["y"].get<float>(), node["z"].get<float>() };
	}

	bool ContainsAny(const std::string& text, std::initializer_list<const char*> keywords)
	{
		for (const char* kw : keywords) { if (text.find(kw) != std::string::npos) return true; }
		return false;
	}

	void BuildSmokeFlareCanisterModel(Object3d::ModelData& data)
	{
		using namespace GamePlayLevelLayout;
		data.vertices.clear();
		data.indices.clear();

		auto AddBox = [&](Vector3 center, Vector3 size, Vector2 uvMin, Vector2 uvMax) {
			const Vector3 h = size * 0.5f;
			Vector3 p[std::size(kUnitBoxCorners)]{};
			for (size_t i = 0; i < std::size(kUnitBoxCorners); ++i)
			{
				p[i] = { center.x + kUnitBoxCorners[i].x * h.x, center.y + kUnitBoxCorners[i].y * h.y, center.z + kUnitBoxCorners[i].z * h.z };
			}
			const Vector2 faceUvs[] = { { uvMin.x, uvMax.y }, { uvMax.x, uvMax.y }, { uvMax.x, uvMin.y }, { uvMin.x, uvMin.y } };
			for (size_t f = 0; f < std::size(kUnitBoxFaces); ++f)
			{
				const uint32_t base = static_cast<uint32_t>(data.vertices.size());
				for (size_t v = 0; v < std::size(faceUvs); ++v)
				{
					const Vector3& pt = p[kUnitBoxFaces[f][v]];
					data.vertices.push_back({ { pt.x, pt.y, pt.z, 1.0f }, faceUvs[v], kUnitBoxNormals[f] });
				}
				for (uint32_t idx : kQuadTriIndices) data.indices.push_back(base + idx);
			}
		};

		auto AddCylinderSegment = [&](float y0, float y1, float r, Vector2 uv) {
			for (int i = 0; i < kCanisterSegments; ++i)
			{
				const float a0 = (static_cast<float>(i) / kCanisterSegments) * 2.0f * kPi;
				const float a1 = (static_cast<float>(i + 1) / kCanisterSegments) * 2.0f * kPi;
				const float x0 = std::cos(a0) * r, z0 = std::sin(a0) * r;
				const float x1 = std::cos(a1) * r, z1 = std::sin(a1) * r;
				Vector3 norm = { (x0 + x1) * 0.5f, 0.0f, (z0 + z1) * 0.5f };
				const float nlen = std::sqrt(norm.x * norm.x + norm.z * norm.z);
				if (nlen > kNormalMinLength) norm = { norm.x / nlen, 0.0f, norm.z / nlen };

				const uint32_t base = static_cast<uint32_t>(data.vertices.size());
				data.vertices.push_back({ { x0, y0, z0, 1.0f }, uv, norm });
				data.vertices.push_back({ { x1, y0, z1, 1.0f }, uv, norm });
				data.vertices.push_back({ { x1, y1, z1, 1.0f }, uv, norm });
				data.vertices.push_back({ { x0, y1, z0, 1.0f }, uv, norm });
				for (uint32_t idx : kQuadTriIndices) data.indices.push_back(base + idx);
			}
		};

		auto AddCylinderCap = [&](float y, float normalY, bool reverseWinding) {
			const Vector3 capNormal = { 0.0f, normalY, 0.0f };
			for (int i = 0; i < kCanisterSegments; ++i)
			{
				const float a0 = (static_cast<float>(i) / kCanisterSegments) * 2.0f * kPi;
				const float a1 = (static_cast<float>(i + 1) / kCanisterSegments) * 2.0f * kPi;
				const uint32_t b = static_cast<uint32_t>(data.vertices.size());
				data.vertices.push_back({ { 0.0f, y, 0.0f, 1.0f }, kUvMetal, capNormal });
				data.vertices.push_back({ { std::cos(a0) * kCanisterBaseRadius, y, std::sin(a0) * kCanisterBaseRadius, 1.0f }, kUvMetal, capNormal });
				data.vertices.push_back({ { std::cos(a1) * kCanisterBaseRadius, y, std::sin(a1) * kCanisterBaseRadius, 1.0f }, kUvMetal, capNormal });
				data.indices.push_back(b);
				data.indices.push_back(b + (reverseWinding ? 2u : 1u));
				data.indices.push_back(b + (reverseWinding ? 1u : 2u));
			}
		};

		// 1. M18軍用スモークグレネード缶本体・上下キャップ / 2. M201A1信管アセンブリ
		for (const auto& band : kCanisterBands) AddCylinderSegment(band.y0, band.y1, kCanisterBaseRadius * band.radiusMult, band.uv);
		AddCylinderCap(0.0f, -1.0f, true);
		AddCylinderCap(kCanisterTopCapY, 1.0f, false);
		for (const auto& box : kCanisterBoxes) AddBox(box.center, box.size, box.uvMin, box.uvMax);
		data.boundingRadius = kCanisterBoundingRadius;
	}
}

GamePlayScene::GamePlayScene() = default;
GamePlayScene::~GamePlayScene() = default;

void GamePlayScene::InitializeScene()
{
	directXCom = dxCommon_;
	camera_ = BaseScene::camera_;
	assert(directXCom != nullptr);

	lastTime_ = std::chrono::steady_clock::now();
	hitStopTimer_ = 0.0f;

	object3dCom = GetObject3dCom();
	skinningObject3dCom = GetSkinningObject3dCom();
	materialManager = GetMaterialManager();
	light = GetLight();
	particleManager = GetParticleManager();
	appParticleManager_ = std::make_unique<AppParticleManager>();
	appParticleManager_->Initialize(particleManager);
	smokeParticleManager_ = std::make_unique<AppParticleManager>();
	smokeParticleManager_->Initialize(particleManager);
	smokeParticleManager_->SetAlphaBlend(true);
	smokeParticleManager_->SetStableInstanceSlots(true);
	smokeParticleManager_->SetGroundY(kSmokeGroundY);

	// シネマティックライティング設定（温かい午後の陽光と陰影）
	if (light)
	{
		Vector3 sunDir = kSunRawDirection;
		const float len = std::sqrt(sunDir.x * sunDir.x + sunDir.y * sunDir.y + sunDir.z * sunDir.z);
		light->SetDirectionalLight(kSunLightColor, { sunDir.x / len, sunDir.y / len, sunDir.z / len }, kSunBaseIntensity);
	}
	if (SceneManager::GetInstance()) SceneManager::GetInstance()->SetShowSkybox(true);

	// カメラの初期化（Escape from Duckov 準拠の広角クォータービュー俯瞰）
	cameraZoom_ = 1.0f;
	isOverheadCameraMode_ = false;
	overheadCameraBlend_ = 0.0f;
	cameraLookaheadOffset_ = { 0.0f, 0.0f, 0.0f };
	isFullMapOpen_ = false;
	if (camera_)
	{
		camera_->SetFarZ(kCameraFarZ);
		camera_->SetTranslate(GameConfig::Camera::kDefaultFollowOffset * cameraZoom_);
		camera_->SetRotate(GameConfig::Camera::kFollowRotation);
		camera_->Update();
	}

	targets_.clear();
	allTargetsDestroyed_ = false;
	isGameCleared_ = false;
	clearClimaxTriggered_ = false;
	clearTransitionStarted_ = false;
	clearSlowMoTimer_ = kClearSlowMoDuration;
	isPlayerInExtractionZone_ = false;
	extractionTimer_ = kExtractionMaxTime;
	extractionUnlockBannerTimer_ = 0.0f;
	hitIndicatorTimer_ = 0.0f;
	hitIndicatorAngle_ = 0.0f;
	goalRingTransform_.translate = GameConfig::Environment::kExtractionPadPosition;

	InitializeEnvironment();
	InitializeCharacters();
	InitializeSprites();
	InitializeAudioAndParticles();
	InitializeObstacles();

	// サブシステム初期化
	lootSystem_ = std::make_unique<LootSystem>();
	lootSystem_->Initialize();
	hud_ = std::make_unique<GamePlayHUD>();
	hud_->Initialize();
	combatSystem_ = std::make_unique<CombatSystem>(this);
	collisionSystem_ = std::make_unique<CollisionSystem>(this);
	environmentSystem_ = std::make_unique<EnvironmentSystem>(this);
	environmentSystem_->Initialize();
	characterEffectController_ = std::make_unique<CharacterEffectController>(this);
	characterEffectController_->Initialize();
	RaidStats::GetInstance().Reset();

	showDebugGizmos_ = false;
	showPerformanceTracker_ = false;
	CollisionManager::GetInstance()->SetShowDebugColliders(false);
	CollisionManager::GetInstance()->SetShowMeshWireframe(false);

	helicopterIntro_ = std::make_unique<HelicopterInsertionController>(this);
	helicopterIntro_->Initialize();
	isGameIntroActive_ = true;

	deathSequenceController_ = std::make_unique<GameOverSequenceController>(this);
	deathSequenceController_->Initialize();

	// 初回フレーム描画用のWVP行列をイントロカメラで全3Dオブジェクト同期
	if (environmentSystem_) environmentSystem_->UpdateNatureAnimation(0.0f);
	SyncStageObjectsForCamera(0.0f, true);
}

void GamePlayScene::SyncStageObjectsForCamera(float targetDeltaTime, bool syncSkyBox)
{
	UpdateObstacles();
	if (extractionPadObject_) extractionPadObject_->Update();
	if (extractionSignalFlareObject_) extractionSignalFlareObject_->Update();
	for (auto& enemy : enemies_) { if (enemy) enemy->SyncCameraTransforms(); }
	for (auto& t : targets_) { if (t) t->Update(targetDeltaTime); }
	if (syncSkyBox && SceneManager::GetInstance() && SceneManager::GetInstance()->GetSkyBox())
	{
		SceneManager::GetInstance()->GetSkyBox()->Update();
	}
}

void GamePlayScene::InitializeEnvironment()
{
	if (!object3dCom || !materialManager || !light || !particleManager) return;

	hitEffect_ = std::make_unique<HitEffect>();
	hitEffect_->Initialize(
		directXCom, object3dCom, materialManager, light, camera_,
		kEnvInit.cylSegs, kEnvInit.cylHeight, kEnvInit.cylRadius,
		kEnvInit.ringSegs, kEnvInit.ringInner, kEnvInit.ringOuter, kEnvInit.ringScale);
	hitEffect_->SetParticleManager(particleManager);
	hitEffect_->SetCylinderEnabled(true);
	hitEffect_->SetRingEnabled(true);
	hitEffect_->SetEffectDuration(kEnvInit.duration);
	hitEffect_->GetCylinderTransform() = { { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
	hitEffect_->Update(kFixedDeltaTime);

	sphere_ = std::make_unique<Sphere>();
	sphere_->Initialize(directXCom, object3dCom, materialManager, light, camera_);

	// 立体脱出ヘリパッド (extraction_pad.obj + extraction_pad.png)
	{
		Object3d::ModelData padModel = Object3d::LoadObjFile("Resources", "extraction_pad.obj");
		padModel.material.textureFilePath = "Resources/extraction_pad.png";
		extractionPadTextureIndex_ = TextureManager::GetInstance()->Load("Resources/extraction_pad.png");
		padModel.material.textureIndex = extractionPadTextureIndex_;
		padModel.boundingRadius = kEnvInit.padBoundingRadius;

		extractionPadObject_ = std::make_unique<Object3d>();
		extractionPadObject_->Initialize(object3dCom, padModel);
		extractionPadObject_->SetCamera(camera_);
		extractionPadObject_->SetTranslate(GameConfig::Environment::kExtractionPadPosition);
		extractionPadObject_->SetScale({ 1.0f, 1.0f, 1.0f });
		extractionPadObject_->SetRotate({ 0.0f, 0.0f, 0.0f });
		extractionPadObject_->SetEnableLighting(false);
		extractionPadObject_->SetReflectionFactor(0.0f);
		extractionPadObject_->SetAllowWireframeOverlay(false);
		extractionPadObject_->Update();
	}

	// 立体脱出地点シグナル・発煙筒スモーク缶（Green Smoke Flare Canister）
	{
		Object3d::ModelData flareModel{};
		BuildSmokeFlareCanisterModel(flareModel);
		flareModel.material.textureFilePath = "Resources/container_military.png";
		flareModel.material.textureIndex = TextureManager::GetInstance()->Load("Resources/container_military.png");

		extractionSignalFlareObject_ = std::make_unique<Object3d>();
		extractionSignalFlareObject_->Initialize(object3dCom, flareModel);
		extractionSignalFlareObject_->SetCamera(camera_);
		extractionSignalFlareObject_->SetTranslate(GameConfig::Environment::kExtractionPadPosition + GamePlayLevelLayout::kFlarePadOffset);
		extractionSignalFlareObject_->SetScale({ 1.0f, 1.0f, 1.0f });
		extractionSignalFlareObject_->SetRotate(GamePlayLevelLayout::kFlarePadRotation);
		extractionSignalFlareObject_->Update();
	}

	isGameCleared_ = false;
	extractionTimer_ = kExtractionMaxTime;

	levelEditor_ = std::make_unique<LevelEditor>();
	levelEditor_->Initialize(directXCom, object3dCom);
}

void GamePlayScene::InitializeCharacters()
{
	if (!player_)
	{
		player_ = std::make_unique<Player>();
		player_->Initialize(object3dCom, camera_);
	}
}

Enemy* GamePlayScene::SpawnEnemy(
	const Vector3& position, bool isPatrol, float initialYaw,
	Enemy::TacticalRole role, const std::vector<Vector3>& customPatrolRoute, bool loopPatrol)
{
	using namespace GamePlayLevelLayout;
	if (!object3dCom || !camera_) return nullptr;

	std::unique_ptr<Enemy> newEnemy;
	SpriteCom* sc = spriteCom ? spriteCom : (SceneManager::GetInstance() ? SceneManager::GetInstance()->GetSpriteCom() : nullptr);
	const bool useMobileUnit = isPatrol || (role == Enemy::TacticalRole::Patrol) || (role == Enemy::TacticalRole::Flanker);

	if (useMobileUnit)
	{
		auto moving = std::make_unique<MovingEnemy>();
		moving->Initialize(object3dCom, camera_);
		const Enemy::TacticalRole effectiveRole = (role == Enemy::TacticalRole::Garrison) ? Enemy::TacticalRole::Patrol : role;
		moving->SetTacticalRole(effectiveRole);
		moving->SetPosition(position);

		std::vector<Vector3> patrolSpline = customPatrolRoute;
		if (patrolSpline.size() < 2)
		{
			patrolSpline.reserve(std::size(kDefaultPatrolOffsets));
			for (const Vector2& off : kDefaultPatrolOffsets) patrolSpline.push_back({ position.x + off.x, position.y, position.z + off.y });
		}
		moving->SetPatrolPath(patrolSpline, loopPatrol, (effectiveRole == Enemy::TacticalRole::Flanker) ? kFlankerPatrolSpeed : kStandardPatrolSpeed);

		Sprite *alertBar = nullptr, *alertDot = nullptr;
		if (CreateWhiteSpritePair(sc, sprites, { 0.5f, 1.0f }, { 0.5f, 0.0f }, alertBar, alertDot)) moving->SetAlertSprites(alertBar, alertDot);
		newEnemy = std::move(moving);
	}
	else
	{
		auto sentry = std::make_unique<Enemy>();
		sentry->Initialize(object3dCom, camera_);
		sentry->SetTacticalRole(role);
		sentry->SetPosition(position);
		if (initialYaw > kValidInitialYawMin) sentry->SetBaseYaw(initialYaw);
		newEnemy = std::move(sentry);
	}

	Sprite *hpBg = nullptr, *hpFg = nullptr;
	if (CreateWhiteSpritePair(sc, sprites, { 0.0f, 0.0f }, { 0.0f, 0.0f }, hpBg, hpFg)) newEnemy->SetHPBarSprites(hpBg, hpFg);

	Enemy* ret = newEnemy.get();
	enemies_.push_back(std::move(newEnemy));
	return ret;
}

void GamePlayScene::TriggerReinforcements()
{
	if (reinforcementTriggered_) return;
	reinforcementTriggered_ = true;

	// 脱出開放 ＆ 増援接近の戦術通知発動（アラーム音は鳴らさず無線通信のみ再生）
	TriggerCameraShake(kReinforceCfg.shakeDur, kReinforceCfg.shakeInt);
	extractionUnlockBannerTimer_ = kReinforceCfg.bannerTime;
	TacticalAudio::Play2D("Resources/Audio/SE_Radio_Reinforce.wav", kReinforceCfg.radioVol);

	const Vector3 alertTarget = player_ ? player_->GetPosition() : GameConfig::Environment::kExtractionPadPosition;
	const Vector3& padPos = GameConfig::Environment::kExtractionPadPosition;

	if (!reinforceSpawnPoints_.empty())
	{
		for (const auto& sp : reinforceSpawnPoints_)
		{
			const std::vector<Vector3> qrfRoute = {
				sp, { (sp.x + padPos.x) * 0.5f, 0.0f, sp.z + GamePlayLevelLayout::kQrfMidpointOffsetZ }, { padPos.x, 0.0f, padPos.z }
			};
			if (Enemy* qrf = SpawnEnemy(sp, true, kAutoInitialYaw, Enemy::TacticalRole::Flanker, qrfRoute, false)) qrf->AlertEnemy(alertTarget);
		}
	}
	else if (Enemy* qrfE = SpawnEnemy(GamePlayLevelLayout::kFallbackQrfSpawnPos, true, kAutoInitialYaw, Enemy::TacticalRole::Flanker))
	{
		qrfE->AlertEnemy(alertTarget);
	}
}

void GamePlayScene::InitializeSprites()
{
	if (!spriteManager_)
	{
		if (SpriteCom* sc = SceneManager::GetInstance()->GetSpriteCom())
		{
			spriteManager_ = std::make_unique<SpriteManager>();
			spriteManager_->Initialize(sc, "Resources/uvChecker.png", 0);
		}
	}
	if (!directXCom) return;

	mouseInput.Initialize(directXCom->GetWindowAPI());
	SpriteCom* sc = spriteCom ? spriteCom : SceneManager::GetInstance()->GetSpriteCom();
	if (!sc) return;
	if (!spriteCom) spriteCom = sc;

	const Sprite::Transform defaultTf = { { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
	if (auto cursor = Sprite::Create(sc, defaultTf, "Resources/CG4/circle2.png"))
	{
		cursor->SetSize({ 0.0f, 0.0f });
		cursor->SetAnchorPoint({ 0.5f, 0.5f });
		sprites.emplace_back(std::move(cursor));
		cursorSpriteIndex = static_cast<int>(sprites.size()) - 1;
		ShowCursor(FALSE);
	}

	auto InitStyledBar = [&](Vector2 pos, Vector2 size, Vector4 fgCol, Sprite*& outBg, Sprite*& outFg) {
		if (CreateWhiteSpritePair(sc, sprites, { 0.0f, 0.0f }, { 0.0f, 0.0f }, outBg, outFg))
		{
			outBg->SetPosition(pos); outBg->SetSize(size); outBg->SetColor(kHudSpriteCfg.barBgColor);
			outFg->SetPosition(pos); outFg->SetSize(size); outFg->SetColor(fgCol);
		}
	};

	InitStyledBar(kHudSpriteCfg.hpPos, kHudSpriteCfg.hpSize, kHudSpriteCfg.hpFgColor, playerHpBarBg_, playerHpBarFg_);
	InitStyledBar(kHudSpriteCfg.staminaPos, kHudSpriteCfg.staminaSize, kHudSpriteCfg.staminaFgColor, playerStaminaBarBg_, playerStaminaBarFg_);
	CreateWhiteSpritePair(sc, sprites, { 0.0f, 0.0f }, { 0.0f, 0.0f }, playerReloadBarBg_, playerReloadBarFg_);

	speedLines_.clear();
	speedLines_.reserve(kHudSpriteCfg.speedLineCount);
	speedLineAlpha_ = 0.0f;
	for (int i = 0; i < kHudSpriteCfg.speedLineCount; ++i)
	{
		if (auto line = Sprite::Create(sc, defaultTf, "Resources/CG4/gradationLine.png"))
		{
			line->SetAnchorPoint({ 0.5f, 0.0f });
			line->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
			sprites.emplace_back(std::move(line));
			speedLines_.push_back(sprites.back().get());
		}
	}

	vignetteAlpha_ = 0.0f;
	if (auto vignette = Sprite::Create(sc, defaultTf, "Resources/CG4/human/white.png"))
	{
		vignette->SetAnchorPoint({ 0.0f, 0.0f });
		vignette->SetPosition({ 0.0f, 0.0f });
		vignette->SetSize(kHudSpriteCfg.vignetteSize);
		vignette->SetColor({ 1.0f, 0.0f, 0.0f, 0.0f });
		sprites.emplace_back(std::move(vignette));
		vignetteSprite_ = sprites.back().get();
	}
}

void GamePlayScene::InitializeAudioAndParticles()
{
	emitter.transform.SetTranslate({ 0.0f, 0.0f, 0.0f });
	emitter.transform.SetRotate({ 0.0f, 0.0f, 0.0f });
	emitter.transform.SetScale({ 1.0f, 1.0f, 1.0f });
	emitter.count = kEnvInit.emitterCount;
	emitter.frequency = kEnvInit.emitterFreq;
	emitter.frequencyTime = 0.0f;

	cylinderTextureIndex_       = LoadTexWithFallback("Resources/CG4/gradationLine.png");
	particleTextureA            = LoadTexWithFallback("Resources/uvChecker.png");
	particleTextureB            = LoadTexWithFallback("Resources/CG4/circle2.png");
	bloodTextureIndex_          = LoadTexWithFallback("Resources/blood_splatter.png");
	smokeTextureIndex_          = LoadTexWithFallback("Resources/smoke_dark.png");
	escapeSmokeTextureIndex_    = LoadTexWithFallback("Resources/smoke_puff_white.png");
	if (escapeSmokeTextureIndex_ == TextureManager::kInvalidTextureIndex)
	{
		escapeSmokeTextureIndex_ = LoadTexWithFallback("Resources/title_smoke_puff.png");
	}
	fenceTextureIndex_          = LoadTexWithFallback("Resources/duckov_fence.png");
	starburstTextureIndex_      = LoadTexWithFallback("Resources/starburst.png");
	minovskyCircleTextureIndex_ = particleTextureB;
	wispySmokeTextureIndex_     = LoadTexWithFallback("Resources/clear_smoke_wispy.png", escapeSmokeTextureIndex_);
	cineAtlasTextureIndex_      = LoadTexWithFallback("Resources/gameplay_cine_atlas.png");
	clearDigitsTextureIndex_    = LoadTexWithFallback("Resources/clear_digits_atlas.png");
	soilDustTextureIndex_       = LoadTexWithFallback("Resources/particle_soil_dust.png", wispySmokeTextureIndex_);
	sparkStreakTextureIndex_    = LoadTexWithFallback("Resources/particle_spark_streak.png", particleTextureB);
	debrisChipTextureIndex_     = LoadTexWithFallback("Resources/particle_debris_chip.png", particleTextureB);
	waterFoamTextureIndex_      = LoadTexWithFallback("Resources/particle_water_foam.png", wispySmokeTextureIndex_);
	featherTextureIndex_        = LoadTexWithFallback("Resources/feather.png", particleTextureB);
	leafTextureIndex_           = LoadTexWithFallback("Resources/leaf.png", debrisChipTextureIndex_);

	if (hitEffect_)
	{
		hitEffect_->SetPlaneParticleTextureIndex(particleTextureB);
		hitEffect_->SetPlaneParticleCount(emitter.count);
		hitEffect_->SetRingTextureIndex(particleTextureA);
		hitEffect_->SetTextureIndex(cylinderTextureIndex_);
	}
}

void GamePlayScene::Finalize()
{
	if (helicopterIntro_) { helicopterIntro_->Finalize(); helicopterIntro_.reset(); }
	smokeParticleManager_.reset();
	appParticleManager_.reset();
	if (deathSequenceController_) { deathSequenceController_->Finalize(); deathSequenceController_.reset(); }

	bgmLoop_.Stop();
	flareHissLoop_.Stop();
	prevEnemyAIStates_.clear();

	if (hitEffect_) { hitEffect_->Finalize(); hitEffect_.reset(); }
	if (goalRing_) { goalRing_->Finalize(); goalRing_.reset(); }
	extractionPadObject_.reset();
	extractionSignalFlareObject_.reset();

	hud_.reset();
	lootSystem_.reset();
	if (combatSystem_) { combatSystem_->Finalize(); combatSystem_.reset(); }
	collisionSystem_.reset();
	if (environmentSystem_) { environmentSystem_->Finalize(); environmentSystem_.reset(); }
	if (player_) { player_->Finalize(); player_.reset(); }

	for (auto& enemy : enemies_) { if (enemy) enemy->Finalize(); }
	enemies_.clear();
	for (auto& obs : obstacles_) { if (obs) obs->Finalize(); }
	obstacles_.clear();
	for (auto& t : targets_) { if (t) t->Finalize(); }
	targets_.clear();

	ShowCursor(TRUE);
}

float GamePlayScene::AdvanceDeltaTime()
{
	const auto now = std::chrono::steady_clock::now();
	const float deltaTime = (std::min)(std::chrono::duration<float>(now - lastTime_).count(), kMaxFrameDeltaTime);
	lastTime_ = now;
	return deltaTime;
}

void GamePlayScene::UpdateExtractionGoal(float deltaTime)
{
	if (environmentSystem_) environmentSystem_->UpdateExtractionGoal(deltaTime);
}

void GamePlayScene::UpdateEnvironment()
{
	if (!environmentSystem_) return;
	environmentSystem_->UpdateBarrier();
	environmentSystem_->UpdateHitEffect(kFixedDeltaTime);
}

void GamePlayScene::UpdateParticles(float deltaTime)
{
	if (environmentSystem_) environmentSystem_->UpdateRiverEffects(deltaTime);
	if (characterEffectController_) characterEffectController_->Update(deltaTime);
}

void GamePlayScene::UpdateSprites(float deltaTime)
{
	if (spriteManager_ && directXCom && directXCom->GetWindowAPI()) spriteManager_->Update();

	// 速度線の更新
	if (directXCom && directXCom->GetWindowAPI())
	{
		WindowAPI* windowAPI = directXCom->GetWindowAPI();
		const float width = static_cast<float>(windowAPI->GetClientWidth());
		const float height = static_cast<float>(windowAPI->GetClientHeight());
		const Vector2 center = { width * 0.5f, height * 0.5f };

		const bool dodging = (player_ && player_->IsDodging());
		speedLineAlpha_ = dodging
			? (std::min)(kHudSpriteCfg.speedMaxAlpha, speedLineAlpha_ + kHudSpriteCfg.speedFadeIn * deltaTime)
			: (std::max)(0.0f, speedLineAlpha_ - kHudSpriteCfg.speedFadeOut * deltaTime);

		const float dodgeProgress = dodging ? (player_->GetDodgeTimer() / player_->GetDodgeDuration()) : 1.0f;
		const float maxDist = (std::max)(width, height) * 0.5f;
		const float dist = maxDist * (kHudSpriteCfg.speedDistBase + kHudSpriteCfg.speedDistAmp * std::sin(dodgeProgress * kPi));
		const float lineHeight = maxDist * kHudSpriteCfg.speedHeightRatio;
		const float angleStep = speedLines_.empty() ? 0.0f : (kTwoPi / static_cast<float>(speedLines_.size()));

		for (size_t i = 0; i < speedLines_.size(); ++i)
		{
			Sprite* line = speedLines_[i];
			if (!line) continue;
			if (speedLineAlpha_ <= 0.0f)
			{
				if (line->GetSize().x != 0.0f || line->GetSize().y != 0.0f) { line->SetSize({ 0.0f, 0.0f }); line->Update(); }
				continue;
			}
			const float angle = static_cast<float>(i) * angleStep;
			const float lineWidth = kHudSpriteCfg.speedWidthBase
				+ kHudSpriteCfg.speedWidthAmp * std::sin(static_cast<float>(i) + dodgeProgress * kHudSpriteCfg.speedPhaseMult);
			line->SetPosition({ center.x + std::cos(angle) * dist, center.y + std::sin(angle) * dist });
			line->SetRotation(angle - kHalfPi);
			line->SetSize({ lineWidth, lineHeight });
			line->SetColor({ 1.0f, 1.0f, 1.0f, speedLineAlpha_ });
			line->Update();
		}
	}

	mouseInput.Update();
	HideSpriteAtIndex(sprites, cursorSpriteIndex);

	// ビネット（ダメージ赤フラッシュ ＆ 瀕死時赤鼓動ビネット）の更新
	if (vignetteSprite_ && directXCom && directXCom->GetWindowAPI())
	{
		float lowHpVignette = 0.0f;
		if (player_ && !player_->IsDead())
		{
			const float hpRatio = player_->GetHPRatio();
			if (hpRatio <= kHudSpriteCfg.lowHpThreshold)
			{
				static float heartTimer = 0.0f;
				const float dangerRatio = 1.0f - hpRatio / kHudSpriteCfg.lowHpThreshold;
				heartTimer += (kHudSpriteCfg.heartPulseBase + dangerRatio * kHudSpriteCfg.heartPulseGain) * deltaTime;
				const float sinVal = std::sin(heartTimer);
				const float pulse = sinVal > 0.0f ? (sinVal * sinVal) : 0.0f;
				lowHpVignette = pulse * (kHudSpriteCfg.heartIntBase + dangerRatio * kHudSpriteCfg.heartIntGain);

				const int curBeatCycle = static_cast<int>(heartTimer / kTwoPi);
				if (curBeatCycle != prevHeartbeatCycle_)
				{
					prevHeartbeatCycle_ = curBeatCycle;
					TacticalAudio::Play2D("Resources/Audio/SE_Heartbeat.wav", kHudSpriteCfg.heartVolBase + dangerRatio * kHudSpriteCfg.heartVolGain, kHudSpriteCfg.heartCooldownMs);
				}
			}
			else
			{
				prevHeartbeatCycle_ = -1;
			}
		}

		if (vignetteAlpha_ > 0.0f) vignetteAlpha_ = (std::max)(0.0f, vignetteAlpha_ - kHudSpriteCfg.vignetteFadeOut * deltaTime);
		vignetteSprite_->SetColor({ 1.0f, 0.0f, 0.0f, (std::max)(vignetteAlpha_, lowHpVignette) });
		vignetteSprite_->Update();
	}
}

void GamePlayScene::UpdateDebugInput()
{
#if APP_DEBUG_UI
	// --- デバッグ専用ショートカット（Release / SHIPPING_BUILD では完全無効） ---
	const bool ctrlHeld = IsAppKeyDown(VK_CONTROL);
	const bool allowDebugKeys = !AppDebugState::IsImGuiCapturingTextInput();

	if (ConsumeKeyEdge<KeyEdgeId::Debug9>(allowDebugKeys && IsAppKeyDown('9')) && hitEffect_)
	{
		Vector3 effectTranslate = emitter.transform.GetTranslate();
		effectTranslate.y += kInputCfg.debugFxOffsetY;
		hitEffect_->Play(effectTranslate);
	}

	if (ConsumeKeyEdge<KeyEdgeId::Debug8>(allowDebugKeys && IsAppKeyDown('8')) && particleManager)
	{
		std::list<ParticleManager::Particle> newParticles;
		for (uint32_t i = 0; i < emitter.count; ++i)
		{
			Vector3 effectTranslate = emitter.transform.GetTranslate();
			effectTranslate.y += kInputCfg.debugFxOffsetY;
			auto p = particleManager->MakeNewParticles(particleManager->GetRandomEngine(), effectTranslate);
			p.textureIndex = particleTextureB;
			p.lifeTime = kInputCfg.debugParticleLife;
			newParticles.push_back(p);
		}
		particleManager->AddParticles(newParticles);
	}

	// F1: デバッグギズモ切替 / F3: プロファイラー切替
	if (ConsumeKeyEdge<KeyEdgeId::DebugF1>(!ctrlHeld && IsAppKeyDown(VK_F1))) showDebugGizmos_ = !showDebugGizmos_;
	if (ConsumeKeyEdge<KeyEdgeId::DebugF3>(!ctrlHeld && IsAppKeyDown(VK_F3))) showPerformanceTracker_ = !showPerformanceTracker_;

	// F8 または Ctrl+K で即座にHP0・戦死シネマティック演出を発動テスト
	if (ConsumeKeyEdge<KeyEdgeId::DebugSuicide>(IsAppKeyDown(VK_F8) || (ctrlHeld && IsAppKeyDown('K'))) &&
		player_ && !player_->IsDead() && !isDeathSequenceActive_)
	{
		const float yaw = player_->GetRotation().y;
		Vector3 forward = { std::sin(yaw), 0.0f, std::cos(yaw) };
		if (forward.x * forward.x + forward.z * forward.z < GameConfig::Math::Square(kInputCfg.minForwardLen)) forward = { 0.0f, 0.0f, 1.0f };
		Vector3 dummyAttackerPos = player_->GetPosition() + forward * kInputCfg.debugAttackerDist;
		player_->TakeDamage(
			kInputCfg.lethalDamage, "HOSTILE SENTRY (SVD SNIPER)",
			"CRITICAL HIT // CHEST VITAL PENETRATION (7.62x54mmR)", &dummyAttackerPos);
	}
#endif

	// [Y] 視点切替（通常クォータービュー ⇔ 上空広域タクティカル俯瞰）
	if (ConsumeKeyEdge<KeyEdgeId::ToggleCamY>(IsAppKeyDown('Y')))
	{
		isOverheadCameraMode_ = !isOverheadCameraMode_;
		TacticalAudio::Play2D("Resources/Audio/SE_UI_Click.wav", kInputCfg.uiClickVol, kInputCfg.uiClickCooldownMs);
	}

	// [M] 全体作戦マップの開閉トグル
	if (ConsumeKeyEdge<KeyEdgeId::ToggleMapM>(IsAppKeyDown('M')))
	{
		isFullMapOpen_ = !isFullMapOpen_;
		TacticalAudio::Play2D("Resources/Audio/SE_UI_Click.wav", kInputCfg.uiClickVol, kInputCfg.uiClickCooldownMs);
	}
	if (isFullMapOpen_ && IsAppKeyDown(VK_ESCAPE))
	{
		isFullMapOpen_ = false;
		TacticalAudio::Play2D("Resources/Audio/SE_UI_Click.wav", kInputCfg.uiEscClickVol, kInputCfg.uiClickCooldownMs);
	}

	// カメラズーム微調整 (マウスホイール または [ / ] キー)
	if (IsAppKeyDown(VK_OEM_4) || IsAppKeyDown(VK_SUBTRACT)) cameraZoom_ = (std::min)(cameraZoom_ + kInputCfg.zoomKeyStep, GameConfig::Camera::kMaxZoomScale);
	if (IsAppKeyDown(VK_OEM_6) || IsAppKeyDown(VK_ADD)) cameraZoom_ = (std::max)(cameraZoom_ - kInputCfg.zoomKeyStep, GameConfig::Camera::kMinZoomScale);
	if (!isFullMapOpen_ && TacticalUICanvas::GetInstance())
	{
		const float wheel = TacticalUICanvas::GetInstance()->ConsumeMouseWheel();
		if (std::abs(wheel) > kInputCfg.wheelDeadzone)
		{
			cameraZoom_ = std::clamp(cameraZoom_ - wheel * kInputCfg.wheelZoomScale, GameConfig::Camera::kMinZoomScale, GameConfig::Camera::kMaxZoomScale);
		}
	}
}

void GamePlayScene::UpdatePlayerReloadAndMovementEffects(float deltaTime)
{
	if (!player_ || !particleManager || !appParticleManager_ || player_->IsDead()) return;

	const Vector3 playerPos = player_->GetPosition();

	// --- リアル3段階リロード物理演出（3D湾曲PMAG排出・挿入ロック振動・ボルト閉鎖蒸気） ---
	static bool spawnedReloadMag      = false;
	static bool triggeredMagSeatClick = false;
	static bool triggeredBoltSlam     = false;

	if (player_->IsReloading() && !wasPlayerReloadingPrev_)
	{
		remainingAmmoOnReload_ = player_->GetMagazineAmmo();
		droppedCasingsCount_   = 0;
		spawnedReloadMag = triggeredMagSeatClick = triggeredBoltSlam = false;
		TacticalAudio::Play2D("Resources/Audio/SE_Reload_MagOut.wav", kReloadFx.magOutVol);
	}
	wasPlayerReloadingPrev_ = player_->IsReloading();

	if (player_->IsReloading())
	{
		const float progress = player_->GetReloadProgress();
		const float yaw = player_->GetRotation().y;
		const Vector3 forward = { std::sin(yaw), 0.0f, std::cos(yaw) };

		if (progress >= kReloadFx.stage1Prog && !spawnedReloadMag)
		{
			if (combatSystem_) combatSystem_->SpawnDroppedMagazine(player_->GetMagwellWorldPosition(), forward, remainingAmmoOnReload_ > 0, false);
			spawnedReloadMag = true;
		}
		if (progress >= kReloadFx.stage2Prog && !triggeredMagSeatClick)
		{
			TriggerCameraShake(kReloadFx.stage2ShakeDur, kReloadFx.stage2ShakeInt);
			TacticalAudio::Play2D("Resources/Audio/SE_Reload_MagIn.wav", kReloadFx.magInVol);
			triggeredMagSeatClick = true;
		}
		if (progress >= kReloadFx.stage3Prog && !triggeredBoltSlam)
		{
			const bool dry = player_->WasDryReload();
			TriggerCameraShake(dry ? kReloadFx.dryShakeDur : kReloadFx.tacShakeDur, dry ? kReloadFx.dryShakeInt : kReloadFx.tacShakeInt);
			TacticalAudio::Play2D("Resources/Audio/SE_Reload_BoltRack.wav", dry ? kReloadFx.dryBoltVol : kReloadFx.tacBoltVol);

			const uint32_t wispyTex = (wispySmokeTextureIndex_ != TextureManager::kInvalidTextureIndex) ? wispySmokeTextureIndex_ : smokeTextureIndex_;
			if (dry && smokeParticleManager_ && wispyTex != TextureManager::kInvalidTextureIndex)
			{
				smokeParticleManager_->AddParticle(MakeSceneParticle(
					player_->GetRiflePosition() + Vector3{ 0.0f, kReloadFx.wispOffsetY, 0.0f }, kReloadFx.wispRotZ,
					kReloadFx.wispInitSc, kSpecBoltWisp.aspect, kReloadFx.wispInitSc, kReloadFx.wispTargetSc,
					{ forward.z * kReloadFx.wispSideSpd, kReloadFx.wispUpSpd, -forward.x * kReloadFx.wispSideSpd },
					kReloadFx.wispStartCol, kReloadFx.wispEndCol, kReloadFx.wispLife, wispyTex, 0.0f, kSpecBoltWisp));
			}
			triggeredBoltSlam = true;
		}
	}
	else
	{
		playerReloadCasingTimer_ = 0.0f;
		spawnedReloadMag = triggeredMagSeatClick = triggeredBoltSlam = false;
	}
	wasPlayerReloading_ = player_->IsReloading();

	auto NotifyEnemiesWithinNoiseRadius = [&](float maxRad, float ringDuration) {
		playerSoundMaxRadius_ = maxRad;
		playerSoundRadius_ = 0.0f;
		playerSoundTimer_ = ringDuration;
		const float maxRadSq = maxRad * maxRad;
		for (auto& enemy : enemies_)
		{
			if (!enemy || enemy->IsDead()) continue;
			const float dx = enemy->GetPosition().x - playerPos.x;
			const float dz = enemy->GetPosition().z - playerPos.z;
			if (dx * dx + dz * dz <= maxRadSq) enemy->HearNoise(playerPos);
		}
	};

	AppParticleManager* smokeMgr = smokeParticleManager_ ? smokeParticleManager_.get() : appParticleManager_.get();
	const uint32_t dustTex = (soilDustTextureIndex_ != TextureManager::kInvalidTextureIndex) ? soilDustTextureIndex_ : wispySmokeTextureIndex_;
	const uint32_t chipTex = (debrisChipTextureIndex_ != TextureManager::kInvalidTextureIndex) ? debrisChipTextureIndex_ : particleTextureB;

	static bool prevDodge = false;
	if (player_->IsDodging())
	{
		if (!prevDodge)
		{
			TriggerCameraShake(kMoveFx.startShakeDur, kMoveFx.startShakeInt);
			TacticalAudio::Play2D("Resources/Audio/SE_Dodge_Roll.wav", kMoveFx.startVol);
			NotifyEnemiesWithinNoiseRadius(kMoveFx.soundMaxRad, kMoveFx.soundDuration);

			const bool onBridge = (playerPos.z >= kMoveFx.bridgeMinZ && playerPos.z <= kMoveFx.bridgeMaxZ &&
				std::abs(std::abs(playerPos.x) - kMoveFx.bridgeAbsX) <= kMoveFx.bridgeHalfWidth);
			if (smokeMgr)
			{
				auto& rng = particleManager->GetRandomEngine();
				std::uniform_real_distribution<float> jitter(-kMoveFx.jitterRange, kMoveFx.jitterRange);
				std::uniform_real_distribution<float> rotDist(0.0f, kTwoPi);

				// 足元から地表に沿って巻き上がる砂埃
				for (int i = 0; i < kMoveFx.kickDustCount; ++i)
				{
					const float angle  = static_cast<float>(i) * (kTwoPi / static_cast<float>(kMoveFx.kickDustCount)) + jitter(rng);
					const float rotZ   = rotDist(rng);
					const float angVel = jitter(rng) * kMoveFx.kdAngMult;
					const float initSc = kMoveFx.kdBaseSc + static_cast<float>(i % kMoveFx.modThree) * kMoveFx.kdStepSc;
					const float spd    = kMoveFx.kdBaseSpd + static_cast<float>(i % kMoveFx.modTwo) * kMoveFx.kdStepSpd;
					const Vector3 pos  = playerPos + Vector3{ std::cos(angle) * kMoveFx.kdRadius, onBridge ? kMoveFx.kdBridgeY : kMoveFx.kdGroundY, std::sin(angle) * kMoveFx.kdRadius };
					const Vector3 vel  = { std::cos(angle) * spd, kMoveFx.kdBaseVy + static_cast<float>(i % kMoveFx.modThree) * kMoveFx.kdStepVy, std::sin(angle) * spd };
					const float life   = kMoveFx.kdBaseLife + static_cast<float>(i % kMoveFx.modThree) * kMoveFx.kdStepLife;
					smokeMgr->AddParticle(MakeSceneParticle(
						pos, rotZ, initSc, kSpecKickDust.aspect, initSc, initSc * kMoveFx.kdTargetMult,
						vel, onBridge ? kMoveFx.kdBridgeCol : kMoveFx.kdGroundCol, kMoveFx.kdEndCol, life, dustTex, angVel, kSpecKickDust));
				}

				// 蹴り上げた土塊・小石粒
				for (int i = 0; i < kMoveFx.kickChipCount; ++i)
				{
					const float angle  = rotDist(rng);
					const Vector3 pos  = playerPos + Vector3{ jitter(rng), onBridge ? kMoveFx.kcBridgeY : kMoveFx.kcGroundY, jitter(rng) };
					const float rotZ   = rotDist(rng);
					const float sc     = kMoveFx.kcBaseSc + static_cast<float>(i % kMoveFx.modThree) * kMoveFx.kcStepSc;
					const float spd    = kMoveFx.kcBaseSpd + static_cast<float>(i % kMoveFx.modThree) * kMoveFx.kcStepSpd;
					const Vector3 vel  = { std::cos(angle) * spd, kMoveFx.kcBaseVy + static_cast<float>(i % kMoveFx.modThree) * kMoveFx.kcStepVy, std::sin(angle) * spd };
					const float life   = kMoveFx.kcBaseLife + static_cast<float>(i % kMoveFx.modTwo) * kMoveFx.kcStepLife;
					const float angVel = (i % kMoveFx.modTwo == 0) ? kMoveFx.kcAngVel : -kMoveFx.kcAngVel;
					smokeMgr->AddParticle(MakeSceneParticle(
						pos, rotZ, sc, kSpecKickChip.aspect, sc, sc * kMoveFx.kcTargetMult,
						vel, onBridge ? kMoveFx.kcBridgeCol : kMoveFx.kcGroundCol, kMoveFx.kcEndCol, life, chipTex, angVel, kSpecKickChip));
				}

				// 激しい回避動作でふわりと抜ける綿羽
				if (featherTextureIndex_ != TextureManager::kInvalidTextureIndex)
				{
					for (int i = 0; i < kMoveFx.kickFeatherCount; ++i)
					{
						const Vector3 pos = playerPos + Vector3{
							jitter(rng) * kMoveFx.kfJitterPosMult, kMoveFx.kfBaseY + static_cast<float>(i) * kMoveFx.kfStepY, jitter(rng) * kMoveFx.kfJitterPosMult
						};
						const float rotZ  = rotDist(rng);
						const Vector3 vel = {
							jitter(rng) * kMoveFx.kfJitterVelMult, kMoveFx.kfBaseVy + static_cast<float>(i) * kMoveFx.kfStepVy, jitter(rng) * kMoveFx.kfJitterVelMult
						};
						const float sc     = kMoveFx.kfBaseSc + static_cast<float>(i) * kMoveFx.kfStepSc;
						const Vector4 col  = (i == 0) ? kMoveFx.kfCol0 : kMoveFx.kfCol1;
						const float life   = kMoveFx.kfBaseLife + static_cast<float>(i) * kMoveFx.kfStepLife;
						const float angVel = (i == 0) ? kMoveFx.kfAngVel : -kMoveFx.kfAngVel;
						AppParticle f = MakeSceneParticle(
							pos, rotZ, sc, kSpecKickFeather.aspect, sc, sc * kMoveFx.kfTargetMult,
							vel, col, { col.x, col.y, col.z, 0.0f }, life, featherTextureIndex_, angVel, kSpecKickFeather);
						f.wobbleFreq = kMoveFx.kfWobbleFreq;
						f.wobbleAmp = kMoveFx.kfWobbleAmp;
						f.wobbleAxis = kMoveFx.kfWobbleAxis;
						smokeMgr->AddParticle(f);
					}
				}
			}
			prevDodge = true;
		}
		return;
	}

	if (prevDodge)
	{
		// 回避着地スライド時のリアルな砂埃＆小石飛沫
		TriggerCameraShake(kMoveFx.landShakeDur, kMoveFx.landShakeInt);
		TacticalAudio::Play2D("Resources/Audio/SE_Dodge_Land.wav", kMoveFx.landVol);
		if (smokeMgr)
		{
			auto& rng = particleManager->GetRandomEngine();
			std::uniform_real_distribution<float> rotDist(0.0f, kTwoPi);
			for (int i = 0; i < kMoveFx.landDustCount; ++i)
			{
				const float angle  = static_cast<float>(i) * (kTwoPi / static_cast<float>(kMoveFx.landDustCount));
				const float rotZ   = rotDist(rng);
				const float initSc = kMoveFx.ldBaseSc + static_cast<float>(i % kMoveFx.modTwo) * kMoveFx.ldStepSc;
				const Vector3 pos  = playerPos + Vector3{ std::cos(angle) * kMoveFx.ldRadius, kMoveFx.ldY, std::sin(angle) * kMoveFx.ldRadius };
				const Vector3 vel  = { std::cos(angle) * kMoveFx.ldSpd, kMoveFx.ldVy, std::sin(angle) * kMoveFx.ldSpd };
				const float life   = kMoveFx.ldBaseLife + static_cast<float>(i % kMoveFx.modThree) * kMoveFx.ldStepLife;
				smokeMgr->AddParticle(MakeSceneParticle(
					pos, rotZ, initSc, kSpecLandDust.aspect, initSc, initSc * kMoveFx.ldTargetMult,
					vel, kMoveFx.ldCol, kMoveFx.kdEndCol, life, dustTex, 0.0f, kSpecLandDust));
			}
			for (int i = 0; i < kMoveFx.landChipCount; ++i)
			{
				const float angle = rotDist(rng);
				const float rotZ  = rotDist(rng);
				const float sc    = kMoveFx.lcBaseSc + static_cast<float>(i % kMoveFx.modTwo) * kMoveFx.lcStepSc;
				const Vector3 vel = { std::cos(angle) * kMoveFx.lcSpd, kMoveFx.lcBaseVy + static_cast<float>(i % kMoveFx.modThree) * kMoveFx.lcStepVy, std::sin(angle) * kMoveFx.lcSpd };
				smokeMgr->AddParticle(MakeSceneParticle(
					playerPos + Vector3{ 0.0f, kMoveFx.lcY, 0.0f }, rotZ, sc, kMoveFx.lcAspect, 0.0f, 0.0f,
					vel, kMoveFx.lcCol, kMoveFx.kcEndCol, kMoveFx.lcLife, chipTex, kMoveFx.lcAngVel, kSpecLandChip));
			}
		}
	}
	prevDodge = false;

	if (player_->IsMoving())
	{
		static float stepNoiseAccumulator = 0.0f;
		stepNoiseAccumulator += deltaTime;
		if (stepNoiseAccumulator >= kMoveFx.stepNoiseInterval)
		{
			stepNoiseAccumulator = 0.0f;
			NotifyEnemiesWithinNoiseRadius(kMoveFx.stepNoiseMaxRad, kMoveFx.stepNoiseDuration);
		}
	}
	else
	{
		playerDustTimer_ = 0.0f;
	}
}

bool GamePlayScene::ComputePlayerCoverState(const Vector3& playerPos) const
{
	const float coverReachSq = GameConfig::Math::Square(kAiCfg.coverReachDist);
	for (const auto& obs : obstacles_)
	{
		if (!obs || !obs->GetCollider()) continue;
		const auto* box = obs->GetCollider();
		const Vector3 extents = box->GetExtents();
		if (extents.y <= kAiCfg.minCoverExtentY) continue;

		const Vector3 center = box->GetWorldPosition();
		const float relX = playerPos.x - center.x;
		const float relZ = playerPos.z - center.z;
		const float maxReach = extents.x + extents.z + kAiCfg.coverReachDist;
		if (std::abs(relX) > maxReach || std::abs(relZ) > maxReach) continue;

		const float ry = box->GetWorldRotation().y;
		float localX = relX, localZ = relZ;
		if (std::abs(ry) >= kAiCfg.coverRotEps)
		{
			const float cs = std::cos(ry), sn = std::sin(ry);
			localX = relX * cs - relZ * sn;
			localZ = relX * sn + relZ * cs;
		}
		const float dx = (std::max)(0.0f, std::abs(localX) - extents.x);
		const float dz = (std::max)(0.0f, std::abs(localZ) - extents.z);
		if (dx * dx + dz * dz <= coverReachSq) return true;
	}
	return false;
}

void GamePlayScene::UpdateEnemiesAndSquadTactics(float deltaTime, WindowAPI* windowAPI, const Vector3* target, bool isPlayerInCover)
{
	// 全敵AIの更新 ＆ 索敵ステート変化無線チャープ
	for (auto& enemy : enemies_)
	{
		if (!enemy) continue;
		enemy->Update(windowAPI, target, obstacles_, deltaTime, isPlayerInCover);

		const int curState = static_cast<int>(enemy->GetAIState());
		if (!enemy->IsDead() && target != nullptr)
		{
			auto it = prevEnemyAIStates_.find(enemy.get());
			if (it != prevEnemyAIStates_.end() && it->second != curState)
			{
				if (enemy->GetAIState() == Enemy::AIState::Chase)
				{
					TacticalAudio::Play3D("Resources/Audio/SE_AI_Alert.wav", enemy->GetPosition(), *target, kAiCfg.alertVol, kAiCfg.alertMinDist, kAiCfg.alertMaxDist, kAiCfg.aiChirpCooldownMs);
				}
				else if (it->second == static_cast<int>(Enemy::AIState::Patrol) && enemy->GetAIState() != Enemy::AIState::Patrol)
				{
					TacticalAudio::Play3D("Resources/Audio/SE_AI_Question.wav", enemy->GetPosition(), *target, kAiCfg.questionVol, kAiCfg.questionMinDist, kAiCfg.questionMaxDist, kAiCfg.aiChirpCooldownMs);
				}
			}
		}
		prevEnemyAIStates_[enemy.get()] = curState;
	}

	// 局所分隊無線リンク ＆ 銃声索敵伝播（看守長は完全独立）
	if (player_ && !player_->IsDead())
	{
		const Vector3 pPos = player_->GetPosition();
		const float alertRadSq = GameConfig::Math::Square(kAiCfg.squadAlertRadius);
		const float noiseRadSq = GameConfig::Math::Square(kAiCfg.squadNoiseRadius);

		for (size_t i = 0; i < enemies_.size(); ++i)
		{
			Enemy* chaser = enemies_[i].get();
			if (!chaser || chaser->IsDead() || chaser->GetAIState() != Enemy::AIState::Chase || chaser->GetTacticalRole() == Enemy::TacticalRole::Warden) continue;

			const Vector3 cPos = chaser->GetPosition();
			for (size_t j = 0; j < enemies_.size(); ++j)
			{
				if (i == j) continue;
				Enemy* ally = enemies_[j].get();
				if (!ally || ally->IsDead() || ally->GetAIState() == Enemy::AIState::Chase || ally->GetTacticalRole() == Enemy::TacticalRole::Warden) continue;

				const float dx = ally->GetPosition().x - cPos.x;
				const float dz = ally->GetPosition().z - cPos.z;
				const float distSq = dx * dx + dz * dz;
				if (distSq <= alertRadSq) ally->AlertEnemy(pPos);
				else if (distSq <= noiseRadSq && ally->GetAIState() == Enemy::AIState::Patrol) ally->HearNoise(pPos);
			}
		}
	}

	// 敵同士のペアワイズ重なり防止（セパレーション押し出し）＆ 移動時の足元土煙
	const float minSpacingSq = GameConfig::Math::Square(kAiCfg.minEnemySpacing);
	AppParticleManager* enemySmokeMgr = smokeParticleManager_ ? smokeParticleManager_.get() : appParticleManager_.get();
	const uint32_t enemyDustTex = (soilDustTextureIndex_ != TextureManager::kInvalidTextureIndex) ? soilDustTextureIndex_ : wispySmokeTextureIndex_;

	for (size_t i = 0; i < enemies_.size(); ++i)
	{
		Enemy* eA = enemies_[i].get();
		if (!eA || eA->IsDead()) continue;

		for (size_t j = i + 1; j < enemies_.size(); ++j)
		{
			Enemy* eB = enemies_[j].get();
			if (!eB || eB->IsDead()) continue;

			const Vector3 posA = eA->GetPosition(), posB = eB->GetPosition();
			const float dx = posA.x - posB.x, dz = posA.z - posB.z;
			const float distSq = dx * dx + dz * dz;
			if (distSq < minSpacingSq)
			{
				const float dist = std::sqrt(distSq);
				const float ang = static_cast<float>(i) * kAiCfg.sepAngStepI + static_cast<float>(j) * kAiCfg.sepAngStepJ;
				const Vector3 dir = (dist > kAiCfg.minSepDist) ? Vector3{ dx / dist, 0.0f, dz / dist } : Vector3{ std::cos(ang), 0.0f, std::sin(ang) };
				const float push = (kAiCfg.minEnemySpacing - dist) * kAiCfg.sepPushRate * deltaTime;
				eA->ApplySeparation(dir * push);
				eB->ApplySeparation(dir * (-push));
			}
		}

		if (particleManager && enemySmokeMgr && eA->IsMovingHorizontally())
		{
			std::uniform_real_distribution<float> chance(0.0f, 1.0f);
			if (chance(particleManager->GetRandomEngine()) < deltaTime * kAiCfg.stepDustChanceRate)
			{
				const Vector3 footPos = eA->GetPosition();
				enemySmokeMgr->EmitFootstepDust(particleManager->GetRandomEngine(), footPos, enemyDustTex);
				if (target != nullptr)
				{
					TacticalAudio::PlayRandom3D(
						{ "Resources/Audio/SE_Step_Dirt_1.wav", "Resources/Audio/SE_Step_Dirt_2.wav" },
						footPos, *target, kAiCfg.stepAudioVol, kAiCfg.stepAudioMinDist, kAiCfg.stepAudioMaxDist, kAiCfg.stepAudioCooldownMs, "enemy_step");
				}
			}
		}
	}

	// 敵の復活時演出 (光の召喚ピラー)
	if (particleManager && appParticleManager_)
	{
		auto& rng = particleManager->GetRandomEngine();
		auto emitRespawnPortal = [&](const Vector3& spawnPos, const Vector4& portalColor, float scaleMult) {
			for (const PortalRingSpec& ring : kRespawnRings)
			{
				const float step = kTwoPi / static_cast<float>(ring.count);
				for (int i = 0; i < ring.count; ++i)
				{
					const float angle = static_cast<float>(i) * step;
					appParticleManager_->EmitDustWithVelocity(
						rng, spawnPos + Vector3{ std::sin(angle) * ring.radius, 0.0f, std::cos(angle) * ring.radius },
						ring.scale * scaleMult, portalColor, { 0.0f, ring.velY, 0.0f }, ring.life, particleTextureB);
				}
			}
			for (int i = 0; i < kAiCfg.coreCount; ++i)
			{
				std::uniform_real_distribution<float> offsetH(-kAiCfg.coreOffsetH, kAiCfg.coreOffsetH);
				std::uniform_real_distribution<float> offsetV(0.0f, kAiCfg.coreOffsetV);
				const Vector3 spawnOffset = { offsetH(rng), offsetV(rng), offsetH(rng) };
				appParticleManager_->EmitDustWithVelocity(
					rng, spawnPos + spawnOffset, kAiCfg.coreScale * scaleMult,
					kAiCfg.coreColor, { 0.0f, kAiCfg.coreVelY, 0.0f }, kAiCfg.coreLife, particleTextureB);
			}
		};

		for (auto& enemy : enemies_)
		{
			if (enemy && enemy->GetJustRespawned())
			{
				emitRespawnPortal(enemy->GetPosition(), kAiCfg.respawnPortalColor, kAiCfg.respawnScaleMult);
				enemy->ClearJustRespawned();
			}
		}
	}
}

void GamePlayScene::UpdateCharacters(float deltaTime)
{
	if (player_)
	{
		player_->Update(deltaTime, &mouseInput);
		UpdatePlayerReloadAndMovementEffects(deltaTime);
	}

	WindowAPI* windowAPI = directXCom ? directXCom->GetWindowAPI() : nullptr;
	const Vector3* target = nullptr;
	Vector3 playerPosTarget{};
	bool isPlayerInCover = false;
	if (player_ && !player_->IsDead())
	{
		playerPosTarget = player_->GetPosition();
		target = &playerPosTarget;
		isPlayerInCover = ComputePlayerCoverState(playerPosTarget);
		player_->SetInCover(isPlayerInCover);
	}

	UpdateEnemiesAndSquadTactics(deltaTime, windowAPI, target, isPlayerInCover);

	if (playerSoundTimer_ > 0.0f)
	{
		playerSoundTimer_ -= deltaTime;
		playerSoundRadius_ = (std::min)(playerSoundMaxRadius_, playerSoundRadius_ + kMoveFx.soundExpandSpeed * deltaTime);
		if (playerSoundTimer_ <= 0.0f) { playerSoundRadius_ = 0.0f; playerSoundMaxRadius_ = 0.0f; }
	}

	if (levelEditor_)
	{
		levelEditor_->Update(deltaTime);
#if defined(USE_IMGUI) && APP_DEBUG_UI
		if (AppDebugState::IsDeveloperImGuiVisible()) levelEditor_->DrawImGui();
#endif
	}
}

bool GamePlayScene::IsWithinRadius(const Vector3& a, const Vector3& b, float radius)
{
	const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
	return (dx * dx + dy * dy + dz * dz) <= (radius * radius);
}

void GamePlayScene::UpdatePlayerHpBar()
{
	if (!player_ || !playerHpBarBg_ || !playerHpBarFg_ || !directXCom || !directXCom->GetWindowAPI()) return;

	// 左上の旧式ベタ塗りHP/スタミナ矩形は GamePlayHUD へ完全移行したため常時非表示
	HideSpriteBarPair(playerHpBarBg_, playerHpBarFg_);
	HideSpriteBarPair(playerStaminaBarBg_, playerStaminaBarFg_);

	if (!playerReloadBarBg_ || !playerReloadBarFg_) return;
	if (isGameIntroActive_ || isGameCleared_ || isDeathSequenceActive_ || !player_->IsReloading() || !camera_)
	{
		HideSpriteBarPair(playerReloadBarBg_, playerReloadBarFg_);
		return;
	}

	WindowAPI* windowAPI = directXCom->GetWindowAPI();
	Vector3 barPos3D = player_->GetPosition();
	barPos3D.y += kHudSpriteCfg.reloadHeadOffsetY;

	const Matrix4x4& vp = camera_->GetViewProjectionMatrix();
	const float clipX = barPos3D.x * vp.m[0][0] + barPos3D.y * vp.m[1][0] + barPos3D.z * vp.m[2][0] + vp.m[kMatRowTrans][0];
	const float clipY = barPos3D.x * vp.m[0][1] + barPos3D.y * vp.m[1][1] + barPos3D.z * vp.m[2][1] + vp.m[kMatRowTrans][1];
	const float clipW = barPos3D.x * vp.m[0][kMatColW] + barPos3D.y * vp.m[1][kMatColW] + barPos3D.z * vp.m[2][kMatColW] + vp.m[kMatRowTrans][kMatColW];
	if (clipW <= 0.0f) { HideSpriteBarPair(playerReloadBarBg_, playerReloadBarFg_); return; }

	const float width = static_cast<float>(windowAPI->GetClientWidth());
	const float height = static_cast<float>(windowAPI->GetClientHeight());
	const Vector2 barPos = { (clipX / clipW + 1.0f) * 0.5f * width - kHudSpriteCfg.reloadBarSize.x * 0.5f, (1.0f - clipY / clipW) * 0.5f * height };

	playerReloadBarBg_->SetPosition(barPos);
	playerReloadBarBg_->SetSize(kHudSpriteCfg.reloadBarSize);
	playerReloadBarBg_->SetColor(kHudSpriteCfg.barBgColor);
	playerReloadBarBg_->Update();

	playerReloadBarFg_->SetPosition(barPos);
	playerReloadBarFg_->SetSize({ kHudSpriteCfg.reloadBarSize.x * player_->GetReloadProgress(), kHudSpriteCfg.reloadBarSize.y });
	playerReloadBarFg_->SetColor(kHudSpriteCfg.reloadFgColor);
	playerReloadBarFg_->Update();
}

void GamePlayScene::CheckGameOver()
{
	if (!player_ || !player_->IsDead() || isDeathSequenceActive_) return;

	isDeathSequenceActive_ = true;
	deathSequenceTimer_ = kDeathSequenceDuration;

	const Vector3 deathPos = player_->GetPosition();
	Vector3 killerPos = player_->HasAttackerPosition()
		? player_->GetLastAttackerPosition()
		: (deathPos + Vector3{ 0.0f, 0.0f, kCineCfg.defaultKillerOffsetZ });

	if (!player_->HasAttackerPosition())
	{
		float closestDistSq = kCineCfg.maxSearchDistSq;
		for (const auto& e : enemies_)
		{
			if (!e || e->IsDead()) continue;
			const Vector3 diff = e->GetPosition() - deathPos;
			const float distSq = diff.x * diff.x + diff.z * diff.z;
			if (distSq < closestDistSq) { closestDistSq = distSq; killerPos = e->GetPosition(); }
		}
	}

	if (deathSequenceController_)
	{
		deathSequenceController_->Start(
			deathPos, killerPos, player_->GetLastAttackerName(), player_->GetCauseOfDeath(), RaidStats::GetInstance().isMIA);
	}
}

void GamePlayScene::UpdateGameIntro(float realDeltaTime)
{
	if (!helicopterIntro_ || !helicopterIntro_->IsActive()) { isGameIntroActive_ = false; return; }

	helicopterIntro_->Update(realDeltaTime);
	isGameIntroActive_ = helicopterIntro_->IsActive();

	SyncStageObjectsForCamera(realDeltaTime, false);
	if (player_ && player_->GetObject3d()) player_->GetObject3d()->Update();
	if (SceneManager::GetInstance() && SceneManager::GetInstance()->GetSkyBox()) SceneManager::GetInstance()->GetSkyBox()->Update();

	UpdateExtractionGoal(realDeltaTime);
	UpdateEnvironment();
	if (environmentSystem_) environmentSystem_->UpdateNatureAnimation(realDeltaTime);
	UpdateParticles(realDeltaTime);
	if (smokeParticleManager_) smokeParticleManager_->Update(realDeltaTime, GetPlayerPosition());
	UpdatePlayerHpBar();
	UpdateSprites(realDeltaTime);

	if (light && helicopterIntro_)
	{
		light->SetPointLight(
			kCineCfg.introHeliLightColor, helicopterIntro_->GetHelicopterPosition() + kCineCfg.introHeliLightOffset,
			kCineCfg.introHeliLightInt, kCineCfg.introHeliLightRad, kCineCfg.introHeliLightDecay);
	}
}

void GamePlayScene::UpdateCameraAndAim(float realDeltaTime)
{
	if (!camera_ || !player_) return;

	const Vector3 playerPos = player_->GetPosition();
	if (isGameCleared_)
	{
		// 脱出成功クライマックス：パッド上のアヒルと低空進入する回収ヘリ Dustoff-1 を同時に捉えるヒーロースイープ
		const float climaxProgress = (std::clamp)(1.0f - (clearSlowMoTimer_ / kClearSlowMoDuration), 0.0f, 1.0f);
		const float climaxS = SmoothStep01(climaxProgress);
		const Vector3 heroEye = playerPos + kCineCfg.heroEyeBase + kCineCfg.heroEyeDelta * climaxS;
		const Vector3 heroTarget = playerPos + kCineCfg.heroTargetBase + kCineCfg.heroTargetDelta * climaxS;
		const Vector3 dir = heroTarget - heroEye;
		const float xzLen = std::sqrt(dir.x * dir.x + dir.z * dir.z);
		const Vector3 targetRot = { std::atan2(-dir.y, (std::max)(xzLen, kCineCfg.minXzLen)), std::atan2(dir.x, dir.z), kCineCfg.heroRollScale * climaxS };

		const float camBlend = (std::min)(1.0f, kCineCfg.heroCamBlendRate * realDeltaTime);
		camera_->SetTranslate(camera_->GetTranslate() + (heroEye - camera_->GetTranslate()) * camBlend);
		camera_->SetRotate(camera_->GetRotate() + (targetRot - camera_->GetRotate()) * camBlend);
	}
	else
	{
		// Escape from Duckov 準拠カメラ：広角クォータービュー ⇔ [Y] 上空俯瞰ブレンド ＋ マウス照準先読み
		const float targetOverhead = isOverheadCameraMode_ ? 1.0f : 0.0f;
		const float modeStep = kCineCfg.overheadBlendSpeed * realDeltaTime;
		overheadCameraBlend_ = (overheadCameraBlend_ < targetOverhead)
			? (std::min)(targetOverhead, overheadCameraBlend_ + modeStep)
			: (std::max)(targetOverhead, overheadCameraBlend_ - modeStep);
		const float s = SmoothStep01(overheadCameraBlend_);

		const Vector3 baseOffset = GameConfig::Camera::kDefaultFollowOffset
			+ (GameConfig::Camera::kOverheadFollowOffset - GameConfig::Camera::kDefaultFollowOffset) * s;
		const Vector3 baseRot = {
			GameConfig::Camera::kFollowRotation.x + (GameConfig::Camera::kOverheadFollowRotation.x - GameConfig::Camera::kFollowRotation.x) * s,
			0.0f, 0.0f
		};

		Vector3 desiredLookahead = { 0.0f, 0.0f, 0.0f };
		if (!(isFullMapOpen_ || IsAppKeyDown(VK_TAB)) && directXCom && directXCom->GetWindowAPI())
		{
			WindowAPI* win = directXCom->GetWindowAPI();
			const float cw = static_cast<float>(win->GetClientWidth());
			const float ch = static_cast<float>(win->GetClientHeight());
			if (cw > 0.0f && ch > 0.0f)
			{
				const Vector2 mPos = mouseInput.GetScaledPosition();
				float nx = std::clamp((mPos.x / cw) * 2.0f - 1.0f, -kCineCfg.cursorClamp, kCineCfg.cursorClamp);
				float nz = std::clamp(1.0f - (mPos.y / ch) * 2.0f, -kCineCfg.cursorClamp, kCineCfg.cursorClamp);
				float mag = std::sqrt(nx * nx + nz * nz);
				if (mag > 1.0f) { nx /= mag; nz /= mag; mag = 1.0f; }
				if (mag > kCineCfg.cursorDeadzone)
				{
					const float activeScale = ((mag - kCineCfg.cursorDeadzone) / kCineCfg.cursorActiveRange)
						* GameConfig::Camera::kCursorLookaheadMax * (1.0f - kCineCfg.overheadLookScale * s);
					desiredLookahead = { (nx / mag) * activeScale, 0.0f, (nz / mag) * activeScale };
				}
			}
		}

		const float lookDamp = 1.0f - std::exp(-kCineCfg.lookDampSpeed * realDeltaTime);
		cameraLookaheadOffset_.x += (desiredLookahead.x - cameraLookaheadOffset_.x) * lookDamp;
		cameraLookaheadOffset_.z += (desiredLookahead.z - cameraLookaheadOffset_.z) * lookDamp;

		camera_->SetTranslate(playerPos + (baseOffset * cameraZoom_) + cameraLookaheadOffset_);
		camera_->SetRotate(baseRot);
	}

	camera_->Update();

	if (isGameCleared_)
	{
		const float climaxProgress = (std::clamp)(1.0f - (clearSlowMoTimer_ / kClearSlowMoDuration), 0.0f, 1.0f);
		const float climaxS = SmoothStep01(climaxProgress);
		const float breath = std::sin(climaxProgress * kTwoPi) * kCineCfg.heroBreathAmp;
		player_->SetIntroTransform({ 0.0f, breath, 0.0f }, { 1.0f, 1.0f, 1.0f }, { kCineCfg.heroPitchScale * climaxS, kCineCfg.heroYawScale * climaxS, 0.0f });
	}
	else if (!isDeathSequenceActive_ && !player_->IsDead())
	{
		player_->SyncAimWithCamera(&mouseInput);
	}
}

void GamePlayScene::UpdateSceneLightingAndTexts(float deltaTime)
{
	if (light)
	{
		float intensity = kSunBaseIntensity;
		if (lightFlashTimer_ > 0.0f) { lightFlashTimer_ -= deltaTime; intensity = kSunFlashIntensity; }
		light->SetDirectionalLightIntensity(intensity);

		const float flarePulse = kCineCfg.flarePulseBase + kCineCfg.flarePulseAmp * std::sin(
			RaidStats::GetInstance().raidTime * kCineCfg.flarePulseFreqRaid + clearSlowMoTimer_ * kCineCfg.flarePulseFreqSlow);
		const float baseFlareInt = allTargetsDestroyed_ ? (isGameCleared_ ? kCineCfg.flareIntClear : kCineCfg.flareIntUnlocked) : kCineCfg.flareIntLocked;
		light->SetPointLight(
			kCineCfg.flareLightColor, goalRingTransform_.translate + kCineCfg.flareLightOffset,
			baseFlareInt * flarePulse, allTargetsDestroyed_ ? kCineCfg.flareRadUnlocked : kCineCfg.flareRadLocked, kCineCfg.flareDecay);
	}

	for (auto it = floatingTexts_.begin(); it != floatingTexts_.end(); )
	{
		it->lifeTime -= deltaTime;
		it->position.y += kCineCfg.floatTextRiseSpd * deltaTime;
		if (it->isCritical) it->position.x += std::sin(it->lifeTime * kCineCfg.floatTextCritFreq) * kCineCfg.floatTextCritAmp * deltaTime;
		it = (it->lifeTime <= 0.0f) ? floatingTexts_.erase(it) : std::next(it);
	}
}

void GamePlayScene::Update()
{
	const float realDeltaTime = AdvanceDeltaTime();
	float deltaTime = realDeltaTime;

	// 軍用ヘリ急襲・ファストロープ降下演出中
	if (helicopterIntro_ && helicopterIntro_->IsActive())
	{
		bgmLoop_.Update("Resources/Audio/BGM_Raid_Tension.wav", kCineCfg.bgmTensionPeriod, kCineCfg.bgmIntroVol, realDeltaTime);
		UpdateGameIntro(realDeltaTime);
		return;
	}

	// 軍用シネマティック・ゲームオーバー演出中
	if (isDeathSequenceActive_)
	{
		bgmLoop_.Stop();
		flareHissLoop_.Stop();
		if (deathSequenceController_ && deathSequenceController_->IsActive()) deathSequenceController_->Update(realDeltaTime);
		SyncStageObjectsForCamera(0.0f, true);
		UpdateEnvironment();
		const float slowDt = realDeltaTime * kCineCfg.slowTimeScale;
		if (environmentSystem_) environmentSystem_->UpdateNatureAnimation(slowDt);
		if (combatSystem_) { combatSystem_->UpdateBullets(slowDt); combatSystem_->UpdateShellCasings(slowDt); }
		UpdateParticles(slowDt);
		if (smokeParticleManager_) smokeParticleManager_->Update(slowDt, GetPlayerPosition());
		UpdatePlayerHpBar();
		return;
	}

	if (extractionUnlockBannerTimer_ > 0.0f) extractionUnlockBannerTimer_ = (std::max)(0.0f, extractionUnlockBannerTimer_ - realDeltaTime);
	if (hitIndicatorTimer_ > 0.0f) hitIndicatorTimer_ = (std::max)(0.0f, hitIndicatorTimer_ - realDeltaTime);

	// レイド戦術BGM ＆ M18シグナル発煙筒の空間燃焼音ループ更新
	if (clearTransitionStarted_)
	{
		bgmLoop_.Stop();
		flareHissLoop_.Stop();
	}
	else
	{
		const float bgmVol = isGameCleared_ ? kCineCfg.bgmClearVol : (allTargetsDestroyed_ ? kCineCfg.bgmAlertVol : kCineCfg.bgmNormalVol);
		bgmLoop_.Update(
			allTargetsDestroyed_ ? "Resources/Audio/BGM_Raid_Alert.wav" : "Resources/Audio/BGM_Raid_Tension.wav",
			allTargetsDestroyed_ ? kCineCfg.bgmAlertPeriod : kCineCfg.bgmTensionPeriod, bgmVol, realDeltaTime);

		if (player_)
		{
			const float flareAtten = TacticalAudio::ComputeAttenuation(
				goalRingTransform_.translate + kCineCfg.flareAudioOffset, player_->GetPosition(), kCineCfg.flareAudioMinDist, kCineCfg.flareAudioMaxDist);
			flareHissLoop_.Update("Resources/Audio/SE_Flare_Hiss_Loop.wav", kCineCfg.flareAudioPeriod, kCineCfg.flareAudioBaseVol * flareAtten, realDeltaTime);
		}
	}

	if (hitStopTimer_ > 0.0f)
	{
		hitStopTimer_ = (std::max)(0.0f, hitStopTimer_ - realDeltaTime);
		deltaTime *= kCineCfg.slowTimeScale;
	}

	// クリア演出中のヘリ回収クライマックス＆スローモーション処理
	if (isGameCleared_)
	{
		if (!clearClimaxTriggered_) { clearClimaxTriggered_ = true; TriggerCameraShake(kCineCfg.clearClimaxShakeDur, kCineCfg.clearClimaxShakeInt); }
		deltaTime *= kCineCfg.clearTimeScale;
		clearSlowMoTimer_ -= realDeltaTime;
		if (clearSlowMoTimer_ <= 0.0f)
		{
			if (!clearTransitionStarted_)
			{
				clearTransitionStarted_ = true;
				auto& stats = RaidStats::GetInstance();
				stats.isSurvived = true;
				stats.totalLootValue = player_ ? player_->GetLootValue() : 0;
				stats.remainingHp = player_ ? player_->GetHP() : GameConfig::Player::kDefaultMaxHp;
				stats.maxHp = player_ ? player_->GetMaxHP() : GameConfig::Player::kDefaultMaxHp;
				stats.totalTargets = (std::max)(1, static_cast<int>(targets_.size()));
				SetSceneData<RaidStats>("RaidStats", stats);
				SceneTransition::Change("CLEAR", kCineCfg.clearFadeSec);
			}
			return;
		}
	}
	else if (!isDeathSequenceActive_)
	{
		RaidStats::GetInstance().raidTime += realDeltaTime;
		if (RaidStats::GetInstance().GetRemainingTime() <= 0.0f && player_ && !player_->IsDead())
		{
			RaidStats::GetInstance().isMIA = true;
			player_->TakeDamage(kInputCfg.lethalDamage, "DUCKOV AIRSPACE LOCKDOWN", "MIA (MISSING IN ACTION) - RAID TIME EXPIRED");
		}
	}

	UpdateSprites(deltaTime);
	UpdateDebugInput();
	if (!isGameCleared_)
	{
		UpdateCharacters(deltaTime);
		if (combatSystem_) combatSystem_->Update(deltaTime);
		if (collisionSystem_) collisionSystem_->Update();
		if (combatSystem_) combatSystem_->RemoveDeadBullets();
		if (player_) player_->PostCollisionUpdate();
	}
	else if (combatSystem_)
	{
		combatSystem_->UpdateBullets(deltaTime);
		combatSystem_->UpdateShellCasings(deltaTime);
		combatSystem_->RemoveDeadBullets();
	}
	UpdateParticles(deltaTime);
	UpdateCameraAndAim(realDeltaTime);
	UpdateExtractionGoal(deltaTime);
	if (helicopterIntro_) helicopterIntro_->UpdateExtractionPickup(realDeltaTime);
	if (smokeParticleManager_) smokeParticleManager_->Update(isGameCleared_ ? (realDeltaTime * kCineCfg.clearSmokeDtScale) : deltaTime, GetPlayerPosition());
	if (lootSystem_)
	{
		lootSystem_->Update(
			deltaTime, player_.get(), appParticleManager_.get(), floatingTexts_,
			smokeParticleManager_.get(), soilDustTextureIndex_, debrisChipTextureIndex_, sparkStreakTextureIndex_);
	}

	UpdateEnvironment();
	if (environmentSystem_) environmentSystem_->UpdateNatureAnimation(realDeltaTime);
	SyncStageObjectsForCamera(deltaTime, false);

	// すべての的が破壊されたかチェック
	const int totalTargets = static_cast<int>(targets_.size());
	int destroyedCount = 0;
	bool anyTargetAlive = false;
	for (const auto& t : targets_)
	{
		if (t && !t->IsDead()) anyTargetAlive = true;
		else if (t && t->IsDead()) ++destroyedCount;
	}
	allTargetsDestroyed_ = (totalTargets > 0 && !anyTargetAlive && destroyedCount >= totalTargets);
	if (allTargetsDestroyed_ && !reinforcementTriggered_) TriggerReinforcements();

	UpdatePlayerHpBar();
	CheckGameOver();
	if (sceneEntranceFadeTimer_ > 0.0f) sceneEntranceFadeTimer_ = (std::max)(0.0f, sceneEntranceFadeTimer_ - realDeltaTime);
	UpdateStressTestMode();
	UpdateSceneLightingAndTexts(deltaTime);
}

void GamePlayScene::BuildGroundTilesAndRiver(bool insertAtFront)
{
	using namespace GamePlayLevelLayout;
	const float startX = 0.0f - kGroundTileSize * static_cast<float>(kGroundGridW) * 0.5f + kGroundTileSize * 0.5f;
	const float startZ = kGroundGridCenterZ - kGroundTileSize * static_cast<float>(kGroundGridH) * 0.5f + kGroundTileSize * 0.5f;

	std::vector<std::unique_ptr<Obstacle>> groundAndRiver;
	groundAndRiver.reserve(static_cast<size_t>(kGroundGridW * kGroundGridH) + 1);
	for (int gz = 0; gz < kGroundGridH; ++gz)
	{
		for (int gx = 0; gx < kGroundGridW; ++gx)
		{
			auto tile = std::make_unique<Obstacle>();
			tile->Initialize(
				object3dCom, camera_,
				{ startX + static_cast<float>(gx) * kGroundTileSize, kGroundTileY, startZ + static_cast<float>(gz) * kGroundTileSize },
				1.0f, "ground_tile.obj", kUnitScale, { 0.0f, 0.0f, 0.0f });
			groundAndRiver.push_back(std::move(tile));
		}
	}

	auto river = std::make_unique<Obstacle>();
	river->Initialize(
		object3dCom, camera_, GameConfig::Environment::kRiverPosition, 1.0f, "river.obj",
		GameConfig::Environment::kRiverScale, GameConfig::Environment::kRiverRotation);
	groundAndRiver.push_back(std::move(river));

	const auto insertIt = insertAtFront ? obstacles_.begin() : obstacles_.end();
	obstacles_.insert(insertIt, std::make_move_iterator(groundAndRiver.begin()), std::make_move_iterator(groundAndRiver.end()));
}

void GamePlayScene::InitializeObstacles()
{
	using namespace GamePlayLevelLayout;
	obstacles_.clear();

	struct PendingEnemySpawn { Vector3 pos{}; float yaw = kAutoInitialYaw; bool isPatrol = false; Enemy::TacticalRole role = Enemy::TacticalRole::Garrison; std::string name; };
	std::vector<PendingEnemySpawn> pendingEnemySpawns;
	reinforceSpawnPoints_.clear();

	bool success = false;
	const char* const pathCandidates[] = { "Resources/stage_layout.json", "project/Resources/stage_layout.json", "../project/Resources/stage_layout.json" };

	nlohmann::json j;
	for (const char* filepath : pathCandidates)
	{
		if (!std::filesystem::exists(filepath)) continue;
		if (!BaziruEngine::IO::JsonSafeLoader::LoadSafe(filepath, j))
		{
			OutputDebugStringA((std::string("GamePlayScene: Failed to load stage_layout.json (corrupted or missing): ") + filepath + "\n").c_str());
			continue;
		}
		try
		{
			pendingEnemySpawns.clear();
			reinforceSpawnPoints_.clear();
			for (const auto& obj : j)
			{
				const std::string name = obj.value("name", "");
				const std::string type = obj.value("type", "");
				const Vector3 pos = ReadJsonVec3(obj["position"]);

				float spawnYaw = kAutoInitialYaw;
				if (obj.contains("rotation"))
				{
					const float ry = obj["rotation"]["y"].get<float>();
					if (std::abs(ry) > kSpawnYawEpsilon) spawnYaw = ry;
				}

				if (type == "EnemySpawn_Reinforce" || name.find("Enemy_Reinforce") != std::string::npos)
				{
					reinforceSpawnPoints_.push_back(pos);
					continue;
				}

				bool matchedEnemyRole = false;
				for (const auto& rule : kEnemyRoleRules)
				{
					if (type == rule.typeName || name.find(rule.nameKeyword) != std::string::npos)
					{
						pendingEnemySpawns.push_back({ pos, spawnYaw, rule.isPatrol, rule.role, name });
						matchedEnemyRole = true;
						break;
					}
				}
				if (matchedEnemyRole) continue;

				if (type == "EnemySpawn" || name == "Enemy_Spawn_Point" || ContainsAny(name, { "Enemy_Sentry", "Enemy_Garrison", "Enemy_Spawn" }))
				{
					pendingEnemySpawns.push_back({ pos, spawnYaw, false, Enemy::TacticalRole::Garrison, name });
					continue;
				}

				if (name.empty() || name == "GroundPlane" || type == "SpawnPoint" || type == "GoalRing" || name == "Player_Spawn_Point" || name == "Goal_Extraction_Ring") continue;

				const Vector3 scl = obj.contains("scale") ? ReadJsonVec3(obj["scale"]) : kUnitScale;
				const Vector3 rot = obj.contains("rotation") ? ReadJsonVec3(obj["rotation"]) : Vector3{ 0.0f, 0.0f, 0.0f };
				const std::string modelFile = obj.value("modelFilename", "fence.obj");

				if (type == "Target" || ContainsAny(name, { "Target", "target" }))
				{
					auto target = std::make_unique<Target>();
					target->Initialize(object3dCom, camera_, pos, obj.value("radius", kDefaultTargetRadius));
					targets_.push_back(std::move(target));
					continue;
				}
				if (type == "River" || ContainsAny(name, { "River", "river", "Water", "water" })) continue;
				if (modelFile == "plane.obj" || ContainsAny(name, { "Ground", "Plane" })) continue;

				auto obs = std::make_unique<Obstacle>();
				obs->Initialize(object3dCom, camera_, pos, 1.0f, modelFile, scl, rot);
				obstacles_.push_back(std::move(obs));
			}

			BuildGroundTilesAndRiver(true);
			success = true;
			OutputDebugStringA("GamePlayScene: Successfully loaded obstacles from JSON.\n");
			break;
		}
		catch (const std::exception& e)
		{
			char errorMsg[kJsonErrorBufferSize];
			sprintf_s(errorMsg, "GamePlayScene: Failed to parse stage_layout.json: %s\n", e.what());
			OutputDebugStringA(errorMsg);
		}
	}

	// JSON読み込み失敗またはデータ不足時はフォールバックテーブルから一括構築
	if (!success || obstacles_.size() <= kMinValidObstacleCount)
	{
		OutputDebugStringA("GamePlayScene: Using updated default obstacle placement.\n");
		BuildGroundTilesAndRiver(false);
		obstacles_.reserve(obstacles_.size() + std::size(kFallbackObstacles));
		for (const auto& spec : kFallbackObstacles)
		{
			auto obs = std::make_unique<Obstacle>();
			obs->Initialize(object3dCom, camera_, spec.pos, 1.0f, spec.modelFile, spec.scale, spec.rot);
			obstacles_.push_back(std::move(obs));
		}
	}

	// 全障害物・川・橋コライダー登録完了後、非矩形マルチウイング全域のNavMeshをベイク
	BaziruEngine::AI::NavMesh::GetInstance()->BuildGrid(
		kNavMeshGrid.minX, kNavMeshGrid.maxX, kNavMeshGrid.minZ, kNavMeshGrid.maxZ, kNavMeshGrid.cellSize, kNavMeshGrid.agentRadius);

	const std::vector<Vector3> nwPatrolVec(std::begin(kNwWingPatrolRoute), std::end(kNwWingPatrolRoute));
	if (!pendingEnemySpawns.empty())
	{
		for (const auto& sp : pendingEnemySpawns)
		{
			std::vector<Vector3> customRoute;
			if (sp.isPatrol)
			{
				customRoute = (sp.pos.x < kNwWingPatrolMaxX && sp.pos.z >= kNwWingPatrolMinZ && sp.pos.z < kNwWingPatrolMaxZ)
					? nwPatrolVec
					: std::vector<Vector3>{ { sp.pos.x - kGenericPatrolHalfSpanX, 0.0f, sp.pos.z }, { sp.pos.x, 0.0f, sp.pos.z }, { sp.pos.x + kGenericPatrolHalfSpanX, 0.0f, sp.pos.z } };
			}
			SpawnEnemy(sp.pos, sp.isPatrol, sp.yaw, sp.role, customRoute, false);
		}
	}
	else if (enemies_.empty())
	{
		for (const auto& spec : kFallbackEnemies)
		{
			SpawnEnemy(spec.pos, spec.isPatrol, spec.yaw, spec.role, spec.useNwRoute ? nwPatrolVec : std::vector<Vector3>{}, false);
		}
	}

	if (targets_.empty())
	{
		targets_.reserve(std::size(kFallbackTargetPositions));
		for (const Vector3& pos : kFallbackTargetPositions)
		{
			auto target = std::make_unique<Target>();
			target->Initialize(object3dCom, camera_, pos, kDefaultTargetRadius);
			targets_.push_back(std::move(target));
		}
	}

	// 非矩形マルチウイング要塞の導線に沿った5段階チュートリアル看板
	tutorialSigns_.clear();
	tutorialSigns_.reserve(std::size(kTutorialSigns));
	for (const auto& spec : kTutorialSigns)
	{
		auto sign = std::make_unique<TutorialSign>();
		sign->Initialize(object3dCom, camera_, spec.pos, reinterpret_cast<const char*>(spec.message), spec.triggerRadius);
		tutorialSigns_.push_back(std::move(sign));
	}
}

void GamePlayScene::UpdateObstacles()
{
	for (auto& obs : obstacles_) { if (obs) obs->Update(); }
	if (player_)
	{
		const Vector3 pPos = player_->GetPosition();
		for (auto& sign : tutorialSigns_) { if (sign) sign->Update(pPos); }
	}
}

RenderContext GamePlayScene::BuildRenderContext() const
{
	RenderContext ctx{};
	if (!directXCom) return ctx;

	ctx.commandList = directXCom->GetCommandList().Get();
	ctx.windowAPI = directXCom->GetWindowAPI();
	ctx.camera = camera_;
	ctx.light = SceneManager::GetInstance()->GetLight();
	ctx.materialGPUAddress = (materialManager && materialManager->GetMaterialResource())
		? materialManager->GetMaterialResource()->GetGPUVirtualAddress() : 0;
	return ctx;
}

void GamePlayScene::Draw(SceneRenderRequests& renderRequests)
{
	renderRequests.sceneDrawn = true;
	const RenderContext ctx = BuildRenderContext();

	// 0. 広域外周芝生フィールド -> 1. ベースステージ構造物 -> 2. リアル自然環境レイヤー
	if (environmentSystem_) environmentSystem_->DrawOuterGround(ctx);
	for (auto& obs : obstacles_) { if (obs) obs->Draw(ctx); }
	for (auto& obs : stressTestObstacles_) { if (obs) obs->Draw(ctx); }
	if (environmentSystem_) environmentSystem_->DrawNatureLayers(ctx);

	// 3. 立体脱出ヘリパッド＆地上信号弾・標的・看板・キャラクター・弾丸を描画
	if (extractionPadObject_)
	{
		extractionPadObject_->SetColor(allTargetsDestroyed_ ? kPadUnlockedColor : kPadLockedColor);
		extractionPadObject_->Draw(ctx);
	}
	if (extractionSignalFlareObject_) extractionSignalFlareObject_->Draw(ctx);
	for (auto& t : targets_) { if (t) t->Draw(ctx); }
	for (auto& sign : tutorialSigns_) { if (sign) sign->Draw(ctx); }
	if (player_) player_->Draw(ctx);
	for (auto& enemy : enemies_) { if (enemy) enemy->Draw(ctx); }

	if (combatSystem_)
	{
		combatSystem_->DrawDroppedMagazines(ctx);
		combatSystem_->DrawShellCasings(ctx);
		ID3D12PipelineState* tracerPSO = combatSystem_->GetAdditiveTracerPSO();
		for (auto& bullet : combatSystem_->GetBullets()) { if (bullet) bullet->Draw(ctx, tracerPSO); }
	}

	if (helicopterIntro_ && (helicopterIntro_->IsActive() || helicopterIntro_->IsExtractionVisible())) helicopterIntro_->Draw3D(ctx);
	if (deathSequenceController_ && deathSequenceController_->IsActive()) deathSequenceController_->Draw3D(ctx);
	if (hitEffect_) hitEffect_->Draw();

	// ストレートアルファ薄煙・加算合成・GPUParticle の描画
	if (smokeParticleManager_ && particleManager)
	{
		const uint32_t sTex = (wispySmokeTextureIndex_ != TextureManager::kInvalidTextureIndex) ? wispySmokeTextureIndex_ : smokeTextureIndex_;
		smokeParticleManager_->Draw(MakeTexturedContext(ctx, sTex));
	}
	if (appParticleManager_ && particleManager)
	{
		appParticleManager_->Draw(MakeTexturedContext(ctx, smokeTextureIndex_));
	}
	if (particleManager)
	{
		const uint32_t gpuSmokeTex = (escapeSmokeTextureIndex_ != TextureManager::kInvalidTextureIndex) ? escapeSmokeTextureIndex_ : smokeTextureIndex_;
		particleManager->Draw(ctx.commandList, MakeTexturedContext(ctx, gpuSmokeTex), 0);
	}

	HideSpriteAtIndex(sprites, cursorSpriteIndex);
	if (spriteManager_) spriteManager_->DrawAll(ctx, &debugCamera_, &sprites);

	// --- タクティカルHUD統合描画 ---
	if (hud_ && !isGameIntroActive_ &&
		(!StingerTransition::GetInstance() || !StingerTransition::GetInstance()->IsTransitioning()) &&
		(!DeathTransition::GetInstance() || !DeathTransition::GetInstance()->IsTransitioning()))
	{
		GamePlayHUDContext hudCtx;
		hudCtx.camera = camera_;
		hudCtx.player = player_.get();
		hudCtx.enemies = &enemies_;
		hudCtx.obstacles = &obstacles_;
		hudCtx.targets = &targets_;
		hudCtx.tutorialSigns = &tutorialSigns_;
		hudCtx.lootProps = lootSystem_ ? &lootSystem_->GetProps() : nullptr;
		hudCtx.floatingTexts = &floatingTexts_;
		hudCtx.extractionGoalPos = goalRingTransform_.translate;
		hudCtx.isReadyToExtract = allTargetsDestroyed_;
		hudCtx.isPlayerInExtractionZone = isPlayerInExtractionZone_;
		hudCtx.extractionTimer = extractionTimer_;
		hudCtx.extractionMaxTime = kExtractionMaxTime;
		hudCtx.extractionUnlockBannerTimer = extractionUnlockBannerTimer_;
		hudCtx.clearClimaxProgress = isGameCleared_ ? (std::clamp)(1.0f - (clearSlowMoTimer_ / kClearSlowMoDuration), 0.0f, 1.0f) : 0.0f;
		hudCtx.isGameCleared = isGameCleared_;
		hudCtx.isDeathSequenceActive = isDeathSequenceActive_;
		hudCtx.deathSequenceTimer = deathSequenceTimer_;
		hudCtx.hitIndicatorTimer = hitIndicatorTimer_;
		hudCtx.hitIndicatorAngle = hitIndicatorAngle_;
		hudCtx.playerSoundRadius = playerSoundRadius_;
		hudCtx.playerSoundMaxRadius = playerSoundMaxRadius_;
		hudCtx.playerSoundTimer = playerSoundTimer_;
		hudCtx.showDebugGizmos = showDebugGizmos_;
		hudCtx.showPerformanceTracker = showPerformanceTracker_;
		hudCtx.isStressTestActive = isStressTestActive_;
		hudCtx.stressTestCount = static_cast<int>(stressTestObstacles_.size());
		hudCtx.sceneEntranceFadeTimer = sceneEntranceFadeTimer_;
		hudCtx.isOverheadCameraMode = isOverheadCameraMode_;
		hudCtx.isFullMapOpen = isFullMapOpen_ || IsAppKeyDown(VK_TAB);
		hudCtx.cameraZoom = cameraZoom_;
		hud_->Draw(hudCtx, kFixedDeltaTime);
	}

	if (helicopterIntro_ && helicopterIntro_->IsActive()) helicopterIntro_->DrawUI();
	if (deathSequenceController_ && deathSequenceController_->IsActive()) deathSequenceController_->DrawUI();
	renderRequests.sceneDrawn = true;
}

void GamePlayScene::AddFloatingText(const Vector3& worldPos, const std::string& text, const Vector4& color, bool isCritical)
{
	FloatingText ft;
	ft.position = worldPos;
	ft.position.x += (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 2.0f - 1.0f) * kCineCfg.floatTextJitterXZ;
	ft.position.z += (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 2.0f - 1.0f) * kCineCfg.floatTextJitterXZ;
	ft.text = text;
	ft.color = color;
	ft.maxLifeTime = isCritical ? kCineCfg.floatTextCritLife : kCineCfg.floatTextNormLife;
	ft.lifeTime = ft.maxLifeTime;
	ft.isCritical = isCritical;
	floatingTexts_.push_back(ft);
}

void GamePlayScene::UpdateStressTestMode()
{
	using namespace GamePlayLevelLayout;
	if (!object3dCom || !camera_) return;

	if (isStressTestActive_ && stressTestObstacles_.empty())
	{
		stressTestObstacles_.reserve(static_cast<size_t>(kStressTestRows * kStressTestCols));
		for (int r = 0; r < kStressTestRows; ++r)
		{
			for (int c = 0; c < kStressTestCols; ++c)
			{
				const Vector3 pos = {
					(static_cast<float>(c) - static_cast<float>(kStressTestCols) * 0.5f) * kStressSpacing, 0.0f,
					(static_cast<float>(r) - static_cast<float>(kStressTestRows) * 0.5f) * kStressSpacing + kStressBaseOffsetZ
				};
				auto obs = std::make_unique<Obstacle>();
				obs->Initialize(object3dCom, camera_, pos, 1.0f, "container.obj", kUnitScale, { 0.0f, 0.0f, 0.0f });
				stressTestObstacles_.push_back(std::move(obs));
			}
		}
	}
	else if (!isStressTestActive_ && !stressTestObstacles_.empty())
	{
		stressTestObstacles_.clear();
	}
}

void GamePlayScene::TriggerCameraShake(float duration, float intensity)
{
	if (camera_) camera_->Shake(duration, intensity);
}
