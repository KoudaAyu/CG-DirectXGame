#pragma once

#include "BaseScene.h"

class DirectXCom;
class KeyInput;
class Camera;

class TitleScene : public BaseScene
{
public:
	void Initialize(DirectXCom* dxCommon, Camera* camera) override;
	void Finalize() override;
	void Update() override;
	void Draw() override;

private:
	DirectXCom* dxCommon_ = nullptr;
	KeyInput* input_ = nullptr;
};

