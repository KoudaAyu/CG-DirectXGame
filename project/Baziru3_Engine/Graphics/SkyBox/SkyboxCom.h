#pragma once

#include <d3d12.h>
#include <ostream>
#include "DirectXCom.h"

class SkyboxCom
{
public:
    SkyboxCom(std::ostream& logStream, DirectXCom* dxCommon);
    ~SkyboxCom();

    void Initialize();

    void RootSignature();
    void Descriptor();
    void CreateRootParameters();
    void StaticSamplers();
    void SignatureBlob();
    void RootSignatureFromBlob();
    void InputLayer();
    void ShaderCompile();
    void InitializeGraphicPipeline();
    void CreateGraphicsPipeline();
    void DepthStencilDesc();
    void RasterizerState();

    void SetupDraw(ID3D12GraphicsCommandList* commandList);

    const Microsoft::WRL::ComPtr<ID3D12RootSignature>& GetRootSignature() const { return rootSignature; }
    const Microsoft::WRL::ComPtr<ID3D12PipelineState>& GetPipelineState() const { return pipelineState; }

private:
    DirectXCom* dxCommon = nullptr;
    std::ostream& logStream;

    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
    D3D12_ROOT_PARAMETER rootParameters[2] = {};
    D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr;

    D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
    D3D12_RASTERIZER_DESC rasterizerDesc{};
    D3D12_BLEND_DESC blendDesc{};

    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob;
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicPipelineStateDesc{};
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState = nullptr;
};
