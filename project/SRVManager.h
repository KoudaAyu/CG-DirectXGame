#pragma once
#include"DirectXCom.h"
class SRVManager
{
public:
	void Initialize(DirectXCom* directXCom);

	//SRV作成
	void CreateSRVforTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLeveles);

	//SRV生成(Structured Buffer用)
	void CreateSRVforStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);

	void PreDraw();


	uint32_t Allocate();

public:
	void SeTGraphicsRootDescriptorTable(UINT RootParameterIndex, uint32_t srvIndex)
	{
		directXCom_->GetCommandList()->SetGraphicsRootDescriptorTable(RootParameterIndex, GetGPUDescriptorHandle(srvIndex));
	}

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index);

private:
	DirectXCom* directXCom_ = nullptr;

	//最大SRV数(最大テクスチャ枚数)
	static const uint32_t kMaxSRVCount;
	//SRV用のデスクリプタサイズ
	uint32_t descriptorSize_;
	//SRV用のデスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptirHeap;

	//次に使用するSRVインデックス
	// index 0 is commonly reserved (e.g. for ImGui). Start at 1 to avoid overwriting reserved slot.
	uint32_t useIndex = 1;

	

	
};
