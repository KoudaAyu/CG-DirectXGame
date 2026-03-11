#include"SRVManager.h"
#include <cassert>

const uint32_t SRVManager::kMaxSRVCount = 512;

void SRVManager::Initialize(DirectXCom* directXCom)
{
	assert(directXCom);

	directXCom_ = directXCom;

	descriptorHeap = directXCom_->GetSrvDescriptorHeap();

	assert(descriptorHeap.Get());

	//デスクリプタサイズ1個分のサイズを取得して記録
	descriptorSize_ = directXCom_->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	allocatedFlags_.assign(kMaxSRVCount, 0);
	for (uint32_t i = kMaxSRVCount; i-- > 2;)
	{
		freeIndices_.push_back(i);
	}
}

D3D12_CPU_DESCRIPTOR_HANDLE SRVManager::GetCPUDescriptorHandle(uint32_t index)
{
	return directXCom_->GetSRVHandleCPU(index);
}

D3D12_GPU_DESCRIPTOR_HANDLE SRVManager::GetGPUDescriptorHandle(uint32_t index)
{
	return directXCom_->GetSRVHandleGPU(index);
}

void SRVManager::CreateSRVForTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLeveles)
{
	assert(srvIndex < kMaxSRVCount);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = MipLeveles;
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	directXCom_->GetDevice()->CreateShaderResourceView(
		pResource,
		&srvDesc,
		GetCPUDescriptorHandle(srvIndex)
	);
}

void SRVManager::CreateSRVForStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride)
{
	assert(srvIndex < kMaxSRVCount);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = numElements;
	srvDesc.Buffer.StructureByteStride = structureByteStride;
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	directXCom_->GetDevice()->CreateShaderResourceView(
		pResource,
		&srvDesc,
		GetCPUDescriptorHandle(srvIndex)
	);
}

void SRVManager::PreDraw()
{
	//SRVデスクリプタヒープのセット
	ID3D12DescriptorHeap* descriptorHeaps[] = { descriptorHeap.Get() };
	directXCom_->GetCommandList()->SetDescriptorHeaps(1, descriptorHeaps);
}

uint32_t SRVManager::Allocate()
{
	if (freeIndices_.empty())
	{
		assert(false && "SRVManager::Allocate - no free SRV indices");
		return UINT32_MAX; // エラーコードとして最大値を返す
	}

	uint32_t index = freeIndices_.back();
	freeIndices_.pop_back();

	//安全の溜めにフラグをセットする
	allocatedFlags_[index] = 1;

	return index;
}

void SRVManager::Free(uint32_t index)
{
	//0,1は予約済みなら解放不可
	if (index < 2 || index >= kMaxSRVCount)
	{
		assert(false && "SRVManager::Free - invalid index");
		return;
	}

	//二重解放防止
	if (!allocatedFlags_[index])
	{
		assert(false && "SRVManager::Free - index already free");
		return;
	}

	allocatedFlags_[index] = 0;
	freeIndices_.push_back(index);
}
