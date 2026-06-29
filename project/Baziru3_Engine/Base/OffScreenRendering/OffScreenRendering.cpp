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

    if (depthSrvIndex_ != TextureManager::kInvalidTextureIndex)
    {
        if (auto* textureManager = TextureManager::GetInstance())
        {
            if (auto* srvManager = textureManager->GetSRVManager())
            {
                srvManager->Free(depthSrvIndex_);
            }
        }
        depthSrvIndex_ = TextureManager::kInvalidTextureIndex;
    }

    if (inverseProjectionBuffer_)
    {
        inverseProjectionBuffer_->Unmap(0, nullptr);
        inverseProjectionMap_ = nullptr;
        inverseProjectionBuffer_.Reset();
    }

    if (radialBlurBuffer_)
    {
        radialBlurBuffer_->Unmap(0, nullptr);
        radialBlurMap_ = nullptr;
        radialBlurBuffer_.Reset();
    }

    for (size_t i = 0; i < static_cast<size_t>(PostEffect::Count); ++i)
    {
        pipelineStates_[i].Reset();
        pixelShaderBlobs_[i].Reset();
    }
    rootSignature_.Reset();
    signatureBlob_.Reset();
    errorBlob_.Reset();
    vertexShaderBlob_.Reset();
    offScreenTexture_.Reset();
    depthStencilResource_.Reset();
    rtvDescriptorHeap_.Reset();
    dsvDescriptorHeap_.Reset();
    srvHandleGPU_ = {};
    depthSrvHandleGPU_ = {};
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
    assert(depthSrvHandleGPU_.ptr != 0);

    // Transition depth buffer resource barrier: WRITE -> PIXEL_SHADER_RESOURCE
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = depthStencilResource_.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);

    ID3D12DescriptorHeap* descriptorHeaps[] = { dxCommon_->GetSrvDescriptorHeap().Get() };
    commandList->SetDescriptorHeaps(1, descriptorHeaps);
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineStates_[static_cast<size_t>(currentEffect_)].Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->SetGraphicsRootDescriptorTable(0, srvHandleGPU_);
    commandList->SetGraphicsRootDescriptorTable(1, depthSrvHandleGPU_);
    if (radialBlurBuffer_)
    {
        commandList->SetGraphicsRootConstantBufferView(2, radialBlurBuffer_->GetGPUVirtualAddress());
    }
    if (inverseProjectionBuffer_)
    {
        commandList->SetGraphicsRootConstantBufferView(3, inverseProjectionBuffer_->GetGPUVirtualAddress());
    }
    commandList->DrawInstanced(3, 1, 0, 0);

    // Transition depth buffer resource barrier back: PIXEL_SHADER_RESOURCE -> WRITE
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    commandList->ResourceBarrier(1, &barrier);
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
    // Color Texture (t0)
    descriptorRange_[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange_[0].NumDescriptors = 1;
    descriptorRange_[0].BaseShaderRegister = 0;
    descriptorRange_[0].RegisterSpace = 0;
    descriptorRange_[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters_[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters_[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters_[0].DescriptorTable.pDescriptorRanges = &descriptorRange_[0];
    rootParameters_[0].DescriptorTable.NumDescriptorRanges = 1;

    // Depth Texture (t1)
    descriptorRange_[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange_[1].NumDescriptors = 1;
    descriptorRange_[1].BaseShaderRegister = 1;
    descriptorRange_[1].RegisterSpace = 0;
    descriptorRange_[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters_[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters_[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters_[1].DescriptorTable.pDescriptorRanges = &descriptorRange_[1];
    rootParameters_[1].DescriptorTable.NumDescriptorRanges = 1;

    // Constant Buffer (b0) - RadialBlur
    rootParameters_[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters_[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters_[2].Descriptor.ShaderRegister = 0;
    rootParameters_[2].Descriptor.RegisterSpace = 0;

    // Constant Buffer (b1) - ProjectionMatrix
    rootParameters_[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters_[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters_[3].Descriptor.ShaderRegister = 1;
    rootParameters_[3].Descriptor.RegisterSpace = 0;

    // Sampler (s0) - Linear
    staticSamplers_[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers_[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers_[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers_[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers_[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers_[0].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers_[0].ShaderRegister = 0;
    staticSamplers_[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // Sampler (s1) - Point
    staticSamplers_[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    staticSamplers_[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers_[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers_[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers_[1].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers_[1].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers_[1].ShaderRegister = 1;
    staticSamplers_[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

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

    const wchar_t* shaderPaths[static_cast<size_t>(PostEffect::Count)] = {
        L"Resources/shaders/CopyImage.PS.hlsl",
        L"Resources/shaders/DepthBasedOutline.PS.hlsl",
        L"Resources/shaders/LuminanceBaseOutline.PS.hlsl",
        L"Resources/shaders/RadialBlur.PS.hlsl",
        L"Resources/shaders/GaussianFilter.PS.hlsl",
        L"Resources/shaders/BoxFilter.PS.hlsl"
    };

    for (size_t i = 0; i < static_cast<size_t>(PostEffect::Count); ++i)
    {
        pixelShaderBlobs_[i] = dxCommon_->CompileShader(
            shaderPaths[i],
            L"ps_6_0",
            dxCommon_->GetDxcUtils().Get(),
            dxCommon_->GetDxcCompiler(),
            dxCommon_->GetIncludeHandler(),
            logStream_);
        assert(pixelShaderBlobs_[i] != nullptr);
    }
}

void OffScreenRendering::InitializePipelineState()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = rootSignature_.Get();
    desc.InputLayout.pInputElementDescs = nullptr;
    desc.InputLayout.NumElements = 0;
    desc.VS = { vertexShaderBlob_->GetBufferPointer(), vertexShaderBlob_->GetBufferSize() };

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

    for (size_t i = 0; i < static_cast<size_t>(PostEffect::Count); ++i)
    {
        desc.PS = { pixelShaderBlobs_[i]->GetBufferPointer(), pixelShaderBlobs_[i]->GetBufferSize() };
        dxCommon_->SetHr(dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipelineStates_[i])));
        if (FAILED(dxCommon_->GetHr()))
        {
            Logger::Log(logStream_, std::format(
                "OffScreenRendering::InitializePipelineState - CreateGraphicsPipelineState failed index={} hr=0x{:08X}\n",
                i, static_cast<unsigned int>(dxCommon_->GetHr())));
            assert(false);
        }
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

    // Create Depth SRV (Format: DXGI_FORMAT_R24_UNORM_X8_TYPELESS)
    depthSrvIndex_ = srvManager->Allocate();
    D3D12_SHADER_RESOURCE_VIEW_DESC depthTextureSrvDesc{};
    depthTextureSrvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    depthTextureSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    depthTextureSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    depthTextureSrvDesc.Texture2D.MipLevels = 1;
    dxCommon_->GetDevice()->CreateShaderResourceView(
        depthStencilResource_.Get(),
        &depthTextureSrvDesc,
        srvManager->GetCPUDescriptorHandle(depthSrvIndex_));
    depthSrvHandleGPU_ = srvManager->GetGPUDescriptorHandle(depthSrvIndex_);

    // Create Constant Buffer for inverseProjection matrix
    D3D12_HEAP_PROPERTIES uploadHeapProperties{};
    uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
    
    D3D12_RESOURCE_DESC bufferDesc{};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = (sizeof(Matrix4x4) + 255) & ~255;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    dxCommon_->SetHr(dxCommon_->GetDevice()->CreateCommittedResource(
        &uploadHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&inverseProjectionBuffer_)));
    assert(SUCCEEDED(dxCommon_->GetHr()));

    dxCommon_->SetHr(inverseProjectionBuffer_->Map(0, nullptr, &inverseProjectionMap_));
    assert(SUCCEEDED(dxCommon_->GetHr()));

    // Create Constant Buffer for RadialBlur parameters
    D3D12_RESOURCE_DESC radialBlurBufferDesc{};
    radialBlurBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    radialBlurBufferDesc.Width = (sizeof(RadialBlurData) + 255) & ~255;
    radialBlurBufferDesc.Height = 1;
    radialBlurBufferDesc.DepthOrArraySize = 1;
    radialBlurBufferDesc.MipLevels = 1;
    radialBlurBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    radialBlurBufferDesc.SampleDesc.Count = 1;
    radialBlurBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    radialBlurBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    dxCommon_->SetHr(dxCommon_->GetDevice()->CreateCommittedResource(
        &uploadHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &radialBlurBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&radialBlurBuffer_)));
    assert(SUCCEEDED(dxCommon_->GetHr()));

    dxCommon_->SetHr(radialBlurBuffer_->Map(0, nullptr, &radialBlurMap_));
    assert(SUCCEEDED(dxCommon_->GetHr()));
    std::memcpy(radialBlurMap_, &radialBlurData_, sizeof(RadialBlurData));

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

void OffScreenRendering::SetProjectionInverse(const Matrix4x4& projectionInverse)
{
    if (inverseProjectionMap_)
    {
        std::memcpy(inverseProjectionMap_, &projectionInverse, sizeof(Matrix4x4));
    }
}

void OffScreenRendering::SetRadialBlurCenter(const Vector2& center)
{
    radialBlurData_.center = center;
    if (radialBlurMap_)
    {
        std::memcpy(radialBlurMap_, &radialBlurData_, sizeof(RadialBlurData));
    }
}

void OffScreenRendering::SetRadialBlurWidth(float width)
{
    radialBlurData_.blurWidth = width;
    if (radialBlurMap_)
    {
        std::memcpy(radialBlurMap_, &radialBlurData_, sizeof(RadialBlurData));
    }
}
