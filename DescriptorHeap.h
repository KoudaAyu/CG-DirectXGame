#pragma once

#include<cassert>
#include<d3d12.h>
#include<wrl.h>

class DescriptorHeap
{
public:
	static Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>  CreateDescriptorHeap(
		const Microsoft::WRL::ComPtr<ID3D12Device>& device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);
};
