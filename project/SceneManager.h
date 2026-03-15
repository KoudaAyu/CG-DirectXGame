#pragma once
#include "AbstractSceneFactory.h"
#include "BaseScene.h"
#include <memory>

class DirectXCom;

class SceneManager
{

public:
	SceneManager(DirectXCom* dxCommon = nullptr) : dxCommon_(dxCommon) {}
	~SceneManager();

	void Update();

	void Draw();

	/// <summary>
	/// 次のシーン予約
	/// </summary>
	/// <param name="sceneName">シーン名</param>
	void ChangeScene(const std::string& sceneName);

	void SetDirectXCom(DirectXCom* dxCommon) { dxCommon_ = dxCommon; }

	void SetSceneFactory(std::unique_ptr<AbstractSceneFactory> sceneFactory) { sceneFactory_ = std::move(sceneFactory); }

	static SceneManager* GetInstance();
	static void Destroy();

private:
	std::unique_ptr<AbstractSceneFactory> sceneFactory_;
	std::unique_ptr<BaseScene> scene_ = nullptr;
	std::unique_ptr<BaseScene> nextScene_ = nullptr;
	DirectXCom* dxCommon_ = nullptr;

	
	static std::unique_ptr<SceneManager> instance;
};

