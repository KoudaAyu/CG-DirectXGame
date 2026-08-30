#include "TitleScene.h"
#include "SceneManager.h"
#include "KeyInput.h"
#include "DirectXCom.h"
#include "Baziru3_Engine/Graphics/Graphics/SceneRenderRequests.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

void TitleScene::InitializeScene()
{
    if (dxCommon_)
    {
        input_ = new KeyInput();
        input_->Initialize(dxCommon_->GetWindowAPI());
    }
}

void TitleScene::Finalize()
{
    delete input_;
    input_ = nullptr;
}

void TitleScene::Update()
{
    if (input_) input_->Update();

    // Space キーでプレイシーンへ遷移
    if (input_ && input_->TriggerKey(DIK_SPACE))
    {
        SceneManager::GetInstance()->ChangeScene("GAMEPLAY");
    }

#ifdef USE_IMGUI
    // 画面中央に大きく目立つタイトルHUDを表示
    ImGui::SetNextWindowPos(ImVec2(360, 230), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(560, 240), ImGuiCond_Always);
    ImGui::Begin("TITLE_HUD", nullptr,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);

    ImGui::Spacing();
    ImGui::SetWindowFontScale(1.6f);
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "=== [ TITLE SCENE ] ===");
    ImGui::SetWindowFontScale(1.0f);

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "Baziru3 Game Engine - Master Template");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.4f, 1.0f), ">> Press SPACE key to START GAMEPLAY <<");
    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - 200.0f) * 0.5f + ImGui::GetCursorPosX());
    if (ImGui::Button("START GAMEPLAY", ImVec2(200.0f, 42.0f)))
    {
        SceneManager::GetInstance()->ChangeScene("GAMEPLAY");
    }

    ImGui::End();
#endif
}

void TitleScene::Draw(SceneRenderRequests& /*renderRequests*/)
{
    // 背景スカイボックスの描画
    if (dxCommon_ && dxCommon_->GetCommandList())
    {
        SceneManager::GetInstance()->DrawSkybox(dxCommon_->GetCommandList().Get());
    }
}
