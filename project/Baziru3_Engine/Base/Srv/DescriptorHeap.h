#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <string>

class DescriptorHeap
{
public:
	DescriptorHeap() = default;
	~DescriptorHeap() = default;

	void Initialize(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t maxDescriptors, bool shaderVisible);
	void Finalize();

	uint32_t Allocate();
	uint32_t AllocateAt(uint32_t index);
	void Free(uint32_t index);

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index) const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index) const;

	ID3D12DescriptorHeap* GetHeap() const { return heap_.Get(); }
	uint32_t GetDescriptorSize() const { return descriptorSize_; }
	uint32_t GetMaxDescriptors() const { return maxDescriptors_; }

private:
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap_;
	D3D12_DESCRIPTOR_HEAP_TYPE type_;
	uint32_t descriptorSize_ = 0;
	uint32_t maxDescriptors_ = 0;
	bool shaderVisible_ = false;

	std::vector<uint32_t> freeIndices_;
	std::vector<char> allocatedFlags_;
};
