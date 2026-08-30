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
#include <Log.h>
#include <cassert>
#include <memory>

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

  auto newScene = sceneFactory_->CreateScene(sceneName);
  if (!newScene) {
    std::string errMsg = "[SceneManager ERROR] ChangeScene failed to create scene: " + sceneName + "\n";
    OutputDebugStringA(errMsg.c_str());
    Logger::Log(logStream_, errMsg);
    return;
  }

  std::string msg = "[SceneManager] ChangeScene requested: " + sceneName + "\n";
  OutputDebugStringA(msg.c_str());
  nextScene_ = std::move(newScene);
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

  AsyncLoader::GetInstance()->Update();

  ApplyPendingSceneChange();

  // 実行中のシーンを更新
  if (scene_) {
    scene_->Update();
  }

  // Engine-level subsystems: update particle manager so that scenes may
  // add particle definitions but the engine performs simulation.
  if (particleManager_) {
    particleManager_->Update(deltaTime);
  }

  ApplyPendingSceneChange();
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