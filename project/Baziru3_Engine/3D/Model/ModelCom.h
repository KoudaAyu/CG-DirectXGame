#pragma once

#include"DirectXCom.h"

class ModelCom
{
public:
	void Initialize(DirectXCom* dxCommon);

public:
	DirectXCom* GetDxCommon() const { return dxCommon_; }

private:
	DirectXCom* dxCommon_ = nullptr;
};