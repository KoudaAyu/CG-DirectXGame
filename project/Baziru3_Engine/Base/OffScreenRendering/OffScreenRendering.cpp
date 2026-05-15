#include "OffScreenRendering.h"

#include "Log.h"

#include <cassert>
#include <format>

OffScreenRendering::OffScreenRendering(std::ostream& logStream, DirectXCom* dxCommon)
    : logStream_(logStream), dxCommon_(dxCommon)
{
}

void OffScreenRendering::Initialize(
    uint32_t width,
    uint32_t height,
    DXGI_FORMAT format,
    const Vector4& clearColor)
{
    assert(dxCommon_);

    width_ = width != 0 ? width : static_cast<uint32_t>(dxCommon_->GetViewport().Width);
    height_ = height != 0 ? height : static_cast<uint32_t>(dxCommon_->GetViewport().Height);
    format_ = format;
    clearColor_ = clearColor;

    CreateRootSignature();
    ShaderCompile();
    InitializePipelineState();
    CreateRenderTargets();
}

void OffScreenRendering::Finalize()
{
    if (srvIndex_ != TextureManager::kInvalidTextureIndex)
    {
        if (auto* textureManager = TextureManager::GetInstance())
        {
            if (auto* srvManager = textureManager->GetSRVManager())
            {
                srvManager->Free(srvIndex_);
            }
        }
        srvIndex_ = TextureManager::kInvalidTextureIndex;
    }

    pipelineState_.Reset();
    rootSignature_.Reset();
    signatureBlob_.Reset();
    errorBlob_.Reset();
    vertexShaderBlob_.Reset();
    pixelShaderBlob_.Reset();
    offScreenTexture_.Reset();
    depthStencilResource_.Reset();
    rtvDescriptorHeap_.Reset();
    dsvDescriptorHeap_.Reset();
    srvHandleGPU_ = {};
    currentState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;
}

void OffScreenRendering::Begin(ID3D12GraphicsCommandList* commandList)
{
    assert(commandList);

    TransitionToRenderTarget(commandList);

    commandList->OMSetRenderTargets(1, &rtvHandle_, false, &dsvHandle_);
    commandList->ClearRenderTargetView(rtvHandle_, &clearColor_.x, 0, nullptr);
    commandList->ClearDepthStencilView(dsvHandle_, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    commandList->RSSetViewports(1, &dxCommon_->GetViewport());
    commandList->RSSetScissorRects(1, &dxCommon_->GetScissorRect());

    ID3D12DescriptorHeap* descriptorHeaps[] = { dxCommon_->GetSrvDescriptorHeap().Get() };
    commandList->SetDescriptorHeaps(1, descriptorHeaps);
}

void OffScreenRendering::End(ID3D12GraphicsCommandList* commandList)
{
    assert(commandList);
    TransitionToPixelShaderResource(commandList);
}

void OffScreenRendering::SetMainRenderTarget(ID3D12GraphicsCommandList* commandList) const
{
    assert(commandList);

    const uint32_t backBufferIndex = dxCommon_->GetSwapChain()->GetCurrentBackBufferIndex();
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = dxCommon_->GetRtvHandles()[backBufferIndex];

    commandList->OMSetRenderTargets(1, &rtvHandle, false, nullptr);
    commandList->RSSetViewports(1, &dxCommon_->GetViewport());
    commandList->RSSetScissorRects(1, &dxCommon_->GetScissorRect());
}

void OffScreenRendering::DrawToBackBuffer(ID3D12GraphicsCommandList* commandList)
{
    assert(commandList);
    assert(srvHandleGPU_.ptr != 0);

    ID3D12DescriptorHeap* descriptorHeaps[] = { dxCommon_->GetSrvDescriptorHeap().Get() };
    commandList->SetDescriptorHeaps(1, descriptorHeaps);
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->SetGraphicsRootDescriptorTable(0, srvHandleGPU_);
    commandList->DrawInstanced(3, 1, 0, 0);
}

D3D12_GPU_DESCRIPTOR_HANDLE OffScreenRendering::GetSrvHandleGPU() const
{
    return srvHandleGPU_;
}

ID3D12Resource* OffScreenRendering::GetTextureResource() const
{
    return offScreenTexture_.Get();
}

void OffScreenRendering::CreateRootSignature()
{
    descriptorRange_[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange_[0].NumDescriptors = 1;
    descriptorRange_[0].BaseShaderRegister = 0;
    descriptorRange_[0].RegisterSpace = 0;
    descriptorRange_[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters_[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters_[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters_[0].DescriptorTable.pDescriptorRanges = descriptorRange_;
    rootParameters_[0].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange_);

    staticSamplers_[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers_[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers_[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers_[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers_[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers_[0].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers_[0].ShaderRegister = 0;
    staticSamplers_[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    rootSignatureDesc.pParameters = rootParameters_;
    rootSignatureDesc.NumParameters = _countof(rootParameters_);
    rootSignatureDesc.pStaticSamplers = staticSamplers_;
    rootSignatureDesc.NumStaticSamplers = _countof(staticSamplers_);

    dxCommon_->SetHr(D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        signatureBlob_.GetAddressOf(),
        errorBlob_.GetAddressOf()));

    if (FAILED(dxCommon_->GetHr()))
    {
        if (errorBlob_)
        {
            Logger::Log(logStream_, reinterpret_cast<const char*>(errorBlob_->GetBufferPointer()));
        }
        assert(false);
    }

    dxCommon_->SetHr(dxCommon_->GetDevice()->CreateRootSignature(
        0,
        signatureBlob_->GetBufferPointer(),
        signatureBlob_->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature_)));
    assert(SUCCEEDED(dxCommon_->GetHr()));
}

void OffScreenRendering::ShaderCompile()
{
    vertexShaderBlob_ = dxCommon_->CompileShader(
        L"Resources/shaders/CopyImage.VS.hlsl",
        L"vs_6_0",
        dxCommon_->GetDxcUtils().Get(),
        dxCommon_->GetDxcCompiler(),
        dxCommon_->GetIncludeHandler(),
        logStream_);
    assert(vertexShaderBlob_ != nullptr);

    pixelShaderBlob_ = dxCommon_->CompileShader(
        L"Resources/shaders/GrayScale.PS.hlsl",
        L"ps_6_0",
        dxCommon_->GetDxcUtils().Get(),
        dxCommon_->GetDxcCompiler(),
        dxCommon_->GetIncludeHandler(),
        logStream_);
    assert(pixelShaderBlob_ != nullptr);
}

void OffScreenRendering::InitializePipelineState()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = rootSignature_.Get();
    desc.InputLayout.pInputElementDescs = nullptr;
    desc.InputLayout.NumElements = 0;
    desc.VS = { vertexShaderBlob_->GetBufferPointer(), vertexShaderBlob_->GetBufferSize() };
    desc.PS = { pixelShaderBlob_->GetBufferPointer(), pixelShaderBlob_->GetBufferSize() };

    D3D12_BLEND_DESC blendDesc{};
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    blendDesc.RenderTarget[0].BlendEnable = FALSE;
    blendDesc.RenderTarget[0].LogicOpEnable = FALSE;
    blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    desc.BlendState = blendDesc;

    D3D12_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterizerDesc.FrontCounterClockwise = FALSE;
    rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    rasterizerDesc.DepthClipEnable = TRUE;
    rasterizerDesc.MultisampleEnable = FALSE;
    rasterizerDesc.AntialiasedLineEnable = FALSE;
    rasterizerDesc.ForcedSampleCount = 0;
    rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    desc.RasterizerState = rasterizerDesc;

    D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
    depthStencilDesc.DepthEnable = FALSE;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    depthStencilDesc.StencilEnable = FALSE;
    depthStencilDesc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
    depthStencilDesc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
    depthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    depthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    depthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
    depthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    depthStencilDesc.BackFace = depthStencilDesc.FrontFace;
    desc.DepthStencilState = depthStencilDesc;

    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = format_;
    desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    dxCommon_->SetHr(dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipelineState_)));
    if (FAILED(dxCommon_->GetHr()))
    {
        Logger::Log(logStream_, std::format(
            "OffScreenRendering::InitializePipelineState - CreateGraphicsPipelineState failed hr=0x{:08X}\n",
            static_cast<unsigned int>(dxCommon_->GetHr())));
        assert(false);
    }
}

void OffScreenRendering::CreateRenderTargets()
{
    offScreenTexture_ = renderTexture_.CreateRenderTargetTexture(
        dxCommon_->GetDevice(), width_, height_, format_, clearColor_);
    assert(offScreenTexture_ != nullptr);

    rtvDescriptorHeap_ = dxCommon_->CreateDescriptorHeap(
        dxCommon_->GetDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, false);
    rtvHandle_ = rtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format = format_;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    dxCommon_->GetDevice()->CreateRenderTargetView(offScreenTexture_.Get(), &rtvDesc, rtvHandle_);

    depthStencilResource_ = dxCommon_->CreateDepthStencilTextureResource(
        dxCommon_->GetDevice(), static_cast<int32_t>(width_), static_cast<int32_t>(height_));
    dsvDescriptorHeap_ = dxCommon_->CreateDescriptorHeap(
        dxCommon_->GetDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
    dsvHandle_ = dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dxCommon_->GetDevice()->CreateDepthStencilView(depthStencilResource_.Get(), &dsvDesc, dsvHandle_);

    auto* textureManager = TextureManager::GetInstance();
    assert(textureManager);
    auto* srvManager = textureManager->GetSRVManager();
    assert(srvManager);

    srvIndex_ = srvManager->Allocate();
    srvManager->CreateSRVForTexture2D(srvIndex_, offScreenTexture_.Get(), format_, 1);
    srvHandleGPU_ = srvManager->GetGPUDescriptorHandle(srvIndex_);
    currentState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;
}

void OffScreenRendering::TransitionToRenderTarget(ID3D12GraphicsCommandList* commandList)
{
    TransitionResource(commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
}

void OffScreenRendering::TransitionToPixelShaderResource(ID3D12GraphicsCommandList* commandList)
{
    TransitionResource(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void OffScreenRendering::TransitionResource(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES nextState)
{
    if (currentState_ == nextState)
    {
        return;
    }

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = offScreenTexture_.Get();
    barrier.Transition.StateBefore = currentState_;
    barrier.Transition.StateAfter = nextState;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);
    currentState_ = nextState;
}
