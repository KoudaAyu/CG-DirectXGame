#pragma once

#include "BaseScene.h"

class DirectXCom;
class KeyInput;

class TitleScene : public BaseScene
{
public:
	void Initialize(DirectXCom* dxCommon) override;
	void Finalize() override;
	void Update() override;
	void Draw() override;

private:
	DirectXCom* dxCommon_ = nullptr;
	KeyInput* input_ = nullptr;
};

