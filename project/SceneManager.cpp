#include "SceneManager.h"
#include "SceneFactory.h"

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

	assert(nextScene_ == nullptr);

	
	auto newScene = sceneFactory_->CreateScene(sceneName);
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

void SceneManager::Update()
{
	if (!dxCommon_)
	{
        Logger::Log(logStream_, "SceneManager::Update() called before DirectXCom is set.");
		return;
	}

    // Apply any pending scene change first. This is separated so callers can
    // decide when to perform the actual swap (e.g. after a transition).
    ApplyPendingSceneChange();

    //実行中のシーンを更新
    if (scene_)
    {
        scene_->Update();
    }
}

void SceneManager::ApplyPendingSceneChange()
{
    if (!nextScene_) return;

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

void SceneManager::Draw()
{
	//実行中のシーンを描画
	if (scene_)
	{
		scene_->Draw();
	}
}