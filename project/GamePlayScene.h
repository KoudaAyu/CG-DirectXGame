#pragma once

#include "BaseScene.h"
#include"DirectXCom.h"
#include"Sound.h"
#include"Sphere.h"


class GamePlayScene : public BaseScene
{
public:
	
	void Initialize(DirectXCom* dxCommon) override;

	void Finalize() override;

	void Update() override;

	void Draw() override;

private:

	DirectXCom* directXCom = nullptr;
	Sound* sound = nullptr;
	Sphere* sphere = nullptr;
};