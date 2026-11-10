#include "GameOverSequenceController.h"
#include "GameOverSequenceTuning.h"
#include "GamePlayScene.h"
#include "RaidStats.h"
#include "Application/Config/GameConfig.h"
#include "Application/Transition/StingerTransition.h"
#include "Application/Transition/DeathTransition.h"
#include "Camera.h"
#include "Application/Player/Player.h"
#include "Application/Enemy/Enemy.h"
#include "Application/Particle/AppParticleManager.h"
#include "Baziru3_Engine/Framework/Particle/ParticleManager.h"
#include "TextureManager.h"
#include "SceneManager.h"
#include "Baziru3_Engine/Framework/Audio/AudioManager.h"
#include "Application/Audio/TacticalAudio.h"
#include "Application/UI/TacticalUICanvas.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <iterator>

namespace
{
	using namespace GameOverSequenceTuning;
	using GameConfig::Math::kDegToRad;
	using GameConfig::Math::kHalfPi;
	using GameConfig::Math::kPi;
	using GameConfig::Math::kRadToDeg;
	using GameConfig::Math::kTwoPi;

	float Rand01() { return static_cast<float>(rand()) / static_cast<float>(RAND_MAX); }
	float RandRange(float a, float b) { return a + (b - a) * Rand01(); }
	float RandRangeVec(const Vector2& r) { return RandRange(r.x, r.y); }
	float EvalSpanOrConst(const Vector2& r) { return (r.x == r.y) ? r.x : RandRangeVec(r); }

	Vector3 RandBoxVec3(float halfXZ, const Vector2& yRange)
	{
		return { RandRange(-halfXZ, halfXZ), RandRangeVec(yRange), RandRange(-halfXZ, halfXZ) };
	}

	float EaseOutQuad(float t)
	{
		t = (std::clamp)(t, 0.0f, 1.0f);
		return t * (2.0f - t);
	}

	float EaseInOutCubic(float t)
	{
		t = (std::clamp)(t, 0.0f, 1.0f);
		if (t < 0.5f) return kEaseCubicFirstHalfScale * t * t * t;
		const float u = -2.0f * t + 2.0f;
		return 1.0f - (u * u * u) * 0.5f;
	}

	Vector3 LerpVector3(const Vector3& a, const Vector3& b, float t)
	{
		return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t };
	}

	Vector3 CalcLookAtRotation(const Vector3& eye, const Vector3& target, float roll = 0.0f)
	{
		const Vector3 dir = { target.x - eye.x, target.y - eye.y, target.z - eye.z };
		const float xzLen = std::sqrt(dir.x * dir.x + dir.z * dir.z);
		return { std::atan2(-dir.y, (std::max)(xzLen, kLookAtMinXzLen)), std::atan2(dir.x, dir.z), roll };
	}

	Vector3 EvalCamLocalOffset(const Vector3& anchor, const Vector3& flingDir, const Vector3& perp, const CamLocalOffset& off)
	{
		return anchor + flingDir * off.fling + perp * off.perp + Vector3{ 0.0f, off.height, 0.0f };
	}

	uint32_t ResolveTex(uint32_t preferred, uint32_t fallback)
	{
		return (preferred != UINT32_MAX) ? preferred : fallback;
	}

	uint32_t PackColor(const RgbColor& rgb, int alpha)
	{
		return UI_COL32(rgb.r, rgb.g, rgb.b, (std::clamp)(alpha, kAlphaMin, kAlphaMax));
	}

	float SampleEcgWave(float ecgTimer, const EcgWaveSpec& spec, float ampScale)
	{
		const float cycle = std::fmod(ecgTimer * spec.freqHz, 1.0f);
		if (cycle < spec.peakEnd)
		{
			return std::sin(cycle / spec.peakEnd * kPi) * (spec.peakAmp * ampScale);
		}
		if (cycle < spec.dipEnd)
		{
			return -std::sin((cycle - spec.peakEnd) / spec.dipSpan * kPi) * (spec.dipAmp * ampScale);
		}
		return 0.0f;
	}

	void EmitRadialDustBurst(
		AppParticleManager& mgr, std::mt19937& randEng, const Vector3& center, uint32_t tex, const RadialDustBurstSpec& spec)
	{
		const float step = kTwoPi / static_cast<float>(spec.count);
		for (int i = 0; i < spec.count; ++i)
		{
			float angle = static_cast<float>(i) * step;
			if (spec.angleJitter > 0.0f) angle += RandRange(-spec.angleJitter, spec.angleJitter);
			const float spd = EvalSpanOrConst(spec.speedRange);
			const float upVel = EvalSpanOrConst(spec.upVelRange);
			const Vector3 vel = { std::cos(angle) * spd, upVel, std::sin(angle) * spd };
			Vector3 pos = center + Vector3{ 0.0f, spec.originOffsetY, 0.0f };
			if (spec.posJitterXZ > 0.0f)
			{
				pos.x += RandRange(-spec.posJitterXZ, spec.posJitterXZ);
				pos.z += RandRange(-spec.posJitterXZ, spec.posJitterXZ);
			}
			mgr.EmitDustWithVelocity(randEng, pos, spec.scale, spec.color, vel, spec.life, tex);
		}
	}

	void DrawShadowedCenteredSubtitle(
		TacticalDrawList* drawList, const TacticalFont* font, float fontSize,
		const UIVec2& disp, float posYRatio, const RgbColor& textRgb, int alpha, const char* text)
	{
		if (!drawList || !font || !text || alpha <= kAlphaMin) return;
		const UIVec2 rSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);
		const float subX = (disp.x - rSize.x) * 0.5f;
		const float subY = disp.y * posYRatio - rSize.y * 0.5f;
		drawList->AddText(font, fontSize, UIVec2(subX + kSubtitleShadowOffset, subY + kSubtitleShadowOffset), PackColor(kColBlackRgb, alpha), text);
		drawList->AddText(font, fontSize, UIVec2(subX, subY), PackColor(textRgb, alpha), text);
	}

	std::unique_ptr<Object3d> CreateLitSceneObject(
		auto* object3dCom, Camera* camera, const Object3d::ModelData& modelData, const Vector3& scale, const Vector4& color)
	{
		auto obj = std::make_unique<Object3d>();
		obj->Initialize(object3dCom, modelData);
		obj->SetCamera(camera);
		obj->SetScale(scale);
		obj->SetColor(color);
		obj->SetEnableLighting(true);
		obj->SetAllowWireframeOverlay(false);
		return obj;
	}

	void UpdatePose(Object3d* obj, const Vector3& pos, const Vector3& rot)
	{
		obj->SetTranslate(pos);
		obj->SetRotate(rot);
		obj->Update();
	}

	void PoseKneelingMedic(Object3d* medic, const Vector3& pos, const Vector3& woundedPos)
	{
		const Vector3 toWounded = woundedPos - pos;
		UpdatePose(medic, pos, { kMedicKneelPitch, std::atan2(toWounded.x, toWounded.z), 0.0f });
	}
}

// -----------------------------------------------------------------------------
// 3Dモデル生成ヘルパー（吹き飛ぶライフル ＆ 軍用担架）
// -----------------------------------------------------------------------------

void GameOverSequenceController::BuildRifleModel(Object3d::ModelData& data)
{
	Player::BuildRifleModel(data);
}

void GameOverSequenceController::BuildStretcherModel(Object3d::ModelData& data)
{
	data.vertices.clear();
	data.indices.clear();

	const size_t boxCount = std::size(kStretcherBoxes);
	data.vertices.reserve(boxCount * kBoxFaceCount * kFaceVertCount);
	data.indices.reserve(boxCount * kBoxFaceCount * kFaceIndexCount);

	for (const BoxSpec& box : kStretcherBoxes)
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
				vd.texcoord = kQuadCornerUVs[v];
				vd.normal = kBoxFaceNormals[f];
				data.vertices.push_back(vd);
			}
			for (int idx = 0; idx < kFaceIndexCount; ++idx)
			{
				data.indices.push_back(fBase + kQuadTriIndices[idx]);
			}
		}
	}

	data.boundingRadius = kStretcherBoundingRadius;
}

// -----------------------------------------------------------------------------
// コントローラーライフサイクル
// -----------------------------------------------------------------------------

GameOverSequenceController::GameOverSequenceController(GamePlayScene* scene)
	: scene_(scene)
{
	ecgHistory_.reserve(kEcgHistoryCapacity);
	ecgHistory_.assign(kEcgHistoryMaxSamples, 0.0f);
}

GameOverSequenceController::~GameOverSequenceController()
{
	Finalize();
}

void GameOverSequenceController::Initialize()
{
	isActive_ = false;
	timer_ = 0.0f;
	transitionTriggered_ = false;
	groundImpactTriggered_ = false;
	smokeTimer_ = 0.0f;
	featherTimer_ = 0.0f;
	darkVignetteAlpha_ = 0.0f;
	fadeBlackAlpha_ = 0.0f;
	ecgTimer_ = 0.0f;
	medicWalkPhase_ = 0.0f;

	if (!scene_ || !scene_->object3dCom || !scene_->camera_) return;

	if (auto* tm = TextureManager::GetInstance())
	{
		auto loadByPath = [&](const char* path) -> uint32_t {
			tm->LoadTexture(path);
			return tm->GetTextureIndexByFilePath(path);
		};

		circle2TextureIndex_   = loadByPath("Resources/circle2.png");
		leafTextureIndex_      = loadByPath("Resources/leaf.png");
		vignetteTextureIndex_  = loadByPath("Resources/title_vignette.png");
		starburstTextureIndex_ = loadByPath("Resources/starburst.png");
		smokeDarkTextureIndex_ = loadByPath("Resources/smoke_dark.png");
		featherTextureIndex_   = tm->Load("Resources/feather.png");
		medicTexIndex_         = tm->Load("Resources/duck_medic.png");
		stretcherTexIndex_     = tm->Load("Resources/container_military.png");
		woundedDuckTexIndex_   = tm->Load("Resources/duck_player_wounded.png");
		normalTexIndex_        = tm->Load("Resources/duck.png");
	}

	const uint32_t containerTex = ResolveTex(stretcherTexIndex_, kFallbackTextureIndex);
	const uint32_t medicTex     = ResolveTex(medicTexIndex_, kFallbackTextureIndex);

	// 1. 吹き飛ぶライフルオブジェクトの生成（軍用ガンメタルブラック）
	Object3d::ModelData rifleData{};
	BuildRifleModel(rifleData);
	rifleData.material.textureFilePath = "Resources/container_military.png";
	rifleData.material.textureIndex = containerTex;
	droppedRifle_ = CreateLitSceneObject(scene_->object3dCom, scene_->camera_, rifleData, kUnitScale, kDroppedRifleColor);

	// 2. 衛生兵アヒル（前方・後方）と担架ベッドの生成（GameOverSceneのヘリ搬送に直結）
	Object3d::ModelData medicData = Object3d::LoadObjFile("Resources", "player.obj");
	medicData.material.textureFilePath = "Resources/duck_medic.png";
	medicData.material.textureIndex = medicTex;
	medicData.boundingRadius = kMedicBoundingRadius;
	medicFront_ = CreateLitSceneObject(scene_->object3dCom, scene_->camera_, medicData, kMedicModelScale, kUnitWhiteColor);
	medicRear_  = CreateLitSceneObject(scene_->object3dCom, scene_->camera_, medicData, kMedicModelScale, kUnitWhiteColor);

	Object3d::ModelData stretcherData{};
	BuildStretcherModel(stretcherData);
	stretcherData.material.textureFilePath = "Resources/container_military.png";
	stretcherData.material.textureIndex = containerTex;
	stretcherBed_ = CreateLitSceneObject(scene_->object3dCom, scene_->camera_, stretcherData, kStretcherModelScale, kUnitWhiteColor);

	// オーディオのロード（死亡時アラーム音は廃止し、アヒルボイスのみ保持）
	if (SceneManager::GetInstance() && SceneManager::GetInstance()->GetAudioManager())
	{
		alarmSoundId_ = -1;
		duckSoundId_ = SceneManager::GetInstance()->GetAudioManager()->Load("Resources/Audio/SE_Duck.mp3");
	}
}

void GameOverSequenceController::StopFlatlineAudio()
{
	if (flatlinePlayId_ != -1)
	{
		if (auto* am = TacticalAudio::GetAudio()) am->Stop(flatlinePlayId_);
		flatlinePlayId_ = -1;
	}
}

void GameOverSequenceController::Finalize()
{
	isActive_ = false;
	StopFlatlineAudio();

	if (scene_ && scene_->GetPlayer() && normalTexIndex_ != UINT32_MAX)
	{
		scene_->GetPlayer()->SetTextureIndex(normalTexIndex_);
	}
	if (scene_ && scene_->camera_)
	{
		scene_->camera_->SetFovY(initialCamFov_);
	}
	droppedRifle_.reset();
	medicFront_.reset();
	medicRear_.reset();
	stretcherBed_.reset();
}

void GameOverSequenceController::Start(
	const Vector3& deathPos, const Vector3& killerPos, const std::string& killerName, const std::string& causeOfDeath, bool isMIA)
{
	isActive_ = true;
	timer_ = 0.0f;
	deathPos_ = deathPos;
	killerPos_ = killerPos;
	killerName_ = killerName.empty() ? "HOSTILE SENTRY (SVD SNIPER)" : killerName;
	causeOfDeath_ = causeOfDeath.empty() ? "7.62x54mmR AP // CHEST VITAL PENETRATION" : causeOfDeath;
	isMIA_ = isMIA;
	transitionTriggered_ = false;
	groundImpactTriggered_ = false;
	alarmTriggered_ = false;
	StopFlatlineAudio();
	flatlineTriggered_ = false;
	rifleBounceSoundPlayed_ = false;
	prevEcgCycle_ = -1;
	prevMedicStep_ = -1;
	smokeTimer_ = 0.0f;
	featherTimer_ = 0.0f;
	darkVignetteAlpha_ = kInitialVignetteAlpha;
	fadeBlackAlpha_ = 0.0f;
	medicWalkPhase_ = 0.0f;

	// 負傷アヒルテクスチャへの即時切り替え（GameOverSceneの負傷姿と完全に統一）
	if (scene_ && scene_->GetPlayer() && woundedDuckTexIndex_ != UINT32_MAX)
	{
		originalPlayerTexIndex_ = scene_->GetPlayer()->GetTextureIndex();
		scene_->GetPlayer()->SetTextureIndex(woundedDuckTexIndex_);
	}

	// 被弾赤フラッシュの即時キル（眩しい光や赤みを完全にカット）
	if (scene_) scene_->vignetteAlpha_ = 0.0f;

	// キラーからのノックバック方向ベクトル
	const Vector3 toKiller = killerPos_ - deathPos_;
	killerDistance_ = std::sqrt(toKiller.x * toKiller.x + toKiller.z * toKiller.z);
	const float rad = std::atan2(toKiller.x, toKiller.z);
	float deg = rad * kRadToDeg;
	if (deg < 0.0f) deg += kFullCircleDeg;
	killerBearingDeg_ = deg;

	flingDir_ = (killerDistance_ > kMinKillerDistance)
		? Vector3{ -toKiller.x / killerDistance_, 0.0f, -toKiller.z / killerDistance_ }
		: Vector3{ 0.0f, 0.0f, -1.0f };

	duckPos_ = deathPos_;
	duckRot_ = { 0.0f, rad, 0.0f };
	duckScale_ = kUnitScale;

	// 吹き飛ぶライフルの初期化（被弾衝撃で手から上空斜め前へ弾き飛びスピン）
	const Vector3 perp = { -flingDir_.z, 0.0f, flingDir_.x };
	riflePos_ = (scene_ && scene_->GetPlayer()) ? scene_->GetPlayer()->GetRiflePosition() : (deathPos_ + kFallbackRifleOffset);
	rifleRot_ = { kInitialRiflePitch, rad, 0.0f };
	rifleVel_ = flingDir_ * kRifleFlingForwardSpd - perp * kRifleFlingSideSpd + Vector3{ 0.0f, kRifleFlingUpSpd, 0.0f };
	rifleRotVel_ = { RandRangeVec(kRifleSpinRangeX), RandRangeVec(kRifleSpinRangeY), RandRangeVec(kRifleSpinRangeZ) };
	rifleGrounded_ = false;

	// 衛生兵チームのスタート位置と目標位置（GameOverSceneのヘリ搬送チームに接続）
	const Vector3 duckFinalPos = deathPos_ + flingDir_ * kDuckFinalSlideDist;
	medicTeamTargetPos_ = duckFinalPos + perp * kMedicTargetSideDist;
	medicTeamStartPos_ = medicTeamTargetPos_ - flingDir_ * kMedicStartBackDist + perp * kMedicStartSideDist;
	medicTeamPos_ = medicTeamStartPos_;

	if (scene_ && scene_->camera_)
	{
		initialCamPos_ = scene_->camera_->GetTranslate();
		initialCamRot_ = scene_->camera_->GetRotate();
		initialCamFov_ = scene_->camera_->GetFovY();
	}

	// 致命弾インパクト！ 特撮映画調の重いヒットストップ＆カメラ激震（光なし、重厚な物理衝撃）
	if (scene_)
	{
		scene_->TriggerCameraShake(kHitShakeDuration, kHitShakeIntensity);
		scene_->TriggerHitStop(kHitStopDuration);
	}

	TacticalAudio::Play2D("Resources/Audio/SE_KIA_Impact.wav", kKiaImpactVolume);
	if (duckSoundId_ >= 0 && SceneManager::GetInstance() && SceneManager::GetInstance()->GetAudioManager())
	{
		SceneManager::GetInstance()->GetAudioManager()->PlayOneShot(duckSoundId_, kDuckVoiceVolume);
	}

	EmitStartImpactParticles();
}

void GameOverSequenceController::EmitStartImpactParticles()
{
	if (!scene_ || !scene_->particleManager || !scene_->appParticleManager_) return;

	auto& randEng = scene_->particleManager->GetRandomEngine();
	const uint32_t darkSmokeTex = ResolveTex(smokeDarkTextureIndex_, scene_->smokeTextureIndex_);
	const uint32_t fTex = ResolveTex(featherTextureIndex_, scene_->particleTextureB);
	const Vector3 chestPos = duckPos_ + Vector3{ 0.0f, kChestBurstHeight, 0.0f };

	// 1. 超音速弾丸の貫通トレース ＆ 2. 胸部羽毛バースト ＆ 3. 足元衝撃波ダストリング
	for (int i = 0; i < kHitStreakCount; ++i)
	{
		const float speed = RandRangeVec(kHitStreakSpeedRange);
		const Vector3 streakVel = flingDir_ * speed + Vector3{
			RandRange(-kHitStreakJitterXZ, kHitStreakJitterXZ),
			RandRange(-kHitStreakJitterY, kHitStreakJitterY),
			RandRange(-kHitStreakJitterXZ, kHitStreakJitterXZ)
		};
		scene_->appParticleManager_->EmitDustWithVelocity(randEng, chestPos, kHitStreakScale, kHitStreakColor, streakVel, kHitStreakLife, darkSmokeTex);
	}
	for (int i = 0; i < kHitFeatherCount; ++i)
	{
		scene_->appParticleManager_->EmitFeather(randEng, chestPos, kHitFeatherColor, fTex);
	}
	EmitRadialDustBurst(*scene_->appParticleManager_, randEng, duckPos_, darkSmokeTex, kHitFootRingBurst);
}

void GameOverSequenceController::Update(float realDeltaTime)
{
	if (!isActive_) return;

	// 映画的バレットタイム・スローモーション演出 (Bullet-Time Time Dilation)
	float timeScale = 1.0f;
	if (timer_ < kBulletTimeSlowEnd)
	{
		timeScale = kBulletTimeMinScale;
	}
	else if (timer_ < kGroundImpactTime)
	{
		const float u = (timer_ - kBulletTimeSlowEnd) / kBulletTimeRecoverSpan;
		timeScale = kBulletTimeMinScale + kBulletTimeAccelGain * (u * u);
	}

	const float simDelta = realDeltaTime * timeScale;
	timer_ += simDelta;
	fadeBlackAlpha_ = 0.0f;

	UpdateMotion(simDelta);
	UpdateWeaponMotion(simDelta);
	UpdateAtmosphere(simDelta);
	UpdateCamera(simDelta);
	UpdateParticles(simDelta);

	// 心電図波形の視覚更新（アラーム・電子ビープ音は鳴らさない）
	ecgTimer_ += simDelta;
	float currentPulse = 0.0f;
	if (timer_ < kEcgTachyEndTime)
	{
		currentPulse = SampleEcgWave(ecgTimer_, kEcgTachySpec, 1.0f);
	}
	else if (timer_ < kAct2Duration)
	{
		const float decay = 1.0f - (timer_ - kEcgTachyEndTime) / (kAct2Duration - kEcgTachyEndTime);
		currentPulse = SampleEcgWave(ecgTimer_, kEcgBradySpec, decay);
	}
	else
	{
		flatlineTriggered_ = true;
	}

	ecgHistory_.push_back(currentPulse);
	if (ecgHistory_.size() > kEcgHistoryMaxSamples) ecgHistory_.erase(ecgHistory_.begin());

	if ((IsAppKeyDown(VK_SPACE) || IsAppKeyDown(VK_RETURN)) && timer_ > kSkipEnableTime)
	{
		Skip();
		return;
	}
	if (timer_ >= kDuration && !transitionTriggered_)
	{
		FinishSequence();
	}
}

// -----------------------------------------------------------------------------
// 3Dアヒル兵士の物理アニメーション（吹き飛び・激突・バウンド・滑走）
// -----------------------------------------------------------------------------

void GameOverSequenceController::EmitGroundImpactParticles()
{
	if (!scene_ || !scene_->particleManager || !scene_->appParticleManager_) return;

	auto& randEng = scene_->particleManager->GetRandomEngine();
	const uint32_t darkSmokeTex = ResolveTex(smokeDarkTextureIndex_, scene_->smokeTextureIndex_);
	const uint32_t fTex = ResolveTex(featherTextureIndex_, scene_->particleTextureB);

	// 衝撃波ダストリング・泥煙雲・砂利破片 ＆ 散らばる羽毛
	for (const RadialDustBurstSpec& burst : kGroundImpactDustBursts)
	{
		EmitRadialDustBurst(*scene_->appParticleManager_, randEng, duckPos_, darkSmokeTex, burst);
	}
	for (int i = 0; i < kImpactFeatherCount; ++i)
	{
		scene_->appParticleManager_->EmitFeather(randEng, duckPos_ + RandBoxVec3(kImpactFeatherSpreadXZ, kImpactFeatherYRange), kImpactFeatherColor, fTex);
	}
}

void GameOverSequenceController::UpdateMotion(float realDeltaTime)
{
	if (!scene_ || !scene_->GetPlayer()) return;
	Object3d* obj = scene_->GetPlayer()->GetObject3d();
	if (!obj) return;

	const float baseYaw = killerBearingDeg_ * kDegToRad;

	if (timer_ < kGroundImpactTime)
	{
		// 1. 【致命弾インパクト・胸部スナップ＆空中放物線吹き飛び＆3Dラグドール捻転】 (0.0s 〜 0.38s)
		const float t = timer_ / kGroundImpactTime;
		const float jumpArc = kAirJumpArcHeight * (kParabolaPeakFactor * t * (1.0f - t));
		const float oneMinusT = 1.0f - t;
		const float horizDist = kAirKnockbackDist * (1.0f - oneMinusT * oneMinusT);

		duckPos_ = deathPos_ + flingDir_ * horizDist + Vector3{ 0.0f, kAirBaseLiftY + jumpArc, 0.0f };
		duckRot_ = {
			-EaseOutQuad((std::min)(1.0f, t * kAirPitchTimeScale)) * kPronePitchRad,
			baseYaw - std::sin(t * kHalfPi) * kAirYawSpinAmp,
			t * kAirRollAmp
		};

		if (t < kAirSquashEndT)
		{
			const float impactSquash = std::sin(t / kAirSquashEndT * kPi);
			const float xzScale = kAirSquashBaseXZ + impactSquash * kAirSquashGainXZ;
			duckScale_ = { xzScale, kAirSquashBaseY - impactSquash * kAirSquashLossY, xzScale };
		}
		else
		{
			const float airStretch = std::sin((t - kAirSquashEndT) / kAirStretchSpanT * kPi);
			duckScale_ = { kAirStretchBaseXZ, kAirStretchBaseY + airStretch * kAirStretchGainY, kAirStretchBaseXZ };
		}
	}
	else if (timer_ < kAct1Duration)
	{
		// 2. 【地面激突インパクト＆強烈な摩擦スライディング＆慣性バウンド】 (0.38s 〜 0.85s)
		const float t = (timer_ - kGroundImpactTime) / kSkidPhaseSpan;

		if (!groundImpactTriggered_)
		{
			groundImpactTriggered_ = true;
			TacticalAudio::Play2D("Resources/Audio/SE_Body_Collapse.wav", kBodyCollapseVolume);
			scene_->TriggerCameraShake(kGroundShakeDuration, kGroundShakeIntensity);
			EmitGroundImpactParticles();
		}

		const float bounceArc = (t < kMicroBounceEndT) ? (std::sin((t / kMicroBounceEndT) * kPi) * kMicroBounceHeight) : 0.0f;
		const float invT = 1.0f - t;
		const float skidEase = 1.0f - (invT * invT * invT);
		duckPos_ = deathPos_ + flingDir_ * (kAirKnockbackDist + kSkidExtraDist * skidEase) + Vector3{ 0.0f, kGroundRestY + bounceArc, 0.0f };

		// スライディング中の地面擦過ダスト
		static float skidDustTimer = 0.0f;
		skidDustTimer += realDeltaTime;
		if (t < kSkidDustEndT && skidDustTimer >= kSkidDustInterval && scene_->particleManager && scene_->appParticleManager_)
		{
			skidDustTimer = 0.0f;
			const uint32_t darkSmokeTex = ResolveTex(smokeDarkTextureIndex_, scene_->smokeTextureIndex_);
			const Vector3 dVel = flingDir_ * kSkidDustBackVel + RandBoxVec3(kSkidDustJitterXZ, kSkidDustUpRange);
			scene_->appParticleManager_->EmitDustWithVelocity(
				scene_->particleManager->GetRandomEngine(),
				duckPos_ + Vector3{ 0.0f, kSkidDustY, 0.0f }, kSkidDustScale, kSkidDustColor, dVel, kSkidDustLife, darkSmokeTex);
		}

		const float neckWhip = (t < kNeckWhipEndT) ? std::sin(t / kNeckWhipEndT * kPi) * kNeckWhipAmp : 0.0f;
		duckRot_ = {
			-kPronePitchRad - neckWhip,
			baseYaw - kAirYawSpinAmp * (1.0f - t * kSkidYawRelaxRatio),
			kAirRollAmp - kSkidRollSettle * skidEase
		};

		if (t < kGroundSquashEndT)
		{
			const float impactSquash = std::sin(t / kGroundSquashEndT * kPi);
			const float xzScale = kGroundSquashBaseXZ + impactSquash * kGroundSquashGainXZ;
			duckScale_ = { xzScale, kGroundSquashBaseY - impactSquash * kGroundSquashLossY, xzScale };
		}
		else
		{
			duckScale_ = kUnitScale;
		}
	}
	else
	{
		// 3. 【戦場に横たわる兵士・微弱な呼吸】 (0.85s 以降)
		duckPos_ = deathPos_ + flingDir_ * kDuckFinalSlideDist;
		duckPos_.y = kGroundRestY;
		duckRot_ = { -kPronePitchRad, baseYaw - kProneYawOffset, kProneRollRad };

		const float breath = std::sin((timer_ - kAct1Duration) * kProneBreathFreq) * kProneBreathAmp;
		duckScale_ = { kProneScaleXZ + breath, kProneScaleY + breath * 0.5f, kProneScaleXZ };
	}

	obj->SetScale(duckScale_);
	obj->SetColor(kWoundedDuckTint);
	UpdatePose(obj, duckPos_, duckRot_);

	UpdateMedicTeamMotion(realDeltaTime);
}

void GameOverSequenceController::UpdateMedicTeamMotion(float realDeltaTime)
{
	// 4. 【衛生兵レスキューチームの駆けつけ】 (1.60s 以降、GameOverSceneのヘリ搬送に直結)
	if (timer_ < kMedicAppearTime || !medicFront_ || !medicRear_ || !stretcherBed_) return;

	medicWalkPhase_ += realDeltaTime * kMedicWalkSpeed;

	const Vector3 toDuck = medicTeamTargetPos_ - medicTeamStartPos_;
	const float dist = std::sqrt(toDuck.x * toDuck.x + toDuck.z * toDuck.z);
	const Vector3 forward = (dist > kMinDirectionDist)
		? Vector3{ toDuck.x / dist, 0.0f, toDuck.z / dist }
		: Vector3{ 1.0f, 0.0f, 0.0f };
	const float yaw = std::atan2(forward.x, forward.z);

	if (timer_ < kMedicRunEndTime)
	{
		medicTeamPos_ = LerpVector3(medicTeamStartPos_, medicTeamTargetPos_, EaseOutQuad((timer_ - kMedicAppearTime) / kMedicRunDuration));

		const float sinWalk = std::sin(medicWalkPhase_);
		const float bob = std::fabs(sinWalk) * kMedicBobAmp;
		const float waddle = sinWalk * kMedicWaddleAmp;

		const int medicStep = static_cast<int>(medicWalkPhase_ / kPi);
		if (medicStep != prevMedicStep_)
		{
			prevMedicStep_ = medicStep;
			TacticalAudio::PlayRandom2D(
				{ "Resources/Audio/SE_Step_Dirt_1.wav", "Resources/Audio/SE_Step_Dirt_2.wav" },
				kMedicStepVolume, kMedicStepCooldownMs, "gameover_medic_step");
		}

		UpdatePose(medicRear_.get(),    medicTeamPos_ - forward * kMedicArmOffset + Vector3{ 0.0f, bob, 0.0f }, { 0.0f, yaw, waddle });
		UpdatePose(medicFront_.get(),   medicTeamPos_ + forward * kMedicArmOffset + Vector3{ 0.0f, bob, 0.0f }, { 0.0f, yaw, -waddle });
		UpdatePose(stretcherBed_.get(), medicTeamPos_ + Vector3{ 0.0f, kStretcherCarryY + bob * 0.5f, 0.0f },   { 0.0f, yaw + kHalfPi, 0.0f });
	}
	else
	{
		medicTeamPos_ = medicTeamTargetPos_;
		UpdatePose(stretcherBed_.get(), medicTeamPos_ + Vector3{ 0.0f, kStretcherGroundY, 0.0f }, { 0.0f, yaw + kHalfPi, 0.0f });

		const Vector3 kneelOff = { 0.0f, kMedicKneelOffsetY, 0.0f };
		PoseKneelingMedic(medicRear_.get(),  medicTeamPos_ - forward * kMedicArmOffset + kneelOff, duckPos_);
		PoseKneelingMedic(medicFront_.get(), medicTeamPos_ + forward * kMedicArmOffset + kneelOff, duckPos_);
	}
}

// -----------------------------------------------------------------------------
// 吹き飛ぶライフル（武器）の物理運動 ＆ 環境パーティクル
// -----------------------------------------------------------------------------

void GameOverSequenceController::UpdateWeaponMotion(float realDeltaTime)
{
	if (!droppedRifle_) return;

	if (!rifleGrounded_)
	{
		rifleVel_.y -= kRifleGravity * realDeltaTime;
		riflePos_ += rifleVel_ * realDeltaTime;
		rifleRot_ += rifleRotVel_ * realDeltaTime;

		if (riflePos_.y <= kRifleGroundY)
		{
			riflePos_.y = kRifleGroundY;
			rifleVel_.y = -rifleVel_.y * kRifleRestitutionY;
			rifleVel_.x *= kRifleFrictionXZ;
			rifleVel_.z *= kRifleFrictionXZ;
			rifleRotVel_ = rifleRotVel_ * kRifleSpinDamping;

			if (!rifleBounceSoundPlayed_)
			{
				rifleBounceSoundPlayed_ = true;
				TacticalAudio::Play2D("Resources/Audio/SE_Weapon_Holster.wav", kRifleBounceVolume);
			}

			if (scene_ && scene_->particleManager && scene_->appParticleManager_)
			{
				auto& randEng = scene_->particleManager->GetRandomEngine();
				const uint32_t darkSmokeTex = ResolveTex(smokeDarkTextureIndex_, scene_->smokeTextureIndex_);
				for (int i = 0; i < kRifleDustCount; ++i)
				{
					scene_->appParticleManager_->EmitDustWithVelocity(
						randEng, riflePos_, kRifleDustScale, kRifleDustColor,
						RandBoxVec3(kRifleDustJitterXZ, kRifleDustUpRange), kRifleDustLife, darkSmokeTex);
				}
			}

			if (std::abs(rifleVel_.y) < kRifleRestVelLimit)
			{
				rifleGrounded_ = true;
				rifleVel_ = { 0.0f, 0.0f, 0.0f };
			}
		}
	}

	droppedRifle_->SetScale(kUnitScale);
	UpdatePose(droppedRifle_.get(), riflePos_, rifleRot_);
}

void GameOverSequenceController::UpdateAtmosphere(float realDeltaTime)
{
	if (!scene_ || !scene_->appParticleManager_) return;

	const uint32_t darkSmokeTex = ResolveTex(smokeDarkTextureIndex_, scene_->smokeTextureIndex_);

	// 1. 被弾部位から立ち上る繊細な硝煙（顔を覆い隠さないよう胸部後方から奥へ逃がす）
	if (timer_ >= kWoundSmokeStartTime)
	{
		smokeTimer_ += realDeltaTime;
		if (smokeTimer_ >= kWoundSmokeInterval && scene_->particleManager)
		{
			smokeTimer_ = 0.0f;
			const Vector3 perp = { -flingDir_.z, 0.0f, flingDir_.x };
			const Vector3 sPos = duckPos_ - flingDir_ * kWoundSmokeBackOff + perp * kWoundSmokeSideOff + Vector3{
				RandRange(-kWoundSmokePosJitXZ, kWoundSmokePosJitXZ), kWoundSmokePosY, RandRange(-kWoundSmokePosJitXZ, kWoundSmokePosJitXZ)
			};
			const Vector3 sVel = flingDir_ * kWoundSmokeVelFling + perp * kWoundSmokeVelSide + RandBoxVec3(kWoundSmokeVelJitXZ, kWoundSmokeUpRange);
			scene_->appParticleManager_->EmitDustWithVelocity(
				scene_->particleManager->GetRandomEngine(), sPos, kWoundSmokeScale, kWoundSmokeColor, sVel, kWoundSmokeLife, darkSmokeTex);
		}
	}

	// 2. 接近する救難ヘリのローター下降気流（Rotor Downwash Wind）
	if (timer_ >= kDownwashStartTime && timer_ < kDownwashEndTime && scene_->particleManager)
	{
		static float downwashTimer = 0.0f;
		downwashTimer += realDeltaTime;
		if (downwashTimer >= kDownwashInterval)
		{
			downwashTimer = 0.0f;
			const Vector3 wPos = duckPos_ + Vector3{
				RandRange(-kDownwashSpreadXZ, kDownwashSpreadXZ), kDownwashPosY, RandRange(-kDownwashSpreadXZ, kDownwashSpreadXZ)
			};
			const Vector3 wVel = { RandRangeVec(kDownwashVelXRange), RandRangeVec(kDownwashVelYRange), RandRangeVec(kDownwashVelZRange) };
			scene_->appParticleManager_->EmitDustWithVelocity(
				scene_->particleManager->GetRandomEngine(), wPos, kDownwashScale, kDownwashColor, wVel, kDownwashLife, darkSmokeTex);
		}
	}

	// 3. 空中をゆっくり舞い散る羽毛
	featherTimer_ += realDeltaTime;
	if (featherTimer_ >= kDriftFeatherInterval && timer_ < kAct3Duration && scene_->particleManager)
	{
		featherTimer_ = 0.0f;
		const uint32_t fTex = ResolveTex(featherTextureIndex_, scene_->particleTextureB);
		scene_->appParticleManager_->EmitFeather(
			scene_->particleManager->GetRandomEngine(),
			duckPos_ + RandBoxVec3(kDriftFeatherSpreadXZ, kDriftFeatherYRange), kDriftFeatherColor, fTex);
	}
}

// -----------------------------------------------------------------------------
// カメラワーク（重厚な地面ローアングル ＆ 衛生兵駆けつけフォーカス ＆ シネマ暗転）
// -----------------------------------------------------------------------------

void GameOverSequenceController::UpdateCamera(float /*realDeltaTime*/)
{
	if (!scene_ || !scene_->camera_) return;

	const Vector3 perp = { -flingDir_.z, 0.0f, flingDir_.x };
	const Vector3 duckLandingPos = deathPos_ + flingDir_ * kDuckFinalSlideDist;

	Vector3 camPos{};
	Vector3 lookTarget{};
	float camRoll = 0.0f;
	float camFov = initialCamFov_;

	if (timer_ < kGroundImpactTime)
	{
		// 【Phase 1: 致命被弾インパクト＆前方斜め3/4ダイナミック・スムーズトラッキングドリー】 (0.0s 〜 0.38s)
		const float u = timer_ / kGroundImpactTime;
		const float invU = 1.0f - u;
		const float smoothDolly = 1.0f - (invU * invU * invU);

		camPos = LerpVector3(initialCamPos_, EvalCamLocalOffset(duckLandingPos, flingDir_, perp, kCamPhase1Target), smoothDolly);
		const float shock = 1.0f - u;
		camPos += perp * (std::sin(timer_ * kCamMicroShakeFreqX) * kCamMicroShakeAmpX * shock);
		camPos.y += std::cos(timer_ * kCamMicroShakeFreqY) * kCamMicroShakeAmpY * shock;

		lookTarget = duckPos_ + Vector3{ 0.0f, kCamPhase1LookY, 0.0f };
		camRoll = kCamDutchRollRad * EaseOutQuad(u);
		camFov = initialCamFov_ + (kCamPhase1TargetFov - initialCamFov_) * smoothDolly;
	}
	else if (timer_ < kAct1Duration)
	{
		// 【Phase 2: 地面激突インパクト＆バウンド滑走・顔フォーカス】 (0.38s 〜 0.85s)
		const float u = (timer_ - kGroundImpactTime) / kSkidPhaseSpan;
		const float ease = EaseOutQuad(u);
		const CamLocalOffset driftedOff = {
			kCamPhase1Target.fling + kCamPhase2Drift.fling * ease,
			kCamPhase1Target.height + kCamPhase2Drift.height * ease,
			kCamPhase1Target.perp + kCamPhase2Drift.perp * ease
		};
		const float slamDecay = (std::max)(0.0f, 1.0f - u * kCamSlamDecayRate);
		camPos = EvalCamLocalOffset(duckLandingPos, flingDir_, perp, driftedOff);
		camPos.y -= std::sin(u * kPi * kCamSlamFreqFactor) * kCamSlamAmpY * slamDecay;

		lookTarget = duckPos_ + Vector3{ 0.0f, kCamPhase2LookY, 0.0f };
		camRoll = kCamDutchRollRad * (1.0f - ease);
		camFov = kCamPhase1TargetFov - kCamPhase2FovDrop * ease;
	}
	else if (timer_ < kAct2Duration)
	{
		// 【Phase 3: 倒れ込み静止・無念のアヒル表情シネマティックオービット旋回】 (0.85s 〜 1.70s)
		const float ease = EaseInOutCubic((timer_ - kAct1Duration) / (kAct2Duration - kAct1Duration));
		camPos = LerpVector3(
			EvalCamLocalOffset(duckLandingPos, flingDir_, perp, kCamPhase3Start),
			EvalCamLocalOffset(duckLandingPos, flingDir_, perp, kCamPhase3End), ease);
		camPos.y += std::sin((timer_ - kAct1Duration) * kCamBreathFreq) * kCamBreathAmpY;

		lookTarget = duckLandingPos + Vector3{ 0.0f, kCamPhase3LookY, 0.0f };
		camFov = kCamPhase3StartFov - kCamPhase3FovDrop * ease;
	}
	else if (timer_ < kAct3Duration)
	{
		// 【Phase 4: 救難チーム駆けつけ＆担架搬送ワイドシネマ構図】 (1.70s 〜 2.80s)
		const float ease = EaseInOutCubic((timer_ - kAct2Duration) / (kAct3Duration - kAct2Duration));
		camPos = LerpVector3(
			EvalCamLocalOffset(duckLandingPos, flingDir_, perp, kCamPhase3End),
			EvalCamLocalOffset(duckLandingPos, flingDir_, perp, kCamPhase4End), ease);

		const Vector3 teamCenter = (duckLandingPos + medicTeamPos_) * 0.5f;
		lookTarget = LerpVector3(
			duckLandingPos + Vector3{ 0.0f, kCamPhase3LookY, 0.0f },
			teamCenter + Vector3{ 0.0f, kCamTeamLookY, 0.0f }, ease * kCamPhase4LookBlend);
		camFov = kCamPhase4StartFov + kCamPhase4FovGain * ease;
	}
	else
	{
		// 【Phase 5: 担架収容・ヘビーシャッター閉鎖待機ワイド構図】 (2.80s 以降)
		camPos = EvalCamLocalOffset(duckLandingPos, flingDir_, perp, kCamPhase4End);
		lookTarget = (duckLandingPos + medicTeamPos_) * 0.5f + Vector3{ 0.0f, kCamTeamLookY, 0.0f };
		camFov = kCamPhase5Fov;
	}

	scene_->camera_->SetTranslate(camPos);
	scene_->camera_->SetRotate(CalcLookAtRotation(camPos, lookTarget, camRoll));
	scene_->camera_->SetFovY(camFov);
	scene_->camera_->Update();
}

void GameOverSequenceController::UpdateParticles(float /*realDeltaTime*/)
{
	// 硝煙・羽毛の更新は UpdateAtmosphere に統合
}

void GameOverSequenceController::TriggerGameOverTransition(float shutterDuration)
{
	StopFlatlineAudio();
	if (scene_) scene_->SetSceneData<RaidStats>("RaidStats", RaidStats::GetInstance());
	DeathTransition::GetInstance()->StartTransition("GAMEOVER", shutterDuration);
}

void GameOverSequenceController::FinishSequence()
{
	if (transitionTriggered_) return;
	transitionTriggered_ = true;
	TriggerGameOverTransition(kNormalShutterDuration);
}

void GameOverSequenceController::Skip()
{
	if (transitionTriggered_) return;
	transitionTriggered_ = true;
	TacticalAudio::Play2D("Resources/Audio/SE_UI_Click.wav", kSkipClickVolume);
	TriggerGameOverTransition(kSkipShutterDuration);
}

bool GameOverSequenceController::Project3DTo2D(
	const Vector3& pos3D, const Matrix4x4& vp, float screenW, float screenH, Vector2& outPos)
{
	const float clipX = pos3D.x * vp.m[kAxisX][kAxisX] + pos3D.y * vp.m[kAxisY][kAxisX] + pos3D.z * vp.m[kAxisZ][kAxisX] + vp.m[kAxisW][kAxisX];
	const float clipY = pos3D.x * vp.m[kAxisX][kAxisY] + pos3D.y * vp.m[kAxisY][kAxisY] + pos3D.z * vp.m[kAxisZ][kAxisY] + vp.m[kAxisW][kAxisY];
	const float clipW = pos3D.x * vp.m[kAxisX][kAxisW] + pos3D.y * vp.m[kAxisY][kAxisW] + pos3D.z * vp.m[kAxisZ][kAxisW] + vp.m[kAxisW][kAxisW];
	if (clipW <= kProjMinClipW) return false;

	const float ndcX = clipX / clipW;
	const float ndcY = clipY / clipW;
	if (ndcX < -kProjNdcCullLimit || ndcX > kProjNdcCullLimit || ndcY < -kProjNdcCullLimit || ndcY > kProjNdcCullLimit)
	{
		return false;
	}

	outPos.x = (ndcX * 0.5f + 0.5f) * screenW;
	outPos.y = (-ndcY * 0.5f + 0.5f) * screenH;
	return true;
}

// -----------------------------------------------------------------------------
// 3Dレンダリング ＆ シネマスコープUI描画
// -----------------------------------------------------------------------------

void GameOverSequenceController::Draw3D(const RenderContext& ctx)
{
	if (!isActive_) return;

	if (droppedRifle_) droppedRifle_->Draw(ctx);
	if (timer_ >= kMedicAppearTime)
	{
		if (stretcherBed_) stretcherBed_->Draw(ctx);
		if (medicRear_)    medicRear_->Draw(ctx);
		if (medicFront_)   medicFront_->Draw(ctx);
	}
}

void GameOverSequenceController::DrawUI()
{
	if (!isActive_) return;
	if (DeathTransition::GetInstance() && DeathTransition::GetInstance()->IsTransitioning()) return;

	TacticalUICanvas* ui = TacticalUICanvas::GetInstance();
	if (!ui) return;
	TacticalDrawList* drawList = ui->GetForegroundDrawList();
	const UIVec2 disp = ui->GetDisplaySize();
	if (!drawList || disp.x <= 0.0f || disp.y <= 0.0f) return;

	// 映画風の重厚なダークレンズヴィネット（眩しいフラッシュ光は完全排除）
	if (darkVignetteAlpha_ > kVignetteMinAlpha)
	{
		const float pulse = kVignettePulseBase + kVignettePulseAmp * std::sin(timer_ * kVignettePulseFreq);
		const int vigA = static_cast<int>(kVignetteAlphaScale * darkVignetteAlpha_ * pulse);
		drawList->AddRect(
			UIVec2(0.0f, 0.0f), disp,
			PackColor(kColBlackRgb, static_cast<int>(vigA * kVignetteRectAlphaRatio)), 0.0f, 0, kVignetteStrokeWidth);
	}

	// 映画用シネマスコープ黒帯（上下 11%）
	const float curBarH = (disp.y * kLetterboxHeightRatio) * (std::min)(1.0f, timer_ / kLetterboxOpenDuration);
	const TacticalFont* fontDefault = ui->GetFont(UIFontRole::DefaultJP);
	const TacticalFont* fontSub     = ui->GetFont(UIFontRole::SubBoldJP);

	if (curBarH > kLetterboxMinHeightPx)
	{
		drawList->AddRectFilled(UIVec2(0.0f, 0.0f), UIVec2(disp.x, curBarH), kColLetterboxSolid);
		drawList->AddRectFilled(UIVec2(0.0f, disp.y - curBarH), disp, kColLetterboxSolid);

		const float pulse = kSkipPulseBase + kSkipPulseAmp * std::sin(timer_ * kSkipPulseFreq);
		const char* skipPrompt = reinterpret_cast<const char*>(u8"SPACE : 救難要請（スキップ）");
		const UIVec2 sSize = fontDefault->CalcTextSizeA(kSkipFontSize, FLT_MAX, 0.0f, skipPrompt);
		drawList->AddText(
			fontDefault, kSkipFontSize,
			UIVec2((disp.x - sSize.x) * 0.5f, disp.y - curBarH * 0.5f - sSize.y * 0.5f),
			PackColor(kColSkipPromptRgb, static_cast<int>(kSkipAlphaScale * pulse)), skipPrompt);
	}

	// 救難無線字幕（データテーブル駆動）
	for (const GameOverRadioCue& cue : kGameOverRadioCues)
	{
		if (timer_ >= cue.startTime && timer_ < cue.endTime)
		{
			const float textAlpha = (std::min)(1.0f, (timer_ - cue.startTime) / cue.fadeInDuration);
			DrawShadowedCenteredSubtitle(
				drawList, fontSub, kSubtitleFontSize, disp, kSubtitlePosYRatio,
				cue.color, static_cast<int>(kSubtitleAlphaScale * textAlpha),
				reinterpret_cast<const char*>(cue.text));
			break;
		}
	}

	// 終盤のブラックアウト ＆ 収容無線字幕
	if (fadeBlackAlpha_ > kBlackoutMinAlpha)
	{
		drawList->AddRectFilled(UIVec2(0.0f, 0.0f), disp, PackColor(kColBlackRgb, static_cast<int>(kAlphaMaxF * fadeBlackAlpha_)));
		if (fadeBlackAlpha_ > kBlackoutSubThreshold)
		{
			const int textA = static_cast<int>(kAlphaMaxF * (fadeBlackAlpha_ - kBlackoutSubThreshold) / kBlackoutSubSpan);
			DrawShadowedCenteredSubtitle(
				drawList, fontSub, kSubtitleFontSize, disp, kBlackoutSubPosYRatio,
				kColRadioMaydayRgb, textA, reinterpret_cast<const char*>(u8"「収容完了！ 救難ヘリへ搬送する！」"));
		}
	}
}
