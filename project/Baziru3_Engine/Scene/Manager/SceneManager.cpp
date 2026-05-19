#include "SceneManager.h"
#include "SceneFactory.h"

#include "Baziru3_Engine\Graphics\SceneRenderRequests.h"
#include "FadeApplication.h"
#include "SkyBox.h"
#include "SkyboxCom.h"
#include "TextureManager.h"
// ParticleManager is used for engine-level particle updates/draws
#include "Baziru3_Engine\Particle\ParticleManager.h"

#include <cassert>
#include<memory>
#include <Log.h>

namespace {
    static std::unique_ptr<SceneManager>& SceneManagerStorage()
    {
        static std::unique_ptr<SceneManager> instance;
        return instance;
    }
}

SceneManager::~SceneManager()
{
	if (scene_)
	{
		scene_->Finalize();
		scene_.reset();
	}
}

void SceneManager::ChangeScene(const std::string& sceneName)
{

	if (!sceneFactory_)
	{
		sceneFactory_.reset(new SceneFactory());
	}

  if (nextScene_ || isSceneTransitioning_)
    {
        Logger::Log(logStream_, "SceneManager::ChangeScene() ignored because a transition is already in progress.\n");
        return;
    }

	
	auto newScene = sceneFactory_->CreateScene(sceneName);
   if (!newScene)
    {
        Logger::Log(logStream_, "SceneManager::ChangeScene() failed to create scene.\n");
        return;
    }

	nextScene_ = std::move(newScene);
}

SceneManager* SceneManager::GetInstance()
{
    auto& instance = SceneManagerStorage();
    if (!instance)
    {
        instance = std::make_unique<SceneManager>();
    }
    return instance.get();
}

void SceneManager::Destroy()
{
    SceneManagerStorage().reset();
}

void SceneManager::Initialize(DirectXCom* dxCommon)
{
	dxCommon_ = dxCommon;
}

void SceneManager::Update(float deltaTime)
{
	if (!dxCommon_)
	{
        Logger::Log(logStream_, "SceneManager::Update() called before DirectXCom is set.");
		return;
	}


    ApplyPendingSceneChange();

    //実行中のシーンを更新
    if (scene_)
    {
        scene_->Update();
    }

    // Engine-level subsystems: update particle manager so that scenes may
    // add particle definitions but the engine performs simulation.
    if (particleManager_)
    {
        particleManager_->Update(deltaTime);
    }

    ApplyPendingSceneChange();
}

void SceneManager::ApplyPendingSceneChange()
{
    if (!nextScene_) return;

   if (!scene_ || !fadeApplication_ || !fadeApplication_->IsAvailable())
    {
        CommitPendingSceneChange();
        isSceneTransitioning_ = false;
        hasSwitchedSceneDuringFade_ = false;
        return;
    }

    if (!isSceneTransitioning_)
    {
        fadeApplication_->StartFadeOut();
        isSceneTransitioning_ = true;
        return;
    }

    if (!hasSwitchedSceneDuringFade_)
    {
        if (!fadeApplication_->IsFadeOutFinished())
        {
            return;
        }

        CommitPendingSceneChange();
        hasSwitchedSceneDuringFade_ = true;
        fadeApplication_->StartFadeIn();
        return;
    }

    if (!fadeApplication_->IsBusy())
    {
        isSceneTransitioning_ = false;
        hasSwitchedSceneDuringFade_ = false;
    }
}

void SceneManager::CommitPendingSceneChange()
{
    if (!nextScene_)
    {
        return;
    }

    //旧シーンの終了処理
    if (scene_)
    {
        scene_->Finalize();
        scene_.reset();
    }

    //シーン切り替え
    scene_ = std::move(nextScene_);

    scene_->SetSceneManager(this);

    //次シーンの初期化
    scene_->Initialize(dxCommon_, camera_);
}

void SceneManager::Draw(SceneRenderRequests& renderRequests)
{
    //実行中のシーンを描画
    if (scene_)
    {
        // Clear flag before calling into scene. Scene will populate renderRequests
        // if it produced any draw work.
        renderRequests.sceneDrawn = false;
        scene_->Draw(renderRequests);

        if (!renderRequests.spheres.GetRequestedSpheres().empty())
        {
            renderRequests.sceneDrawn = true;
        }
    }
}

void SceneManager::DrawSkybox(ID3D12GraphicsCommandList* commandList) const
{
	if (!commandList || !skybox_ || !skyboxCom_ || skyboxTextureIndex_ == TextureManager::kInvalidTextureIndex)
	{
		return;
	}

	skyboxCom_->SetupDraw(commandList);
	skybox_->Draw(commandList, TextureManager::GetInstance()->GetSrvHandleGPU(skyboxTextureIndex_));
}