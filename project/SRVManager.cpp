#include"SRVManager.h"
#include <cassert>

const uint32_t SRVManager::kMaxSRVCount = 512;

void SRVManager::Initialize(DirectXCom* directXCom)
{
	directXCom_ = directXCom;

	//デスクリプタヒープの設定
	descriptirHeap = directXCom_->CreateDescriptorHeap(directXCom_->GetDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kMaxSRVCount, true);

	//デスクリプタサイズ1個分のサイズを取得して記録
	descriptorSize_ = directXCom_->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void SRVManager::CreateSRVforTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLeveles)
{
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
		GetSRVHandleCPU(srvIndex)
	);
}

void SRVManager::CreateSRVforStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride)
{
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
		GetSRVHandleCPU(srvIndex)
	);
}

void SRVManager::PreDraw()
{
	//SRVデスクリプタヒープのセット
	ID3D12DescriptorHeap* descriptorHeaps[] = { descriptirHeap.Get() };
	directXCom_->GetCommandList()->SetDescriptorHeaps(1, descriptorHeaps);
}

uint32_t SRVManager::Allocate()
{
	// 上限チェック: 使用中のインデックスが最大数に達していないことを確認
	assert(useIndex < kMaxSRVCount && "SRVManager::Allocate - exceeded maximum SRV count");

	int index = useIndex;
	useIndex++;
	return index;
}
D3D12_CPU_DESCRIPTOR_HANDLE SRVManager::GetSRVHandleCPU(uint32_t index)
{
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptirHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += (descriptorSize_ * index);
	return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE SRVManager::GetGPUDescriptorHandle(uint32_t index)
{
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptirHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += (descriptorSize_ * index);
	return handleGPU;
}