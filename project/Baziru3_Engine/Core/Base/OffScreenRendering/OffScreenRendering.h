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
    enum class PostEffect {
        Normal,               // CopyImage.PS.hlsl
        DepthBasedOutline,    // DepthBasedOutline.PS.hlsl
        LuminanceBaseOutline, // LuminanceBaseOutline.PS.hlsl
        RadialBlur,           // RadialBlur.PS.hlsl
        GaussianFilter,       // GaussianFilter.PS.hlsl
        BoxFilter,            // BoxFilter.PS.hlsl
        Count
    };

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

    /// <summary>
    /// 水面屈折・水深透過用に、不透明パス描画時点のシーンカラーと深度バッファをスナップショット退避する
    /// </summary>
    void CaptureSceneSnapshot(ID3D12GraphicsCommandList* commandList);

    D3D12_GPU_DESCRIPTOR_HANDLE GetRefractionColorSrvGPU() const { return refractionColorSrvHandleGPU_; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetRefractionDepthSrvGPU() const { return refractionDepthSrvHandleGPU_; }

    // 逆プロジェクション行列を設定する関数
    void SetProjectionInverse(const Matrix4x4& projectionInverse);

    void SetRadialBlurCenter(const Vector2& center);
    void SetRadialBlurWidth(float width);
    const Vector2& GetRadialBlurCenter() const { return radialBlurData_.center; }
    float GetRadialBlurWidth() const { return radialBlurData_.blurWidth; }

    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU() const;
    ID3D12Resource* GetTextureResource() const;

    PostEffect GetPostEffect() const { return currentEffect_; }
    void SetPostEffect(PostEffect effect) { currentEffect_ = effect; }

    D3D12_CPU_DESCRIPTOR_HANDLE GetRtvHandle() const { return rtvHandle_; }
    D3D12_CPU_DESCRIPTOR_HANDLE GetDsvHandle() const { return dsvHandle_; }

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
    uint32_t depthSrvIndex_ = TextureManager::kInvalidTextureIndex;
    uint32_t refractionColorSrvIndex_ = TextureManager::kInvalidTextureIndex;
    uint32_t refractionDepthSrvIndex_ = TextureManager::kInvalidTextureIndex;
    D3D12_RESOURCE_STATES currentState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;
    D3D12_RESOURCE_STATES refractionColorState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;
    D3D12_RESOURCE_STATES refractionDepthState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;

    RenderTexture renderTexture_;
    Microsoft::WRL::ComPtr<ID3D12Resource> offScreenTexture_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> refractionColorTexture_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> refractionDepthTexture_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_ = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_{};
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_{};
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU_{};
    D3D12_GPU_DESCRIPTOR_HANDLE depthSrvHandleGPU_{};
    D3D12_GPU_DESCRIPTOR_HANDLE refractionColorSrvHandleGPU_{};
    D3D12_GPU_DESCRIPTOR_HANDLE refractionDepthSrvHandleGPU_{};

    // 逆プロジェクション行列送信用定数バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> inverseProjectionBuffer_ = nullptr;
    void* inverseProjectionMap_ = nullptr;

    struct RadialBlurData {
        Vector2 center = { 0.5f, 0.5f };
        float blurWidth = 0.01f;
        float padding = 0.0f;
    };
    Microsoft::WRL::ComPtr<ID3D12Resource> radialBlurBuffer_ = nullptr;
    void* radialBlurMap_ = nullptr;
    RadialBlurData radialBlurData_;

    D3D12_DESCRIPTOR_RANGE descriptorRange_[2] = {};
    D3D12_ROOT_PARAMETER rootParameters_[4] = {};
    D3D12_STATIC_SAMPLER_DESC staticSamplers_[2] = {};
    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob_ = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob_ = nullptr;
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlobs_[static_cast<size_t>(PostEffect::Count)] = {};
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStates_[static_cast<size_t>(PostEffect::Count)] = {};
    PostEffect currentEffect_ = PostEffect::Normal;
};
