#pragma once
#include <d3d12.h>
class BlendManager
{
public:
	void CreateBlend();

	const D3D12_BLEND_DESC& GetBlendDesc() const
	{
		return blendDesc;
	}

private:
	//BlendStateの設定
	D3D12_BLEND_DESC blendDesc{};
};
