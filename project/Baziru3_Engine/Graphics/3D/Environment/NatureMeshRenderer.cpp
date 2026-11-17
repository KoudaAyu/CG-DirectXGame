#include "NatureMeshRenderer.h"
#include "Log.h"

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

float Fbm2D(float x, float z, int octaves = 4)
{
    float value = 0.0f;
    float amp = 0.5f;
    float freq = 1.0f;
    for (int i = 0; i < octaves; ++i)
    {
        value += amp * ValueNoise2D(x * freq, z * freq);
        freq *= 2.03f;
        amp *= 0.5f;
    }
    return value;
}

Vector3 Normalize3(const Vector3& v)
{
    float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len > 1e-5f)
    {
        return { v.x / len, v.y / len, v.z / len };
    }
    return { 0.0f, 1.0f, 0.0f };
}

Vector3 Cross3(const Vector3& a, const Vector3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

Vector3 Lerp3(const Vector3& a, const Vector3& b, float t)
{
    return {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t
    };
}

void AppendTaperedLimb(
    const Vector3& p0, const Vector3& p1,
    float r0, float r1, int radialSegs,
    float wind0, float wind1,
    const Vector3& barkColor,
    std::vector<NatureVertex>& verts,
    std::vector<uint32_t>& indices)
{
    Vector3 axis = Normalize3(p1 - p0);
    Vector3 upRef = (std::abs(axis.y) < 0.92f) ? Vector3{ 0.0f, 1.0f, 0.0f } : Vector3{ 1.0f, 0.0f, 0.0f };
    Vector3 right = Normalize3(Cross3(upRef, axis));
    Vector3 forward = Normalize3(Cross3(axis, right));

    uint32_t baseIdx = static_cast<uint32_t>(verts.size());
    for (int i = 0; i <= radialSegs; ++i)
    {
        float u = static_cast<float>(i) / static_cast<float>(radialSegs);
        float ang = u * 2.0f * kPi;
        float c = std::cos(ang);
        float s = std::sin(ang);
        Vector3 radialDir = {
            right.x * c + forward.x * s,
            right.y * c + forward.y * s,
            right.z * c + forward.z * s
        };

        NatureVertex vb{};
        vb.position = p0 + radialDir * r0;
        vb.normal = radialDir;
        vb.uv = { u, 0.0f };
        vb.color = { barkColor.x, barkColor.y, barkColor.z, 1.0f };
        vb.params = { wind0, 0.0f, 0.12f, 0.78f };

        NatureVertex vt{};
        vt.position = p1 + radialDir * r1;
        vt.normal = radialDir;
        vt.uv = { u, 1.0f };
        vt.color = { barkColor.x * 1.06f, barkColor.y * 1.05f, barkColor.z * 1.04f, 1.0f };
        vt.params = { wind1, 0.0f, 0.04f, 0.90f };

        verts.push_back(vb);
        verts.push_back(vt);
    }

    for (int i = 0; i < radialSegs; ++i)
    {
        uint32_t i0 = baseIdx + static_cast<uint32_t>(i * 2);
        uint32_t i1 = i0 + 1;
        uint32_t i2 = i0 + 2;
        uint32_t i3 = i0 + 3;

        indices.push_back(i0);
        indices.push_back(i1);
        indices.push_back(i2);

        indices.push_back(i2);
        indices.push_back(i1);
        indices.push_back(i3);
    }
}
} // namespace

void NatureMeshRenderer::Initialize(DirectXCom* dxCommon, std::ostream& logStream)
{
    dxCommon_ = dxCommon;
    if (!dxCommon_)
    {
        return;
    }

    CreatePipeline(logStream);

    const size_t slotStride = sizeof(BotanicalDrawConstants);
    static_assert(slotStride % 256 == 0, "BotanicalDrawConstants must be 256-byte aligned");
    drawConstantsBuffer_ = dxCommon_->CreateBufferResource(
        dxCommon_->GetDevice().Get(), slotStride * kMaxDrawsPerFrame);
    drawConstantsBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedConstantsBase_));
    currentDrawSlot_ = 0;
}

void NatureMeshRenderer::Finalize()
{
    if (drawConstantsBuffer_)
    {
        drawConstantsBuffer_->Unmap(0, nullptr);
        mappedConstantsBase_ = nullptr;
        drawConstantsBuffer_.Reset();
    }
    pipelineState_.Reset();
    rootSignature_.Reset();
    dxCommon_ = nullptr;
}

void NatureMeshRenderer::CreatePipeline(std::ostream& logStream)
{
    D3D12_DESCRIPTOR_RANGE srvRange{};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 1;
    srvRange.BaseShaderRegister = 0;
    srvRange.RegisterSpace = 0;
    srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParams[5]{};
    // 0: b0 DrawConstants (VS/PS)
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

    // 3: b3 WindForGPU (VS/PS)
    rootParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParams[3].Descriptor.ShaderRegister = 3;

    // 4: t0 Environment Cubemap (PS)
    rootParams[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParams[4].DescriptorTable.pDescriptorRanges = &srvRange;
    rootParams[4].DescriptorTable.NumDescriptorRanges = 1;

    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rsDesc{};
    rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    rsDesc.pParameters = rootParams;
    rsDesc.NumParameters = _countof(rootParams);
    rsDesc.pStaticSamplers = &sampler;
    rsDesc.NumStaticSamplers = 1;

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
        L"Resources/shaders/Botanical.VS.hlsl", L"vs_6_0",
        dxCommon_->GetDxcUtils().Get(), dxCommon_->GetDxcCompiler(), dxCommon_->GetIncludeHandler(), logStream);
    auto psBlob = dxCommon_->CompileShader(
        L"Resources/shaders/Botanical.PS.hlsl", L"ps_6_0",
        dxCommon_->GetDxcUtils().Get(), dxCommon_->GetDxcCompiler(), dxCommon_->GetIncludeHandler(), logStream);
    assert(vsBlob && psBlob);

    D3D12_INPUT_ELEMENT_DESC inputElems[5]{};
    inputElems[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, offsetof(NatureVertex, position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
    inputElems[1] = { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, offsetof(NatureVertex, normal),   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
    inputElems[2] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, offsetof(NatureVertex, uv),       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
    inputElems[3] = { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(NatureVertex, color),    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
    inputElems[4] = { "TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(NatureVertex, params),   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = rootSignature_.Get();
    psoDesc.InputLayout = { inputElems, _countof(inputElems) };
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };

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

NatureMeshHandle NatureMeshRenderer::UploadMesh(
    DirectXCom* dxCommon,
    const std::vector<NatureVertex>& vertices,
    const std::vector<uint32_t>& indices)
{
    NatureMeshHandle handle{};
    if (!dxCommon || vertices.empty() || indices.empty())
    {
        return handle;
    }

    const size_t vbBytes = sizeof(NatureVertex) * vertices.size();
    handle.vertexBuffer = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), vbBytes);
    void* vbMapped = nullptr;
    handle.vertexBuffer->Map(0, nullptr, &vbMapped);
    std::memcpy(vbMapped, vertices.data(), vbBytes);
    handle.vertexBuffer->Unmap(0, nullptr);

    handle.vbv.BufferLocation = handle.vertexBuffer->GetGPUVirtualAddress();
    handle.vbv.SizeInBytes = static_cast<UINT>(vbBytes);
    handle.vbv.StrideInBytes = sizeof(NatureVertex);

    const size_t ibBytes = sizeof(uint32_t) * indices.size();
    handle.indexBuffer = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), ibBytes);
    void* ibMapped = nullptr;
    handle.indexBuffer->Map(0, nullptr, &ibMapped);
    std::memcpy(ibMapped, indices.data(), ibBytes);
    handle.indexBuffer->Unmap(0, nullptr);

    handle.ibv.BufferLocation = handle.indexBuffer->GetGPUVirtualAddress();
    handle.ibv.SizeInBytes = static_cast<UINT>(ibBytes);
    handle.ibv.Format = DXGI_FORMAT_R32_UINT;
    handle.indexCount = static_cast<uint32_t>(indices.size());
    return handle;
}

NatureMeshHandle NatureMeshRenderer::CreateRockMeshFromProcedural(
    DirectXCom* dxCommon,
    const BioProcedural::MeshData& srcMesh,
    const Vector3& rockColor)
{
    std::vector<NatureVertex> verts;
    verts.reserve(srcMesh.vertices.size());

    for (const auto& sv : srcMesh.vertices)
    {
        NatureVertex nv{};
        nv.position = { sv.position.x, sv.position.y, sv.position.z };
        nv.normal = { sv.normal.x, sv.normal.y, sv.normal.z };
        nv.uv = { sv.texcoord.u * 4.0f, sv.texcoord.v * 4.0f };

        float radialLen = std::sqrt(sv.position.x * sv.position.x + sv.position.y * sv.position.y + sv.position.z * sv.position.z);
        float creviceAO = std::clamp(0.48f + radialLen * 0.45f, 0.35f, 1.0f);
        float grain = 0.88f + 0.24f * Fbm2D(sv.position.x * 3.1f + 5.0f, sv.position.z * 3.1f - 2.0f, 3);

        nv.color = {
            rockColor.x * grain,
            rockColor.y * grain,
            rockColor.z * grain,
            1.0f
        };
        // sv.color.r には BioProceduralGenerator::GenerateRock が焼き付けた苔ウェイトが入っている
        nv.params = { 0.0f, 0.0f, sv.color.r, creviceAO };
        verts.push_back(nv);
    }

    return UploadMesh(dxCommon, verts, srcMesh.indices);
}

float NatureMeshRenderer::EvaluateRiverCenterX(float z)
{
    return 4.2f * std::sin(z * 0.072f) + 1.5f * std::cos(z * 0.145f + 0.4f);
}

float NatureMeshRenderer::EvaluateRiversideHeight(float x, float z)
{
    const float riverCenter = EvaluateRiverCenterX(z);
    const float distToRiver = std::abs(x - riverCenter);
    const float riverHalfWidth = 5.6f + 1.1f * std::sin(z * 0.11f + 0.6f);

    // 蛇行の内側を浅瀬（砂州）、外側をやや深い淵にする非対称プロファイル
    const float bendSign = std::cos(z * 0.072f);
    const float sideBias = ((x - riverCenter) * bendSign > 0.0f) ? 0.72f : 1.12f;

    // 川の断面プロファイル (0 = 川底中心, 1 = 岸の上)
    float tBank = std::clamp(distToRiver / riverHalfWidth, 0.0f, 1.8f);
    float channelCarve = 0.0f;
    if (tBank < 1.0f)
    {
        float smooth = tBank * tBank * (3.0f - 2.0f * tBank);
        channelCarve = -1.68f * sideBias * (1.0f - smooth);
    }

    // 岸から外側へなだらかに隆起する草原の丘陵
    float meadowRise = std::clamp((distToRiver - riverHalfWidth) * 0.14f, 0.0f, 2.8f);
    float rollingHills = (Fbm2D(x * 0.055f + 11.3f, z * 0.055f - 7.1f, 4) - 0.45f) * 2.4f;
    float microDetail = (Fbm2D(x * 0.32f, z * 0.32f, 3) - 0.5f) * 0.22f;

    // 水際では丘陵ノイズを穏やかにして自然な砂浜・河原を作る
    float bankBlend = std::clamp((distToRiver - riverHalfWidth * 0.75f) / 5.5f, 0.0f, 1.0f);
    float baseElevation = 0.26f + meadowRise + rollingHills * bankBlend + microDetail;

    return baseElevation + channelCarve;
}

Vector3 NatureMeshRenderer::EvaluateRiversideNormal(float x, float z)
{
    const float eps = 0.25f;
    float hL = EvaluateRiversideHeight(x - eps, z);
    float hR = EvaluateRiversideHeight(x + eps, z);
    float hD = EvaluateRiversideHeight(x, z - eps);
    float hU = EvaluateRiversideHeight(x, z + eps);
    return Normalize3({ hL - hR, 2.0f * eps, hD - hU });
}

void NatureMeshRenderer::BuildRiversideTerrainMesh(
    float sizeX, float sizeZ, int gridX, int gridZ, float waterLevel,
    std::vector<NatureVertex>& outVertices, std::vector<uint32_t>& outIndices)
{
    outVertices.clear();
    outIndices.clear();
    outVertices.reserve(static_cast<size_t>((gridX + 1) * (gridZ + 1)));
    outIndices.reserve(static_cast<size_t>(gridX * gridZ * 6));

    const Vector3 riverbedSand  = { 0.52f, 0.46f, 0.35f };
    const Vector3 shorePebble   = { 0.42f, 0.39f, 0.33f };
    const Vector3 lushTurfGreen = { 0.15f, 0.31f, 0.10f };
    const Vector3 hillLoamGreen = { 0.21f, 0.36f, 0.13f };

    for (int iz = 0; iz <= gridZ; ++iz)
    {
        float vz = static_cast<float>(iz) / static_cast<float>(gridZ);
        float z = (vz - 0.5f) * sizeZ;

        for (int ix = 0; ix <= gridX; ++ix)
        {
            float vx = static_cast<float>(ix) / static_cast<float>(gridX);
            float x = (vx - 0.5f) * sizeX;
            float y = EvaluateRiversideHeight(x, z);
            Vector3 n = EvaluateRiversideNormal(x, z);

            float heightAboveWater = y - waterLevel;
            float noisePatch = Fbm2D(x * 0.22f + 3.7f, z * 0.22f + 9.2f, 3);

            // 高さに応じて「川底の砂礫 → 水際の小石 → 豊かな草地土壌」へ自然に遷移
            Vector3 col = riverbedSand;
            float mossW = 0.0f;
            if (heightAboveWater < -0.15f)
            {
                float deepT = std::clamp((-heightAboveWater - 0.15f) / 1.4f, 0.0f, 1.0f);
                col = Lerp3(shorePebble, riverbedSand * (0.85f + 0.3f * noisePatch), deepT);
            }
            else if (heightAboveWater < 0.38f)
            {
                float t = std::clamp((heightAboveWater + 0.15f) / 0.53f, 0.0f, 1.0f);
                col = Lerp3(shorePebble, lushTurfGreen, t * t);
                mossW = t * 0.25f;
            }
            else
            {
                float hillT = std::clamp((heightAboveWater - 0.38f) / 2.2f, 0.0f, 1.0f);
                col = Lerp3(lushTurfGreen, hillLoamGreen, hillT * 0.6f + noisePatch * 0.4f);
                mossW = std::clamp((1.0f - n.y) * 1.6f + 0.15f * noisePatch, 0.0f, 0.65f);
            }

            float ao = std::clamp(0.68f + 0.32f * n.y, 0.45f, 1.0f);

            NatureVertex v{};
            v.position = { x, y, z };
            v.normal = n;
            v.uv = { x * 0.25f, z * 0.25f };
            v.color = { col.x, col.y, col.z, 1.0f };
            v.params = { 0.0f, 0.0f, mossW, ao };
            outVertices.push_back(v);
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

            outIndices.push_back(i0);
            outIndices.push_back(i2);
            outIndices.push_back(i1);

            outIndices.push_back(i1);
            outIndices.push_back(i2);
            outIndices.push_back(i3);
        }
    }
}

void NatureMeshRenderer::BuildRealisticTreeMesh(
    uint32_t seed, float heightScale, bool autumnTint,
    std::vector<NatureVertex>& outVertices, std::vector<uint32_t>& outIndices)
{
    outVertices.clear();
    outIndices.clear();

    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> u01(0.0f, 1.0f);
    std::uniform_real_distribution<float> uSigned(-1.0f, 1.0f);

    const Vector3 barkBase = autumnTint
        ? Vector3{ 0.34f, 0.27f, 0.22f }
        : Vector3{ 0.28f, 0.21f, 0.15f };

    // 1. 有機的にうねる主幹 (4セグメント)
    const float trunkH = 3.4f * heightScale;
    const float baseR = 0.26f * heightScale;
    Vector3 pPrev = { 0.0f, -0.15f, 0.0f };
    float leanX = uSigned(rng) * 0.28f * heightScale;
    float leanZ = uSigned(rng) * 0.28f * heightScale;

    std::vector<Vector3> crownAnchors;

    const int trunkSteps = 4;
    for (int s = 1; s <= trunkSteps; ++s)
    {
        float t0 = static_cast<float>(s - 1) / static_cast<float>(trunkSteps);
        float t1 = static_cast<float>(s) / static_cast<float>(trunkSteps);
        Vector3 pNext = {
            leanX * t1 * t1 + std::sin(t1 * 3.1f + static_cast<float>(seed)) * 0.08f * heightScale,
            trunkH * t1,
            leanZ * t1 * t1 + std::cos(t1 * 2.7f + static_cast<float>(seed)) * 0.08f * heightScale
        };
        float r0 = baseR * (1.0f - 0.58f * t0);
        float r1 = baseR * (1.0f - 0.58f * t1);
        AppendTaperedLimb(pPrev, pNext, r0, r1, 10, t0 * 0.12f, t1 * 0.16f, barkBase, outVertices, outIndices);
        pPrev = pNext;
    }

    Vector3 trunkTop = pPrev;
    Vector3 crownCenter = { trunkTop.x, trunkTop.y + 1.15f * heightScale, trunkTop.z };

    // 2. 放射状に広がる大枝＆小枝
    const int mainBranchCount = 6;
    for (int b = 0; b < mainBranchCount; ++b)
    {
        float azim = (static_cast<float>(b) + u01(rng) * 0.45f) * (2.0f * kPi / static_cast<float>(mainBranchCount));
        float startRatio = 0.58f + 0.36f * (static_cast<float>(b % 3) / 2.0f);
        Vector3 bStart = {
            trunkTop.x * startRatio,
            trunkH * startRatio,
            trunkTop.z * startRatio
        };
        float len = (1.45f + u01(rng) * 0.65f) * heightScale;
        float elev = 0.48f + u01(rng) * 0.35f;
        Vector3 dir = Normalize3({ std::cos(azim), elev, std::sin(azim) });
        Vector3 bMid = bStart + dir * (len * 0.55f);
        Vector3 bEnd = bMid + Normalize3(dir + Vector3{ uSigned(rng) * 0.25f, 0.22f, uSigned(rng) * 0.25f }) * (len * 0.55f);

        AppendTaperedLimb(bStart, bMid, 0.105f * heightScale, 0.062f * heightScale, 7, 0.14f, 0.28f, barkBase, outVertices, outIndices);
        AppendTaperedLimb(bMid, bEnd, 0.062f * heightScale, 0.026f * heightScale, 6, 0.28f, 0.48f, barkBase, outVertices, outIndices);

        crownAnchors.push_back(bMid + Vector3{ 0.0f, 0.25f * heightScale, 0.0f });
        crownAnchors.push_back(bEnd);

        // 二次小枝
        Vector3 twigDir = Normalize3(dir + Vector3{ -std::sin(azim) * 0.45f, 0.35f, std::cos(azim) * 0.45f });
        Vector3 twigEnd = bMid + twigDir * (len * 0.52f);
        AppendTaperedLimb(bMid, twigEnd, 0.048f * heightScale, 0.020f * heightScale, 5, 0.28f, 0.52f, barkBase, outVertices, outIndices);
        crownAnchors.push_back(twigEnd);
    }
    crownAnchors.push_back(crownCenter + Vector3{ 0.0f, 0.65f * heightScale, 0.0f });

    // 3. 球面法線トランスファー＆透過光(SSS)付きの有機的な葉クラスタ群
    for (size_t a = 0; a < crownAnchors.size(); ++a)
    {
        const Vector3& anchor = crownAnchors[a];
        const int cardsPerAnchor = 9;
        for (int c = 0; c < cardsPerAnchor; ++c)
        {
            Vector3 offset = {
                uSigned(rng) * 0.68f * heightScale,
                uSigned(rng) * 0.52f * heightScale,
                uSigned(rng) * 0.68f * heightScale
            };
            Vector3 center = anchor + offset;
            float cardSize = (0.52f + u01(rng) * 0.34f) * heightScale;

            // ランダムな葉カードの向き (交差配置でどの角度からも立体的)
            float yaw = u01(rng) * 2.0f * kPi;
            float pitch = uSigned(rng) * 0.75f;
            Vector3 right = { std::cos(yaw), std::sin(pitch) * 0.35f, std::sin(yaw) };
            right = Normalize3(right);
            Vector3 up = Normalize3({ -std::sin(yaw) * std::sin(pitch), std::cos(pitch), std::cos(yaw) * std::sin(pitch) });
            Vector3 faceNormal = Normalize3(Cross3(right, up));

            // 樹冠中心から外向きへの球面法線（ふわっとした柔らかい樹冠ライティングの要）
            Vector3 sphericalNormal = Normalize3(center - crownCenter);
            float distFromCrown = std::sqrt(
                (center.x - crownCenter.x) * (center.x - crownCenter.x) +
                (center.y - crownCenter.y) * (center.y - crownCenter.y) +
                (center.z - crownCenter.z) * (center.z - crownCenter.z));
            float outerFactor = std::clamp(distFromCrown / (2.2f * heightScale), 0.25f, 1.0f);

            // 葉色の自然な個体差
            float hueRand = u01(rng);
            Vector3 leafCol = autumnTint
                ? Lerp3({ 0.72f, 0.44f, 0.11f }, { 0.86f, 0.66f, 0.16f }, hueRand)
                : Lerp3({ 0.13f, 0.40f, 0.11f }, { 0.28f, 0.58f, 0.15f }, hueRand * outerFactor);

            float windW = std::clamp(0.45f + outerFactor * 0.55f, 0.35f, 1.0f);
            float ao = std::clamp(0.38f + outerFactor * 0.62f, 0.35f, 1.0f);

            uint32_t vBase = static_cast<uint32_t>(outVertices.size());
            const Vector2 corners[4] = { { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f } };
            const float sx[4] = { -1.0f, 1.0f, -1.0f, 1.0f };
            const float sy[4] = { -1.0f, -1.0f, 1.0f, 1.0f };

            for (int k = 0; k < 4; ++k)
            {
                Vector3 vPos = center + right * (sx[k] * cardSize) + up * (sy[k] * cardSize);
                Vector3 blendedN = Normalize3(Lerp3(faceNormal, sphericalNormal, 0.78f));

                NatureVertex lv{};
                lv.position = vPos;
                lv.normal = blendedN;
                lv.uv = corners[k];
                // alpha = 0.5f は Botanical.PS.hlsl に「プロシージャル葉シルエット切り抜き」を指示するフラグ
                lv.color = { leafCol.x, leafCol.y, leafCol.z, 0.5f };
                lv.params = { windW + (sy[k] > 0.0f ? 0.12f : 0.0f), 1.0f, 0.0f, ao };
                outVertices.push_back(lv);
            }

            outIndices.push_back(vBase + 0);
            outIndices.push_back(vBase + 2);
            outIndices.push_back(vBase + 1);

            outIndices.push_back(vBase + 1);
            outIndices.push_back(vBase + 2);
            outIndices.push_back(vBase + 3);
        }
    }
}

void NatureMeshRenderer::DrawMesh(
    ID3D12GraphicsCommandList* commandList,
    const NatureMeshHandle& mesh,
    const Matrix4x4& worldMatrix,
    const Camera* camera,
    const Light* light,
    const WindSystem* wind,
    D3D12_GPU_DESCRIPTOR_HANDLE envCubemapSrv,
    float waterLevel,
    float mossIntensity,
    float sssIntensity,
    const Vector3& fogColor,
    float fogDensity,
    const Vector4& tint)
{
    if (!commandList || !pipelineState_ || mesh.indexCount == 0 || !camera || !mappedConstantsBase_)
    {
        return;
    }
    if (currentDrawSlot_ >= kMaxDrawsPerFrame)
    {
        return;
    }

    const uint32_t slot = currentDrawSlot_++;
    auto* cb = reinterpret_cast<BotanicalDrawConstants*>(mappedConstantsBase_ + slot * sizeof(BotanicalDrawConstants));
    cb->world = worldMatrix;
    cb->viewProj = Multiply(camera->GetViewMatrix(), camera->GetProjectionMatrix());
    cb->worldInvTranspose = Transpose(Inverse(worldMatrix));
    cb->baseTint = tint;
    cb->waterLevel = waterLevel;
    cb->wetShoreHeight = 0.32f;
    cb->mossIntensity = mossIntensity;
    cb->sssIntensity = sssIntensity;
    cb->fogColor = fogColor;
    cb->fogDensity = fogDensity;

    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &mesh.vbv);
    commandList->IASetIndexBuffer(&mesh.ibv);

    D3D12_GPU_VIRTUAL_ADDRESS slotGpuAddr =
        drawConstantsBuffer_->GetGPUVirtualAddress() + static_cast<UINT64>(slot) * sizeof(BotanicalDrawConstants);
    commandList->SetGraphicsRootConstantBufferView(0, slotGpuAddr);

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
    if (envCubemapSrv.ptr != 0)
    {
        commandList->SetGraphicsRootDescriptorTable(4, envCubemapSrv);
    }

    commandList->DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);
}

} // namespace BaziruEngine::Environment
