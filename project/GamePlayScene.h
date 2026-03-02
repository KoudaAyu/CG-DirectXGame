#pragma once

#include"DirectXCom.h"
#include"Sound.h"
#include"Sphere.h"


class GamePlayScene
{
public:
	void Initialize(DirectXCom* dxCommon);

	void Finalize();

	void Update();

	void Draw();

private:

	DirectXCom* directXCom = nullptr;
	Sound* sound = nullptr;
	Sphere* sphere = nullptr;
};