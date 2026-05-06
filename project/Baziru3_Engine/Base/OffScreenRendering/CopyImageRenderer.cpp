#include "CopyImageRenderer.h"

#include "DirectXCom.h"
#include "Log.h"

#include <cassert>

CopyImageRenderer::CopyImageRenderer(std::ostream& logStream, DirectXCom* dxCommon)
    : dxCommon(dxCommon), logStream(logStream)
{
}

void CopyImageRenderer::Initialize()
{
    assert(dxCommon);

    RootSignature();
    Descriptor();
    CreateRootParameters();
    StaticSamplers();
    SignatureBlob();
    RootSignatureFromBlob();
    ShaderCompile();
    InitializeBlend();
    RasterizerState();
    InitializeGraphicPipeline();
}

void CopyImageRenderer::SetupDraw(ID3D12GraphicsCommandList* commandList)
{
    assert(commandList);
    commandList->SetGraphicsRootSignature(rootSignature.Get());
    commandList->SetPipelineState(pipelineState.Get());
}

void CopyImageRenderer::Draw(ID3D12GraphicsCommandList* commandList, D3D12_GPU_DESCRIPTOR_HANDLE textureHandle)
{
    if (!commandList || textureHandle.ptr == 0)
    {
        return;
    }

    SetupDraw(commandList);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->SetGraphicsRootDescriptorTable(0, textureHandle);
    commandList->DrawInstanced(3, 1, 0, 0);
}

void CopyImageRenderer::RootSignature()
{
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
}

void CopyImageRenderer::Descriptor()
{
    descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange[0].NumDescriptors = 1;
    descriptorRange[0].BaseShaderRegister = 0;
    descriptorRange[0].RegisterSpace = 0;
    descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
}

void CopyImageRenderer::CreateRootParameters()
{
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].DescriptorTable.pDescriptorRanges = descriptorRange;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);
}

void CopyImageRenderer::StaticSamplers()
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
    descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);
}

void CopyImageRenderer::SignatureBlob()
{
    dxCommon->SetHr(D3D12SerializeRootSignature(
        &descriptionRootSignature,
        D3D_ROOT_SIGNATURE_VERSION_1,
        signatureBlob.GetAddressOf(),
        errorBlob.GetAddressOf()));

    if (FAILED(dxCommon->GetHr()))
    {
        if (errorBlob)
        {
            Logger::Log(logStream, reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
        }
        assert(false);
    }
}

void CopyImageRenderer::RootSignatureFromBlob()
{
    dxCommon->SetHr(dxCommon->GetDevice()->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature)));
    assert(SUCCEEDED(dxCommon->GetHr()));
}

void CopyImageRenderer::ShaderCompile()
{
    vertexShaderBlob = dxCommon->CompileShader(
        L"Resources/shaders/CopyImage.VS.hlsl",
        L"vs_6_0",
        dxCommon->GetDxcUtils().Get(),
        dxCommon->GetDxcCompiler(),
        dxCommon->GetIncludeHandler(),
        logStream);
    Logger::Log(logStream, std::format(
        "CopyImageRenderer::ShaderCompile - VS blob=0x{:X} size={}\n",
        static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(vertexShaderBlob.Get())),
        vertexShaderBlob ? vertexShaderBlob->GetBufferSize() : 0));
    assert(vertexShaderBlob != nullptr);

    pixelShaderBlob = dxCommon->CompileShader(
        L"Resources/shaders/CopyImage.PS.hlsl",
        L"ps_6_0",
        dxCommon->GetDxcUtils().Get(),
        dxCommon->GetDxcCompiler(),
        dxCommon->GetIncludeHandler(),
        logStream);
 Logger::Log(logStream, std::format(
        "CopyImageRenderer::ShaderCompile - PS blob=0x{:X} size={}\n",
        static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(pixelShaderBlob.Get())),
        pixelShaderBlob ? pixelShaderBlob->GetBufferSize() : 0));
    assert(pixelShaderBlob != nullptr);
}

void CopyImageRenderer::InitializeGraphicPipeline()
{
  assert(vertexShaderBlob != nullptr);
    assert(pixelShaderBlob != nullptr);
    Logger::Log(logStream, std::format(
        "CopyImageRenderer::InitializeGraphicPipeline - rootSig=0x{:X} VSsize={} PSsize={} RTVFormat={}\n",
        static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(rootSignature.Get())),
        vertexShaderBlob->GetBufferSize(),
        pixelShaderBlob->GetBufferSize(),
        static_cast<uint32_t>(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)));
    graphicPipelineStateDesc.pRootSignature = rootSignature.Get();
    DirectXCom::SetupCopyImageInputLayout(inputLayoutDesc);
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
    DirectXCom::SetupCopyImageDepthStencilState(graphicPipelineStateDesc);

    dxCommon->SetHr(dxCommon->GetDevice()->CreateGraphicsPipelineState(
        &graphicPipelineStateDesc,
        IID_PPV_ARGS(&pipelineState)));
   Logger::Log(logStream, std::format(
        "CopyImageRenderer::InitializeGraphicPipeline - CreateGraphicsPipelineState hr=0x{:08X} pipeline=0x{:X}\n",
        static_cast<unsigned int>(dxCommon->GetHr()),
        static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(pipelineState.Get()))));
    assert(SUCCEEDED(dxCommon->GetHr()));
}

void CopyImageRenderer::RasterizerState()
{
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
}

void CopyImageRenderer::InitializeBlend()
{
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    blendDesc.RenderTarget[0].BlendEnable = FALSE;
    blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
}
