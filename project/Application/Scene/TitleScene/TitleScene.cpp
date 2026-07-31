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

    // Space キーでもデモシーンへ遷移（キーボード派向け）
    if (input_ && input_->TriggerKey(DIK_SPACE))
    {
        SceneManager::GetInstance()->ChangeScene("GAMEPLAY");
    }

#ifdef USE_IMGUI
    ImGui::SetNextWindowPos(ImVec2(320, 240), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(360, 160), ImGuiCond_Once);
    ImGui::Begin("GE3_Game - Engine Base", nullptr,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);

    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Engine Feature Demo");
    ImGui::Spacing();
    ImGui::Text("This branch is for engine foundation development.");
    ImGui::Text("Press SPACE or click below to open the demo scene.");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - 160.0f) * 0.5f + ImGui::GetCursorPosX());
    if (ImGui::Button("Open Demo Scene", ImVec2(160.0f, 40.0f)))
    {
        SceneManager::GetInstance()->ChangeScene("GAMEPLAY");
    }

    ImGui::End();
#endif
}

void TitleScene::Draw(SceneRenderRequests& /*renderRequests*/)
{
    // タイトルシーンは ImGui のみ描画（3D/スプライトなし）
}
