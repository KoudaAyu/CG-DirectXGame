#include "SceneManager.h"
#include "SceneFactory.h"

SceneManager* SceneManager::instance = nullptr;

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
	
	BaseScene* newScene = sceneFactory_->CreateScene(sceneName);
	nextScene_.reset(newScene);
}

SceneManager* SceneManager::GetInstance()
{
	if (!instance)
	{
		instance = new SceneManager();
	}
	return instance;
}

void SceneManager::Destroy()
{
	delete instance;
	instance = nullptr;
}

void SceneManager::Update()
{
	if ((nextScene_))
	{

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
		scene_->Initialize(dxCommon_);
	}


	//実行中のシーンを更新
	if (scene_)
	{
		scene_->Update();
	}
}

void SceneManager::Draw()
{
	//実行中のシーンを描画
	if (scene_)
	{
		scene_->Draw();
	}
}