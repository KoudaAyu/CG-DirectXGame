#include "TitleScene.h"
#include "KeyInput.h"
#include "DirectXCom.h"
#include "SceneManager.h"
#include "Camera.h"
#include "TextureManager.h"
#include "Object3dCom.h"
#include "RenderContext.h"
#include "Baziru3_Engine/Graphics/Graphics/SceneRenderRequests.h"
#include "Application/Particle/AppParticleManager.h"
#include "ParticleManager.h"
#include "SpriteManager.h"
#include "Sprite.h"
#include "WindowsAPI.h"
#include "Matrix4x4.h"
#include <cmath>
#include <algorithm>

void TitleScene::InitializeScene()
{
	if (dxCommon_)
	{
		input_ = new KeyInput();
		input_->Initialize(dxCommon_->GetWindowAPI());
		mouseInput_.Initialize(dxCommon_->GetWindowAPI());
	}
	hasPrevMousePos_ = false;
	mouseRippleCooldown_ = 0.0f;
	mouseWaveEmitCooldown_ = 0.0f;
	lastTime_ = std::chrono::steady_clock::now();
	bgTimer_ = 0.0f;
	startTransitionTimer_ = 0.0f;
	isStarting_ = false;

	// ミノフスキー粒子マネージャー初期化
	minovskyAngle_ = 0.0f;
	ambientMoteTimer_ = 0.0f;
	if (GetParticleManager())
	{
		appParticleManager_ = std::make_unique<AppParticleManager>();
		appParticleManager_->Initialize(GetParticleManager());

		circleTexIndex_ = TextureManager::GetInstance()->Load("Resources/circle2.png");
		if (circleTexIndex_ == UINT32_MAX)
		{
			circleTexIndex_ = TextureManager::GetInstance()->Load("Resources/CG4/circle2.png");
		}
		starTexIndex_ = TextureManager::GetInstance()->Load("Resources/starburst.png");
	}

	// タイトル専用カメラ（アヒルちゃんとミノフスキー粒子がバランスよく映る引きの構図）
	cameraTranslate_ = { 0.0f, 0.25f, -7.5f };
	cameraRotate_ = { 0.05f, 0.0f, 0.0f };
	if (camera_)
	{
		camera_->SetTranslate(cameraTranslate_);
		camera_->SetRotate(cameraRotate_);
		camera_->Update();
	}

	// 1. 手前のプレイヤーアヒル兵士 (Player Duck)
	if (GetObject3dCom() && camera_)
	{
		Object3d::ModelData model = Object3d::LoadObjFile("Resources", "player.obj");
		if (model.material.textureFilePath.empty())
		{
			model.material.textureFilePath = "Resources/duck.png";
		}
		model.boundingRadius = 10.0f;
		duckModel_ = std::make_unique<Object3d>();
		duckModel_->Initialize(GetObject3dCom(), model);
		duckModel_->SetCamera(camera_);
		duckModel_->SetScale({ duckScale_, duckScale_, duckScale_ });
		duckModel_->SetTranslate(duckBasePos_);
		duckModel_->SetRotate({ 0.08f, -0.45f, 0.0f });
		duckModel_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

		// 2. 奥をパトロールする敵アヒル兵士 (Enemy Duck Patrol)
		Object3d::ModelData enemyModel = Object3d::LoadObjFile("Resources", "player.obj");
		enemyModel.material.textureFilePath = "Resources/duck_enemy.png";
		enemyModel.boundingRadius = 10.0f;
		enemyDuckModel_ = std::make_unique<Object3d>();
		enemyDuckModel_->Initialize(GetObject3dCom(), enemyModel);
		enemyDuckModel_->SetCamera(camera_);
		enemyDuckModel_->SetScale({ 0.38f, 0.38f, 0.38f });
		enemyDuckModel_->SetTranslate({ -1.5f, -0.32f, 2.3f });
		enemyDuckModel_->SetRotate({ 0.05f, 1.57f, 0.0f });
		enemyDuckModel_->SetColor({ 0.95f, 0.95f, 1.0f, 1.0f });

		// 3. 川の水面（高密度3D波面グリッド 32x20 = 640頂点・1,178ポリゴンによる立体起伏メッシュ）
		{
			Object3d::ModelData riverModelData{};
			riverModelData.material.textureFilePath = "Resources/water.png";
			riverTexIndex_ = TextureManager::GetInstance()->Load("Resources/water.png");
			riverModelData.material.textureIndex = riverTexIndex_;
			riverModelData.boundingRadius = 60.0f;

			const float kSpanX = 36.0f;
			const float kSpanZ = 22.0f;
			const float kStartX = -18.0f;
			const float kStartZ = -7.0f;

			waterBaseVertices_.clear();
			waterBaseVertices_.reserve(kWaterGridCols * kWaterGridRows);
			riverModelData.vertices.reserve(kWaterGridCols * kWaterGridRows);

			for (int r = 0; r < kWaterGridRows; ++r)
			{
				float ratioZ = static_cast<float>(r) / static_cast<float>(kWaterGridRows - 1);
				float posZ = kStartZ + ratioZ * kSpanZ;
				float v = ratioZ * 4.5f;

				for (int c = 0; c < kWaterGridCols; ++c)
				{
					float ratioX = static_cast<float>(c) / static_cast<float>(kWaterGridCols - 1);
					float posX = kStartX + ratioX * kSpanX;
					float u = ratioX * 7.5f;

					WaterVertexBase base{};
					base.basePosX = posX;
					base.basePosZ = posZ;
					base.baseU = u;
					base.baseV = v;
					waterBaseVertices_.push_back(base);

					Sprite::VertexData vtx{};
					vtx.position = { posX, 0.0f, posZ, 1.0f };
					vtx.texcoord = { u, v };
					vtx.normal = { 0.0f, 1.0f, 0.0f };
					riverModelData.vertices.push_back(vtx);
				}
			}

			// インデックスバッファの構築（グリッドポリゴン）
			riverModelData.indices.reserve((kWaterGridRows - 1) * (kWaterGridCols - 1) * 6);
			for (int r = 0; r < kWaterGridRows - 1; ++r)
			{
				for (int c = 0; c < kWaterGridCols - 1; ++c)
				{
					uint32_t i0 = static_cast<uint32_t>(r * kWaterGridCols + c);
					uint32_t i1 = static_cast<uint32_t>(r * kWaterGridCols + (c + 1));
					uint32_t i2 = static_cast<uint32_t>((r + 1) * kWaterGridCols + c);
					uint32_t i3 = static_cast<uint32_t>((r + 1) * kWaterGridCols + (c + 1));

					// 三角形1 (i0, i1, i2)
					riverModelData.indices.push_back(i0);
					riverModelData.indices.push_back(i1);
					riverModelData.indices.push_back(i2);

					// 三角形2 (i1, i3, i2)
					riverModelData.indices.push_back(i1);
					riverModelData.indices.push_back(i3);
					riverModelData.indices.push_back(i2);
				}
			}

			riverModel_ = std::make_unique<Object3d>();
			riverModel_->Initialize(GetObject3dCom(), riverModelData);
			riverModel_->SetCamera(camera_);
			riverModel_->SetScale({ 1.0f, 1.0f, 1.0f });
			riverModel_->SetTranslate({ 0.0f, -0.72f, 0.0f });
			riverModel_->SetRotate({ 0.0f, 0.0f, 0.0f });
			riverModel_->SetColor({ 0.92f, 0.98f, 1.0f, 0.92f });
			riverModel_->SetEnableLighting(true); // ライティング有効化で波の立体陰影・ハイライトを表現
		}

		// 4. 軍用コンテナ (Military Cargo Container - 背景左奥の重厚なシルエット)
		{
			Object3d::ModelData containerModelData = Object3d::LoadObjFile("Resources", "container.obj");
			containerModelData.material.textureFilePath = "Resources/container_military.png";
			containerTexIndex_ = TextureManager::GetInstance()->Load("Resources/container_military.png");
			containerModelData.material.textureIndex = containerTexIndex_;
			containerModelData.boundingRadius = 15.0f;
			containerModel_ = std::make_unique<Object3d>();
			containerModel_->Initialize(GetObject3dCom(), containerModelData);
			containerModel_->SetCamera(camera_);
			containerModel_->SetScale({ 0.75f, 0.75f, 0.75f });
			containerModel_->SetTranslate({ -3.0f, -0.72f, 3.8f });
			containerModel_->SetRotate({ 0.0f, 0.45f, 0.0f });
			containerModel_->SetColor({ 0.92f, 0.94f, 0.96f, 1.0f });
		}

		// 5. 危険物ドラム缶スタック (Hazardous Fuel Drum Stack - 中央奥の赤・黄ドラム缶)
		{
			Object3d::ModelData barrelModelData = Object3d::LoadObjFile("Resources", "duckov_barrel_stack.obj");
			barrelModelData.material.textureFilePath = "Resources/duckov_barrel.png";
			barrelTexIndex_ = TextureManager::GetInstance()->Load("Resources/duckov_barrel.png");
			barrelModelData.material.textureIndex = barrelTexIndex_;
			barrelModelData.boundingRadius = 8.0f;
			barrelStackModel_ = std::make_unique<Object3d>();
			barrelStackModel_->Initialize(GetObject3dCom(), barrelModelData);
			barrelStackModel_->SetCamera(camera_);
			barrelStackModel_->SetScale({ 0.65f, 0.65f, 0.65f });
			barrelStackModel_->SetTranslate({ -0.7f, -0.72f, 3.2f });
			barrelStackModel_->SetRotate({ 0.0f, -0.22f, 0.0f });
			barrelStackModel_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		}

		// 6. 土嚢バリケード (Sandbag Defense Barricade - 敵パトロールの手前)
		{
			Object3d::ModelData sandbagModelData = Object3d::LoadObjFile("Resources", "duckov_sandbag_wall.obj");
			sandbagModelData.material.textureFilePath = "Resources/duckov_sandbag.png";
			sandbagTexIndex_ = TextureManager::GetInstance()->Load("Resources/duckov_sandbag.png");
			sandbagModelData.material.textureIndex = sandbagTexIndex_;
			sandbagModelData.boundingRadius = 10.0f;
			sandbagModel_ = std::make_unique<Object3d>();
			sandbagModel_->Initialize(GetObject3dCom(), sandbagModelData);
			sandbagModel_->SetCamera(camera_);
			sandbagModel_->SetScale({ 0.60f, 0.60f, 0.60f });
			sandbagModel_->SetTranslate({ -1.6f, -0.72f, 1.8f });
			sandbagModel_->SetRotate({ 0.0f, 0.08f, 0.0f });
			sandbagModel_->SetColor({ 0.95f, 0.95f, 0.95f, 1.0f });
		}

		// 7. 軍用木箱スタック (Military Supply Crates - 右岸の補給物資)
		{
			Object3d::ModelData crateModelData = Object3d::LoadObjFile("Resources", "duckov_crate.obj");
			crateModelData.material.textureFilePath = "Resources/duckov_crate.png";
			crateTexIndex_ = TextureManager::GetInstance()->Load("Resources/duckov_crate.png");
			crateModelData.material.textureIndex = crateTexIndex_;
			crateModelData.boundingRadius = 6.0f;

			// 下段（大木箱）
			crateModel1_ = std::make_unique<Object3d>();
			crateModel1_->Initialize(GetObject3dCom(), crateModelData);
			crateModel1_->SetCamera(camera_);
			crateModel1_->SetScale({ 0.70f, 0.70f, 0.70f });
			crateModel1_->SetTranslate({ 2.7f, -0.72f, 2.0f });
			crateModel1_->SetRotate({ 0.0f, 0.22f, 0.0f });
			crateModel1_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

			// 上段（積み上げ小木箱）
			crateModel2_ = std::make_unique<Object3d>();
			crateModel2_->Initialize(GetObject3dCom(), crateModelData);
			crateModel2_->SetCamera(camera_);
			crateModel2_->SetScale({ 0.52f, 0.52f, 0.52f });
			crateModel2_->SetTranslate({ 2.85f, 0.12f, 2.1f });
			crateModel2_->SetRotate({ 0.0f, -0.35f, 0.0f });
			crateModel2_->SetColor({ 0.95f, 0.95f, 0.95f, 1.0f });
		}
	}

	InitializeSprites();
}

void TitleScene::InitializeSprites()
{
	SpriteCom* sc = SceneManager::GetInstance() ? SceneManager::GetInstance()->GetSpriteCom() : nullptr;
	if (!sc) return;

	spriteManager_ = std::make_unique<SpriteManager>();
	spriteManager_->Initialize(sc, "Resources/uvChecker.png", 0);

	sprites_.clear();

	WindowAPI* win = dxCommon_ ? dxCommon_->GetWindowAPI() : nullptr;
	float screenW = win ? static_cast<float>(win->GetClientWidth()) : 1280.0f;
	float screenH = win ? static_cast<float>(win->GetClientHeight()) : 720.0f;

	Sprite::Transform defaultTransform = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

	// 1. シネマティック・ビネット (四隅のシャドウ)
	if (auto vig = Sprite::Create(sc, defaultTransform, "Resources/title_vignette.png"))
	{
		vig->SetAnchorPoint({ 0.0f, 0.0f });
		vig->SetPosition({ 0.0f, 0.0f });
		vig->SetSize({ screenW, screenH });
		vig->SetColor({ 1.0f, 1.0f, 1.0f, 0.85f });
		sprites_.emplace_back(std::move(vig));
		vignetteSprite_ = sprites_.back().get();
	}

	// 2. メインタイトルロゴ (ESCAPE FROM DUCKOV)
	if (auto logo = Sprite::Create(sc, defaultTransform, "Resources/title_logo.png"))
	{
		logo->SetAnchorPoint({ 0.0f, 0.0f });
		logo->SetPosition({ 52.0f, 44.0f });
		logo->SetSize({ 540.0f, 162.0f }); // 1000x300 テクスチャに合わせた黄金比
		logo->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		sprites_.emplace_back(std::move(logo));
		titleLogoSprite_ = sprites_.back().get();
	}

	// 3. [ PRESS SPACE TO DEPLOY ] プロンプト
	if (auto prompt = Sprite::Create(sc, defaultTransform, "Resources/title_press_space.png"))
	{
		prompt->SetAnchorPoint({ 0.5f, 0.5f });
		prompt->SetPosition({ screenW * 0.5f, screenH * 0.88f });
		prompt->SetSize({ 490.0f, 70.0f });
		prompt->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		sprites_.emplace_back(std::move(prompt));
		pressSpaceSprite_ = sprites_.back().get();
	}

	// 4. 出撃時フェードオーバーレイ (暗転フェード用)
	if (auto fade = Sprite::Create(sc, defaultTransform, "Resources/CG4/human/white.png"))
	{
		fade->SetAnchorPoint({ 0.0f, 0.0f });
		fade->SetPosition({ 0.0f, 0.0f });
		fade->SetSize({ screenW, screenH });
		fade->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
		sprites_.emplace_back(std::move(fade));
		fadeOverlaySprite_ = sprites_.back().get();
	}
}

void TitleScene::Finalize()
{
	delete input_;
	input_ = nullptr;
	if (duckModel_) duckModel_.reset();
	if (enemyDuckModel_) enemyDuckModel_.reset();
	if (riverModel_) riverModel_.reset();
	if (containerModel_) containerModel_.reset();
	if (barrelStackModel_) barrelStackModel_.reset();
	if (sandbagModel_) sandbagModel_.reset();
	if (crateModel1_) crateModel1_.reset();
	if (crateModel2_) crateModel2_.reset();
	if (appParticleManager_) appParticleManager_.reset();
	if (spriteManager_)
	{
		spriteManager_->Finalize();
		spriteManager_.reset();
	}
	sprites_.clear();
	vignetteSprite_ = nullptr;
	titleLogoSprite_ = nullptr;
	pressSpaceSprite_ = nullptr;
	fadeOverlaySprite_ = nullptr;
}

void TitleScene::Update()
{
	if (input_)
	{
		input_->Update();
	}

	auto now = std::chrono::steady_clock::now();
	float deltaTime = std::chrono::duration<float>(now - lastTime_).count();
	lastTime_ = now;
	deltaTime = std::clamp(deltaTime, 0.0001f, 0.1f);

	bgTimer_ += deltaTime;

	// キー入力ハンドリング
	if (!isStarting_ && input_)
	{
		if (input_->TriggerKey(DIK_SPACE) || input_->TriggerKey(DIK_RETURN))
		{
			isStarting_ = true;
			startTransitionTimer_ = 0.0f;
		}
		else if (input_->TriggerKey(DIK_ESCAPE))
		{
			PostQuitMessage(0);
		}
	}

	// 出撃遷移進行（ブラックアウトフェード後にゲームプレイへ移行）
	if (isStarting_)
	{
		startTransitionTimer_ += deltaTime;
		float fadeAlpha = (std::min)(1.0f, startTransitionTimer_ / 0.55f);
		if (fadeOverlaySprite_)
		{
			fadeOverlaySprite_->SetColor({ 0.04f, 0.06f, 0.08f, fadeAlpha });
		}
		if (startTransitionTimer_ >= 0.60f)
		{
			SceneManager::GetInstance()->ChangeScene("GAMEPLAY");
			return;
		}
	}

	// マウス入力更新
	mouseInput_.Update();

	// カメラの更新（マウス位置による微小パララックス視差効果でジオラマの立体感を強調）
	Vector3 effectiveCamPos = cameraTranslate_;
	Vector3 effectiveCamRot = cameraRotate_;
	Vector3 mouseWorldPos{ 0.0f, 0.0f, 0.0f };
	Vector3 mouseWaterPos{ 0.0f, -0.72f, 0.0f };
	bool hasMouseRay = false;
	Vector2 currentMousePos{ 0.0f, 0.0f };

	if (dxCommon_ && camera_)
	{
		WindowAPI* win = dxCommon_->GetWindowAPI();
		if (win && win->GetClientWidth() > 0 && win->GetClientHeight() > 0)
		{
			currentMousePos = mouseInput_.GetScaledPosition();
			float clientW = static_cast<float>(win->GetClientWidth());
			float clientH = static_cast<float>(win->GetClientHeight());
			float normX = (currentMousePos.x / clientW) * 2.0f - 1.0f;
			float normY = (currentMousePos.y / clientH) * 2.0f - 1.0f;
			normX = std::clamp(normX, -1.0f, 1.0f);
			normY = std::clamp(normY, -1.0f, 1.0f);

			effectiveCamPos.x += normX * 0.16f;
			effectiveCamPos.y -= normY * 0.08f;
			effectiveCamRot.y += normX * 0.022f;
			effectiveCamRot.x += normY * 0.012f;

			// 3Dレイキャスト（空間 Z = 0.0f と水面 Y = -0.72f への射影）
			float nx = normX;
			float ny = 1.0f - (currentMousePos.y / clientH) * 2.0f;
			Matrix4x4 viewProj = Multiply(camera_->GetViewMatrix(), camera_->GetProjectionMatrix());
			Matrix4x4 inv = Inverse(viewProj);

			auto transformClip = [&](const Vector4& c) -> Vector3 {
				Vector3 r;
				r.x = c.x * inv.m[0][0] + c.y * inv.m[1][0] + c.z * inv.m[2][0] + c.w * inv.m[3][0];
				r.y = c.x * inv.m[0][1] + c.y * inv.m[1][1] + c.z * inv.m[2][1] + c.w * inv.m[3][1];
				r.z = c.x * inv.m[0][2] + c.y * inv.m[1][2] + c.z * inv.m[2][2] + c.w * inv.m[3][2];
				float w = c.x * inv.m[0][3] + c.y * inv.m[1][3] + c.z * inv.m[2][3] + c.w * inv.m[3][3];
				if (w != 0.0f) { r.x /= w; r.y /= w; r.z /= w; }
				return r;
			};

			Vector3 pNear = transformClip({ nx, ny, 0.0f, 1.0f });
			Vector3 pFar  = transformClip({ nx, ny, 1.0f, 1.0f });
			Vector3 rayDir = { pFar.x - pNear.x, pFar.y - pNear.y, pFar.z - pNear.z };

			// 空間平面 Z = 0.0f
			if (std::abs(rayDir.z) > 1e-5f)
			{
				float tZ = (0.0f - pNear.z) / rayDir.z;
				mouseWorldPos = { pNear.x + rayDir.x * tZ, pNear.y + rayDir.y * tZ, 0.0f };
			}
			// 水面平面 Y = -0.72f
			if (std::abs(rayDir.y) > 1e-5f)
			{
				float tY = (-0.72f - pNear.y) / rayDir.y;
				mouseWaterPos = { pNear.x + rayDir.x * tY, -0.72f, pNear.z + rayDir.z * tY };
			}
			hasMouseRay = true;
		}

		camera_->SetTranslate(effectiveCamPos);
		camera_->SetRotate(effectiveCamRot);
		camera_->Update();
	}

	// マウス水面リップル発生源のライフタイム更新
	for (auto& s : mouseWaveSources_)
	{
		s.age += deltaTime;
	}
	mouseWaveSources_.erase(
		std::remove_if(mouseWaveSources_.begin(), mouseWaveSources_.end(),
			[](const MouseWaveRippleSource& s) { return s.age >= s.maxLife; }),
		mouseWaveSources_.end());

	// マウス移動・クリックによる水面3D凹凸リップルのリアルタイム注入
	mouseWaveEmitCooldown_ -= deltaTime;
	mouseRippleCooldown_ -= deltaTime;

	if (hasMouseRay && hasPrevMousePos_)
	{
		Vector3 waterDelta = { mouseWaterPos.x - prevMouseWorldPos_.x, 0.0f, mouseWaterPos.z - prevMouseWorldPos_.z };
		float waterDist = std::sqrt(waterDelta.x * waterDelta.x + waterDelta.z * waterDelta.z);

		if (waterDist > 0.025f && mouseWaveEmitCooldown_ <= 0.0f && mouseWaveSources_.size() < 16)
		{
			mouseWaveEmitCooldown_ = 0.045f;
			float speedScale = waterDist / (deltaTime > 0.0f ? deltaTime : 0.016f);
			MouseWaveRippleSource rip;
			rip.center = mouseWaterPos;
			rip.age = 0.0f;
			rip.maxLife = 1.6f;
			rip.power = std::clamp(speedScale * 0.035f, 0.03f, 0.09f);
			mouseWaveSources_.push_back(rip);
		}

		if (mouseInput_.TriggerButton(0) && mouseWaveSources_.size() < 16)
		{
			MouseWaveRippleSource rip;
			rip.center = mouseWaterPos;
			rip.age = 0.0f;
			rip.maxLife = 2.4f;
			rip.power = 0.16f; // クリックで力強い物理波紋
			mouseWaveSources_.push_back(rip);
		}
	}

	// 1. 手前のプレイヤーアヒルちゃん：川の流れに乗って穏やかに揺れる（自然なボブ＆ピッチ＆ロール）
	if (duckModel_ && camera_)
	{
		WaterSurfaceSample duckWater = EvaluateWaterSurface(duckBasePos_.x, duckBasePos_.z, bgTimer_);
		float duckX = duckBasePos_.x + std::sin(bgTimer_ * 0.5f) * 0.02f;
		float duckZ = duckBasePos_.z;
		float bobbingY = duckBasePos_.y + duckWater.displacement.y;

		// 法線ベクトルから自然で控えめなピッチ（前後傾斜 2〜3度）とロール（左右傾斜 1〜2度）を算出
		float wavePitch = 0.04f + duckWater.normal.z * 0.16f;
		float waveRoll = -duckWater.normal.x * 0.14f;
		float subtleYaw = -0.45f + std::sin(bgTimer_ * 0.6f) * 0.03f;

		duckModel_->SetScale({ duckScale_, duckScale_, duckScale_ });
		duckModel_->SetTranslate({ duckX, bobbingY, duckZ });
		duckModel_->SetRotate({ wavePitch, subtleYaw, waveRoll });
		duckModel_->Update();
	}

	// 2. 奥の敵アヒル兵士のコミカルな往復パトロール（土嚢バリケードの背後を警戒行進）
	if (enemyDuckModel_ && camera_)
	{
		float enemyX = -1.5f + std::sin(bgTimer_ * 0.8f) * 1.2f;
		float enemyBob = -0.32f + std::abs(std::sin(bgTimer_ * 4.0f)) * 0.035f;
		float enemyFacing = (std::cos(bgTimer_ * 0.8f) > 0.0f) ? 1.57f : -1.57f;

		enemyDuckModel_->SetTranslate({ enemyX, enemyBob, 2.3f });
		enemyDuckModel_->SetRotate({ 0.04f, enemyFacing, std::sin(bgTimer_ * 2.5f) * 0.03f });
		enemyDuckModel_->Update();
	}

	// 3. 川のリアルタイム3D波面物理シミュレーション（滑らかな起伏・動的スペキュラ法線・穏やかなUV流動）
	if (riverModel_ && riverModel_->GetVertexResource() && !waterBaseVertices_.empty())
	{
		struct VertexData
		{
			Vector4 position;
			Vector2 texcoord;
			Vector3 normal;
		};
		VertexData* vData = nullptr;
		D3D12_RANGE readRange{ 0, 0 };
		if (SUCCEEDED(riverModel_->GetVertexResource()->Map(0, &readRange, reinterpret_cast<void**>(&vData))) && vData)
		{
			float t = bgTimer_;
			float flowU = t * 0.075f; // 雄大で穏やかな川の流速

			for (size_t i = 0; i < waterBaseVertices_.size(); ++i)
			{
				const auto& base = waterBaseVertices_[i];
				WaterSurfaceSample sample = EvaluateWaterSurface(base.basePosX, base.basePosZ, t);

				// (X, Z)は安定したグリッドを維持し、Y（高さ）のみ波面起伏を適用（境界のブレや歪みを完全に防止）
				vData[i].position = { base.basePosX, sample.displacement.y, base.basePosZ, 1.0f };
				vData[i].normal = sample.normal;

				// UV座標：波の傾斜（法線水平成分）に応じた繊細で美しい光屈折ディストーション
				vData[i].texcoord.x = base.baseU - flowU + sample.normal.x * 0.035f;
				vData[i].texcoord.y = base.baseV + sample.normal.z * 0.035f;
			}

			D3D12_RANGE writeRange{ 0, sizeof(VertexData) * waterBaseVertices_.size() };
			riverModel_->GetVertexResource()->Unmap(0, &writeRange);
		}

		riverModel_->Update();
	}

	// 波頭の白泡・飛沫（Wave Crest Foam Particles - 大きなうねりの山にのみ微細な飛沫が立つ）
	waveFoamTimer_ += deltaTime;
	if (waveFoamTimer_ >= 0.08f && appParticleManager_ && GetParticleManager())
	{
		waveFoamTimer_ = 0.0f;
		auto& rng = GetParticleManager()->GetRandomEngine();
		std::uniform_real_distribution<float> foamX(-10.0f, 12.0f);
		std::uniform_real_distribution<float> foamZ(-1.5f, 6.0f);

		for (int f = 0; f < 2; ++f)
		{
			float fx = foamX(rng);
			float fz = foamZ(rng);
			WaterSurfaceSample s = EvaluateWaterSurface(fx, fz, bgTimer_);
			if (s.displacement.y > 0.028f)
			{
				Vector3 foamPos = { fx, -0.72f + s.displacement.y + 0.015f, fz };
				appParticleManager_->EmitRiverSplashDroplet(rng, foamPos, circleTexIndex_);
			}
		}
	}

	// 4. 背景ジオラマプロップ（軍用コンテナ、ドラム缶、土嚢、木箱）の更新
	if (containerModel_) containerModel_->Update();
	if (barrelStackModel_) barrelStackModel_->Update();
	if (sandbagModel_) sandbagModel_->Update();
	if (crateModel1_) crateModel1_->Update();
	if (crateModel2_) crateModel2_->Update();

	// 3. ミノフスキー粒子エフェクト（閃光のハサウェイ・キルケーの魔女風 圧倒的高密度GPUフォトン空間）
	if (appParticleManager_ && GetParticleManager())
	{
		auto& rng = GetParticleManager()->GetRandomEngine();
		minovskyAngle_ += deltaTime * 2.8f;

		// 手前のアヒルちゃん (duckModel_) の周囲を螺旋状に舞い上がる二重の微細光流
		Vector3 duckPos = duckModel_ ? duckModel_->GetTranslate() : duckBasePos_;
		Vector3 duckCenter = { duckPos.x, duckPos.y + 0.42f, duckPos.z };
		float swirlRadius = 0.70f;

		// (A) 二重螺旋のミノフスキー光流リボン（12分割サブステップ連続射出で実体感のある光の帯を形成）
		const int kSubSteps = 12;
		for (int step = 0; step < kSubSteps; ++step)
		{
			float subAngle = minovskyAngle_ + static_cast<float>(step) * (6.2831853f / 36.0f);
			float subHeight = -0.42f + std::fmod((minovskyAngle_ * 0.45f + static_cast<float>(step) * 0.045f), 1.25f);

			// シアン螺旋（キルケー・シアンの微細スパーク＆グロー）
			float cyanScale = (step % 3 == 0) ? 0.036f : 0.025f;
			uint32_t cyanTex = (step % 6 == 0 && starTexIndex_ != UINT32_MAX) ? starTexIndex_ : circleTexIndex_;
			appParticleManager_->EmitMinovskySwirl(rng, duckCenter, swirlRadius * (0.92f + (step % 2) * 0.14f), subHeight, subAngle, false, cyanTex, cyanScale);

			// マゼンタ螺旋（180度反対側のネオンマゼンタ光流）
			float magScale = (step % 3 == 0) ? 0.034f : 0.023f;
			uint32_t magTex = (step % 6 == 1 && starTexIndex_ != UINT32_MAX) ? starTexIndex_ : circleTexIndex_;
			appParticleManager_->EmitMinovskySwirl(rng, duckCenter, swirlRadius * (0.94f + (step % 3) * 0.10f), subHeight + 0.06f, subAngle + 3.14159f, true, magTex, magScale);
		}

		// (B) アヒル本体を包み込むミノフスキー・クラフト反重力フィールド（微細フォトンの雲）
		appParticleManager_->EmitMinovskyFlightAura(rng, duckCenter, 16, circleTexIndex_, starTexIndex_);

		// (C) アヒル本体の周囲に立ち上るキラキラ超微細クリスタルダスト（閃光のきらめき）
		for (int d = 0; d < 4; ++d)
		{
			std::uniform_real_distribution<float> rOff(-0.45f, 0.45f);
			std::uniform_real_distribution<float> microScale(0.030f, 0.065f);
			Vector3 dPos = { duckCenter.x + rOff(rng), duckCenter.y + rOff(rng) * 0.6f, duckCenter.z + rOff(rng) };
			uint32_t dTex = (starTexIndex_ != UINT32_MAX && (d % 2 == 0)) ? starTexIndex_ : circleTexIndex_;
			appParticleManager_->EmitMinovskyGlitter(rng, dPos, microScale(rng), dTex);
		}

		// (D) 空間全体にゆっくり漂う微細な浮遊ボケ粒子（被写界深度の星屑感）
		ambientMoteTimer_ += deltaTime;
		if (ambientMoteTimer_ >= 0.02f)
		{
			ambientMoteTimer_ = 0.0f;
			std::uniform_real_distribution<float> distX(-4.2f, 4.2f);
			std::uniform_real_distribution<float> distY(-1.8f, 2.5f);
			std::uniform_real_distribution<float> distZ(-3.8f, 2.2f);
			std::uniform_real_distribution<float> distScale(0.025f, 0.075f);
			std::uniform_int_distribution<int> distColor(0, 1);

			for (int m = 0; m < 3; ++m)
			{
				Vector3 motePos = { distX(rng), distY(rng), distZ(rng) };
				appParticleManager_->EmitMinovskyBokeh(rng, motePos, distScale(rng), distColor(rng) == 1, circleTexIndex_);
			}
		}

		// (E) マウス水面波紋パーティクルインタラクション（引き波・同心円リング）
		if (hasMouseRay && hasPrevMousePos_)
		{
			Vector3 worldDelta = { mouseWorldPos.x - prevMouseWorldPos_.x, mouseWorldPos.y - prevMouseWorldPos_.y, 0.0f };
			float worldDist = std::sqrt(worldDelta.x * worldDelta.x + worldDelta.y * worldDelta.y);
			Vector3 moveDir = (worldDist > 0.001f) ? (worldDelta * (1.0f / worldDist)) : Vector3{ 0.0f, 1.0f, 0.0f };

			// マウスが動いている時：川の流れを手やボートで進むような「引き波（Kelvin wake）」パーティクルを発生！
			if (worldDist > 0.025f && mouseWaveEmitCooldown_ <= 0.0f)
			{
				float speedScale = worldDist / (deltaTime > 0.0f ? deltaTime : 0.016f);
				appParticleManager_->EmitMouseWaveWake(rng, mouseWorldPos, moveDir, speedScale, circleTexIndex_, circleTexIndex_);
			}

			// 川の流れをかき分けるように、周囲の既存粒子を波で押し流す（流体インタラクション）
			if (worldDist > 0.008f)
			{
				appParticleManager_->ApplyMouseWaveDisturbance(mouseWorldPos, worldDelta * (1.0f / (deltaTime > 0.0f ? deltaTime : 0.016f)), 1.25f, 1.8f);
			}
		}

		// クリック時、または長押し時に水面に石を投げたような「同心円の波紋」を発生！
		if (hasMouseRay)
		{
			if (mouseInput_.TriggerButton(0) || (mouseInput_.PushButton(0) && mouseRippleCooldown_ <= 0.0f))
			{
				appParticleManager_->EmitMouseRippleRing(rng, mouseWorldPos, 1.15f, circleTexIndex_, circleTexIndex_);
				appParticleManager_->ApplyMouseWaveDisturbance(mouseWorldPos, { 0.0f, 0.0f, 0.0f }, 2.0f, 3.5f);
			}

			prevMousePos_ = currentMousePos;
			prevMouseWorldPos_ = mouseWorldPos;
			hasPrevMousePos_ = true;
		}

		appParticleManager_->Update(deltaTime, duckCenter);
	}

	// 4. 2DスプライトUIの更新（PRESS SPACE の呼吸パルス発光）
	if (pressSpaceSprite_ && !isStarting_)
	{
		float pulse = 0.5f + 0.5f * std::sin(bgTimer_ * 3.6f);
		float alpha = 0.35f + 0.65f * pulse;
		pressSpaceSprite_->SetColor({ 1.0f, 1.0f, 1.0f, alpha });
	}

	if (spriteManager_)
	{
		spriteManager_->Update();
	}
	for (auto& sp : sprites_)
	{
		if (sp) sp->Update();
	}
}

void TitleScene::Draw(SceneRenderRequests& renderRequests)
{
	if (camera_)
	{
		camera_->Update();
	}

	if (GetObject3dCom() && dxCommon_ && camera_)
	{
		RenderContext baseCtx{};
		baseCtx.commandList = dxCommon_->GetCommandList().Get();
		baseCtx.windowAPI = dxCommon_->GetWindowAPI();
		baseCtx.camera = camera_;
		baseCtx.light = SceneManager::GetInstance() ? SceneManager::GetInstance()->GetLight() : nullptr;
		MaterialManager* matMgr = GetMaterialManager();
		if (matMgr && matMgr->GetMaterialResource())
		{
			baseCtx.materialGPUAddress = matMgr->GetMaterialResource()->GetGPUVirtualAddress();
		}

		// 1. 3Dシーン描画
		// 軍用コンテナ (背景左奥)
		if (containerModel_)
		{
			RenderContext ctx = baseCtx;
			if (containerTexIndex_ != UINT32_MAX)
			{
				ctx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(containerTexIndex_);
			}
			GetObject3dCom()->Draw(containerModel_.get(), ctx, containerModel_->GetModelData(), true);
		}

		// 危険物ドラム缶スタック (背景中央奥)
		if (barrelStackModel_)
		{
			RenderContext ctx = baseCtx;
			if (barrelTexIndex_ != UINT32_MAX)
			{
				ctx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(barrelTexIndex_);
			}
			GetObject3dCom()->Draw(barrelStackModel_.get(), ctx, barrelStackModel_->GetModelData(), true);
		}

		// 土嚢バリケード (敵パトロールの手前)
		if (sandbagModel_)
		{
			RenderContext ctx = baseCtx;
			if (sandbagTexIndex_ != UINT32_MAX)
			{
				ctx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(sandbagTexIndex_);
			}
			GetObject3dCom()->Draw(sandbagModel_.get(), ctx, sandbagModel_->GetModelData(), true);
		}

		// 軍用木箱 (右岸手前)
		if (crateModel1_)
		{
			RenderContext ctx = baseCtx;
			if (crateTexIndex_ != UINT32_MAX)
			{
				ctx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(crateTexIndex_);
			}
			GetObject3dCom()->Draw(crateModel1_.get(), ctx, crateModel1_->GetModelData(), true);
		}
		if (crateModel2_)
		{
			RenderContext ctx = baseCtx;
			if (crateTexIndex_ != UINT32_MAX)
			{
				ctx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(crateTexIndex_);
			}
			GetObject3dCom()->Draw(crateModel2_.get(), ctx, crateModel2_->GetModelData(), true);
		}

		// 奥の敵アヒル兵士 (土嚢の背後をパトロール)
		if (enemyDuckModel_)
		{
			RenderContext enemyCtx = baseCtx;
			uint32_t enemyTexIdx = TextureManager::GetInstance()->Load("Resources/duck_enemy.png");
			if (enemyTexIdx != UINT32_MAX)
			{
				enemyCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(enemyTexIdx);
			}
			GetObject3dCom()->Draw(enemyDuckModel_.get(), enemyCtx, enemyDuckModel_->GetModelData(), true);
		}

		// 川の水面プレーン (足元一面の雄大な水面)
		if (riverModel_)
		{
			RenderContext riverCtx = baseCtx;
			if (riverTexIndex_ != UINT32_MAX)
			{
				riverCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(riverTexIndex_);
			}
			GetObject3dCom()->Draw(riverModel_.get(), riverCtx, riverModel_->GetModelData(), true);
		}

		// 手前のプレイヤーアヒルちゃん (水面に浮かぶ主役)
		if (duckModel_)
		{
			RenderContext ctx = baseCtx;
			uint32_t texIdx = TextureManager::GetInstance()->Load("Resources/duck.png");
			if (texIdx != UINT32_MAX)
			{
				ctx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(texIdx);
			}
			GetObject3dCom()->Draw(duckModel_.get(), ctx, duckModel_->GetModelData(), true);
		}

		// ミノフスキー粒子の3D加算描画
		if (appParticleManager_)
		{
			RenderContext particleCtx = baseCtx;
			if (circleTexIndex_ != UINT32_MAX)
			{
				particleCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(circleTexIndex_);
			}
			appParticleManager_->Draw(particleCtx);
		}

		// 2. 2DタイトルスプライトUI描画（ネイティブSprite描画）
		if (spriteManager_)
		{
			spriteManager_->DrawAll(baseCtx, camera_, &sprites_);
		}
	}

	renderRequests.sceneDrawn = true;
}

TitleScene::WaterSurfaceSample TitleScene::EvaluateWaterSurface(float x, float z, float time) const
{
	WaterSurfaceSample result{};
	result.normal = { 0.0f, 1.0f, 0.0f };

	// 川の流れに沿った穏やかで雄大な3層の川波パラメータ（すべて下流方向 +X を基調とした自然なうねり）
	// 振幅(amplitude), 波長(wavelength), 流速(speed), 方向(dirX, dirZ)
	struct RiverWaveParam
	{
		float dirX;
		float dirZ;
		float amplitude;
		float wavelength;
		float speed;
	};

	static const RiverWaveParam kWaves[3] = {
		// 1. 主流域の雄大でゆったりとした主うねり（川の流向に沿った滑らかな起伏）
		{ 0.965f, -0.262f, 0.032f, 8.2f, 1.15f },
		// 2. 岸辺の地形による緩やかな斜行スウェル（自然な立体感を添える）
		{ 0.894f,  0.447f, 0.018f, 5.4f, 1.35f },
		// 3. 風による水面の穏やかなさざ波（光のきらめき）
		{ 0.990f, -0.141f, 0.007f, 3.2f, 1.80f }
	};

	float dispY = 0.0f;
	float gradX = 0.0f;
	float gradZ = 0.0f;

	for (const auto& w : kWaves)
	{
		float k = 6.2831853f / w.wavelength; // 波数 k = 2pi / lambda
		float omega = k * w.speed;           // 角振動数
		float theta = k * (w.dirX * x + w.dirZ * z) - omega * time;

		float sinT = std::sin(theta);
		float cosT = std::cos(theta);

		dispY += w.amplitude * cosT;

		// 偏微分による法線勾配（解析的微分）
		gradX -= w.dirX * (k * w.amplitude) * sinT;
		gradZ -= w.dirZ * (k * w.amplitude) * sinT;
	}

	// アヒルちゃんの周囲に広がる穏やかな水押し波
	float dxDuck = x - duckBasePos_.x;
	float dzDuck = z - duckBasePos_.z;
	float distDuck = std::sqrt(dxDuck * dxDuck + dzDuck * dzDuck);
	if (distDuck > 0.10f && distDuck < 4.0f)
	{
		float decay = std::exp(-distDuck * 0.90f);
		float ripplePhase = distDuck * 2.6f - time * 2.4f;
		float rippleAmp = 0.015f * decay;
		float rY = rippleAmp * std::cos(ripplePhase);
		dispY += rY;

		float dRipple = -rippleAmp * (0.90f * std::cos(ripplePhase) + 2.6f * std::sin(ripplePhase));
		gradX -= dRipple * (dxDuck / distDuck);
		gradZ -= dRipple * (dzDuck / distDuck);
	}

	// マウスによる動的3D水面凹凸（メッシュ解像度に合わせた滑らかな広域ウェイク）
	for (const auto& rip : mouseWaveSources_)
	{
		if (rip.age >= rip.maxLife) continue;
		float lifeRatio = rip.age / rip.maxLife;
		float fade = (1.0f - lifeRatio) * (1.0f - lifeRatio);

		float rx = x - rip.center.x;
		float rz = z - rip.center.z;
		float r = std::sqrt(rx * rx + rz * rz);

		float waveFrontR = 0.6f + rip.age * 2.2f;
		float dr = r - waveFrontR;
		float ringWidth = 1.35f;

		if (std::abs(dr) < ringWidth && r > 0.05f)
		{
			float ringEnv = 0.5f + 0.5f * std::cos((dr / ringWidth) * 3.14159265f);
			float waveAmp = rip.power * fade * ringEnv * 0.024f;
			dispY += waveAmp;

			float dWave = -waveAmp * (dr / ringWidth);
			gradX -= dWave * (rx / r);
			gradZ -= dWave * (rz / r);
		}
	}

	// 解析的法線ベクトルの正規化
	Vector3 norm = { -gradX, 1.0f, -gradZ };
	float len = std::sqrt(norm.x * norm.x + norm.y * norm.y + norm.z * norm.z);
	if (len > 1e-4f)
	{
		norm.x /= len;
		norm.y /= len;
		norm.z /= len;
	}

	result.displacement = { 0.0f, dispY, 0.0f };
	result.normal = norm;
	result.foamFactor = std::clamp((dispY - 0.035f) * 20.0f, 0.0f, 1.0f);
	return result;
}
