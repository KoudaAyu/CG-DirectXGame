#pragma once
#pragma once

#include <d3d12.h>
#include <dxcapi.h>
#include <wrl.h>

#include <cstdint>
#include <ostream>

#include "DirectXCom.h"
#include "RenderTexture.h"
#include "TextureManager.h"
#include "Matrix4x4.h"

class OffScreenRendering
{
public:
    OffScreenRendering(std::ostream& logStream, DirectXCom* dxCommon);
    ~OffScreenRendering() = default;

    void Initialize(
        uint32_t width = 0,
        uint32_t height = 0,
        DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
        const Vector4& clearColor = { 0.1f, 0.25f, 0.5f, 1.0f });
    void Finalize();

    void Begin(ID3D12GraphicsCommandList* commandList);
    void End(ID3D12GraphicsCommandList* commandList);
    void SetMainRenderTarget(ID3D12GraphicsCommandList* commandList) const;
    void DrawToBackBuffer(ID3D12GraphicsCommandList* commandList);

    // 逆プロジェクション行列を設定する関数を追加
    void SetProjectionInverse(const Matrix4x4& projectionInverse);

    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU() const;
    ID3D12Resource* GetTextureResource() const;

private:
    void CreateRootSignature();
    void ShaderCompile();
    void InitializePipelineState();
    void CreateRenderTargets();
    void TransitionToRenderTarget(ID3D12GraphicsCommandList* commandList);
    void TransitionToPixelShaderResource(ID3D12GraphicsCommandList* commandList);
    void TransitionResource(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES nextState);

private:
    std::ostream& logStream_;
    DirectXCom* dxCommon_ = nullptr;

    uint32_t width_ = 0;
    uint32_t height_ = 0;
    DXGI_FORMAT format_ = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    Vector4 clearColor_ = { 0.1f, 0.25f, 0.5f, 1.0f };
    uint32_t srvIndex_ = TextureManager::kInvalidTextureIndex;
    uint32_t depthSrvIndex_ = TextureManager::kInvalidTextureIndex; // 深度用SRVのインデックスを追加
    D3D12_RESOURCE_STATES currentState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;

    RenderTexture renderTexture_;
    Microsoft::WRL::ComPtr<ID3D12Resource> offScreenTexture_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_ = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_{};
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_{};
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU_{};
    D3D12_GPU_DESCRIPTOR_HANDLE depthSrvHandleGPU_{}; // 深度用SRVのGPUハンドルを追加

    // 逆プロジェクション行列送信用定数バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> inverseProjectionBuffer_ = nullptr;
    void* inverseProjectionMap_ = nullptr;

    D3D12_DESCRIPTOR_RANGE descriptorRange_[2] = {}; // 範囲を2つに増やす（Color, Depth）
    D3D12_ROOT_PARAMETER rootParameters_[3] = {};    // パラメータを3つに増やす（Color, Depth, CBuffer）
    D3D12_STATIC_SAMPLER_DESC staticSamplers_[2] = {}; // サンプラーを2つに増やす（Linear, Point）
    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob_ = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob_ = nullptr;
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_ = nullptr;
};
