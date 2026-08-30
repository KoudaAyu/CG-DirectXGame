#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <array>
#include "Matrix4x4.h"
#include "Vector.h"
#include "Camera.h"
#include "DirectXCom.h"


namespace BaziruEngine::Graphics {

/**
 * @brief カスケードシャドウマップ (Cascading Shadow Maps - CSM) 管理クラス
 *
 * 視錐台を 3 段階（近景・中景・遠景）に分割し、距離に応じた最高品質の影マップを多層生成します。
 */
class CascadingShadowMap {
public:
    static constexpr uint32_t kCascadeCount = 3;
    static constexpr uint32_t kShadowMapResolution = 2048;

    struct Cascade {
        Matrix4x4 viewMatrix;
        Matrix4x4 projMatrix;
        Matrix4x4 viewProjMatrix;
        float splitDistance = 0.0f;
    };

    struct alignas(256) ShadowParamForGPU {
        Matrix4x4 shadowViewProj[kCascadeCount];
        float cascadeSplits[4]; // 4バイトアライメント補正
        Vector3 lightDirection;
        float shadowBias;
    };

public:
    CascadingShadowMap() = default;
    ~CascadingShadowMap() = default;

    void Initialize(DirectXCom* dxCommon);
    void Update(const Camera& mainCamera, const Vector3& lightDirection);

    void BeginRender(ID3D12GraphicsCommandList* commandList, uint32_t cascadeIndex);
    void EndRender(ID3D12GraphicsCommandList* commandList, uint32_t cascadeIndex);

    ID3D12Resource* GetShadowMapResource(uint32_t cascadeIndex) const { return shadowMaps_[cascadeIndex].Get(); }
    D3D12_GPU_DESCRIPTOR_HANDLE GetShadowMapSrvGpuHandle(uint32_t cascadeIndex) const { return srvGpuHandles_[cascadeIndex]; }
    D3D12_GPU_VIRTUAL_ADDRESS GetCbvGpuAddress() const { return shadowParamBuffer_->GetGPUVirtualAddress(); }

    const std::array<Cascade, kCascadeCount>& GetCascades() const { return cascades_; }

private:
    void CreateShadowMapResources();
    void CreateDescriptorHeaps();

private:
    DirectXCom* dxCommon_ = nullptr;

    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kCascadeCount> shadowMaps_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap_;

    std::array<D3D12_CPU_DESCRIPTOR_HANDLE, kCascadeCount> dsvCpuHandles_;
    std::array<D3D12_CPU_DESCRIPTOR_HANDLE, kCascadeCount> srvCpuHandles_;
    std::array<D3D12_GPU_DESCRIPTOR_HANDLE, kCascadeCount> srvGpuHandles_;

    Microsoft::WRL::ComPtr<ID3D12Resource> shadowParamBuffer_;
    ShadowParamForGPU* shadowParamData_ = nullptr;

    std::array<Cascade, kCascadeCount> cascades_;
    std::array<float, kCascadeCount> splitRatios_ = { 0.1f, 0.35f, 1.0f }; // 視錐台3分割レシオ
};

} // namespace BaziruEngine::Graphics
