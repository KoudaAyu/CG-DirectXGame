#include "SceneManager.h"
#include "SceneFactory.h"

#include "../Fade.h"
#include "Baziru3_Engine\Graphics\Graphics\SceneRenderRequests.h"
#include "SkyBox.h"
#include "SkyboxCom.h"
#include "TextureManager.h"
// ParticleManager is used for engine-level particle updates/draws
#include "Baziru3_Engine\Framework\Particle\ParticleManager.h"

#include "AsyncLoader.h"
#include "Baziru3_Engine/Framework/Collision/CollisionManager.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3d.h"
#include "Baziru3_Engine/Graphics/Shapes/Sphere/Sphere.h"
#include "ModelManager.h"
#include <cassert>
#include <memory>
#include "externals/imgui/imgui.h"

namespace {
static std::unique_ptr<SceneManager> &SceneManagerStorage() {
  static std::unique_ptr<SceneManager> instance;
  return instance;
}
} // namespace

SceneManager::~SceneManager() {
  if (scene_) {
    scene_->Finalize();
    scene_.reset();
  }
  AsyncLoader::GetInstance()->Finalize();
  AsyncLoader::Destroy();
  ModelManager::Destroy();
}

void SceneManager::ChangeScene(const std::string &sceneName) {
  if (!sceneFactory_) {
    sceneFactory_ = std::make_unique<SceneFactory>();
  }

  pendingSceneName_ = sceneName;
  std::string msg = "[SceneManager] ChangeScene requested: " + sceneName + "\n";
  OutputDebugStringA(msg.c_str());

  if (fadeApplication_ && fadeApplication_->IsAvailable()) {
    transitionState_ = TransitionState::FadeOut;
    transitionTimer_ = 0.0f;
    int frames = (std::max)(1, static_cast<int>(fadeDuration_ * 60.0f));
    fadeApplication_->StartFadeOut(frames);
  } else {
    // フェードが無い場合は即時切り替え
    auto newScene = sceneFactory_->CreateScene(sceneName);
    if (newScene) {
      nextScene_ = std::move(newScene);
      CommitPendingSceneChange();
      currentSceneName_ = sceneName;
    }
    transitionState_ = TransitionState::None;
  }
}

std::vector<std::string> SceneManager::GetAvailableSceneNames() const {
  if (sceneFactory_) {
    return sceneFactory_->GetRegisteredSceneNames();
  }
  return { "DEFAULT" };
}

void SceneManager::DrawSceneSelectorUI() {
  ImGui::Begin("Engine Scene Manager");
  ImGui::TextColored(ImVec4(0.2f, 0.85f, 1.0f, 1.0f), "Current Scene: %s", currentSceneName_.c_str());
  
  const char* stateStr = "Idle";
  if (transitionState_ == TransitionState::FadeOut) stateStr = "Fading Out...";
  else if (transitionState_ == TransitionState::Switching) stateStr = "Switching...";
  else if (transitionState_ == TransitionState::FadeIn) stateStr = "Fading In...";
  ImGui::Text("Transition: %s", stateStr);

  if (ImGui::Button("Restart Scene (F5)", ImVec2(160, 0))) {
    RestartCurrentScene();
  }
  ImGui::SameLine();
  ImGui::SliderFloat("Fade Duration", &fadeDuration_, 0.05f, 1.5f, "%.2fs");
  ImGui::Separator();

  ImGui::Text("Switch Scene:");
  std::vector<std::string> scenes = GetAvailableSceneNames();
  for (const auto& name : scenes) {
    bool isCurrent = (name == currentSceneName_);
    if (isCurrent) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.3f, 1.0f));
    if (ImGui::Button(name.c_str(), ImVec2(100, 0))) {
      ChangeScene(name);
    }
    if (isCurrent) ImGui::PopStyleColor();
    ImGui::SameLine();
  }
  ImGui::NewLine();

  // シーン共有データ（Context）の表示
  if (!sceneContextData_.empty() || !sceneAnyData_.empty()) {
    if (ImGui::CollapsingHeader("Scene Context Data", ImGuiTreeNodeFlags_DefaultOpen)) {
      for (const auto& [k, v] : sceneContextData_) {
        ImGui::Text("  [%s] = %s", k.c_str(), v.c_str());
      }
      for (const auto& [k, v] : sceneAnyData_) {
        if (sceneContextData_.find(k) == sceneContextData_.end()) {
          ImGui::Text("  [%s] = (type: %s)", k.c_str(), v.type().name());
        }
      }
    }
  }

  ImGui::End();
}

SceneManager *SceneManager::GetInstance() {
  auto &instance = SceneManagerStorage();
  if (!instance) {
    instance = std::make_unique<SceneManager>();
  }
  return instance.get();
}

void SceneManager::Destroy() { SceneManagerStorage().reset(); }

void SceneManager::Initialize(DirectXCom *dxCommon) {
  dxCommon_ = dxCommon;
  ModelManager::GetInstance()->Initialize(dxCommon);
  AsyncLoader::GetInstance()->Initialize(
      ModelManager::GetInstance()->modelCom_.get());
}

void SceneManager::Update(float deltaTime) {
  for (Object3d *obj : Object3d::GetInstances()) {
    if (obj)
      obj->ResetFrameDrawFlags();
  }
  for (Sphere *s : Sphere::GetInstances()) {
    if (s)
      s->ResetFrameDrawFlags();
  }

  if (!dxCommon_) {
    Logger::Log(logStream_,
                "SceneManager::Update() called before DirectXCom is set.");
    return;
  }

  // F5 キーによる開発用即時リスタート
  if ((GetAsyncKeyState(VK_F5) & 0x0001) && transitionState_ == TransitionState::None) {
    RestartCurrentScene();
  }

  // フェードの更新
  if (fadeApplication_) {
    fadeApplication_->Update();
  }

  // シーン遷移ステートマシンの更新
  if (transitionState_ == TransitionState::FadeOut) {
    transitionTimer_ += deltaTime;
    if ((fadeApplication_ && fadeApplication_->IsFadeOutFinished()) || transitionTimer_ >= fadeDuration_) {
      transitionState_ = TransitionState::Switching;
    }
  }

  if (transitionState_ == TransitionState::Switching) {
    if (!sceneFactory_) {
      sceneFactory_ = std::make_unique<SceneFactory>();
    }
    auto newScene = sceneFactory_->CreateScene(pendingSceneName_);
    if (newScene) {
      nextScene_ = std::move(newScene);
      CommitPendingSceneChange();
      currentSceneName_ = pendingSceneName_;
    }

    transitionState_ = TransitionState::FadeIn;
    transitionTimer_ = 0.0f;
    if (fadeApplication_) {
      int frames = (std::max)(1, static_cast<int>(fadeDuration_ * 60.0f));
      fadeApplication_->StartFadeIn(frames);
    }
  } else if (transitionState_ == TransitionState::FadeIn) {
    transitionTimer_ += deltaTime;
    if ((fadeApplication_ && !fadeApplication_->IsBusy()) || transitionTimer_ >= fadeDuration_) {
      transitionState_ = TransitionState::None;
    }
  }

  // 実行中のシーンを更新
  if (scene_) {
    scene_->Update();
  }

  // ImGui デバッグUIの描画
  DrawSceneSelectorUI();

  // Engine-level subsystems: update particle manager so that scenes may
  // add particle definitions but the engine performs simulation.
  if (particleManager_) {
    particleManager_->Update(deltaTime);
  }
}

void SceneManager::ApplyPendingSceneChange() {
  if (!nextScene_)
    return;

  CommitPendingSceneChange();
  isSceneTransitioning_ = false;
  hasSwitchedSceneDuringFade_ = false;
}

void SceneManager::CommitPendingSceneChange() {
  if (!nextScene_) {
    return;
  }

  // 旧シーンの終了処理
  if (scene_) {
    scene_->Finalize();
    scene_.reset();
  }

  // シーン切り替え
  scene_ = std::move(nextScene_);

  scene_->SetSceneManager(this);

  // 次シーンの初期化
  scene_->Initialize(dxCommon_, camera_);
}

void SceneManager::Draw(SceneRenderRequests &renderRequests) {
  // 実行中のシーンを描画
  if (scene_) {
    // Clear flag before calling into scene. Scene will populate renderRequests
    // if it produced any draw work.
    renderRequests.sceneDrawn = false;
    scene_->Draw(renderRequests);

    if (!renderRequests.spheres.GetRequestedSpheres().empty()) {
      renderRequests.sceneDrawn = true;
    }
  }
}

void SceneManager::DrawSkybox(ID3D12GraphicsCommandList *commandList) const {
  if (!showSkybox_ || !commandList || !skybox_ || !skyboxCom_ ||
      skyboxTextureIndex_ == TextureManager::kInvalidTextureIndex) {
    return;
  }

  skyboxCom_->SetupDraw(commandList);
  skybox_->Draw(commandList, TextureManager::GetInstance()->GetSrvHandleGPU(
                                 skyboxTextureIndex_));
}