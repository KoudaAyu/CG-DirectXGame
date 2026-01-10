#include "SrvManager.h"
#include"DirectXCom.h"
#include<cassert>

const uint32_t SrvManager::kMaxSRVCount = 512;

/// <summary>
/// 初期化
/// </summary>
void SrvManager::Initialize(DirectXCom* directXCom)
{
    directXCom_ = directXCom;

    // Use the shared SRV heap from DirectXCom to avoid heap mismatch
    descriptorHeap_ = directXCom_->GetSrvDescriptorHeap();
    descriptorSize_ = directXCom_->GetDevice()->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}
/// <summary>
/// 確保関数
/// </summary>
/// <returns></returns>
uint32_t SrvManager::Allocate()
{
    assert(useIndex_ < kMaxSRVCount);

    //return する番号をいったん確保
    int index = useIndex_;
    //次回のために番号を進める
    useIndex_++;
    //上で記録した番号をreturn
    return index;
}

D3D12_CPU_DESCRIPTOR_HANDLE SrvManager::GetCPUDescriptorHandle(uint32_t index)
{
    D3D12_CPU_DESCRIPTOR_HANDLE handleCPU =
        descriptorHeap_->GetCPUDescriptorHandleForHeapStart();

    handleCPU.ptr += (descriptorSize_ * index);
    return handleCPU;
}
D3D12_GPU_DESCRIPTOR_HANDLE SrvManager::GetGPUDescriptorHandle(uint32_t index)
{
    D3D12_GPU_DESCRIPTOR_HANDLE handleGPU =
        descriptorHeap_->GetGPUDescriptorHandleForHeapStart();

    handleGPU.ptr += (descriptorSize_ * index);
    return handleGPU;
}

/// <summary>
/// SRV生成(2Dテクスチャ用)
/// </summary>
/// <param name="srvIndex"></param>
/// <param name="pResource"></param>
/// <param name="Format"></param>
/// <param name="MipLevels"></param>
void SrvManager::CreateSRVforTexture2D(uint32_t srvIndex, ID3D12Resource* pResource,
    DXGI_FORMAT Format, UINT MipLevels)
{
    assert(pResource);
    assert(srvIndex < kMaxSRVCount);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = MipLevels; // 1 or 実Mip数
    srvDesc.Texture2D.PlaneSlice = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    D3D12_CPU_DESCRIPTOR_HANDLE handle =
        descriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(srvIndex) * descriptorSize_;

    directXCom_->GetDevice()->CreateShaderResourceView(pResource, &srvDesc, handle);
}
/// <summary>
/// SRV生成(Structure Buffer用)
/// </summary>
/// <param name="srvIndex"></param>
/// <param name="pResource"></param>
/// <param name="numElements"></param>
/// <param name="structureByteStride"></param>
void SrvManager::CreateSRVforStructureBuffer(uint32_t srvIndex,
    ID3D12Resource* pResource,
    UINT numElements,
    UINT structureByteStride)
{
    assert(pResource);
    assert(srvIndex < kMaxSRVCount);
    assert(numElements > 0);
    assert(structureByteStride > 0);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN; // 構造化は UNKNOWN
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = numElements;
    srvDesc.Buffer.StructureByteStride = structureByteStride;
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    D3D12_CPU_DESCRIPTOR_HANDLE handle =
        descriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(srvIndex) * descriptorSize_;

    directXCom_->GetDevice()->CreateShaderResourceView(pResource, &srvDesc, handle);
}

/// <summary>
/// 描画
/// </summary>
void SrvManager::PreDraw()
{
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap[] = {
        descriptorHeap_.Get() };
    directXCom_->GetCommandList()->SetDescriptorHeaps(1, descriptorHeap->GetAddressOf());
}

void SrvManager::SetGraphicsRootDescriptorTable(UINT RootParameterIndex,
    uint32_t srvIndex)
{
    directXCom_->GetCommandList()->SetGraphicsRootDescriptorTable(
        RootParameterIndex, GetGPUDescriptorHandle(srvIndex));
}