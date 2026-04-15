#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <cstdint>	
#include "Transform.h"

class RenderTexture
{
public:
	RenderTexture() = default;
	~RenderTexture() = default;

	Microsoft::WRL::ComPtr<ID3D12Resource> CreateRenderTargetTexture(const Microsoft::WRL::ComPtr<ID3D12Device>& device, 
		uint32_t width, uint32_t height, DXGI_FORMAT format,const Vector4& clearColor);
};
