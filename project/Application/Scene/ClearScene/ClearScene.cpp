#include "ClearScene.h"
#include "KeyInput.h"
#include "DirectXCom.h"
#include "SceneManager.h"
#include "TextureManager.h"
#include "../GameScene/RaidStats.h"
#include <cmath>

#ifdef USE_IMGUI
#include "imgui.h"
#endif

void ClearScene::InitializeScene()
{
	if (dxCommon_)
	{
		input_ = new KeyInput();
		input_->Initialize(dxCommon_->GetWindowAPI());
	}
}

void ClearScene::Finalize()
{
	delete input_;
	input_ = nullptr;
}

void ClearScene::Update()
{
	if (input_)
	{
		input_->Update();
	}

	bool redeployTriggered = false;
	bool returnTitle = false;

	// [SPACE] または [ENTER] キーで再出撃 (GAMEPLAYへ)
	if (input_ && (input_->TriggerKey(DIK_SPACE) || input_->TriggerKey(DIK_RETURN)))
	{
		redeployTriggered = true;
	}

	// [T] キーでタイトル画面へ
	if (input_ && input_->TriggerKey(DIK_T))
	{
		returnTitle = true;
	}

#ifdef USE_IMGUI
	ImGuiIO& io = ImGui::GetIO();
	ImVec2 screenSize = io.DisplaySize;

	// --- メインのクリア画面パネル (幅 720px / 高さ 520px) ---
	float panelW = 720.0f;
	float panelH = 520.0f;
	ImGui::SetNextWindowPos(ImVec2((screenSize.x - panelW) * 0.5f, (screenSize.y - panelH) * 0.45f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(panelW, panelH), ImGuiCond_Always);

	ImGui::Begin("ClearScreen", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground);

	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 winPos = ImGui::GetWindowPos();
	ImVec2 winMax = ImVec2(winPos.x + panelW, winPos.y + panelH);

	// 背景パネル (タクティカルミリタリーダークグリーン + エメラルドグリーン二重枠)
	dl->AddRectFilled(winPos, winMax, IM_COL32(10, 22, 16, 248), 12.0f);
	dl->AddRect(winPos, winMax, IM_COL32(0, 255, 140, 255), 12.0f, 0, 3.0f);
	dl->AddRect(ImVec2(winPos.x + 3.0f, winPos.y + 3.0f), ImVec2(winMax.x - 3.0f, winMax.y - 3.0f), IM_COL32(240, 200, 40, 200), 10.0f, 0, 1.5f);

	// 勝利ヘッダー
	ImGui::SetCursorPos(ImVec2(40.0f, 25.0f));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 1.0f, 0.55f, 1.0f));
	ImGui::SetWindowFontScale(1.8f);
	ImGui::Text((const char*)u8"  🏆  SURVIVED  //  生 還 成 功");
	ImGui::SetWindowFontScale(1.0f);
	ImGui::PopStyleColor();

	ImGui::SetCursorPos(ImVec2(45.0f, 65.0f));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.25f, 1.0f));
	ImGui::Text((const char*)u8"=== [ DUCKOV EXTRACTION DEBRIEFING REPORT ] ===");
	ImGui::PopStyleColor();

	// 区切り線
	dl->AddLine(ImVec2(winPos.x + 40.0f, winPos.y + 92.0f), ImVec2(winMax.x - 40.0f, winPos.y + 92.0f), IM_COL32(0, 255, 140, 180), 1.5f);

	// レイド戦績の取得
	auto& stats = RaidStats::GetInstance();
	int rMin = static_cast<int>(stats.raidTime) / 60;
	int rSec = static_cast<int>(stats.raidTime) % 60;

	// --- 📊 戦績グリッドパネル (幅 640px / 高さ 230px) ---
	float gridX = winPos.x + 40.0f;
	float gridY = winPos.y + 110.0f;
	float gridW = panelW - 80.0f;
	float gridH = 250.0f;
	dl->AddRectFilled(ImVec2(gridX, gridY), ImVec2(gridX + gridW, gridY + gridH), IM_COL32(14, 30, 22, 230), 8.0f);
	dl->AddRect(ImVec2(gridX, gridY), ImVec2(gridX + gridW, gridY + gridH), IM_COL32(40, 90, 65, 180), 8.0f, 0, 1.0f);

	char buf[256];
	float rowY = gridY + 16.0f;
	float rowStep = 34.0f;

	// 1. レイド生還時間
	sprintf_s(buf, "⏱️  RAID DURATION       :  %02d:%02d  (STATUS: EXTRACTED ON HELIPAD)", rMin, rSec);
	dl->AddText(ImVec2(gridX + 24.0f, rowY), IM_COL32(230, 245, 235, 255), buf);
	rowY += rowStep;

	// 2. 標的破壊数
	sprintf_s(buf, "🎯  TARGETS DESTROYED   :  %d / %d  [100%% OBJECTIVE COMPLETED]", stats.targetsDestroyed, stats.totalTargets);
	dl->AddText(ImVec2(gridX + 24.0f, rowY), IM_COL32(0, 255, 160, 255), buf);
	rowY += rowStep;

	// 3. 敵撃破数
	sprintf_s(buf, "💀  HOSTILES ELIMINATED :  %d KILLS (SCAV DUCK FORCES)", stats.enemiesKilled);
	dl->AddText(ImVec2(gridX + 24.0f, rowY), IM_COL32(255, 130, 130, 255), buf);
	rowY += rowStep;

	// 4. 射撃精度
	sprintf_s(buf, "🔫  ACCURACY            :  %.1f%%  (%d HITS / %d SHOTS)", stats.GetAccuracy(), stats.shotsHit, stats.shotsFired);
	dl->AddText(ImVec2(gridX + 24.0f, rowY), IM_COL32(100, 220, 255, 255), buf);
	rowY += rowStep;

	// 5. 救急キット使用
	sprintf_s(buf, "🩹  FIRST AID USED      :  %d MEDKITS CONSUMED", stats.medkitsUsed);
	dl->AddText(ImVec2(gridX + 24.0f, rowY), IM_COL32(200, 225, 215, 240), buf);
	rowY += rowStep;

	// 6. 持ち帰り物資総額 (大強調)
	sprintf_s(buf, "💰  EXTRACTED LOOT VALUE:  +$%d ROUBLES (SECURED IN STASH)", stats.totalLootValue);
	dl->AddText(ImVec2(gridX + 24.0f, rowY), IM_COL32(255, 220, 40, 255), buf);

	// --- 🖼️ 回収物資イラストギャラリー (4つのアイテムアイコンを並べて表示) ---
	uint32_t iconDuck = TextureManager::GetInstance()->Load("Resources/item_gold_duck.jpg");
	uint32_t iconMed = TextureManager::GetInstance()->Load("Resources/item_medkit.jpg");
	uint32_t iconAmmo = TextureManager::GetInstance()->Load("Resources/item_ammo.jpg");
	uint32_t iconCash = TextureManager::GetInstance()->Load("Resources/item_roubles.jpg");

	float galY = winPos.y + 375.0f;
	float galX = winPos.x + 40.0f;
	float iSize = 48.0f;
	float iSpacing = 16.0f;

	uint32_t icons[4] = { iconDuck, iconMed, iconAmmo, iconCash };
	const char* iconLabels[4] = { "GOLD DUCK", "IFAK MEDKIT", "AP AMMO", "ROUBLES" };
	ImU32 iconBorders[4] = { IM_COL32(255, 215, 60, 255), IM_COL32(0, 255, 140, 255), IM_COL32(0, 200, 255, 255), IM_COL32(100, 255, 200, 255) };

	for (int i = 0; i < 4; ++i)
	{
		float curX = galX + i * (iSize + 110.0f);
		uint64_t gpu = TextureManager::GetInstance()->GetSrvHandleGPU(icons[i]).ptr;
		if (gpu != 0)
		{
			dl->AddImageRounded((ImTextureID)gpu, ImVec2(curX, galY), ImVec2(curX + iSize, galY + iSize), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), 6.0f);
			dl->AddRect(ImVec2(curX, galY), ImVec2(curX + iSize, galY + iSize), iconBorders[i], 6.0f, 0, 1.5f);
		}
		dl->AddText(ImVec2(curX + iSize + 6.0f, galY + 14.0f), iconBorders[i], iconLabels[i]);
	}

	// --- アクションボタン ---
	static float blinkTimer = 0.0f;
	blinkTimer += io.DeltaTime * 3.5f;
	float alpha = 0.65f + 0.35f * std::sin(blinkTimer);

	ImGui::SetCursorPos(ImVec2(40.0f, 440.0f));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.65f, 0.35f, alpha));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.85f, 0.45f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.05f, 0.45f, 0.25f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

	if (ImGui::Button((const char*)u8"   [ SPACE ]  RE-DEPLOY INTO RAID (再出撃)   ", ImVec2(390.0f, 55.0f)))
	{
		redeployTriggered = true;
	}
	ImGui::PopStyleColor(4);

	ImGui::SetCursorPos(ImVec2(450.0f, 440.0f));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.25f, 0.3f, 0.9f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.4f, 0.5f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.2f, 0.25f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.95f, 1.0f));

	if (ImGui::Button((const char*)u8" [ T ] RETURN TO BASE ", ImVec2(230.0f, 55.0f)))
	{
		returnTitle = true;
	}
	ImGui::PopStyleColor(4);

	ImGui::End();
#endif

	if (redeployTriggered)
	{
		SceneManager::GetInstance()->ChangeScene("GAMEPLAY");
	}
	else if (returnTitle)
	{
		SceneManager::GetInstance()->ChangeScene("TITLE");
	}
}

void ClearScene::Draw(SceneRenderRequests& /*renderRequests*/)
{
}
