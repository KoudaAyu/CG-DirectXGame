#pragma once

#include "BaseScene.h"
#include "KeyInput.h"
#include "DirectXCom.h"
#include "SceneManager.h"
#include "Baziru3_Engine/Graphics/Graphics/SceneRenderRequests.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

class GameOverScene : public BaseScene
{
public:
	void InitializeScene() override
	{
		if (dxCommon_)
		{
			input_ = new KeyInput();
			input_->Initialize(dxCommon_->GetWindowAPI());
		}
	}

	void Finalize() override
	{
		delete input_;
		input_ = nullptr;
	}

	void Update() override
	{
		if (input_)
		{
			input_->Update();
			if (input_->TriggerKey(DIK_SPACE))
			{
				SceneManager::GetInstance()->ChangeScene("TITLE");
			}
		}

#ifdef USE_IMGUI
		// 画面中央に大きく目立つゲームオーバーHUDを表示
		ImGui::SetNextWindowPos(ImVec2(360, 230), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(560, 240), ImGuiCond_Always);
		ImGui::Begin("GAMEOVER_HUD", nullptr,
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);

		ImGui::Spacing();
		ImGui::SetWindowFontScale(1.6f);
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "=== [ GAMEOVER SCENE ] ===");
		ImGui::SetWindowFontScale(1.0f);

		ImGui::Spacing();
		ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "Better luck next time!");
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.4f, 1.0f), ">> Press SPACE key to return to TITLE <<");
		ImGui::Spacing();
		ImGui::Spacing();

		ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - 200.0f) * 0.5f + ImGui::GetCursorPosX());
		if (ImGui::Button("Back to TITLE", ImVec2(200.0f, 42.0f)))
		{
			SceneManager::GetInstance()->ChangeScene("TITLE");
		}

		ImGui::End();
#endif
	}

	void Draw(SceneRenderRequests& /*renderRequests*/) override
	{
		// 背景スカイボックスの描画
		if (dxCommon_ && dxCommon_->GetCommandList())
		{
			SceneManager::GetInstance()->DrawSkybox(dxCommon_->GetCommandList().Get());
		}
	}

	const char* GetSceneType() const { return "GAMEOVER"; }

private:
	KeyInput* input_ = nullptr;
};
