#include "EngineDefaultScene.h"
#include "SceneManager.h"
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

void EngineDefaultScene::InitializeScene() {
  timer_ = 0.0f;
}

void EngineDefaultScene::Finalize() {
}

void EngineDefaultScene::Update() {
  timer_ += 1.0f / 60.0f;

#ifdef USE_IMGUI
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
#endif
}

void EngineDefaultScene::Draw(SceneRenderRequests &renderRequests) {
  (void)renderRequests;
}
