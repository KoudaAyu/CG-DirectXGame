#pragma once
#include"DirectXCom.h"

class ModelCom
{
public:
	void Initialize(DirectXCom* dxCommon);

	DirectXCom* GetDirectXCom() const
	{
		return dxCommon_;
	}

private:
	DirectXCom* dxCommon_ = nullptr;
};
