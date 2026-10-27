#include "HelicopterInsertionController.h"
#include "GamePlayScene.h"
#include "SceneManager.h"
#include "SkyBox.h"
#include "Application/Config/GameConfig.h"
#include "Application/Particle/AppParticleManager.h"
#include "ParticleManager.h"
#include "TextureManager.h"
#include "Baziru3_Engine/Framework/Audio/AudioManager.h"
#include "Application/Transition/StingerTransition.h"
#include "../../Player/Player.h"
#include <cmath>
#include <algorithm>

#ifdef USE_IMGUI
#include "imgui.h"
#endif

namespace {
	constexpr float kPi = 3.14159265358979323846f;

	inline float Length3(const Vector3& v)
	{
		return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
	}

	inline float Rand01()
	{
		return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
	}

	inline float RandRange(float a, float b)
	{
		return a + (b - a) * Rand01();
	}

	// 任意の視点(eye)から注視点(target)を捉える正確なカメラ回転（Pitch, Yaw, Roll）を算出
	inline Vector3 CalcLookAtRotation(const Vector3& eye, const Vector3& target, float roll = 0.0f)
	{
		Vector3 dir = { target.x - eye.x, target.y - eye.y, target.z - eye.z };
		float xzLen = std::sqrt(dir.x * dir.x + dir.z * dir.z);
		float pitch = std::atan2(-dir.y, (std::max)(xzLen, 0.001f));
		float yaw = std::atan2(dir.x, dir.z);
		return { pitch, yaw, roll };
	}

	// 地上シャドウ用モデルデータ生成（円盤クアッドメッシュ）
	void BuildShadowDiscModel(Object3d::ModelData& data, float rx, float rz)
	{
		data.vertices.clear();
		data.indices.clear();
		Sprite::VertexData v0{}, v1{}, v2{}, v3{};
		v0.position = { -rx, 0.0f, -rz, 1.0f };
		v0.texcoord = { 0.0f, 0.0f };
		v0.normal = { 0.0f, 1.0f, 0.0f };

		v1.position = { rx, 0.0f, -rz, 1.0f };
		v1.texcoord = { 1.0f, 0.0f };
		v1.normal = { 0.0f, 1.0f, 0.0f };

		v2.position = { -rx, 0.0f, rz, 1.0f };
		v2.texcoord = { 0.0f, 1.0f };
		v2.normal = { 0.0f, 1.0f, 0.0f };

		v3.position = { rx, 0.0f, rz, 1.0f };
		v3.texcoord = { 1.0f, 1.0f };
		v3.normal = { 0.0f, 1.0f, 0.0f };

		data.vertices.push_back(v0);
		data.vertices.push_back(v1);
		data.vertices.push_back(v2);
		data.vertices.push_back(v3);

		data.indices.push_back(0);
		data.indices.push_back(1);
		data.indices.push_back(2);
		data.indices.push_back(1);
		data.indices.push_back(3);
		data.indices.push_back(2);
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

void HelicopterInsertionController::Initialize()
{
	if (!scene_ || !scene_->object3dCom) return;

	wasSkipped_ = false;
	heliTexIndex_ = TextureManager::GetInstance()->Load("Resources/helicopter_texture.png");
	whiteTexIndex_ = TextureManager::GetInstance()->Load("Resources/white.png");
	spotTexIndex_ = TextureManager::GetInstance()->Load("Resources/circle2.png");
	leafTexIndex_ = TextureManager::GetInstance()->Load("Resources/leaf.png");

	// サウンドのプリロード
	if (SceneManager::GetInstance() && SceneManager::GetInstance()->GetAudioManager())
	{
		duckSoundId_ = SceneManager::GetInstance()->GetAudioManager()->Load("Resources/Audio/SE_Duck.mp3");
	}

	uint32_t modelTexIdx = (heliTexIndex_ != TextureManager::kInvalidTextureIndex) ? heliTexIndex_ : whiteTexIndex_;

	// 1. ヘリコプター本体モデル (fuselage, cabin, tail, skids)
	{
		Object3d::ModelData bodyData = Object3d::LoadObjFile("Resources", "helicopter_body.obj");
		bodyData.material.textureFilePath = "Resources/helicopter_texture.png";
		bodyData.material.textureIndex = modelTexIdx;
		bodyData.boundingRadius = 150.0f;

		heliBody_ = std::make_unique<Object3d>();
		heliBody_->Initialize(scene_->object3dCom, bodyData);
		heliBody_->SetCamera(scene_->camera_);
		heliBody_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		heliBody_->SetEnableLighting(true);
		heliBody_->SetAllowWireframeOverlay(false);
	}

	// 2. メインローターブレード (4枚羽・カーボンブラック ＆ イエロー警戒帯)
	{
		Object3d::ModelData rotorData = Object3d::LoadObjFile("Resources", "helicopter_rotor.obj");
		rotorData.material.textureFilePath = "Resources/helicopter_texture.png";
		rotorData.material.textureIndex = modelTexIdx;
		rotorData.boundingRadius = 150.0f;

		heliMainRotor_ = std::make_unique<Object3d>();
		heliMainRotor_->Initialize(scene_->object3dCom, rotorData);
		heliMainRotor_->SetCamera(scene_->camera_);
		heliMainRotor_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		heliMainRotor_->SetEnableLighting(true);
		heliMainRotor_->SetAllowWireframeOverlay(false);
	}

	// 3. テールローターブレード (2枚羽・カーボンブラック ＆ イエロー警戒帯)
	{
		Object3d::ModelData tailData = Object3d::LoadObjFile("Resources", "helicopter_tailrotor.obj");
		tailData.material.textureFilePath = "Resources/helicopter_texture.png";
		tailData.material.textureIndex = modelTexIdx;
		tailData.boundingRadius = 150.0f;

		heliTailRotor_ = std::make_unique<Object3d>();
		heliTailRotor_->Initialize(scene_->object3dCom, tailData);
		heliTailRotor_->SetCamera(scene_->camera_);
		heliTailRotor_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		heliTailRotor_->SetEnableLighting(true);
		heliTailRotor_->SetAllowWireframeOverlay(false);
	}

	// 4. 軍用ファストロープ (Braided Fast Rope, 21.0m長)
	{
		Object3d::ModelData ropeData = Object3d::LoadObjFile("Resources", "fast_rope.obj");
		ropeData.material.textureFilePath = "Resources/helicopter_texture.png";
		ropeData.material.textureIndex = modelTexIdx;
		ropeData.boundingRadius = 50.0f;

		fastRope_ = std::make_unique<Object3d>();
		fastRope_->Initialize(scene_->object3dCom, ropeData);
		fastRope_->SetCamera(scene_->camera_);
		fastRope_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		fastRope_->SetEnableLighting(true);
		fastRope_->SetAllowWireframeOverlay(false);
	}

	// 5. ヘリコプターの地上投影シャドウ (白昼光ソフトプロジェクション)
	{
		Object3d::ModelData shadowData{};
		BuildShadowDiscModel(shadowData, 4.2f, 6.8f);
		shadowData.material.textureFilePath = "Resources/circle2.png";
		shadowData.material.textureIndex = (spotTexIndex_ != TextureManager::kInvalidTextureIndex) ? spotTexIndex_ : whiteTexIndex_;
		shadowData.boundingRadius = 25.0f;

		heliShadow_ = std::make_unique<Object3d>();
		heliShadow_->Initialize(scene_->object3dCom, shadowData);
		heliShadow_->SetCamera(scene_->camera_);
		heliShadow_->SetColor({ 0.04f, 0.06f, 0.08f, 0.65f });
		heliShadow_->SetEnableLighting(false);
		heliShadow_->SetAllowWireframeOverlay(false);
	}

	// 初期状態セットアップ: ヘリは超遠空 (X: -45m, Y: 32m, Z: -80m) から高速急襲
	isActive_ = true;
	timer_ = 0.0f;
	heliPos_ = { -45.0f, 32.0f, -80.0f };
	heliRot_ = { -0.15f, 0.08f, -0.32f };
	mainRotorAngle_ = 0.0f;
	tailRotorAngle_ = 0.0f;
	downwashDustTimer_ = 0.0f;
	ropeSparkTimer_ = 0.0f;
	landed_ = false;
	headSnapEmitted_ = false;
	eyeGlintEmitted_ = false;
	featherShakeEmitted_ = false;
	footstepDust1_ = false;
	footstepDust2_ = false;
	cockSparkEmitted_ = false;
	duckVoiced_ = false;

	currentCut_ = 1;
	prevCut_ = 1;
	cutFlashAlpha_ = 0.0f;
	impactFlashAlpha_ = 0.0f;

	duckPos_ = heliPos_ + Vector3{ 0.0f, 0.10f, 0.75f };
	duckScale_ = { 1.0f, 1.0f, 1.0f };
	duckRot_ = heliRot_;

	// プレイヤー初期位置（ヘリキャビン内搭乗待機）
	if (scene_->player_)
	{
		scene_->player_->SetIntroTransform(duckPos_, duckScale_, duckRot_);
	}

	// Cut 1 初回カメラ：低空樹木付近から遠空のヘリを仰ぎ見るシネマティック広角視点
	if (scene_->camera_)
	{
		Vector3 eye = { -22.0f, 7.5f, -32.0f };
		Vector3 target = heliPos_ + Vector3{ 0.0f, 0.4f, 0.0f };
		scene_->camera_->SetTranslate(eye);
		scene_->camera_->SetRotate(CalcLookAtRotation(eye, target, -0.06f));
		scene_->camera_->Update();
	}
}

void HelicopterInsertionController::Finalize()
{
	heliBody_.reset();
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

	// プレイヤーを地面 (0, 0, 0) で通常戦闘待機状態に完全復帰
	if (scene_ && scene_->player_)
	{
		scene_->player_->RestoreIntroPose();
	}

	// カメラを通常プレイ時のTPS斜め見下ろしプレイヤー追従へ合流
	if (scene_ && scene_->camera_ && scene_->player_)
	{
		Vector3 targetCamPos = scene_->player_->GetPosition() + (GameConfig::Camera::kDefaultFollowOffset * scene_->cameraZoom_);
		scene_->camera_->SetTranslate(targetCamPos);
		scene_->camera_->SetRotate(GameConfig::Camera::kFollowRotation);
		scene_->camera_->Update();
	}

	// 全3Dオブジェクトを通常TPSカメラのWVP行列で即座に同期
	if (scene_)
	{
		scene_->UpdateObstacles();
		if (scene_->extractionPadObject_)
		{
			scene_->extractionPadObject_->Update();
		}
		for (auto& enemy : scene_->enemies_)
		{
			if (enemy && enemy->GetObject3d())
			{
				enemy->GetObject3d()->Update();
			}
		}
		for (auto& t : scene_->targets_)
		{
			if (t)
			{
				t->Update(0.0f);
			}
		}
		if (scene_->player_ && scene_->player_->GetObject3d())
		{
			scene_->player_->GetObject3d()->Update();
		}
		if (SceneManager::GetInstance() && SceneManager::GetInstance()->GetSkyBox())
		{
			SceneManager::GetInstance()->GetSkyBox()->Update();
		}
	}

	// ヘリ・ロープ・影モデルを画面外へ退避
	heliPos_ = { 0.0f, -200.0f, 0.0f };
	if (heliShadow_)
	{
		heliShadow_->SetTranslate(Vector3{ 0.0f, -200.0f, 0.0f });
		heliShadow_->Update();
	}
}

void HelicopterInsertionController::Update(float deltaTime)
{
	if (!isActive_ || !scene_) return;

	// スキップ入力検知（SPACE, ENTER, または マウスクリック）
	bool skipPressed = false;
	if ((GetAsyncKeyState(VK_SPACE) & 0x8000) != 0 ||
		(GetAsyncKeyState(VK_RETURN) & 0x8000) != 0 ||
		scene_->mouseInput.PushButton(0))
	{
		skipPressed = true;
	}
#ifdef USE_IMGUI
	if (ImGui::IsKeyPressed(ImGuiKey_Space) || ImGui::IsKeyPressed(ImGuiKey_Enter))
	{
		skipPressed = true;
	}
#endif

	if (skipPressed)
	{
		Skip();
		return;
	}

	timer_ += deltaTime;

	// 映画・アニメ的 5幕シネマティック判定
	if (timer_ < kCut1Duration)
	{
		currentCut_ = 1;
	}
	else if (timer_ < kCut2Duration)
	{
		currentCut_ = 2;
	}
	else if (timer_ < kCut3Duration)
	{
		currentCut_ = 3;
	}
	else if (timer_ < kCut4Duration)
	{
		currentCut_ = 4;
	}
	else if (timer_ < kDuration)
	{
		currentCut_ = 5;
	}
	else
	{
		FinishInsertion();
		return;
	}

	// カット切り替え時の上品なホワイトフラッシュ
	if (currentCut_ != prevCut_)
	{
		prevCut_ = currentCut_;
		cutFlashAlpha_ = 0.50f;
	}

	// フラッシュの減衰
	if (cutFlashAlpha_ > 0.0f)
	{
		cutFlashAlpha_ = (std::max)(0.0f, cutFlashAlpha_ - 6.0f * deltaTime);
	}
	if (impactFlashAlpha_ > 0.0f)
	{
		impactFlashAlpha_ = (std::max)(0.0f, impactFlashAlpha_ - 4.5f * deltaTime);
	}

	// 【完全同期パイプライン】
	// 1. 各オブジェクトの目標ワールド座標・姿勢を算出
	UpdateHelicopterMotion(deltaTime);
	// 2. カメラのトランスフォームを更新し、ビュー・射影行列を即時再計算
	UpdateCamera(deltaTime);
	// 3. 最新のカメラ行列をもとにヘリ・ロープ・影・プレイヤーのWVPを更新
	UpdateModelTransforms(deltaTime);
	// 4. パーティクル・演出更新
	UpdateParticles(deltaTime);
}

void HelicopterInsertionController::UpdateHelicopterMotion(float deltaTime)
{
	// ローター超高速回転 (メインローター: 60 rad/s, テールローター: 85 rad/s)
	mainRotorAngle_ += 60.0f * deltaTime;
	if (mainRotorAngle_ > 2.0f * kPi) mainRotorAngle_ -= 2.0f * kPi;

	tailRotorAngle_ += 85.0f * deltaTime;
	if (tailRotorAngle_ > 2.0f * kPi) tailRotorAngle_ -= 2.0f * kPi;

	// ホバリング高度: 高空 (Y: 18.0m)
	const Vector3 kHoverPos = { 0.0f, 18.0f, 0.0f };

	if (currentCut_ == 1)
	{
		// =====================================================================
		// Act 1: 【白昼の超低空急襲・ロングレンジアプローチ＆フレア制動】 (0.0s 〜 3.40s, 計3.4s)
		// 90m彼方の青空からヘリがバンク旋回しながら猛スピードで急行接近し、
		// 降下地点（LZ）の直前で機首を大きく持ち上げて豪快なフレア制動でホバリングへ遷移！
		// =====================================================================
		float t = timer_ / kCut1Duration; // 0.0 -> 1.0

		Vector3 startPos = { -45.0f, 32.0f, -80.0f };

		// 緩やかなS字バンク旋回アプローチ（横方向へのスイング軌道）
		float brake = 1.0f - std::pow(1.0f - t, 2.6f);
		float sCurve = std::sin(t * 3.14159f) * 6.5f;

		Vector3 curPos = startPos + (kHoverPos - startPos) * brake;
		curPos.x += sCurve;
		heliPos_ = curPos;

		// 姿勢制御：前半は深い左バンク＆前傾急行、後半はノーズアップの強烈なフレア制動
		float pitch = 0.0f;
		float roll = 0.0f;
		float yaw = 0.05f;

		if (t < 0.60f)
		{
			float subT = t / 0.60f;
			pitch = -0.16f + 0.10f * subT;
			roll = -0.32f * (1.0f - subT * 0.5f);
		}
		else
		{
			float subT = (t - 0.60f) / 0.40f; // 0.0 -> 1.0
			float flare = std::sin(subT * 3.14159f);
			pitch = flare * 0.28f; // 強烈なフレアノーズアップ
			roll = -0.16f * (1.0f - subT);
		}

		heliRot_ = { pitch, yaw, roll };

		// プレイヤーはキャビン内に着席待機（広々とした機内中央床上に配置）
		duckPos_ = heliPos_ + Vector3{ 0.0f, 0.10f, 0.75f };
		duckScale_ = { 1.0f, 1.0f, 1.0f };
		duckRot_ = heliRot_;
	}
	else if (currentCut_ == 2)
	{
		// =====================================================================
		// Act 2: 【ドア前出撃準備・RED/GREEN LIGHT合図のタメ・飛び移り】 (3.40s 〜 5.60s, 計2.20s)
		// ヘリがホバリング。ロープが地上へ投下され、アヒル兵士がドア敷居へ歩み寄り、
		// 地上を目視確認して頷き（RED LIGHT待機）、GREEN LIGHT点灯と同時にロープへ飛び移る！
		// =====================================================================
		float phase = (timer_ - kCut1Duration) / (kCut2Duration - kCut1Duration); // 0.0 -> 1.0

		// ホバリング微気流揺動
		heliPos_ = kHoverPos + Vector3{
			std::sin(timer_ * 2.2f) * 0.04f,
			std::cos(timer_ * 1.8f) * 0.05f,
			0.0f
		};
		heliRot_ = { std::sin(timer_ * 1.5f) * 0.012f, 0.0f, std::cos(timer_ * 2.0f) * 0.012f };

		if (phase < 0.32f)
		{
			// 1. 序盤0.7s: 機内中央から開放ドア敷居（X = 1.40m）へと力強く前進歩行
			float stepProgress = phase / 0.32f;
			float smoothStep = stepProgress * stepProgress * (3.0f - 2.0f * stepProgress);

			duckPos_ = heliPos_ + Vector3{ 1.40f * smoothStep, 0.10f, 0.75f };
			duckRot_ = { 0.0f, -1.25f * smoothStep, 0.0f };
			duckScale_ = { 1.0f, 1.0f, 1.0f };
		}
		else if (phase < 0.68f)
		{
			// 2. 中盤0.8s: ドア敷居に立ち、地上を見下ろして出撃準備（RED LIGHT待機）
			// 4.6s付近で頭をコクッと下げる力強い頷き（Nod of readiness）
			float prepT = (phase - 0.32f) / 0.36f; // 0.0 -> 1.0
			float nod = 0.0f;
			if (prepT >= 0.50f && prepT < 0.85f)
			{
				float nodProgress = (prepT - 0.50f) / 0.35f;
				nod = std::sin(nodProgress * 3.14159f) * 0.18f; // 首を縦に振る頷き動作
			}
			float windBob = std::sin(timer_ * 6.5f) * 0.012f;
			duckPos_ = heliPos_ + Vector3{ 1.40f, 0.10f + windBob, 0.75f };
			duckRot_ = { 0.08f + nod, -1.30f, 0.04f }; // 身を乗り出す構え＋合図の頷き
			duckScale_ = { 1.0f, 1.0f, 1.0f };
		}
		else
		{
			// 3. 終盤0.7s: 【GREEN LIGHT点灯！ GO! GO! GO!】
			// ドア敷居からロープ（X = 1.65m）へ力強く身を乗り出して飛び移り、両足とグローブでがっしりホールド！
			float leapT = (phase - 0.68f) / 0.32f; // 0.0 -> 1.0
			float smoothLeap = leapT * leapT * (3.0f - 2.0f * leapT);

			float leapProgress = smoothLeap;
			float leapX = 1.40f + (1.34f - 1.40f) * leapProgress;
			float leapZ = 0.75f + (-0.25f - 0.75f) * leapProgress;
			float leapYaw = -1.30f + (0.30f - (-1.30f)) * leapProgress;
			float leapHop = std::sin(leapT * 3.14159f) * 0.10f;

			duckPos_ = heliPos_ + Vector3{ leapX, 0.10f + leapHop, leapZ };
			duckRot_ = { 0.06f * leapProgress, leapYaw, 0.0f };
			duckScale_ = { 1.0f + 0.05f * std::sin(leapT * 3.14159f), 1.0f - 0.05f * std::sin(leapT * 3.14159f), 1.0f };
		}
	}
	else if (currentCut_ == 3)
	{
		// =====================================================================
		// Act 3: 【緩急ファストロープ降下・急加速＆地上直前コンバットブレーキ】 (5.60s 〜 7.10s, 計1.50s)
		// 重力フリーフォールで急加速滑走し、地上3m直前でブーツとグローブをロープに強く
		// 押し当ててコンバットブレーキ！火花と摩擦煙を激しく噴き出して減速！
		// =====================================================================
		float phase = (timer_ - kCut2Duration) / (kCut3Duration - kCut2Duration); // 0.0 -> 1.0

		heliPos_ = kHoverPos + Vector3{
			std::sin(timer_ * 2.4f) * 0.03f,
			std::cos(timer_ * 2.0f) * 0.03f,
			0.0f
		};
		heliRot_ = { 0.0f, 0.0f, 0.0f };

		// 緩急のある降下カーブ（急加速 -> 地上直前急制動ブレーキ）
		float dropProgress = 0.0f;
		if (phase < 0.68f)
		{
			// 前半〜中盤：急加速で高度を一気に18mから地上4m付近へ（0.0 -> 0.82）
			float t = phase / 0.68f;
			dropProgress = 0.82f * (t * t);
		}
		else
		{
			// 終盤：地上3〜4mで強烈な摩擦ブレーキ！ググッと減速（0.82 -> 1.0）
			float t = (phase - 0.68f) / 0.32f;
			float brakeT = 1.0f - (1.0f - t) * (1.0f - t);
			dropProgress = 0.82f + 0.18f * brakeT;
		}

		float duckY = (1.0f - dropProgress) * (kHoverPos.y + 0.10f);
		if (duckY < 0.0f) duckY = 0.0f;

		const float kRopeX = 1.65f;
		const float kRopeZ = 0.75f;
		const float kFacingYaw = 0.30f;
		const float kRopeFrontOffset = 1.05f;

		// アヒル本体はロープの後方（胸の前1.05mにロープが来る位置）に完全固定して直滑降
		// 水平方向への不要なスイングやドリフトを一切排除し、ロープが胴体を貫通するのを物理的に完全防止！
		const float kDuckX = kRopeX - std::sin(kFacingYaw) * kRopeFrontOffset; // 1.34f
		const float kDuckZ = kRopeZ - std::cos(kFacingYaw) * kRopeFrontOffset; // -0.25f

		duckPos_ = { kDuckX, duckY, kDuckZ };

		if (phase < 0.68f)
		{
			// 加速滑空姿勢（身を細めて直滑降、胸の前でロープ把持）
			duckScale_ = { 0.94f, 1.06f, 0.94f };
			duckRot_ = { 0.06f, kFacingYaw, 0.0f };
		}
		else
		{
			// コンバットブレーキ姿勢（身体を起こして足を踏ん張り衝撃に備える）
			float bT = (phase - 0.68f) / 0.32f;
			duckScale_ = { 0.94f + 0.16f * bT, 1.06f - 0.14f * bT, 0.94f + 0.16f * bT };
			duckRot_ = { 0.06f + 0.08f * bT, kFacingYaw, 0.0f };
		}

		// 手袋・ブーツとロープの接触摩擦スパーク火花＆摩擦白煙（ロープ位置 X=1.65, Z=0.75 から激しく噴出！）
		if (phase > 0.05f && phase < 0.96f && scene_->particleManager && scene_->appParticleManager_)
		{
			ropeSparkTimer_ += deltaTime;
			float interval = (phase >= 0.68f) ? 0.015f : 0.035f;
			if (ropeSparkTimer_ >= interval)
			{
				ropeSparkTimer_ = 0.0f;
				Vector3 sparkP = { kRopeX, duckY + 0.65f, kRopeZ };
				float spdMul = (phase >= 0.68f) ? 2.6f : 1.2f;
				Vector3 spkVel = { RandRange(-spdMul, spdMul), RandRange(1.2f, 3.2f), RandRange(-spdMul, spdMul) };
				scene_->appParticleManager_->EmitSparkWithVelocity(
					scene_->particleManager->GetRandomEngine(),
					sparkP,
					spkVel,
					{ 1.0f, 0.90f, 0.40f, 1.0f },
					0.16f,
					(phase >= 0.68f) ? 0.32f : 0.22f,
					scene_->starburstTextureIndex_
				);

				// ブレーキ時の摩擦白煙
				if (phase >= 0.68f && scene_->smokeTextureIndex_ != TextureManager::kInvalidTextureIndex)
				{
					scene_->appParticleManager_->EmitDustWithVelocity(
						scene_->particleManager->GetRandomEngine(),
						sparkP + Vector3{ RandRange(-0.08f, 0.08f), -0.10f, RandRange(-0.08f, 0.08f) },
						0.45f,
						Vector4{ 0.88f, 0.88f, 0.88f, 0.45f },
						Vector3{ RandRange(-0.6f, 0.6f), RandRange(0.4f, 1.2f), RandRange(-0.6f, 0.6f) },
						0.45f,
						scene_->smokeTextureIndex_
					);
				}
			}
		}
	}
	else if (currentCut_ == 4)
	{
		// =====================================================================
		// Act 4: 【着地激突・左右セクター360クリアリング＆コッキング】 (7.10s 〜 8.80s, 計1.70s)
		// 激突着地から即座に立ち上がり、銃口を左右へ素早く振って周囲をクリアリング警戒！
		// 正面へビシッと構え直してショットガンコッキング＆サン・グリント！
		// =====================================================================
		float phase = (timer_ - kCut3Duration) / (kCut4Duration - kCut3Duration); // 0.0 -> 1.0

		// 着地インパクトの瞬間 (7.10s)
		if (!landed_)
		{
			landed_ = true;
			impactFlashAlpha_ = 0.85f;
			scene_->TriggerCameraShake(0.42f, 0.70f);

			Vector3 impactPoint = { 1.34f, 0.05f, -0.25f };

			// GPU着地バースト（爆風衝撃波 ＋ 48粒超高速スパーク ＋ 24方向爆風土煙）
			if (scene_->particleManager && scene_->appParticleManager_)
			{
				scene_->appParticleManager_->EmitLandingGPUBurst(
					scene_->particleManager->GetRandomEngine(),
					impactPoint,
					scene_->starburstTextureIndex_,
					scene_->smokeTextureIndex_,
					scene_->particleTextureB
				);

				// 地面を360度ドーナツ状に高速拡散する衝撃波ダストリング（24方向）
				for (int i = 0; i < 24; ++i)
				{
					float a = i * (2.0f * kPi / 24.0f);
					Vector3 sVel = { std::cos(a) * RandRange(3.8f, 5.5f), 0.25f, std::sin(a) * RandRange(3.8f, 5.5f) };
					scene_->appParticleManager_->EmitDustWithVelocity(
						scene_->particleManager->GetRandomEngine(),
						impactPoint,
						0.70f,
						Vector4{ 0.82f, 0.76f, 0.65f, 0.55f },
						sVel,
						0.45f,
						scene_->smokeTextureIndex_
					);
				}
			}

			// 出撃アヒルボイス
			if (!duckVoiced_ && duckSoundId_ >= 0 && SceneManager::GetInstance() && SceneManager::GetInstance()->GetAudioManager())
			{
				SceneManager::GetInstance()->GetAudioManager()->PlayOneShot(duckSoundId_, 1.0f);
				duckVoiced_ = true;
			}
		}

		// =====================================================================
		// アヒルの姿勢＆フルコンバットアクション：
		// 1. 【慣性前進スキッド3点着地 ＆ 沈み込み衝撃吸収】 (phase 0.00 〜 0.14)
		// 2. 【殺気ヘッドスナップ ＆ 戦闘アイ・グリント閃光】 (phase 0.14 〜 0.22)
		// 3. 【タクティカル・ブルルッ！（プルプル身震い羽毛ダストオフ）＆立ち上がり】 (phase 0.22 〜 0.36)
		// 4. 【ローセンター戦術歩行 ＆ 左右2セクターCQCクリアリング】 (phase 0.36 〜 0.70)
		// 5. 【正面スナップ ➡️ ポンプアクション引き込み ➡️ ボルト激突＆排莢＆閃光】 (phase 0.70 〜 0.86)
		// 6. 【正面ターゲットロック・アイドリング呼吸】 (phase 0.86 〜 1.00)
		// =====================================================================

		if (phase < 0.14f)
		{
			// 1. 【慣性前進スキッド3点着地 ＆ 沈み込み衝撃吸収】 (0.00 〜 0.14)
			// ロープ降下の着地慣性で (1.34f, 0.0f, -0.25f) から (1.10f, 0.0f, -0.20f) へ地面をスキッド滑走！
			float skidT = phase / 0.14f;
			float smoothSkid = skidT * skidT * (3.0f - 2.0f * skidT);
			float squatT = phase / 0.12f;
			float squat = (phase < 0.12f) ? (1.0f - squatT * 0.35f) : 0.65f;

			Vector3 skidStart = { 1.34f, 0.0f, -0.25f };
			Vector3 skidEnd   = { 1.10f, 0.0f, -0.20f };
			Vector3 curPos = skidStart + (skidEnd - skidStart) * smoothSkid;

			duckPos_ = { curPos.x, -0.26f * squat, curPos.z };
			duckScale_ = { 1.42f + 0.08f * squat, 0.52f - 0.08f * squat, 1.38f + 0.08f * squat };
			// スライド着地姿勢：深く前傾し、左手接地＆右腕で武器を抱え込むバンク傾き
			duckRot_ = { 0.44f * squat, 0.28f, -0.09f * (1.0f - smoothSkid) };

			// 地面スキッド摩擦土煙
			if (scene_->particleManager && scene_->appParticleManager_)
			{
				Vector3 skidPoint = duckPos_ + Vector3{ RandRange(-0.08f, 0.08f), 0.05f, RandRange(0.08f, 0.20f) };
				scene_->appParticleManager_->EmitDustWithVelocity(
					scene_->particleManager->GetRandomEngine(),
					skidPoint,
					0.35f,
					Vector4{ 0.80f, 0.74f, 0.62f, 0.40f },
					Vector3{ RandRange(-0.3f, 0.3f), 0.2f, RandRange(1.0f, 2.2f) },
					0.30f,
					scene_->smokeTextureIndex_
				);
			}
		}
		else if (phase < 0.22f)
		{
			// 2. 【殺気ヘッドスナップ ＆ 戦闘アイ・グリント閃光！】 (0.14 〜 0.22)
			float snapT = (phase - 0.14f) / 0.08f;
			float snapSmooth = snapT * snapT * (3.0f - 2.0f * snapT);
			duckScale_ = { 1.25f - 0.18f * snapSmooth, 0.70f + 0.18f * snapSmooth, 1.22f - 0.18f * snapSmooth };
			duckPos_ = { 1.10f, -0.16f * (1.0f - snapSmooth), -0.20f };
			// 伏せた姿勢から正面へガッと鋭利に頭を持ち上げる
			float pitch = 0.36f * (1.0f - snapSmooth) - 0.08f * snapSmooth;
			duckRot_ = { pitch, 0.28f, 0.0f };

			if (!headSnapEmitted_)
			{
				headSnapEmitted_ = true;
				scene_->TriggerCameraShake(0.14f, 0.14f);
			}

			// 瞳の戦闘キラーン光（アイ・グリント閃光）
			if (!eyeGlintEmitted_ && phase >= 0.16f && scene_->particleManager && scene_->appParticleManager_)
			{
				eyeGlintEmitted_ = true;
				Vector3 glintPos = duckPos_ + Vector3{ -0.06f, 0.44f, 0.30f };
				scene_->appParticleManager_->EmitMuzzleFlare(
					scene_->particleManager->GetRandomEngine(),
					glintPos,
					0.55f,
					Vector4{ 0.85f, 1.0f, 1.0f, 0.95f },
					0.14f,
					scene_->starburstTextureIndex_
				);
			}
		}
		else if (phase < 0.36f)
		{
			// 3. 【タクティカル・ブルルッ！（プルプル身震い羽毛ダストオフ）＆立ち上がり】 (0.22 〜 0.36)
			float riseT = (phase - 0.22f) / 0.14f;
			float ruffleFreq = riseT * 6.2831853f * 4.0f; // 高速身震い（4回転振動）
			float ruffleEnv = std::sin(riseT * 3.14159265f); // 立ち上がりピークで最大振動
			float roll = std::sin(ruffleFreq) * (ruffleEnv * 0.20f);
			float pitch = -0.08f + std::abs(roll) * 0.6f;

			duckScale_ = { 1.0f - ruffleEnv * 0.10f, 1.0f + ruffleEnv * 0.16f, 1.0f - ruffleEnv * 0.10f };
			duckPos_ = { 1.10f, 0.03f * ruffleEnv, -0.20f };
			duckRot_ = { pitch, 0.28f * (1.0f - riseT * 0.6f), roll };

			// 身震いで舞い散る白羽パーティクル
			if (!featherShakeEmitted_ && phase >= 0.25f)
			{
				featherShakeEmitted_ = true;
				if (scene_->particleManager && scene_->appParticleManager_)
				{
					for (int i = 0; i < 6; ++i)
					{
						scene_->appParticleManager_->EmitFeather(
							scene_->particleManager->GetRandomEngine(),
							duckPos_ + Vector3{ RandRange(-0.25f, 0.25f), RandRange(0.20f, 0.55f), RandRange(-0.20f, 0.20f) },
							Vector4{ 0.98f, 0.98f, 0.94f, 0.90f },
							scene_->particleTextureB
						);
					}
				}
			}
		}
		else if (phase < 0.70f)
		{
			// 4. 【ローセンター戦術歩行 ＆ 左右2セクターCQCクリアリング】 (0.36 〜 0.70)
			// (1.10f, 0.0f, -0.20f) から 正面 (0.0f, 0.0f, 0.0f) へ滑らかに戦術歩行
			float strideT = (phase - 0.36f) / 0.34f;
			float smoothStride = strideT * strideT * (3.0f - 2.0f * strideT);
			// 踏み込みの上下ボビング
			float bob = std::abs(std::sin(strideT * 3.14159265f * 2.0f)) * 0.045f;
			duckPos_ = {
				1.10f * (1.0f - smoothStride),
				bob,
				-0.20f * (1.0f - smoothStride)
			};
			duckScale_ = { 1.0f, 1.0f, 1.0f };

			// 足元の踏み込み土煙
			if (phase >= 0.44f && !footstepDust1_ && scene_->particleManager && scene_->appParticleManager_)
			{
				footstepDust1_ = true;
				scene_->appParticleManager_->EmitFootstepDust(
					scene_->particleManager->GetRandomEngine(),
					duckPos_,
					scene_->smokeTextureIndex_
				);
			}
			if (phase >= 0.60f && !footstepDust2_ && scene_->particleManager && scene_->appParticleManager_)
			{
				footstepDust2_ = true;
				scene_->appParticleManager_->EmitFootstepDust(
					scene_->particleManager->GetRandomEngine(),
					duckPos_,
					scene_->smokeTextureIndex_
				);
			}

			if (phase < 0.53f)
			{
				// Sector 1: 【左9時方向へ鋭くクリアリングスナップ！】 (0.36 〜 0.53)
				float scanT = (phase - 0.36f) / 0.17f;
				float snapWeight = (std::min)(scanT / 0.36f, 1.0f);
				float smoothSnap = snapWeight * snapWeight * (3.0f - 2.0f * snapWeight);
				float yaw = 0.12f * (1.0f - smoothSnap) + (-0.78f) * smoothSnap; // -45度左方向
				duckRot_ = { 0.03f, yaw, -0.06f * smoothSnap }; // バンク角
			}
			else
			{
				// Sector 2: 【右2時方向へ一気にクロスホイップスキャン！】 (0.53 〜 0.70)
				float scanT = (phase - 0.53f) / 0.17f;
				float snapWeight = (std::min)(scanT / 0.36f, 1.0f);
				float smoothSnap = snapWeight * snapWeight * (3.0f - 2.0f * snapWeight);
				float yaw = -0.78f * (1.0f - smoothSnap) + 0.68f * smoothSnap; // +39度右方向
				duckRot_ = { 0.03f, yaw, 0.06f * smoothSnap }; // バンク角
			}
		}
		else
		{
			// 5〜6. 【正面スナップ ➡️ ポンプアクション引き込み ➡️ ボルト激突＆排莢＆閃光 ➡️ ロックオン呼吸】 (0.70 〜 1.00)
			if (phase < 0.75f)
			{
				// 5-A. 正面12時方向へビシッとスナップ！ (0.70 〜 0.75)
				float frontT = (phase - 0.70f) / 0.05f;
				float sFront = frontT * frontT * (3.0f - 2.0f * frontT);
				duckRot_ = { 0.02f, 0.68f * (1.0f - sFront), 0.0f };
				duckPos_ = { 0.0f, 0.0f, 0.0f };
				duckScale_ = { 1.0f, 1.0f, 1.0f };
			}
			else if (phase < 0.80f)
			{
				// 5-B. 【ショットガン・スライド後方引き込みコッキング！】 (0.75 〜 0.80)
				float rackT = (phase - 0.75f) / 0.05f;
				float sRack = rackT * rackT * (3.0f - 2.0f * rackT);
				duckPos_ = { 0.0f, 0.015f * sRack, -0.10f * sRack };
				duckRot_ = { -0.18f * sRack, 0.0f, 0.06f * sRack }; // 銃口を跳ね上げながら引き込む
				duckScale_ = { 0.94f, 1.06f, 0.94f };
			}
			else if (phase < 0.86f)
			{
				// 5-C. 【ガシャンッ！！ ボルト前進激突＆真鍮薬莢エジェクト排莢！】 (0.80 〜 0.86)
				float slamT = (phase - 0.80f) / 0.06f;
				float rebound = std::sin(slamT * 3.14159265f);
				duckPos_ = { 0.0f, 0.0f, 0.05f * rebound };
				duckRot_ = { 0.07f * rebound, 0.0f, 0.0f };
				duckScale_ = { 1.0f + 0.09f * rebound, 1.0f - 0.09f * rebound, 1.0f + 0.09f * rebound };
			}
			else
			{
				// 6. 【戦闘態勢ロックオン完了・静かな呼吸アイドリング】 (0.86 〜 1.00)
				float idleT = (phase - 0.86f) / 0.14f;
				float breath = std::sin(idleT * 3.14159265f * 2.0f) * 0.012f;
				duckPos_ = { 0.0f, breath, 0.0f };
				duckRot_ = { 0.0f, 0.0f, 0.0f };
				duckScale_ = { 1.0f + breath * 0.5f, 1.0f - breath * 0.5f, 1.0f };
			}
		}

		// 武器コッキング＆真鍮薬莢スピン排莢 ＆ 白昼サン・グリント（ボルト激突瞬間の phase 0.80）
		if (!cockSparkEmitted_ && phase >= 0.80f && scene_->particleManager && scene_->appParticleManager_)
		{
			cockSparkEmitted_ = true;
			// 1. 強烈なカメラパンチシェイク
			scene_->TriggerCameraShake(0.26f, 0.30f);

			// 2. 右側面への真鍮ショットシェル（薬莢）高速スピン排莢！
			Vector3 chamberPos = { 0.22f, 0.50f, 0.25f };
			Vector3 forwardDir = { 0.0f, 0.0f, 1.0f };
			uint32_t shellTex = (scene_->cylinderTextureIndex_ != TextureManager::kInvalidTextureIndex)
				? scene_->cylinderTextureIndex_
				: (scene_->starburstTextureIndex_ != TextureManager::kInvalidTextureIndex ? scene_->starburstTextureIndex_ : scene_->particleTextureB);
			scene_->appParticleManager_->EmitShellCasing(
				scene_->particleManager->GetRandomEngine(),
				chamberPos,
				forwardDir,
				Vector4{ 1.0f, 0.84f, 0.32f, 1.0f }, // 黄金色に輝く真鍮薬莢
				Vector3{ 0.08f, 0.18f, 0.08f },
				shellTex
			);

			// 3. 銃口マズルフレア＆太陽光サン・グリント爆光（放射スパーク14本）
			Vector3 glintPos = { 0.28f, 0.52f, 0.52f };
			scene_->appParticleManager_->EmitMuzzleFlare(
				scene_->particleManager->GetRandomEngine(),
				glintPos,
				1.05f,
				Vector4{ 1.0f, 0.98f, 0.85f, 1.0f },
				0.24f,
				scene_->starburstTextureIndex_
			);
			for (int i = 0; i < 14; ++i)
			{
				float a = i * (2.0f * kPi / 14.0f);
				Vector3 spkVel = { std::cos(a) * 3.2f, std::sin(a) * 3.2f + 0.4f, 0.8f };
				scene_->appParticleManager_->EmitSparkWithVelocity(
					scene_->particleManager->GetRandomEngine(),
					glintPos,
					spkVel,
					Vector4{ 1.0f, 0.95f, 0.80f, 1.0f },
					0.18f,
					0.26f,
					scene_->starburstTextureIndex_
				);
			}
		}

		// ヘリは離脱準備で機首を下げ始める
		heliPos_ = kHoverPos + Vector3{ 0.0f, std::sin(timer_ * 2.0f) * 0.03f, 0.0f };
		heliRot_ = { 0.14f * phase, 0.0f, 0.0f };
	}
	else
	{
		// =====================================================================
		// Act 5: 【ヘリ急上昇離脱＆シームレスTPS合流】 (6.30s 〜 7.20s, 計0.90s)
		// =====================================================================
		float phase = (timer_ - kCut4Duration) / (kDuration - kCut4Duration); // 0.0 -> 1.0
		float accel = phase * phase * (3.0f - 2.0f * phase);

		// 機首を下げてエンジン全開、北空（Z: +120m, Y: 42m）へ急上昇離脱！
		heliPos_ = kHoverPos + Vector3{
			std::sin(phase * 3.14159f) * 3.5f,
			24.0f * (phase * phase),
			120.0f * accel
		};
		heliRot_ = { 0.32f * (1.0f - phase * 0.4f), 0.0f, -0.15f * phase };

		duckPos_ = { 0.0f, 0.0f, 0.0f };
		duckScale_ = { 1.0f, 1.0f, 1.0f };
		duckRot_ = { 0.0f, 0.0f, 0.0f };
	}
}

void HelicopterInsertionController::UpdateCamera(float deltaTime)
{
	if (!scene_ || !scene_->camera_) return;

	Vector3 currentCamPos{};
	Vector3 currentCamRot{};

	if (currentCut_ == 1)
	{
		// =====================================================================
		// Act 1: 【白昼の超低空急襲・ロングレンジアプローチ＆フレア制動】 (0.0s 〜 3.40s)
		// カメラワーク：低空の樹木・地面近くから見上げるダイナミック・パン＆トラッキング！
		// 遠くの地平線から轟音とともに迫り来るヘリを追い、頭上付近で制動する姿を大迫力で捉える！
		// =====================================================================
		float t = timer_ / kCut1Duration;
		float smoothT = t * t * (3.0f - 2.0f * t);

		Vector3 camP0 = { -22.0f, 7.5f, -32.0f };  // 地面・木々の低空アングル
		Vector3 camP1 = { -15.0f, 13.0f, -24.0f }; // ホバリングを仰ぎ見る見上げアングル

		Vector3 eye = camP0 + (camP1 - camP0) * smoothT;
		// 接近に伴うローターの重低音カメラ振動（ヘリが近づくほど強まる）
		eye.y += std::sin(timer_ * 28.0f) * (0.008f + 0.020f * smoothT);

		Vector3 target = heliPos_ + Vector3{ 0.0f, 0.4f, 0.0f };
		currentCamPos = eye;
		currentCamRot = CalcLookAtRotation(eye, target, -0.06f * (1.0f - smoothT));
	}
	else if (currentCut_ == 2)
	{
		// =====================================================================
		// Act 2: 【ドア前出撃準備・RED/GREEN LIGHT合図のタメ・飛び移り】 (3.40s 〜 5.60s)
		// ヘリ後方上空から開放ドア・アヒル兵士・眼下の戦場降下ゾーンを見下ろすハイアングル俯瞰！
		// 空虚なSkyboxではなく、眼下に広がる広大なステージ・ヘリパッド・土嚢陣地を捉える
		// =====================================================================
		float phase = (timer_ - kCut1Duration) / (kCut2Duration - kCut1Duration);
		float camT = phase * phase * (3.0f - 2.0f * phase);

		// ヘリ後方上空（X: +3.2, Y: +3.2, Z: -2.8）から、ドアと眼下の地上を見下ろす（下向きピッチ約-36度）
		Vector3 camStart = heliPos_ + Vector3{ 3.2f, 3.2f, -2.8f };
		Vector3 camEnd   = heliPos_ + Vector3{ 2.5f, 2.6f, -2.0f };
		if (phase >= 0.68f)
		{
			// GREEN LIGHT点灯・飛び移り時はロープと飛び出すアヒルへさらにフォーカス
			camEnd = heliPos_ + Vector3{ 2.0f, 2.2f, -1.6f };
		}
		Vector3 eye = camStart + (camEnd - camStart) * camT;
		Vector3 target = heliPos_ + Vector3{ duckPos_.x - heliPos_.x, 0.20f, 0.75f };
		currentCamPos = eye;
		currentCamRot = CalcLookAtRotation(eye, target, 0.04f);
	}
	else if (currentCut_ == 3)
	{
		// =====================================================================
		// Act 3: 【緩急ファストロープ降下・急加速＆地上直前コンバットブレーキ】 (5.60s 〜 7.10s)
		// 降下開始時はアヒル上方後方から眼下の着地ゾーン（ヘリパッド・地上）を見下ろす追尾ダイブ！
		// 地上が画面いっぱいに迫り上がり、Skyboxではなく戦場へのダイナミック突入感を演出！
		// 地上接近（後半）で滑らかに地上ローアングル（X=-0.5, Z=3.4）へと旋回ドリーイン
		// =====================================================================
		float phase = (timer_ - kCut2Duration) / (kCut3Duration - kCut2Duration);

		// 上空追尾カメラ（眼下の地上を大きく見下ろす俯瞰ダイブ視点）
		Vector3 highCamPos = Vector3{ 1.8f, duckPos_.y + 2.8f, -2.2f };
		Vector3 highTarget = Vector3{ duckPos_.x, duckPos_.y - 0.3f, duckPos_.z + 0.5f };

		// 地上着地カメラ（Act 4の camP0 へ完全一致するミドルアングル）
		Vector3 lowCamPos = Vector3{ -0.5f, 1.25f, 3.4f };
		Vector3 lowTarget = Vector3{ duckPos_.x, 0.45f, duckPos_.z };

		// 後半（phase >= 0.45, 地上高6m以下）にかけて、地上カメラへ滑らかにスウープ合流
		float blendT = (std::max)(0.0f, (phase - 0.45f) / 0.55f);
		float blendSmooth = blendT * blendT * (3.0f - 2.0f * blendT);

		Vector3 eye = highCamPos + (lowCamPos - highCamPos) * blendSmooth;
		Vector3 target = highTarget + (lowTarget - highTarget) * blendSmooth;

		currentCamPos = eye;
		currentCamRot = CalcLookAtRotation(eye, target, 0.03f * (1.0f - blendSmooth));
	}
	else if (currentCut_ == 4)
	{
		// =====================================================================
		// Act 4: 【着地激突・左右セクター360クリアリング＆コッキング】 (7.10s 〜 8.80s)
		// 着地激突から立ち上がり＆クリアリングスキャン、正面構えまで適度な距離（Z: 3.4m 〜 4.4m）を保ち、
		// アヒルの全身・足元土煙・舞い散る羽毛・真鍮排莢アクションをワイドに見渡せるシネマティック構図
		// =====================================================================
		float phase = (timer_ - kCut3Duration) / (kCut4Duration - kCut3Duration);
		float camT = phase * phase * (3.0f - 2.0f * phase);

		Vector3 camP0 = { -0.5f, 1.25f, 3.4f }; // 着地激突時：Act 3から完全接続するローミドルアングル
		Vector3 camP1 = {  0.0f, 1.35f, 4.4f }; // 正面ロックオン構え時：全身が綺麗に収まるミディアムフル構図

		Vector3 eye = camP0 + (camP1 - camP0) * camT;

		// カメラロール傾き＆着地衝撃沈み込み：着地ダッチアングル ➡️ 左右クリアリング追従 ➡️ 正面水平
		float camRoll = 0.0f;
		if (phase < 0.18f)
		{
			float dip = (1.0f - phase / 0.18f);
			eye.y -= 0.12f * dip * dip; // カメラも着地衝撃でグッと下へ沈み込む！
			camRoll = -0.05f * dip;     // キネティック・ダッチアングル傾き！
		}
		else if (phase >= 0.36f && phase < 0.53f)
		{
			// 左セクタースキャン追従傾き
			camRoll = -0.028f * std::sin((phase - 0.36f) / 0.17f * 3.14159265f);
		}
		else if (phase >= 0.53f && phase < 0.70f)
		{
			// 右セクタースキャン追従傾き
			camRoll = 0.028f * std::sin((phase - 0.53f) / 0.17f * 3.14159265f);
		}

		// ボルト前進激突＆排莢の瞬間に映画的マイクロパンチズーム！ (phase 0.78 〜 0.86)
		if (phase >= 0.78f && phase < 0.86f)
		{
			float punchT = (phase - 0.78f) / 0.08f;
			float punch = std::sin(punchT * 3.14159265f) * 0.25f;
			eye.z -= punch;
			eye.y -= punch * 0.15f;
		}

		// 視線注視点：duckPos_ の旋回に合わせてわずかに先行して追従
		float targetX = duckPos_.x * 0.6f;
		if (phase >= 0.36f && phase < 0.53f)
		{
			targetX -= 0.15f * std::sin((phase - 0.36f) / 0.17f * 3.14159265f);
		}
		else if (phase >= 0.53f && phase < 0.70f)
		{
			targetX += 0.15f * std::sin((phase - 0.53f) / 0.17f * 3.14159265f);
		}
		Vector3 target = { targetX, 0.65f, duckPos_.z * 0.6f };
		currentCamPos = eye;
		currentCamRot = CalcLookAtRotation(eye, target, camRoll);
	}
	else
	{
		// =====================================================================
		// Act 5: 【ヘリ急上昇離脱＆シームレスTPSドリーアウト】 (8.80s 〜 9.70s)
		// 地上ヒーロー視点（Z: 4.4m）から、上昇気流に乗って通常TPSカメラへ滑らかにドリーアウト！
		// =====================================================================
		float phase = (timer_ - kCut4Duration) / (kDuration - kCut4Duration);
		float smoothT = phase * phase * (3.0f - 2.0f * phase);

		Vector3 camStartPos = { 0.0f, 1.35f, 4.4f };
		Vector3 camEndPos = scene_->player_->GetPosition() + (GameConfig::Camera::kDefaultFollowOffset * scene_->cameraZoom_);

		// 素早く高度を稼いで地上の土嚢・柵の上空を安全に抜け、通常TPS位置へ上昇ドリーアウト
		float heightT = std::sin(smoothT * 1.5707963f);
		currentCamPos.y = camStartPos.y + (camEndPos.y - camStartPos.y) * heightT;
		currentCamPos.z = camStartPos.z + (camEndPos.z - camStartPos.z) * smoothT;
		currentCamPos.x = camStartPos.x + (camEndPos.x - camStartPos.x) * smoothT - std::sin(smoothT * 3.14159265f) * 1.2f;

		// カメラ注視点を常にプレイヤー中心に合わせることで、不自然な回転揺れを防ぎ被写体を安定捕捉
		Vector3 lookTarget = { scene_->player_->GetPosition().x, 0.70f, scene_->player_->GetPosition().z };
		Vector3 lookRot = CalcLookAtRotation(currentCamPos, lookTarget, 0.0f);
		Vector3 camEndRot = GameConfig::Camera::kFollowRotation;

		// 終盤(smoothT > 0.65)でTPSの標準見下ろし角(0.805, 0, 0)に完全一致するようにスムーズブレンド
		float blendFactor = (std::max)(0.0f, (smoothT - 0.65f) / 0.35f);
		float blendSmooth = blendFactor * blendFactor * (3.0f - 2.0f * blendFactor);
		currentCamRot = {
			lookRot.x + (camEndRot.x - lookRot.x) * blendSmooth,
			lookRot.y * (1.0f - blendSmooth),
			0.0f
		};
	}

	scene_->camera_->SetTranslate(currentCamPos);
	scene_->camera_->SetRotate(currentCamRot);
	scene_->camera_->Update();
}

void HelicopterInsertionController::UpdateModelTransforms(float deltaTime)
{
	// 3Dモデルトランスフォームの反映（※最新カメラのWVP行列を用いて更新）
	if (heliBody_)
	{
		heliBody_->SetTranslate(heliPos_);
		heliBody_->SetRotate(heliRot_);
		heliBody_->SetScale({ 1.0f, 1.0f, 1.0f });
		heliBody_->Update();
	}

	if (heliMainRotor_)
	{
		Matrix4x4 R = Multiply(MakeRotateXMatrix(heliRot_.x), Multiply(MakeRotateYMatrix(heliRot_.y), MakeRotateZMatrix(heliRot_.z)));
		Vector3 mastOffset = { 0.0f, 4.15f, 0.75f };
		Vector3 mastWorld = {
			mastOffset.x * R.m[0][0] + mastOffset.y * R.m[1][0] + mastOffset.z * R.m[2][0],
			mastOffset.x * R.m[0][1] + mastOffset.y * R.m[1][1] + mastOffset.z * R.m[2][1],
			mastOffset.x * R.m[0][2] + mastOffset.y * R.m[1][2] + mastOffset.z * R.m[2][2]
		};

		heliMainRotor_->SetTranslate(heliPos_ + mastWorld);
		heliMainRotor_->SetRotate({ heliRot_.x, heliRot_.y + mainRotorAngle_, heliRot_.z });
		heliMainRotor_->SetScale({ 1.0f, 1.0f, 1.0f });
		heliMainRotor_->Update();
	}

	if (heliTailRotor_)
	{
		Matrix4x4 R = Multiply(MakeRotateXMatrix(heliRot_.x), Multiply(MakeRotateYMatrix(heliRot_.y), MakeRotateZMatrix(heliRot_.z)));
		Vector3 tailOffset = { 0.20f, 3.35f, -8.60f };
		Vector3 tailWorld = {
			tailOffset.x * R.m[0][0] + tailOffset.y * R.m[1][0] + tailOffset.z * R.m[2][0],
			tailOffset.x * R.m[0][1] + tailOffset.y * R.m[1][1] + tailOffset.z * R.m[2][1],
			tailOffset.x * R.m[0][2] + tailOffset.y * R.m[1][2] + tailOffset.z * R.m[2][2]
		};

		heliTailRotor_->SetTranslate(heliPos_ + tailWorld);
		heliTailRotor_->SetRotate({ heliRot_.x + tailRotorAngle_, heliRot_.y, heliRot_.z });
		heliTailRotor_->SetScale({ 1.0f, 1.0f, 1.0f });
		heliTailRotor_->Update();
	}

	// ヘリコプターの地上投影シャドウ (昼光ソフトシャドウ)
	if (heliShadow_)
	{
		// 地上高0.035m（草地テクスチャとZファイティングしない高さ）
		// 太陽光の傾き（南東からの日射）に合わせてわずかに北西にオフセット
		Vector3 shadowPos = { heliPos_.x + 0.10f * heliPos_.y, 0.035f, heliPos_.z + 0.15f * heliPos_.y };
		float heightRatio = std::clamp(heliPos_.y / 24.0f, 0.5f, 2.5f);
		float shadowScale = 2.4f + 0.35f * (heightRatio - 1.0f);
		float shadowAlpha = std::clamp(0.68f - 0.008f * heliPos_.y, 0.15f, 0.65f);

		// 離脱中（Act 5）は離脱加速に伴い滑らかに薄くなる
		if (currentCut_ == 5)
		{
			float exitPhase = (timer_ - kCut4Duration) / (kDuration - kCut4Duration);
			shadowAlpha *= (1.0f - exitPhase);
		}

		heliShadow_->SetTranslate(shadowPos);
		heliShadow_->SetRotate({ 0.0f, heliRot_.y, 0.0f });
		heliShadow_->SetScale({ shadowScale, 1.0f, shadowScale * 1.55f });
		heliShadow_->SetColor({ 0.03f, 0.05f, 0.07f, shadowAlpha });
		heliShadow_->Update();
	}

	// ファストロープ (Act 2 & Act 3 でヘリから地面0mへ垂らす)
	if (fastRope_)
	{
		if (timer_ >= kCut1Duration && timer_ < kCut4Duration + 0.15f)
		{
			Vector3 ropeTop = heliPos_ + Vector3{ 1.65f, 2.38f, 0.75f };
			fastRope_->SetTranslate(ropeTop);
			fastRope_->SetRotate({ 0.0f, 0.0f, 0.0f });

			// Act 2 序盤0.60sでロープが地上まで急速伸長
			float deployProgress = 1.0f;
			if (timer_ < kCut1Duration + 0.60f)
			{
				deployProgress = (timer_ - kCut1Duration) / 0.60f;
				deployProgress = (std::min)(deployProgress, 1.0f);
			}
			float ropeLen = ropeTop.y * deployProgress;
			fastRope_->SetScale({ 1.0f, (std::max)(ropeLen / 25.0f, 0.01f), 1.0f });
			fastRope_->Update();
		}
		else
		{
			fastRope_->SetScale({ 0.0f, 0.0f, 0.0f });
			fastRope_->Update();
		}
	}

	// プレイヤー（アヒル兵士）のトランスフォーム反映
	if (scene_->player_)
	{
		scene_->player_->SetIntroTransform(duckPos_, duckScale_, duckRot_);
	}
}

void HelicopterInsertionController::UpdateParticles(float deltaTime)
{
	if (!scene_ || !scene_->particleManager || !scene_->appParticleManager_) return;

	// ヘリのトランスフォーム行列（ローカル座標系からワールド座標系への方向変換用）
	Matrix4x4 R = Multiply(MakeRotateXMatrix(heliRot_.x), Multiply(MakeRotateYMatrix(heliRot_.y), MakeRotateZMatrix(heliRot_.z)));
	auto LocalToWorldDir = [&](const Vector3& v) -> Vector3 {
		return {
			v.x * R.m[0][0] + v.y * R.m[1][0] + v.z * R.m[2][0],
			v.x * R.m[0][1] + v.y * R.m[1][1] + v.z * R.m[2][1],
			v.x * R.m[0][2] + v.y * R.m[1][2] + v.z * R.m[2][2]
		};
	};

	// 1. メインローターブレード先端からの翼端渦・凝結ベイパー雲 (Rotor Tip Vortices)
	// 高速回転（60 rad/s）する4枚羽の翼端から、圧力差による螺旋状の白い気流ベイパーを連続放出
	rotorVortexTimer_ += deltaTime;
	if (rotorVortexTimer_ >= 0.022f)
	{
		rotorVortexTimer_ = 0.0f;
		Vector3 mastWorld = heliPos_ + LocalToWorldDir({ 0.0f, 4.15f, 0.75f });

		// フレア制動（Act 1後半）および急上昇離脱（Act 5）時はローター高負荷でベイパーが濃厚化
		bool isHighG = (currentCut_ == 1 && timer_ >= 2.0f) || (currentCut_ == 5);
		float vaporAlpha = isHighG ? 0.45f : 0.26f;
		float vaporScale = isHighG ? 0.38f : 0.25f;

		for (int i = 0; i < 4; ++i)
		{
			float angle = mainRotorAngle_ + i * (kPi * 0.5f);
			Vector3 localTip = { std::cos(angle) * 3.25f, -0.06f, std::sin(angle) * 3.25f };
			Vector3 tipWorld = mastWorld + LocalToWorldDir(localTip);

			// 接線方向速度 ＋ 下降気流（ダウンウォッシュ成分）
			Vector3 localTangent = { -std::sin(angle), 0.0f, std::cos(angle) };
			Vector3 tipVel = LocalToWorldDir(localTangent) * 4.6f + LocalToWorldDir({ 0.0f, -2.4f, 0.0f })
				+ Vector3{ RandRange(-0.2f, 0.2f), RandRange(-0.15f, 0.15f), RandRange(-0.2f, 0.2f) };

			scene_->appParticleManager_->EmitDustWithVelocity(
				scene_->particleManager->GetRandomEngine(),
				tipWorld,
				vaporScale,
				{ 0.95f, 0.98f, 1.0f, vaporAlpha },
				tipVel,
				isHighG ? 0.26f : 0.18f,
				scene_->smokeTextureIndex_
			);
		}
	}

	// 2. ツインタービンエンジンの排気ポート（左右）からの高温排気陽炎＆スモーク (Engine Exhaust Trails)
	// 機体上部キャビン後方の左右ノズルから後下方へ高速噴射
	engineExhaustTimer_ += deltaTime;
	if (engineExhaustTimer_ >= 0.030f)
	{
		engineExhaustTimer_ = 0.0f;
		Vector3 leftExhaustPos = heliPos_ + LocalToWorldDir({ -0.75f, 3.45f, -0.45f });
		Vector3 rightExhaustPos = heliPos_ + LocalToWorldDir({ +0.75f, 3.45f, -0.45f });

		// 離脱中（Act 5）や急行中（Act 1）はエンジンスロットル全開で排気ガスが濃縮
		bool isFullThrottle = (currentCut_ == 5) || (currentCut_ == 1 && timer_ < 1.8f);
		Vector4 exhaustColor = isFullThrottle
			? Vector4{ 0.32f, 0.30f, 0.28f, 0.32f } // フルスロットルの排気微粒子
			: Vector4{ 0.48f, 0.46f, 0.44f, 0.18f }; // アイドリング／ホバリング時の熱風陽炎
		float exhaustScale = isFullThrottle ? 0.48f : 0.35f;
		float exhaustSpeed = isFullThrottle ? 6.5f : 4.5f;

		Vector3 exhaustBaseVel = LocalToWorldDir({ 0.0f, -0.30f, -exhaustSpeed });

		// 左排気
		Vector3 leftVel = exhaustBaseVel + LocalToWorldDir({ -0.4f, 0.0f, 0.0f })
			+ Vector3{ RandRange(-0.2f, 0.2f), RandRange(-0.2f, 0.2f), RandRange(-0.2f, 0.2f) };
		scene_->appParticleManager_->EmitDustWithVelocity(
			scene_->particleManager->GetRandomEngine(),
			leftExhaustPos,
			exhaustScale,
			exhaustColor,
			leftVel,
			0.38f,
			scene_->smokeTextureIndex_
		);

		// 右排気
		Vector3 rightVel = exhaustBaseVel + LocalToWorldDir({ +0.4f, 0.0f, 0.0f })
			+ Vector3{ RandRange(-0.2f, 0.2f), RandRange(-0.2f, 0.2f), RandRange(-0.2f, 0.2f) };
		scene_->appParticleManager_->EmitDustWithVelocity(
			scene_->particleManager->GetRandomEngine(),
			rightExhaustPos,
			exhaustScale,
			exhaustColor,
			rightVel,
			0.38f,
			scene_->smokeTextureIndex_
		);
	}

	// 3. テールローターのアンチトルク側方気流ジェット (Tail Rotor Jet)
	tailRotorVortexTimer_ += deltaTime;
	if (tailRotorVortexTimer_ >= 0.045f)
	{
		tailRotorVortexTimer_ = 0.0f;
		Vector3 tailRotorPos = heliPos_ + LocalToWorldDir({ 0.35f, 3.35f, -8.60f });
		Vector3 tailJetVel = LocalToWorldDir({ RandRange(3.5f, 5.5f), RandRange(-0.4f, 0.4f), RandRange(-0.6f, 0.6f) });

		scene_->appParticleManager_->EmitDustWithVelocity(
			scene_->particleManager->GetRandomEngine(),
			tailRotorPos,
			0.24f,
			{ 0.90f, 0.94f, 1.0f, 0.16f },
			tailJetVel,
			0.16f,
			scene_->smokeTextureIndex_
		);
	}

	// 4. 航空警告灯・衝突防止ストロボ閃光パルス (Anti-Collision Beacon & Strobe)
	beaconTimer_ += deltaTime;
	uint32_t flashTex = (scene_->starburstTextureIndex_ != TextureManager::kInvalidTextureIndex)
		? scene_->starburstTextureIndex_ : scene_->particleTextureB;

	// (A) 垂直尾翼頂部の赤色ビーコン閃光（0.80秒周期パルス）
	float tailCycle = std::fmod(beaconTimer_, 0.80f);
	if (tailCycle < deltaTime * 1.25f)
	{
		Vector3 tailBeaconPos = heliPos_ + LocalToWorldDir({ 0.05f, 4.35f, -8.70f });
		scene_->appParticleManager_->EmitMuzzleFlare(
			scene_->particleManager->GetRandomEngine(),
			tailBeaconPos,
			1.25f,
			{ 1.0f, 0.10f, 0.06f, 1.0f },
			0.08f,
			flashTex
		);
	}

	// (B) 機体腹部（ベリー）の白色高輝度ダブルストロボ閃光（1.20秒周期: ピカッ、ピカッと2連打）
	float bellyCycle = std::fmod(beaconTimer_, 1.20f);
	if ((bellyCycle < deltaTime * 1.25f) || (bellyCycle >= 0.15f && bellyCycle < 0.15f + deltaTime * 1.25f))
	{
		Vector3 bellyStrobePos = heliPos_ + LocalToWorldDir({ 0.0f, 0.15f, 0.50f });
		scene_->appParticleManager_->EmitMuzzleFlare(
			scene_->particleManager->GetRandomEngine(),
			bellyStrobePos,
			1.50f,
			{ 1.0f, 1.0f, 1.0f, 1.0f },
			0.06f,
			flashTex
		);
	}

	// 5. ヘリのローター下降風圧（ダウンウォッシュ）による地面砂塵・舞い散る木の葉の大拡散
	// Act 1後半（ヘリ接近時 1.4s以降）からAct 4終了まで継続
	if (timer_ >= 1.4f && timer_ < kCut4Duration + 0.2f)
	{
		downwashDustTimer_ += deltaTime;
		if (downwashDustTimer_ >= 0.040f)
		{
			downwashDustTimer_ = 0.0f;
			Vector3 groundCenter = { 0.0f, 0.05f, 0.0f };

			// 8方向に吹き荒れる放射状サンドダスト (太陽光に透ける明るい白砂煙)
			for (int i = 0; i < 8; ++i)
			{
				float angle = i * (2.0f * kPi / 8.0f) + RandRange(-0.2f, 0.2f);
				float spd = RandRange(3.0f, 5.5f);
				Vector3 vel = { std::cos(angle) * spd, RandRange(0.25f, 0.60f), std::sin(angle) * spd };
				Vector3 spawnP = groundCenter + Vector3{ std::cos(angle) * 1.5f, 0.02f, std::sin(angle) * 1.5f };

				scene_->appParticleManager_->EmitDustWithVelocity(
					scene_->particleManager->GetRandomEngine(),
					spawnP,
					0.90f,
					{ 0.92f, 0.88f, 0.76f, 0.42f }, // 日差しに照らされた白砂塵
					vel,
					0.45f,
					scene_->smokeTextureIndex_
				);
			}

			// 風圧で激しく舞い散る白昼の木の葉（発生頻度0.65、新緑〜針葉樹オリーブのバリエーション）
			if (Rand01() < 0.65f)
			{
				float leafAngle = RandRange(0.0f, 2.0f * kPi);
				Vector3 leafPos = groundCenter + Vector3{ std::cos(leafAngle) * 3.5f, RandRange(0.15f, 0.70f), std::sin(leafAngle) * 3.5f };
				Vector3 leafVel = { std::cos(leafAngle) * RandRange(3.2f, 5.8f), RandRange(1.4f, 3.2f), std::sin(leafAngle) * RandRange(3.2f, 5.8f) };

				Vector4 leafCol = (Rand01() < 0.5f)
					? Vector4{ 0.24f, 0.58f, 0.16f, 0.95f }
					: Vector4{ 0.35f, 0.48f, 0.14f, 0.95f };

				uint32_t leafTex = (leafTexIndex_ != TextureManager::kInvalidTextureIndex)
					? leafTexIndex_ : scene_->particleTextureB;

				scene_->appParticleManager_->EmitSparkWithVelocity(
					scene_->particleManager->GetRandomEngine(),
					leafPos,
					leafVel,
					leafCol,
					0.25f,
					0.75f,
					leafTex
				);
			}
		}
	}
}

void HelicopterInsertionController::Draw3D(const RenderContext& ctx)
{
	if (!isActive_) return;

	// 地上シャドウを先に描画（地面に密着投影）
	if (heliShadow_) heliShadow_->Draw(ctx);
	if (heliBody_) heliBody_->Draw(ctx);
	if (heliMainRotor_) heliMainRotor_->Draw(ctx);
	if (heliTailRotor_) heliTailRotor_->Draw(ctx);
	if (fastRope_) fastRope_->Draw(ctx);
}

void HelicopterInsertionController::DrawUI()
{
#ifdef USE_IMGUI
	if (!isActive_) return;

	ImDrawList* drawList = ImGui::GetForegroundDrawList();
	ImVec2 disp = ImGui::GetIO().DisplaySize;
	if (disp.x <= 0.0f || disp.y <= 0.0f) return;

	// =========================================================================
	// ⚡ 1. 上品なカット切り替えフラッシュ ＆ 着地インパクトフレーム
	// =========================================================================
	float totalWhiteFlash = (std::min)(cutFlashAlpha_ + impactFlashAlpha_, 1.0f);
	if (totalWhiteFlash > 0.02f)
	{
		int flashA = static_cast<int>(230 * totalWhiteFlash);
		drawList->AddRectFilled(ImVec2(0.0f, 0.0f), disp, IM_COL32(255, 255, 255, flashA));
	}

	// =========================================================================
	// 🎬 2. 映画風シネマスコープ黒帯（21:9 アスペクト比演出）
	// =========================================================================
	float maxBarH = disp.y * 0.085f; // 画面上下8.5%
	float barRatio = 1.0f;
	if (currentCut_ == 5)
	{
		float exitT = (timer_ - kCut4Duration) / (kDuration - kCut4Duration);
		barRatio = (std::max)(1.0f - exitT * 1.4f, 0.0f);
	}
	float curBarH = maxBarH * barRatio;

	if (curBarH > 0.5f)
	{
		// 漆黒シネマスコープバー
		drawList->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(disp.x, curBarH), IM_COL32(4, 6, 8, 250));
		drawList->AddRectFilled(ImVec2(0.0f, disp.y - curBarH), ImVec2(disp.x, disp.y), IM_COL32(4, 6, 8, 250));

		// 上下の極細チタンアクセントライン（控えめで洗練されたデザイン）
		drawList->AddLine(ImVec2(0.0f, curBarH), ImVec2(disp.x, curBarH), IM_COL32(50, 80, 95, 140), 1.0f);
		drawList->AddLine(ImVec2(0.0f, disp.y - curBarH), ImVec2(disp.x, disp.y - curBarH), IM_COL32(50, 80, 95, 140), 1.0f);

		// 上部バー：洗練された軍用コード
		const char* topCode = "/// TASK FORCE DUCKOV ///  OPERATION: IRON FEATHER  [DAYLIGHT AIRBORNE ASSAULT]";
		ImVec2 tSize = ImGui::CalcTextSize(topCode);
		drawList->AddText(ImVec2((disp.x - tSize.x) * 0.5f, curBarH * 0.5f - tSize.y * 0.5f), IM_COL32(140, 175, 190, 180), topCode);

		// 下部バー：スキップキーガイド（控えめなブリージング表示）
		float pulse = 0.55f + 0.35f * std::sin(timer_ * 5.0f);
		int alpha = static_cast<int>(180 * pulse);
		const char* skipPrompt = "[ SPACE ] SKIP";
		ImVec2 sSize = ImGui::CalcTextSize(skipPrompt);
		drawList->AddText(ImVec2((disp.x - sSize.x) * 0.5f, disp.y - curBarH * 0.5f - sSize.y * 0.5f), IM_COL32(180, 190, 195, alpha), skipPrompt);
	}

	// =========================================================================
	// 🛰️ 3. 洗練されたミニマル軍用OSD（ブラックホーク・ダウン調 白昼作戦HUD）
	// =========================================================================
	if (timer_ < kCut4Duration + 0.4f)
	{
		float osdX = 45.0f;
		float osdY = disp.y - maxBarH - 46.0f;

		// 録画インジケーター（点滅する赤い丸）
		bool recBlink = (std::fmod(timer_, 1.0f) < 0.65f);
		if (recBlink)
		{
			drawList->AddCircleFilled(ImVec2(osdX, osdY + 5.0f), 4.0f, IM_COL32(235, 45, 45, 230));
		}
		drawList->AddText(ImVec2(osdX + 12.0f, osdY), IM_COL32(220, 230, 240, 220), "REC  00:0");

		char timeSecBuf[16];
		sprintf_s(timeSecBuf, "%d", static_cast<int>(timer_));
		drawList->AddText(ImVec2(osdX + 68.0f, osdY), IM_COL32(220, 230, 240, 220), timeSecBuf);
		drawList->AddText(ImVec2(osdX + 88.0f, osdY), IM_COL32(160, 185, 205, 180), "// DAYLIGHT ASSAULT");

		// ロケーション＆時刻表示
		drawList->AddText(ImVec2(osdX, osdY + 16.0f), IM_COL32(170, 190, 205, 200), "LOC: SECTOR-01 OUTPOST | 14:24 HRS");

		// ステータスライン
		const char* statusText = nullptr;
		ImU32 statusColor = IM_COL32(140, 200, 220, 220);

		if (currentCut_ == 1)
		{
			statusText = "INGRESS: LOW-ALTITUDE AIRBORNE APPROACH [ALT 24M]";
			statusColor = IM_COL32(245, 205, 80, 230);
		}
		else if (currentCut_ == 2)
		{
			float act2Phase = (timer_ - kCut1Duration) / (kCut2Duration - kCut1Duration);
			if (act2Phase < 0.68f)
			{
				statusText = "[ RED LIGHT ] STANDBY // CONFIRMING LZ PERIMETER";
				statusColor = IM_COL32(255, 80, 80, 240);

				// 画面右上RED LIGHTインジケーター
				drawList->AddRectFilled(ImVec2(disp.x - 265.0f, curBarH + 18.0f), ImVec2(disp.x - 45.0f, curBarH + 50.0f), IM_COL32(45, 12, 12, 220));
				drawList->AddRect(ImVec2(disp.x - 265.0f, curBarH + 18.0f), ImVec2(disp.x - 45.0f, curBarH + 50.0f), IM_COL32(230, 60, 60, 200), 0.0f, 0, 1.5f);
				drawList->AddCircleFilled(ImVec2(disp.x - 245.0f, curBarH + 34.0f), 6.0f, IM_COL32(255, 50, 50, 255));
				drawList->AddText(ImVec2(disp.x - 230.0f, curBarH + 26.0f), IM_COL32(255, 200, 200, 240), "HOLD POSITION: RED");
			}
			else
			{
				statusText = "[ GREEN LIGHT ] EXECUTE INSERTION >> GO! GO! GO!";
				statusColor = IM_COL32(50, 255, 120, 255);

				// 画面右上GREEN LIGHTインジケーター（点滅発光）
				drawList->AddRectFilled(ImVec2(disp.x - 265.0f, curBarH + 18.0f), ImVec2(disp.x - 45.0f, curBarH + 50.0f), IM_COL32(10, 48, 22, 230));
				drawList->AddRect(ImVec2(disp.x - 265.0f, curBarH + 18.0f), ImVec2(disp.x - 45.0f, curBarH + 50.0f), IM_COL32(60, 245, 110, 230), 0.0f, 0, 2.0f);
				drawList->AddCircleFilled(ImVec2(disp.x - 245.0f, curBarH + 34.0f), 7.0f, IM_COL32(50, 255, 110, 255));
				drawList->AddText(ImVec2(disp.x - 230.0f, curBarH + 26.0f), IM_COL32(210, 255, 225, 255), "GREEN LIGHT: GO!");
			}
		}
		else if (currentCut_ == 3)
		{
			float act3Phase = (timer_ - kCut2Duration) / (kCut3Duration - kCut2Duration);
			if (act3Phase < 0.68f)
			{
				statusText = "FAST-ROPE RAPID COMBAT DESCENT [FREE-FALL SPEED]";
				statusColor = IM_COL32(100, 230, 255, 230);
			}
			else
			{
				statusText = ">> COMBAT BRAKE ENGAGED [ALT 3M] - DECELERATING <<";
				statusColor = IM_COL32(255, 225, 65, 255);
			}
		}
		else if (currentCut_ == 4)
		{
			float act4Phase = (timer_ - kCut3Duration) / (kCut4Duration - kCut3Duration);
			if (act4Phase < 0.25f)
			{
				statusText = "TOUCHDOWN IMPACT // ABSORBING LANDING";
				statusColor = IM_COL32(255, 205, 75, 240);
			}
			else if (act4Phase < 0.72f)
			{
				statusText = "CLEARING 360-DEG PERIMETER // SCANNING FOR HOSTILES";
				statusColor = IM_COL32(110, 225, 255, 245);
			}
			else
			{
				statusText = "LZ ALL CLEAR // WEAPONS HOT - COMMENCE OPERATION!";
				statusColor = IM_COL32(65, 255, 135, 255);
			}
		}

		if (statusText)
		{
			drawList->AddText(ImVec2(osdX, osdY + 31.0f), statusColor, statusText);
		}
	}
#endif
}
