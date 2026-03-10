#pragma once
#include"BaseScene.h"

class DirectXCom; // forward

class SceneManager
{

public:
	SceneManager(DirectXCom* dxCommon = nullptr) : dxCommon_(dxCommon) {}
	~SceneManager();

	void Update();

	void Draw();

	void SetNextScene(BaseScene* nextScene)
	{
		nextScene_ = nextScene;
	}

	void SetDirectXCom(DirectXCom* dxCommon) { dxCommon_ = dxCommon; }

private:
	BaseScene* scene_ = nullptr;
	BaseScene* nextScene_ = nullptr;
	DirectXCom* dxCommon_ = nullptr;
};

