#include"SRVManager.h"
#include <cassert>

const uint32_t SRVManager::kMaxSRVCount = 512;

void SRVManager::Initialize(DirectXCom* directXCom)
{
    directXCom_ = directXCom;

    // Use the DirectXCom's SRV descriptor heap instead of creating a separate heap
    descriptorHeap = directXCom_->GetSrvDescriptorHeap();

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
		GetCPUDescriptorHandle(srvIndex)
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
    // 上限チェック: 使用中のインデックスが最大数に達していないことを確認
    if (useIndex >= kMaxSRVCount)
    {
        // Debug-time assert to catch allocation logic errors
        assert(false && "SRVManager::Allocate - exceeded maximum SRV count");
        // Return invalid index to the caller so it can handle the error gracefully in release builds
        return UINT32_MAX;
    }

    uint32_t index = useIndex;
    ++useIndex;
    return index;
}
D3D12_CPU_DESCRIPTOR_HANDLE SRVManager::GetCPUDescriptorHandle(uint32_t index)
{
    D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    handleCPU.ptr += (descriptorSize_ * index);
    return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE SRVManager::GetGPUDescriptorHandle(uint32_t index)
{
    D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
    handleGPU.ptr += (descriptorSize_ * index);
    return handleGPU;
}