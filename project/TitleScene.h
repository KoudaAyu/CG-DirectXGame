#pragma once

#include "BaseScene.h"

class TitleScene : public BaseScene
{
public:
	void Initialize(DirectXCom* dxCommon) override;
	void Finalize() override;
	void Update() override;
	void Draw() override;
};

