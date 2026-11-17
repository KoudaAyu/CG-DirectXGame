#include "WaterSurface.h"
#include "Log.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>

namespace BaziruEngine::Environment
{

void WaterSurface::InitializeGrid(
    DirectXCom* dxCommon,
    std::ostream& logStream,
    float sizeX,
    float sizeZ,
    int gridX,
    int gridZ,
    float waterLevel)
{
    dxCommon_ = dxCommon;
    waterLevel_ = waterLevel;
    if (!dxCommon_)
    {
        return;
    }

    CreatePipeline(logStream);
    BuildGridMesh(sizeX, sizeZ, gridX, gridZ);

    const size_t cbBytes = (sizeof(RealisticWaterConstants) + 255) & ~static_cast<size_t>(255);
    constantsBuffer_ = dxCommon_->CreateBufferResource(dxCommon_->GetDevice().Get(), cbBytes);
    constantsBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedConstants_));
}

void WaterSurface::Finalize()
{
    if (constantsBuffer_)
    {
        constantsBuffer_->Unmap(0, nullptr);
        mappedConstants_ = nullptr;
        constantsBuffer_.Reset();
    }
    vertexBuffer_.Reset();
    indexBuffer_.Reset();
    pipelineState_.Reset();
    rootSignature_.Reset();
    dxCommon_ = nullptr;
}

void WaterSurface::Update(float deltaTime)
{
    time_ += deltaTime;
}

void WaterSurface::BuildGridMesh(float sizeX, float sizeZ, int gridX, int gridZ)
{
    gridX = (std::max)(2, gridX);
    gridZ = (std::max)(2, gridZ);

    std::vector<WaterVertex> verts;
    std::vector<uint32_t> indices;
    verts.reserve(static_cast<size_t>((gridX + 1) * (gridZ + 1)));
    indices.reserve(static_cast<size_t>(gridX * gridZ * 6));

    for (int iz = 0; iz <= gridZ; ++iz)
    {
        float vz = static_cast<float>(iz) / static_cast<float>(gridZ);
        float z = (vz - 0.5f) * sizeZ;
        for (int ix = 0; ix <= gridX; ++ix)
        {
            float vx = static_cast<float>(ix) / static_cast<float>(gridX);
            float x = (vx - 0.5f) * sizeX;
            WaterVertex v{};
            v.position = { x, 0.0f, z };
            v.uv = { vx, vz };
            verts.push_back(v);
        }
    }

    const uint32_t stride = static_cast<uint32_t>(gridX + 1);
    for (int iz = 0; iz < gridZ; ++iz)
    {
        for (int ix = 0; ix < gridX; ++ix)
        {
            uint32_t i0 = static_cast<uint32_t>(iz) * stride + static_cast<uint32_t>(ix);
            uint32_t i1 = i0 + 1;
            uint32_t i2 = i0 + stride;
            uint32_t i3 = i2 + 1;

            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);

            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }

    const size_t vbBytes = sizeof(WaterVertex) * verts.size();
    vertexBuffer_ = dxCommon_->CreateBufferResource(dxCommon_->GetDevice().Get(), vbBytes);
    void* vbPtr = nullptr;
    vertexBuffer_->Map(0, nullptr, &vbPtr);
    std::memcpy(vbPtr, verts.data(), vbBytes);
    vertexBuffer_->Unmap(0, nullptr);

    vbv_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
    vbv_.SizeInBytes = static_cast<UINT>(vbBytes);
    vbv_.StrideInBytes = sizeof(WaterVertex);

    const size_t ibBytes = sizeof(uint32_t) * indices.size();
    indexBuffer_ = dxCommon_->CreateBufferResource(dxCommon_->GetDevice().Get(), ibBytes);
    void* ibPtr = nullptr;
    indexBuffer_->Map(0, nullptr, &ibPtr);
    std::memcpy(ibPtr, indices.data(), ibBytes);
    indexBuffer_->Unmap(0, nullptr);

    ibv_.BufferLocation = indexBuffer_->GetGPUVirtualAddress();
    ibv_.SizeInBytes = static_cast<UINT>(ibBytes);
    ibv_.Format = DXGI_FORMAT_R32_UINT;
    indexCount_ = static_cast<uint32_t>(indices.size());
}

void WaterSurface::CreatePipeline(std::ostream& logStream)
{
    D3D12_DESCRIPTOR_RANGE srvRanges[3]{};
    // t0: Refraction Scene Color
    srvRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRanges[0].NumDescriptors = 1;
    srvRanges[0].BaseShaderRegister = 0;
    srvRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // t1: Refraction Scene Depth
    srvRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRanges[1].NumDescriptors = 1;
    srvRanges[1].BaseShaderRegister = 1;
    srvRanges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // t2: Environment Cubemap
    srvRanges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRanges[2].NumDescriptors = 1;
    srvRanges[2].BaseShaderRegister = 2;
    srvRanges[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParams[6]{};
    // 0: b0 RealisticWaterConstants (VS/PS)
    rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParams[0].Descriptor.ShaderRegister = 0;

    // 1: b1 LightGroup (PS)
    rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParams[1].Descriptor.ShaderRegister = 1;

    // 2: b2 Camera (PS)
    rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParams[2].Descriptor.ShaderRegister = 2;

    // 3: t0 Refraction Color (PS)
    rootParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParams[3].DescriptorTable.pDescriptorRanges = &srvRanges[0];
    rootParams[3].DescriptorTable.NumDescriptorRanges = 1;

    // 4: t1 Refraction Depth (PS)
    rootParams[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParams[4].DescriptorTable.pDescriptorRanges = &srvRanges[1];
    rootParams[4].DescriptorTable.NumDescriptorRanges = 1;

    // 5: t2 Environment Cubemap (PS)
    rootParams[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParams[5].DescriptorTable.pDescriptorRanges = &srvRanges[2];
    rootParams[5].DescriptorTable.NumDescriptorRanges = 1;

    D3D12_STATIC_SAMPLER_DESC samplers[3]{};
    // s0: Linear Clamp (Refraction Color)
    samplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    samplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    samplers[0].ShaderRegister = 0;
    samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // s1: Point Clamp (Refraction Depth)
    samplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    samplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[1].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    samplers[1].MaxLOD = D3D12_FLOAT32_MAX;
    samplers[1].ShaderRegister = 1;
    samplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // s2: Linear Wrap (Cubemap)
    samplers[2].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samplers[2].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplers[2].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplers[2].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplers[2].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    samplers[2].MaxLOD = D3D12_FLOAT32_MAX;
    samplers[2].ShaderRegister = 2;
    samplers[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rsDesc{};
    rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    rsDesc.pParameters = rootParams;
    rsDesc.NumParameters = _countof(rootParams);
    rsDesc.pStaticSamplers = samplers;
    rsDesc.NumStaticSamplers = _countof(samplers);

    Microsoft::WRL::ComPtr<ID3DBlob> sigBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errBlob;
    HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob);
    if (FAILED(hr))
    {
        if (errBlob)
        {
            Logger::Log(logStream, reinterpret_cast<const char*>(errBlob->GetBufferPointer()));
        }
        assert(false);
    }

    hr = dxCommon_->GetDevice()->CreateRootSignature(
        0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));

    auto vsBlob = dxCommon_->CompileShader(
        L"Resources/shaders/RealisticWater.VS.hlsl", L"vs_6_0",
        dxCommon_->GetDxcUtils().Get(), dxCommon_->GetDxcCompiler(), dxCommon_->GetIncludeHandler(), logStream);
    auto psBlob = dxCommon_->CompileShader(
        L"Resources/shaders/RealisticWater.PS.hlsl", L"ps_6_0",
        dxCommon_->GetDxcUtils().Get(), dxCommon_->GetDxcCompiler(), dxCommon_->GetIncludeHandler(), logStream);
    assert(vsBlob && psBlob);

    D3D12_INPUT_ELEMENT_DESC inputElems[2]{};
    inputElems[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(WaterVertex, position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
    inputElems[1] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(WaterVertex, uv),       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = rootSignature_.Get();
    psoDesc.InputLayout = { inputElems, _countof(inputElems) };
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };

    // ピクセルシェーダー側でスナップショット背景と物理屈折合成するため不透明書き込みで高精度出力
    psoDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;

    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
    assert(SUCCEEDED(hr));
}

void WaterSurface::Draw(
    ID3D12GraphicsCommandList* commandList,
    OffScreenRendering* offScreen,
    const Camera* camera,
    const Light* light,
    D3D12_GPU_DESCRIPTOR_HANDLE envCubemapSrv)
{
    if (!commandList || !offScreen || !pipelineState_ || indexCount_ == 0 || !camera || !mappedConstants_)
    {
        return;
    }

    // 1. 不透明シーン（川底・岩・岸辺の草木・空）のカラー＆深度を屈折テクスチャへスナップショット
    offScreen->CaptureSceneSnapshot(commandList);

    // 2. 定数バッファ更新
    Matrix4x4 world = MakeTranslateMatrix({ 0.0f, waterLevel_, 0.0f });
    Matrix4x4 viewProj = Multiply(camera->GetViewMatrix(), camera->GetProjectionMatrix());

    mappedConstants_->world = world;
    mappedConstants_->viewProj = viewProj;
    mappedConstants_->invViewProj = Inverse(viewProj);
    mappedConstants_->shallowColor = shallowColor_;
    mappedConstants_->time = time_;
    mappedConstants_->deepColor = deepColor_;
    mappedConstants_->waveHeight = waveHeight_;
    mappedConstants_->absorptionCoeff = baseAbsorption_ * absorptionScale_;
    mappedConstants_->flowSpeed = flowSpeed_;

    float dirLen = std::sqrt(flowDirection_.x * flowDirection_.x + flowDirection_.y * flowDirection_.y);
    mappedConstants_->flowDirection = (dirLen > 1e-4f)
        ? Vector2{ flowDirection_.x / dirLen, flowDirection_.y / dirLen }
        : Vector2{ 0.0f, 1.0f };

    mappedConstants_->refractionScale = refractionScale_;
    mappedConstants_->causticsIntensity = causticsIntensity_;
    mappedConstants_->foamWidth = foamWidth_;
    mappedConstants_->foamIntensity = foamIntensity_;
    mappedConstants_->nearZ = camera->GetNearZ();
    mappedConstants_->farZ = camera->GetFarZ();
    mappedConstants_->fogColor = fogColor_;
    mappedConstants_->fogDensity = fogDensity_;

    // 3. パイプライン＆リソースバインド
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &vbv_);
    commandList->IASetIndexBuffer(&ibv_);

    commandList->SetGraphicsRootConstantBufferView(0, constantsBuffer_->GetGPUVirtualAddress());
    if (light && light->GetLightResource())
    {
        commandList->SetGraphicsRootConstantBufferView(1, light->GetLightResource()->GetGPUVirtualAddress());
    }
    if (camera->GetCameraGpuAddress() != 0)
    {
        commandList->SetGraphicsRootConstantBufferView(2, camera->GetCameraGpuAddress());
    }

    commandList->SetGraphicsRootDescriptorTable(3, offScreen->GetRefractionColorSrvGPU());
    commandList->SetGraphicsRootDescriptorTable(4, offScreen->GetRefractionDepthSrvGPU());
    if (envCubemapSrv.ptr != 0)
    {
        commandList->SetGraphicsRootDescriptorTable(5, envCubemapSrv);
    }

    commandList->DrawIndexedInstanced(indexCount_, 1, 0, 0, 0);
}

} // namespace BaziruEngine::Environment
