#pragma once
#include"DirectXCom.h"

#include<vector>

class SRVManager
{
public:
	SRVManager() = default;
	void Initialize(DirectXCom* directXCom);

	//SRV作成
	void CreateSRVForTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLeveles);

	//SRV生成(Structured Buffer用)
	void CreateSRVForStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);

	void PreDraw();

	uint32_t Allocate();

	void Free(uint32_t index);

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

	//最大SRV数(最大テクスチャ枚数)
	static const uint32_t kMaxSRVCount;
	//SRV用のデスクリプタサイズ
	uint32_t descriptorSize_;
	//SRV用のデスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;

	//次に使用するSRVインデックス
	uint32_t useIndex = 2;

	std::vector<uint32_t> freeIndices_;
	std::vector<char> allocatedFlags_; // インデックスの使用状況を管理するマップ
};
