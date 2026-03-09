#pragma once
#include"BaseScene.h"

class SceneManager
{
	void Update();

	void Draw();

	void SetNextScene(BaseScene* nextScene)
	{
		nextScene_ = nextScene;
	}

private:
	BaseScene* scene_ = nullptr;
	BaseScene* nextScene_ = nullptr;
};

