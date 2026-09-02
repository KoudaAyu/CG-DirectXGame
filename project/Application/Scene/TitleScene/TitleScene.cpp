#include "TitleScene.h"
#include "KeyInput.h"
#include "DirectXCom.h"
#include "SceneManager.h"
#include "Camera.h"
#include "TextureManager.h"
#include "Object3dCom.h"
#include "RenderContext.h"
#include "Baziru3_Engine/Graphics/Graphics/SceneRenderRequests.h"
#include <cmath>
#include <algorithm>

#ifdef USE_IMGUI
#include "imgui.h"
#endif

void TitleScene::InitializeScene()
{
	if (dxCommon_)
	{
		input_ = new KeyInput();
		input_->Initialize(dxCommon_->GetWindowAPI());
	}
	currentMenu_ = MenuState::Main;
	bgTimer_ = 0.0f;
	startTransitionTimer_ = 0.0f;
	isStarting_ = false;
	duckJumpTimer_ = 0.0f;
	duckSpinAngle_ = 0.0f;
	duckCurrentYaw_ = -0.5f;
	duckCurrentPitch_ = 0.1f;
	splashes_.clear();

	// タイトル専用カメラ（右側にアヒルちゃんが大きく魅力的に映るアングル）
	if (camera_)
	{
		camera_->SetTranslate({ 0.0f, 0.30f, -3.6f });
		camera_->SetRotate({ 0.08f, 0.0f, 0.0f });
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
		duckModel_->SetScale({ 2.0f, 2.0f, 2.0f }); // 大きく見栄え良く！
		duckModel_->SetTranslate({ 1.25f, -0.35f, 0.0f });
		duckModel_->SetRotate({ 0.1f, -0.55f, 0.0f });
		duckModel_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

		// 2. 奥をパトロールする敵アヒル兵士 (Enemy Duck Patrol)
		Object3d::ModelData enemyModel = Object3d::LoadObjFile("Resources", "player.obj");
		enemyModel.material.textureFilePath = "Resources/duck_enemy.png";
		enemyModel.boundingRadius = 10.0f;
		enemyDuckModel_ = std::make_unique<Object3d>();
		enemyDuckModel_->Initialize(GetObject3dCom(), enemyModel);
		enemyDuckModel_->SetCamera(camera_);
		enemyDuckModel_->SetScale({ 1.3f, 1.3f, 1.3f });
		enemyDuckModel_->SetTranslate({ -1.2f, 0.35f, 2.4f });
		enemyDuckModel_->SetRotate({ 0.05f, 1.57f, 0.0f });
		enemyDuckModel_->SetColor({ 0.95f, 0.95f, 1.0f, 1.0f });
	}
}

void TitleScene::Finalize()
{
	delete input_;
	input_ = nullptr;
	if (duckModel_)
	{
		duckModel_.reset();
	}
	if (enemyDuckModel_)
	{
		enemyDuckModel_.reset();
	}
}

void TitleScene::Update()
{
	if (input_)
	{
		input_->Update();
	}

#ifdef USE_IMGUI
	ImGuiIO& io = ImGui::GetIO();
	float deltaTime = io.DeltaTime;
	bgTimer_ += deltaTime;

	float screenW = io.DisplaySize.x;
	float screenH = io.DisplaySize.y;

	// マウスカーソル位置の取得
	ImVec2 mousePos = io.MousePos;
	float normMouseX = (mousePos.x / screenW) * 2.0f - 1.0f; // -1.0 ~ 1.0
	float normMouseY = (mousePos.y / screenH) * 2.0f - 1.0f;

	// クリック時の水しぶき爆発生成＆アヒルのジャンプ
	if (io.MouseClicked[0] && !isStarting_)
	{
		splashes_.push_back({ mousePos.x, mousePos.y, 0.0f, 0.5f, 1.0f });

		if (currentMenu_ == MenuState::Main && mousePos.x > screenW * 0.45f)
		{
			if (duckJumpTimer_ <= 0.0f)
			{
				duckJumpTimer_ = 0.55f; // クリックでジャンプも発動！
			}
		}
	}

	// 水しぶき爆発パーティクルの更新
	for (auto it = splashes_.begin(); it != splashes_.end();)
	{
		it->timer += deltaTime;
		if (it->timer >= it->maxTime)
		{
			it = splashes_.erase(it);
		}
		else
		{
			++it;
		}
	}

	// ジャンプ＆スピン演出の更新
	float jumpOffsetY = 0.0f;
	if (duckJumpTimer_ > 0.0f)
	{
		duckJumpTimer_ -= deltaTime;
		float jumpProgress = 1.0f - (duckJumpTimer_ / 0.55f);
		if (jumpProgress < 0.0f) jumpProgress = 0.0f;
		if (jumpProgress > 1.0f) jumpProgress = 1.0f;

		jumpOffsetY = std::sin(jumpProgress * 3.14159265f) * 0.42f;
		duckSpinAngle_ = jumpProgress * 6.2831853f;
	}
	else
	{
		duckSpinAngle_ = 0.0f;
	}

	// 1. 手前のプレイヤーアヒルちゃんのプカプカ浮遊 ＆ 視線追従
	if (duckModel_ && camera_)
	{
		float bobbingY = -0.35f + std::sin(bgTimer_ * 2.2f) * 0.05f + jumpOffsetY;
		float wobbleRoll = std::sin(bgTimer_ * 1.8f) * 0.06f;

		float targetYaw = -0.55f + normMouseX * 0.45f;
		float targetPitch = 0.10f + normMouseY * 0.25f;
		duckCurrentYaw_ += (targetYaw - duckCurrentYaw_) * 5.0f * deltaTime;
		duckCurrentPitch_ += (targetPitch - duckCurrentPitch_) * 5.0f * deltaTime;

		float finalYaw = duckCurrentYaw_ + duckSpinAngle_;
		float finalPitch = duckCurrentPitch_ + std::cos(bgTimer_ * 2.2f) * 0.03f;

		duckModel_->SetTranslate({ 1.25f, bobbingY, 0.0f });
		duckModel_->SetRotate({ finalPitch, finalYaw, wobbleRoll });
		duckModel_->Update();
	}

	// 2. 奥の敵アヒル兵士のコミカルな往復パトロール（Diorama Animation）
	if (enemyDuckModel_ && camera_)
	{
		float enemyX = std::sin(bgTimer_ * 0.9f) * 2.6f;
		float enemyBob = 0.15f + std::sin(bgTimer_ * 3.0f) * 0.04f;
		float enemyFacing = (std::cos(bgTimer_ * 0.9f) > 0.0f) ? 1.57f : -1.57f; // 進行方向を向く

		enemyDuckModel_->SetTranslate({ enemyX, enemyBob, 2.2f });
		enemyDuckModel_->SetRotate({ 0.05f, enemyFacing, std::sin(bgTimer_ * 2.5f) * 0.05f });
		enemyDuckModel_->Update();
	}

	// 出撃遷移演出（タクティカル・フェードアウト＆出撃中ローディング演出）
	if (isStarting_)
	{
		startTransitionTimer_ += deltaTime;
		float progress = (std::min)(1.0f, startTransitionTimer_ / 0.75f);
		int fadeAlpha = static_cast<int>(255.0f * progress);

		ImDrawList* fg = ImGui::GetForegroundDrawList();
		if (fg && screenW > 0.0f && screenH > 0.0f)
		{
			// フルスクリーンブラックフェード
			fg->AddRectFilled(ImVec2(0, 0), ImVec2(screenW, screenH), IM_COL32(8, 14, 20, fadeAlpha));

			if (progress > 0.20f)
			{
				float textAlpha = (progress - 0.20f) / 0.80f;
				int tVal = static_cast<int>(255.0f * textAlpha);

				const char* depText = ">> DEPLOYING TO MISSION ZONE // 作戦区域へ出撃中...";
				ImVec2 txtSz = ImGui::CalcTextSize(depText);
				float tx = (screenW - txtSz.x) * 0.5f;
				float ty = (screenH - txtSz.y) * 0.5f - 15.0f;

				fg->AddText(ImVec2(tx, ty), IM_COL32(0, 255, 160, tVal), depText);

				// プログレスバー
				float barW = 320.0f;
				float barH = 6.0f;
				float bx = (screenW - barW) * 0.5f;
				float by = ty + txtSz.y + 16.0f;

				fg->AddRectFilled(ImVec2(bx, by), ImVec2(bx + barW, by + barH), IM_COL32(20, 35, 45, tVal), 3.0f);
				fg->AddRectFilled(ImVec2(bx, by), ImVec2(bx + barW * progress, by + barH), IM_COL32(0, 255, 160, tVal), 3.0f);
			}
		}

		if (startTransitionTimer_ >= 0.85f)
		{
			SceneManager::GetInstance()->ChangeScene("GAMEPLAY");
			return;
		}
	}

	// キー入力ハンドリング
	if (!isStarting_ && input_)
	{
		if (currentMenu_ == MenuState::Main)
		{
			if (input_->TriggerKey(DIK_SPACE) || input_->TriggerKey(DIK_RETURN))
			{
				isStarting_ = true;
				startTransitionTimer_ = 0.0f;
			}
			else if (input_->TriggerKey(DIK_B) || input_->TriggerKey(DIK_H))
			{
				currentMenu_ = MenuState::Briefing;
			}
			else if (input_->TriggerKey(DIK_S))
			{
				currentMenu_ = MenuState::Settings;
			}
			else if (input_->TriggerKey(DIK_ESCAPE))
			{
				PostQuitMessage(0);
			}
		}
		else if (input_->TriggerKey(DIK_ESCAPE) || input_->TriggerKey(DIK_SPACE) || input_->TriggerKey(DIK_RETURN))
		{
			currentMenu_ = MenuState::Main;
		}
	}

	// 1. ポップ＆リバー動的背景（サンシャフト、レインボーコースティクス、波紋、水泡、紙吹雪、水しぶき爆発）
	DrawCinematicBackground(screenW, screenH, deltaTime);

	// 2. メニュー描画
	if (currentMenu_ == MenuState::Main)
	{
		DrawMainMenu(screenW, screenH, deltaTime);
	}
	else if (currentMenu_ == MenuState::Briefing)
	{
		DrawBriefingModal(screenW, screenH);
	}
	else if (currentMenu_ == MenuState::Settings)
	{
		DrawSettingsModal(screenW, screenH);
	}
#endif
}

void TitleScene::Draw(SceneRenderRequests& renderRequests)
{
	if (GetObject3dCom() && dxCommon_ && camera_)
	{
		RenderContext baseCtx{};
		baseCtx.commandList = dxCommon_->GetCommandList().Get();
		baseCtx.windowAPI = dxCommon_->GetWindowAPI();
		baseCtx.camera = camera_;
		baseCtx.light = SceneManager::GetInstance() ? SceneManager::GetInstance()->GetLight() : nullptr;

		// 1. 奥の敵アヒル兵士の描画
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

		// 2. 手前のプレイヤーアヒルちゃんの描画
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
	}
	renderRequests.sceneDrawn = true;
}

#ifdef USE_IMGUI
void TitleScene::DrawCinematicBackground(float screenW, float screenH, float deltaTime)
{
	ImDrawList* dl = ImGui::GetBackgroundDrawList();
	if (!dl || screenW <= 0.0f || screenH <= 0.0f) return;

	// (A) ダック軍基地（Tactical HQ）の重厚でハイテックなミリタリーネイビー背景
	dl->AddRectFilledMultiColor(
		ImVec2(0, 0), ImVec2(screenW, screenH),
		IM_COL32(10, 20, 32, 255),   // 左上: ダークミリタリーネイビー
		IM_COL32(14, 28, 44, 255),   // 右上: コマンドスレート
		IM_COL32(8, 18, 28, 255),    // 右下: ディープベース
		IM_COL32(6, 14, 22, 255)     // 左下: チャコールブラック
	);

	// (B) 司令部ホログラム・タクティカルグリッド（Base Tactical Grid）
	float gridSize = 48.0f;
	ImU32 gridCol = IM_COL32(0, 180, 255, 18);
	ImU32 gridDotCol = IM_COL32(0, 220, 255, 45);

	for (float x = 0.0f; x < screenW; x += gridSize)
	{
		for (float y = 0.0f; y < screenH; y += gridSize)
		{
			dl->AddCircleFilled(ImVec2(x, y), 1.2f, gridDotCol);
		}
	}

	// (C) 画面右側: 3Dアヒルちゃんのホログラム出撃台座（Hologram Base Platform）
	float pedestalCx = screenW * 0.70f;
	float pedestalCy = screenH * 0.68f;

	// 台座の六角形・円形ホログラムリング
	for (int ring = 1; ring <= 3; ++ring)
	{
		float rRad = ring * 45.0f;
		float rAlpha = 0.25f - ring * 0.05f;
		dl->AddEllipse(ImVec2(pedestalCx, pedestalCy), ImVec2(rRad, rRad * 0.38f), IM_COL32(0, 220, 255, static_cast<int>(255 * rAlpha)), 0.0f, 32, 2.0f);
	}
	// 足元の発光スポット
	dl->AddEllipseFilled(ImVec2(pedestalCx, pedestalCy), ImVec2(110.0f, 110.0f * 0.38f), IM_COL32(0, 180, 255, 30));

	// (D) 画面右上: 作戦エリア情報モニター（Sector River Operation Monitor）
	float monX = screenW - 320.0f;
	float monY = 30.0f;
	float monW = 290.0f;
	float monH = 110.0f;

	dl->AddRectFilled(ImVec2(monX, monY), ImVec2(monX + monW, monY + monH), IM_COL32(10, 24, 38, 200), 8.0f);
	dl->AddRect(ImVec2(monX, monY), ImVec2(monX + monW, monY + monH), IM_COL32(0, 200, 240, 180), 8.0f, 0, 1.5f);

	dl->AddText(ImVec2(monX + 16.0f, monY + 12.0f), IM_COL32(0, 230, 255, 240), "📡 OPERATION INTEL // SECTOR RIVER");
	dl->AddText(ImVec2(monX + 16.0f, monY + 36.0f), IM_COL32(200, 220, 240, 220), "• OBJECTIVE : DESTROY 3 TARGETS");
	dl->AddText(ImVec2(monX + 16.0f, monY + 58.0f), IM_COL32(200, 220, 240, 220), "• EXTRACTION : HELIPAD NORTH");
	dl->AddText(ImVec2(monX + 16.0f, monY + 80.0f), IM_COL32(80, 255, 160, 240), "• STATUS    : READY FOR DEPLOY");

	// (E) 浮遊する微細ホログラムダスト・光粒子（Ambient Data Dust）
	const int numDust = 32;
	for (int i = 0; i < numDust; ++i)
	{
		float seed = static_cast<float>(i);
		float speed = 12.0f + std::fmod(seed * 17.0f, 20.0f);
		float x = std::fmod(seed * 183.0f + std::sin(bgTimer_ * 0.6f + seed) * 25.0f, screenW);
		float y = std::fmod(screenH - (bgTimer_ * speed + seed * 61.0f), screenH);
		if (y < 0.0f) y += screenH;

		float dSize = (i % 3 == 0) ? 2.5f : 1.5f;
		float dAlpha = 0.25f + 0.35f * std::sin(bgTimer_ * 1.8f + seed);
		if (dAlpha < 0.05f) dAlpha = 0.05f;

		ImU32 dCol = (i % 2 == 0)
			? IM_COL32(0, 220, 255, static_cast<int>(255 * dAlpha))   // シアンホログラム
			: IM_COL32(255, 215, 50, static_cast<int>(255 * dAlpha));  // ダックイエロー

		dl->AddCircleFilled(ImVec2(x, y), dSize, dCol);
	}

	// (F) 💥 クリック時のホログラム衝撃波エフェクト
	for (const auto& sp : splashes_)
	{
		float progress = sp.timer / sp.maxTime;
		float sAlpha = (1.0f - progress);

		float sRingR = progress * 55.0f;
		dl->AddCircle(ImVec2(sp.x, sp.y), sRingR, IM_COL32(0, 230, 255, static_cast<int>(255 * sAlpha)), 24, 2.0f * sAlpha);
		dl->AddCircleFilled(ImVec2(sp.x, sp.y), (1.0f - progress) * 4.0f, IM_COL32(255, 230, 80, static_cast<int>(255 * sAlpha)));
	}

	// (G) 出撃開始時のホワイトアウトフェード
	if (isStarting_)
	{
		float transAlpha = (startTransitionTimer_ / 0.6f);
		if (transAlpha > 1.0f) transAlpha = 1.0f;
		dl->AddRectFilled(ImVec2(0, 0), ImVec2(screenW, screenH), IM_COL32(245, 250, 255, static_cast<int>(255 * transAlpha)));
	}
}

void TitleScene::DrawMainMenu(float screenW, float screenH, float deltaTime)
{
	ImDrawList* dl = ImGui::GetForegroundDrawList();
	if (!dl || isStarting_) return;

	float startX = screenW * 0.07f;
	float startY = screenH * 0.16f;

	// --- 1. 超豪華立体エンブレム・タイトルロゴプレート ---
	float bannerW = 460.0f;
	float bannerH = 115.0f;
	ImVec2 bMin = ImVec2(startX - 15.0f, startY - 10.0f);
	ImVec2 bMax = ImVec2(startX + bannerW, startY + bannerH);

	// プレート背景 (ディープネイビー ＋ ゴールド枠)
	dl->AddRectFilled(bMin, bMax, IM_COL32(10, 26, 42, 220), 16.0f);
	dl->AddRect(bMin, bMax, IM_COL32(255, 215, 30, 240), 16.0f, 0, 3.0f);
	dl->AddRect(ImVec2(bMin.x + 3.0f, bMin.y + 3.0f), ImVec2(bMax.x - 3.0f, bMax.y - 3.0f), IM_COL32(80, 200, 255, 180), 14.0f, 0, 1.5f);

	// 人の目を引くキラキラスパークル (Jewel Sparkles)
	for (int s = 0; s < 4; ++s)
	{
		float spkTime = bgTimer_ * 1.5f + s * 1.6f;
		float spkCycle = std::fmod(spkTime, 2.5f);
		if (spkCycle < 0.6f)
		{
			float spkProgress = spkCycle / 0.6f;
			float spkAlpha = std::sin(spkProgress * 3.14159f);
			float spkX = startX + 50.0f + s * 95.0f;
			float spkY = startY + 12.0f;
			float spkSize = 6.0f * spkAlpha;

			dl->AddCircleFilled(ImVec2(spkX, spkY), spkSize * 1.5f, IM_COL32(255, 230, 120, static_cast<int>(120 * spkAlpha)));
			dl->AddCircleFilled(ImVec2(spkX, spkY), spkSize * 0.7f, IM_COL32(255, 255, 255, static_cast<int>(255 * spkAlpha)));
		}
	}

	ImGui::SetNextWindowPos(ImVec2(startX, startY), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(540.0f, 540.0f), ImGuiCond_Always);
	ImGui::Begin("TitlePopMenu", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground);

	// タイトルロゴ (ダックイエロー立体文字・固定サイズ)
	dl->AddText(ImVec2(startX + 3.0f, startY + 3.0f), IM_COL32(0, 0, 0, 240), "ESCAPE FROM DUCKOV");

	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.88f, 0.12f, 1.0f));
	ImGui::SetWindowFontScale(2.3f);
	ImGui::Text("ESCAPE FROM DUCKOV");
	ImGui::SetWindowFontScale(1.0f);
	ImGui::PopStyleColor();

	// サブタイトルバッジ (世界観統一)
	ImGui::Dummy(ImVec2(0.0f, 4.0f));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.95f, 0.85f, 1.0f));
	ImGui::Text((const char*)u8"🦆 TACTICAL DUCK EXTRACTION OPERATION // SECTOR RIVER 🦆");
	ImGui::PopStyleColor();

	ImGui::Dummy(ImVec2(0.0f, 32.0f));

	// --- 2. ポップ＆タクティカルな角丸ボタン群 (目を引くパルス付き) ---
	float btnW = 330.0f;
	float btnH = 50.0f;

	auto drawPopButton = [&](const char* label, const char* shortcut, ImVec4 baseCol, ImVec4 hoverCol, bool isPulsing = false) -> bool {
		ImVec4 finalCol = baseCol;
		if (isPulsing)
		{
			float pulse = 0.5f + 0.5f * std::sin(bgTimer_ * 4.0f);
			finalCol.x = baseCol.x + (hoverCol.x - baseCol.x) * pulse * 0.35f;
			finalCol.y = baseCol.y + (hoverCol.y - baseCol.y) * pulse * 0.35f;
			finalCol.z = baseCol.z + (hoverCol.z - baseCol.z) * pulse * 0.35f;
		}

		ImGui::PushStyleColor(ImGuiCol_Button, finalCol);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverCol);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(baseCol.x * 0.8f, baseCol.y * 0.8f, baseCol.z * 0.8f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);

		char fullText[64];
		sprintf_s(fullText, "%s   %s", label, shortcut);
		bool clicked = ImGui::Button(fullText, ImVec2(btnW, btnH));

		ImGui::PopStyleVar();
		ImGui::PopStyleColor(4);

		ImGui::Dummy(ImVec2(0.0f, 8.0f));
		return clicked;
	};

	// 1. 出撃ボタン (パルス発光)
	if (drawPopButton((const char*)u8"  ▶  START MISSION", "[ SPACE ]", ImVec4(0.95f, 0.58f, 0.05f, 0.95f), ImVec4(1.0f, 0.75f, 0.18f, 1.0f), true))
	{
		isStarting_ = true;
		startTransitionTimer_ = 0.0f;
	}

	// 2. あそびかたボタン
	if (drawPopButton((const char*)u8"  📋  HOW TO PLAY", "[ B ]", ImVec4(0.12f, 0.65f, 0.48f, 0.90f), ImVec4(0.18f, 0.78f, 0.58f, 1.0f)))
	{
		currentMenu_ = MenuState::Briefing;
	}

	// 3. せっていボタン
	if (drawPopButton((const char*)u8"  ⚙️  SETTINGS", "[ S ]", ImVec4(0.15f, 0.45f, 0.72f, 0.90f), ImVec4(0.22f, 0.55f, 0.85f, 1.0f)))
	{
		currentMenu_ = MenuState::Settings;
	}

	// 4. おわるボタン
	if (drawPopButton((const char*)u8"  ✖  QUIT GAME", "[ ESC ]", ImVec4(0.25f, 0.32f, 0.38f, 0.85f), ImVec4(0.35f, 0.42f, 0.48f, 1.0f)))
	{
		PostQuitMessage(0);
	}

	// 3Dアヒルのインタラクションガイド
	ImGui::Dummy(ImVec2(0.0f, 14.0f));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.9f, 1.0f, 0.85f));
	ImGui::Text((const char*)u8"💡 右のアヒルちゃんをクリックやドラッグで遊べるよ！");
	ImGui::PopStyleColor();

	// 画面下部の可愛いバージョン表記
	ImGui::Dummy(ImVec2(0.0f, 8.0f));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 0.9f, 0.8f));
	ImGui::Text("v2.5.0 // DUCK ADVENTURE EDITION");
	ImGui::PopStyleColor();

	ImGui::End();
}

void TitleScene::DrawBriefingModal(float screenW, float screenH)
{
	float modalW = 640.0f;
	float modalH = 440.0f;
	ImGui::SetNextWindowPos(ImVec2((screenW - modalW) * 0.5f, (screenH - modalH) * 0.5f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(modalW, modalH), ImGuiCond_Always);

	ImGui::Begin("HowToPlayModal", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);

	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 wPos = ImGui::GetWindowPos();
	ImVec2 wMax = ImVec2(wPos.x + modalW, wPos.y + modalH);

	// ポップなディープブルーパネル ＆ イエローゴールド枠
	dl->AddRectFilled(wPos, wMax, IM_COL32(16, 38, 58, 250), 12.0f);
	dl->AddRect(wPos, wMax, IM_COL32(255, 210, 40, 240), 12.0f, 0, 2.5f);

	ImGui::SetCursorPos(ImVec2(30.0f, 25.0f));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.88f, 0.15f, 1.0f));
	ImGui::SetWindowFontScale(1.3f);
	ImGui::Text((const char*)u8"🦆 あそびかた // HOW TO PLAY");
	ImGui::SetWindowFontScale(1.0f);
	ImGui::PopStyleColor();

	ImGui::Dummy(ImVec2(0.0f, 12.0f));

	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.98f, 1.0f, 1.0f));
	ImGui::TextWrapped((const char*)u8"【ゲームの目的】\n1. ステージに隠された【3つの射撃標的】をすべて撃ち抜こう！\n2. 敵のアヒル兵士に見つからないように、コンテナやフェンスの【かげ（遮蔽物）】に隠れよう！\n3. 標的を壊したら、川の向こうの【脱出ヘリパッド】へ向かって脱出しよう！");

	ImGui::Dummy(ImVec2(0.0f, 14.0f));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.95f, 0.75f, 1.0f));
	ImGui::Text((const char*)u8"【そうさほうほう】");
	ImGui::PopStyleColor();

	ImGui::TextWrapped((const char*)u8"・[ W ][ A ][ S ][ D ] : いどう\n・[ マウス ] / [ 左クリック ] : ねらいをつける / たまをうつ\n・[ SPACE ] : かいひローリング（すばやく前転！）\n・[ R ] : リロード（たまごめ）\n・遮蔽物のそばに行くと、自動的に 🛡️ COVER（かくれる）状態になって見つかりにくくなるよ！");
	ImGui::PopStyleColor();

	ImGui::Dummy(ImVec2(0.0f, 20.0f));
	ImGui::SetCursorPosX((modalW - 180.0f) * 0.5f);

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.65f, 0.48f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.78f, 0.58f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.08f, 0.50f, 0.35f, 1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
	if (ImGui::Button((const char*)u8"  わかった！ [ESC]  ", ImVec2(180.0f, 40.0f)))
	{
		currentMenu_ = MenuState::Main;
	}
	ImGui::PopStyleVar();
	ImGui::PopStyleColor(3);

	ImGui::End();
}

void TitleScene::DrawSettingsModal(float screenW, float screenH)
{
	float modalW = 520.0f;
	float modalH = 360.0f;
	ImGui::SetNextWindowPos(ImVec2((screenW - modalW) * 0.5f, (screenH - modalH) * 0.5f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(modalW, modalH), ImGuiCond_Always);

	ImGui::Begin("SettingsModal", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);

	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 wPos = ImGui::GetWindowPos();
	ImVec2 wMax = ImVec2(wPos.x + modalW, wPos.y + modalH);

	dl->AddRectFilled(wPos, wMax, IM_COL32(16, 38, 58, 250), 12.0f);
	dl->AddRect(wPos, wMax, IM_COL32(80, 180, 255, 240), 12.0f, 0, 2.5f);

	ImGui::SetCursorPos(ImVec2(30.0f, 25.0f));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.85f, 1.0f, 1.0f));
	ImGui::SetWindowFontScale(1.3f);
	ImGui::Text((const char*)u8"⚙️ せってい // SETTINGS");
	ImGui::SetWindowFontScale(1.0f);
	ImGui::PopStyleColor();

	ImGui::Dummy(ImVec2(0.0f, 16.0f));

	static float masterVol = 0.8f;
	static float sfxVol = 0.9f;
	static bool showFps = true;

	ImGui::SliderFloat((const char*)u8"マスター音量", &masterVol, 0.0f, 1.0f, "%.2f");
	ImGui::SliderFloat((const char*)u8"効果音の大きさ", &sfxVol, 0.0f, 1.0f, "%.2f");
	ImGui::Checkbox((const char*)u8"FPS・パフォーマンス表示", &showFps);

	ImGui::Dummy(ImVec2(0.0f, 24.0f));
	ImGui::SetCursorPosX((modalW - 180.0f) * 0.5f);

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.45f, 0.72f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.55f, 0.85f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.10f, 0.35f, 0.58f, 1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
	if (ImGui::Button((const char*)u8"  もどる [ESC]  ", ImVec2(180.0f, 40.0f)))
	{
		currentMenu_ = MenuState::Main;
	}
	ImGui::PopStyleVar();
	ImGui::PopStyleColor(3);

	ImGui::End();
}
#endif


