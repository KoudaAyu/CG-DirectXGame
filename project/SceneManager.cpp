#include "SceneManager.h"

SceneManager::~SceneManager()
{
	if (scene_)
	{
		scene_->Finalize();
		delete scene_;
		scene_ = nullptr;
	}
}

void SceneManager::Update()
{
	if ((nextScene_))
	{

		//旧シーンの終了処理
		if (scene_)
		{
			scene_->Finalize();
			delete scene_;
		}

		//シーン切り替え
		scene_ = nextScene_;
		nextScene_ = nullptr;

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