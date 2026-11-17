#include "EngineDefaultScene.h"
#include "SceneManager.h"
#ifdef USE_IMGUI
#include <imgui.h>
#endif

void EngineDefaultScene::InitializeScene() {
  timer_ = 0.0f;
}

void EngineDefaultScene::Finalize() {
}

void EngineDefaultScene::Update() {
  timer_ += 1.0f / 60.0f;

#ifdef USE_IMGUI
  if (ImGui::GetCurrentContext()) {
    ImGui::Begin("Engine Fallback Scene");
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "=== Baziru3 Engine Default Scene ===");
    ImGui::Text("Engine is running standalone (no application scene active).");
    ImGui::Text("Register game scenes in SceneRegistration::RegisterScenes().");

    if (sceneManager_) {
      auto scenes = sceneManager_->GetAvailableSceneNames();
      if (scenes.size() > 1) {
        ImGui::Separator();
        ImGui::Text("Switch Scene:");
        for (const auto &name : scenes) {
          if (name == "DEFAULT") {
            continue;
          }
          if (ImGui::Button(name.c_str())) {
            sceneManager_->ChangeScene(name);
          }
          ImGui::SameLine();
        }
        ImGui::NewLine();
      }
    }
    ImGui::End();
  }
#endif
}

void EngineDefaultScene::Draw(SceneRenderRequests &renderRequests) {
  (void)renderRequests;
}