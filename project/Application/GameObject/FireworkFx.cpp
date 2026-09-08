#include "FireworkFx.h"

#include "Baziru3_Engine/Core/Base/Allocator/ConstantBufferAllocator.h"
#include "Camera.h"
#include "DirectXCom.h"
#include "Matrix4x4.h"
#include "TextureManager.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>

namespace {

constexpr float kPi = 3.14159265358979323846f;

// ルートパラメータの並び
constexpr UINT kRootParamScene = 0;   // b0 (VS) : ViewProjection
constexpr UINT kRootParamTexture = 1; // t0 (PS) : パーティクルのテクスチャ

/// <summary>シェーダに渡すシーン定数（FireworkFx.VS.hlsl の SceneParams と一致させること）</summary>
struct SceneParams
{
    Matrix4x4 viewProjection;
};

float Rand01(std::mt19937& rng)
{
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(rng);
}

float RandRange(std::mt19937& rng, float minValue, float maxValue)
{
    std::uniform_real_distribution<float> dist(minValue, maxValue);
    return dist(rng);
}

Vector4 Lerp(const Vector4& a, const Vector4& b, float t)
{
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t,
            a.w + (b.w - a.w) * t};
}

float Dot(const Vector3& a, const Vector3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

Vector3 Cross(const Vector3& a, const Vector3& b)
{
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

Vector3 Normalize(const Vector3& v, const Vector3& fallback)
{
    const float lengthSquared = Dot(v, v);
    if (lengthSquared < 1e-8f)
    {
        return fallback;
    }
    const float inverse = 1.0f / std::sqrt(lengthSquared);
    return {v.x * inverse, v.y * inverse, v.z * inverse};
}

} // namespace

FireworkFx::~FireworkFx()
{
    Finalize();
}

// ===================================================================
// 初期化
// ===================================================================

void FireworkFx::Initialize(DirectXCom* dxCommon, Camera* camera, uint32_t capacity)
{
    dxCommon_ = dxCommon;
    camera_ = camera;

    if (!dxCommon_ || !dxCommon_->GetDevice() || capacity == 0)
    {
        return;
    }

    TextureManager* textureManager = TextureManager::GetInstance();
    softTextureIndex_ = textureManager->Load("Resources/CG4/circle2.png");
    sparkTextureIndex_ = textureManager->Load("Resources/starburst.png");

    particles_.assign(capacity, Particle{});
    pendingEmits_.reserve(256);

    if (!CreateRootSignature(dxCommon_))
    {
        return;
    }
    if (!CreatePipelineStates(dxCommon_))
    {
        return;
    }
    if (!CreateBuffers(dxCommon_, capacity))
    {
        return;
    }

    isReady_ = true;
    activeCount_ = 0;
    nextSearchIndex_ = 0;
}

bool FireworkFx::CreateRootSignature(DirectXCom* dxCommon)
{
    // b0 (VS) に ViewProjection、t0 (PS) にテクスチャ。それだけの小さいもの
    D3D12_DESCRIPTOR_RANGE descriptorRange[1]{};
    descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange[0].NumDescriptors = 1;
    descriptorRange[0].BaseShaderRegister = 0;
    descriptorRange[0].RegisterSpace = 0;
    descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[2]{};
    rootParameters[kRootParamScene].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[kRootParamScene].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[kRootParamScene].Descriptor.ShaderRegister = 0;

    rootParameters[kRootParamTexture].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[kRootParamTexture].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[kRootParamTexture].DescriptorTable.pDescriptorRanges = descriptorRange;
    rootParameters[kRootParamTexture].DescriptorTable.NumDescriptorRanges =
        _countof(descriptorRange);

    D3D12_STATIC_SAMPLER_DESC staticSamplers[1]{};
    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers[0].ShaderRegister = 0;
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC description{};
    description.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    description.pParameters = rootParameters;
    description.NumParameters = _countof(rootParameters);
    description.pStaticSamplers = staticSamplers;
    description.NumStaticSamplers = _countof(staticSamplers);

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&description, D3D_ROOT_SIGNATURE_VERSION_1,
                                             &signatureBlob, &errorBlob);
    if (FAILED(hr))
    {
        if (errorBlob)
        {
            std::cout << reinterpret_cast<const char*>(errorBlob->GetBufferPointer()) << std::endl;
        }
        return false;
    }

    hr = dxCommon->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(),
                                                    signatureBlob->GetBufferSize(),
                                                    IID_PPV_ARGS(&rootSignature_));
    return SUCCEEDED(hr);
}

bool FireworkFx::CreatePipelineStates(DirectXCom* dxCommon)
{
    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon->CompileShader(
        L"Resources/shaders/FireworkFx.VS.hlsl", L"vs_6_0", dxCommon->GetDxcUtils().Get(),
        dxCommon->GetDxcCompiler(), dxCommon->GetIncludeHandler(), std::cout);
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon->CompileShader(
        L"Resources/shaders/FireworkFx.PS.hlsl", L"ps_6_0", dxCommon->GetDxcUtils().Get(),
        dxCommon->GetDxcCompiler(), dxCommon->GetIncludeHandler(), std::cout);
    if (!vertexShaderBlob || !pixelShaderBlob)
    {
        return false;
    }

    // POSITION(float4) / TEXCOORD(float2) / COLOR(float4) = 40 バイト
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[3]{};
    inputElementDescs[0].SemanticName = "POSITION";
    inputElementDescs[0].SemanticIndex = 0;
    inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElementDescs[1].SemanticName = "TEXCOORD";
    inputElementDescs[1].SemanticIndex = 0;
    inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElementDescs[2].SemanticName = "COLOR";
    inputElementDescs[2].SemanticIndex = 0;
    inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
    inputLayoutDesc.pInputElementDescs = inputElementDescs;
    inputLayoutDesc.NumElements = _countof(inputElementDescs);

    D3D12_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = rootSignature_.Get();
    desc.InputLayout = inputLayoutDesc;
    desc.VS = {vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize()};
    desc.PS = {pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize()};
    desc.RasterizerState = rasterizerDesc;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.SampleDesc.Count = 1;
    desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    // デプスはテストするが書き込まない。粒同士が隠し合わず、3D の前後関係だけ効く
    desc.DepthStencilState.DepthEnable = TRUE;
    desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    D3D12_RENDER_TARGET_BLEND_DESC& blend = desc.BlendState.RenderTarget[0];
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    blend.BlendEnable = TRUE;
    blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blend.BlendOp = D3D12_BLEND_OP_ADD;
    blend.SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.DestBlendAlpha = D3D12_BLEND_ONE;

    // 加算合成
    blend.DestBlend = D3D12_BLEND_ONE;
    HRESULT hr = dxCommon->GetDevice()->CreateGraphicsPipelineState(
        &desc, IID_PPV_ARGS(&additivePipelineState_));
    if (FAILED(hr))
    {
        return false;
    }

    // アルファブレンド（DestBlend だけ差し替え）
    blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    hr = dxCommon->GetDevice()->CreateGraphicsPipelineState(&desc,
                                                            IID_PPV_ARGS(&alphaPipelineState_));
    return SUCCEEDED(hr);
}

bool FireworkFx::CreateBuffers(DirectXCom* dxCommon, uint32_t capacity)
{
    maxQuads_ = capacity;

    const size_t vertexBufferSize = sizeof(Vertex) * static_cast<size_t>(maxQuads_) * 4;

    // 頂点バッファは3枚。GPU が前フレームぶんをまだ読んでいる可能性があるので、
    // 毎フレーム書き込む先をずらす
    for (uint32_t i = 0; i < kFrameCount; ++i)
    {
        vertexResources_[i] = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(),
                                                             vertexBufferSize);
        if (!vertexResources_[i])
        {
            return false;
        }

        vertexBufferViews_[i].BufferLocation = vertexResources_[i]->GetGPUVirtualAddress();
        vertexBufferViews_[i].SizeInBytes = static_cast<UINT>(vertexBufferSize);
        vertexBufferViews_[i].StrideInBytes = sizeof(Vertex);

        // 書き込みっぱなしでマップしておく（アップロードヒープなので安全）
        vertexMaps_[i].reset(vertexResources_[i]);
        if (!vertexMaps_[i])
        {
            return false;
        }
    }

    // インデックスは中身が毎フレーム同じなので1本だけ作って使い回す。
    // 板 q 枚目 → 頂点 4q..4q+3
    const size_t indexBufferSize = sizeof(uint32_t) * static_cast<size_t>(maxQuads_) * 6;
    indexResource_ = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), indexBufferSize);
    if (!indexResource_)
    {
        return false;
    }

    indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    indexBufferView_.SizeInBytes = static_cast<UINT>(indexBufferSize);
    indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

    {
        Baziru3::ScopedMap<uint32_t> indexMap(indexResource_);
        uint32_t* indices = indexMap.get();
        if (!indices)
        {
            return false;
        }
        for (uint32_t q = 0; q < maxQuads_; ++q)
        {
            const uint32_t base = q * 4;
            indices[q * 6 + 0] = base + 0;
            indices[q * 6 + 1] = base + 1;
            indices[q * 6 + 2] = base + 2;
            indices[q * 6 + 3] = base + 0;
            indices[q * 6 + 4] = base + 2;
            indices[q * 6 + 5] = base + 3;
        }
    }

    return true;
}

void FireworkFx::Finalize()
{
    // マップを外してからリソースを手放す
    for (uint32_t i = 0; i < kFrameCount; ++i)
    {
        vertexMaps_[i].release();
        vertexResources_[i].Reset();
    }
    indexResource_.Reset();

    additivePipelineState_.Reset();
    alphaPipelineState_.Reset();
    rootSignature_.Reset();

    particles_.clear();
    pendingEmits_.clear();
    colorField_ = nullptr;

    isReady_ = false;
    activeCount_ = 0;
    softQuadCount_ = 0;
    sparkQuadCount_ = 0;
    drawnQuadCount_ = 0;
}

void FireworkFx::Clear()
{
    for (Particle& particle : particles_)
    {
        particle.isActive = false;
    }
    pendingEmits_.clear();
    activeCount_ = 0;
    nextSearchIndex_ = 0;
    softQuadCount_ = 0;
    sparkQuadCount_ = 0;
    drawnQuadCount_ = 0;
}

// ===================================================================
// 発生
// ===================================================================

int FireworkFx::FindFreeIndex()
{
    const int count = static_cast<int>(particles_.size());
    if (count == 0)
    {
        return -1;
    }

    // 前回の続きから探す。毎回先頭から舐めると粒が増えたとき O(n^2) になる
    for (int i = 0; i < count; ++i)
    {
        const int index = (nextSearchIndex_ + i) % count;
        if (!particles_[static_cast<size_t>(index)].isActive)
        {
            nextSearchIndex_ = (index + 1) % count;
            return index;
        }
    }
    return -1;
}

void FireworkFx::Emit(const FireworkFxDesc& desc)
{
    const int index = FindFreeIndex();
    if (index < 0)
    {
        return; // 満杯のときは黙って捨てる
    }

    Particle& particle = particles_[static_cast<size_t>(index)];
    particle.isActive = true;
    particle.position = desc.position;
    particle.velocity = desc.velocity;
    particle.gravity = desc.gravity;
    particle.drag = desc.drag;
    particle.colorBegin = desc.colorBegin;
    particle.colorEnd = desc.colorEnd;
    particle.scaleBegin = desc.scaleBegin;
    particle.scaleEnd = desc.scaleEnd;
    particle.scaleAspect = desc.scaleAspect;
    particle.lifeTime = (desc.lifeTime > 0.001f) ? desc.lifeTime : 0.001f;
    particle.age = 0.0f;
    particle.shape = desc.shape;
    particle.useSparkTexture = desc.useSparkTexture;
    particle.useColorField = desc.useColorField;
    particle.alignToVelocity = desc.alignToVelocity;
    particle.trailInterval = desc.trailInterval;
    particle.trailAccum = 0.0f;
    particle.trailLifeTime = desc.trailLifeTime;
    particle.trailScale = desc.trailScale;
}

void FireworkFx::EmitSpark(std::mt19937& /*rng*/, const Vector3& position, const Vector3& velocity,
                           const Vector4& color, float scale, float lifeTime, bool spark,
                           bool useColorField)
{
    FireworkFxDesc desc;
    desc.position = position;
    desc.velocity = velocity;
    desc.colorBegin = color;
    desc.colorEnd = {color.x, color.y, color.z, 0.0f};
    desc.scaleBegin = scale;
    desc.scaleEnd = scale * 0.15f;
    desc.lifeTime = lifeTime;
    desc.useSparkTexture = spark;
    desc.useColorField = useColorField;
    Emit(desc);
}

void FireworkFx::EmitSphereBurst(std::mt19937& rng, const Vector3& center, int count, float speed,
                                 float gravity, float drag, const Vector4& color, float scale,
                                 float lifeTime, bool spark, bool useColorField,
                                 float trailInterval, float trailLifeTime)
{
    for (int i = 0; i < count; ++i)
    {
        const float theta = RandRange(rng, 0.0f, kPi * 2.0f);
        // 真下は控えめにして上半球寄りに散らす
        const float cosPhi = RandRange(rng, -0.55f, 1.0f);
        const float sinPhi = std::sqrt((std::max)(0.0f, 1.0f - cosPhi * cosPhi));
        const Vector3 direction = {sinPhi * std::cos(theta), cosPhi, sinPhi * std::sin(theta)};
        const float currentSpeed = speed * (0.7f + Rand01(rng) * 0.6f);

        FireworkFxDesc desc;
        desc.position = center;
        desc.velocity = direction * currentSpeed;
        desc.gravity = gravity;
        desc.drag = drag;
        desc.colorBegin = color;
        // 消え際に色温度を落とすと火花っぽくなる（カラー場を使うときは α だけ効く）
        desc.colorEnd = {color.x * 0.7f, color.y * 0.45f, color.z * 0.3f, 0.0f};
        desc.scaleBegin = scale * (0.8f + Rand01(rng) * 0.6f);
        desc.scaleEnd = scale * 0.15f;
        desc.scaleAspect = 2.2f;   // 進行方向に伸ばして線に見せる
        desc.alignToVelocity = true;
        desc.lifeTime = lifeTime * (0.75f + Rand01(rng) * 0.5f);
        desc.useSparkTexture = spark;
        desc.useColorField = useColorField;
        desc.trailInterval = trailInterval;
        desc.trailLifeTime = trailLifeTime;
        desc.trailScale = scale * 0.45f;
        Emit(desc);
    }
}

void FireworkFx::EmitFirework(std::mt19937& rng, const Vector3& center, const Vector4& coreColor,
                              const Vector4& shellColor, float power, bool useColorField,
                              FireworkStyle style)
{
    // 1. 中心の閃光。短く、一気に開く
    {
        FireworkFxDesc desc;
        desc.position = center;
        desc.colorBegin = coreColor;
        desc.colorEnd = {coreColor.x, coreColor.y, coreColor.z, 0.0f};
        desc.scaleBegin = 0.5f * power;
        desc.scaleEnd = 2.4f * power;
        desc.lifeTime = 0.20f;
        desc.useColorField = useColorField;
        Emit(desc);
    }

    if (style == FireworkStyle::Willow)
    {
        // 柳。ゆっくり開いて、重力で垂れ下がりながら長く尾を引く
        EmitSphereBurst(rng, center, 32, 10.0f * power, 8.5f, 0.55f, shellColor, 0.22f * power,
                        2.1f, true, useColorField, 0.030f, 0.65f);

        // 上に抜ける火花を少しだけ足して、開いた瞬間の勢いを出す
        EmitSphereBurst(rng, center, 10, 15.0f * power, 16.0f, 1.4f, coreColor, 0.18f * power,
                        0.55f, true, useColorField, 0.028f, 0.28f);
        return;
    }

    // 菊。内殻は速くて短く、外殻が本体
    EmitSphereBurst(rng, center, 20, 9.0f * power, 14.0f, 1.6f, coreColor, 0.20f * power, 0.50f,
                    true, useColorField, 0.030f, 0.22f);
    EmitSphereBurst(rng, center, 34, 14.0f * power, 20.0f, 0.9f, shellColor, 0.24f * power, 0.95f,
                    true, useColorField, 0.028f, 0.30f);

    // 尾を引く火花。横長に潰して線に見せる
    for (int i = 0; i < 10; ++i)
    {
        const float theta = RandRange(rng, 0.0f, kPi * 2.0f);
        const float cosPhi = RandRange(rng, -0.2f, 1.0f);
        const float sinPhi = std::sqrt((std::max)(0.0f, 1.0f - cosPhi * cosPhi));
        const Vector3 direction = {sinPhi * std::cos(theta), cosPhi, sinPhi * std::sin(theta)};

        FireworkFxDesc desc;
        desc.position = center;
        desc.velocity = direction * (17.0f * power * (0.8f + Rand01(rng) * 0.4f));
        desc.gravity = 22.0f;
        desc.drag = 2.0f;
        desc.colorBegin = shellColor;
        desc.colorEnd = {shellColor.x, shellColor.y, shellColor.z, 0.0f};
        desc.scaleBegin = 0.30f * power;
        desc.scaleEnd = 0.05f;
        desc.scaleAspect = 3.0f;
        desc.alignToVelocity = true;
        desc.lifeTime = 0.55f;
        desc.useSparkTexture = true;
        desc.useColorField = useColorField;
        desc.trailInterval = 0.026f;
        desc.trailLifeTime = 0.26f;
        desc.trailScale = 0.09f * power;
        Emit(desc);
    }
}

// ===================================================================
// 更新
// ===================================================================

void FireworkFx::Update(float deltaTime)
{
    if (!isReady_)
    {
        return;
    }

    elapsedTime_ += deltaTime;

    pendingEmits_.clear();
    activeCount_ = 0;

    for (Particle& particle : particles_)
    {
        if (!particle.isActive)
        {
            continue;
        }

        particle.age += deltaTime;
        if (particle.age >= particle.lifeTime)
        {
            particle.isActive = false;
            continue;
        }

        if (particle.drag > 0.0f)
        {
            const float decay = 1.0f - (std::min)(1.0f, particle.drag * deltaTime);
            particle.velocity.x *= decay;
            particle.velocity.y *= decay;
            particle.velocity.z *= decay;
        }
        particle.velocity.y -= particle.gravity * deltaTime;

        particle.position.x += particle.velocity.x * deltaTime;
        particle.position.y += particle.velocity.y * deltaTime;
        particle.position.z += particle.velocity.z * deltaTime;

        // 軌跡。飛びながら、その場に置き去りにする粒を撒く。
        // ここで直接 Emit すると、生えたばかりの粒を同じフレームで進めてしまうので溜めておく
        if (isTrailEnabled_ && particle.trailInterval > 0.0f && trailDensity_ > 0.0f)
        {
            const float interval = particle.trailInterval / trailDensity_;
            particle.trailAccum += deltaTime;
            while (particle.trailAccum >= interval)
            {
                particle.trailAccum -= interval;

                FireworkFxDesc trail;
                trail.position = particle.position;
                trail.gravity = 0.6f; // ほんの少しだけ垂れる
                trail.drag = 1.0f;
                trail.colorBegin = particle.colorBegin;
                trail.colorEnd = {particle.colorBegin.x, particle.colorBegin.y,
                                  particle.colorBegin.z, 0.0f};
                trail.scaleBegin = particle.trailScale;
                trail.scaleEnd = 0.0f;
                trail.lifeTime = particle.trailLifeTime;
                trail.useSparkTexture = false; // 丸のほうが線として滑らかに繋がる
                trail.useColorField = particle.useColorField;
                pendingEmits_.push_back(trail);
            }
        }

        ++activeCount_;
    }

    for (const FireworkFxDesc& desc : pendingEmits_)
    {
        Emit(desc);
    }
    pendingEmits_.clear();

    // 頂点の積み上げ。GPU が前フレームぶんを読んでいるかもしれないので書き込み先をずらす
    frameIndex_ = (frameIndex_ + 1) % kFrameCount;

    Vertex* destination = vertexMaps_[frameIndex_].get();
    softQuadCount_ = 0;
    sparkQuadCount_ = 0;
    if (!destination)
    {
        drawnQuadCount_ = 0;
        return;
    }

    // 前半に丸テクスチャ、後半にキラッ。テクスチャごとに1回ずつ描けるようにまとめる
    softQuadCount_ = BuildVertices(destination, maxQuads_, false);
    sparkQuadCount_ =
        BuildVertices(destination + static_cast<size_t>(softQuadCount_) * 4,
                      maxQuads_ - softQuadCount_, true);
    drawnQuadCount_ = softQuadCount_ + sparkQuadCount_;
}

uint32_t FireworkFx::BuildVertices(Vertex* destination, uint32_t destinationCapacityInQuads,
                                   bool spark)
{
    if (!destination || destinationCapacityInQuads == 0)
    {
        return 0;
    }

    // ビルボードの基準になるカメラの姿勢。
    // ワールド行列の各行がそのままカメラの右／上／前になっている
    Vector3 cameraRight = {1.0f, 0.0f, 0.0f};
    Vector3 cameraUp = {0.0f, 1.0f, 0.0f};
    Vector3 cameraForward = {0.0f, 0.0f, 1.0f};
    if (camera_)
    {
        const Matrix4x4& world = camera_->GetWorldMatrix();
        cameraRight = Normalize({world.m[0][0], world.m[0][1], world.m[0][2]}, cameraRight);
        cameraUp = Normalize({world.m[1][0], world.m[1][1], world.m[1][2]}, cameraUp);
        cameraForward = Normalize({world.m[2][0], world.m[2][1], world.m[2][2]}, cameraForward);
    }

    uint32_t quadCount = 0;

    for (const Particle& particle : particles_)
    {
        if (!particle.isActive || particle.useSparkTexture != spark)
        {
            continue;
        }
        if (quadCount >= destinationCapacityInQuads)
        {
            break;
        }

        const float t = particle.age / particle.lifeTime;
        const float scale = particle.scaleBegin + (particle.scaleEnd - particle.scaleBegin) * t;
        if (scale <= 0.0001f)
        {
            continue;
        }

        Vector4 color = Lerp(particle.colorBegin, particle.colorEnd, t);
        if (particle.useColorField && colorField_)
        {
            const Vector4 field = colorField_(elapsedTime_, particle.position);
            color = {field.x, field.y, field.z, color.w * field.w};
        }
        if (color.w <= 0.0001f)
        {
            continue;
        }

        // 板の2軸を決める
        Vector3 axisRight = cameraRight;
        Vector3 axisUp = cameraUp;

        if (particle.shape == FireworkFxShape::Ground)
        {
            // 地面に寝かせる
            axisRight = {1.0f, 0.0f, 0.0f};
            axisUp = {0.0f, 0.0f, 1.0f};
        }
        else if (particle.alignToVelocity)
        {
            // 速度をビルボード平面に射影して、それを縦軸にする。
            // こうすると板が進行方向に伸びて、そのまま尾に見える
            const Vector3& velocity = particle.velocity;
            const Vector3 projected = {velocity.x - cameraForward.x * Dot(velocity, cameraForward),
                                       velocity.y - cameraForward.y * Dot(velocity, cameraForward),
                                       velocity.z - cameraForward.z * Dot(velocity, cameraForward)};
            axisUp = Normalize(projected, cameraUp);
            axisRight = Normalize(Cross(axisUp, cameraForward), cameraRight);
        }

        // scaleAspect は「進行方向（縦）にどれだけ伸ばすか」。横は素の scale のまま
        const float halfWidth = scale * 0.5f;
        const float halfHeight = scale * particle.scaleAspect * 0.5f;

        const Vector3 right = axisRight * halfWidth;
        const Vector3 up = axisUp * halfHeight;
        const Vector3& center = particle.position;

        Vertex* quad = destination + static_cast<size_t>(quadCount) * 4;

        // 左下 → 左上 → 右上 → 右下（インデックスは 0,1,2 / 0,2,3）
        quad[0].position = {center.x - right.x - up.x, center.y - right.y - up.y,
                            center.z - right.z - up.z, 1.0f};
        quad[0].texcoord = {0.0f, 1.0f};
        quad[1].position = {center.x - right.x + up.x, center.y - right.y + up.y,
                            center.z - right.z + up.z, 1.0f};
        quad[1].texcoord = {0.0f, 0.0f};
        quad[2].position = {center.x + right.x + up.x, center.y + right.y + up.y,
                            center.z + right.z + up.z, 1.0f};
        quad[2].texcoord = {1.0f, 0.0f};
        quad[3].position = {center.x + right.x - up.x, center.y + right.y - up.y,
                            center.z + right.z - up.z, 1.0f};
        quad[3].texcoord = {1.0f, 1.0f};

        quad[0].color = color;
        quad[1].color = color;
        quad[2].color = color;
        quad[3].color = color;

        ++quadCount;
    }

    return quadCount;
}

// ===================================================================
// 描画
// ===================================================================

void FireworkFx::Draw(ID3D12GraphicsCommandList* commandList)
{
    if (!isReady_ || !commandList || !camera_ || drawnQuadCount_ == 0)
    {
        return;
    }

    auto* cbAllocator = dxCommon_ ? dxCommon_->GetCBAllocator() : nullptr;
    if (!cbAllocator)
    {
        return;
    }

    // ViewProjection を1個だけ切り出す。粒が何個あっても定数バッファはこれだけ
    SceneParams sceneParams{};
    sceneParams.viewProjection = camera_->GetViewProjectionMatrix();

    auto sceneAllocation = cbAllocator->Allocate(sizeof(SceneParams));
    if (!sceneAllocation.cpuAddress)
    {
        return;
    }
    std::memcpy(sceneAllocation.cpuAddress, &sceneParams, sizeof(SceneParams));

    const auto& pipelineState = isAdditive_ ? additivePipelineState_ : alphaPipelineState_;
    if (!pipelineState)
    {
        return;
    }

    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState.Get());
    commandList->SetGraphicsRootConstantBufferView(kRootParamScene, sceneAllocation.gpuAddress);

    commandList->IASetVertexBuffers(0, 1, &vertexBufferViews_[frameIndex_]);
    commandList->IASetIndexBuffer(&indexBufferView_);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    TextureManager* textureManager = TextureManager::GetInstance();

    // 丸テクスチャの分（頂点バッファの前半）
    if (softQuadCount_ > 0 && softTextureIndex_ != TextureManager::kInvalidTextureIndex)
    {
        const D3D12_GPU_DESCRIPTOR_HANDLE handle =
            textureManager->GetSrvHandleGPU(softTextureIndex_);
        if (handle.ptr != 0)
        {
            commandList->SetGraphicsRootDescriptorTable(kRootParamTexture, handle);
            commandList->DrawIndexedInstanced(softQuadCount_ * 6, 1, 0, 0, 0);
        }
    }

    // キラッの分（後半）。インデックスは頂点番号を直に指しているので、開始位置をずらすだけでいい
    if (sparkQuadCount_ > 0 && sparkTextureIndex_ != TextureManager::kInvalidTextureIndex)
    {
        const D3D12_GPU_DESCRIPTOR_HANDLE handle =
            textureManager->GetSrvHandleGPU(sparkTextureIndex_);
        if (handle.ptr != 0)
        {
            commandList->SetGraphicsRootDescriptorTable(kRootParamTexture, handle);
            commandList->DrawIndexedInstanced(sparkQuadCount_ * 6, 1, softQuadCount_ * 6, 0, 0);
        }
    }
}
