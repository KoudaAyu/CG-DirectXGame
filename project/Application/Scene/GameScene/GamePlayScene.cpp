#include "GamePlayScene.h"
#include "SceneManager.h"
#include "DirectXCom.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

void GamePlayScene::InitializeScene()
{
    // 1. 入力システムの初期化
    if (dxCommon_ && dxCommon_->GetWindowAPI())
    {
        keyInput_ = std::make_unique<KeyInput>();
        keyInput_->Initialize(dxCommon_->GetWindowAPI());
    }

    // 2. カメラの初期化
    camera_ = std::make_unique<Camera>();
    camera_->Initialize(dxCommon_);
    camera_->SetTranslate({ 0.0f, 5.0f, -10.0f });
    camera_->SetRotate({ 0.35f, 0.0f, 0.0f });
    camera_->Update();
}

void GamePlayScene::Finalize()
{
    camera_.reset();
    keyInput_.reset();
}

void GamePlayScene::Update()
{
    if (keyInput_)
    {
        keyInput_->Update();

        // SPACEキーでクリアシーンへ遷移
        if (keyInput_->TriggerKey(DIK_SPACE))
        {
            SceneManager::GetInstance()->ChangeScene("CLEAR");
        }
    }

    if (camera_)
    {
        camera_->Update();
    }

#ifdef USE_IMGUI
    // 画面上部に目立つゲームプレイHUDを表示
    ImGui::SetNextWindowPos(ImVec2(360, 40), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(560, 210), ImGuiCond_Always);
    ImGui::Begin("GAMEPLAY_HUD", nullptr,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);

    ImGui::Spacing();
    ImGui::SetWindowFontScale(1.6f);
    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "=== [ GAMEPLAY SCENE ] ===");
    ImGui::SetWindowFontScale(1.0f);

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "Game logic canvas is ready. Implement your gameplay here.");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.4f, 1.0f), ">> Press SPACE key to CLEAR scene <<");
    ImGui::Spacing();

    if (ImGui::Button("Go To CLEAR Scene", ImVec2(165, 34)))
    {
        SceneManager::GetInstance()->ChangeScene("CLEAR");
    }
    ImGui::SameLine();
    if (ImGui::Button("Go To GAMEOVER Scene", ImVec2(165, 34)))
    {
        SceneManager::GetInstance()->ChangeScene("GAMEOVER");
    }
    ImGui::SameLine();
    if (ImGui::Button("Back To TITLE", ImVec2(165, 34)))
    {
        SceneManager::GetInstance()->ChangeScene("TITLE");
    }

    ImGui::End();
#endif
}

void GamePlayScene::Draw(SceneRenderRequests& renderRequests)
{
    renderRequests.sceneDrawn = true;

    // 背景スカイボックスの描画
    if (dxCommon_ && dxCommon_->GetCommandList())
    {
        SceneManager::GetInstance()->DrawSkybox(dxCommon_->GetCommandList().Get());
    }
}
