#include "HelicopterInsertionController.h"
#include "HelicopterInsertionTuning.h"
#include "GamePlayScene.h"
#include "SceneManager.h"
#include "SkyBox.h"
#include "Application/Config/GameConfig.h"
#include "Application/Particle/AppParticleManager.h"
#include "ParticleManager.h"
#include "TextureManager.h"
#include "Baziru3_Engine/Framework/Audio/AudioManager.h"
#include "Application/Transition/StingerTransition.h"
#include "Application/UI/TacticalUICanvas.h"
#include "../../Player/Player.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <span>

namespace
{
	using namespace HelicopterInsertionTuning;
	using GameConfig::Math::kHalfPi;
	using GameConfig::Math::kPi;
	using GameConfig::Math::kTwoPi;

	inline float Rand01() { return static_cast<float>(rand()) / static_cast<float>(RAND_MAX); }
	inline float RandRange(float a, float b) { return a + (b - a) * Rand01(); }
	inline float RandRangeVec(const Vector2& range) { return RandRange(range.x, range.y); }

	inline Vector3 RandBoxVec3(float halfX, const Vector2& yRange, float halfZ)
	{
		return { RandRange(-halfX, halfX), RandRangeVec(yRange), RandRange(-halfZ, halfZ) };
	}

	inline void AdvanceWrappedAngle(float& angle, float speed, float dt)
	{
		angle += speed * dt;
		if (angle > kTwoPi) angle -= kTwoPi;
	}

	inline float SmoothStep01(float x)
	{
		const float t = std::clamp(x, 0.0f, 1.0f);
		return t * t * (kSmoothStepLinear - 2.0f * t);
	}

	inline float EaseOutCubic(float x)
	{
		const float inv = 1.0f - std::clamp(x, 0.0f, 1.0f);
		return 1.0f - inv * inv * inv;
	}

	inline Vector3 LerpVec3(const Vector3& a, const Vector3& b, float t)
	{
		return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t };
	}

	// 任意の視点(eye)から注視点(target)を捉える正確なカメラ回転（Pitch, Yaw, Roll）を算出
	inline Vector3 CalcLookAtRotation(const Vector3& eye, const Vector3& target, float roll = 0.0f)
	{
		const Vector3 dir = { target.x - eye.x, target.y - eye.y, target.z - eye.z };
		const float xzLen = std::sqrt(dir.x * dir.x + dir.z * dir.z);
		return { std::atan2(-dir.y, (std::max)(xzLen, kLookAtMinXzLen)), std::atan2(dir.x, dir.z), roll };
	}

	// オイラー角から3D回転行列を生成し、ローカルオフセットを回転変換するヘルパー
	inline Matrix4x4 MakeEulerRotationMatrix(const Vector3& rot)
	{
		return Multiply(MakeRotateXMatrix(rot.x), Multiply(MakeRotateYMatrix(rot.y), MakeRotateZMatrix(rot.z)));
	}

	inline Vector3 RotateByMatrix(const Matrix4x4& r, const Vector3& v)
	{
		return {
			v.x * r.m[kAxisX][kAxisX] + v.y * r.m[kAxisY][kAxisX] + v.z * r.m[kAxisZ][kAxisX],
			v.x * r.m[kAxisX][kAxisY] + v.y * r.m[kAxisY][kAxisY] + v.z * r.m[kAxisZ][kAxisY],
			v.x * r.m[kAxisX][kAxisZ] + v.y * r.m[kAxisY][kAxisZ] + v.z * r.m[kAxisZ][kAxisZ]
		};
	}

	inline Vector3 LocalToWorldWithMatrix(const Vector3& origin, const Matrix4x4& r, const Vector3& localOff)
	{
		return origin + RotateByMatrix(r, localOff);
	}

	inline uint32_t PackColor(const RgbColor& rgb, int alpha)
	{
		return UI_COL32(rgb.r, rgb.g, rgb.b, std::clamp(alpha, kAlphaMin, kAlphaMax));
	}

	inline uint32_t ResolveTex(uint32_t preferred, uint32_t fallback)
	{
		return (preferred != TextureManager::kInvalidTextureIndex) ? preferred : fallback;
	}

	inline void PlayDuckVoiceOnce(bool& playedFlag, int soundId)
	{
		if (!playedFlag && soundId >= 0 && SceneManager::GetInstance() && SceneManager::GetInstance()->GetAudioManager())
		{
			SceneManager::GetInstance()->GetAudioManager()->PlayOneShot(soundId, kDuckVoiceVolume);
			playedFlag = true;
		}
	}

	void BuildBoxListModel(Object3d::ModelData& data, std::span<const UvBoxSpec> boxes, float boundingRadius)
	{
		data.vertices.clear();
		data.indices.clear();
		data.vertices.reserve(boxes.size() * kBoxFaceCount * kFaceVertCount);
		data.indices.reserve(boxes.size() * kBoxFaceCount * kFaceIndexCount);

		for (const UvBoxSpec& box : boxes)
		{
			const Vector3 half = box.size * 0.5f;
			Vector3 corners[kBoxCornerCount]{};
			for (int c = 0; c < kBoxCornerCount; ++c)
			{
				corners[c] = {
					box.center.x + half.x * kBoxCornerSigns[c].x,
					box.center.y + half.y * kBoxCornerSigns[c].y,
					box.center.z + half.z * kBoxCornerSigns[c].z
				};
			}

			for (int f = 0; f < kBoxFaceCount; ++f)
			{
				const uint32_t fBase = static_cast<uint32_t>(data.vertices.size());
				for (int v = 0; v < kFaceVertCount; ++v)
				{
					const Vector3& pt = corners[kBoxFaceVertices[f][v]];
					Sprite::VertexData vd{};
					vd.position = { pt.x, pt.y, pt.z, 1.0f };
					vd.texcoord = { kFaceUseUvMaxX[v] ? box.uvMax.x : box.uvMin.x, kFaceUseUvMaxY[v] ? box.uvMax.y : box.uvMin.y };
					vd.normal = kBoxFaceNormals[f];
					data.vertices.push_back(vd);
				}
				for (int idx = 0; idx < kFaceIndexCount; ++idx)
				{
					data.indices.push_back(fBase + kQuadTriIndices[idx]);
				}
			}
		}

		data.boundingRadius = boundingRadius;
	}

	// 地上シャドウ用モデルデータ生成（円盤クアッドメッシュ）
	void BuildShadowDiscModel(Object3d::ModelData& data, float rx, float rz)
	{
		data.vertices.clear();
		data.indices.clear();
		data.vertices.reserve(kShadowVertCount);
		data.indices.reserve(kShadowIndexCount);

		for (int v = 0; v < kShadowVertCount; ++v)
		{
			Sprite::VertexData vd{};
			vd.position = { rx * kShadowCornerSigns[v].x, 0.0f, rz * kShadowCornerSigns[v].y, 1.0f };
			vd.texcoord = kShadowCornerUVs[v];
			vd.normal = { 0.0f, 1.0f, 0.0f };
			data.vertices.push_back(vd);
		}
		for (int i = 0; i < kShadowIndexCount; ++i)
		{
			data.indices.push_back(kShadowTriIndices[i]);
		}
	}

	std::unique_ptr<Object3d> CreateModelObject(
		auto* object3dCom, Camera* camera, const Object3d::ModelData& modelData, const Vector4& color, bool enableLighting = true)
	{
		auto obj = std::make_unique<Object3d>();
		obj->Initialize(object3dCom, modelData);
		obj->SetCamera(camera);
		obj->SetColor(color);
		obj->SetEnableLighting(enableLighting);
		obj->SetAllowWireframeOverlay(false);
		return obj;
	}

	std::unique_ptr<Object3d> CreateSharedObject(auto* object3dCom, Camera* camera, Object3d* sharedSource, const Vector4& color)
	{
		auto obj = std::make_unique<Object3d>();
		obj->InitializeShared(object3dCom, sharedSource);
		obj->SetCamera(camera);
		obj->SetColor(color);
		obj->SetEnableLighting(true);
		obj->SetAllowWireframeOverlay(false);
		return obj;
	}

	std::unique_ptr<Object3d> CreateProceduralHeliPart(
		auto* object3dCom, Camera* camera, std::span<const UvBoxSpec> boxes, float boundingRadius, uint32_t texIdx, const Vector4& color)
	{
		Object3d::ModelData data{};
		BuildBoxListModel(data, boxes, boundingRadius);
		data.material.textureFilePath = "Resources/helicopter_texture.png";
		data.material.textureIndex = texIdx;
		return CreateModelObject(object3dCom, camera, data, color);
	}

	Object3d::ModelData LoadHeliObjModel(const char* objFileName, uint32_t texIdx, float boundingRadius)
	{
		Object3d::ModelData data = Object3d::LoadObjFile("Resources", objFileName);
		data.material.textureFilePath = "Resources/helicopter_texture.png";
		data.material.textureIndex = texIdx;
		data.boundingRadius = boundingRadius;
		return data;
	}

	inline void UpdateObjectTransform(Object3d* obj, const Vector3& pos, const Vector3& rot, const Vector3& scale = kUnitScale)
	{
		if (!obj) return;
		obj->SetTranslate(pos);
		obj->SetRotate(rot);
		obj->SetScale(scale);
		obj->Update();
	}

	AppParticle MakeStyledParticle(
		const Vector3& pos, const Vector3& vel, float rotZ, float initialScale,
		float targetScale, float lifeTime, uint32_t texIdx, const ParticleStyleSpec& style)
	{
		AppParticle p{};
		p.transform.Initialize();
		p.transform.SetTranslate(pos);
		if (rotZ != 0.0f) p.transform.SetRotate({ 0.0f, 0.0f, rotZ });
		p.scaleAspect = style.scaleAspect;
		p.initialScale = initialScale;
		p.targetScale = targetScale;
		p.transform.SetScale({ initialScale * style.scaleAspect.x, initialScale * style.scaleAspect.y, 1.0f });
		p.velocity = vel;
		p.color = style.color;
		p.endColor = style.endColor;
		p.hasColorShift = true;
		p.lifeTime = lifeTime;
		p.currentTime = 0.0f;
		p.textureIndex = texIdx;
		p.gravity = style.gravity;
		p.drag = style.drag;
		p.bounceElasticity = style.bounceElasticity;
		p.fadeInRatio = style.fadeInRatio;
		p.wobbleFreq = style.wobbleFreq;
		p.wobbleAmp = style.wobbleAmp;
		p.wobbleAxis = style.wobbleAxis;
		p.slotLayer = style.slotLayer;
		return p;
	}

	UIVec2 MeasureCineText(UIFontRole role, float fontSize, const char* text)
	{
		if (!text || !text[0]) return UIVec2(0.0f, 0.0f);
		TacticalUICanvas* ui = TacticalUICanvas::GetInstance();
		if (!ui) return UIVec2(0.0f, 0.0f);
		return ui->CalcTextSize(text, nullptr, std::round(std::clamp(fontSize, kCineFontMinPx, kCineFontMaxPx)), role);
	}

	void DrawFramedBox(
		TacticalDrawList* dl, float x, float y, float w, float h,
		uint32_t bgCol, uint32_t borderCol, float borderThick = kBoxBorderThickness)
	{
		dl->AddRectFilled(UIVec2(x, y), UIVec2(x + w, y + h), bgCol, kBoxCornerRadius);
		dl->AddRect(UIVec2(x, y), UIVec2(x + w, y + h), borderCol, kBoxCornerRadius, 0, borderThick);
	}

	void DrawCineTextAligned(
		TacticalDrawList* dl, UIFontRole role, float fontSize,
		float anchorX, float centerY, uint32_t col, const char* text, int align = kAlignLeft)
	{
		if (!dl || !text || !text[0]) return;
		const uint32_t alphaByte = (col >> UI_COL32_A_SHIFT) & kByteMask;
		if (alphaByte == 0u) return;
		TacticalUICanvas* ui = TacticalUICanvas::GetInstance();
		if (!ui) return;
		const TacticalFont* font = ui->GetFont(role);
		if (!font) return;

		const float px = std::round(std::clamp(fontSize, kCineFontMinPx, kCineFontMaxPx));
		const UIVec2 sz = ui->CalcTextSize(text, nullptr, px, role);
		float tx = anchorX;
		if (align == kAlignCenter) tx -= sz.x * 0.5f;
		else if (align == kAlignRight) tx -= sz.x;
		const float sx = std::round(tx);
		const float sy = std::round(centerY - sz.y * 0.5f);
		const int shA = static_cast<int>(alphaByte * kCineShadowAlphaRatio);
		dl->AddText(font, px, UIVec2(sx + kCineShadowOffsetPx, sy + kCineShadowOffsetPx), PackColor(kColBlackRgb, shA), text);
		dl->AddText(font, px, UIVec2(sx, sy), col, text);
	}
}

HelicopterInsertionController::HelicopterInsertionController(GamePlayScene* scene)
	: scene_(scene)
{
}

HelicopterInsertionController::~HelicopterInsertionController()
{
	Finalize();
}

AppParticleManager* HelicopterInsertionController::GetSmokeMgr() const
{
	return scene_->smokeParticleManager_ ? scene_->smokeParticleManager_.get() : scene_->appParticleManager_.get();
}

void HelicopterInsertionController::Initialize()
{
	if (!scene_ || !scene_->object3dCom) return;

	wasSkipped_ = false;
	auto* tm = TextureManager::GetInstance();
	heliTexIndex_  = tm->Load("Resources/helicopter_texture.png");
	whiteTexIndex_ = tm->Load("Resources/white.png");
	spotTexIndex_  = tm->Load("Resources/circle2.png");
	leafTexIndex_  = tm->Load("Resources/leaf.png");

	if (SceneManager::GetInstance() && SceneManager::GetInstance()->GetAudioManager())
	{
		duckSoundId_ = SceneManager::GetInstance()->GetAudioManager()->Load("Resources/Audio/SE_Duck.mp3");
	}

	const uint32_t modelTexIdx = ResolveTex(heliTexIndex_, whiteTexIndex_);

	// 1. 機体本体・キャビン内装・スライドカーゴドア・ローター・ファストロープ・地上影
	heliBody_ = CreateModelObject(
		scene_->object3dCom, scene_->camera_, LoadHeliObjModel("helicopter_body.obj", modelTexIdx, kHeliModelBoundingRadius), kUnitWhiteColor);
	heliCabinInterior_ = CreateProceduralHeliPart(
		scene_->object3dCom, scene_->camera_, kCabinInteriorBoxes, kCabinInteriorBoundingRadius, modelTexIdx, kCabinInteriorColor);
	heliCargoDoor_ = CreateProceduralHeliPart(
		scene_->object3dCom, scene_->camera_, kCargoDoorBoxes, kCargoDoorBoundingRadius, modelTexIdx, kUnitWhiteColor);

	heliMainRotor_ = CreateModelObject(
		scene_->object3dCom, scene_->camera_, LoadHeliObjModel("helicopter_rotor.obj", modelTexIdx, kHeliModelBoundingRadius), kUnitWhiteColor);
	heliMainRotorBlur1_ = CreateSharedObject(scene_->object3dCom, scene_->camera_, heliMainRotor_.get(), kMainRotorBlur1Color);
	heliMainRotorBlur2_ = CreateSharedObject(scene_->object3dCom, scene_->camera_, heliMainRotor_.get(), kMainRotorBlur2Color);

	heliTailRotor_ = CreateModelObject(
		scene_->object3dCom, scene_->camera_, LoadHeliObjModel("helicopter_tailrotor.obj", modelTexIdx, kHeliModelBoundingRadius), kUnitWhiteColor);
	fastRope_ = CreateModelObject(
		scene_->object3dCom, scene_->camera_, LoadHeliObjModel("fast_rope.obj", modelTexIdx, kFastRopeBoundingRadius), kUnitWhiteColor);

	Object3d::ModelData shadowData{};
	BuildShadowDiscModel(shadowData, kShadowDiscRadiusX, kShadowDiscRadiusZ);
	shadowData.material.textureFilePath = "Resources/circle2.png";
	shadowData.material.textureIndex = ResolveTex(spotTexIndex_, whiteTexIndex_);
	shadowData.boundingRadius = kShadowBoundingRadius;
	heliShadow_ = CreateModelObject(scene_->object3dCom, scene_->camera_, shadowData, kInitialShadowColor, false);

	// 初期状態セットアップ: 超遠空からの高速急襲アプローチ
	isActive_ = true;
	isExtractionVisible_ = false;
	extractionApproachTimer_ = 0.0f;
	pickupDescentBlend_ = 0.0f;
	clearQuackPlayed_ = false;

	timer_ = 0.0f;
	heliPos_ = kInitialHeliPos;
	heliRot_ = kInitialHeliRot;
	mainRotorAngle_ = 0.0f;
	tailRotorAngle_ = 0.0f;
	doorSlideProgress_ = 1.0f;
	ropeSwayAngleX_ = 0.0f;
	ropeSwayAngleZ_ = 0.0f;
	downwashDustTimer_ = 0.0f;
	ropeSparkTimer_ = 0.0f;
	rotorVortexTimer_ = 0.0f;
	engineExhaustTimer_ = 0.0f;
	beaconTimer_ = 0.0f;
	tailRotorVortexTimer_ = 0.0f;
	landed_ = false;
	headSnapEmitted_ = false;
	eyeGlintEmitted_ = false;
	featherShakeEmitted_ = false;
	footstepDust1_ = false;
	footstepDust2_ = false;
	cockSparkEmitted_ = false;
	duckVoiced_ = false;
	wasSkipped_ = false;
	skipInputArmed_ = false;

	currentCut_ = kCutAct1;
	prevCut_ = kCutAct1;
	cutFlashAlpha_ = 0.0f;
	impactFlashAlpha_ = 0.0f;

	duckPos_ = LocalToWorldWithMatrix(heliPos_, MakeEulerRotationMatrix(heliRot_), kInitialCabinDuckOffset);
	duckScale_ = kUnitScale;
	duckRot_ = heliRot_;

	if (scene_->player_) scene_->player_->SetIntroTransform(duckPos_, duckScale_, duckRot_);

	if (scene_->camera_)
	{
		scene_->camera_->SetTranslate(kAct1CamPosStart);
		scene_->camera_->SetRotate(CalcLookAtRotation(kAct1CamPosStart, heliPos_ + kAct1CamLookOffset, kAct1CamRollStart));
		scene_->camera_->Update();
	}

	UpdateHelicopterCommonParts(kInitRotorBlurOffset, true, 1.0f);
}

void HelicopterInsertionController::Finalize()
{
	rotorLoop_.Stop();
	heliBody_.reset();
	heliCabinInterior_.reset();
	heliCargoDoor_.reset();
	heliMainRotorBlur2_.reset();
	heliMainRotorBlur1_.reset();
	heliMainRotor_.reset();
	heliTailRotor_.reset();
	fastRope_.reset();
	heliShadow_.reset();
}

void HelicopterInsertionController::Skip()
{
	wasSkipped_ = true;
	FinishInsertion();
}

void HelicopterInsertionController::FinishInsertion()
{
	isActive_ = false;
	rotorLoop_.Stop();

	if (scene_ && scene_->player_) scene_->player_->RestoreIntroPose();

	if (scene_ && scene_->camera_ && scene_->player_)
	{
		scene_->camera_->SetTranslate(scene_->player_->GetPosition() + (GameConfig::Camera::kDefaultFollowOffset * scene_->cameraZoom_));
		scene_->camera_->SetRotate(GameConfig::Camera::kFollowRotation);
		scene_->camera_->Update();
	}

	if (scene_)
	{
		scene_->UpdateObstacles();
		if (scene_->extractionPadObject_) scene_->extractionPadObject_->Update();
		if (scene_->extractionSignalFlareObject_) scene_->extractionSignalFlareObject_->Update();
		for (auto& enemy : scene_->enemies_)
		{
			if (enemy && enemy->GetObject3d()) enemy->GetObject3d()->Update();
		}
		for (auto& t : scene_->targets_)
		{
			if (t) t->Update(0.0f);
		}
		if (scene_->player_ && scene_->player_->GetObject3d()) scene_->player_->GetObject3d()->Update();
		if (SceneManager::GetInstance() && SceneManager::GetInstance()->GetSkyBox())
		{
			SceneManager::GetInstance()->GetSkyBox()->Update();
		}
	}

	heliPos_ = kParkedHiddenPos;
	if (fastRope_)
	{
		fastRope_->SetScale(kZeroVec3);
		fastRope_->Update();
	}
	if (heliShadow_)
	{
		heliShadow_->SetTranslate(kParkedHiddenPos);
		heliShadow_->Update();
	}
}

void HelicopterInsertionController::Update(float deltaTime)
{
	if (!isActive_ || !scene_) return;

	// スキップ入力検知（タイトル画面からのクリック押しっぱなし持ち越しを吸収してから新規押下のみ反応）
	const bool osLeftDown = IsAppWindowFocused() && ((GetAsyncKeyState(VK_LBUTTON) & kAsyncKeyDownMask) != 0);
	const bool anySkipHeld =
		IsAppKeyDown(VK_SPACE) || IsAppKeyDown(VK_RETURN) ||
		scene_->mouseInput.PushButton(0) || osLeftDown ||
		(TacticalUICanvas::GetInstance() && TacticalUICanvas::GetInstance()->IsMouseClicked(0));

	if (!skipInputArmed_)
	{
		if (!anySkipHeld) skipInputArmed_ = true;
	}
	else if (anySkipHeld)
	{
		Skip();
		return;
	}

	timer_ += deltaTime;

	// 映画・アニメ的 5幕シネマティック判定
	if (timer_ < kCut1Duration)      currentCut_ = kCutAct1;
	else if (timer_ < kCut2Duration) currentCut_ = kCutAct2;
	else if (timer_ < kCut3Duration) currentCut_ = kCutAct3;
	else if (timer_ < kCut4Duration) currentCut_ = kCutAct4;
	else if (timer_ < kDuration)     currentCut_ = kCutAct5;
	else
	{
		FinishInsertion();
		return;
	}

	if (currentCut_ != prevCut_)
	{
		prevCut_ = currentCut_;
		cutFlashAlpha_ = kCutSwitchFlashAlpha;
	}
	if (cutFlashAlpha_ > 0.0f)    cutFlashAlpha_    = (std::max)(0.0f, cutFlashAlpha_ - kCutFlashDecayRate * deltaTime);
	if (impactFlashAlpha_ > 0.0f) impactFlashAlpha_ = (std::max)(0.0f, impactFlashAlpha_ - kImpactFlashDecayRate * deltaTime);

	// 【完全同期パイプライン】
	UpdateHelicopterMotion(deltaTime);
	UpdateCamera(deltaTime);
	UpdateModelTransforms(deltaTime);
	UpdateParticles(deltaTime);

	// 強襲ヘリ・メインローター空間音響ループ更新
	const float camAtten = scene_->camera_
		? TacticalAudio::ComputeAttenuation(heliPos_, scene_->camera_->GetTranslate(), kInsertAudioMinDist, kInsertAudioMaxDist)
		: kInsertFallbackAtten;
	const float cut5Fade = (currentCut_ == kCutAct5)
		? std::clamp(1.0f - (timer_ - kCut4Duration) / (kDuration - kCut4Duration), 0.0f, 1.0f)
		: 1.0f;
	rotorLoop_.Update("Resources/Audio/SE_Heli_Rotor_Loop.wav", kRotorLoopDurationSec, kInsertRotorVolume * camAtten * cut5Fade, deltaTime);
}

void HelicopterInsertionController::UpdateHelicopterMotion(float deltaTime)
{
	AdvanceWrappedAngle(mainRotorAngle_, kMainRotorAngularSpeed, deltaTime);
	AdvanceWrappedAngle(tailRotorAngle_, kTailRotorAngularSpeed, deltaTime);

	switch (currentCut_)
	{
	case kCutAct1: UpdateAct1ApproachMotion(); break;
	case kCutAct2: UpdateAct2CabinPrepMotion(); break;
	case kCutAct3: UpdateAct3FastRopeMotion(deltaTime); break;
	case kCutAct4: UpdateAct4LandingClearMotion(); break;
	default:       UpdateAct5BreakawayMotion(); break;
	}
}

void HelicopterInsertionController::UpdateAct1ApproachMotion()
{
	// Act 1: 【白昼の超低空急襲・ロングレンジアプローチ＆フレア制動】 (0.0s 〜 3.40s)
	const float t = timer_ / kCut1Duration;
	const float brake = 1.0f - std::pow(1.0f - t, kAct1BrakeExponent);
	heliPos_ = kInitialHeliPos + (kHoverAnchorPos - kInitialHeliPos) * brake;
	heliPos_.x += std::sin(t * kPi) * kAct1SCurveAmpX;

	float pitch = 0.0f;
	float roll = 0.0f;
	const float yaw = kAct1YawInitial * (1.0f - SmoothStep01((t - kAct1YawSettleStartT) / kAct1YawSettleSpanT));

	if (t < kAct1FlareStartT)
	{
		const float subT = t / kAct1FlareStartT;
		pitch = kAct1CruisePitchStart + kAct1CruisePitchGain * subT;
		roll = kAct1CruiseRollStart * (1.0f - subT * kAct1CruiseRollDecayRat);
	}
	else
	{
		const float subT = (t - kAct1FlareStartT) / kAct1FlareSpanT;
		pitch = std::sin(subT * kPi) * kAct1FlarePitchAmp;
		roll = kAct1FlareRollStart * (1.0f - subT);
	}
	heliRot_ = { pitch, yaw, roll };

	if (t >= kAct1DoorOpenStartT && !doorOpenSoundPlayed_)
	{
		doorOpenSoundPlayed_ = true;
		TacticalAudio::Play2D("Resources/Audio/SE_Heli_Door.wav", kDoorOpenSoundVolume);
	}
	doorSlideProgress_ = 1.0f - kDoorOpenTravel * SmoothStep01((t - kAct1DoorOpenStartT) / kAct1DoorOpenSpanT);
	ropeSwayAngleX_ = 0.0f;
	ropeSwayAngleZ_ = 0.0f;

	duckPos_ = LocalToWorldWithMatrix(heliPos_, MakeEulerRotationMatrix(heliRot_), kAct1CabinDuckOffset);
	duckScale_ = kUnitScale;
	duckRot_ = heliRot_;
}

void HelicopterInsertionController::UpdateAct2CabinPrepMotion()
{
	// Act 2: 【ドア前出撃準備・RED/GREEN LIGHT合図のタメ・飛び移り】 (3.40s 〜 5.60s)
	const float phase = (timer_ - kCut1Duration) / (kCut2Duration - kCut1Duration);
	doorSlideProgress_ = kDoorOpenRestProgress;

	heliPos_ = kHoverAnchorPos + Vector3{
		std::sin(timer_ * kAct2HoverOscFreqX) * kAct2HoverOscAmpX,
		std::cos(timer_ * kAct2HoverOscFreqY) * kAct2HoverOscAmpY,
		0.0f
	};
	heliRot_ = { std::sin(timer_ * kAct2RotOscFreqX) * kAct2RotOscAmp, 0.0f, std::cos(timer_ * kAct2RotOscFreqZ) * kAct2RotOscAmp };

	const float deployDamp = std::exp(-phase * kRopeDeployDampRate);
	ropeSwayAngleZ_ = std::sin(phase * kRopeDeployFreqZ) * kRopeDeployAmpZ * deployDamp + kRopeWindAmpZ * std::sin(timer_ * kRopeWindFreqZ);
	ropeSwayAngleX_ = std::cos(phase * kRopeDeployFreqX) * kRopeDeployAmpX * deployDamp;

	const Matrix4x4 rotMat = MakeEulerRotationMatrix(heliRot_);

	if (phase < kAct2WalkEndPhase)
	{
		const float walkNorm = phase / kAct2WalkEndPhase;
		const float smoothStep = SmoothStep01(walkNorm);
		const float stepBob = std::fabs(std::sin(walkNorm * kTwoPi)) * kAct2WalkBobAmp;
		duckPos_ = LocalToWorldWithMatrix(heliPos_, rotMat, {
			kAct1CabinDuckOffset.x + kAct2WalkDeltaX * smoothStep,
			kAct1CabinDuckOffset.y + stepBob,
			kAct1CabinDuckOffset.z
		});
		duckRot_ = { heliRot_.x, heliRot_.y - kAct2WalkTurnYaw * smoothStep, heliRot_.z };
		duckScale_ = kUnitScale;
	}
	else if (phase < kAct2PrepEndPhase)
	{
		if (phase >= kAct2GreenBeepPhase && !greenLightBeepPlayed_)
		{
			greenLightBeepPlayed_ = true;
			TacticalAudio::Play2D("Resources/Audio/SE_Exfil_Tick.wav", kGreenLightBeepVolume);
		}
		const float prepT = (phase - kAct2WalkEndPhase) / kAct2PrepSpanPhase;
		const float nod = (prepT >= kAct2NodStartT && prepT < kAct2NodEndT)
			? (std::sin(((prepT - kAct2NodStartT) / kAct2NodSpanT) * kPi) * kAct2NodAmp)
			: 0.0f;
		const float windBob = std::sin(timer_ * kAct2WindBobFreq) * kAct2WindBobAmp;
		duckPos_ = LocalToWorldWithMatrix(heliPos_, rotMat, { kAct2StandLocalX, kAct1CabinDuckOffset.y + windBob, kAct1CabinDuckOffset.z });
		duckRot_ = { heliRot_.x + kAct2StandPitch + nod, heliRot_.y + kAct2StandYaw, heliRot_.z + kAct2StandRoll };
		duckScale_ = kUnitScale;
	}
	else
	{
		if (!jumpSoundPlayed_)
		{
			jumpSoundPlayed_ = true;
			TacticalAudio::Play2D("Resources/Audio/SE_Dodge_Roll.wav", kJumpSoundVolume);
		}
		const float leapT = (phase - kAct2PrepEndPhase) / kAct2LeapSpanPhase;
		const float smoothLeap = SmoothStep01(leapT);
		const float sinLeap = std::sin(leapT * kPi);

		const float leapX   = kAct2StandLocalX + (kAct2LeapEndX - kAct2StandLocalX) * smoothLeap;
		const float leapZ   = kAct1CabinDuckOffset.z + (kAct2LeapEndZ - kAct1CabinDuckOffset.z) * smoothLeap;
		const float leapYaw = kAct2StandYaw + (kAct2LeapEndYaw - kAct2StandYaw) * smoothLeap;

		duckPos_ = LocalToWorldWithMatrix(heliPos_, rotMat, { leapX, kAct1CabinDuckOffset.y + sinLeap * kAct2LeapHopAmp, leapZ });
		duckRot_ = { heliRot_.x + kAct2LeapPitchGain * smoothLeap, heliRot_.y + leapYaw, heliRot_.z };
		duckScale_ = { 1.0f + kAct2LeapStretchAmp * sinLeap, 1.0f - kAct2LeapStretchAmp * sinLeap, 1.0f };

		const float tensionFactor = 1.0f - smoothLeap * kAct2RopeTensionDamp;
		ropeSwayAngleZ_ *= tensionFactor;
		ropeSwayAngleX_ *= tensionFactor;
	}
}

void HelicopterInsertionController::UpdateAct3FastRopeMotion(float deltaTime)
{
	// Act 3: 【緩急ファストロープ降下・急加速＆地上直前コンバットブレーキ】 (5.60s 〜 7.10s)
	if (!ropeSlideSoundPlayed_)
	{
		ropeSlideSoundPlayed_ = true;
		TacticalAudio::Play2D("Resources/Audio/SE_Rope_Slide.wav", kRopeSlideSoundVolume);
	}
	const float phase = (timer_ - kCut2Duration) / (kCut3Duration - kCut2Duration);
	doorSlideProgress_ = kDoorOpenRestProgress;

	heliPos_ = kHoverAnchorPos + Vector3{
		std::sin(timer_ * kAct3HoverOscFreqX) * kAct3HoverOscAmp,
		std::cos(timer_ * kAct3HoverOscFreqY) * kAct3HoverOscAmp,
		0.0f
	};
	heliRot_ = kZeroVec3;

	const bool isBraking = (phase >= kAct3BrakeStartPhase);
	ropeSwayAngleX_ = 0.0f;
	ropeSwayAngleZ_ = isBraking ? (std::sin(timer_ * kAct3RopeVibFreq) * kAct3RopeVibAmp) : 0.0f;

	float dropProgress = 0.0f;
	if (!isBraking)
	{
		const float t = phase / kAct3BrakeStartPhase;
		dropProgress = kAct3FreefallRatio * (t * t);
	}
	else
	{
		const float invT = 1.0f - (phase - kAct3BrakeStartPhase) / kAct3BrakeSpanPhase;
		dropProgress = kAct3FreefallRatio + kAct3BrakeRatio * (1.0f - invT * invT);
	}

	const float duckY = (std::max)(0.0f, (1.0f - dropProgress) * (kHoverAnchorPos.y + kAct3DropTopExtraY));
	duckPos_ = {
		kRopeWorldX - std::sin(kRopeFacingYaw) * kRopeFrontBodyOffset,
		duckY,
		kRopeWorldZ - std::cos(kRopeFacingYaw) * kRopeFrontBodyOffset
	};

	if (!isBraking)
	{
		duckScale_ = kAct3FreefallScale;
		duckRot_ = { kAct3FreefallPitch, kRopeFacingYaw, 0.0f };
	}
	else
	{
		const float bT = (phase - kAct3BrakeStartPhase) / kAct3BrakeSpanPhase;
		duckScale_ = {
			kAct3FreefallScale.x + kAct3BrakeScaleGainXZ * bT,
			kAct3FreefallScale.y - kAct3BrakeScaleLossY * bT,
			kAct3FreefallScale.z + kAct3BrakeScaleGainXZ * bT
		};
		duckRot_ = { kAct3FreefallPitch + kAct3BrakePitchGain * bT, kRopeFacingYaw, 0.0f };
	}

	// 手袋・ブーツとロープの接触摩擦スパーク火花＆リアル摩擦ウィスプ煙
	if (phase > kRopeSparkStartPhase && phase < kRopeSparkEndPhase && scene_->particleManager && scene_->appParticleManager_)
	{
		ropeSparkTimer_ += deltaTime;
		if (ropeSparkTimer_ >= (isBraking ? kRopeSparkIntervalBrk : kRopeSparkIntervalSld))
		{
			ropeSparkTimer_ = 0.0f;
			const Vector3 sparkP = { kRopeWorldX, duckY + kRopeSparkOffsetY, kRopeWorldZ };
			const float spdMul = isBraking ? kRopeSparkSpeedBrake : kRopeSparkSpeedSlide;
			const uint32_t ropeSparkTex = ResolveTex(scene_->sparkStreakTextureIndex_, scene_->particleTextureB);
			const Vector3 sparkVel = RandBoxVec3(spdMul, kRopeSparkUpVelRange, spdMul);

			scene_->appParticleManager_->AddParticle(MakeStyledParticle(
				sparkP, sparkVel, RandRange(0.0f, kTwoPi), 1.0f, kRopeSparkTargetScale,
				isBraking ? kRopeSparkLifeBrake : kRopeSparkLifeSlide, ropeSparkTex,
				isBraking ? kRopeSparkBrakeStyle : kRopeSparkSlideStyle));

			AppParticleManager* smokeMgr = GetSmokeMgr();
			const uint32_t wispyTex = ResolveTex(scene_->wispySmokeTextureIndex_, scene_->smokeTextureIndex_);
			if (phase >= kRopeSmokeStartPhase && smokeMgr && wispyTex != TextureManager::kInvalidTextureIndex)
			{
				const float sc = RandRangeVec(kRopeSmokeScaleRange);
				const Vector3 smokePos = sparkP + Vector3{
					RandRange(-kRopeSmokeJitterXZ, kRopeSmokeJitterXZ), kRopeSmokeOffsetY, RandRange(-kRopeSmokeJitterXZ, kRopeSmokeJitterXZ)
				};
				const Vector3 smokeVel = RandBoxVec3(kRopeSmokeVelSpreadXZ, kRopeSmokeUpVelRange, kRopeSmokeVelSpreadXZ);
				smokeMgr->AddParticle(MakeStyledParticle(
					smokePos, smokeVel, 0.0f, sc, sc * kRopeSmokeTargetMul,
					RandRangeVec(kRopeSmokeLifeRange), wispyTex, kRopeBrakeSmokeStyle));
			}
		}
	}
}

void HelicopterInsertionController::EmitLandingImpactEffects()
{
	impactFlashAlpha_ = kLandingImpactFlash;
	scene_->TriggerCameraShake(kLandingShakeDuration, kLandingShakeIntensity);
	TacticalAudio::Play2D("Resources/Audio/SE_Boot_Land_Heavy.wav", kLandingBootVolume);

	AppParticleManager* smokeMgr = GetSmokeMgr();
	const uint32_t fallbackSmokeTex = ResolveTex(scene_->wispySmokeTextureIndex_, scene_->smokeTextureIndex_);
	const uint32_t dustTex = ResolveTex(scene_->soilDustTextureIndex_, fallbackSmokeTex);
	const uint32_t chipTex = ResolveTex(scene_->debrisChipTextureIndex_, scene_->particleTextureB);

	// 1. リアルな360度ドーナツ状着地ダストリング ＆ 弾け飛ぶ小石粒
	if (smokeMgr && dustTex != TextureManager::kInvalidTextureIndex)
	{
		const float dustStep = kTwoPi / static_cast<float>(kLandingDustCount);
		for (int i = 0; i < kLandingDustCount; ++i)
		{
			const float a = static_cast<float>(i) * dustStep + RandRange(-kLandingDustAngleJit, kLandingDustAngleJit);
			const float cosA = std::cos(a);
			const float sinA = std::sin(a);
			const float spd = RandRangeVec(kLandingDustSpdRange);
			const float sc = RandRangeVec(kLandingDustScaleRange);
			const Vector3 pos = kLandingImpactPoint + Vector3{ cosA * kLandingDustSpawnRad, kLandingDustSpawnY, sinA * kLandingDustSpawnRad };
			const Vector3 vel = { cosA * spd, RandRangeVec(kLandingDustUpRange), sinA * spd };
			smokeMgr->AddParticle(MakeStyledParticle(
				pos, vel, 0.0f, sc, sc * kLandingDustTargetMul, RandRangeVec(kLandingDustLifeRange), dustTex, kLandingDustStyle));
		}

		for (int i = 0; i < kLandingChipCount; ++i)
		{
			const float a = RandRange(0.0f, kTwoPi);
			const float cosA = std::cos(a);
			const float sinA = std::sin(a);
			const float sc = RandRangeVec(kLandingChipScaleRange);
			const Vector3 pos = kLandingImpactPoint + Vector3{ cosA * kLandingChipSpawnRad, kLandingChipSpawnY, sinA * kLandingChipSpawnRad };
			const Vector3 vel = { cosA * RandRangeVec(kLandingChipSpdRange), RandRangeVec(kLandingChipUpRange), sinA * RandRangeVec(kLandingChipSpdRange) };
			AppParticle ch = MakeStyledParticle(
				pos, vel, a, sc, sc * kLandingChipTargetMul, RandRangeVec(kLandingChipLifeRange), chipTex, kLandingChipStyle);
			ch.angularVelocity = RandRange(-kLandingChipSpinMax, kLandingChipSpinMax);
			smokeMgr->AddParticle(ch);
		}
	}

	// 2. 着地ブーツ金具スパーク粒子 ＆ 3. 出撃アヒルボイス
	if (scene_->particleManager && scene_->appParticleManager_)
	{
		const uint32_t spkTex = ResolveTex(scene_->sparkStreakTextureIndex_, scene_->particleTextureB);
		for (int i = 0; i < kLandingSparkCount; ++i)
		{
			const float a = RandRange(0.0f, kTwoPi);
			const Vector3 vel = { std::cos(a) * RandRangeVec(kLandingSparkSpdRange), RandRangeVec(kLandingSparkUpRange), std::sin(a) * RandRangeVec(kLandingSparkSpdRange) };
			scene_->appParticleManager_->AddParticle(MakeStyledParticle(
				kLandingImpactPoint, vel, a, 1.0f, kLandingSparkTargetScl, RandRangeVec(kLandingSparkLifeRange), spkTex, kLandingSparkStyle));
		}
	}
	PlayDuckVoiceOnce(duckVoiced_, duckSoundId_);
}

void HelicopterInsertionController::EmitWeaponCockEffects()
{
	scene_->TriggerCameraShake(kCockShakeDuration, kCockShakeIntensity);
	TacticalAudio::Play2D("Resources/Audio/SE_Reload_BoltRack.wav", kBoltRackVolume);
	TacticalAudio::Play2D("Resources/Audio/SE_Casing_Bounce.wav", kCasingBounceVolume);

	const uint32_t shellTex = ResolveTex(scene_->cylinderTextureIndex_, ResolveTex(scene_->starburstTextureIndex_, scene_->particleTextureB));
	scene_->appParticleManager_->EmitShellCasing(
		scene_->particleManager->GetRandomEngine(), kChamberEjectPos, kUnitForwardZ, kBrassCasingColor, kBrassCasingScale, shellTex);

	const uint32_t cockSparkTex = ResolveTex(scene_->sparkStreakTextureIndex_, scene_->particleTextureB);
	scene_->appParticleManager_->EmitMuzzleFlare(
		scene_->particleManager->GetRandomEngine(), kCockGlintPos, kCockGlintScale, kCockGlintColor, kCockGlintLife, cockSparkTex);

	const float step = kTwoPi / static_cast<float>(kCockSparkCount);
	for (int i = 0; i < kCockSparkCount; ++i)
	{
		const float a = static_cast<float>(i) * step;
		const Vector3 vel = { std::cos(a) * kCockSparkRadialSpeed, std::sin(a) * kCockSparkRadialSpeed + kCockSparkUpBias, kCockSparkForwardVel };
		scene_->appParticleManager_->AddParticle(MakeStyledParticle(
			kCockGlintPos, vel, a, 1.0f, kCockSparkTargetScale, kCockSparkLife, cockSparkTex, kCockSparkStyle));
	}
}

void HelicopterInsertionController::UpdateAct4LandingClearMotion()
{
	// Act 4: 【着地激突・左右セクター360クリアリング＆コッキング】 (7.10s 〜 8.80s)
	const float phase = (timer_ - kCut3Duration) / (kCut4Duration - kCut3Duration);
	doorSlideProgress_ = kDoorOpenRestProgress;
	ropeSwayAngleZ_ = std::sin(phase * kAct4RopeSwayFreq) * kAct4RopeSwayAmp * (1.0f - phase);

	if (!landed_)
	{
		landed_ = true;
		EmitLandingImpactEffects();
	}

	if (phase < kAct4SkidEndPhase)
	{
		const float smoothSkid = SmoothStep01(phase / kAct4SkidEndPhase);
		const float squat = (phase < kAct4SquatEndPhase) ? (1.0f - (phase / kAct4SquatEndPhase) * kAct4SquatDrop) : kAct4SquatFloor;
		const Vector3 curPos = LerpVec3(kAct4SkidStartPos, kAct4SkidEndPos, smoothSkid);

		duckPos_ = { curPos.x, kAct4SkidSinkY * squat, curPos.z };
		duckScale_ = {
			kAct4SkidScaleBaseX + kAct4SkidScaleSquatDelta * squat,
			kAct4SkidScaleBaseY - kAct4SkidScaleSquatDelta * squat,
			kAct4SkidScaleBaseZ + kAct4SkidScaleSquatDelta * squat
		};
		duckRot_ = { kAct4SkidPitchScale * squat, kAct4SkidYaw, kAct4SkidRollStart * (1.0f - smoothSkid) };

		AppParticleManager* smokeMgr = GetSmokeMgr();
		const uint32_t wispyTex = ResolveTex(scene_->wispySmokeTextureIndex_, scene_->smokeTextureIndex_);
		if (smokeMgr && wispyTex != TextureManager::kInvalidTextureIndex)
		{
			const float sc = RandRangeVec(kSkidSmokeScaleRange);
			const Vector3 skPos = duckPos_ + Vector3{ RandRange(-kSkidSmokeJitterX, kSkidSmokeJitterX), kSkidSmokeOffsetY, RandRangeVec(kSkidSmokeOffsetZRange) };
			const Vector3 skVel = { RandRange(-kSkidSmokeVelJitterX, kSkidSmokeVelJitterX), RandRangeVec(kSkidSmokeVelYRange), RandRangeVec(kSkidSmokeVelZRange) };
			smokeMgr->AddParticle(MakeStyledParticle(
				skPos, skVel, 0.0f, sc, sc * kSkidSmokeTargetMul, kSkidSmokeLife, wispyTex, kLandingSkidSmokeStyle));
		}
	}
	else if (phase < kAct4SnapEndPhase)
	{
		const float snapSmooth = SmoothStep01((phase - kAct4SkidEndPhase) / kAct4SnapSpanPhase);
		duckScale_ = {
			kAct4SnapScaleStartX - kAct4SnapScaleDelta * snapSmooth,
			kAct4SnapScaleStartY + kAct4SnapScaleDelta * snapSmooth,
			kAct4SnapScaleStartZ - kAct4SnapScaleDelta * snapSmooth
		};
		duckPos_ = { kAct4SkidEndPos.x, kAct4SnapStartY * (1.0f - snapSmooth), kAct4SkidEndPos.z };
		duckRot_ = { kAct4SnapPitchStart * (1.0f - snapSmooth) + kAct4SnapPitchEnd * snapSmooth, kAct4SkidYaw, 0.0f };

		if (!headSnapEmitted_)
		{
			headSnapEmitted_ = true;
			scene_->TriggerCameraShake(kHeadSnapShakeDuration, kHeadSnapShakeIntensity);
		}
		if (!eyeGlintEmitted_ && phase >= kEyeGlintPhase && scene_->particleManager && scene_->appParticleManager_)
		{
			eyeGlintEmitted_ = true;
			const uint32_t eyeGlintTex = ResolveTex(scene_->sparkStreakTextureIndex_, scene_->starburstTextureIndex_);
			scene_->appParticleManager_->EmitMuzzleFlare(
				scene_->particleManager->GetRandomEngine(), duckPos_ + kEyeGlintOffset, kEyeGlintScale, kEyeGlintColor, kEyeGlintLife, eyeGlintTex);
		}
	}
	else if (phase < kAct4RuffleEndPhase)
	{
		const float riseT = (phase - kAct4SnapEndPhase) / kAct4RuffleSpanPhase;
		const float ruffleEnv = std::sin(riseT * kPi);
		const float roll = std::sin(riseT * kTwoPi * kAct4RuffleCycles) * (ruffleEnv * kAct4RuffleRollAmp);

		duckScale_ = { 1.0f - ruffleEnv * kAct4RuffleScaleLossXZ, 1.0f + ruffleEnv * kAct4RuffleScaleGainY, 1.0f - ruffleEnv * kAct4RuffleScaleLossXZ };
		duckPos_ = { kAct4SkidEndPos.x, kAct4RuffleHopY * ruffleEnv, kAct4SkidEndPos.z };
		duckRot_ = { kAct4SnapPitchEnd + std::fabs(roll) * kAct4RufflePitchGain, kAct4SkidYaw * (1.0f - riseT * kAct4RuffleYawDecay), roll };

		if (!featherShakeEmitted_ && phase >= kFeatherShakePhase)
		{
			featherShakeEmitted_ = true;
			AppParticleManager* fMgr = GetSmokeMgr();
			const uint32_t fTex = ResolveTex(scene_->featherTextureIndex_, scene_->particleTextureB);
			if (fMgr)
			{
				for (int i = 0; i < kFeatherShakeCount; ++i)
				{
					const float sc = RandRangeVec(kFeatherScaleRange);
					const Vector3 fPos = duckPos_ + RandBoxVec3(kFeatherPosJitterX, kFeatherPosYRange, kFeatherPosJitterZ);
					const Vector3 fVel = RandBoxVec3(kFeatherVelJitterXZ, kFeatherVelYRange, kFeatherVelJitterXZ);
					AppParticle f = MakeStyledParticle(
						fPos, fVel, RandRange(0.0f, kTwoPi), sc, sc * kFeatherTargetMul,
						RandRangeVec(kFeatherLifeRange), fTex, kFeatherShakeStyle);
					const Vector4& col = kFeatherAlternateCols[i & 1];
					f.color = col;
					f.endColor = { col.x, col.y, col.z, 0.0f };
					f.angularVelocity = RandRange(-kFeatherSpinMax, kFeatherSpinMax);
					fMgr->AddParticle(f);
				}
			}
		}
	}
	else if (phase < kAct4StrideEndPhase)
	{
		const float strideT = (phase - kAct4RuffleEndPhase) / kAct4StrideSpanPhase;
		const float smoothStride = SmoothStep01(strideT);
		const float bob = std::fabs(std::sin(strideT * kTwoPi)) * kAct4StrideBobAmp;
		duckPos_ = { kAct4SkidEndPos.x * (1.0f - smoothStride), bob, kAct4SkidEndPos.z * (1.0f - smoothStride) };
		duckScale_ = kUnitScale;

		AppParticleManager* stepMgr = GetSmokeMgr();
		const uint32_t stepDustTex = ResolveTex(scene_->soilDustTextureIndex_, scene_->wispySmokeTextureIndex_);
		auto tryEmitStep = [&](float triggerPhase, bool& emitted, const char* sePath) {
			if (phase >= triggerPhase && !emitted && scene_->particleManager && stepMgr)
			{
				emitted = true;
				TacticalAudio::Play2D(sePath, kFootstepVolume);
				stepMgr->EmitFootstepDust(scene_->particleManager->GetRandomEngine(), duckPos_, stepDustTex);
			}
		};
		tryEmitStep(kFootstep1Phase, footstepDust1_, "Resources/Audio/SE_Step_Dirt_1.wav");
		tryEmitStep(kFootstep2Phase, footstepDust2_, "Resources/Audio/SE_Step_Dirt_2.wav");

		const bool isLeftScan = (phase < kAct4ScanMidPhase);
		const float scanStart = isLeftScan ? kAct4RuffleEndPhase : kAct4ScanMidPhase;
		const float fromYaw   = isLeftScan ? kAct4ScanStartYaw   : kAct4ScanLeftYaw;
		const float toYaw     = isLeftScan ? kAct4ScanLeftYaw    : kAct4ScanRightYaw;
		const float bankSign  = isLeftScan ? -1.0f : 1.0f;
		const float smoothSnap = SmoothStep01((std::min)(((phase - scanStart) / kAct4ScanHalfSpan) / kAct4ScanSnapPortion, 1.0f));
		duckRot_ = { kAct4ScanPitch, fromYaw * (1.0f - smoothSnap) + toYaw * smoothSnap, bankSign * kAct4ScanBankRoll * smoothSnap };
	}
	else if (phase < kAct4FrontReturnEnd)
	{
		const float sFront = SmoothStep01((phase - kAct4StrideEndPhase) / kAct4SubStepSpan05);
		duckRot_ = { kAct4FrontReturnPitch, kAct4ScanRightYaw * (1.0f - sFront), 0.0f };
		duckPos_ = kZeroVec3;
		duckScale_ = kUnitScale;
	}
	else if (phase < kAct4RackPullEnd)
	{
		const float sRack = SmoothStep01((phase - kAct4FrontReturnEnd) / kAct4SubStepSpan05);
		duckPos_ = { 0.0f, kAct4RackLiftY * sRack, kAct4RackPullZ * sRack };
		duckRot_ = { kAct4RackPitch * sRack, 0.0f, kAct4RackRoll * sRack };
		duckScale_ = kAct3FreefallScale;
	}
	else if (phase < kAct4BoltSlamEnd)
	{
		const float rebound = std::sin(((phase - kAct4RackPullEnd) / kAct4BoltSlamSpan) * kPi);
		duckPos_ = { 0.0f, 0.0f, kAct4SlamReboundZ * rebound };
		duckRot_ = { kAct4SlamPitch * rebound, 0.0f, 0.0f };
		duckScale_ = { 1.0f + kAct4SlamScaleDelta * rebound, 1.0f - kAct4SlamScaleDelta * rebound, 1.0f + kAct4SlamScaleDelta * rebound };
	}
	else
	{
		const float breath = std::sin(((phase - kAct4BoltSlamEnd) / kAct4IdleSpan) * kTwoPi) * kAct4IdleBreathAmp;
		duckPos_ = { 0.0f, breath, 0.0f };
		duckRot_ = kZeroVec3;
		duckScale_ = { 1.0f + breath * 0.5f, 1.0f - breath * 0.5f, 1.0f };
	}

	if (!cockSparkEmitted_ && phase >= kAct4RackPullEnd && scene_->particleManager && scene_->appParticleManager_)
	{
		cockSparkEmitted_ = true;
		EmitWeaponCockEffects();
	}

	heliPos_ = kHoverAnchorPos + Vector3{ 0.0f, std::sin(timer_ * kAct4HeliHoverFreq) * kAct4HeliHoverAmpY, 0.0f };
	heliRot_ = { kAct4HeliNosePitch * phase, 0.0f, 0.0f };
}

void HelicopterInsertionController::UpdateAct5BreakawayMotion()
{
	// Act 5: 【ヘリ急上昇離脱（ドア閉鎖）＆シームレスTPS合流】 (8.80s 〜 9.70s)
	if (!doorCloseSoundPlayed_)
	{
		doorCloseSoundPlayed_ = true;
		TacticalAudio::Play2D("Resources/Audio/SE_Heli_Door.wav", kDoorCloseSoundVolume);
	}
	const float phase = (timer_ - kCut4Duration) / (kDuration - kCut4Duration);
	doorSlideProgress_ = kDoorOpenRestProgress + kDoorOpenTravel * SmoothStep01(phase / kAct5DoorCloseSpan);

	heliPos_ = kHoverAnchorPos + Vector3{
		std::sin(phase * kPi) * kAct5ClimbBankAmpX,
		kAct5ClimbGainY * (phase * phase),
		kAct5ForwardDashZ * SmoothStep01(phase)
	};
	heliRot_ = { kAct5BreakPitchStart * (1.0f - phase * kAct5BreakPitchDecay), 0.0f, kAct5BreakRollEnd * phase };

	duckPos_ = kZeroVec3;
	duckScale_ = kUnitScale;
	duckRot_ = kZeroVec3;
}

void HelicopterInsertionController::UpdateCamera(float /*deltaTime*/)
{
	if (!scene_ || !scene_->camera_) return;

	Vector3 currentCamPos{};
	Vector3 currentCamRot{};

	if (currentCut_ == kCutAct1)
	{
		const float smoothT = SmoothStep01(timer_ / kCut1Duration);
		Vector3 eye = LerpVec3(kAct1CamPosStart, kAct1CamPosEnd, smoothT);
		eye.y += std::sin(timer_ * kAct1CamVibFreq) * (kAct1CamVibBase + kAct1CamVibGain * smoothT);
		currentCamPos = eye;
		currentCamRot = CalcLookAtRotation(eye, heliPos_ + kAct1CamLookOffset, kAct1CamRollStart * (1.0f - smoothT));
	}
	else if (currentCut_ == kCutAct2)
	{
		const float phase = (timer_ - kCut1Duration) / (kCut2Duration - kCut1Duration);
		const Vector3 camEnd = heliPos_ + ((phase >= kAct2PrepEndPhase) ? kAct2CamLeapEndOff : kAct2CamHoldEndOff);
		currentCamPos = LerpVec3(heliPos_ + kAct2CamStartOff, camEnd, SmoothStep01(phase));
		currentCamRot = CalcLookAtRotation(currentCamPos, { duckPos_.x, duckPos_.y + kAct2CamTargetLiftY, duckPos_.z }, kAct2CamRoll);
	}
	else if (currentCut_ == kCutAct3)
	{
		const float phase = (timer_ - kCut2Duration) / (kCut3Duration - kCut2Duration);
		const Vector3 highCamPos = { kAct3HighCamX, duckPos_.y + kAct3HighCamLiftY, kAct3HighCamZ };
		const Vector3 highTarget = { duckPos_.x, duckPos_.y + kAct3HighTargetDropY, duckPos_.z + kAct3HighTargetLeadZ };
		const Vector3 lowTarget  = { duckPos_.x, kAct3LowTargetY, duckPos_.z };

		const float blendSmooth = SmoothStep01((std::max)(0.0f, (phase - kAct3CamBlendStartPhase) / kAct3CamBlendSpanPhase));
		currentCamPos = LerpVec3(highCamPos, kAct3LowCamPos, blendSmooth);
		currentCamRot = CalcLookAtRotation(currentCamPos, LerpVec3(highTarget, lowTarget, blendSmooth), kAct3CamRollStart * (1.0f - blendSmooth));
	}
	else if (currentCut_ == kCutAct4)
	{
		const float phase = (timer_ - kCut3Duration) / (kCut4Duration - kCut3Duration);
		Vector3 eye = LerpVec3(kAct3LowCamPos, kAct4CamPosEnd, SmoothStep01(phase));

		float camRoll = 0.0f;
		float targetX = duckPos_.x * kAct4CamFollowTrackRatio;
		if (phase < kAct4CamDipEndPhase)
		{
			const float dip = 1.0f - phase / kAct4CamDipEndPhase;
			eye.y -= kAct4CamDipAmpY * dip * dip;
			camRoll = kAct4CamDipRoll * dip;
		}
		else if (phase >= kAct4RuffleEndPhase && phase < kAct4StrideEndPhase)
		{
			const bool isLeftScan = (phase < kAct4ScanMidPhase);
			const float scanSin = std::sin(((phase - (isLeftScan ? kAct4RuffleEndPhase : kAct4ScanMidPhase)) / kAct4ScanHalfSpan) * kPi);
			const float dirSign = isLeftScan ? -1.0f : 1.0f;
			camRoll = dirSign * kAct4CamScanRollAmp * scanSin;
			targetX += dirSign * kAct4CamScanLeadX * scanSin;
		}

		if (phase >= kAct4CamPunchStartPhase && phase < kAct4CamPunchEndPhase)
		{
			const float punch = std::sin(((phase - kAct4CamPunchStartPhase) / kAct4CamPunchSpanPhase) * kPi) * kAct4CamPunchAmpZ;
			eye.z -= punch;
			eye.y -= punch * kAct4CamPunchYRatio;
		}

		currentCamPos = eye;
		currentCamRot = CalcLookAtRotation(eye, { targetX, kAct4CamTargetY, duckPos_.z * kAct4CamFollowTrackRatio }, camRoll);
	}
	else
	{
		const float smoothT = SmoothStep01((timer_ - kCut4Duration) / (kDuration - kCut4Duration));
		const Vector3 playerPos = scene_->player_->GetPosition();
		const Vector3 camEndPos = playerPos + (GameConfig::Camera::kDefaultFollowOffset * scene_->cameraZoom_);

		currentCamPos = {
			kAct4CamPosEnd.x + (camEndPos.x - kAct4CamPosEnd.x) * smoothT - std::sin(smoothT * kPi) * kAct5CamSwerveAmpX,
			kAct4CamPosEnd.y + (camEndPos.y - kAct4CamPosEnd.y) * std::sin(smoothT * kHalfPi),
			kAct4CamPosEnd.z + (camEndPos.z - kAct4CamPosEnd.z) * smoothT
		};

		const Vector3 lookRot = CalcLookAtRotation(currentCamPos, { playerPos.x, kAct5CamLookTargetY, playerPos.z }, 0.0f);
		const float blendSmooth = SmoothStep01((std::max)(0.0f, (smoothT - kAct5CamRotBlendStartT) / kAct5CamRotBlendSpanT));
		currentCamRot = {
			lookRot.x + (GameConfig::Camera::kFollowRotation.x - lookRot.x) * blendSmooth,
			lookRot.y * (1.0f - blendSmooth),
			0.0f
		};
	}

	scene_->camera_->SetTranslate(currentCamPos);
	scene_->camera_->SetRotate(currentCamRot);
	scene_->camera_->Update();
}

void HelicopterInsertionController::UpdateHelicopterCommonParts(float blurOffset, bool showShadow, float shadowAlphaScale)
{
	const Matrix4x4 rotMat = MakeEulerRotationMatrix(heliRot_);

	UpdateObjectTransform(heliBody_.get(), heliPos_, heliRot_);
	UpdateObjectTransform(heliCabinInterior_.get(), heliPos_, heliRot_);

	if (heliCargoDoor_)
	{
		const float curX = kDoorClosedX - kDoorSlideDeltaX * doorSlideProgress_;
		const float curZ = kDoorOpenZ + (kDoorClosedZ - kDoorOpenZ) * doorSlideProgress_;
		UpdateObjectTransform(heliCargoDoor_.get(), LocalToWorldWithMatrix(heliPos_, rotMat, { curX, kDoorLocalY, curZ }), heliRot_);
	}

	if (heliMainRotor_)
	{
		const Vector3 mastWorld = LocalToWorldWithMatrix(heliPos_, rotMat, kMainRotorMastLocal);
		UpdateObjectTransform(heliMainRotor_.get(),      mastWorld, { heliRot_.x, heliRot_.y + mainRotorAngle_,                     heliRot_.z });
		UpdateObjectTransform(heliMainRotorBlur1_.get(), mastWorld, { heliRot_.x, heliRot_.y + mainRotorAngle_ - blurOffset,        heliRot_.z });
		UpdateObjectTransform(heliMainRotorBlur2_.get(), mastWorld, { heliRot_.x, heliRot_.y + mainRotorAngle_ - blurOffset * 2.0f, heliRot_.z });
	}

	UpdateObjectTransform(
		heliTailRotor_.get(), LocalToWorldWithMatrix(heliPos_, rotMat, kTailRotorHubLocal),
		{ heliRot_.x + tailRotorAngle_, heliRot_.y, heliRot_.z });

	if (heliShadow_)
	{
		if (showShadow && shadowAlphaScale > kShadowMinAlphaScale)
		{
			const Vector3 shadowPos = {
				heliPos_.x + kShadowSunOffsetXPerY * heliPos_.y, kShadowGroundY, heliPos_.z + kShadowSunOffsetZPerY * heliPos_.y
			};
			const float heightRatio = std::clamp(heliPos_.y / kShadowRefHeight, kShadowHeightRatioMin, kShadowHeightRatioMax);
			const float shadowScale = kShadowBaseScale + kShadowHeightScaleGain * (heightRatio - 1.0f);
			const float shadowAlpha = std::clamp(kShadowBaseAlpha - kShadowAlphaDropPerY * heliPos_.y, kShadowAlphaMin, kShadowAlphaMax) * shadowAlphaScale;

			heliShadow_->SetColor({ kShadowRuntimeRgb.x, kShadowRuntimeRgb.y, kShadowRuntimeRgb.z, shadowAlpha });
			UpdateObjectTransform(heliShadow_.get(), shadowPos, { 0.0f, heliRot_.y, 0.0f }, { shadowScale, 1.0f, shadowScale * kShadowAspectZ });
		}
		else
		{
			heliShadow_->SetTranslate(kParkedHiddenPos);
			heliShadow_->Update();
		}
	}
}

void HelicopterInsertionController::UpdateModelTransforms(float /*deltaTime*/)
{
	const float shadowFade = (currentCut_ == kCutAct5)
		? (std::max)(0.0f, 1.0f - (timer_ - kCut4Duration) / (kDuration - kCut4Duration))
		: 1.0f;
	const float blurOff = (currentCut_ == kCutAct1 || currentCut_ == kCutAct5) ? kHighSpeedRotorBlurOff : kHoverRotorBlurOff;
	UpdateHelicopterCommonParts(blurOff, true, shadowFade);

	// ファストロープ (Act 2 & Act 3 & Act 4序盤で機体の3D姿勢から地面へ垂らす)
	if (fastRope_)
	{
		if (timer_ >= kCut1Duration && timer_ < kCut4Duration + kFastRopeVisibleExtraSec)
		{
			const Vector3 ropeTop = LocalToWorldWithMatrix(heliPos_, MakeEulerRotationMatrix(heliRot_), kFastRopeAnchorLocal);
			const float deployProgress = (timer_ < kCut1Duration + kFastRopeDeployDuration)
				? SmoothStep01((timer_ - kCut1Duration) / kFastRopeDeployDuration)
				: 1.0f;
			const float scaleY = (std::max)((ropeTop.y * deployProgress) / kFastRopeMeshBaseLen, kFastRopeMinScaleY);
			UpdateObjectTransform(fastRope_.get(), ropeTop, { ropeSwayAngleX_, 0.0f, ropeSwayAngleZ_ }, { 1.0f, scaleY, 1.0f });
		}
		else
		{
			fastRope_->SetScale(kZeroVec3);
			fastRope_->Update();
		}
	}

	if (scene_->player_) scene_->player_->SetIntroTransform(duckPos_, duckScale_, duckRot_);
}

void HelicopterInsertionController::EmitHelicopterFlightParticles(float deltaTime, bool isHighG, bool isFullThrottle, float washHeightLimit)
{
	if (!scene_ || !scene_->particleManager || !scene_->appParticleManager_) return;

	AppParticleManager* smokeMgr = GetSmokeMgr();
	const uint32_t wispyTex = ResolveTex(scene_->wispySmokeTextureIndex_, scene_->smokeTextureIndex_);
	const Matrix4x4 rotMat = MakeEulerRotationMatrix(heliRot_);

	// 1. メインローター翼端渦ベイパー
	rotorVortexTimer_ += deltaTime;
	if (rotorVortexTimer_ >= kRotorVortexInterval && smokeMgr && wispyTex != TextureManager::kInvalidTextureIndex)
	{
		rotorVortexTimer_ = 0.0f;
		const Vector3 mastWorld = LocalToWorldWithMatrix(heliPos_, rotMat, kMainRotorMastLocal);
		const float vaporAlpha = isHighG ? kVortexAlphaHighG : kVortexAlphaNormal;
		const float vaporScale = isHighG ? kVortexScaleHighG : kVortexScaleNormal;
		const float vaporLife  = isHighG ? kVortexLifeHighG  : kVortexLifeNormal;
		const Vector3 downwashVec = RotateByMatrix(rotMat, { 0.0f, kRotorTipDownwashVelY, 0.0f });

		for (int i = 0; i < kRotorBladeCount; ++i)
		{
			const float angle = mainRotorAngle_ + static_cast<float>(i) * kHalfPi;
			const float cosA = std::cos(angle);
			const float sinA = std::sin(angle);
			const Vector3 tipWorld = mastWorld + RotateByMatrix(rotMat, { cosA * kRotorTipRadius, kRotorTipDropY, sinA * kRotorTipRadius });
			const Vector3 tipVel = RotateByMatrix(rotMat, { -sinA, 0.0f, cosA }) * kRotorTipTangentSpd + downwashVec;

			AppParticle vp = MakeStyledParticle(
				tipWorld, tipVel, 0.0f, vaporScale, vaporScale * kVortexTargetMul, vaporLife, wispyTex, kRotorVortexStyle);
			vp.color.w = vaporAlpha;
			smokeMgr->AddParticle(vp);
		}
	}

	// 2. ツインタービンエンジンの黒煙排気トレイル
	engineExhaustTimer_ += deltaTime;
	if (engineExhaustTimer_ >= kExhaustInterval && smokeMgr && wispyTex != TextureManager::kInvalidTextureIndex)
	{
		engineExhaustTimer_ = 0.0f;
		const float exhaustSpeed = isFullThrottle ? kExhaustSpeedFull : kExhaustSpeedCruise;
		const float scBase       = isFullThrottle ? kExhaustScaleFull : kExhaustScaleCruise;
		const float alphaBase    = isFullThrottle ? kExhaustAlphaFull : kExhaustAlphaCruise;

		for (float sideF : kExhaustNozzleSides)
		{
			const Vector3 nozzlePos = LocalToWorldWithMatrix(heliPos_, rotMat, { sideF * kExhaustNozzleSideX, kExhaustNozzleY, kExhaustNozzleZ });
			const Vector3 exVel = RotateByMatrix(rotMat, { sideF * kExhaustVelSideX, kExhaustVelDownY, -exhaustSpeed })
				+ RandBoxVec3(kExhaustJitterXZ, kExhaustJitterYRange, kExhaustJitterXZ);

			AppParticle ex = MakeStyledParticle(
				nozzlePos, exVel, 0.0f, scBase, scBase * kExhaustTargetMul,
				RandRangeVec(kExhaustLifeRange), wispyTex, kEngineExhaustStyle);
			ex.color.w = alphaBase;
			smokeMgr->AddParticle(ex);
		}
	}

	// 3. 航空衝突防止ビーコン（尾翼レッド ＆ ベリーダブルストロボ）
	beaconTimer_ += deltaTime;
	const uint32_t flashTex = ResolveTex(scene_->starburstTextureIndex_, scene_->particleTextureB);
	const float gateWindow = deltaTime * kBeaconGateDtMul;

	if (std::fmod(beaconTimer_, kTailBeaconPeriod) < gateWindow)
	{
		scene_->appParticleManager_->EmitMuzzleFlare(
			scene_->particleManager->GetRandomEngine(),
			LocalToWorldWithMatrix(heliPos_, rotMat, kTailBeaconLocal),
			kTailBeaconScale, kTailBeaconColor, kTailBeaconLife, flashTex);
	}

	const float bellyCycle = std::fmod(beaconTimer_, kBellyStrobePeriod);
	if ((bellyCycle < gateWindow) || (bellyCycle >= kBellySecondPulseSec && bellyCycle < kBellySecondPulseSec + gateWindow))
	{
		scene_->appParticleManager_->EmitMuzzleFlare(
			scene_->particleManager->GetRandomEngine(),
			LocalToWorldWithMatrix(heliPos_, rotMat, kBellyStrobeLocal),
			kBellyStrobeScale, kBellyStrobeColor, kBellyStrobeLife, flashTex);
	}

	// 4. ローターダウンウォッシュ（煙柱への風圧渦 ＆ 地表砂塵リング ＆ 舞い散る木の葉）
	if (heliPos_.y < washHeightLimit && smokeMgr && wispyTex != TextureManager::kInvalidTextureIndex)
	{
		const float washRatio = std::clamp(
			1.0f - (heliPos_.y - kWashBaseHeightMin) / (std::max)(washHeightLimit - kWashBaseHeightMin, 1.0f),
			kWashRatioMin, 1.0f);
		smokeMgr->ApplyRotorDownwash(
			{ heliPos_.x, (std::min)(heliPos_.y * 0.5f, kWashCenterMaxY), heliPos_.z },
			kWashEffectRadius, kWashStrengthBase + kWashStrengthGain * washRatio, deltaTime);

		downwashDustTimer_ += deltaTime;
		if (downwashDustTimer_ >= kDownwashDustInterval)
		{
			downwashDustTimer_ = 0.0f;
			const Vector3 groundCenter = { heliPos_.x, kDownwashGroundY, heliPos_.z };

			for (int i = 0; i < kDownwashDustCount; ++i)
			{
				const float angle = RandRange(0.0f, kTwoPi);
				const float rad = RandRangeVec(kDownwashSpawnRadRange);
				const float dirX = std::cos(angle);
				const float dirZ = std::sin(angle);
				const float outSpd = (kDownwashOutSpdBase + kDownwashOutSpdGain * washRatio) * RandRangeVec(kDownwashSpdMulRange);
				const float sc = RandRangeVec(kDownwashScaleRange);

				const Vector3 dPos = groundCenter + Vector3{ dirX * rad, RandRangeVec(kDownwashPosYRange), dirZ * rad };
				const Vector3 dVel = { dirX * outSpd, RandRangeVec(kDownwashUpVelRange) * washRatio, dirZ * outSpd };
				AppParticle dust = MakeStyledParticle(
					dPos, dVel, 0.0f, sc, RandRangeVec(kDownwashTargetRange), RandRangeVec(kDownwashLifeRange), wispyTex, kDownwashDustStyle);
				dust.color.w = kDownwashBaseAlpha * washRatio;
				smokeMgr->AddParticle(dust);
			}

			if (Rand01() < kDownwashLeafChance)
			{
				const float leafAngle = RandRange(0.0f, kTwoPi);
				const float cosL = std::cos(leafAngle);
				const float sinL = std::sin(leafAngle);
				const Vector3 leafPos = groundCenter + Vector3{ cosL * kDownwashLeafSpawnRad, RandRangeVec(kDownwashLeafPosYRange), sinL * kDownwashLeafSpawnRad };
				const Vector3 leafVel = { cosL * RandRangeVec(kDownwashLeafSpdRange), RandRangeVec(kDownwashLeafUpRange), sinL * RandRangeVec(kDownwashLeafSpdRange) };
				const Vector4& leafCol = (Rand01() < kDownwashLeafAltChance) ? kLeafBrightGreen : kLeafOliveGreen;

				scene_->appParticleManager_->EmitSparkWithVelocity(
					scene_->particleManager->GetRandomEngine(), leafPos, leafVel, leafCol,
					kDownwashLeafScale, kDownwashLeafLife, ResolveTex(leafTexIndex_, scene_->particleTextureB));
			}
		}
	}
}

void HelicopterInsertionController::UpdateParticles(float deltaTime)
{
	const bool isHighG = (currentCut_ == kCutAct1 && timer_ >= kAct1HighGStartSec) || (currentCut_ == kCutAct5);
	const bool isFullThrottle = (currentCut_ == kCutAct5) || (currentCut_ == kCutAct1 && timer_ < kAct1ThrottleEndSec);
	const float washHeightLimit = (timer_ >= kInsertWashStartSec && timer_ < kCut4Duration + kInsertWashExtraEndSec)
		? kInsertWashHeightLimit
		: 0.0f;
	EmitHelicopterFlightParticles(deltaTime, isHighG, isFullThrottle, washHeightLimit);
}

void HelicopterInsertionController::UpdateExtractionPickup(float realDeltaTime)
{
	if (!scene_ || isActive_ || !scene_->allTargetsDestroyed_ || scene_->isDeathSequenceActive_)
	{
		if (!isActive_) rotorLoop_.Stop();
		isExtractionVisible_ = false;
		return;
	}

	isExtractionVisible_ = true;
	extractionApproachTimer_ += realDeltaTime;
	AdvanceWrappedAngle(mainRotorAngle_, kPickupMainRotorSpeed, realDeltaTime);
	AdvanceWrappedAngle(tailRotorAngle_, kPickupTailRotorSpeed, realDeltaTime);

	const Vector3 goalPos = scene_->goalRingTransform_.translate;
	const float approachT = EaseOutCubic(std::clamp(extractionApproachTimer_ / kPickupApproachDuration, 0.0f, 1.0f));

	// 1. 北東上空からのアプローチ ＆ パッド後方での警戒ホバリング軌道
	const Vector3 holdOrbitPos = goalPos + Vector3{
		kPickupHoldBaseOffset.x + std::sin(extractionApproachTimer_ * kPickupHoldOscFreq.x) * kPickupHoldOscAmp.x,
		kPickupHoldBaseOffset.y + std::cos(extractionApproachTimer_ * kPickupHoldOscFreq.y) * kPickupHoldOscAmp.y,
		kPickupHoldBaseOffset.z + std::cos(extractionApproachTimer_ * kPickupHoldOscFreq.z) * kPickupHoldOscAmp.z
	};
	const Vector3 basePos = LerpVec3(goalPos + kPickupSkyEntryOffset, holdOrbitPos, approachT);

	// 2. 脱出ゾーン滞在時〜クリア確定時の低空ピックアップ進入
	float targetDescent = 0.0f;
	if (scene_->isGameCleared_)
	{
		targetDescent = 1.0f;
	}
	else if (scene_->isPlayerInExtractionZone_)
	{
		const float extractProgress = 1.0f - std::clamp(scene_->extractionTimer_ / GamePlayScene::kExtractionMaxTime, 0.0f, 1.0f);
		targetDescent = kPickupZoneBaseDescent + kPickupZoneGainDescent * extractProgress;
	}

	const float blendSpeed = scene_->isGameCleared_ ? kPickupBlendSpeedCleared : kPickupBlendSpeedZone;
	pickupDescentBlend_ += (targetDescent - pickupDescentBlend_) * (std::min)(1.0f, blendSpeed * realDeltaTime);
	const float smoothDescent = SmoothStep01(pickupDescentBlend_);

	const float climaxSmooth = SmoothStep01(scene_->isGameCleared_
		? std::clamp(1.0f - (scene_->clearSlowMoTimer_ / GamePlayScene::kClearSlowMoDuration), 0.0f, 1.0f)
		: 0.0f);

	const Vector3 pickupHoverPos = goalPos + Vector3{
		kPickupHoverBaseOffset.x + kPickupClimaxDeltaOffset.x * climaxSmooth,
		kPickupHoverBaseOffset.y + kPickupClimaxDeltaOffset.y * climaxSmooth + std::sin(extractionApproachTimer_ * kPickupHoverBobFreq) * kPickupHoverBobAmpY,
		kPickupHoverBaseOffset.z + kPickupClimaxDeltaOffset.z * climaxSmooth
	};
	heliPos_ = LerpVec3(basePos, pickupHoverPos, smoothDescent);

	// 姿勢制御：北東からバンク旋回進入し、パッドへ右キャビン開放ドアを向ける
	const float approachBank = 1.0f - approachT;
	heliRot_ = {
		kPickupApproachPitch * approachBank + std::sin(smoothDescent * kPi) * kPickupFlarePulseAmp + kPickupClimaxPitchDelta * climaxSmooth,
		kPickupOrbitYaw * (1.0f - smoothDescent) + kPickupPadFacingYaw * smoothDescent,
		kPickupApproachRoll * approachBank + std::cos(extractionApproachTimer_ * kPickupRollOscFreq) * kPickupRollOscAmp
	};

	// スライドドア開放 ＆ 空間3Dサウンド
	const float doorOpenRatio = (std::max)(std::clamp((approachT - kPickupDoorOpenStartT) / kPickupDoorOpenSpanT, 0.0f, 1.0f), smoothDescent);
	doorSlideProgress_ = 1.0f - kPickupDoorTravel * SmoothStep01(doorOpenRatio);

	const Vector3 listenerPos = scene_->player_ ? scene_->player_->GetPosition() : goalPos;
	if (doorOpenRatio > kPickupDoorSoundTrigger && !extractionDoorSoundPlayed_)
	{
		extractionDoorSoundPlayed_ = true;
		TacticalAudio::Play3D("Resources/Audio/SE_Heli_Door.wav", heliPos_, listenerPos, kPickupDoorSoundVolume, kPickupDoorAudioMinDist, kPickupDoorAudioMaxDist);
	}

	const float atten = TacticalAudio::ComputeAttenuation(heliPos_, listenerPos, kPickupRotorAudioMinDist, kPickupRotorAudioMaxDist);
	rotorLoop_.Update("Resources/Audio/SE_Heli_Rotor_Loop.wav", kRotorLoopDurationSec, kPickupRotorVolume * atten, realDeltaTime);

	if (fastRope_)
	{
		fastRope_->SetScale(kZeroVec3);
		fastRope_->Update();
	}
	UpdateHelicopterCommonParts(kPickupRotorBlurOff, true, 1.0f);

	if (scene_->isGameCleared_) PlayDuckVoiceOnce(clearQuackPlayed_, duckSoundId_);

	EmitHelicopterFlightParticles(realDeltaTime, smoothDescent > kPickupHighGDescent, approachT < kPickupFullThrottleT, kPickupWashHeightLimit);
}

void HelicopterInsertionController::Draw3D(const RenderContext& ctx)
{
	if (!isActive_ && !isExtractionVisible_) return;

	if (heliShadow_)         heliShadow_->Draw(ctx);
	if (heliCabinInterior_)  heliCabinInterior_->Draw(ctx);
	if (heliBody_)           heliBody_->Draw(ctx);
	if (heliCargoDoor_)      heliCargoDoor_->Draw(ctx);
	if (heliMainRotorBlur2_) heliMainRotorBlur2_->Draw(ctx);
	if (heliMainRotorBlur1_) heliMainRotorBlur1_->Draw(ctx);
	if (heliMainRotor_)      heliMainRotor_->Draw(ctx);
	if (heliTailRotor_)      heliTailRotor_->Draw(ctx);
	if (isActive_ && fastRope_) fastRope_->Draw(ctx);
}

void HelicopterInsertionController::DrawUI()
{
	if (!isActive_ || !scene_) return;

	TacticalUICanvas* ui = TacticalUICanvas::GetInstance();
	if (!ui) return;
	TacticalDrawList* drawList = ui->GetForegroundDrawList();
	const UIVec2 disp = ui->GetDisplaySize();
	if (!drawList || disp.x <= 0.0f || disp.y <= 0.0f) return;

	const float uiScale = std::clamp(disp.x / kUiRefWidth, kUiScaleMin, kUiScaleMax);

	// 1. 控えめな光学シャッター＆着地ショックフラッシュ（ウォームアンバー寄り）
	const float totalFlash = (std::min)(cutFlashAlpha_ + impactFlashAlpha_, kMaxTotalFlashAlpha);
	if (totalFlash > kMinVisibleFlashAlpha)
	{
		drawList->AddRectFilled(UIVec2(0.0f, 0.0f), disp, PackColor(kColWarmFlashRgb, static_cast<int>(kAlphaMaxF * totalFlash)));
	}

	// 2. 21:9 シネマスコープ黒帯 ＆ シャープ作戦ヘッダー / スキップガイド
	const float barRatio = (currentCut_ == kCutAct5)
		? (std::max)(1.0f - SmoothStep01(((timer_ - kCut4Duration) / (kDuration - kCut4Duration)) * kLetterboxExitSpeed), 0.0f)
		: 1.0f;
	const float curBarH = (disp.y * kLetterboxHeightRatio) * barRatio;

	if (curBarH > 1.0f)
	{
		drawList->AddRectFilled(UIVec2(0.0f, 0.0f), UIVec2(disp.x, curBarH), kColLetterboxFill);
		drawList->AddRectFilled(UIVec2(0.0f, disp.y - curBarH), disp, kColLetterboxFill);
		drawList->AddLine(UIVec2(0.0f, curBarH), UIVec2(disp.x, curBarH),
			PackColor(kColTopAmberLineRgb, static_cast<int>(kTopAmberLineAlpha * barRatio)), kTopLineThickness);
		drawList->AddLine(UIVec2(0.0f, disp.y - curBarH), UIVec2(disp.x, disp.y - curBarH),
			PackColor(kColBottomSteelLineRgb, static_cast<int>(kBottomSteelLineAlpha * barRatio)), 1.0f);

		if (barRatio > kLetterboxContentMinRat)
		{
			DrawCineTextAligned(
				drawList, UIFontRole::TitleBoldJP, kHeaderFontSize * uiScale, disp.x * 0.5f, curBarH * 0.5f,
				PackColor(kColHeaderGoldRgb, static_cast<int>(kHeaderMaxAlpha * barRatio)),
				reinterpret_cast<const char*>(u8"OPERATION: IRON FEATHER // 強襲ヘリ降下作戦"), kAlignCenter);

			const float pulse = kSkipPulseBase + kSkipPulseAmp * std::sin(timer_ * kSkipPulseFreq);
			const int skA = static_cast<int>(kSkipMaxAlpha * pulse * barRatio);
			const char* skipTxt = reinterpret_cast<const char*>(u8"[SPACE] スキップ");
			const UIVec2 skSz = MeasureCineText(UIFontRole::SubBoldJP, kSkipFontSize * uiScale, skipTxt);
			const float skBoxW = skSz.x + (kSkipPadX * uiScale) * 2.0f;
			const float skBoxH = skSz.y + kSkipPadY * uiScale;
			const float skBoxX = disp.x - skBoxW - kRightEdgeMargin * uiScale;
			const float skBoxY = disp.y - curBarH * 0.5f - skBoxH * 0.5f;
			DrawFramedBox(drawList, skBoxX, skBoxY, skBoxW, skBoxH,
				PackColor(kColSkipBgRgb, static_cast<int>(kSkipBgMaxAlpha * barRatio)), PackColor(kColSkipBorderRgb, skA));
			DrawCineTextAligned(drawList, UIFontRole::SubBoldJP, kSkipFontSize * uiScale,
				skBoxX + skBoxW * 0.5f, disp.y - curBarH * 0.5f, PackColor(kColSkipTextRgb, skA), skipTxt, kAlignCenter);

			const float recX = kLeftEdgeMargin * uiScale;
			const float recY = disp.y - curBarH * 0.5f;
			if (std::fmod(timer_, 1.0f) < kRecBlinkDutyRatio)
			{
				drawList->AddCircleFilled(UIVec2(recX, recY), kRecDotRadius * uiScale,
					PackColor(kColRecDotRgb, static_cast<int>(kRecDotMaxAlpha * barRatio)));
			}
			DrawCineTextAligned(drawList, UIFontRole::SubBoldJP, kRecFontSize * uiScale, recX + kRecTextOffsetX * uiScale, recY,
				PackColor(kColRecTextRgb, static_cast<int>(kRecTextMaxAlpha * barRatio)), "REC // HELMET-CAM  [SECTOR-01 LZ]", kAlignLeft);
		}
	}

	// 3. 左上ライブ高度・経過時間テレメトリ
	if (timer_ < kCut4Duration + kTelemetryFadeTailSec)
	{
		const float teleAlpha = (timer_ > kCut4Duration)
			? std::clamp(1.0f - (timer_ - kCut4Duration) / kTelemetryFadeTailSec, 0.0f, 1.0f)
			: 1.0f;
		const float altMeters = (currentCut_ <= kCutAct2) ? heliPos_.y : (std::max)(0.0f, duckPos_.y);
		char teleBuf[kTelemetryBufSize];
		std::snprintf(teleBuf, sizeof(teleBuf), "ALT %.1fm   T+00:0%d", altMeters, (std::min)(kTelemetryMaxSingleSec, static_cast<int>(timer_)));
		DrawCineTextAligned(drawList, UIFontRole::SubBoldJP, kTelemetryFontSize * uiScale,
			kLeftEdgeMargin * uiScale, curBarH + kTelemetryOffsetY * uiScale,
			PackColor(kColTelemetryRgb, static_cast<int>(kTelemetryMaxAlpha * teleAlpha)), teleBuf, kAlignLeft);
	}

	// 4. 右上 RED LIGHT -> GREEN LIGHT タクティカルジャンプランプ
	if (timer_ >= kCut1Duration && timer_ < kCut2Duration + kLampExtraShowSec)
	{
		const float slideIn = SmoothStep01((timer_ - kCut1Duration) / kLampSlideInSec);
		const float badgeAlpha = (timer_ > kCut2Duration + kLampFadeStartOffsetSec)
			? std::clamp(1.0f - (timer_ - (kCut2Duration + kLampFadeStartOffsetSec)) / kLampFadeDurationSec, 0.0f, 1.0f)
			: 1.0f;
		const bool isGreen = (((timer_ - kCut1Duration) / (kCut2Duration - kCut1Duration)) >= kAct2PrepEndPhase);

		const char* lampTxt = isGreen
			? reinterpret_cast<const char*>(u8"GREEN LIGHT // 降下開始！")
			: reinterpret_cast<const char*>(u8"RED LIGHT // 降下待機");
		const UIVec2 lSz = MeasureCineText(UIFontRole::SubBoldJP, kLampFontSize * uiScale, lampTxt);
		const float bdgW = lSz.x + kLampWidthPad * uiScale;
		const float bdgH = kLampHeight * uiScale;
		const float bdgX = disp.x - bdgW - kRightEdgeMargin * uiScale + (1.0f - slideIn) * kLampSlideOffsetX * uiScale;
		const float bdgY = curBarH + kLampOffsetY * uiScale;

		const int bA = static_cast<int>(kLampMaxAlpha * badgeAlpha);
		const uint32_t bgC = PackColor(isGreen ? kColGreenLampBgRgb     : kColRedLampBgRgb,     bA);
		const uint32_t bdC = PackColor(isGreen ? kColGreenLampBorderRgb : kColRedLampBorderRgb, bA);
		const uint32_t txC = PackColor(isGreen ? kColGreenLampTextRgb   : kColRedLampTextRgb,   bA);

		DrawFramedBox(drawList, bdgX, bdgY, bdgW, bdgH, bgC, bdC, kLampBorderThickness);
		drawList->AddCircleFilled(UIVec2(bdgX + kLampDotOffsetX * uiScale, bdgY + bdgH * 0.5f), kLampDotRadius * uiScale, bdC, kLampDotSegments);
		DrawCineTextAligned(drawList, UIFontRole::SubBoldJP, kLampFontSize * uiScale,
			bdgX + kLampTextOffsetX * uiScale, bdgY + bdgH * 0.5f, txC, lampTxt, kAlignLeft);
	}

	// 5. 下部タクティカル無線字幕（データテーブル駆動）
	if (timer_ < kCut4Duration + kSubtitleEndExtraSec)
	{
		for (const RadioSubtitleCue& cue : kInsertionSubtitles)
		{
			if (timer_ >= cue.startTime && timer_ < cue.endTime)
			{
				const char* subText = reinterpret_cast<const char*>(cue.text);
				const float subAlpha = SmoothStep01((timer_ - cue.startTime) / cue.fadeInDuration)
					* SmoothStep01((cue.endTime - timer_) / cue.fadeOutDuration);
				if (subAlpha > kSubtitleMinAlpha)
				{
					const UIVec2 sSz = MeasureCineText(UIFontRole::SubBoldJP, kSubtitleFontSize * uiScale, subText);
					const float subW = sSz.x + kSubtitlePadW * uiScale;
					const float subH = sSz.y + kSubtitlePadH * uiScale;
					const float subX = (disp.x - subW) * 0.5f;
					const float subY = disp.y - curBarH - subH - kSubtitleMarginBottom * uiScale;
					DrawFramedBox(drawList, subX, subY, subW, subH,
						PackColor(kColSubtitleBgRgb, static_cast<int>(kSubtitleBgMaxAlpha * subAlpha)),
						PackColor(kColSubtitleBorderRgb, static_cast<int>(kSubtitleBorderMaxAlpha * subAlpha)));
					DrawCineTextAligned(drawList, UIFontRole::SubBoldJP, kSubtitleFontSize * uiScale,
						disp.x * 0.5f, subY + subH * 0.5f,
						PackColor(kColSubtitleTextRgb, static_cast<int>(kSubtitleTextMaxAlpha * subAlpha)), subText, kAlignCenter);
				}
				break;
			}
		}
	}
}
