#include "SkyboxCom.h"
#include "Log.h"
#include <cassert>

SkyboxCom::SkyboxCom(std::ostream& logStream, DirectXCom* dxCommon)
    : logStream(logStream), dxCommon(dxCommon)
{
}

SkyboxCom::~SkyboxCom()
{
}

void SkyboxCom::Initialize()
{
    CreateGraphicsPipeline();
}

void SkyboxCom::RootSignature()
{
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
}

void SkyboxCom::Descriptor()
{
    // Skybox uses a single cubemap SRV at t0
    descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange[0].NumDescriptors = 1;
    descriptorRange[0].BaseShaderRegister = 0; // t0
    descriptorRange[0].RegisterSpace = 0;
    descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
}

void SkyboxCom::CreateRootParameters()
{
    // b0: transformation matrix (vertex shader)
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[0].Descriptor.ShaderRegister = 0;

    // descriptor table for cubemap (t0)
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRange;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;

    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);
}

void SkyboxCom::StaticSamplers()
{
    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers[0].ShaderRegister = 0;
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    descriptionRootSignature.pStaticSamplers = staticSamplers;
    descriptionRootSignature.NumStaticSamplers = 1;
}

void SkyboxCom::SignatureBlob()
{
    HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature,
        D3D_ROOT_SIGNATURE_VERSION_1, signatureBlob.GetAddressOf(), errorBlob.GetAddressOf());
    dxCommon->SetHr(hr);
    if (FAILED(hr))
    {
        if (errorBlob)
        {
            Logger::Log(logStream, reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
        }
        assert(false);
    }
}

void SkyboxCom::RootSignatureFromBlob()
{
    dxCommon->SetHr(dxCommon->GetDevice()->CreateRootSignature(0,
        signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));
    assert(SUCCEEDED(dxCommon->GetHr()));
}

void SkyboxCom::InputLayer()
{
    inputElementDescs[0].SemanticName = "POSITION";
    inputElementDescs[0].SemanticIndex = 0;
    inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    inputElementDescs[1].SemanticName = "TEXCOORD";
    inputElementDescs[1].SemanticIndex = 0;
    inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    inputElementDescs[2].SemanticName = "NORMAL";
    inputElementDescs[2].SemanticIndex = 0;
    inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    inputLayoutDesc.pInputElementDescs = inputElementDescs;
    inputLayoutDesc.NumElements = _countof(inputElementDescs);
}

void SkyboxCom::ShaderCompile()
{
    vertexShaderBlob = dxCommon->CompileShader(L"Resources/shaders/Skybox.VS.hlsl",
        L"vs_6_0", dxCommon->GetDxcUtils().Get(), dxCommon->GetDxcCompiler(), dxCommon->GetIncludeHandler(), logStream);
    assert(vertexShaderBlob != nullptr);

    pixelShaderBlob = dxCommon->CompileShader(L"Resources/shaders/Skybox.PS.hlsl",
        L"ps_6_0", dxCommon->GetDxcUtils().Get(), dxCommon->GetDxcCompiler(), dxCommon->GetIncludeHandler(), logStream);
    assert(pixelShaderBlob != nullptr);
}

void SkyboxCom::InitializeGraphicPipeline()
{
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    graphicPipelineStateDesc.pRootSignature = rootSignature.Get();
    graphicPipelineStateDesc.InputLayout = inputLayoutDesc;
    graphicPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
    graphicPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
    graphicPipelineStateDesc.BlendState = blendDesc;
    graphicPipelineStateDesc.RasterizerState = rasterizerDesc;
    graphicPipelineStateDesc.NumRenderTargets = 1;
    graphicPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    graphicPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    graphicPipelineStateDesc.SampleDesc.Count = 1;
    graphicPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    // apply depth-stencil which should have been set already by DepthStencilDesc
    dxCommon->SetHr(dxCommon->GetDevice()->CreateGraphicsPipelineState(&graphicPipelineStateDesc, IID_PPV_ARGS(&pipelineState)));
    assert(SUCCEEDED(dxCommon->GetHr()));
}

void SkyboxCom::CreateGraphicsPipeline()
{
    RootSignature();
    Descriptor();
    CreateRootParameters();
    StaticSamplers();
    SignatureBlob();
    RootSignatureFromBlob();
    InputLayer();
    ShaderCompile();
    RasterizerState();
    DepthStencilDesc();
    InitializeGraphicPipeline();
}

void SkyboxCom::DepthStencilDesc()
{
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
    depthStencilDesc.DepthEnable = TRUE;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // don't write depth
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    graphicPipelineStateDesc.DepthStencilState = depthStencilDesc;
    graphicPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
}

void SkyboxCom::RasterizerState()
{
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
}

void SkyboxCom::SetupDraw(ID3D12GraphicsCommandList* commandList)
{
    if (!pipelineState)
    {
        CreateGraphicsPipeline();
    }
    commandList->SetGraphicsRootSignature(rootSignature.Get());
    commandList->SetPipelineState(pipelineState.Get());
}
