#include "GameOverScene.h"
#include "GameOverSceneTuning.h"
#include "KeyInput.h"
#include "DirectXCom.h"
#include "SceneManager.h"
#include "TextureManager.h"
#include "Object3dCom.h"
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
	}

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

	const Vector3 stretcherInsideRot = { heliRot_.x, heliRot_.y + kHalfPi, heliRot_.z };
	const Vector3 woundedInsideRot   = { heliRot_.x + kWoundedPitchTilt, heliRot_.y + kHalfPi, heliRot_.z + kWoundedSupineRoll };
	const Vector3 medicInsideRot     = { heliRot_.x, heliRot_.y + kHalfPi, heliRot_.z };
	const float breath = std::sin(timer_ * kBreathFreq) * kBreathAmp;

	auto SetAndUpdateModel = [](Object3d* model, const Vector3& pos, const Vector3& rot) {
		if (!model) return;
		model->SetTranslate(pos);
		model->SetRotate(rot);
		model->Update();
	};

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
		teamPos_ = LerpVec3(teamStartPos_, kApproachTargetPos, EaseOutQuad(t));

		const float waddle = std::sin(walkPhase_) * kWalkWaddleAmp;
		const float bob = std::fabs(std::sin(walkPhase_)) * kWalkBobAmp;

		SetAndUpdateModel(duckRearMedic_.get(), teamPos_ + Vector3{ -kMedicCarryOffsetX, bob, 0.0f }, { 0.0f, kHalfPi, waddle });
		SetAndUpdateModel(duckFrontMedic_.get(), teamPos_ + Vector3{ kMedicCarryOffsetX, bob, 0.0f }, { 0.0f, kHalfPi, -waddle });
		SetAndUpdateModel(stretcherBed_.get(), teamPos_ + Vector3{ 0.0f, kStretcherCarryHeightY + bob * 0.5f, 0.0f }, { 0.0f, 0.0f, 0.0f });
		SetAndUpdateModel(duckWounded_.get(), teamPos_ + Vector3{ 0.0f, kWoundedCarryHeightY + bob * 0.5f + breath, 0.0f }, { kWoundedPitchTilt, 0.0f, kWoundedSupineRoll });
	}
	else if (state_ == State::Loading)
	{
		const float t = std::clamp(actionTimer_ / kLoadDuration, 0.0f, 1.0f);

		const Vector3 frontStart   = { kApproachTargetPos.x + kMedicCarryOffsetX, 0.0f, kApproachTargetPos.z };
		const Vector3 rearStart    = { kApproachTargetPos.x - kMedicCarryOffsetX, 0.0f, kApproachTargetPos.z };
		const Vector3 bedStart     = { kApproachTargetPos.x, kStretcherCarryHeightY, kApproachTargetPos.z };
		const Vector3 woundedStart = { kApproachTargetPos.x, kWoundedCarryHeightY, kApproachTargetPos.z };

		const float frontT = EaseInOutCubic(std::clamp(t / kFrontMedicLoadEndRatio, 0.0f, 1.0f));
		SetAndUpdateModel(
			duckFrontMedic_.get(),
			LerpVec3(frontStart, LocalToHeliWorld(kFrontMedicCabinLocal), frontT),
			LerpVec3({ 0.0f, kHalfPi, 0.0f }, medicInsideRot, frontT));

		const float bedT = EaseInOutCubic(std::clamp((t - kBedLoadStartRatio) / kBedLoadSpanRatio, 0.0f, 1.0f));
		const float liftArc = std::sin(bedT * kPi) * kBedLoadLiftArcHeight;
		SetAndUpdateModel(
			stretcherBed_.get(),
			LerpVec3(bedStart, LocalToHeliWorld(kStretcherCabinLocal), bedT) + Vector3{ 0.0f, liftArc, 0.0f },
			LerpVec3({ 0.0f, 0.0f, 0.0f }, stretcherInsideRot, bedT));

		SetAndUpdateModel(
			duckWounded_.get(),
			LerpVec3(woundedStart, LocalToHeliWorld(kWoundedCabinLocal), bedT) + Vector3{ 0.0f, liftArc + breath, 0.0f },
			LerpVec3({ kWoundedPitchTilt, 0.0f, kWoundedSupineRoll }, woundedInsideRot, bedT));

		const float rearT = EaseInOutCubic(std::clamp((t - kRearMedicLoadStartRatio) / kRearMedicLoadSpanRatio, 0.0f, 1.0f));
		SetAndUpdateModel(
			duckRearMedic_.get(),
			LerpVec3(rearStart, LocalToHeliWorld(kRearMedicCabinLocal), rearT),
			LerpVec3({ 0.0f, kHalfPi, 0.0f }, medicInsideRot, rearT));
	}
	else
	{
		SetAndUpdateModel(stretcherBed_.get(), LocalToHeliWorld(kStretcherCabinLocal), stretcherInsideRot);
		SetAndUpdateModel(duckWounded_.get(), LocalToHeliWorld(kWoundedCabinLocal) + Vector3{ 0.0f, breath, 0.0f }, woundedInsideRot);
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
