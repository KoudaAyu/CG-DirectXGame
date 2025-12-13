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

private:
	DirectXCom* directXCom_ = nullptr;

	//最大SRV数(最大テクスチャ枚数)
	static const uint32_t kMaxSRVCount;
	//SRV用のデスクリプタサイズ
	uint32_t descriptorSize_;
	//SRV用のデスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptirHeap;

	//次に使用するSRVインデックス
	uint32_t useIndex = 0;

	uint32_t Allocate();

	D3D12_CPU_DESCRIPTOR_HANDLE GetSRVHandleCPU(uint32_t index);
	D3D12_GPU_DESCRIPTOR_HANDLE GetSRVHandleGPU(uint32_t index);

	
};
