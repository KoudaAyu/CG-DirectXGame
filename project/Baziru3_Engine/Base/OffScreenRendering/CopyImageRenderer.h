#pragma once

#include <d3d12.h>
#include <dxcapi.h>
#include <wrl.h>
#include <ostream>

class DirectXCom;
struct ID3D12GraphicsCommandList;

class CopyImageRenderer
{
public:
    CopyImageRenderer(std::ostream& logStream, DirectXCom* dxCommon);
    ~CopyImageRenderer() = default;

    void Initialize();
    void SetupDraw(ID3D12GraphicsCommandList* commandList);
    void Draw(ID3D12GraphicsCommandList* commandList, D3D12_GPU_DESCRIPTOR_HANDLE textureHandle);

private:
    void RootSignature();
    void Descriptor();
    void CreateRootParameters();
    void StaticSamplers();
    void SignatureBlob();
    void RootSignatureFromBlob();
    void ShaderCompile();
    void InitializeGraphicPipeline();
    void RasterizerState();
    void InitializeBlend();

private:
    DirectXCom* dxCommon = nullptr;
    std::ostream& logStream;

    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
    D3D12_ROOT_PARAMETER rootParameters[1] = {};
    D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr;

    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
    D3D12_RASTERIZER_DESC rasterizerDesc{};
    D3D12_BLEND_DESC blendDesc{};
    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = nullptr;
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = nullptr;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicPipelineStateDesc{};
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState = nullptr;
};
