#pragma once

#include"DirectXCom.h"

class BaseScene
{
public:
	virtual ~BaseScene() = default;

	virtual void Initialize(DirectXCom* dxCommon) = 0;
	virtual void Finalize() = 0;
	virtual void Update() = 0;
	virtual void Draw() = 0;

private:
};

