#include "GameOverSequenceController.h"
#include "GamePlayScene.h"
#include "RaidStats.h"
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

#include <cmath>
#include <algorithm>
#include <cstdio>

#ifdef USE_IMGUI
#include "imgui.h"
#endif

namespace
{
	constexpr float kPi = 3.14159265358979323846f;

	float Rand01()
	{
		return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
	}

	float RandRange(float a, float b)
	{
		return a + (b - a) * Rand01();
	}

	float EaseOutQuad(float t)
	{
		t = (std::clamp)(t, 0.0f, 1.0f);
		return t * (2.0f - t);
	}

	float EaseInOutCubic(float t)
	{
		t = (std::clamp)(t, 0.0f, 1.0f);
		return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) * 0.5f;
	}

	float EaseOutBack(float t)
	{
		t = (std::clamp)(t, 0.0f, 1.0f);
		constexpr float c1 = 1.70158f;
		constexpr float c3 = c1 + 1.0f;
		return 1.0f + c3 * std::pow(t - 1.0f, 3.0f) + c1 * std::pow(t - 1.0f, 2.0f);
	}

	Vector3 LerpVector3(const Vector3& a, const Vector3& b, float t)
	{
		return {
			a.x + (b.x - a.x) * t,
			a.y + (b.y - a.y) * t,
			a.z + (b.z - a.z) * t
		};
	}

	Vector3 CalcLookAtRotation(const Vector3& eye, const Vector3& target, float roll = 0.0f)
	{
		Vector3 dir = { target.x - eye.x, target.y - eye.y, target.z - eye.z };
		float xzLen = std::sqrt(dir.x * dir.x + dir.z * dir.z);
		float pitch = std::atan2(-dir.y, (std::max)(xzLen, 0.001f));
		float yaw = std::atan2(dir.x, dir.z);
		return { pitch, yaw, roll };
	}
}

// -----------------------------------------------------------------------------
// 3Dモデル生成ヘルパー（吹き飛ぶライフル）
// -----------------------------------------------------------------------------

void GameOverSequenceController::BuildRifleModel(Object3d::ModelData& data)
{
	Player::BuildRifleModel(data);
}

void GameOverSequenceController::BuildStretcherModel(Object3d::ModelData& data)
{
	data.vertices.clear();
	data.indices.clear();

	auto AddBox = [&](Vector3 center, Vector3 size) {
		Vector3 h = size * 0.5f;
		Vector3 p[8] = {
			{ center.x - h.x, center.y - h.y, center.z - h.z },
			{ center.x + h.x, center.y - h.y, center.z - h.z },
			{ center.x + h.x, center.y + h.y, center.z - h.z },
			{ center.x - h.x, center.y + h.y, center.z - h.z },
			{ center.x - h.x, center.y - h.y, center.z + h.z },
			{ center.x + h.x, center.y - h.y, center.z + h.z },
			{ center.x + h.x, center.y + h.y, center.z + h.z },
			{ center.x - h.x, center.y + h.y, center.z + h.z },
		};

		int faces[6][4] = {
			{ 0, 1, 2, 3 }, // Front (-Z)
			{ 5, 4, 7, 6 }, // Back (+Z)
			{ 4, 0, 3, 7 }, // Left (-X)
			{ 1, 5, 6, 2 }, // Right (+X)
			{ 3, 2, 6, 7 }, // Top (+Y)
			{ 4, 5, 1, 0 }  // Bottom (-Y)
		};
		Vector3 normals[6] = {
			{ 0.0f, 0.0f, -1.0f },
			{ 0.0f, 0.0f, 1.0f },
			{ -1.0f, 0.0f, 0.0f },
			{ 1.0f, 0.0f, 0.0f },
			{ 0.0f, 1.0f, 0.0f },
			{ 0.0f, -1.0f, 0.0f }
		};

		for (int f = 0; f < 6; ++f)
		{
			uint32_t fBase = static_cast<uint32_t>(data.vertices.size());
			for (int v = 0; v < 4; ++v)
			{
				Sprite::VertexData vd{};
				vd.position = { p[faces[f][v]].x, p[faces[f][v]].y, p[faces[f][v]].z, 1.0f };
				vd.texcoord = { (v == 1 || v == 2) ? 1.0f : 0.0f, (v == 2 || v == 3) ? 1.0f : 0.0f };
				vd.normal = normals[f];
				data.vertices.push_back(vd);
			}
			data.indices.push_back(fBase + 0);
			data.indices.push_back(fBase + 1);
			data.indices.push_back(fBase + 2);
			data.indices.push_back(fBase + 0);
			data.indices.push_back(fBase + 2);
			data.indices.push_back(fBase + 3);
		}
	};

	// 2 本のアルミニウム担架棒（長辺 X: 2.40m, 太さ: 0.06m, 左右幅 1.20m -> Z: ±0.60m）
	AddBox({ 0.0f, 0.0f, -0.60f }, { 2.40f, 0.06f, 0.06f });
	AddBox({ 0.0f, 0.0f,  0.60f }, { 2.40f, 0.06f, 0.06f });

	// ハンドルグリップ（前後端 X: ±1.15m）
	AddBox({ -1.15f, 0.0f, -0.60f }, { 0.16f, 0.08f, 0.08f });
	AddBox({ -1.15f, 0.0f,  0.60f }, { 0.16f, 0.08f, 0.08f });
	AddBox({  1.15f, 0.0f, -0.60f }, { 0.16f, 0.08f, 0.08f });
	AddBox({  1.15f, 0.0f,  0.60f }, { 0.16f, 0.08f, 0.08f });

	// 担架のナイロンキャンバス生地（幅 1.15m, 長さ 1.85m, 厚さ 0.03m）
	AddBox({ 0.0f, -0.015f, 0.0f }, { 1.85f, 0.03f, 1.15f });

	// 4本の折りたたみ脚（高さ 0.12m）
	AddBox({ -0.75f, -0.07f, -0.60f }, { 0.05f, 0.12f, 0.05f });
	AddBox({ -0.75f, -0.07f,  0.60f }, { 0.05f, 0.12f, 0.05f });
	AddBox({  0.75f, -0.07f, -0.60f }, { 0.05f, 0.12f, 0.05f });
	AddBox({  0.75f, -0.07f,  0.60f }, { 0.05f, 0.12f, 0.05f });

	// 負傷アヒルの頭部を支える大型ミリタリー救急枕
	AddBox({ -0.60f, 0.04f, 0.0f }, { 0.45f, 0.08f, 0.85f });

	data.boundingRadius = 4.0f;
}

// -----------------------------------------------------------------------------
// コントローラーライフサイクル
// -----------------------------------------------------------------------------

GameOverSequenceController::GameOverSequenceController(GamePlayScene* scene)
	: scene_(scene)
{
	ecgHistory_.reserve(120);
	for (int i = 0; i < 80; ++i)
	{
		ecgHistory_.push_back(0.0f);
	}
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

	// テクスチャのロード
	auto* tm = TextureManager::GetInstance();
	if (tm)
	{
		tm->LoadTexture("Resources/circle2.png");
		circle2TextureIndex_ = tm->GetTextureIndexByFilePath("Resources/circle2.png");

		tm->LoadTexture("Resources/leaf.png");
		leafTextureIndex_ = tm->GetTextureIndexByFilePath("Resources/leaf.png");

		tm->LoadTexture("Resources/title_vignette.png");
		vignetteTextureIndex_ = tm->GetTextureIndexByFilePath("Resources/title_vignette.png");

		tm->LoadTexture("Resources/starburst.png");
		starburstTextureIndex_ = tm->GetTextureIndexByFilePath("Resources/starburst.png");

		tm->LoadTexture("Resources/smoke_dark.png");
		smokeDarkTextureIndex_ = tm->GetTextureIndexByFilePath("Resources/smoke_dark.png");

		featherTextureIndex_ = tm->Load("Resources/feather.png");

		medicTexIndex_ = tm->Load("Resources/duck_medic.png");
		stretcherTexIndex_ = tm->Load("Resources/container_military.png");
		woundedDuckTexIndex_ = tm->Load("Resources/duck_player_wounded.png");
		normalTexIndex_ = tm->Load("Resources/duck.png");
	}

	// 1. 吹き飛ぶライフルオブジェクトの生成
	{
		Object3d::ModelData rifleData{};
		BuildRifleModel(rifleData);
		rifleData.material.textureFilePath = "Resources/container_military.png";
		rifleData.material.textureIndex = (stretcherTexIndex_ != UINT32_MAX) ? stretcherTexIndex_ : 0;

		droppedRifle_ = std::make_unique<Object3d>();
		droppedRifle_->Initialize(scene_->object3dCom, rifleData);
		droppedRifle_->SetCamera(scene_->camera_);
		droppedRifle_->SetColor({ 0.20f, 0.20f, 0.22f, 1.0f }); // 軍用ガンメタルブラック（光の反射・白飛び完全防止）
		droppedRifle_->SetEnableLighting(true);
		droppedRifle_->SetAllowWireframeOverlay(false);
	}

	// 2. 衛生兵アヒル（前方・後方）と担架ベッドの生成（GameOverSceneのヘリ搬送に直結）
	{
		Object3d::ModelData medicData = Object3d::LoadObjFile("Resources", "player.obj");
		medicData.material.textureFilePath = "Resources/duck_medic.png";
		medicData.material.textureIndex = (medicTexIndex_ != UINT32_MAX) ? medicTexIndex_ : 0;
		medicData.boundingRadius = 10.0f;

		medicFront_ = std::make_unique<Object3d>();
		medicFront_->Initialize(scene_->object3dCom, medicData);
		medicFront_->SetCamera(scene_->camera_);
		medicFront_->SetScale({ 0.95f, 0.95f, 0.95f });
		medicFront_->SetEnableLighting(true);
		medicFront_->SetAllowWireframeOverlay(false);

		medicRear_ = std::make_unique<Object3d>();
		medicRear_->Initialize(scene_->object3dCom, medicData);
		medicRear_->SetCamera(scene_->camera_);
		medicRear_->SetScale({ 0.95f, 0.95f, 0.95f });
		medicRear_->SetEnableLighting(true);
		medicRear_->SetAllowWireframeOverlay(false);

		Object3d::ModelData stretcherData{};
		BuildStretcherModel(stretcherData);
		stretcherData.material.textureFilePath = "Resources/container_military.png";
		stretcherData.material.textureIndex = (stretcherTexIndex_ != UINT32_MAX) ? stretcherTexIndex_ : 0;

		stretcherBed_ = std::make_unique<Object3d>();
		stretcherBed_->Initialize(scene_->object3dCom, stretcherData);
		stretcherBed_->SetCamera(scene_->camera_);
		stretcherBed_->SetScale({ 1.25f, 1.25f, 1.25f });
		stretcherBed_->SetEnableLighting(true);
		stretcherBed_->SetAllowWireframeOverlay(false);
	}

	// オーディオのロード
	if (SceneManager::GetInstance() && SceneManager::GetInstance()->GetAudioManager())
	{
		auto* am = SceneManager::GetInstance()->GetAudioManager();
		alarmSoundId_ = am->Load("Resources/Alarm01.wav");
		duckSoundId_ = am->Load("Resources/Audio/SE_Duck.mp3");
	}
}

void GameOverSequenceController::Finalize()
{
	isActive_ = false;
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

void GameOverSequenceController::Start(const Vector3& deathPos, const Vector3& killerPos, const std::string& killerName, const std::string& causeOfDeath, bool isMIA)
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
	smokeTimer_ = 0.0f;
	featherTimer_ = 0.0f;
	darkVignetteAlpha_ = 0.65f;
	fadeBlackAlpha_ = 0.0f;
	medicWalkPhase_ = 0.0f;

	// 負傷アヒルテクスチャへの即時切り替え（GameOverSceneの負傷姿と完全に統一）
	if (scene_ && scene_->GetPlayer() && woundedDuckTexIndex_ != UINT32_MAX)
	{
		originalPlayerTexIndex_ = scene_->GetPlayer()->GetTextureIndex();
		scene_->GetPlayer()->SetTextureIndex(woundedDuckTexIndex_);
	}

	// 被弾赤フラッシュの即時キル（眩しい光や赤みを完全にカット）
	if (scene_)
	{
		scene_->vignetteAlpha_ = 0.0f;
	}

	// キラーからのノックバック方向ベクトル
	Vector3 toKiller = killerPos_ - deathPos_;
	killerDistance_ = std::sqrt(toKiller.x * toKiller.x + toKiller.z * toKiller.z);
	float rad = std::atan2(toKiller.x, toKiller.z);
	float deg = rad * (180.0f / kPi);
	if (deg < 0.0f) deg += 360.0f;
	killerBearingDeg_ = deg;

	if (killerDistance_ > 0.1f)
	{
		flingDir_ = { -toKiller.x / killerDistance_, 0.0f, -toKiller.z / killerDistance_ };
	}
	else
	{
		flingDir_ = { 0.0f, 0.0f, -1.0f };
	}

	// 初期アヒル姿勢
	duckPos_ = deathPos_;
	duckRot_ = { 0.0f, rad, 0.0f };
	duckScale_ = { 1.0f, 1.0f, 1.0f };

	// 吹き飛ぶライフルの初期化（被弾衝撃で手から上空斜め前へ弾き飛びスピン）
	Vector3 perp = { -flingDir_.z, 0.0f, flingDir_.x };
	if (scene_ && scene_->GetPlayer())
	{
		riflePos_ = scene_->GetPlayer()->GetRiflePosition();
	}
	else
	{
		riflePos_ = deathPos_ + Vector3{ 0.0f, 0.68f, 0.0f };
	}
	rifleRot_ = { 0.2f, rad, 0.0f };
	rifleVel_ = flingDir_ * 2.4f - perp * 1.1f + Vector3{ 0.0f, 4.0f, 0.0f };
	rifleRotVel_ = { RandRange(14.0f, 18.0f), RandRange(8.0f, 12.0f), RandRange(12.0f, 16.0f) };
	rifleGrounded_ = false;

	// 衛生兵チームのスタート位置と目標位置（GameOverSceneのヘリ搬送チームに接続）
	// 倒れたアヒルの最終着地位置から横方向（perp）に1.80m離れた位置に担架を下ろす
	Vector3 duckFinalPos = deathPos_ + flingDir_ * 1.55f;
	medicTeamTargetPos_ = duckFinalPos + perp * 1.80f;
	// 後方・外側から駆けつけてくる
	medicTeamStartPos_ = medicTeamTargetPos_ - flingDir_ * 6.5f + perp * 0.6f;
	medicTeamPos_ = medicTeamStartPos_;

	// 初期カメラ状態の保存
	if (scene_ && scene_->camera_)
	{
		initialCamPos_ = scene_->camera_->GetTranslate();
		initialCamRot_ = scene_->camera_->GetRotate();
		initialCamFov_ = scene_->camera_->GetFovY();
	}

	// 💥 致命弾インパクト！ 特撮映画調の重いヒットストップ＆カメラ激震（光なし、重厚な物理衝撃）
	if (scene_)
	{
		scene_->TriggerCameraShake(0.70f, 3.5f);
		scene_->TriggerHitStop(0.16f);
	}

	// オーディオ発動
	if (SceneManager::GetInstance() && SceneManager::GetInstance()->GetAudioManager())
	{
		auto* am = SceneManager::GetInstance()->GetAudioManager();
		if (duckSoundId_ >= 0) am->PlayOneShot(duckSoundId_, 1.0f);
	}

	// 💥 着弾瞬間の特撮物理エフェクト（超音速弾道トレースライン ＆ 扇状羽毛ブラスト・光演出完全排除）
	if (scene_ && scene_->particleManager && scene_->appParticleManager_)
	{
		auto& randEng = scene_->particleManager->GetRandomEngine();
		uint32_t darkSmokeTex = (smokeDarkTextureIndex_ != UINT32_MAX) ? smokeDarkTextureIndex_ : scene_->smokeTextureIndex_;
		uint32_t fTex = (featherTextureIndex_ != UINT32_MAX) ? featherTextureIndex_ : scene_->particleTextureB;

		// 1. 超音速弾丸の貫通トレース（胸部を貫通して後方へ抜ける暗色硝煙ライン・白飛び防止）
		for (int i = 0; i < 10; ++i)
		{
			float speed = RandRange(10.0f, 18.0f);
			Vector3 streakVel = flingDir_ * speed + Vector3{ RandRange(-0.25f, 0.25f), RandRange(-0.15f, 0.15f), RandRange(-0.25f, 0.25f) };
			scene_->appParticleManager_->EmitDustWithVelocity(
				randEng, duckPos_ + Vector3{ 0.0f, 0.36f, 0.0f }, 0.22f,
				{ 0.12f, 0.11f, 0.10f, 0.35f }, streakVel, 0.18f, darkSmokeTex
			);
		}

		// 2. 胸部着弾から扇状に吹き出す羽毛（Feather Kinetic Burst - リアルな羽毛テクスチャ、光の玉完全排除）
		for (int i = 0; i < 16; ++i)
		{
			Vector3 spread = flingDir_ * RandRange(2.8f, 5.5f) + perp * RandRange(-1.6f, 1.6f) + Vector3{ 0.0f, RandRange(1.0f, 2.8f), 0.0f };
			scene_->appParticleManager_->EmitFeather(randEng, duckPos_ + Vector3{ 0.0f, 0.36f, 0.0f }, { 0.22f, 0.19f, 0.14f, 0.45f }, fTex);
		}

		// 3. 足元から弾け飛ぶ衝撃波ダスト（暗い砂煙リング・発光完全防止）
		for (int i = 0; i < 12; ++i)
		{
			float a = i * (2.0f * kPi / 12.0f);
			Vector3 rVel = { std::cos(a) * 2.6f, 0.10f, std::sin(a) * 2.6f };
			scene_->appParticleManager_->EmitDustWithVelocity(
				randEng, duckPos_ + Vector3{ 0.0f, 0.02f, 0.0f }, 0.40f,
				{ 0.11f, 0.10f, 0.08f, 0.30f }, rVel, 0.32f, darkSmokeTex
			);
		}
	}
}

void GameOverSequenceController::Update(float realDeltaTime)
{
	if (!isActive_) return;

	// 🎬 映画的バレットタイム・スローモーション演出 (Bullet-Time Time Dilation)
	// 致命弾着弾の瞬間 (0.0s 〜 0.22s) は時間の流れが約 20% に劇的減速！
	// 激しい衝撃、のけぞり、空中に爆散する羽毛、宙を舞うライフルがスローで鮮明に映し出され、
	// 地面激突インパクト (0.38s) に向けて一気にフルスピード (100%) へスナップ加速！
	float timeScale = 1.0f;
	if (timer_ < 0.22f)
	{
		timeScale = 0.20f; // 劇的バレットタイム・スローモーション
	}
	else if (timer_ < 0.38f)
	{
		float u = (timer_ - 0.22f) / 0.16f;
		timeScale = 0.20f + 0.80f * (u * u); // 地面激突に向けてフルスピードへスナップ！
	}
	else
	{
		timeScale = 1.0f; // 骨身に染みる重厚な地面衝突＆滑走
	}
	float simDelta = realDeltaTime * timeScale;
	timer_ += simDelta;

	// 心電図・バイタル警告音（0.85sで心拍低下アラーム発動）
	if (timer_ >= 0.85f && !alarmTriggered_)
	{
		alarmTriggered_ = true;
		if (SceneManager::GetInstance() && SceneManager::GetInstance()->GetAudioManager())
		{
			auto* am = SceneManager::GetInstance()->GetAudioManager();
			if (alarmSoundId_ >= 0) am->PlayOneShot(alarmSoundId_, 0.85f);
		}
	}

	// 終盤のブラックアウトフェード
	// (DeathTransition の重装甲ブラストシャッターが画面を直接覆って閉鎖するため、黒矩形フェードは不要)
	fadeBlackAlpha_ = 0.0f;

	// 各シネマティック物理・カメラ・エフェクトの更新
	UpdateMotion(simDelta);
	UpdateWeaponMotion(simDelta);
	UpdateAtmosphere(simDelta);
	UpdateCamera(simDelta);
	UpdateParticles(simDelta);

	// 心電図の更新
	ecgTimer_ += simDelta;
	float currentPulse = 0.0f;
	if (timer_ < 0.35f)
	{
		// 瀕死の頻脈 (150 BPM)
		float cycle = std::fmod(ecgTimer_ * 5.0f, 1.0f);
		if (cycle < 0.14f) currentPulse = std::sin(cycle / 0.14f * kPi) * 1.35f;
		else if (cycle < 0.26f) currentPulse = -std::sin((cycle - 0.14f) / 0.12f * kPi) * 0.50f;
	}
	else if (timer_ < kAct2Duration)
	{
		// 徐脈・不整脈・心停止へ (40 BPM)
		float decay = (1.0f - (timer_ - 0.35f) / (kAct2Duration - 0.35f));
		float cycle = std::fmod(ecgTimer_ * 2.0f, 1.0f);
		if (cycle < 0.16f) currentPulse = std::sin(cycle / 0.16f * kPi) * decay;
		else if (cycle < 0.28f) currentPulse = -std::sin((cycle - 0.16f) / 0.12f * kPi) * (0.35f * decay);
	}
	else
	{
		currentPulse = 0.0f; // フラットライン停止
	}
	ecgHistory_.push_back(currentPulse);
	if (ecgHistory_.size() > 80)
	{
		ecgHistory_.erase(ecgHistory_.begin());
	}

	// [SPACE] または [ENTER] キーでの即時スキップ
	bool skipPressed = false;
	if ((GetAsyncKeyState(VK_SPACE) & 0x8000) != 0 || (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0)
	{
		skipPressed = true;
	}
#ifdef USE_IMGUI
	if (ImGui::IsKeyPressed(ImGuiKey_Space) || ImGui::IsKeyPressed(ImGuiKey_Enter))
	{
		skipPressed = true;
	}
#endif
	if (skipPressed && timer_ > 0.35f)
	{
		Skip();
		return;
	}

	// タイムライン完走時の自動シーン移行
	if (timer_ >= kDuration && !transitionTriggered_)
	{
		FinishSequence();
	}
}

// -----------------------------------------------------------------------------
// 3Dアヒル兵士の物理アニメーション（吹き飛び・激突・バウンド・滑走）
// -----------------------------------------------------------------------------

void GameOverSequenceController::UpdateMotion(float realDeltaTime)
{
	if (!scene_ || !scene_->GetPlayer()) return;
	Player* player = scene_->GetPlayer();
	Object3d* obj = player->GetObject3d();
	if (!obj) return;

	Vector3 perp = { -flingDir_.z, 0.0f, flingDir_.x };
	float baseYaw = killerBearingDeg_ * (kPi / 180.0f);

	if (timer_ < 0.38f)
	{
		// 1. 【致命弾インパクト・胸部スナップ＆空中放物線吹き飛び＆3Dラグドール捻転】 (0.0s 〜 0.38s)
		float t = timer_ / 0.38f;
		// 弾道運動力学：急激な跳ね上がりと重力落下 (最高点 0.26m)
		float jumpArc = 0.26f * (4.0f * t * (1.0f - t));
		// 水平ノックバック（衝撃で勢いよく後方へ吹き飛び、空気抵抗でわずかに減速）
		float horizDist = 1.10f * (1.0f - (1.0f - t) * (1.0f - t));

		duckPos_ = deathPos_ + flingDir_ * horizDist + Vector3{ 0.0f, 0.08f + jumpArc, 0.0f };

		// 激しいのけぞり・首折れ・3Dラグドール捻転
		// X軸（後方への仰け反り）: 地面接触直前には約 -70度 (-1.22 rad) まで反り返り、顔が真上や後ろではなくカメラ方向へ仰ぐ
		float pitchT = EaseOutQuad((std::min)(1.0f, t * 1.25f));
		duckRot_.x = -pitchT * 1.22f;

		// Y軸（非対称弾丸衝撃によるスピン捻れ）: 被弾方向からカメラ側（-perp側）へ顔・くちばしを向ける
		float yawSpin = -std::sin(t * kPi * 0.5f) * 0.30f;
		duckRot_.y = baseYaw + yawSpin;

		// Z軸（ロール傾き・片翼からの落下）: 約 +20度 (0.35 rad) 横に傾き、カメラ側に胸と顔を傾けて倒れる
		duckRot_.z = t * 0.35f;

		// 着弾衝撃によるボディの伸縮（物理エネルギーの伝播）
		if (t < 0.25f)
		{
			float impactSquash = std::sin(t / 0.25f * kPi);
			duckScale_ = { 1.08f + impactSquash * 0.08f, 0.90f - impactSquash * 0.10f, 1.08f + impactSquash * 0.08f };
		}
		else
		{
			float airStretch = std::sin((t - 0.25f) / 0.75f * kPi);
			duckScale_ = { 0.96f, 1.05f + airStretch * 0.06f, 0.96f };
		}
	}
	else if (timer_ < 0.85f)
	{
		// 2. 【地面激突インパクト＆強烈な摩擦スライディング＆慣性バウンド】 (0.38s 〜 0.85s)
		float t = (timer_ - 0.38f) / 0.47f;

		// 💥 地面衝突瞬間（0.38s）の特撮演出: 激震＆暗色土煙バースト＆飛び散る小石（光演出完全排除）
		if (!groundImpactTriggered_)
		{
			groundImpactTriggered_ = true;
			if (scene_)
			{
				scene_->TriggerCameraShake(0.65f, 3.8f);
				if (scene_->particleManager && scene_->appParticleManager_)
				{
					auto& randEng = scene_->particleManager->GetRandomEngine();
					uint32_t darkSmokeTex = (smokeDarkTextureIndex_ != UINT32_MAX) ? smokeDarkTextureIndex_ : scene_->smokeTextureIndex_;
					uint32_t fTex = (featherTextureIndex_ != UINT32_MAX) ? featherTextureIndex_ : scene_->particleTextureB;

					// 1. 地面スレスレを薙ぎ払う暗色衝撃波ダスト（16方向、暗い土埃リング・発光完全防止）
					for (int i = 0; i < 16; ++i)
					{
						float angle = i * (2.0f * kPi / 16.0f);
						Vector3 vel = { std::cos(angle) * 4.6f, RandRange(0.08f, 0.22f), std::sin(angle) * 4.6f };
						scene_->appParticleManager_->EmitDustWithVelocity(
							randEng, duckPos_ + Vector3{ 0.0f, 0.03f, 0.0f }, 0.80f,
							{ 0.14f, 0.12f, 0.10f, 0.38f }, vel, 0.50f, darkSmokeTex
						);
					}

					// 2. ふわっと立ち上がる濃い土煙雲（8方向、重厚な泥煙・白飛びなし）
					for (int i = 0; i < 8; ++i)
					{
						float angle = i * (2.0f * kPi / 8.0f) + RandRange(-0.2f, 0.2f);
						Vector3 vel = { std::cos(angle) * 1.5f, RandRange(0.6f, 1.4f), std::sin(angle) * 1.5f };
						scene_->appParticleManager_->EmitDustWithVelocity(
							randEng, duckPos_ + Vector3{ 0.0f, 0.06f, 0.0f }, 1.10f,
							{ 0.09f, 0.08f, 0.07f, 0.45f }, vel, 0.65f, darkSmokeTex
						);
					}

					// 3. 飛び散る土塊・砂利（小石・破片の飛散距離を広げてよりダイナミックに拡散）
					for (int i = 0; i < 14; ++i)
					{
						float angle = i * (2.0f * kPi / 14.0f) + RandRange(-0.25f, 0.25f);
						Vector3 vel = { std::cos(angle) * RandRange(2.2f, 4.6f), RandRange(0.6f, 2.2f), std::sin(angle) * RandRange(2.2f, 4.6f) };
						scene_->appParticleManager_->EmitDustWithVelocity(
							randEng, duckPos_ + Vector3{ RandRange(-0.25f, 0.25f), 0.04f, RandRange(-0.25f, 0.25f) }, 0.38f,
							{ 0.10f, 0.08f, 0.07f, 0.45f }, vel, 0.45f, darkSmokeTex
						);
					}

					// 4. 散らばる羽毛（地面激突でより広範囲へふわりと舞い散る）
					for (int i = 0; i < 14; ++i)
					{
						Vector3 fOffset = { RandRange(-0.50f, 0.50f), RandRange(0.12f, 0.40f), RandRange(-0.50f, 0.50f) };
						scene_->appParticleManager_->EmitFeather(randEng, duckPos_ + fOffset, { 0.20f, 0.18f, 0.14f, 0.40f }, fTex);
					}
				}
			}
		}

		// 激突直後のマイクロバウンド（背中が土に叩きつけられて5cm跳ねる）
		float bounceArc = 0.0f;
		if (t < 0.35f)
		{
			float bT = t / 0.35f;
			bounceArc = std::sin(bT * kPi) * 0.05f;
		}

		// 地面摩擦滑走（EaseOutCubicによる力強い減速スライド 0.45m）
		// 地表に自然に乗る高さ (Y: 0.14m) に設定し地面への埋もれを完全防止
		float skidEase = 1.0f - std::pow(1.0f - t, 3.0f);
		float skidDist = 1.10f + 0.45f * skidEase;
		duckPos_ = deathPos_ + flingDir_ * skidDist + Vector3{ 0.0f, 0.14f + bounceArc, 0.0f };

		// スライディング中の地面擦過ダスト（土煙の尾を引く）
		static float skidDustTimer = 0.0f;
		skidDustTimer += realDeltaTime;
		if (t < 0.70f && skidDustTimer >= 0.08f && scene_ && scene_->particleManager && scene_->appParticleManager_)
		{
			skidDustTimer = 0.0f;
			auto& randEng = scene_->particleManager->GetRandomEngine();
			uint32_t darkSmokeTex = (smokeDarkTextureIndex_ != UINT32_MAX) ? smokeDarkTextureIndex_ : scene_->smokeTextureIndex_;
			Vector3 dVel = flingDir_ * -0.6f + Vector3{ RandRange(-0.2f, 0.2f), RandRange(0.1f, 0.35f), RandRange(-0.2f, 0.2f) };
			scene_->appParticleManager_->EmitDustWithVelocity(
				randEng, duckPos_ + Vector3{ 0.0f, 0.04f, 0.0f }, 0.45f,
				{ 0.12f, 0.10f, 0.09f, 0.32f }, dVel, 0.35f, darkSmokeTex
			);
		}

		// 首・頭部の慣性セトル（頭部が地面に叩きつけられた後、カメラ側に顔とくちばしを向けた状態で安定）
		float neckWhip = (t < 0.40f) ? std::sin(t / 0.40f * kPi) * 0.08f : 0.0f;
		duckRot_.x = -1.22f - neckWhip; // 約 -70度（顔が地面に埋もれず、くちばしがカメラ側を仰ぎ見る）
		duckRot_.y = baseYaw - 0.30f * (1.0f - t * 0.15f); // カメラ側（-perp）を向いたまま安定
		duckRot_.z = 0.35f - 0.05f * skidEase;         // 片翼を支えに +17度 カメラ側へ傾いたポーズ

		// 激突の衝撃伸縮（ペシャンと潰れてから復元）
		if (t < 0.20f)
		{
			float impactSquash = std::sin(t / 0.20f * kPi);
			duckScale_ = { 1.12f + impactSquash * 0.08f, 0.84f - impactSquash * 0.10f, 1.12f + impactSquash * 0.08f };
		}
		else
		{
			duckScale_ = { 1.0f, 1.0f, 1.0f };
		}
	}
	else
	{
		// 3. 【戦場に横たわる兵士・微弱な呼吸】 (0.85s 以降)
		Vector3 finalPos = deathPos_ + flingDir_ * 1.55f;
		finalPos.y = 0.14f; // 地表の上に自然に横たわる（地面埋没防止）
		duckPos_ = finalPos;

		duckRot_.x = -1.22f; // 約 -70度: くちばしと顔がカメラ側斜め上を仰ぐ
		duckRot_.y = baseYaw - 0.26f; // カメラ方向
		duckRot_.z = 0.30f; // 片方の翼を下にして横たわるリアルな負傷兵ポーズ（顔がカメラから明瞭に見える）

		float breath = std::sin((timer_ - 0.85f) * 1.8f) * 0.010f;
		duckScale_ = { 1.04f + breath, 0.96f + breath * 0.5f, 1.04f };
	}

	obj->SetTranslate(duckPos_);
	obj->SetRotate(duckRot_);
	obj->SetScale(duckScale_);
	obj->SetColor({ 0.95f, 0.95f, 0.98f, 1.0f });
	obj->Update();

	// 4. 【衛生兵レスキューチームの駆けつけ】 (1.60s 以降、GameOverSceneのヘリ搬送に直結)
	if (timer_ >= 1.60f && medicFront_ && medicRear_ && stretcherBed_)
	{
		medicWalkPhase_ += realDeltaTime * 7.5f;

		Vector3 toDuck = medicTeamTargetPos_ - medicTeamStartPos_;
		float dist = std::sqrt(toDuck.x * toDuck.x + toDuck.z * toDuck.z);
		Vector3 forward = (dist > 0.01f)
			? Vector3{ toDuck.x / dist, 0.0f, toDuck.z / dist }
			: Vector3{ 1.0f, 0.0f, 0.0f };
		float yaw = std::atan2(forward.x, forward.z);

		constexpr float kMedicArmOffset = 1.48f; // 担架ハンドル位置（前後 X: ±1.44m）に合わせた衛生兵の配置オフセット

		if (timer_ < 2.80f)
		{
			float u = (timer_ - 1.60f) / 1.20f;
			float ease = EaseOutQuad(u);
			medicTeamPos_ = LerpVector3(medicTeamStartPos_, medicTeamTargetPos_, ease);

			float bob = std::fabs(std::sin(medicWalkPhase_)) * 0.06f;
			float waddle = std::sin(medicWalkPhase_) * 0.14f;

			medicRear_->SetTranslate(medicTeamPos_ - forward * kMedicArmOffset + Vector3{ 0.0f, bob, 0.0f });
			medicRear_->SetRotate({ 0.0f, yaw, waddle });
			medicRear_->Update();

			medicFront_->SetTranslate(medicTeamPos_ + forward * kMedicArmOffset + Vector3{ 0.0f, bob, 0.0f });
			medicFront_->SetRotate({ 0.0f, yaw, -waddle });
			medicFront_->Update();

			stretcherBed_->SetTranslate(medicTeamPos_ + Vector3{ 0.0f, 0.38f + bob * 0.5f, 0.0f });
			stretcherBed_->SetRotate({ 0.0f, yaw + 1.5707963f, 0.0f });
			stretcherBed_->Update();
		}
		else
		{
			medicTeamPos_ = medicTeamTargetPos_;

			stretcherBed_->SetTranslate(medicTeamPos_ + Vector3{ 0.0f, 0.08f, 0.0f });
			stretcherBed_->SetRotate({ 0.0f, yaw + 1.5707963f, 0.0f });
			stretcherBed_->Update();

			Vector3 rearPos = medicTeamPos_ - forward * kMedicArmOffset + Vector3{ 0.0f, -0.06f, 0.0f };
			Vector3 toWoundedRear = duckPos_ - rearPos;
			float rearLookYaw = std::atan2(toWoundedRear.x, toWoundedRear.z);
			medicRear_->SetTranslate(rearPos);
			medicRear_->SetRotate({ 0.25f, rearLookYaw, 0.0f });
			medicRear_->Update();

			Vector3 frontPos = medicTeamPos_ + forward * kMedicArmOffset + Vector3{ 0.0f, -0.06f, 0.0f };
			Vector3 toWoundedFront = duckPos_ - frontPos;
			float frontLookYaw = std::atan2(toWoundedFront.x, toWoundedFront.z);
			medicFront_->SetTranslate(frontPos);
			medicFront_->SetRotate({ 0.25f, frontLookYaw, 0.0f });
			medicFront_->Update();
		}
	}
}

// -----------------------------------------------------------------------------
// 吹き飛ぶライフル（武器）の物理運動
// -----------------------------------------------------------------------------

void GameOverSequenceController::UpdateWeaponMotion(float realDeltaTime)
{
	if (!droppedRifle_) return;

	if (!rifleGrounded_)
	{
		rifleVel_.y -= 12.0f * realDeltaTime;
		riflePos_ += rifleVel_ * realDeltaTime;
		rifleRot_ += rifleRotVel_ * realDeltaTime;

		if (riflePos_.y <= 0.04f)
		{
			riflePos_.y = 0.04f;
			rifleVel_.y = -rifleVel_.y * 0.28f;
			rifleVel_.x *= 0.55f;
			rifleVel_.z *= 0.55f;
			rifleRotVel_ = rifleRotVel_ * 0.35f;

			// 地面激突時の砂塵（暗い砂埃、光は出さない）
			if (scene_ && scene_->particleManager && scene_->appParticleManager_)
			{
				auto& randEng = scene_->particleManager->GetRandomEngine();
				uint32_t darkSmokeTex = (smokeDarkTextureIndex_ != UINT32_MAX) ? smokeDarkTextureIndex_ : scene_->smokeTextureIndex_;
				for (int i = 0; i < 4; ++i)
				{
					Vector3 dVel = { RandRange(-0.8f, 0.8f), RandRange(0.2f, 0.5f), RandRange(-0.8f, 0.8f) };
					scene_->appParticleManager_->EmitDustWithVelocity(
						randEng, riflePos_, 0.40f, { 0.12f, 0.10f, 0.09f, 0.30f }, dVel, 0.30f, darkSmokeTex
					);
				}
			}

			if (std::abs(rifleVel_.y) < 0.2f)
			{
				rifleGrounded_ = true;
				rifleVel_ = { 0.0f, 0.0f, 0.0f };
			}
		}
	}

	droppedRifle_->SetTranslate(riflePos_);
	droppedRifle_->SetRotate(rifleRot_);
	droppedRifle_->SetScale({ 1.0f, 1.0f, 1.0f });
	droppedRifle_->Update();
}

// -----------------------------------------------------------------------------
// 被弾部位から立ち上る硝煙 ＆ ヘリのローター下降気流 ＆ 舞い散る羽毛
// -----------------------------------------------------------------------------

void GameOverSequenceController::UpdateAtmosphere(float realDeltaTime)
{
	// 1. 被弾部位から立ち上る繊細な硝煙（顔を覆い隠さないよう胸部後方から奥へ逃がす・光らない暗色の煙）
	if (timer_ >= 0.45f)
	{
		smokeTimer_ += realDeltaTime;
		if (smokeTimer_ >= 0.09f && scene_ && scene_->particleManager && scene_->appParticleManager_)
		{
			smokeTimer_ = 0.0f;
			auto& randEng = scene_->particleManager->GetRandomEngine();
			Vector3 perp = { -flingDir_.z, 0.0f, flingDir_.x };
			// 顔の真正面を避け、胸部後方・カメラ奥側（+perp）へ優雅に抜ける配置
			Vector3 sPos = duckPos_ - flingDir_ * 0.18f + perp * 0.12f + Vector3{ RandRange(-0.06f, 0.06f), 0.14f, RandRange(-0.06f, 0.06f) };
			Vector3 sVel = flingDir_ * 0.22f + perp * 0.35f + Vector3{ RandRange(-0.10f, 0.10f), RandRange(0.8f, 1.4f), RandRange(-0.10f, 0.10f) };
			scene_->appParticleManager_->EmitDustWithVelocity(
				randEng, sPos, 0.42f, // 顔を隠さない小ぶりのリアルな硝煙サイズ
				{ 0.08f, 0.07f, 0.07f, 0.25f }, // 薄く自然な硝煙アルファ
				sVel, 0.40f,
				(smokeDarkTextureIndex_ != UINT32_MAX) ? smokeDarkTextureIndex_ : scene_->smokeTextureIndex_
			);
		}
	}

	// 2. 接近する救難ヘリのローター下降気流（Rotor Downwash Wind）
	if (timer_ >= 1.0f && timer_ < 2.90f && scene_ && scene_->particleManager && scene_->appParticleManager_)
	{
		static float downwashTimer = 0.0f;
		downwashTimer += realDeltaTime;
		if (downwashTimer >= 0.12f)
		{
			downwashTimer = 0.0f;
			auto& randEng = scene_->particleManager->GetRandomEngine();
			Vector3 wPos = duckPos_ + Vector3{ RandRange(-1.6f, 1.6f), 0.08f, RandRange(-1.6f, 1.6f) };
			Vector3 wVel = { RandRange(1.8f, 3.2f), RandRange(0.1f, 0.3f), RandRange(0.8f, 2.0f) };
			scene_->appParticleManager_->EmitDustWithVelocity(
				randEng, wPos, 0.75f, { 0.14f, 0.12f, 0.10f, 0.28f }, wVel, 0.35f,
				(smokeDarkTextureIndex_ != UINT32_MAX) ? smokeDarkTextureIndex_ : scene_->smokeTextureIndex_
			);
		}
	}

	// 3. 空中をゆっくり舞い散る羽毛（光らない本物の羽毛）
	featherTimer_ += realDeltaTime;
	if (featherTimer_ >= 0.25f && timer_ < kAct3Duration && scene_ && scene_->appParticleManager_)
	{
		featherTimer_ = 0.0f;
		auto& randEng = scene_->particleManager->GetRandomEngine();
		Vector3 fPos = duckPos_ + Vector3{ RandRange(-0.8f, 0.8f), RandRange(0.3f, 1.2f), RandRange(-0.8f, 0.8f) };
		uint32_t fTex = (featherTextureIndex_ != UINT32_MAX) ? featherTextureIndex_ : scene_->particleTextureB;
		scene_->appParticleManager_->EmitFeather(randEng, fPos, { 0.18f, 0.16f, 0.13f, 0.35f }, fTex);
	}
}

// -----------------------------------------------------------------------------
// カメラワーク（重厚な地面ローアングル ＆ 衛生兵駆けつけフォーカス ＆ シネマ暗転）
// -----------------------------------------------------------------------------

void GameOverSequenceController::UpdateCamera(float /*realDeltaTime*/)
{
	if (!scene_ || !scene_->camera_) return;

	Vector3 perp = { -flingDir_.z, 0.0f, flingDir_.x };
	Vector3 duckLandingPos = deathPos_ + flingDir_ * 1.55f;

	Vector3 camPos{};
	Vector3 lookTarget{};
	float camRoll = 0.0f;
	float camFov = initialCamFov_;

	if (timer_ < 0.38f)
	{
		// 【Phase 1: 致命被弾インパクト＆前方斜め3/4ダイナミック・スムーズトラッキングドリー】 (0.0s 〜 0.38s)
		// 被弾の瞬間からアヒルの飛翔に合わせてカメラが美しく滑らかに追従（EaseOutCubicで18mから一瞬で墜落するのを防止）！
		// カメラ距離を少し離し（約3.6m）、アヒルの顔やくちばしを美しく捉えつつ、宙を舞うライフルや戦場を広々とフレーミング。
		float u = timer_ / 0.38f;
		float smoothDolly = 1.0f - std::pow(1.0f - u, 3.0f); // 優雅な3次イージング追従

		// アヒル着地点の前方（胸・くちばし側）-1.90m・横（-perp）2.90m・高さ1.05m（ゆったりとしたフロント3/4、距離約3.6m）
		Vector3 targetCamPhase1 = duckLandingPos - flingDir_ * 1.90f - perp * 2.90f + Vector3{ 0.0f, 1.05f, 0.0f };
		camPos = LerpVector3(initialCamPos_, targetCamPhase1, smoothDolly);

		// 被弾の衝撃による微細なマイクロシェイク（時間減衰）
		float shock = 1.0f - u;
		camPos += perp * (std::sin(timer_ * 50.0f) * 0.022f * shock);
		camPos += Vector3{ 0.0f, std::cos(timer_ * 40.0f) * 0.016f * shock, 0.0f };

		// アヒルの顔・頭部（くちばし周辺）を画面の中心にピント追従
		lookTarget = duckPos_ + Vector3{ 0.0f, 0.32f, 0.0f };

		// 被弾衝撃による映画風ダッチアングル（画面が斜めに約 -4度 傾く）
		camRoll = -0.070f * EaseOutQuad(u);

		// 視野角のドラマチックな微圧縮
		camFov = LerpVector3({ initialCamFov_, 0.0f, 0.0f }, { 0.48f, 0.0f, 0.0f }, smoothDolly).x;
	}
	else if (timer_ < kAct1Duration)
	{
		// 【Phase 2: 地面激突インパクト＆バウンド滑走・顔フォーカス】 (0.38s 〜 0.85s)
		float u = (timer_ - 0.38f) / 0.47f;
		float ease = EaseOutQuad(u);

		// アヒルの滑走に合わせて優雅にバックドリフト（顔の正面斜めを常にキープしつつ広めの距離を保つ）
		float driftFling = ease * 0.22f;
		float driftPerp  = ease * 0.16f;
		Vector3 basePos = duckLandingPos - flingDir_ * (1.90f + driftFling) - perp * (2.90f + driftPerp)
			+ Vector3{ 0.0f, 1.05f + ease * 0.10f, 0.0f };

		// 💥 激突スラム縦揺れインパルス（地面に叩きつけられた瞬間の激震！）
		float slamDecay = (std::max)(0.0f, 1.0f - u * 2.5f);
		float slamY = -std::sin(u * kPi * 4.5f) * 0.038f * slamDecay;
		camPos = basePos + Vector3{ 0.0f, slamY, 0.0f };

		// アヒルの顔・くちばしをしっかりと画面中心に注視
		lookTarget = duckPos_ + Vector3{ 0.0f, 0.28f, 0.0f };

		// ダッチロールが水平に戻り始める
		camRoll = -0.070f * (1.0f - ease);

		// FOVの引き締め
		camFov = 0.48f - 0.02f * ease;
	}
	else if (timer_ < kAct2Duration)
	{
		// 【Phase 3: 倒れ込み静止・無念のアヒル表情シネマティックオービット旋回】 (0.85s 〜 1.70s)
		// 倒れたアヒルを中心に、引きのシネマディスタンス（約3.7m）をゆったり保ちながら
		// フロント3/4からサイド3/4へとアヒルの負傷した顔・くちばしを逃さず半円オービット旋回！
		// 立ち上る繊細な硝煙、散らばる羽毛、横たわる兵士の表情が広々とした戦場の中で映し出される。
		float u = (timer_ - kAct1Duration) / (kAct2Duration - kAct1Duration);
		float ease = EaseInOutCubic(u);

		// 相対オフセット（フロント3/4からサイド3/4へオービット旋回、距離約3.7m）
		Vector3 startOffset = flingDir_ * (-2.05f) - perp * 2.95f + Vector3{ 0.0f, 1.15f, 0.0f };
		Vector3 endOffset   = flingDir_ * (-0.65f) - perp * 3.35f + Vector3{ 0.0f, 1.35f, 0.0f };
		camPos = duckLandingPos + LerpVector3(startOffset, endOffset, ease);

		// 手持ちカメラの静かな呼吸感
		camPos.y += std::sin((timer_ - 0.85f) * 2.5f) * 0.008f;

		// 倒れたアヒルの顔〜胸を注視
		lookTarget = duckLandingPos + Vector3{ 0.0f, 0.26f, 0.0f };

		// 水平への復帰
		camRoll = 0.0f;

		// シネマ望遠レンズ（背景の美しい圧縮）
		camFov = 0.46f - 0.02f * ease;
	}
	else if (timer_ < kAct3Duration)
	{
		// 【Phase 4: 救難チーム駆けつけ＆担架搬送ワイドシネマ構図】 (1.70s 〜 2.90s)
		// アラーム音とともにカメラが広角に引き、手前に倒れたアヒル（顔が見える構図）、
		// 奥から全速力で駆け寄ってくる衛生兵2羽と担架を、広々とした戦場の中でワイドに捉える！
		float u = (timer_ - kAct2Duration) / (kAct3Duration - kAct2Duration);
		float ease = EaseInOutCubic(u);

		Vector3 startPos = duckLandingPos - flingDir_ * 0.65f - perp * 3.35f + Vector3{ 0.0f, 1.35f, 0.0f };
		Vector3 endPos   = duckLandingPos - flingDir_ * 1.95f - perp * 3.75f + Vector3{ 0.0f, 1.80f, 0.0f };
		camPos = LerpVector3(startPos, endPos, ease);

		// 手前の倒れたアヒル（顔）と奥の衛生兵チームの中間を注視（両者が広々とした画角で完璧に収まる）
		Vector3 teamCenter = (duckLandingPos + medicTeamPos_) * 0.5f;
		lookTarget = LerpVector3(duckLandingPos + Vector3{ 0.0f, 0.26f, 0.0f }, teamCenter + Vector3{ 0.0f, 0.30f, 0.0f }, ease * 0.60f);

		camRoll = 0.0f;
		camFov = 0.44f + 0.04f * ease;
	}
	else
	{
		// 【Phase 5: 担架収容・ヘビーシャッター閉鎖待機ワイド構図】 (2.80s 以降)
		// 衛生兵が担架を下ろしてアヒルの横に到着。倒れた兵士と救難チームを安定してワイド捕捉。
		camPos = duckLandingPos - flingDir_ * 1.95f - perp * 3.75f + Vector3{ 0.0f, 1.80f, 0.0f };
		Vector3 teamCenter = (duckLandingPos + medicTeamPos_) * 0.5f;
		lookTarget = teamCenter + Vector3{ 0.0f, 0.30f, 0.0f };

		camRoll = 0.0f;
		camFov = 0.48f;
	}

	Vector3 camRot = CalcLookAtRotation(camPos, lookTarget, camRoll);
	scene_->camera_->SetTranslate(camPos);
	scene_->camera_->SetRotate(camRot);
	scene_->camera_->SetFovY(camFov);
	scene_->camera_->Update();
}

void GameOverSequenceController::UpdateParticles(float /*realDeltaTime*/)
{
	// 硝煙・羽毛の更新は UpdateAtmosphere に統合
}

void GameOverSequenceController::FinishSequence()
{
	if (transitionTriggered_) return;
	transitionTriggered_ = true;

	if (scene_)
	{
		scene_->SetSceneData<RaidStats>("RaidStats", RaidStats::GetInstance());
	}
	// 軍用メディバック・ヘビーシャッタートランジションを開始（1.70秒）
	DeathTransition::GetInstance()->StartTransition("GAMEOVER", 1.70f);
}

void GameOverSequenceController::Skip()
{
	if (transitionTriggered_) return;
	transitionTriggered_ = true;

	if (scene_)
	{
		scene_->SetSceneData<RaidStats>("RaidStats", RaidStats::GetInstance());
	}
	// スキップ時もヘビーシャッターで素早く切り替え（1.00秒）
	DeathTransition::GetInstance()->StartTransition("GAMEOVER", 1.00f);
}

bool GameOverSequenceController::Project3DTo2D(const Vector3& pos3D, const Matrix4x4& vp, float screenW, float screenH, Vector2& outPos)
{
	float clipX = pos3D.x * vp.m[0][0] + pos3D.y * vp.m[1][0] + pos3D.z * vp.m[2][0] + vp.m[3][0];
	float clipY = pos3D.x * vp.m[0][1] + pos3D.y * vp.m[1][1] + pos3D.z * vp.m[2][1] + vp.m[3][1];
	float clipZ = pos3D.x * vp.m[0][2] + pos3D.y * vp.m[1][2] + pos3D.z * vp.m[2][2] + vp.m[3][2];
	float clipW = pos3D.x * vp.m[0][3] + pos3D.y * vp.m[1][3] + pos3D.z * vp.m[2][3] + vp.m[3][3];

	if (clipW <= 0.001f) return false;

	float ndcX = clipX / clipW;
	float ndcY = clipY / clipW;

	if (ndcX < -1.4f || ndcX > 1.4f || ndcY < -1.4f || ndcY > 1.4f) return false;

	outPos.x = (ndcX * 0.5f + 0.5f) * screenW;
	outPos.y = (-ndcY * 0.5f + 0.5f) * screenH;
	return true;
}

// -----------------------------------------------------------------------------
// 3Dレンダリング（吹き飛ぶライフル ＆ 衛生兵チーム・担架）
// -----------------------------------------------------------------------------

void GameOverSequenceController::Draw3D(const RenderContext& ctx)
{
	if (!isActive_) return;

	// 吹き飛ぶライフル（戦場に転がった銃器）
	if (droppedRifle_)
	{
		droppedRifle_->Draw(ctx);
	}

	// 駆けつける衛生兵チーム＆担架
	if (timer_ >= 1.60f)
	{
		if (stretcherBed_)
		{
			stretcherBed_->Draw(ctx);
		}
		if (medicRear_)
		{
			medicRear_->Draw(ctx);
		}
		if (medicFront_)
		{
			medicFront_->Draw(ctx);
		}
	}
}

// -----------------------------------------------------------------------------
// 映画風シネマスコープ黒帯・ダークヴィネット・救難無線交信字幕描画
// -----------------------------------------------------------------------------

void GameOverSequenceController::DrawUI()
{
#ifdef USE_IMGUI
	if (!isActive_) return;
	// ヘビーシャッター作動中は、シャッター側の軍用テレメトリ＆心電図フラットラインに完全移行
	if (DeathTransition::GetInstance() && DeathTransition::GetInstance()->IsTransitioning()) return;

	ImDrawList* drawList = ImGui::GetForegroundDrawList();
	ImVec2 disp = ImGui::GetIO().DisplaySize;
	if (disp.x <= 0.0f || disp.y <= 0.0f) return;

	// 映画風の重厚なダークレンズヴィネット（眩しいフラッシュ光は完全排除）
	if (darkVignetteAlpha_ > 0.05f)
	{
		float pulse = 0.85f + 0.15f * std::sin(timer_ * 4.5f);
		int vigA = static_cast<int>(180 * darkVignetteAlpha_ * pulse);
		drawList->AddRect(ImVec2(0.0f, 0.0f), disp, IM_COL32(0, 0, 0, vigA / 2), 0.0f, 0, 48.0f);
	}

	// 映画用シネマスコープ黒帯（上下 11%）
	float maxBarH = disp.y * 0.11f;
	float barRatio = (std::min)(1.0f, timer_ / 0.28f);
	float curBarH = maxBarH * barRatio;

	ImFont* fontDefault = (ImGui::GetIO().Fonts && ImGui::GetIO().Fonts->Fonts.Size > 0) ? ImGui::GetIO().Fonts->Fonts[0] : ImGui::GetFont();
	ImFont* fontSub     = (ImGui::GetIO().Fonts && ImGui::GetIO().Fonts->Fonts.Size > 2) ? ImGui::GetIO().Fonts->Fonts[2] : fontDefault;

	if (curBarH > 0.5f)
	{
		drawList->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(disp.x, curBarH), IM_COL32(0, 0, 0, 255));
		drawList->AddRectFilled(ImVec2(0.0f, disp.y - curBarH), ImVec2(disp.x, disp.y), IM_COL32(0, 0, 0, 255));

		// 下部: 洗練されたスキップ案内（自然な日本語プロンプト）
		float pulse = 0.55f + 0.45f * std::sin(timer_ * 5.0f);
		int alpha = static_cast<int>(210 * pulse);
		const char* skipPrompt = (const char*)u8"SPACE : 救難要請（スキップ）";
		ImVec2 sSize = fontDefault->CalcTextSizeA(14.0f, FLT_MAX, 0.0f, skipPrompt);
		drawList->AddText(fontDefault, 14.0f, ImVec2((disp.x - sSize.x) * 0.5f, disp.y - curBarH * 0.5f - sSize.y * 0.5f), IM_COL32(203, 213, 225, alpha), skipPrompt);
	}

	// (映画風シネマティック: 画面中央を塞ぐ余計なUI枠を全撤廃し、純粋な映像と無線字幕のみに洗練)

	// 救難無線字幕（GameOverSceneのヘリ搬送に直結するストーリーライン）
	if (timer_ >= 0.85f && timer_ < kAct2Duration)
	{
		const char* radioText = (const char*)u8"「メーデー、メーデー！ 味方が倒れた！ 緊急救難信号を発信！」";
		ImVec2 rSize = fontSub->CalcTextSizeA(20.0f, FLT_MAX, 0.0f, radioText);
		float textAlpha = (std::min)(1.0f, (timer_ - 0.85f) / 0.35f);
		int textA = static_cast<int>(240 * textAlpha);
		const float subX = (disp.x - rSize.x) * 0.5f;
		const float subY = disp.y * 0.80f - rSize.y * 0.5f;
		drawList->AddText(fontSub, 20.0f, ImVec2(subX + 1.5f, subY + 1.5f), IM_COL32(0, 0, 0, textA), radioText);
		drawList->AddText(fontSub, 20.0f, ImVec2(subX, subY), IM_COL32(245, 215, 120, textA), radioText);
	}
	else if (timer_ >= kAct2Duration && timer_ < kAct3Duration)
	{
		const char* radioText = (const char*)u8"「救難ヘリ1-1了解！ 衛生部隊が現場到着――担架を展開する！」";
		ImVec2 rSize = fontSub->CalcTextSizeA(20.0f, FLT_MAX, 0.0f, radioText);
		float textAlpha = (std::min)(1.0f, (timer_ - kAct2Duration) / 0.30f);
		int textA = static_cast<int>(240 * textAlpha);
		const float subX = (disp.x - rSize.x) * 0.5f;
		const float subY = disp.y * 0.80f - rSize.y * 0.5f;
		drawList->AddText(fontSub, 20.0f, ImVec2(subX + 1.5f, subY + 1.5f), IM_COL32(0, 0, 0, textA), radioText);
		drawList->AddText(fontSub, 20.0f, ImVec2(subX, subY), IM_COL32(245, 225, 130, textA), radioText);
	}

	// 終盤のブラックアウト ＆ 収容無線字幕
	if (fadeBlackAlpha_ > 0.01f)
	{
		int blackA = static_cast<int>(255 * fadeBlackAlpha_);
		drawList->AddRectFilled(ImVec2(0.0f, 0.0f), disp, IM_COL32(0, 0, 0, blackA));

		if (fadeBlackAlpha_ > 0.20f)
		{
			const char* radioText = (const char*)u8"「負傷兵の収容完了！ 救難ヘリへ急行する！」";
			ImVec2 rSize = fontSub->CalcTextSizeA(20.0f, FLT_MAX, 0.0f, radioText);
			int textA = static_cast<int>(255 * (fadeBlackAlpha_ - 0.20f) / 0.80f);
			const float subX = (disp.x - rSize.x) * 0.5f;
			const float subY = disp.y * 0.5f - rSize.y * 0.5f;
			drawList->AddText(fontSub, 20.0f, ImVec2(subX + 1.5f, subY + 1.5f), IM_COL32(0, 0, 0, textA), radioText);
			drawList->AddText(fontSub, 20.0f, ImVec2(subX, subY), IM_COL32(245, 215, 120, textA), radioText);
		}
	}
#endif
}
