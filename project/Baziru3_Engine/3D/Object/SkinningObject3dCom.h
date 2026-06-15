#pragma once
#include"Camera.h"
#include"DirectXCom.h"
#include <d3d12.h>
#include <ostream>
#include"Log.h"
#include "Object3d.h"
#include "RenderContext.h"

class SkinningObject3dCom
{
public:
    // 参照メンバーのためのコンストラクタを宣言
    SkinningObject3dCom(std::ostream& logStream);
    ~SkinningObject3dCom() = default;

    void Initialize(DirectXCom* directXCom);
    void Update();

    void RootSignature();
    void Descriptor();
    void CreateRootParameters();
    void StaticSamplers();
    void SignatureBlob();
    void RootSignatureFromBlob();
    void InputLayer();
    void InitializeBlend();
    void RasterizerState();
    void ShaderCompile();
    void InitializeGraphicPipeline();
    void CreateGraphicsPipelineState();

    void PreDraw();

    void Draw(Object3d* object, const RenderContext& ctx, const Object3d::ModelData& modelData, bool drawObject);

public:
    void SetDefaultCamera(Camera* camera)
    {
        defaultCamera_ = camera;
    }
    Camera* GetDefaultCamera() const
    {
        return defaultCamera_;
    }
    DirectXCom* GetDirectXCom() const
    {
        return dxCommon;
    }
    const Microsoft::WRL::ComPtr<ID3D12PipelineState>& GetPipelineState() const
    {
        return pipelineState;
    }
    const Microsoft::WRL::ComPtr<ID3D12RootSignature>& GetRootSignature() const
    {
        return rootSignature;
    }

private:
    Camera* defaultCamera_ = nullptr;
    DirectXCom* dxCommon = nullptr;

private:
    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    D3D12_DESCRIPTOR_RANGE descriptorRange[2] = {};
    D3D12_ROOT_PARAMETER rootParameters[6] = {};
    D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr;
    D3D12_BLEND_DESC blendDesc{};
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[5] = {};
    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
    D3D12_RASTERIZER_DESC rasterizerDesc{};
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob;
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicPipelineStateDesc{};
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState = nullptr;

    std::ostream& logStream;
};
