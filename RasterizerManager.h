#pragma once
#include <d3d12.h>
class RasterizerManager
{
public:
	void RasterizerSetting();

	const D3D12_RASTERIZER_DESC& GetRasterizerDesc() const
	{
		return rasterizerDesc;
	}

private:
	//RasterizerStateの設定
	D3D12_RASTERIZER_DESC rasterizerDesc{};
};
