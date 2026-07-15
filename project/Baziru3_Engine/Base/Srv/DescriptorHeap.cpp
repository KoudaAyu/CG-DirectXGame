#include "DescriptorHeap.h"
#include <cassert>
#include <sstream>
#include <Windows.h>
#include <algorithm>

void DescriptorHeap::Initialize(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t maxDescriptors, bool shaderVisible)
{
	assert(device);
	type_ = type;
	maxDescriptors_ = maxDescriptors;
	shaderVisible_ = shaderVisible;

	descriptorSize_ = device->GetDescriptorHandleIncrementSize(type_);

	D3D12_DESCRIPTOR_HEAP_DESC desc{};
	desc.Type = type_;
	desc.NumDescriptors = maxDescriptors_;
	desc.Flags = shaderVisible_ ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap_));
	assert(SUCCEEDED(hr));

	allocatedFlags_.assign(maxDescriptors_, 0);
	freeIndices_.clear();

	for (uint32_t i = maxDescriptors_ - 1; ; --i)
	{
		freeIndices_.push_back(i);
		if (i == 0) break;
	}
}

void DescriptorHeap::Finalize()
{
	heap_.Reset();
	freeIndices_.clear();
	allocatedFlags_.clear();
}

uint32_t DescriptorHeap::Allocate()
{
	if (freeIndices_.empty())
	{
		assert(false && "DescriptorHeap::Allocate - no free descriptors");
		return UINT32_MAX;
	}

	uint32_t index = freeIndices_.back();
	freeIndices_.pop_back();

	allocatedFlags_[index] = 1;
	return index;
}

uint32_t DescriptorHeap::AllocateAt(uint32_t index)
{
	if (index >= maxDescriptors_)
	{
		assert(false && "DescriptorHeap::AllocateAt - invalid index");
		return UINT32_MAX;
	}

	if (allocatedFlags_[index])
	{
		assert(false && "DescriptorHeap::AllocateAt - index already allocated");
		return UINT32_MAX;
	}

	auto it = std::find(freeIndices_.begin(), freeIndices_.end(), index);
	if (it != freeIndices_.end())
	{
		freeIndices_.erase(it);
	}

	allocatedFlags_[index] = 1;
	return index;
}

void DescriptorHeap::Free(uint32_t index)
{
	if (index >= maxDescriptors_)
	{
		OutputDebugStringA("DescriptorHeap::Free - ERROR: invalid index\n");
		return;
	}

	// 0, 1, 2番目は予約領域のため解放をスキップする
	if (index < 3)
	{
		return;
	}

	if (!allocatedFlags_[index])
	{
		OutputDebugStringA("DescriptorHeap::Free - WARNING: index already free\n");
		return;
	}

	allocatedFlags_[index] = 0;
	freeIndices_.push_back(index);
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::GetCPUDescriptorHandle(uint32_t index) const
{
	assert(index < maxDescriptors_);
	D3D12_CPU_DESCRIPTOR_HANDLE handle = heap_->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += static_cast<SIZE_T>(index) * descriptorSize_;
	return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::GetGPUDescriptorHandle(uint32_t index) const
{
	assert(index < maxDescriptors_);
	assert(shaderVisible_ && "Cannot get GPU handle for non-shader-visible heap");
	D3D12_GPU_DESCRIPTOR_HANDLE handle = heap_->GetGPUDescriptorHandleForHeapStart();
	handle.ptr += static_cast<SIZE_T>(index) * descriptorSize_;
	return handle;
}
