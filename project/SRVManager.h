#pragma once
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

class DirectXCom;

class SrvManager
{
private:
    // DirectX
    DirectXCom* directXCom_ = nullptr;

public:
    // 最大SRV数(最大テクスチャ数)
    static const uint32_t kMaxSRVCount;
    // SRV用のデスクリプタサイズ
    uint32_t descriptorSize_;
    // SRV用デスクリプタヒープ
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap_;

    // 次に使用するSRVインデックス
    uint32_t useIndex_ = 0;

public:
    /// <summary>
    /// 初期化
    /// </summary>
    void Initialize(DirectXCom* directXCom);
    /// <summary>
    /// 確保関数
    /// </summary>
    /// <returns></returns>
    uint32_t Allocate();

    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index);

    /// <summary>
    /// SRV生成(2Dテクスチャ用)
    /// </summary>
    /// <param name="srvIndex"></param>
    /// <param name="pResource"></param>
    /// <param name="Format"></param>
    /// <param name="MipLevels"></param>
    void CreateSRVforTexture2D(uint32_t srvIndex, ID3D12Resource* pResource,
        DXGI_FORMAT Format, UINT MipLevels);
    /// <summary>
    /// SRV生成(Structure Buffer用)
    /// </summary>
    /// <param name="srvIndex"></param>
    /// <param name="pResource"></param>
    /// <param name="numElements"></param>
    /// <param name="structureByteStride"></param>
    void CreateSRVforStructureBuffer(uint32_t srvIndex, ID3D12Resource* pResource,
        UINT numElements, UINT structureByteStride);

    /// <summary>
    /// 描画
    /// </summary>
    void PreDraw();

    void SetGraphicsRootDescriptorTable(UINT RootParameterIndex,
        uint32_t srvIndex);

    bool CanAllocate() const { return useIndex_ < kMaxSRVCount; }
};