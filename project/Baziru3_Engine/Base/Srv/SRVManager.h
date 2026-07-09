#pragma once
#include"DirectXCom.h"

#include<vector>

class SRVManager
{
public:
	SRVManager() = default;
	void Initialize(DirectXCom* directXCom);


	static constexpr uint32_t kMaxSRVCount = 512;

	//SRV作成
	void CreateSRVForTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLeveles);
	void CreateSRVForTexture2D(uint32_t index, ID3D12Resource* resource, const DirectX::TexMetadata& meta);

	//SRV生成(Structured Buffer用)
	void CreateSRVForStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);

	void PreDraw();

	uint32_t Allocate();

	// Allocate a specific index (useful for binding t0/t1 fixed slots)
	uint32_t AllocateAt(uint32_t index);

	void Free(uint32_t index);
	DirectXCom* GetDirectXCom() const { return directXCom_; }

public:
	void SeTGraphicsRootDescriptorTable(UINT RootParameterIndex, uint32_t srvIndex)
	{
		directXCom_->GetCommandList()->SetGraphicsRootDescriptorTable(RootParameterIndex, GetGPUDescriptorHandle(srvIndex));
	}

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index);

private:
	SRVManager(const SRVManager&) = delete;
	SRVManager& operator=(const SRVManager&) = delete;
	DirectXCom* directXCom_ = nullptr;
};
