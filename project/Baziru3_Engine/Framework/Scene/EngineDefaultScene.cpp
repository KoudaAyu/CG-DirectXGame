#include "EngineDefaultScene.h"
#include "SceneManager.h"
#include "externals/imgui/imgui.h"

void EngineDefaultScene::InitializeScene() {
  timer_ = 0.0f;
}

void EngineDefaultScene::Finalize() {
}

void EngineDefaultScene::Update() {
  timer_ += 1.0f / 60.0f;

  // ImGui デバッグUI
  ImGui::Begin("Engine Fallback Scene");
  ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "=== Baziru3 Engine Default Scene ===");
  ImGui::Text("No active application scene loaded or unknown scene requested.");
  ImGui::Separator();
  ImGui::Text("Switch Scene:");
  if (ImGui::Button("TITLE")) {
    if (sceneManager_) sceneManager_->ChangeScene("TITLE");
  }
  ImGui::SameLine();
  if (ImGui::Button("GAMEPLAY")) {
    if (sceneManager_) sceneManager_->ChangeScene("GAMEPLAY");
  }
  ImGui::SameLine();
  if (ImGui::Button("CLEAR")) {
    if (sceneManager_) sceneManager_->ChangeScene("CLEAR");
  }
  ImGui::SameLine();
  if (ImGui::Button("GAMEOVER")) {
    if (sceneManager_) sceneManager_->ChangeScene("GAMEOVER");
  }
  ImGui::End();
}

void EngineDefaultScene::Draw(SceneRenderRequests &renderRequests) {
  (void)renderRequests;
}
