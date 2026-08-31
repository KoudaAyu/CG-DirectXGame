#include "GameOverScene.h"
#include "KeyInput.h"
#include "DirectXCom.h"
#include "SceneManager.h"
#include "../GameScene/RaidStats.h"
#include <cmath>

#ifdef USE_IMGUI
#include "imgui.h"
#endif

void GameOverScene::InitializeScene()
{
	if (dxCommon_)
	{
		input_ = new KeyInput();
		input_->Initialize(dxCommon_->GetWindowAPI());
	}
}

void GameOverScene::Finalize()
{
	delete input_;
	input_ = nullptr;
}

void GameOverScene::Update()
{
	if (input_)
	{
		input_->Update();
	}

	bool retryTriggered = false;
	bool titleTriggered = false;

	// [SPACE] または [ENTER] キーのトリガー判定で即座にリトライ (GAMEPLAYへ)
	if (input_ && (input_->TriggerKey(DIK_SPACE) || input_->TriggerKey(DIK_RETURN)))
	{
		retryTriggered = true;
	}

	// [T] キーのトリガー判定でタイトル画面へ
	if (input_ && input_->TriggerKey(DIK_T))
	{
		titleTriggered = true;
	}

#ifdef USE_IMGUI
	ImGuiIO& io = ImGui::GetIO();
	ImVec2 screenSize = io.DisplaySize;

	// --- メインのゲームオーバー画面パネル (幅 720px / 高さ 530px) ---
	float panelW = 720.0f;
	float panelH = 530.0f;
	ImGui::SetNextWindowPos(ImVec2((screenSize.x - panelW) * 0.5f, (screenSize.y - panelH) * 0.45f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(panelW, panelH), ImGuiCond_Always);

	ImGui::Begin("GameOverScreen", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground);

	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 winPos = ImGui::GetWindowPos();
	ImVec2 winMax = ImVec2(winPos.x + panelW, winPos.y + panelH);

	auto& stats = RaidStats::GetInstance();
	bool isMIA = stats.isMIA;

	// 背景パネル (ダーククリムゾン / MIA時はダークアンバー)
	ImU32 bgCol = isMIA ? IM_COL32(28, 18, 10, 250) : IM_COL32(24, 10, 12, 250);
	ImU32 borderCol = isMIA ? IM_COL32(255, 140, 30, 255) : IM_COL32(255, 40, 40, 255);
	ImU32 innerBorderCol = isMIA ? IM_COL32(255, 200, 40, 200) : IM_COL32(240, 60, 60, 200);

	dl->AddRectFilled(winPos, winMax, bgCol, 12.0f);
	dl->AddRect(winPos, winMax, borderCol, 12.0f, 0, 3.0f);
	dl->AddRect(ImVec2(winPos.x + 3.0f, winPos.y + 3.0f), ImVec2(winMax.x - 3.0f, winMax.y - 3.0f), innerBorderCol, 10.0f, 0, 1.5f);

	// 敗北ヘッダー
	ImGui::SetCursorPos(ImVec2(40.0f, 25.0f));
	ImVec4 headCol = isMIA ? ImVec4(1.0f, 0.65f, 0.1f, 1.0f) : ImVec4(1.0f, 0.25f, 0.25f, 1.0f);
	ImGui::PushStyleColor(ImGuiCol_Text, headCol);
	ImGui::SetWindowFontScale(1.8f);
	if (isMIA)
	{
		ImGui::Text((const char*)u8"  ⏱️  MISSING IN ACTION  //  作 戦 時 間 切 れ");
	}
	else
	{
		ImGui::Text((const char*)u8"  💀  KILLED IN ACTION  //  戦 死・作 戦 失 敗");
	}
	ImGui::SetWindowFontScale(1.0f);
	ImGui::PopStyleColor();

	ImGui::SetCursorPos(ImVec2(45.0f, 65.0f));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.25f, 1.0f));
	ImGui::Text((const char*)u8"=== [ CASUALTY & GEAR LOSS REPORT // SECTOR-04 ] ===");
	ImGui::PopStyleColor();

	// 区切り線
	dl->AddLine(ImVec2(winPos.x + 40.0f, winPos.y + 92.0f), ImVec2(winMax.x - 40.0f, winPos.y + 92.0f), borderCol, 1.5f);

	// レイド戦績の取得
	int rMin = static_cast<int>(stats.raidTime) / 60;
	int rSec = static_cast<int>(stats.raidTime) % 60;

	// --- 📊 損害レポートグリッド (幅 640px / 高さ 250px) ---
	float gridX = winPos.x + 40.0f;
	float gridY = winPos.y + 110.0f;
	float gridW = panelW - 80.0f;
	float gridH = 260.0f;
	dl->AddRectFilled(ImVec2(gridX, gridY), ImVec2(gridX + gridW, gridY + gridH), IM_COL32(32, 14, 16, 235), 8.0f);
	dl->AddRect(ImVec2(gridX, gridY), ImVec2(gridX + gridW, gridY + gridH), IM_COL32(100, 35, 40, 180), 8.0f, 0, 1.0f);

	char buf[256];
	float rowY = gridY + 16.0f;
	float rowStep = 34.0f;

	// 1. キラー / 要因
	sprintf_s(buf, "⚠️  KILLER / HAZARD    :  %s", stats.killerName.c_str());
	dl->AddText(ImVec2(gridX + 24.0f, rowY), IM_COL32(255, 230, 120, 255), buf);
	rowY += rowStep;

	// 2. 死因
	sprintf_s(buf, "💥  CAUSE OF CASUALTY  :  %s", stats.causeOfDeath.c_str());
	dl->AddText(ImVec2(gridX + 24.0f, rowY), IM_COL32(255, 120, 120, 255), buf);
	rowY += rowStep;

	// 3. 生存時間
	sprintf_s(buf, "⏱️  SURVIVED TIME      :  %02d:%02d  (STATUS: KIA / SIGNAL LOST)", rMin, rSec);
	dl->AddText(ImVec2(gridX + 24.0f, rowY), IM_COL32(230, 235, 240, 240), buf);
	rowY += rowStep;

	// 4. 撃破数 ＆ 標的
	sprintf_s(buf, "🎯  COMBAT SCORE       :  %d TARGETS DESTROYED  |  %d KILLS", stats.targetsDestroyed, stats.enemiesKilled);
	dl->AddText(ImVec2(gridX + 24.0f, rowY), IM_COL32(200, 220, 240, 240), buf);
	rowY += rowStep;

	// 5. 命中率
	sprintf_s(buf, "🔫  ACCURACY           :  %.1f%%  (%d HITS / %d SHOTS)", stats.GetAccuracy(), stats.shotsHit, stats.shotsFired);
	dl->AddText(ImVec2(gridX + 24.0f, rowY), IM_COL32(140, 210, 255, 240), buf);
	rowY += rowStep;

	// 6. ロスト物資
	sprintf_s(buf, "❌  LOOT & GEAR STATUS :  -$%d ROUBLES (ALL IN-RAID LOOT LOST)", stats.totalLootValue);
	dl->AddText(ImVec2(gridX + 24.0f, rowY), IM_COL32(255, 80, 80, 255), buf);

	// --- アクションボタン ---
	static float blinkTimer = 0.0f;
	blinkTimer += io.DeltaTime * 3.5f;
	float alpha = 0.65f + 0.35f * std::sin(blinkTimer);

	ImGui::SetCursorPos(ImVec2(40.0f, 395.0f));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.18f, 0.18f, alpha));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.28f, 0.28f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.55f, 0.1f, 0.1f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

	if (ImGui::Button((const char*)u8"   [ SPACE ]  RE-DEPLOY OPERATION (再出撃)   ", ImVec2(390.0f, 55.0f)))
	{
		retryTriggered = true;
	}
	ImGui::PopStyleColor(4);

	ImGui::SetCursorPos(ImVec2(450.0f, 395.0f));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.25f, 0.3f, 0.9f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.4f, 0.5f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.2f, 0.25f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.95f, 1.0f));

	if (ImGui::Button((const char*)u8" [ T ] RETURN TO BASE ", ImVec2(230.0f, 55.0f)))
	{
		titleTriggered = true;
	}
	ImGui::PopStyleColor(4);

	ImGui::End();
#endif

	if (retryTriggered)
	{
		SceneManager::GetInstance()->ChangeScene("GAMEPLAY");
	}
	else if (titleTriggered)
	{
		SceneManager::GetInstance()->ChangeScene("TITLE");
	}
}

void GameOverScene::Draw(SceneRenderRequests& /*renderRequests*/)
{
}
