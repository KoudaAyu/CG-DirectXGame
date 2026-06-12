#include "SceneManager.h"
#include "SceneFactory.h"
#include <fstream>

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
	std::ofstream log("debug_scene_log.txt", std::ios::app);
	log << "ChangeScene called with: " << sceneName << std::endl;
	if (nextScene_) log << "  ignored because nextScene_ is not null" << std::endl;
	if (isSceneTransitioning_) log << "  ignored because isSceneTransitioning_ is true" << std::endl;

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
		log << "  failed to create scene from factory" << std::endl;
		Logger::Log(logStream_, "SceneManager::ChangeScene() failed to create scene.\n");
		return;
	}

	log << "  successfully created scene, setting nextScene_" << std::endl;
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
    if (!nextScene_ && !isSceneTransitioning_) return;

	std::ofstream log("debug_scene_log.txt", std::ios::app);

	// 1. フェードイン待ち（シーン切り替えが完了し、フェードインの最中）の場合
	if (isSceneTransitioning_ && hasSwitchedSceneDuringFade_)
	{
		if (fadeApplication_ && !fadeApplication_->IsBusy())
		{
			log << "  Fade in finished. Completing transition." << std::endl;
			isSceneTransitioning_ = false;
			hasSwitchedSceneDuringFade_ = false;
		}
		return;
	}

	// 2. 新しいシーン切り替えの開始、またはフェードアウト待ち
	log << "ApplyPendingSceneChange: nextScene_ is set" << std::endl;
	log << "  scene_: " << (scene_ ? "not null" : "null") << std::endl;
	log << "  fadeApplication_: " << (fadeApplication_ ? "not null" : "null") << std::endl;
	if (fadeApplication_) {
		log << "  fade IsAvailable: " << (fadeApplication_->IsAvailable() ? "true" : "false") << std::endl;
		log << "  fade IsBusy: " << (fadeApplication_->IsBusy() ? "true" : "false") << std::endl;
		log << "  fade IsFadeOutFinished: " << (fadeApplication_->IsFadeOutFinished() ? "true" : "false") << std::endl;
	}
	log << "  isSceneTransitioning_: " << (isSceneTransitioning_ ? "true" : "false") << std::endl;
	log << "  hasSwitchedSceneDuringFade_: " << (hasSwitchedSceneDuringFade_ ? "true" : "false") << std::endl;

   if (!scene_ || !fadeApplication_ || !fadeApplication_->IsAvailable())
    {
		log << "  Directly committing scene change (no fade or first scene)" << std::endl;
        CommitPendingSceneChange();
        isSceneTransitioning_ = false;
        hasSwitchedSceneDuringFade_ = false;
        return;
    }

    if (!isSceneTransitioning_)
    {
		log << "  Starting fade out" << std::endl;
        fadeApplication_->StartFadeOut();
        isSceneTransitioning_ = true;
        return;
    }

    if (!hasSwitchedSceneDuringFade_)
    {
        if (!fadeApplication_->IsFadeOutFinished())
        {
			log << "  Waiting for fade out to finish" << std::endl;
            return;
        }

		log << "  Fade out finished. Committing scene and starting fade in" << std::endl;
        CommitPendingSceneChange();
        hasSwitchedSceneDuringFade_ = true;
        fadeApplication_->StartFadeIn();
        return;
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