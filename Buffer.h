#pragma once

#include<Windows.h>
#include<cassert>
#include <d3d12.h> 
#include<wrl.h>

class Buffer
{
public:

	static Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(const Microsoft::WRL::ComPtr<ID3D12Device>& device,
		size_t sizeInBytes);
};
