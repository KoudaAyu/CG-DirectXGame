#include "GameOverScene.h"
#include "GameOverSceneTuning.h"
#include "KeyInput.h"
#include "DirectXCom.h"
#include "SceneManager.h"
#include "TextureManager.h"
#include "Object3dCom.h"
#include "ParticleManager.h"
#include "RenderContext.h"
#include "Baziru3_Engine/Graphics/Light/Light.h"
#include "Baziru3_Engine/Graphics/Graphics/SceneRenderRequests.h"
#include "Baziru3_Engine/Framework/Audio/AudioManager.h"
#include "Application/Transition/StingerTransition.h"
#include "Application/Transition/DeathTransition.h"
#include "Application/UI/TacticalUICanvas.h"
#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>

namespace
{
	using namespace GameOverSceneTuning;

	float EaseInQuad(float t) { return t * t; }
	float EaseOutQuad(float t) { return t * (2.0f - t); }

	float EaseInOutCubic(float t)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		if (t < 0.5f)
		{
			return kCubicInScale * t * t * t;
		}
		const float u = (1.0f - t) * 2.0f;
		return 1.0f - (u * u * u) * 0.5f;
	}

	Vector3 LerpVec3(const Vector3& a, const Vector3& b, float t)
	{
		return {
			a.x + (b.x - a.x) * t,
			a.y + (b.y - a.y) * t,
			a.z + (b.z - a.z) * t
		};
	}

	Vector3 TransformOffsetByRotation(const Vector3& offset, const Matrix4x4& rotMat)
	{
		return {
			offset.x * rotMat.m[0][0] + offset.y * rotMat.m[1][0] + offset.z * rotMat.m[2][0],
			offset.x * rotMat.m[0][1] + offset.y * rotMat.m[1][1] + offset.z * rotMat.m[2][1],
			offset.x * rotMat.m[0][2] + offset.y * rotMat.m[1][2] + offset.z * rotMat.m[2][2]
		};
	}

	void DrawModel(Object3dCom* com, Object3d* model, const RenderContext& base, uint32_t texIndex)
	{
		if (!com || !model) return;
		RenderContext ctx = base;
		if (texIndex != UINT32_MAX)
		{
			ctx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(texIndex);
		}
		com->Draw(model, ctx, model->GetModelData(), true);
	}

	std::unique_ptr<Object3d> CreateDioramaObject(
		Object3dCom* com, Camera* camera, const Object3d::ModelData& modelData, const Vector3& scale)
	{
		auto obj = std::make_unique<Object3d>();
		obj->Initialize(com, modelData);
		obj->SetCamera(camera);
		obj->SetScale(scale);
		obj->SetEnableLighting(true);
		return obj;
	}

	Object3d::ModelData LoadTexturedObj(
		const char* objFile, const char* texPath, uint32_t texIndex, float boundingRadius)
	{
		Object3d::ModelData data = Object3d::LoadObjFile("Resources", objFile);
		data.material.textureFilePath = texPath;
		data.material.textureIndex = texIndex;
		data.boundingRadius = boundingRadius;
		return data;
	}

	void AppendBoxMesh(
		Object3d::ModelData& data,
		const Vector3& center,
		const Vector3& size,
		const Vector2& uvMin,
		const Vector2& uvMax)
	{
		const Vector3 h = size * 0.5f;
		const Vector3 corners[kBoxCornerCount] = {
			{ center.x - h.x, center.y - h.y, center.z - h.z },
			{ center.x + h.x, center.y - h.y, center.z - h.z },
			{ center.x + h.x, center.y + h.y, center.z - h.z },
			{ center.x - h.x, center.y + h.y, center.z - h.z },
			{ center.x - h.x, center.y - h.y, center.z + h.z },
			{ center.x + h.x, center.y - h.y, center.z + h.z },
			{ center.x + h.x, center.y + h.y, center.z + h.z },
			{ center.x - h.x, center.y + h.y, center.z + h.z },
		};

		for (int f = 0; f < kBoxFaceCount; ++f)
		{
			const uint32_t fBase = static_cast<uint32_t>(data.vertices.size());
			for (int v = 0; v < kFaceVertexCount; ++v)
			{
				const Vector3& corner = corners[kBoxFaces[f][v]];
				Sprite::VertexData vd{};
				vd.position = { corner.x, corner.y, corner.z, 1.0f };
				vd.texcoord = {
					kQuadUseMaxU[v] ? uvMax.x : uvMin.x,
					kQuadUseMaxV[v] ? uvMax.y : uvMin.y
				};
				vd.normal = kBoxNormals[f];
				data.vertices.push_back(vd);
			}
			for (int i = 0; i < kFaceIndexCount; ++i)
			{
				data.indices.push_back(fBase + kQuadTriOffsets[i]);
			}
		}
	}

	void BuildStretcherModel(Object3d::ModelData& data)
	{
		data.vertices.clear();
		data.indices.clear();
		data.vertices.reserve(std::size(kStretcherBoxes) * kBoxFaceCount * kFaceVertexCount);
		data.indices.reserve(std::size(kStretcherBoxes) * kBoxFaceCount * kFaceIndexCount);

		const Vector2 fullUvMin{ 0.0f, 0.0f };
		const Vector2 fullUvMax{ 1.0f, 1.0f };
		for (const auto& part : kStretcherBoxes)
		{
			AppendBoxMesh(data, part.center, part.size, fullUvMin, fullUvMax);
		}
		data.boundingRadius = kProceduralBoundingRadius;
	}

	void BuildHelicopterDoorModel(Object3d::ModelData& data)
	{
		data.vertices.clear();
		data.indices.clear();
		data.vertices.reserve(std::size(kHeliDoorBoxes) * kBoxFaceCount * kFaceVertexCount);
		data.indices.reserve(std::size(kHeliDoorBoxes) * kBoxFaceCount * kFaceIndexCount);

		for (const auto& part : kHeliDoorBoxes)
		{
			const UvRectSpec& uv = kDoorUvRects[static_cast<size_t>(part.uvZone)];
			AppendBoxMesh(data, part.center, part.size, uv.uvMin, uv.uvMax);
		}
		data.boundingRadius = kProceduralBoundingRadius;
	}

	uint32_t WithAlpha(uint32_t col, float a)
	{
		a = std::clamp(a, 0.0f, 1.0f);
		const uint32_t baseA = (col >> UI_COL32_A_SHIFT) & kAlphaByteMask;
		const uint32_t newA = static_cast<uint32_t>(static_cast<float>(baseA) * a);
		return (col & ~UI_COL32_A_MASK) | (newA << UI_COL32_A_SHIFT);
	}

	void AddCenteredText(
		TacticalDrawList* dl, const TacticalFont* font, float size,
		float centerX, float topY, uint32_t col, const char* text)
	{
		if (!dl || !font || !text) return;
		const UIVec2 ts = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
		dl->AddText(font, size, UIVec2(centerX - ts.x * 0.5f, topY), col, text);
	}
}

// -----------------------------------------------------------------------------
// シーン初期化 / 終了
// -----------------------------------------------------------------------------

bool GameOverScene::IsAnyTransitionActive() const
{
	return (DeathTransition::GetInstance() && DeathTransition::GetInstance()->IsTransitioning()) ||
	       (StingerTransition::GetInstance() && StingerTransition::GetInstance()->IsTransitioning());
}

void GameOverScene::BeginTakeoff(bool toTitle)
{
	state_ = State::Takeoff;
	actionTimer_ = 0.0f;
	ducksLoaded_ = true;
	nextScene_ = toTitle ? "TITLE" : "GAMEPLAY";
	if (!takeoffSoundPlayed_)
	{
		takeoffSoundPlayed_ = true;
		TacticalAudio::Play2D("Resources/Audio/SE_UI_Click.wav", kUiClickVolume);
		TacticalAudio::Play2D("Resources/Audio/SE_Heli_Door.wav", kHeliDoorVolume);
	}

	if (duckSeSoundId_ >= 0)
	{
		if (AudioManager* am = GetAudioManager())
		{
			am->PlayOneShot(duckSeSoundId_, kDuckQuackVolume);
		}
	}
}

void GameOverScene::InitializeScene()
{
	ShowCursor(TRUE);
	DirectXCom* dx = GetDirectXCom();
	if (dx)
	{
		input_ = new KeyInput();
		input_->Initialize(dx->GetWindowAPI());
	}

	raidStats_ = GetSceneData<RaidStats>("RaidStats", RaidStats::GetInstance());

	state_ = State::Approaching;
	timer_ = 0.0f;
	actionTimer_ = 0.0f;
	transitionRequested_ = false;
	nextScene_ = "GAMEPLAY";
	ducksLoaded_ = false;
	loadSoundPlayed_ = false;
	takeoffSoundPlayed_ = false;
	prevWalkStep_ = -1;

	// 音声マネージャーからアヒルSEをプリロード
	duckSeSoundId_ = -1;
	if (AudioManager* am = GetAudioManager())
	{
		duckSeSoundId_ = am->Load("Resources/Audio/SE_Duck.mp3");
	}

	// スカイボックスを表示して黄昏の空を展開
	if (SceneManager::GetInstance())
	{
		SceneManager::GetInstance()->SetShowSkybox(true);
	}

	// 黄昏・夜間救難作戦ライティング（冷たい青灰色の薄明 ＆ ヘリの強力なハロゲン救難投光器）
	if (SceneManager::GetInstance() && SceneManager::GetInstance()->GetLight())
	{
		Light* light = SceneManager::GetInstance()->GetLight();
		light->SetDirectionalLight(kDirLightColor, kDirLightDirection, kDirLightIntensity);
		light->SetPointLight(
			kPointLightInitColor, kPointLightPosition,
			kPointLightInitIntensity, kPointLightRadius, kPointLightDecay);
	}

	InitializeDiorama();
}

void GameOverScene::InitializeDiorama()
{
	Object3dCom* com = GetObject3dCom();
	if (!com) return;

	// 1. エンジンのメインカメラを取得・初期化（正面ワイド望遠構図）
	if (!camera_)
	{
		camera_ = SceneManager::GetInstance() ? SceneManager::GetInstance()->GetCamera() : nullptr;
	}
	if (camera_)
	{
		camera_->SetTranslate(kBaseCameraPos);
		camera_->SetRotate(kBaseCameraRot);
		camera_->SetFovY(kBaseCameraFovY);
		camera_->Update();
	}

	// 2. テクスチャ読み込み
	if (TextureManager* tm = TextureManager::GetInstance())
	{
		duckTexIndex_ = tm->Load("Resources/duck_player_wounded.png");
		if (duckTexIndex_ == UINT32_MAX)
		{
			duckTexIndex_ = tm->Load("Resources/duck.png");
		}
		duckMedicTexIndex_ = tm->Load("Resources/duck_medic.png");
		heliTexIndex_      = tm->Load("Resources/helicopter_texture.png");
		helipadTexIndex_   = tm->Load("Resources/extraction_pad.png");
		stretcherTexIndex_ = tm->Load("Resources/container_military.png");
		pineTexIndex_      = tm->Load("Resources/duckov_pine_tree.png");
		sandbagTexIndex_   = tm->Load("Resources/duckov_sandbag.png");
		barrelTexIndex_    = tm->Load("Resources/duckov_barrel.png");
		crateTexIndex_     = tm->Load("Resources/duckov_crate.png");
		fenceTexIndex_     = tm->Load("Resources/duckov_fence.png");
		groundTexIndex_    = tm->Load("Resources/grass.png");
		vignetteTexIndex_  = tm->Load("Resources/title_vignette.png");
		logoTexIndex_      = tm->Load("Resources/gameover_logo.png");
		promptTexIndex_    = tm->Load("Resources/gameover_prompt.png");
		debriefTexIndex_   = tm->Load("Resources/gameover_debrief.png");

		moteCircleTexIndex_ = tm->Load("Resources/particle_minovsky_circle.png");
		if (moteCircleTexIndex_ == UINT32_MAX) moteCircleTexIndex_ = tm->Load("Resources/circle2.png");
		rainStreakTexIndex_ = tm->Load("Resources/particle_spark_streak.png");
		if (rainStreakTexIndex_ == UINT32_MAX) rainStreakTexIndex_ = moteCircleTexIndex_;
		mistSmokeTexIndex_  = tm->Load("Resources/particle_wispy_smoke.png");
		if (mistSmokeTexIndex_ == UINT32_MAX) mistSmokeTexIndex_ = tm->Load("Resources/title_smoke_puff.png");
		if (mistSmokeTexIndex_ == UINT32_MAX) mistSmokeTexIndex_ = moteCircleTexIndex_;
	}

	// 2B. 夜間救難ジオラマ用 GPUパーティクルマネージャー初期化（冷たい霧雨・ハロゲン投光器ダスト・ローター夜霧）
	if (ParticleManager* pm = GetParticleManager())
	{
		appParticleManager_ = std::make_unique<AppParticleManager>();
		appParticleManager_->Initialize(pm);
		appParticleManager_->SetGroundY(0.0f);
		appParticleManager_->SetPremultiplyAlpha(true);
		appParticleManager_->SetStableInstanceSlots(true);

		smokeParticleManager_ = std::make_unique<AppParticleManager>();
		smokeParticleManager_->Initialize(pm);
		smokeParticleManager_->SetGroundY(0.02f);
		smokeParticleManager_->SetAlphaBlend(true);
		smokeParticleManager_->SetStableInstanceSlots(true);
	}
	rainEmitTimer_ = lightMoteTimer_ = rotorMistTimer_ = 0.0f;

	// 3. 救難ヘリコプター（MEDEVAC HELICOPTER）
	{
		const Object3d::ModelData bodyData = LoadTexturedObj(
			"helicopter_body.obj", "Resources/helicopter_texture.png", heliTexIndex_, kHeliBoundingRadius);
		heliBody_ = CreateDioramaObject(com, camera_, bodyData, kHeliScaleVec);

		const Object3d::ModelData rotorData = LoadTexturedObj(
			"helicopter_rotor.obj", "Resources/helicopter_texture.png", heliTexIndex_, kHeliBoundingRadius);
		heliMainRotor_      = CreateDioramaObject(com, camera_, rotorData, kHeliScaleVec);
		heliMainRotorBlur1_ = CreateDioramaObject(com, camera_, rotorData, kHeliScaleVec);
		heliMainRotorBlur2_ = CreateDioramaObject(com, camera_, rotorData, kHeliScaleVec);

		const Object3d::ModelData tailData = LoadTexturedObj(
			"helicopter_tailrotor.obj", "Resources/helicopter_texture.png", heliTexIndex_, kHeliBoundingRadius);
		heliTailRotor_ = CreateDioramaObject(com, camera_, tailData, kHeliScaleVec);

		Object3d::ModelData doorData{};
		BuildHelicopterDoorModel(doorData);
		doorData.material.textureFilePath = "Resources/helicopter_texture.png";
		doorData.material.textureIndex = heliTexIndex_;
		doorData.boundingRadius = kHeliBoundingRadius;
		heliCargoDoor_ = CreateDioramaObject(com, camera_, doorData, kHeliScaleVec);

		doorSlideProgress_ = 0.0f;
		heliPos_ = kInitialHeliPos;
		heliRot_ = kInitialHeliRot;
		mainRotorAngle_ = 0.0f;
		tailRotorAngle_ = 0.0f;
	}

	// 4. 脱出ヘリパッド（EXTRACTION HELIPAD）
	{
		const Object3d::ModelData padData = LoadTexturedObj(
			"plane.obj", "Resources/extraction_pad.png", helipadTexIndex_, kHelipadBoundingRadius);
		helipadModel_ = CreateDioramaObject(com, camera_, padData, kHelipadScale);
		helipadModel_->SetRotate({ -kHalfPi, 0.0f, 0.0f });
		helipadModel_->SetTranslate(kHelipadPos);
	}

	// 5. 担架チーム（2羽の衛生兵アヒル＋本格担架＋負傷主役アヒル）
	{
		const Object3d::ModelData duckData = LoadTexturedObj(
			"player.obj", "Resources/duck.png", duckTexIndex_, kDuckBoundingRadius);
		duckRearMedic_  = CreateDioramaObject(com, camera_, duckData, kDuckScaleVec);
		duckFrontMedic_ = CreateDioramaObject(com, camera_, duckData, kDuckScaleVec);
		duckWounded_    = CreateDioramaObject(com, camera_, duckData, kDuckScaleVec);

		Object3d::ModelData stretcherData{};
		BuildStretcherModel(stretcherData);
		stretcherData.material.textureFilePath = "Resources/container_military.png";
		stretcherData.material.textureIndex = stretcherTexIndex_;
		stretcherBed_ = CreateDioramaObject(com, camera_, stretcherData, kStretcherScaleVec);

		teamPos_ = kInitialTeamPos;
		teamStartPos_ = teamPos_;
		walkPhase_ = 0.0f;
		stretcherLiftY_ = 0.0f;
		stretcherSlideX_ = 0.0f;
		ducksLoaded_ = false;
	}

	// 6. 地形（地面プレーン）
	{
		const Object3d::ModelData groundData = LoadTexturedObj(
			"ground.obj", "Resources/grass.png", groundTexIndex_, kGroundBoundingRadius);
		groundModel_ = CreateDioramaObject(com, camera_, groundData, kGroundScale);
		groundModel_->SetTranslate({ 0.0f, 0.0f, 0.0f });
	}

	// 7. リアルな軍事救難前線基地・脱出ゾーン（LZ）の背景小道具構築（メッシュ読込を種類単位でキャッシュ）
	struct PropAssetBinding
	{
		const char* objFile;
		const char* texPath;
		uint32_t    texIndex;
	};
	const std::array<PropAssetBinding, static_cast<size_t>(PropKind::Count)> assetBindings = {{
		{ "duckov_pine_tree.obj",    "Resources/duckov_pine_tree.png",   pineTexIndex_      },
		{ "watchtower.obj",          "Resources/duckov_crate.png",       crateTexIndex_     },
		{ "duckov_fence.obj",        "Resources/duckov_fence.png",       fenceTexIndex_     },
		{ "container.obj",           "Resources/container_military.png", stretcherTexIndex_ },
		{ "duckov_sandbag_wall.obj", "Resources/duckov_sandbag.png",     sandbagTexIndex_   },
		{ "duckov_barrel_stack.obj", "Resources/duckov_barrel.png",      barrelTexIndex_    },
		{ "duckov_crate.obj",        "Resources/duckov_crate.png",       crateTexIndex_     },
	}};

	std::array<Object3d::ModelData, static_cast<size_t>(PropKind::Count)> cachedModels{};
	std::array<bool, static_cast<size_t>(PropKind::Count)> modelLoaded{};

	props_.clear();
	props_.reserve(std::size(kDioramaProps));
	for (const auto& spec : kDioramaProps)
	{
		const size_t kindIdx = static_cast<size_t>(spec.kind);
		const PropAssetBinding& binding = assetBindings[kindIdx];
		if (!modelLoaded[kindIdx])
		{
			cachedModels[kindIdx] = LoadTexturedObj(
				binding.objFile, binding.texPath, binding.texIndex, kPropBoundingRadius);
			modelLoaded[kindIdx] = true;
		}
		auto model = CreateDioramaObject(com, camera_, cachedModels[kindIdx], spec.scale);
		model->SetTranslate(spec.pos);
		model->SetRotate(spec.rot);
		props_.push_back({ std::move(model), binding.texIndex });
	}
}

void GameOverScene::Finalize()
{
	bgmLoop_.Stop();
	rotorLoop_.Stop();

	delete input_;
	input_ = nullptr;

	heliBody_.reset();
	heliMainRotor_.reset();
	heliMainRotorBlur1_.reset();
	heliMainRotorBlur2_.reset();
	heliTailRotor_.reset();
	heliCargoDoor_.reset();
	helipadModel_.reset();
	duckRearMedic_.reset();
	duckFrontMedic_.reset();
	duckWounded_.reset();
	stretcherBed_.reset();
	groundModel_.reset();
	props_.clear();
	smokeParticleManager_.reset();
	appParticleManager_.reset();
}

// -----------------------------------------------------------------------------
// 更新
// -----------------------------------------------------------------------------

void GameOverScene::Update()
{
	const float deltaTime = kFixedDeltaTime;
	const bool isTransitioning = IsAnyTransitionActive();

	// シャッターが完全に開き切るまで、シーンタイマーは進めない
	if (!isTransitioning)
	{
		timer_ += deltaTime;
		actionTimer_ += deltaTime;
	}

	bool confirm = false;
	bool toTitle = false;
	if (input_ && !isTransitioning)
	{
		input_->Update();
		confirm = input_->TriggerKey(DIK_SPACE) || input_->TriggerKey(DIK_RETURN);
		toTitle = input_->TriggerKey(DIK_T);
		if (input_->TriggerKey(DIK_F7))
		{
			RaidStats::GetInstance().autoDemoMode = !RaidStats::GetInstance().autoDemoMode;
			TacticalAudio::Play2D("Resources/Audio/SE_UI_Click.wav", 0.55f);
			if (RaidStats::GetInstance().autoDemoMode)
			{
				RaidStats::GetInstance().autoDemoCycleIndex = 1;
				confirm = true;
			}
		}
	}
	if (RaidStats::GetInstance().autoDemoMode && state_ == State::Ready && actionTimer_ >= 3.6f)
	{
		confirm = true;
	}

	switch (state_)
	{
	case State::Approaching:
		if (timer_ >= kEarlySkipEnableTime && (confirm || toTitle))
		{
			BeginTakeoff(toTitle);
		}
		else if (actionTimer_ >= kApproachDuration)
		{
			state_ = State::Loading;
			actionTimer_ = 0.0f;
			if (!loadSoundPlayed_)
			{
				loadSoundPlayed_ = true;
				TacticalAudio::Play2D("Resources/Audio/SE_Weapon_Holster.wav", kHolsterLoadVolume);
			}
		}
		break;

	case State::Loading:
		if (confirm || toTitle)
		{
			BeginTakeoff(toTitle);
		}
		else if (actionTimer_ >= kLoadDuration)
		{
			state_ = State::Ready;
			actionTimer_ = 0.0f;
			ducksLoaded_ = true;
		}
		break;

	case State::Ready:
		if (confirm || toTitle)
		{
			BeginTakeoff(toTitle);
		}
		break;

	case State::Takeoff:
	{
		const bool skip = confirm && actionTimer_ > kTakeoffSkipMinTime;
		if (!transitionRequested_ && (actionTimer_ >= kTakeoffDuration || skip))
		{
			transitionRequested_ = true;
			if (RaidStats::GetInstance().autoDemoMode && nextScene_ == "GAMEPLAY")
			{
				// 1周目の即死・救難ジオラマ実演が完了したら、次の出撃は必ず「2周目：全制圧＆脱出クリア実演」へ進める
				RaidStats::GetInstance().autoDemoCycleIndex = 1;
			}
			bgmLoop_.Stop();
			rotorLoop_.Stop();
			SceneTransition::Change(nextScene_, kSceneTransitionDuration);
		}
		break;
	}
	}

	// シームレスな救難鎮魂BGM ＆ ヘリローター音ループ更新
	const float bgmVol = transitionRequested_ ? 0.0f : kBgmVolume;
	bgmLoop_.Update("Resources/Audio/BGM_GameOver.wav", kBgmLoopEndSec, bgmVol, deltaTime);

	float rotorVol = kRotorVolApproaching;
	if (state_ == State::Loading)
	{
		rotorVol = kRotorVolLoading;
	}
	else if (state_ == State::Ready)
	{
		rotorVol = kRotorVolReady;
	}
	else if (state_ == State::Takeoff)
	{
		const float u = std::clamp(actionTimer_ / kTakeoffDuration, 0.0f, 1.0f);
		rotorVol = kRotorVolTakeoffPeak * (1.0f - u * kRotorVolTakeoffFade);
	}
	if (transitionRequested_)
	{
		rotorVol = 0.0f;
	}
	rotorLoop_.Update("Resources/Audio/SE_Heli_Rotor_Loop.wav", kRotorLoopEndSec, rotorVol, deltaTime);

	UpdateDiorama(deltaTime);
	DrawScreen();
}

void GameOverScene::UpdateDiorama(float deltaTime)
{
	// -------------------------------------------------------------------------
	// 1. 救難ヘリコプターのアニメーション
	// -------------------------------------------------------------------------
	float mainRotorSpeed = kMainRotorSpeedApproaching;
	float tailRotorSpeed = kTailRotorSpeedApproaching;

	if (state_ == State::Loading)
	{
		mainRotorSpeed = kMainRotorSpeedLoading;
		tailRotorSpeed = kTailRotorSpeedLoading;
	}
	else if (state_ == State::Ready)
	{
		mainRotorSpeed = kMainRotorSpeedReady;
		tailRotorSpeed = kTailRotorSpeedReady;
	}
	else if (state_ == State::Takeoff)
	{
		const float t = std::clamp(actionTimer_ / kTakeoffDuration, 0.0f, 1.0f);
		const float spoolT = std::clamp(actionTimer_ / kTakeoffSpoolDuration, 0.0f, 1.0f);
		const float spoolEase = EaseOutQuad(spoolT);
		mainRotorSpeed = kMainRotorSpeedReady + spoolEase * kMainRotorTakeoffBoost;
		tailRotorSpeed = kTailRotorSpeedReady + spoolEase * kTailRotorTakeoffBoost;

		if (t > kTakeoffAscendStartRatio)
		{
			const float liftT = (t - kTakeoffAscendStartRatio) / kTakeoffAscendSpanRatio;
			const float ascend = EaseInQuad(liftT);
			const float liftCurve = std::pow(liftT, kTakeoffLiftCurveExp);
			heliPos_.y = kInitialHeliPos.y + ascend * kTakeoffLiftOffset.y;
			heliPos_.x = kInitialHeliPos.x + liftCurve * kTakeoffLiftOffset.x;
			heliPos_.z = kInitialHeliPos.z + liftCurve * kTakeoffLiftOffset.z;
			heliRot_.x = kTakeoffPitchAmp * std::sin(liftT * kPi * kTakeoffPitchFreqScale);
			heliRot_.z = kTakeoffRollAmp * std::sin(liftT * kPi * kTakeoffRollFreqScale);
		}
	}

	mainRotorAngle_ += deltaTime * mainRotorSpeed;
	tailRotorAngle_ += deltaTime * tailRotorSpeed;

	// ミリタリー・ハロゲン投光器の重厚な微細ゆらぎ演出
	if (SceneManager::GetInstance() && SceneManager::GetInstance()->GetLight())
	{
		Light* light = SceneManager::GetInstance()->GetLight();
		const float pulse = kPointLightPulseBase + std::sin(timer_ * kPointLightPulseFreq) * kPointLightPulseAmp;
		light->SetPointLight(
			kPointLightPulseColor, kPointLightPosition, pulse, kPointLightRadius, kPointLightDecay);
	}

	if (heliBody_)
	{
		heliBody_->SetTranslate(heliPos_);
		heliBody_->SetRotate(heliRot_);
		heliBody_->Update();
	}

	// ヘリコプター回転行列を1フレームにつき1回だけ構築して全パーツ・搭乗員で共有
	const Matrix4x4 heliRotMat = Multiply(
		MakeRotateXMatrix(heliRot_.x),
		Multiply(MakeRotateYMatrix(heliRot_.y), MakeRotateZMatrix(heliRot_.z)));

	auto LocalToHeliWorld = [&](const Vector3& localOffset) -> Vector3 {
		return heliPos_ + TransformOffsetByRotation(localOffset * kHeliScale, heliRotMat);
	};

	auto WorldToHeliLocal = [&](const Vector3& worldPos) -> Vector3 {
		const Vector3 d = worldPos - heliPos_;
		return {
			(d.x * heliRotMat.m[0][0] + d.y * heliRotMat.m[0][1] + d.z * heliRotMat.m[0][2]) / kHeliScale,
			(d.x * heliRotMat.m[1][0] + d.y * heliRotMat.m[1][1] + d.z * heliRotMat.m[1][2]) / kHeliScale,
			(d.x * heliRotMat.m[2][0] + d.y * heliRotMat.m[2][1] + d.z * heliRotMat.m[2][2]) / kHeliScale
		};
	};

	if (heliMainRotor_)
	{
		const Vector3 mastWorldPos = LocalToHeliWorld(kMastLocalOffset);
		heliMainRotor_->SetTranslate(mastWorldPos);
		heliMainRotor_->SetRotate({ heliRot_.x, heliRot_.y + mainRotorAngle_, heliRot_.z });
		heliMainRotor_->Update();

		float blurOffset = 0.0f;
		if (state_ == State::Takeoff)
		{
			const float spoolT = std::clamp(actionTimer_ / kTakeoffSpoolDuration, 0.0f, 1.0f);
			blurOffset = EaseOutQuad(spoolT) * kRotorBlurMaxOffset;
		}

		if (heliMainRotorBlur1_)
		{
			heliMainRotorBlur1_->SetTranslate(mastWorldPos);
			heliMainRotorBlur1_->SetRotate({ heliRot_.x, heliRot_.y + mainRotorAngle_ - blurOffset, heliRot_.z });
			heliMainRotorBlur1_->Update();
		}
		if (heliMainRotorBlur2_)
		{
			heliMainRotorBlur2_->SetTranslate(mastWorldPos);
			heliMainRotorBlur2_->SetRotate({ heliRot_.x, heliRot_.y + mainRotorAngle_ - blurOffset * 2.0f, heliRot_.z });
			heliMainRotorBlur2_->Update();
		}
	}

	if (heliTailRotor_)
	{
		heliTailRotor_->SetTranslate(LocalToHeliWorld(kTailRotorLocalOffset));
		heliTailRotor_->SetRotate({ heliRot_.x + tailRotorAngle_, heliRot_.y, heliRot_.z });
		heliTailRotor_->Update();
	}

	// -------------------------------------------------------------------------
	// 2. スライドカーゴドア ＆ 担架チームのアニメーション
	// -------------------------------------------------------------------------
	// スライドカーゴドアの開閉アニメーション（SPACE押下による離陸開始で速やかに密閉）
	if (state_ == State::Takeoff)
	{
		const float doorT = std::clamp(actionTimer_ / kDoorCloseDuration, 0.0f, 1.0f);
		doorSlideProgress_ = EaseInOutCubic(doorT);
	}
	else
	{
		doorSlideProgress_ = 0.0f;
	}

	if (heliCargoDoor_)
	{
		const float curX = kDoorOpenLocalX - kDoorSlideDeltaX * doorSlideProgress_;
		const float curZ = kDoorOpenLocalZ + (kDoorClosedLocalZ - kDoorOpenLocalZ) * doorSlideProgress_;
		heliCargoDoor_->SetTranslate(LocalToHeliWorld({ curX, kDoorLocalY, curZ }));
		heliCargoDoor_->SetRotate(heliRot_);
		heliCargoDoor_->SetScale(kHeliScaleVec);
		heliCargoDoor_->Update();
	}

	auto SetAndUpdateModel = [](Object3d* model, const Vector3& pos, const Vector3& rot) {
		if (!model) return;
		model->SetTranslate(pos);
		model->SetRotate(rot);
		model->Update();
	};

	auto NormalizeAngleDelta = [](float target, float base) -> float {
		float d = std::fmod(target - base + kPi, 2.0f * kPi);
		if (d < 0.0f) d += 2.0f * kPi;
		return d - kPi;
	};

	// 担架ベッドのローカル座標（スケール kHeliScale 前）をワールド座標へ変換するヘルパー
	auto StretcherLocalToWorld = [](const Vector3& bedPos, const Vector3& bedRot, const Vector3& localUnscaled) -> Vector3 {
		const Matrix4x4 Rb = Multiply(MakeRotateXMatrix(bedRot.x), Multiply(MakeRotateYMatrix(bedRot.y), MakeRotateZMatrix(bedRot.z)));
		return bedPos + TransformOffsetByRotation(localUnscaled * kHeliScale, Rb);
	};

	// 負傷アヒルを担架と完全一体化した剛体として配置（頭部が必ず -X 側の救急枕に乗り、顔・くちばしがカメラ＆開放ドア側を向く）
	auto UpdateWoundedOnStretcher = [&](const Vector3& bedPos, const Vector3& bedRot, float breathOffset) {
		if (!duckWounded_) return;

		const Matrix4x4 Rb = Multiply(MakeRotateXMatrix(bedRot.x), Multiply(MakeRotateYMatrix(bedRot.y), MakeRotateZMatrix(bedRot.z)));
		const Matrix4x4 Rlocal = Multiply(MakeRotateYMatrix(kWoundedLocalRotY), Multiply(MakeRotateXMatrix(kWoundedLocalRotX), MakeRotateYMatrix(kHalfPi)));
		const Matrix4x4 Mw = Multiply(Rlocal, Rb);

		const float wy = std::asin(std::clamp(-Mw.m[0][2], -1.0f, 1.0f));
		const float wx = std::atan2(Mw.m[1][2], Mw.m[2][2]);
		const float wz = std::atan2(Mw.m[0][1], Mw.m[0][0]);

		const Vector3 woundedPos = StretcherLocalToWorld(bedPos, bedRot, kWoundedOnBedLocal) + Vector3{ 0.0f, breathOffset, 0.0f };
		SetAndUpdateModel(duckWounded_.get(), woundedPos, { wx, wy, wz });
	};

	// helicopter_body.obj 実寸に基づき、アヒル足元がスキッドとドア敷居を確実に跨いでキャビン床へ着地するヘリローカルY座標を算出
	auto CalcSafeMedicFootYHeli = [](float xHeli, float groundYHeli) -> float {
		if (xHeli >= kSafeFootGroundX) return groundYHeli;
		if (xHeli > kSafeFootStepUpX)
		{
			const float u = EaseInOutCubic((kSafeFootGroundX - xHeli) / (kSafeFootGroundX - kSafeFootStepUpX));
			return groundYHeli + (kSafeFootClearHeightY - groundYHeli) * u;
		}
		if (xHeli >= kSafeFootStepDownX)
		{
			return kSafeFootClearHeightY;
		}
		if (xHeli > kSafeFootCabinX)
		{
			const float u = EaseInOutCubic((kSafeFootStepDownX - xHeli) / (kSafeFootStepDownX - kSafeFootCabinX));
			return kSafeFootClearHeightY + (kSafeFootCabinFloorY - kSafeFootClearHeightY) * u;
		}
		return kSafeFootCabinFloorY;
	};

	// 開放ドアの中央安全窓を地上停止地点に設定
	Vector3 teamStopPos = LocalToHeliWorld(kTeamStopHeliLocal);
	teamStopPos.y = 0.0f;

	const Vector3 stretcherInsideRot = { heliRot_.x, heliRot_.y - kHalfPi, heliRot_.z };
	const Vector3 medicInsideRot     = { heliRot_.x, heliRot_.y + kHalfPi, heliRot_.z };
	const float breath = std::sin(timer_ * kBreathFreq) * kBreathAmp;

	if (state_ == State::Approaching)
	{
		walkPhase_ += deltaTime * kWalkPhaseSpeed;
		if (!IsAnyTransitionActive() && !transitionRequested_)
		{
			const int stepIdx = static_cast<int>(walkPhase_ / kPi);
			if (stepIdx != prevWalkStep_)
			{
				prevWalkStep_ = stepIdx;
				TacticalAudio::PlayRandom2D(
					{ "Resources/Audio/SE_Step_Dirt_1.wav", "Resources/Audio/SE_Step_Dirt_2.wav" },
					kWalkStepVolume, kWalkStepCooldownMs, "gameover_scene_step");
			}
		}

		const float t = std::clamp(actionTimer_ / kApproachDuration, 0.0f, 1.0f);
		const float progress = EaseOutQuad(t);
		teamPos_ = LerpVec3(teamStartPos_, teamStopPos, progress);

		const float strideFade = 1.0f - progress * kStrideFadeScale;
		const float waddle = std::sin(walkPhase_) * kWalkWaddleAmp * strideFade;
		const float bob = std::fabs(std::sin(walkPhase_)) * kWalkBobAmp * strideFade;

		const Vector3 bedPos = teamPos_ + Vector3{ 0.0f, kStretcherCarryHeightY + bob * 0.5f, 0.0f };
		const Vector3 bedRot = { 0.0f, 0.0f, waddle * kBedWaddleRollScale };

		// 後方衛生兵（枕側のハンドルを保持）・前方衛生兵（足元側のハンドルを保持）
		SetAndUpdateModel(duckRearMedic_.get(), teamPos_ + Vector3{ -kMedicCarryOffsetX, bob, 0.0f }, { 0.0f, kHalfPi, waddle });
		SetAndUpdateModel(duckFrontMedic_.get(), teamPos_ + Vector3{ kMedicCarryOffsetX, bob, 0.0f }, { 0.0f, kHalfPi, -waddle });
		// 担架ベッド ＆ 枕に頭を乗せた負傷主役アヒル
		SetAndUpdateModel(stretcherBed_.get(), bedPos, bedRot);
		UpdateWoundedOnStretcher(bedPos, bedRot, breath);
	}
	else if (state_ == State::Loading)
	{
		const float t = std::clamp(actionTimer_ / kLoadDuration, 0.0f, 1.0f);

		// Approaching 終了時の正確なヘリローカル座標（フレーム境界での位置ジャンプ 0.00m を保証）
		const Vector3 bedStartHeli   = WorldToHeliLocal(teamStopPos + Vector3{  0.0f, kStretcherCarryHeightY, 0.0f });
		const Vector3 frontStartHeli = WorldToHeliLocal(teamStopPos + Vector3{  kMedicCarryOffsetX, 0.0f, 0.0f });
		const Vector3 rearStartHeli  = WorldToHeliLocal(teamStopPos + Vector3{ -kMedicCarryOffsetX, 0.0f, 0.0f });

		// 1. 前方衛生兵：スキッドとドア敷居を飛び越えて開口部中央前方から機内奥へ先導搭乗
		if (duckFrontMedic_)
		{
			const float fMove = EaseInOutCubic(std::clamp(t / kFrontMedicMoveEnd, 0.0f, 1.0f));
			Vector3 fHeli = LerpVec3(frontStartHeli, kFrontMedicCabinLocal, fMove);
			if (fHeli.x > kDoorSafeClampXMin && fHeli.x < kDoorSafeClampXMax)
			{
				fHeli.z = std::clamp(fHeli.z, kDoorSafeClampZMin, kDoorSafeClampZMax);
			}
			fHeli.y = CalcSafeMedicFootYHeli(fHeli.x, frontStartHeli.y);
			const float hopF = std::fabs(std::sin(t * kPi * kFrontMedicHopFreq)) * kMedicHopAmp * (1.0f - fMove);
			fHeli.y += hopF;

			const float turnF = EaseInOutCubic(std::clamp((t - kFrontMedicTurnStart) / kFrontMedicTurnSpan, 0.0f, 1.0f));
			const float entryYaw = heliRot_.y - kHalfPi;
			const float startYaw = kHalfPi;
			const float midYaw = startYaw + NormalizeAngleDelta(entryYaw, startYaw) * EaseOutQuad(std::clamp(t / kFrontMedicTurnStart, 0.0f, 1.0f));
			const float frontYaw = midYaw + NormalizeAngleDelta(medicInsideRot.y, midYaw) * turnF;
			const float frontWaddle = std::sin(t * kPi * kMedicLoadWaddleFreq) * kMedicLoadWaddleAmp * (1.0f - fMove);

			SetAndUpdateModel(duckFrontMedic_.get(), LocalToHeliWorld(fHeli), { medicInsideRot.x * turnF, frontYaw, frontWaddle });
		}

		// 2. 担架ベッド ＆ 負傷アヒル：スキッド手前で開口部と平行へ向きを揃えてから敷居越え高度で滑らかに機内へスライド
		{
			const float bedTurnT  = EaseInOutCubic(std::clamp(t / kBedTurnEnd, 0.0f, 1.0f));
			const float bedSlideT = EaseInOutCubic(std::clamp((t - kBedSlideStart) / kBedSlideSpan, 0.0f, 1.0f));

			Vector3 bedHeli = LerpVec3(bedStartHeli, kStretcherCabinLocal, bedSlideT);
			const float sillLift = std::sin(bedSlideT * kPi) * kBedSillLiftArc;
			bedHeli.y += sillLift;
			if (bedHeli.x > kBedSillGuardXMin && bedHeli.x < kBedSillGuardXMax)
			{
				bedHeli.y = (std::max)(bedHeli.y, kBedSillMinHeightY);
			}

			const Vector3 bedPos = LocalToHeliWorld(bedHeli);
			const Vector3 bedRot = {
				stretcherInsideRot.x * bedTurnT,
				NormalizeAngleDelta(stretcherInsideRot.y, 0.0f) * bedTurnT,
				stretcherInsideRot.z * bedTurnT
			};

			SetAndUpdateModel(stretcherBed_.get(), bedPos, bedRot);
			UpdateWoundedOnStretcher(bedPos, bedRot, breath);
		}

		// 3. 後方衛生兵：開口部中央寄りを通って担架を機内へ押し込み、キャビン内部へ入ってから枕元奥へ回り込む
		if (duckRearMedic_)
		{
			Vector3 rHeli{};
			if (t < kRearBoardSplitT)
			{
				const float rBoard = EaseInOutCubic(std::clamp((t - kRearBoardStart) / kRearBoardSpan, 0.0f, 1.0f));
				rHeli = LerpVec3(rearStartHeli, kRearDoorwayInside, rBoard);
			}
			else
			{
				const float rSeat = EaseInOutCubic(std::clamp((t - kRearBoardSplitT) / kRearSeatSpan, 0.0f, 1.0f));
				rHeli = LerpVec3(kRearDoorwayInside, kRearMedicCabinLocal, rSeat);
			}

			if (rHeli.x > kDoorSafeClampXMin && rHeli.x < kDoorSafeClampXMax)
			{
				rHeli.z = std::clamp(rHeli.z, kDoorSafeClampZMin, kDoorSafeClampZMax);
			}
			rHeli.y = CalcSafeMedicFootYHeli(rHeli.x, rearStartHeli.y);
			const float hopR = std::fabs(std::sin(t * kPi * kRearMedicHopFreq + kRearMedicHopPhase)) * kMedicHopAmp * (1.0f - EaseInOutCubic(t));
			rHeli.y += hopR;

			const float pushDirYaw = heliRot_.y - kPi;
			const float startYaw = kHalfPi;
			const float approachYaw = startYaw + NormalizeAngleDelta(pushDirYaw, startYaw) * EaseOutQuad(std::clamp(t / kRearApproachTurnEnd, 0.0f, 1.0f));
			const float faceDoorBlend = EaseInOutCubic(std::clamp((t - kRearFaceDoorStart) / kRearFaceDoorSpan, 0.0f, 1.0f));
			const float rearYaw = approachYaw + NormalizeAngleDelta(medicInsideRot.y, approachYaw) * faceDoorBlend;
			const float rearWaddle = -std::sin(t * kPi * kMedicLoadWaddleFreq) * kMedicLoadWaddleAmp * (1.0f - faceDoorBlend);

			SetAndUpdateModel(duckRearMedic_.get(), LocalToHeliWorld(rHeli), { medicInsideRot.x * faceDoorBlend, rearYaw, rearWaddle });
		}
	}
	else // State::Ready または State::Takeoff（全員ヘリ機内に搭乗し、離陸時の機体ピッチ＆ロールとも完全剛体同期）
	{
		const Vector3 cabinBedWorld = LocalToHeliWorld(kStretcherCabinLocal);
		SetAndUpdateModel(stretcherBed_.get(), cabinBedWorld, stretcherInsideRot);
		UpdateWoundedOnStretcher(cabinBedWorld, stretcherInsideRot, breath);
		SetAndUpdateModel(duckFrontMedic_.get(), LocalToHeliWorld(kFrontMedicCabinLocal), medicInsideRot);
		SetAndUpdateModel(duckRearMedic_.get(), LocalToHeliWorld(kRearMedicCabinLocal), medicInsideRot);
	}

	// -------------------------------------------------------------------------
	// 3. 地形・ヘリパッド・遠景樹木の更新
	// -------------------------------------------------------------------------
	if (groundModel_) groundModel_->Update();
	if (helipadModel_) helipadModel_->Update();
	for (auto& prop : props_)
	{
		if (prop.model) prop.model->Update();
	}

	// -------------------------------------------------------------------------
	// 4. 映画的カメラのリアルタイム追従
	// -------------------------------------------------------------------------
	if (camera_)
	{
		Vector3 camPos = kBaseCameraPos;
		Vector3 camRot = kBaseCameraRot;

		if (state_ == State::Takeoff)
		{
			const float t = std::clamp(actionTimer_ / kTakeoffDuration, 0.0f, 1.0f);
			if (t > kTakeoffAscendStartRatio)
			{
				const float lookUpT = (t - kTakeoffAscendStartRatio) / kTakeoffAscendSpanRatio;
				camRot.x -= lookUpT * kCamLookUpPitchDelta;
				camRot.y += lookUpT * kCamLookUpYawDelta;
				camPos.y += lookUpT * kCamLookUpLiftY;
			}

			const float shakeFactor = 1.0f - t * kCamShakeDecayScale;
			camPos.x += std::sin(actionTimer_ * kCamShakeFreqX) * kCamShakeAmpX * shakeFactor;
			camPos.y += std::cos(actionTimer_ * kCamShakeFreqY) * kCamShakeAmpY * shakeFactor;
		}

		camera_->SetTranslate(camPos);
		camera_->SetRotate(camRot);
		camera_->SetFovY(kBaseCameraFovY);
		camera_->Update();
	}

	// -------------------------------------------------------------------------
	// 4. 夜間救難ジオラマ 3層パーティクル演出（霧雨ストリーク・投光器浮遊ダスト・ローター夜霧）
	// -------------------------------------------------------------------------
	auto rand01 = []() -> float { return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX); };
	auto randRange = [&](float a, float b) -> float { return a + (b - a) * rand01(); };
	constexpr float kTwoPi = kPi * 2.0f;

	if (appParticleManager_)
	{
		// (A) 黄昏の夜空から斜めに降り注ぐ冷たい霧雨ストリーク ＆ 地面水しぶきバウンド
		rainEmitTimer_ += deltaTime;
		while (rainEmitTimer_ >= 0.022f)
		{
			rainEmitTimer_ -= 0.022f;
			for (int i = 0; i < 4; ++i)
			{
				AppParticle rain{};
				rain.transform.Initialize();
				rain.transform.SetTranslate({ randRange(-8.5f, 9.5f), randRange(5.2f, 7.6f), randRange(-4.5f, 8.5f) });
				rain.scaleAspect = { 0.14f, 3.35f };
				rain.initialScale = randRange(0.055f, 0.088f);
				rain.targetScale = rain.initialScale * 0.72f;
				rain.transform.SetScale({ rain.initialScale * rain.scaleAspect.x, rain.initialScale * rain.scaleAspect.y, 1.0f });
				rain.velocity = { randRange(-2.6f, -1.8f), randRange(-16.5f, -12.8f), randRange(-0.8f, 0.4f) };
				rain.color = { 0.62f, 0.80f, 1.0f, randRange(0.28f, 0.48f) };
				rain.endColor = { 0.45f, 0.68f, 0.95f, 0.0f };
				rain.hasColorShift = true;
				rain.bounceElasticity = 0.14f;
				rain.lifeTime = 0.52f;
				rain.textureIndex = rainStreakTexIndex_;
				rain.slotLayer = 1;
				appParticleManager_->AddParticle(rain);
			}
		}

		// (B) ヘリのハロゲン救難投光器ビーム内に浮かび上がる暖かい黄金ボリューメトリック・ダスト
		lightMoteTimer_ += deltaTime;
		if (lightMoteTimer_ >= 0.055f)
		{
			lightMoteTimer_ = std::fmod(lightMoteTimer_, 0.055f);
			for (int m = 0; m < 2; ++m)
			{
				const float a = randRange(0.0f, kTwoPi);
				AppParticle mote{};
				mote.transform.Initialize();
				mote.transform.SetTranslate({
					randRange(-2.8f, 4.2f),
					randRange(0.28f, 2.65f),
					randRange(-1.2f, 3.6f)
				});
				mote.scaleAspect = { 0.92f, 1.15f };
				mote.initialScale = randRange(0.032f, 0.058f);
				mote.targetScale = mote.initialScale * 0.35f;
				mote.transform.SetScale({ mote.initialScale * mote.scaleAspect.x, mote.initialScale * mote.scaleAspect.y, 1.0f });
				mote.velocity = { randRange(-0.28f, 0.28f), randRange(0.18f, 0.55f), randRange(-0.28f, 0.28f) };
				mote.color = { 1.0f, 0.84f, 0.46f, randRange(0.55f, 0.85f) };
				mote.endColor = { 1.0f, 0.58f, 0.18f, 0.0f };
				mote.hasColorShift = true;
				mote.drag = 1.2f;
				mote.curlFreq = 2.4f;
				mote.curlAmp = 0.20f;
				mote.lifeTime = randRange(0.95f, 1.55f);
				mote.fadeInRatio = 0.22f;
				mote.twinkleSpeed = 14.0f;
				mote.twinklePhase = a;
				mote.textureIndex = moteCircleTexIndex_;
				mote.slotLayer = 2;
				appParticleManager_->AddParticle(mote);
			}
		}

		appParticleManager_->ApplyRotorDownwash(
			heliPos_ + Vector3{ 0.0f, 1.85f, 0.0f }, 5.4f,
			(state_ == State::Takeoff) ? 13.5f : 4.2f, deltaTime);
		appParticleManager_->Update(deltaTime, teamPos_);
	}

	if (smokeParticleManager_)
	{
		// (C) ヘリパッド地表を渦巻きながら吹き広がるローター吹き下ろし夜霧（離陸時に大迫力で爆散！）
		const float mistInterval = (state_ == State::Takeoff) ? 0.024f : 0.052f;
		rotorMistTimer_ += deltaTime;
		while (rotorMistTimer_ >= mistInterval)
		{
			rotorMistTimer_ -= mistInterval;
			const int burst = (state_ == State::Takeoff) ? 3 : 2;
			for (int k = 0; k < burst; ++k)
			{
				const float a = randRange(0.0f, kTwoPi);
				const float r = randRange(0.65f, 2.45f);
				const float outSpd = (state_ == State::Takeoff) ? randRange(3.6f, 6.8f) : randRange(1.25f, 2.65f);
				AppParticle mist{};
				mist.transform.Initialize();
				mist.transform.SetTranslate({ kHelipadPos.x + std::cos(a) * r, randRange(0.05f, 0.24f), kHelipadPos.z + std::sin(a) * r });
				mist.transform.SetRotate({ 0.0f, 0.0f, a });
				mist.scaleAspect = { 1.65f, 0.72f };
				mist.initialScale = randRange(0.28f, 0.46f);
				mist.targetScale = mist.initialScale * ((state_ == State::Takeoff) ? 2.95f : 2.35f);
				mist.transform.SetScale({ mist.initialScale * mist.scaleAspect.x, mist.initialScale * mist.scaleAspect.y, 1.0f });
				mist.velocity = {
					std::cos(a) * outSpd - std::sin(a) * 1.15f,
					randRange(0.12f, 0.52f),
					std::sin(a) * outSpd + std::cos(a) * 1.15f
				};
				mist.color = { 0.72f, 0.80f, 0.90f, (state_ == State::Takeoff) ? 0.34f : 0.22f };
				mist.endColor = { 0.60f, 0.70f, 0.82f, 0.0f };
				mist.hasColorShift = true;
				mist.drag = 2.1f;
				mist.curlFreq = 2.0f;
				mist.curlAmp = 0.25f;
				mist.lifeTime = randRange(0.70f, 1.15f);
				mist.fadeInRatio = 0.18f;
				mist.textureIndex = mistSmokeTexIndex_;
				mist.slotLayer = 0;
				smokeParticleManager_->AddParticle(mist);
			}
		}

		smokeParticleManager_->ApplyRotorDownwash(
			heliPos_ + Vector3{ 0.0f, 1.85f, 0.0f }, 5.8f,
			(state_ == State::Takeoff) ? 16.0f : 5.5f, deltaTime);
		smokeParticleManager_->Update(deltaTime, teamPos_);
	}
}

// -----------------------------------------------------------------------------
// 3D 描画（DirectX12 ネイティブ並列描画パイプライン）
// -----------------------------------------------------------------------------

void GameOverScene::Draw(SceneRenderRequests& renderRequests)
{
	DirectXCom* dx = GetDirectXCom();
	Object3dCom* com = GetObject3dCom();
	if (!dx || !com || !camera_) return;

	renderRequests.sceneDrawn = true;
	camera_->Update();

	RenderContext base{};
	base.commandList = dx->GetCommandList().Get();
	base.windowAPI = dx->GetWindowAPI();
	base.camera = camera_;
	base.light = SceneManager::GetInstance() ? SceneManager::GetInstance()->GetLight() : nullptr;
	if (MaterialManager* matMgr = GetMaterialManager(); matMgr && matMgr->GetMaterialResource())
	{
		base.materialGPUAddress = matMgr->GetMaterialResource()->GetGPUVirtualAddress();
	}

	// 1. 地面 & 脱出ヘリパッド
	DrawModel(com, groundModel_.get(), base, groundTexIndex_);
	DrawModel(com, helipadModel_.get(), base, helipadTexIndex_);

	// 2. 遠景樹木・小道具
	for (const auto& prop : props_)
	{
		DrawModel(com, prop.model.get(), base, prop.texIndex);
	}

	// 3. 担架チーム（救難衛生兵2羽 ＋ 担架ベッド ＋ 負傷したプレイヤー主役アヒル）
	DrawModel(com, stretcherBed_.get(), base, stretcherTexIndex_);
	DrawModel(com, duckRearMedic_.get(), base, duckMedicTexIndex_);
	DrawModel(com, duckFrontMedic_.get(), base, duckMedicTexIndex_);
	DrawModel(com, duckWounded_.get(), base, duckTexIndex_);

	// 4. 救難ヘリコプター
	DrawModel(com, heliBody_.get(), base, heliTexIndex_);
	DrawModel(com, heliCargoDoor_.get(), base, heliTexIndex_);
	if (state_ == State::Takeoff)
	{
		DrawModel(com, heliMainRotorBlur2_.get(), base, heliTexIndex_);
		DrawModel(com, heliMainRotorBlur1_.get(), base, heliTexIndex_);
	}
	DrawModel(com, heliMainRotor_.get(), base, heliTexIndex_);
	DrawModel(com, heliTailRotor_.get(), base, heliTexIndex_);

	// 5. 夜間救難ジオラマ GPUパーティクル描画（ストレートアルファ夜霧 ＆ 加算合成霧雨・投光器ダスト）
	if (smokeParticleManager_ && GetParticleManager())
	{
		RenderContext sCtx = base;
		if (mistSmokeTexIndex_ != UINT32_MAX)
		{
			sCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(mistSmokeTexIndex_);
		}
		smokeParticleManager_->Draw(sCtx);
	}
	if (appParticleManager_ && GetParticleManager())
	{
		RenderContext aCtx = base;
		if (moteCircleTexIndex_ != UINT32_MAX)
		{
			aCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(moteCircleTexIndex_);
		}
		appParticleManager_->Draw(aCtx);
	}
}

// -----------------------------------------------------------------------------
// 2D 映画的HUD ＆ 戦績レポート描画（TacticalUICanvas DirectX 12 ネイティブ描画）
// -----------------------------------------------------------------------------

void GameOverScene::DrawScreen()
{
	if (IsAnyTransitionActive()) return;

	TacticalUICanvas* ui = TacticalUICanvas::GetInstance();
	if (!ui) return;
	const UIVec2 disp = ui->GetDisplaySize();
	const float sw = disp.x;
	const float sh = disp.y;
	if (sw <= 0.0f || sh <= 0.0f) return;

	TacticalDrawList* dl = ui->GetBackgroundDrawList();
	if (!dl) return;
	const TacticalFont* fontDefault = ui->GetFont(UIFontRole::DefaultJP);
	const TacticalFont* fontTitle   = ui->GetFont(UIFontRole::TitleBoldJP);

	const float centerX = sw * 0.5f;
	const float enterT = std::clamp((timer_ - kHudEnterDelay) / kHudEnterDuration, 0.0f, 1.0f);

	// 0. 重厚なシネマティック・ビネット（周辺減光）
	if (vignetteTexIndex_ != UINT32_MAX)
	{
		const UITextureID vigID = static_cast<UITextureID>(
			TextureManager::GetInstance()->GetSrvHandleGPU(vignetteTexIndex_).ptr);
		dl->AddImage(vigID, UIVec2(0.0f, 0.0f), UIVec2(sw, sh), UIVec2(0, 0), UIVec2(1, 1),
			WithAlpha(kColVignetteTint, enterT));
	}

	// 1. シネマスコープ黒帯（上下の映画的レターボックス）
	const float barHeight = sh * kLetterboxHeightRatio;
	dl->AddRectFilled(UIVec2(0.0f, 0.0f), UIVec2(sw, barHeight), kColLetterboxFill);
	dl->AddRectFilled(UIVec2(0.0f, sh - barHeight), UIVec2(sw, sh), kColLetterboxFill);
	dl->AddLine(UIVec2(0.0f, barHeight), UIVec2(sw, barHeight), WithAlpha(kColLetterboxRule, enterT), kRuleThickness);
	dl->AddLine(UIVec2(0.0f, sh - barHeight), UIVec2(sw, sh - barHeight), WithAlpha(kColLetterboxRule, enterT), kRuleThickness);

	// 1B. 自動デモ稼働中：1周目救難ジオラマ実演 → 2周目クリア実演への自動出撃案内を上帯に表示
	if (RaidStats::GetInstance().autoDemoMode)
	{
		const char* demoNotice = reinterpret_cast<const char*>(
			u8"自動デモ [1/2周目：夜間救難パーティクル実演]  ──  まもなく 2/2周目（ステージクリア実演）へ自動出撃します  [F7] 手動操作");
		AddCenteredText(dl, fontDefault, 15.0f, centerX, barHeight * 0.5f - 7.5f,
			UI_COL32(255, 222, 130, 238), demoNotice);
	}

	// 2. 画面中央：映画タイトル調の重厚かつ静寂なタイトル
	{
		const float fadeAlpha = EaseInOutCubic(enterT);
		const float titleY = sh * kTitlePosYRatio;
		const char* titleText = reinterpret_cast<const char*>(u8"作　戦　失　敗");

		AddCenteredText(dl, fontTitle, kTitleFontSize, centerX, titleY + kTitleShadowDropY,
			WithAlpha(kColTitleShadowDeep, fadeAlpha), titleText);
		AddCenteredText(dl, fontTitle, kTitleFontSize, centerX + kTitleShadowDiagonal, titleY + kTitleShadowDiagonal,
			WithAlpha(kColTitleShadowSide, fadeAlpha), titleText);
		AddCenteredText(dl, fontTitle, kTitleFontSize, centerX - kTitleShadowDiagonal, titleY + kTitleShadowDiagonal,
			WithAlpha(kColTitleShadowSide, fadeAlpha), titleText);
		AddCenteredText(dl, fontTitle, kTitleFontSize, centerX, titleY,
			WithAlpha(kColTitleMain, fadeAlpha), titleText);

		dl->AddLine(
			UIVec2(centerX - kCrimsonRuleHalfWidth, titleY + kCrimsonRuleOffsetY),
			UIVec2(centerX + kCrimsonRuleHalfWidth, titleY + kCrimsonRuleOffsetY),
			WithAlpha(kColCrimsonRule, fadeAlpha), kRuleThickness);

		const char* subStatus = reinterpret_cast<const char*>(u8"緊　急　救　難　・　後　方　搬　送");
		AddCenteredText(dl, fontDefault, kSubStatusFontSize, centerX, titleY + kSubStatusOffsetY,
			WithAlpha(kColSubStatus, fadeAlpha), subStatus);
	}

	// 3. 下部シネマスコープ黒帯：静かで控えめな操作案内
	const float promptY = sh - barHeight * 0.5f - kPromptOffsetUpY;
	switch (state_)
	{
	case State::Approaching:
	{
		const float blink = kApproachBlinkBase + kApproachBlinkAmp * std::sin(timer_ * kApproachBlinkFreq);
		const char* promptText = reinterpret_cast<const char*>(u8"SPACE : 再出撃         T : タイトル画面へ");
		AddCenteredText(dl, fontDefault, kPromptFontSize, centerX, promptY,
			WithAlpha(kColPromptApproaching, enterT * blink), promptText);
		break;
	}
	case State::Loading:
	{
		const char* loadMsg = reinterpret_cast<const char*>(u8"負傷兵を救難ヘリへ収容中...");
		AddCenteredText(dl, fontDefault, kPromptFontSize, centerX, promptY, kColPromptLoading, loadMsg);
		break;
	}
	case State::Ready:
	{
		const float blink = kReadyBlinkBase + kReadyBlinkAmp * std::sin(timer_ * kReadyBlinkFreq);
		const char* readyMsg = reinterpret_cast<const char*>(u8"収容完了  |  SPACE : 離陸して再出撃         T : タイトル画面へ");
		AddCenteredText(dl, fontDefault, kPromptFontSize, centerX, promptY,
			WithAlpha(kColPromptReady, enterT * blink), readyMsg);
		break;
	}
	case State::Takeoff:
	{
		static constexpr const char* kDots[kTakeoffDotsCount] = { "", ".", "..", "..." };
		const int dotIndex = static_cast<int>(actionTimer_ * kTakeoffDotsSpeed) % kTakeoffDotsCount;
		char buf[kPromptBufferSize];
		if (actionTimer_ < kDoorCloseDuration)
		{
			sprintf_s(buf, reinterpret_cast<const char*>(u8"カーゴドア閉鎖中... %d%%"),
				static_cast<int>(doorSlideProgress_ * kPercentScale));
		}
		else
		{
			sprintf_s(buf, reinterpret_cast<const char*>(u8"救難ヘリ離陸中  前線から緊急脱出%s"), kDots[dotIndex]);
		}
		AddCenteredText(dl, fontDefault, kPromptFontSize, centerX, promptY, kColPromptTakeoff, buf);
		break;
	}
	}

	// 4. 離陸時の退場フェード
	if (state_ == State::Takeoff)
	{
		const float fadeT = std::clamp(
			(actionTimer_ - kTakeoffDuration * kExitFadeStartRatio) / (kTakeoffDuration * kExitFadeSpanRatio),
			0.0f, 1.0f);
		if (fadeT > 0.0f)
		{
			dl->AddRectFilled(UIVec2(0.0f, 0.0f), UIVec2(sw, sh), WithAlpha(kColLetterboxFill, fadeT));
		}
	}
}
