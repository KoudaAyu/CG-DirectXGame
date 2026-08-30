#include "ClearScene.h"
#include "KeyInput.h"
#include "DirectXCom.h"
#include "SceneManager.h"
#include "Baziru3_Engine/Graphics/Graphics/SceneRenderRequests.h"

#ifdef USE_IMGUI
#include <imgui.h>
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

        // SPACEキーでタイトルへ戻る
        if (input_->TriggerKey(DIK_SPACE))
        {
            SceneManager::GetInstance()->ChangeScene("TITLE");
        }
    }

#ifdef USE_IMGUI
    // 画面中央に大きく目立つクリアHUDを表示
    ImGui::SetNextWindowPos(ImVec2(360, 230), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(560, 240), ImGuiCond_Always);
    ImGui::Begin("CLEAR_HUD", nullptr,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);

    ImGui::Spacing();
    ImGui::SetWindowFontScale(1.6f);
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "=== [ CLEAR SCENE ] ===");
    ImGui::SetWindowFontScale(1.0f);

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "Congratulations! Game Cleared.");
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

void ClearScene::Draw(SceneRenderRequests& /*renderRequests*/)
{
    // 背景スカイボックスの描画
    if (dxCommon_ && dxCommon_->GetCommandList())
    {
        SceneManager::GetInstance()->DrawSkybox(dxCommon_->GetCommandList().Get());
    }
}
