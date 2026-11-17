#include "FoliageField.h"
#include "Log.h"
#include "SRVManager.h"
#include "TextureManager.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <random>

namespace BaziruEngine::Environment
{

namespace
{
constexpr float kPi = 3.14159265358979323846f;

float Hash2D(int x, int z)
{
    int n = x * 374761393 + z * 668265263;
    n = (n ^ (n >> 13)) * 1274126177;
    return static_cast<float>((n ^ (n >> 16)) & 0x7fffffff) / 2147483647.0f;
}

float ValueNoise2D(float x, float z)
{
    int ix = static_cast<int>(std::floor(x));
    int iz = static_cast<int>(std::floor(z));
    float fx = x - static_cast<float>(ix);
    float fz = z - static_cast<float>(iz);
    float ux = fx * fx * (3.0f - 2.0f * fx);
    float uz = fz * fz * (3.0f - 2.0f * fz);

    float a = Hash2D(ix, iz);
    float b = Hash2D(ix + 1, iz);
    float c = Hash2D(ix, iz + 1);
    float d = Hash2D(ix + 1, iz + 1);

    float ab = a + (b - a) * ux;
    float cd = c + (d - c) * ux;
    return ab + (cd - ab) * uz;
}
} // namespace

void FoliageField::Initialize(DirectXCom* dxCommon, std::ostream& logStream, uint32_t maxInstances)
{
    dxCommon_ = dxCommon;
    maxInstances_ = (std::max)(1u, maxInstances);
    if (!dxCommon_)
    {
        return;
    }

    CreateBladeMesh();
    CreatePipeline(logStream);

    // インスタンス用 StructuredBuffer
    const size_t instBytes = sizeof(GrassInstance) * maxInstances_;
    instanceBuffer_ = dxCommon_->CreateBufferResource(dxCommon_->GetDevice().Get(), instBytes);
    instanceBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedInstances_));

    auto* srvMgr = TextureManager::GetInstance() ? TextureManager::GetInstance()->GetSRVManager() : nullptr;
    assert(srvMgr != nullptr);
    instanceSrvIndex_ = srvMgr->Allocate();
    srvMgr->CreateSRVForStructuredBuffer(instanceSrvIndex_, instanceBuffer_.Get(), maxInstances_, sizeof(GrassInstance));
    instanceSrvGpu_ = srvMgr->GetGPUDescriptorHandle(instanceSrvIndex_);

    // 描画定数バッファ
    const size_t cbBytes = (sizeof(GrassFieldConstants) + 255) & ~static_cast<size_t>(255);
    constantsBuffer_ = dxCommon_->CreateBufferResource(dxCommon_->GetDevice().Get(), cbBytes);
    constantsBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedConstants_));
}

void FoliageField::Finalize()
{
    if (instanceSrvIndex_ != UINT32_MAX)
    {
        if (auto* texMgr = TextureManager::GetInstance())
        {
            if (auto* srvMgr = texMgr->GetSRVManager())
            {
                srvMgr->Free(instanceSrvIndex_);
            }
        }
        instanceSrvIndex_ = UINT32_MAX;
    }

    if (instanceBuffer_)
    {
        instanceBuffer_->Unmap(0, nullptr);
        mappedInstances_ = nullptr;
        instanceBuffer_.Reset();
    }
    if (constantsBuffer_)
    {
        constantsBuffer_->Unmap(0, nullptr);
        mappedConstants_ = nullptr;
        constantsBuffer_.Reset();
    }
    bladeVertexBuffer_.Reset();
    bladeIndexBuffer_.Reset();
    pipelineState_.Reset();
    rootSignature_.Reset();
    dxCommon_ = nullptr;
}

void FoliageField::CreateBladeMesh()
{
    // 5セグメント先細り＆断面キャンバー付きの立体草ブレード (11頂点・9三角形)
    constexpr int kSegments = 5;
    std::vector<GrassBladeVertex> verts;
    std::vector<uint32_t> indices;
    verts.reserve(kSegments * 2 + 1);

    for (int s = 0; s < kSegments; ++s)
    {
        float t = static_cast<float>(s) / static_cast<float>(kSegments);
        float halfW = 0.5f * (1.0f - 0.72f * t * t);
        float zCurve = 0.14f * t * t;

        // 左右で法線を少し外向きに開くことで、どの角度からも立体的な太陽ハイライトを拾う
        GrassBladeVertex vL{};
        vL.localPos = { -halfW, t, zCurve };
        vL.normal = { -0.38f, 0.24f, -0.89f };
        vL.heightT = t;

        GrassBladeVertex vR{};
        vR.localPos = { halfW, t, zCurve };
        vR.normal = { 0.38f, 0.24f, -0.89f };
        vR.heightT = t;

        verts.push_back(vL);
        verts.push_back(vR);
    }

    // 鋭い穂先頂点
    GrassBladeVertex vTip{};
    vTip.localPos = { 0.0f, 1.0f, 0.18f };
    vTip.normal = { 0.0f, 0.42f, -0.91f };
    vTip.heightT = 1.0f;
    verts.push_back(vTip);

    for (int s = 0; s < kSegments - 1; ++s)
    {
        uint32_t b = static_cast<uint32_t>(s * 2);
        indices.push_back(b + 0);
        indices.push_back(b + 2);
        indices.push_back(b + 1);

        indices.push_back(b + 1);
        indices.push_back(b + 2);
        indices.push_back(b + 3);
    }
    uint32_t lastPair = static_cast<uint32_t>((kSegments - 1) * 2);
    uint32_t tipIdx = static_cast<uint32_t>(kSegments * 2);
    indices.push_back(lastPair + 0);
    indices.push_back(tipIdx);
    indices.push_back(lastPair + 1);

    const size_t vbBytes = sizeof(GrassBladeVertex) * verts.size();
    bladeVertexBuffer_ = dxCommon_->CreateBufferResource(dxCommon_->GetDevice().Get(), vbBytes);
    void* vbPtr = nullptr;
    bladeVertexBuffer_->Map(0, nullptr, &vbPtr);
    std::memcpy(vbPtr, verts.data(), vbBytes);
    bladeVertexBuffer_->Unmap(0, nullptr);

    vbv_.BufferLocation = bladeVertexBuffer_->GetGPUVirtualAddress();
    vbv_.SizeInBytes = static_cast<UINT>(vbBytes);
    vbv_.StrideInBytes = sizeof(GrassBladeVertex);

    const size_t ibBytes = sizeof(uint32_t) * indices.size();
    bladeIndexBuffer_ = dxCommon_->CreateBufferResource(dxCommon_->GetDevice().Get(), ibBytes);
    void* ibPtr = nullptr;
    bladeIndexBuffer_->Map(0, nullptr, &ibPtr);
    std::memcpy(ibPtr, indices.data(), ibBytes);
    bladeIndexBuffer_->Unmap(0, nullptr);

    ibv_.BufferLocation = bladeIndexBuffer_->GetGPUVirtualAddress();
    ibv_.SizeInBytes = static_cast<UINT>(ibBytes);
    ibv_.Format = DXGI_FORMAT_R32_UINT;
    bladeIndexCount_ = static_cast<uint32_t>(indices.size());
}

void FoliageField::CreatePipeline(std::ostream& logStream)
{
    D3D12_DESCRIPTOR_RANGE srvRange{};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 1;
    srvRange.BaseShaderRegister = 0;
    srvRange.RegisterSpace = 0;
    srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParams[5]{};
    // 0: b0 GrassFieldConstants (VS/PS)
    rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParams[0].Descriptor.ShaderRegister = 0;

    // 1: b1 LightGroup (PS)
    rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParams[1].Descriptor.ShaderRegister = 1;

    // 2: b2 Camera (VS/PS)
    rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParams[2].Descriptor.ShaderRegister = 2;

    // 3: b3 WindForGPU (VS/PS)
    rootParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParams[3].Descriptor.ShaderRegister = 3;

    // 4: t0 StructuredBuffer<GrassInstance> (VS)
    rootParams[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParams[4].DescriptorTable.pDescriptorRanges = &srvRange;
    rootParams[4].DescriptorTable.NumDescriptorRanges = 1;

    D3D12_ROOT_SIGNATURE_DESC rsDesc{};
    rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    rsDesc.pParameters = rootParams;
    rsDesc.NumParameters = _countof(rootParams);

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
        L"Resources/shaders/GrassBlade.VS.hlsl", L"vs_6_0",
        dxCommon_->GetDxcUtils().Get(), dxCommon_->GetDxcCompiler(), dxCommon_->GetIncludeHandler(), logStream);
    auto psBlob = dxCommon_->CompileShader(
        L"Resources/shaders/GrassBlade.PS.hlsl", L"ps_6_0",
        dxCommon_->GetDxcUtils().Get(), dxCommon_->GetDxcCompiler(), dxCommon_->GetIncludeHandler(), logStream);
    assert(vsBlob && psBlob);

    D3D12_INPUT_ELEMENT_DESC inputElems[3]{};
    inputElems[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(GrassBladeVertex, localPos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
    inputElems[1] = { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(GrassBladeVertex, normal),   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
    inputElems[2] = { "TEXCOORD", 0, DXGI_FORMAT_R32_FLOAT,       0, offsetof(GrassBladeVertex, heightT),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = rootSignature_.Get();
    psoDesc.InputLayout = { inputElems, _countof(inputElems) };
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };

    psoDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // 裏表両面描画
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

void FoliageField::PopulateOnTerrain(
    float areaSizeX,
    float areaSizeZ,
    float waterLevel,
    uint32_t targetCount,
    uint32_t seed,
    const std::function<float(float, float)>& heightFunc,
    const std::function<Vector3(float, float)>& normalFunc)
{
    const uint32_t cap = (std::min)(targetCount, maxInstances_);
    std::vector<GrassInstance> generated;
    generated.reserve(cap);

    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> distX(-areaSizeX * 0.5f, areaSizeX * 0.5f);
    std::uniform_real_distribution<float> distZ(-areaSizeZ * 0.5f, areaSizeZ * 0.5f);
    std::uniform_real_distribution<float> u01(0.0f, 1.0f);

    const uint32_t maxAttempts = cap * 4;
    for (uint32_t attempt = 0; attempt < maxAttempts && generated.size() < cap; ++attempt)
    {
        float x = distX(rng);
        float z = distZ(rng);
        float y = heightFunc(x, z);

        // 水中や水際ギリギリの濡れた砂浜には草を生やさない
        float aboveWater = y - waterLevel;
        if (aboveWater < 0.22f)
        {
            continue;
        }

        Vector3 n = normalFunc(x, z);
        if (n.y < 0.68f)
        {
            continue;
        }

        // クランプ（株）ノイズで疎密のある自然な草原をつくる
        float clump = ValueNoise2D(x * 0.24f + 5.1f, z * 0.24f - 3.4f) * 0.65f
                    + ValueNoise2D(x * 0.68f - 1.7f, z * 0.68f + 8.3f) * 0.35f;
        float shoreFade = std::clamp((aboveWater - 0.22f) / 0.55f, 0.0f, 1.0f);
        if (u01(rng) > (0.28f + 0.72f * clump) * shoreFade)
        {
            continue;
        }

        // 水辺に近い肥沃な場所ほど背が高く瑞々しい草にする
        float moistureBonus = 1.0f - std::clamp((aboveWater - 0.35f) / 2.2f, 0.0f, 0.42f);

        GrassInstance inst{};
        inst.position = { x, y - 0.03f, z };
        inst.height = (0.38f + 0.56f * clump + 0.18f * u01(rng)) * moistureBonus;
        inst.width = 0.032f + 0.020f * u01(rng);
        inst.yaw = u01(rng) * 2.0f * kPi;
        inst.bend = 0.18f + 0.32f * u01(rng);
        inst.colorVariation = std::clamp((1.0f - moistureBonus) * 0.7f + u01(rng) * 0.55f, 0.0f, 1.0f);
        generated.push_back(inst);
    }

    UploadInstances(generated);
}

void FoliageField::UploadInstances(const std::vector<GrassInstance>& instances)
{
    if (!mappedInstances_)
    {
        return;
    }
    const uint32_t count = static_cast<uint32_t>((std::min)(instances.size(), static_cast<size_t>(maxInstances_)));
    if (count > 0)
    {
        std::memcpy(mappedInstances_, instances.data(), sizeof(GrassInstance) * count);
    }
    uploadedInstanceCount_ = count;
    activeInstanceCount_ = count;
}

void FoliageField::SetActiveInstanceCount(uint32_t count)
{
    activeInstanceCount_ = (std::min)(count, uploadedInstanceCount_);
}

void FoliageField::Draw(
    ID3D12GraphicsCommandList* commandList,
    const Camera* camera,
    const Light* light,
    const WindSystem* wind)
{
    if (!commandList || !pipelineState_ || activeInstanceCount_ == 0 || !camera || !mappedConstants_)
    {
        return;
    }

    mappedConstants_->viewProj = Multiply(camera->GetViewMatrix(), camera->GetProjectionMatrix());
    mappedConstants_->baseColorRoot = rootColor_;
    mappedConstants_->sssIntensity = sssIntensity_;
    mappedConstants_->baseColorTip = tipColor_;
    mappedConstants_->maxDrawDistance = maxDrawDistance_;
    mappedConstants_->dryTipColor = dryColor_;
    mappedConstants_->specularIntensity = specularIntensity_;
    mappedConstants_->fogColor = fogColor_;
    mappedConstants_->fogDensity = fogDensity_;

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
    if (wind && wind->GetGpuVirtualAddress() != 0)
    {
        commandList->SetGraphicsRootConstantBufferView(3, wind->GetGpuVirtualAddress());
    }
    commandList->SetGraphicsRootDescriptorTable(4, instanceSrvGpu_);

    commandList->DrawIndexedInstanced(bladeIndexCount_, activeInstanceCount_, 0, 0, 0);
}

} // namespace BaziruEngine::Environment
