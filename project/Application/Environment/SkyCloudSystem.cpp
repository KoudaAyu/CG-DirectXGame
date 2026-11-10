#include "SkyCloudSystem.h"

#include "Baziru3_Engine/Core/Base/Allocator/ConstantBufferAllocator.h"
#include "Camera.h"
#include "DirectXCom.h"

#include <cmath>
#include <cstring>
#include <dxcapi.h>

namespace
{
    // 全UVスクロール速度の共通周期（40分 = 2400秒）。
    // 各レイヤーの速度を K / 2400.0f（Kは整数）に設定することで、
    // 長時間稼働時に time_ を 2400秒で折り返しても完全に継ぎ目ゼロ（整数UVシフト）を保証する。
    constexpr float    kSeamlessCycleSeconds     = 2400.0f;
    constexpr size_t   kConstantBufferAlignment  = 16;
    constexpr size_t   kVertexPositionComponents = 4;
    constexpr size_t   kCubeVertexCount          = 8;
    constexpr uint32_t kCubeIndexCount           = 36;
    constexpr float    kSkyBoxHalfExtent         = 50.0f;
    constexpr Vector3  kSkyBoxScale{ kSkyBoxHalfExtent, kSkyBoxHalfExtent, kSkyBoxHalfExtent };
    constexpr float    kCameraParallaxUvScale    = 0.00045f;

    struct SkyCloudVertex
    {
        float position[kVertexPositionComponents];
    };

    struct alignas(kConstantBufferAlignment) SkyCloudConstants
    {
        Matrix4x4 wvp;
        float time;
        float camOffsetX;
        float camOffsetZ;
        float pad0;
    };

    constexpr SkyCloudVertex kCubeVertices[kCubeVertexCount] = {
        { { -1.0f, -1.0f, -1.0f, 1.0f } },
        { { -1.0f,  1.0f, -1.0f, 1.0f } },
        { {  1.0f,  1.0f, -1.0f, 1.0f } },
        { {  1.0f, -1.0f, -1.0f, 1.0f } },
        { { -1.0f, -1.0f,  1.0f, 1.0f } },
        { { -1.0f,  1.0f,  1.0f, 1.0f } },
        { {  1.0f,  1.0f,  1.0f, 1.0f } },
        { {  1.0f, -1.0f,  1.0f, 1.0f } },
    };

    constexpr uint32_t kCubeIndices[kCubeIndexCount] = {
        0, 1, 2,  0, 2, 3, // -Z
        4, 6, 5,  4, 7, 6, // +Z
        4, 5, 1,  4, 1, 0, // -X
        3, 2, 6,  3, 6, 7, // +X
        1, 5, 6,  1, 6, 2, // +Y
        4, 0, 3,  4, 3, 7  // -Y
    };

    bool UploadToBuffer(ID3D12Resource* buffer, const void* srcData, size_t byteSize)
    {
        if (!buffer || !srcData) return false;
        void* mapped = nullptr;
        if (FAILED(buffer->Map(0, nullptr, &mapped)) || !mapped) return false;
        std::memcpy(mapped, srcData, byteSize);
        buffer->Unmap(0, nullptr);
        return true;
    }

    Microsoft::WRL::ComPtr<IDxcBlob> CompileInlineHLSL(
        DirectXCom* dx,
        const char* hlslSource,
        const wchar_t* entryPoint,
        const wchar_t* targetProfile)
    {
        if (!dx || !dx->GetDxcCompiler() || !hlslSource)
        {
            return nullptr;
        }

        DxcBuffer srcBuf{};
        srcBuf.Ptr = hlslSource;
        srcBuf.Size = std::strlen(hlslSource);
        srcBuf.Encoding = DXC_CP_UTF8;

        LPCWSTR args[] = {
            L"SkyCloudInline.hlsl",
            L"-E", entryPoint,
            L"-T", targetProfile,
            L"-O3",
            L"-Zpr"
        };

        Microsoft::WRL::ComPtr<IDxcResult> result;
        HRESULT hr = dx->GetDxcCompiler()->Compile(
            &srcBuf,
            args,
            _countof(args),
            nullptr,
            IID_PPV_ARGS(&result));
        if (FAILED(hr) || !result)
        {
            return nullptr;
        }

        Microsoft::WRL::ComPtr<IDxcBlobUtf8> errBlob;
        result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errBlob), nullptr);
        if (errBlob && errBlob->GetStringLength() > 0)
        {
            OutputDebugStringA("[SkyCloudSystem] HLSL Compile Message:\n");
            OutputDebugStringA(errBlob->GetStringPointer());
        }

        HRESULT statusHr = S_OK;
        result->GetStatus(&statusHr);
        if (FAILED(statusHr))
        {
            return nullptr;
        }

        Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob;
        hr = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
        if (FAILED(hr))
        {
            return nullptr;
        }
        return shaderBlob;
    }

    constexpr const char* kSkyCloudHLSL = R"HLSL(
cbuffer SkyCloudCB : register(b0)
{
    float4x4 gWVP;
    float    gTime;
    float    gCamOffsetX;
    float    gCamOffsetZ;
    float    gPad0;
};

Texture2D<float4> gCloudTex : register(t0);
SamplerState      gSampler  : register(s0);

struct VSInput
{
    float4 position : POSITION0;
};

struct PSInput
{
    float4 svPos    : SV_POSITION;
    float3 localDir : TEXCOORD0;
};

PSInput VSMain(VSInput input)
{
    PSInput o;
    // z = w として最遠方深度 (1.0) に配置し、手前の全3Dオブジェクトに確実に遮蔽させる
    o.svPos    = mul(input.position, gWVP).xyww;
    o.localDir = input.position.xyz;
    return o;
}

float SrgbToLinear1(float c)
{
    c = saturate(c);
    return (c <= 0.04045f) ? (c / 12.92f) : pow(max((c + 0.055f) / 1.055f, 1e-6f), 2.4f);
}

float3 SrgbToLinear(float3 c)
{
    return float3(SrgbToLinear1(c.r), SrgbToLinear1(c.g), SrgbToLinear1(c.b));
}

float ComputeShellDist(float dyEff, float Hc)
{
    const float Re = 14.0f;
    float b = Re * dyEff;
    return sqrt(b * b + 2.0f * Re * Hc + Hc * Hc) - b;
}

float2 EvalCumulusDensity(
    float2 pu,
    float2 ddxBase,
    float2 ddyBase,
    float2 windM,
    float2 windP,
    float2 windD,
    float2 windF,
    float coverageMin)
{
    float m   = gCloudTex.SampleGrad(gSampler, pu + windM,         ddxBase,         ddyBase).r;
    float pf  = gCloudTex.SampleGrad(gSampler, pu * 1.65f + windP, ddxBase * 1.65f, ddyBase * 1.65f).g;
    float dt  = gCloudTex.SampleGrad(gSampler, pu * 2.85f + windD, ddxBase * 2.85f, ddyBase * 2.85f).b;
    float dt2 = gCloudTex.SampleGrad(gSampler, pu * 5.70f + windF, ddxBase * 5.70f, ddyBase * 5.70f).b;

    float combined  = m * 0.68f + pf * 0.32f;
    float baseShape = smoothstep(coverageMin, coverageMin + 0.34f, combined);
    float micro     = pf * 0.52f + dt * 0.34f + dt2 * 0.14f;
    float erode     = (1.0f - micro) * 0.22f * (1.0f - baseShape * 0.65f);
    float dens      = saturate((baseShape - erode) * 1.22f);
    return float2(dens, pf);
}

void AccumulateCumulusLayer(
    inout float3 accumPremulLinear,
    inout float  accumAlpha,
    float3 rayDir,
    float  tS,
    float2 baseUV,
    float2 ddxBase,
    float2 ddyBase,
    float2 windM,
    float2 windP,
    float2 windD,
    float2 windF,
    float  coverageMin,
    float  layerOpacity,
    float  sunDot,
    float3 sunDir,
    float3 horizonMistSrgb)
{
    float2 cur = EvalCumulusDensity(baseUV, ddxBase, ddyBase, windM, windP, windD, windF, coverageMin);
    float dens = cur.x;
    float puffLobe = cur.y;

    if (dens > 0.002f)
    {
        float2 sunStep = sunDir.xz * 0.032f;
        float dSunNear = EvalCumulusDensity(baseUV + sunStep * 0.55f, ddxBase, ddyBase, windM, windP, windD, windF, coverageMin).x;
        float dSunFar  = EvalCumulusDensity(baseUV + sunStep * 1.35f, ddxBase, ddyBase, windM, windP, windD, windF, coverageMin).x;

        // Beer-Lambert 多段階セルフシャドウ + Worley パフドーム立体ハイライト
        float occ  = saturate((dSunNear * 0.45f + dSunFar * 0.55f) * 1.15f - dens * 0.18f);
        float beer = exp(-occ * 1.55f);
        float puffHighlight = smoothstep(0.30f, 0.85f, puffLobe) * 0.24f;
        float lit  = saturate(beer * 0.82f + puffHighlight + (1.0f - dens) * 0.14f);

        // 空の散乱光を受けた淡いパウダーブルーの雲底から、太陽光を浴びた温かいクリームホワイトの雲頂へ補間
        float3 bellySrgb = float3(174.0f / 255.0f, 195.0f / 255.0f, 220.0f / 255.0f);
        float3 crownSrgb = float3(253.0f / 255.0f, 251.0f / 255.0f, 246.0f / 255.0f);
        float3 colSrgb   = lerp(bellySrgb, crownSrgb, lit);

        // 太陽付近の雲エッジが白金色に輝く前方散乱（シルバーライニング）
        float sd2    = sunDot * sunDot;
        float sd6    = sd2 * sd2 * sd2;
        float silver = sd6 * (1.0f - dens * 0.65f) * 0.45f;
        colSrgb = saturate(colSrgb + silver * float3(24.0f / 255.0f, 19.0f / 255.0f, 10.0f / 255.0f));

        // 遠方の地平線ミストへの大気遠近フェード（duckov_skybox.dds の地平線色と完全一致）
        float distHaze = saturate((tS - 2.4f) / 6.2f);
        distHaze = distHaze * distHaze * 0.76f;
        colSrgb  = lerp(colSrgb, horizonMistSrgb, distHaze);

        float hFade = smoothstep(-0.035f, 0.085f, rayDir.y);
        float alpha = smoothstep(0.0f, 0.55f, dens) * layerOpacity * hFade * (1.0f - distHaze * 0.35f);

        float3 colLinear  = SrgbToLinear(colSrgb);
        accumPremulLinear = accumPremulLinear * (1.0f - alpha) + colLinear * alpha;
        accumAlpha        = accumAlpha * (1.0f - alpha) + alpha;
    }
}

float4 PSMain(PSInput input) : SV_TARGET0
{
    float3 rayDir = normalize(input.localDir);
    float  dyEff  = max(rayDir.y + 0.055f, 0.025f);

    // 動的分岐の外側（均一制御フロー）で各高度シェルのベースUVと偏微分を算出
    float tCir  = ComputeShellDist(dyEff, 2.60f);
    float tMain = ComputeShellDist(dyEff, 1.45f);
    float tLow  = ComputeShellDist(dyEff, 0.95f);

    float2 camParallax = float2(gCamOffsetX, gCamOffsetZ);
    float2 pCir  = rayDir.xz * (tCir  * 0.075f) + camParallax * 0.18f;
    float2 pMain = rayDir.xz * (tMain * 0.092f) + camParallax * 0.45f;
    float2 pLow  = rayDir.xz * (tLow  * 0.115f) + camParallax * 0.85f + float2(0.41f, 0.29f);

    float2 ddxCir  = ddx(pCir),  ddyCir  = ddy(pCir);
    float2 ddxMain = ddx(pMain), ddyMain = ddy(pMain);
    float2 ddxLow  = ddx(pLow),  ddyLow  = ddy(pLow);

    // 地平線より下はテクスチャフェッチなしで即座に透過終了
    if (rayDir.y <= -0.040f)
    {
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    const float3 sunDir = float3(0.4216f, 0.7830f, -0.4574f);
    const float3 horizonMistSrgb = float3(188.0f / 255.0f, 212.0f / 255.0f, 232.0f / 255.0f);
    float sunDot = saturate(dot(rayDir, sunDir));

    // 2400秒周期の整数比風速ベクトル（継ぎ目ゼロ保証）
    const float invP = 1.0f / 2400.0f;
    float t = gTime;

    float3 accumPremulLinear = float3(0.0f, 0.0f, 0.0f);
    float  accumAlpha        = 0.0f;

    // -----------------------------------------------------------------
    // Layer 0: 上空の繊細な巻雲（ゆっくりとした高高度気流）
    // -----------------------------------------------------------------
    {
        float2 wC1 = float2(t * (12.0f * invP), t * ( 5.0f * invP));
        float2 wC2 = float2(t * (17.0f * invP), t * (-4.0f * invP));
        float c1 = gCloudTex.SampleGrad(gSampler, pCir + wC1, ddxCir, ddyCir).a;
        float c2 = gCloudTex.SampleGrad(gSampler, pCir * 1.70f + float2(0.23f, 0.11f) + wC2, ddxCir * 1.70f, ddyCir * 1.70f).a;
        float cir = smoothstep(0.50f, 0.86f, c1 * 0.65f + c2 * 0.35f);
        if (cir > 0.002f)
        {
            float hFade    = smoothstep(-0.02f, 0.12f, rayDir.y);
            float distFade = 1.0f - saturate((tCir - 3.5f) / 8.5f) * 0.65f;
            float aCir     = cir * 0.36f * hFade * distFade;
            float3 cirSrgb = lerp(float3(246.0f / 255.0f, 249.0f / 255.0f, 253.0f / 255.0f), horizonMistSrgb, (1.0f - distFade) * 0.6f);
            float3 cirLin  = SrgbToLinear(cirSrgb);
            accumPremulLinear = accumPremulLinear * (1.0f - aCir) + cirLin * aCir;
            accumAlpha        = accumAlpha * (1.0f - aCir) + aCir;
        }
    }

    // -----------------------------------------------------------------
    // Layer 1: 主積雲デッキ（豊かな立体感と穏やかな変形を伴う中層風）
    // -----------------------------------------------------------------
    AccumulateCumulusLayer(
        accumPremulLinear,
        accumAlpha,
        rayDir,
        tMain,
        pMain,
        ddxMain,
        ddyMain,
        float2(t * (32.0f * invP), t * ( 15.0f * invP)),
        float2(t * (45.0f * invP), t * ( 28.0f * invP)),
        float2(t * (58.0f * invP), t * (-16.0f * invP)),
        float2(t * (72.0f * invP), t * ( 24.0f * invP)),
        0.43f,
        0.94f,
        sunDot,
        sunDir,
        horizonMistSrgb);

    // -----------------------------------------------------------------
    // Layer 2: 低層ちぎり積雲（手前を速く流れる視差パララックス層）
    // -----------------------------------------------------------------
    AccumulateCumulusLayer(
        accumPremulLinear,
        accumAlpha,
        rayDir,
        tLow,
        pLow,
        ddxLow,
        ddyLow,
        float2(t * (56.0f * invP), t * ( 26.0f * invP)),
        float2(t * (77.0f * invP), t * ( 45.0f * invP)),
        float2(t * (96.0f * invP), t * (-23.0f * invP)),
        float2(t * (118.0f * invP), t * (37.0f * invP)),
        0.52f,
        0.78f,
        sunDot,
        sunDir,
        horizonMistSrgb);

    return float4(accumPremulLinear, accumAlpha);
}
)HLSL";
} // namespace

bool SkyCloudSystem::Initialize(DirectXCom* dx)
{
    Finalize();
    if (!dx || !dx->GetDevice())
    {
        return false;
    }
    dx_ = dx;
    time_ = 0.0f;

    if (TextureManager::GetInstance())
    {
        cloudNoiseTexIndex_ = TextureManager::GetInstance()->Load("Resources/duckov_cloud_noise.dds");
    }
    if (cloudNoiseTexIndex_ == TextureManager::kInvalidTextureIndex)
    {
        return false;
    }

    if (!CreateGeometry() || !CreatePipeline())
    {
        Finalize();
        return false;
    }

    initialized_ = true;
    return true;
}

void SkyCloudSystem::Finalize()
{
    initialized_ = false;
    pipelineState_.Reset();
    rootSignature_.Reset();
    vertexBuffer_.Reset();
    indexBuffer_.Reset();
    vbView_ = {};
    ibView_ = {};
    cloudNoiseTexIndex_ = TextureManager::kInvalidTextureIndex;
    dx_ = nullptr;
}

void SkyCloudSystem::Update(float deltaTime)
{
    if (!initialized_)
    {
        return;
    }
    time_ += deltaTime;
    if (time_ >= kSeamlessCycleSeconds)
    {
        time_ -= kSeamlessCycleSeconds;
    }
}

bool SkyCloudSystem::CreateGeometry()
{
    vertexBuffer_ = dx_->CreateBufferResource(dx_->GetDevice(), sizeof(kCubeVertices));
    indexBuffer_  = dx_->CreateBufferResource(dx_->GetDevice(), sizeof(kCubeIndices));
    if (!UploadToBuffer(vertexBuffer_.Get(), kCubeVertices, sizeof(kCubeVertices)) ||
        !UploadToBuffer(indexBuffer_.Get(), kCubeIndices, sizeof(kCubeIndices)))
    {
        return false;
    }

    vbView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
    vbView_.SizeInBytes    = sizeof(kCubeVertices);
    vbView_.StrideInBytes  = sizeof(SkyCloudVertex);

    ibView_.BufferLocation = indexBuffer_->GetGPUVirtualAddress();
    ibView_.SizeInBytes    = sizeof(kCubeIndices);
    ibView_.Format         = DXGI_FORMAT_R32_UINT;
    return true;
}

bool SkyCloudSystem::CreatePipeline()
{
    auto* device = dx_->GetDevice().Get();

    D3D12_DESCRIPTOR_RANGE srvRange{};
    srvRange.RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors                    = 1;
    srvRange.BaseShaderRegister                = 0; // t0
    srvRange.RegisterSpace                     = 0;
    srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParams[2]{};
    // [0] b0: SkyCloudCB (VS + PS)
    rootParams[0].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[0].ShaderVisibility          = D3D12_SHADER_VISIBILITY_ALL;
    rootParams[0].Descriptor.ShaderRegister = 0;
    rootParams[0].Descriptor.RegisterSpace  = 0;

    // [1] t0: duckov_cloud_noise.dds (PS)
    rootParams[1].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[1].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[1].DescriptorTable.pDescriptorRanges   = &srvRange;

    D3D12_STATIC_SAMPLER_DESC samplerDesc{};
    samplerDesc.Filter           = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU         = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.AddressV         = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.AddressW         = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.ComparisonFunc   = D3D12_COMPARISON_FUNC_NEVER;
    samplerDesc.MaxLOD           = D3D12_FLOAT32_MAX;
    samplerDesc.ShaderRegister   = 0; // s0
    samplerDesc.RegisterSpace    = 0;
    samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rsDesc{};
    rsDesc.NumParameters     = _countof(rootParams);
    rsDesc.pParameters       = rootParams;
    rsDesc.NumStaticSamplers = 1;
    rsDesc.pStaticSamplers   = &samplerDesc;
    rsDesc.Flags             = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> sigBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errBlob;
    HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob);
    if (FAILED(hr))
    {
        return false;
    }
    hr = device->CreateRootSignature(
        0,
        sigBlob->GetBufferPointer(),
        sigBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature_));
    if (FAILED(hr))
    {
        return false;
    }

    auto vsBlob = CompileInlineHLSL(dx_, kSkyCloudHLSL, L"VSMain", L"vs_6_0");
    auto psBlob = CompileInlineHLSL(dx_, kSkyCloudHLSL, L"PSMain", L"ps_6_0");
    if (!vsBlob || !psBlob)
    {
        return false;
    }

    D3D12_INPUT_ELEMENT_DESC inputElem{};
    inputElem.SemanticName         = "POSITION";
    inputElem.SemanticIndex        = 0;
    inputElem.Format               = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElem.InputSlot            = 0;
    inputElem.AlignedByteOffset    = 0;
    inputElem.InputSlotClass       = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    inputElem.InstanceDataStepRate = 0;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = rootSignature_.Get();
    psoDesc.InputLayout    = { &inputElem, 1 };
    psoDesc.VS             = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS             = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };

    // プリマルチプライドαブレンド（3層パララックス合成を1ドローコールで完全等価出力）
    auto& rtBlend                 = psoDesc.BlendState.RenderTarget[0];
    rtBlend.BlendEnable           = TRUE;
    rtBlend.SrcBlend              = D3D12_BLEND_ONE;
    rtBlend.DestBlend             = D3D12_BLEND_INV_SRC_ALPHA;
    rtBlend.BlendOp               = D3D12_BLEND_OP_ADD;
    rtBlend.SrcBlendAlpha         = D3D12_BLEND_ONE;
    rtBlend.DestBlendAlpha        = D3D12_BLEND_INV_SRC_ALPHA;
    rtBlend.BlendOpAlpha          = D3D12_BLEND_OP_ADD;
    rtBlend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    psoDesc.SampleMask                        = D3D12_DEFAULT_SAMPLE_MASK;
    psoDesc.RasterizerState.CullMode          = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.FillMode          = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.DepthClipEnable   = TRUE;

    // 深度書き込み無効（DepthBasedOutline の輪郭誤検出を防止し、全3Dモデルを雲の手前に正しく描画）
    psoDesc.DepthStencilState.DepthEnable    = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    psoDesc.DepthStencilState.DepthFunc      = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    psoDesc.DepthStencilState.StencilEnable  = FALSE;

    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets      = 1;
    psoDesc.RTVFormats[0]         = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    psoDesc.DSVFormat             = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.SampleDesc.Count      = 1;

    hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
    return SUCCEEDED(hr);
}

void SkyCloudSystem::Draw(ID3D12GraphicsCommandList* cmdList, Camera* camera)
{
    if (!initialized_ || !dx_ || !cmdList || !camera ||
        cloudNoiseTexIndex_ == TextureManager::kInvalidTextureIndex || !TextureManager::GetInstance())
    {
        return;
    }

    auto* cbAllocator = dx_->GetCBAllocator();
    if (!cbAllocator)
    {
        return;
    }

    const Vector3 camPos = camera->GetWorldPosition();
    const Matrix4x4 worldMatrix = MakeAffineMatrix(kSkyBoxScale, Vector3{ 0.0f, 0.0f, 0.0f }, camPos);
    const Matrix4x4 wvp = Multiply(worldMatrix, Multiply(camera->GetViewMatrix(), camera->GetProjectionMatrix()));

    SkyCloudConstants cbData{};
    cbData.wvp        = wvp;
    cbData.time       = time_;
    cbData.camOffsetX = camPos.x * kCameraParallaxUvScale;
    cbData.camOffsetZ = camPos.z * kCameraParallaxUvScale;
    cbData.pad0       = 0.0f;

    auto alloc = cbAllocator->Allocate(sizeof(SkyCloudConstants));
    if (!alloc.cpuAddress || alloc.gpuAddress == 0)
    {
        return;
    }
    std::memcpy(alloc.cpuAddress, &cbData, sizeof(SkyCloudConstants));

    D3D12_GPU_DESCRIPTOR_HANDLE srvHandle =
        TextureManager::GetInstance()->GetSrvHandleGPU(cloudNoiseTexIndex_);
    if (srvHandle.ptr == 0)
    {
        return;
    }

    cmdList->SetGraphicsRootSignature(rootSignature_.Get());
    cmdList->SetPipelineState(pipelineState_.Get());
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->IASetVertexBuffers(0, 1, &vbView_);
    cmdList->IASetIndexBuffer(&ibView_);
    cmdList->SetGraphicsRootConstantBufferView(0, alloc.gpuAddress);
    cmdList->SetGraphicsRootDescriptorTable(1, srvHandle);
    cmdList->DrawIndexedInstanced(kCubeIndexCount, 1, 0, 0, 0);
}

