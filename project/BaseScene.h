#pragma once

#include"DirectXCom.h"

class SceneManager;

class BaseScene
{
public:
	virtual ~BaseScene() = default;

	virtual void Initialize(DirectXCom* dxCommon) = 0;
	virtual void Finalize() = 0;
	virtual void Update() = 0;
	virtual void Draw() = 0;

	virtual void SetSceneManager(SceneManager* sceneManager)
	{
		sceneManager_ = sceneManager;
	}

protected:
	SceneManager* sceneManager_ = nullptr;
};

