#include "SlimeFx.h"

#include "Baziru3_Engine/Graphics/3D/Object/Object3dCom.h"
#include "DirectXCom.h"
#include "Light.h"
#include "RootParam.h"
#include "SceneManager.h"
#include "TextureManager.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace {

constexpr float kPi = 3.14159265358979323846f;

// Object3dCom::Draw() は環境マップをルートパラメータ 5 に張っている。
// RootParam::Object3D の enum には入っていないので、ここでも同じ値を使う。
constexpr UINT kRootParamEnvironmentMap = 5;

/// <summary>0..1 の一様乱数</summary>
float Rand01(std::mt19937& rng)
{
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(rng);
}

/// <summary>min..max の一様乱数</summary>
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

} // namespace

SlimeFx::~SlimeFx()
{
    Finalize();
}

Object3d::ModelData SlimeFx::MakeQuadMesh()
{
    Object3d::ModelData modelData;

    // XY 平面に立てた 1x1 の板。原点が中心。
    // 地面用は X 軸まわりに 90 度倒して使う（カリングは CULL_NONE なので裏表を気にしなくていい）
    const Vector3 normal = {0.0f, 0.0f, -1.0f};

    auto pushVertex = [&](float x, float y, float u, float v) {
        Sprite::VertexData vertex{};
        vertex.position = {x, y, 0.0f, 1.0f};
        vertex.texcoord = {u, v};
        vertex.normal = normal;
        modelData.vertices.push_back(vertex);
    };

    pushVertex(-0.5f, -0.5f, 0.0f, 1.0f);
    pushVertex(-0.5f, 0.5f, 0.0f, 0.0f);
    pushVertex(0.5f, 0.5f, 1.0f, 0.0f);
    pushVertex(0.5f, -0.5f, 1.0f, 1.0f);

    modelData.indices = {0, 1, 2, 0, 2, 3};
    modelData.boundingRadius = 1.0f;

    return modelData;
}

void SlimeFx::Initialize(Object3dCom* object3dCom, Camera* camera, uint32_t capacity)
{
    object3dCom_ = object3dCom;
    camera_ = camera;

    if (!object3dCom_ || capacity == 0)
    {
        return;
    }

    TextureManager* textureManager = TextureManager::GetInstance();
    softTextureIndex_ = textureManager->Load("Resources/CG4/circle2.png");
    sparkTextureIndex_ = textureManager->Load("Resources/starburst.png");

    quadModelData_ = MakeQuadMesh();
    quadModelData_.material.textureIndex = softTextureIndex_;

    particles_.assign(capacity, Particle{});
    objects_.clear();
    objects_.reserve(capacity);

    for (uint32_t i = 0; i < capacity; ++i)
    {
        auto object = std::make_unique<Object3d>();
        // Object3d::InitializeShared() は頂点バッファを共有できるが、
        // PrepareConstantBuffers() がマテリアル定数バッファのアドレスまで
        // マスターと共有してしまうので、粒ごとに色を変えられなくなる。
        // ここは1粒ずつ普通に Initialize する
        object->Initialize(object3dCom_, quadModelData_);
        object->SetCamera(camera_);
        // ライティングを切って、マテリアルの色 × テクスチャだけで出す
        object->SetEnableLighting(false);
        object->SetAllowWireframeOverlay(false);
        object->SetScale({0.0f, 0.0f, 0.0f});
        object->SetTranslate({0.0f, -1000.0f, 0.0f});
        object->Update();
        objects_.push_back(std::move(object));
    }

    CreateAdditivePipelineState();

    activeCount_ = 0;
    nextSearchIndex_ = 0;
}

void SlimeFx::CreateAdditivePipelineState()
{
    additivePipelineState_.Reset();

    if (!object3dCom_)
    {
        return;
    }
    DirectXCom* dx = object3dCom_->GetDirectXCom();
    if (!dx || !dx->GetDevice())
    {
        return;
    }

    const auto& rootSignature = object3dCom_->GetRootSignature();
    if (!rootSignature)
    {
        return;
    }

    // Object3D と同じシェーダを使う（ライティングは切って使うので実質 色 x テクスチャ）
    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dx->CompileShader(
        L"Resources/shaders/Object3D.VS.hlsl", L"vs_6_0", dx->GetDxcUtils().Get(),
        dx->GetDxcCompiler(), dx->GetIncludeHandler(), std::cout);
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dx->CompileShader(
        L"Resources/shaders/Object3D.PS.hlsl", L"ps_6_0", dx->GetDxcUtils().Get(),
        dx->GetDxcCompiler(), dx->GetIncludeHandler(), std::cout);
    if (!vertexShaderBlob || !pixelShaderBlob)
    {
        return; // 失敗したらアルファブレンドの PSO にフォールバックする
    }

    // Object3D のインプットレイアウトと同じ並び
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[3]{};
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

    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
    inputLayoutDesc.pInputElementDescs = inputElementDescs;
    inputLayoutDesc.NumElements = _countof(inputElementDescs);

    D3D12_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = rootSignature.Get();
    desc.InputLayout = inputLayoutDesc;
    desc.VS = {vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize()};
    desc.PS = {pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize()};

    // ここだけが Object3D_Effect との違い: DestBlend を ONE にして加算合成にする
    desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    desc.BlendState.RenderTarget[0].BlendEnable = TRUE;
    desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
    desc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    desc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    desc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;

    desc.RasterizerState = rasterizerDesc;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.SampleDesc.Count = 1;
    desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    // デプスはテストするが書き込まない（Object3D_Effect と同じ）
    desc.DepthStencilState.DepthEnable = TRUE;
    desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
    const HRESULT hr =
        dx->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipelineState));
    if (SUCCEEDED(hr))
    {
        additivePipelineState_ = pipelineState;
    }
}

void SlimeFx::Finalize()
{
    objects_.clear();
    particles_.clear();
    additivePipelineState_.Reset();
    activeCount_ = 0;
    nextSearchIndex_ = 0;
}

void SlimeFx::Clear()
{
    for (Particle& particle : particles_)
    {
        particle.isActive = false;
        particle.isParked = false;
    }
    activeCount_ = 0;
}

int SlimeFx::FindFreeIndex()
{
    const int count = static_cast<int>(particles_.size());
    if (count == 0)
    {
        return -1;
    }

    // 前回の続きから探す。全部埋まっていたら諦める（古い粒を殺すより自然）
    for (int i = 0; i < count; ++i)
    {
        const int index = (nextSearchIndex_ + i) % count;
        if (!particles_[index].isActive)
        {
            nextSearchIndex_ = (index + 1) % count;
            return index;
        }
    }
    return -1;
}

void SlimeFx::Emit(const SlimeFxDesc& desc)
{
    const int index = FindFreeIndex();
    if (index < 0)
    {
        return;
    }

    Particle& particle = particles_[index];
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
    particle.attractTarget = desc.attractTarget;
    particle.attractStrength = desc.attractStrength;
    particle.isParked = false;

    ++activeCount_;
}

void SlimeFx::Update(float deltaTime)
{
    if (objects_.empty())
    {
        return;
    }

    const float billboardPitch = -cameraPitch_;
    const size_t count = particles_.size();

    activeCount_ = 0;

    for (size_t i = 0; i < count; ++i)
    {
        Particle& particle = particles_[i];
        Object3d* object = objects_[i].get();
        if (!object)
        {
            continue;
        }

        if (!particle.isActive)
        {
            // 消えた粒は一度だけ片付ける。
            // 描画側で弾いているので、毎フレーム行列を計算し直す必要はない
            if (!particle.isParked)
            {
                object->SetScale({0.0f, 0.0f, 0.0f});
                object->SetTranslate({0.0f, -1000.0f, 0.0f});
                object->Update();
                particle.isParked = true;
            }
            continue;
        }

        particle.age += deltaTime;
        if (particle.age >= particle.lifeTime)
        {
            particle.isActive = false;
            object->SetScale({0.0f, 0.0f, 0.0f});
            object->SetTranslate({0.0f, -1000.0f, 0.0f});
            object->Update();
            particle.isParked = true;
            continue;
        }

        // 速度の減衰と重力
        if (particle.drag > 0.0f)
        {
            const float decay = 1.0f - (std::min)(1.0f, particle.drag * deltaTime);
            particle.velocity.x *= decay;
            particle.velocity.y *= decay;
            particle.velocity.z *= decay;
        }
        particle.velocity.y -= particle.gravity * deltaTime;

        // 吸引。距離が近いほど加速度を上げて「吸い込まれる」感じを出す
        if (particle.attractStrength > 0.0f)
        {
            const Vector3 toTarget = particle.attractTarget - particle.position;
            const float distance = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y +
                                             toTarget.z * toTarget.z);
            if (distance > 0.01f)
            {
                // 1/(距離+0.6) で頭打ちさせないと中心付近で発散する
                const float pull = particle.attractStrength / (distance + 0.6f) * deltaTime;
                particle.velocity.x += (toTarget.x / distance) * pull;
                particle.velocity.y += (toTarget.y / distance) * pull;
                particle.velocity.z += (toTarget.z / distance) * pull;
            }
        }

        particle.position.x += particle.velocity.x * deltaTime;
        particle.position.y += particle.velocity.y * deltaTime;
        particle.position.z += particle.velocity.z * deltaTime;

        const float t = particle.age / particle.lifeTime;
        const float scale = particle.scaleBegin + (particle.scaleEnd - particle.scaleBegin) * t;
        const Vector4 color = Lerp(particle.colorBegin, particle.colorEnd, t);

        object->SetTranslate(particle.position);
        object->SetColor(color);

        if (particle.shape == SlimeFxShape::Ground)
        {
            // X 軸まわりに 90 度倒して地面に寝かせる
            object->SetRotate({kPi * 0.5f, 0.0f, 0.0f});
            object->SetScale({scale * particle.scaleAspect, scale, 1.0f});
        }
        else
        {
            // カメラは yaw / roll を使っていないので、ピッチを打ち消すだけで正対する
            object->SetRotate({billboardPitch, 0.0f, 0.0f});
            object->SetScale({scale * particle.scaleAspect, scale, 1.0f});
        }

        object->Update();
        ++activeCount_;
    }
}

void SlimeFx::DrawGroup(const RenderContext& ctx, bool spark)
{
    if (!object3dCom_ || !ctx.commandList || !ctx.camera)
    {
        return;
    }

    // 描くものがあるか先に確認しておく（無駄なステート変更を避ける）
    bool hasAny = false;
    for (const Particle& particle : particles_)
    {
        if (particle.isActive && particle.useSparkTexture == spark)
        {
            hasAny = true;
            break;
        }
    }
    if (!hasAny)
    {
        return;
    }

    const auto& rootSignature = object3dCom_->GetRootSignature();
    // どちらもデプス書き込み無効。粒同士が隠し合わず、3D の前後関係だけ効く
    const auto& pipelineState = (isAdditive_ && additivePipelineState_)
                                    ? additivePipelineState_
                                    : object3dCom_->GetEffectPipelineState();
    if (!rootSignature || !pipelineState)
    {
        return;
    }

    ID3D12GraphicsCommandList* commandList = ctx.commandList;
    commandList->SetGraphicsRootSignature(rootSignature.Get());
    commandList->SetPipelineState(pipelineState.Get());

    TextureManager* textureManager = TextureManager::GetInstance();
    const uint32_t textureIndex = spark ? sparkTextureIndex_ : softTextureIndex_;
    D3D12_GPU_DESCRIPTOR_HANDLE textureHandle{};
    if (textureIndex != TextureManager::kInvalidTextureIndex)
    {
        textureHandle = textureManager->GetSrvHandleGPU(textureIndex);
    }
    if (textureHandle.ptr != 0)
    {
        commandList->SetGraphicsRootDescriptorTable(RootParam::Object3D::kTextureTable,
                                                    textureHandle);
    }

    // 環境マップ。ライティングを切っているので実際には使われないが、
    // ルートシグネチャが要求するので張っておく
    const uint32_t skyboxIndex = SceneManager::GetInstance()->GetSkyboxTextureIndex();
    D3D12_GPU_DESCRIPTOR_HANDLE skyboxHandle = textureHandle;
    if (skyboxIndex != TextureManager::kInvalidTextureIndex)
    {
        skyboxHandle = textureManager->GetSrvHandleGPU(skyboxIndex);
    }
    if (skyboxHandle.ptr != 0)
    {
        commandList->SetGraphicsRootDescriptorTable(kRootParamEnvironmentMap, skyboxHandle);
    }

    if (ctx.light && ctx.light->GetDirectionalLightResource())
    {
        commandList->SetGraphicsRootConstantBufferView(
            RootParam::Object3D::kLight,
            ctx.light->GetDirectionalLightResource()->GetGPUVirtualAddress());
    }

    if (ctx.camera->GetCameraGpuAddress() != 0)
    {
        commandList->SetGraphicsRootConstantBufferView(RootParam::Object3D::kCamera,
                                                       ctx.camera->GetCameraGpuAddress());
    }

    const size_t count = particles_.size();
    for (size_t i = 0; i < count; ++i)
    {
        const Particle& particle = particles_[i];
        if (!particle.isActive || particle.useSparkTexture != spark)
        {
            continue;
        }
        Object3d* object = objects_[i].get();
        if (object && !object->IsCulled())
        {
            object->DrawInternal(ctx);
        }
    }
}

void SlimeFx::Draw(const RenderContext& ctx)
{
    if (objects_.empty())
    {
        return;
    }

    // テクスチャごとに2パス。ステート変更は最大2回で済む
    DrawGroup(ctx, false);
    DrawGroup(ctx, true);
}

// ===================================================================
// 用途別ヘルパー
// ===================================================================

void SlimeFx::EmitSparkle(std::mt19937& rng, const Vector3& center, float radius, int count,
                          const Vector4& color, float scale, float lifeTime)
{
    for (int i = 0; i < count; ++i)
    {
        // 球の「内側」から湧かせて、そのまま外へ押し出す。
        // 体積内で一様にしたいので、半径方向は cbrt(0..1) で分布させる
        const float theta = RandRange(rng, 0.0f, kPi * 2.0f);
        const float cosPhi = RandRange(rng, -1.0f, 1.0f);
        const float sinPhi = std::sqrt((std::max)(0.0f, 1.0f - cosPhi * cosPhi));
        const Vector3 direction = {sinPhi * std::cos(theta), cosPhi, sinPhi * std::sin(theta)};

        const float depth = std::cbrt(Rand01(rng)); // 0 = 中心 / 1 = 表面
        const float distance = radius * depth * 0.55f;

        // 中心付近から出たものほど強く押し出す。体の中から染み出してくるように見える
        const float outwardSpeed = 1.4f + (1.0f - depth) * 1.6f;

        SlimeFxDesc desc;
        desc.position = {center.x + direction.x * distance, center.y + direction.y * distance,
                         center.z + direction.z * distance};
        desc.velocity = {direction.x * outwardSpeed, direction.y * outwardSpeed + 0.5f,
                         direction.z * outwardSpeed};
        desc.drag = 2.4f; // 表面を抜けたあたりで失速して漂う
        desc.colorBegin = color;
        desc.colorEnd = {color.x, color.y, color.z, 0.0f};
        // 加算合成向けに「小さく生まれて、広がりながら薄れる」
        desc.scaleBegin = scale * (0.35f + Rand01(rng) * 0.35f);
        desc.scaleEnd = scale * (1.1f + Rand01(rng) * 0.6f);
        desc.lifeTime = lifeTime * (0.7f + Rand01(rng) * 0.6f);
        desc.useSparkTexture = true;
        Emit(desc);
    }
}

void SlimeFx::EmitBurst(std::mt19937& rng, const Vector3& center, int count, float speed,
                        float upSpeed, const Vector4& color, float scale, float lifeTime,
                        bool spark)
{
    if (count <= 0)
    {
        return;
    }

    const float angleStep = (kPi * 2.0f) / static_cast<float>(count);
    for (int i = 0; i < count; ++i)
    {
        const float angle = angleStep * static_cast<float>(i) + RandRange(rng, -0.15f, 0.15f);
        const float currentSpeed = speed * (0.8f + Rand01(rng) * 0.4f);

        SlimeFxDesc desc;
        desc.position = center;
        desc.velocity = {std::sin(angle) * currentSpeed, upSpeed * (0.7f + Rand01(rng) * 0.6f),
                         std::cos(angle) * currentSpeed};
        desc.gravity = 9.0f;
        desc.drag = 0.8f;
        desc.colorBegin = color;
        desc.colorEnd = {color.x, color.y, color.z, 0.0f};
        desc.scaleBegin = scale * (0.8f + Rand01(rng) * 0.5f);
        desc.scaleEnd = scale * 0.2f;
        desc.lifeTime = lifeTime * (0.8f + Rand01(rng) * 0.4f);
        desc.useSparkTexture = spark;
        Emit(desc);
    }
}

void SlimeFx::EmitConverge(std::mt19937& rng, const Vector3& from, const Vector3& to, int count,
                           const Vector4& color, float scale, float lifeTime)
{
    const Vector3 toTarget = {to.x - from.x, to.y - from.y, to.z - from.z};
    const float distance =
        std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y + toTarget.z * toTarget.z);
    if (distance < 1e-4f)
    {
        return;
    }

    // 寿命の間にちょうど到達するくらいの速度にする
    const float speed = distance / (std::max)(0.05f, lifeTime * 0.9f);
    const Vector3 direction = {toTarget.x / distance, toTarget.y / distance, toTarget.z / distance};

    for (int i = 0; i < count; ++i)
    {
        SlimeFxDesc desc;
        desc.position = {from.x + RandRange(rng, -0.25f, 0.25f), from.y + RandRange(rng, -0.1f, 0.3f),
                         from.z + RandRange(rng, -0.25f, 0.25f)};
        const float currentSpeed = speed * (0.85f + Rand01(rng) * 0.3f);
        desc.velocity = {direction.x * currentSpeed, direction.y * currentSpeed,
                         direction.z * currentSpeed};
        desc.colorBegin = color;
        desc.colorEnd = {color.x, color.y, color.z, 0.0f};
        desc.scaleBegin = scale;
        desc.scaleEnd = scale * 0.3f;
        desc.lifeTime = lifeTime;
        desc.useSparkTexture = false;
        Emit(desc);
    }
}

void SlimeFx::EmitStrand(std::mt19937& rng, const Vector3& from, const Vector3& to, int count,
                         const Vector4& color, float scale, float lifeTime)
{
    if (count <= 0)
    {
        return;
    }

    for (int i = 0; i < count; ++i)
    {
        const float t = static_cast<float>(i + 1) / static_cast<float>(count + 1);

        SlimeFxDesc desc;
        desc.position = {from.x + (to.x - from.x) * t + RandRange(rng, -0.04f, 0.04f),
                         from.y + (to.y - from.y) * t + RandRange(rng, -0.04f, 0.04f),
                         from.z + (to.z - from.z) * t + RandRange(rng, -0.04f, 0.04f)};
        desc.velocity = {0.0f, 0.0f, 0.0f};
        desc.colorBegin = color;
        desc.colorEnd = {color.x, color.y, color.z, 0.0f};
        // 中央ほど細くして「引き伸ばされた粘液」に見せる
        const float thin = 1.0f - std::abs(t - 0.5f) * 1.2f;
        desc.scaleBegin = scale * (0.5f + thin * 0.5f);
        desc.scaleEnd = 0.0f;
        desc.lifeTime = lifeTime;
        desc.useSparkTexture = false;
        Emit(desc);
    }
}

void SlimeFx::EmitDroplet(std::mt19937& rng, const Vector3& position, const Vector3& velocity,
                          const Vector4& color, float scale, float lifeTime)
{
    SlimeFxDesc desc;
    desc.position = position;
    desc.velocity = velocity;
    desc.drag = 1.5f;
    desc.colorBegin = color;
    desc.colorEnd = {color.x, color.y, color.z, 0.0f};
    desc.scaleBegin = scale * (0.8f + Rand01(rng) * 0.4f);
    desc.scaleEnd = scale * 0.15f;
    desc.lifeTime = lifeTime;
    desc.useSparkTexture = false;
    Emit(desc);
}

void SlimeFx::EmitGroundMark(std::mt19937& rng, const Vector3& position, float scale,
                             const Vector4& color, float lifeTime)
{
    SlimeFxDesc desc;
    desc.position = position;
    desc.velocity = {0.0f, 0.0f, 0.0f};
    desc.colorBegin = color;
    desc.colorEnd = {color.x, color.y, color.z, 0.0f};
    // 置いた瞬間に少し広がってから消える
    desc.scaleBegin = scale * (0.85f + Rand01(rng) * 0.3f);
    desc.scaleEnd = scale * 1.35f;
    desc.scaleAspect = 0.9f + Rand01(rng) * 0.3f;
    desc.lifeTime = lifeTime;
    desc.shape = SlimeFxShape::Ground;
    desc.useSparkTexture = false;
    Emit(desc);
}

void SlimeFx::EmitSphereBurst(std::mt19937& rng, const Vector3& center, int count, float speed,
                              float gravity, float drag, const Vector4& color, float scale,
                              float lifeTime, bool spark)
{
    for (int i = 0; i < count; ++i)
    {
        const float theta = RandRange(rng, 0.0f, kPi * 2.0f);
        // 真下は控えめにして、上半球寄りに散らす
        const float cosPhi = RandRange(rng, -0.55f, 1.0f);
        const float sinPhi = std::sqrt((std::max)(0.0f, 1.0f - cosPhi * cosPhi));
        const Vector3 direction = {sinPhi * std::cos(theta), cosPhi, sinPhi * std::sin(theta)};
        const float currentSpeed = speed * (0.7f + Rand01(rng) * 0.6f);

        SlimeFxDesc desc;
        desc.position = center;
        desc.velocity = direction * currentSpeed;
        desc.gravity = gravity;
        desc.drag = drag;
        desc.colorBegin = color;
        // 消え際に色温度を落とすと火花っぽくなる
        desc.colorEnd = {color.x * 0.7f, color.y * 0.45f, color.z * 0.3f, 0.0f};
        desc.scaleBegin = scale * (0.8f + Rand01(rng) * 0.6f);
        desc.scaleEnd = scale * 0.15f;
        desc.lifeTime = lifeTime * (0.75f + Rand01(rng) * 0.5f);
        desc.useSparkTexture = spark;
        Emit(desc);
    }
}

void SlimeFx::EmitVortex(std::mt19937& rng, const Vector3& center, float radius, int count,
                         const Vector4& color, float scale, float lifeTime, float attractStrength)
{
    for (int i = 0; i < count; ++i)
    {
        const float angle = RandRange(rng, 0.0f, kPi * 2.0f);
        const float currentRadius = radius * (0.7f + Rand01(rng) * 0.5f);
        const Vector3 offset = {std::sin(angle) * currentRadius, RandRange(rng, -0.1f, 0.9f),
                                std::cos(angle) * currentRadius};

        // 円周の接線方向に流しておいて、そこへ中心への吸引を掛けると螺旋を描く
        const float spin = RandRange(rng, 2.2f, 3.8f);
        const Vector3 tangent = {std::cos(angle) * spin, 0.0f, -std::sin(angle) * spin};

        SlimeFxDesc desc;
        desc.position = center + offset;
        desc.velocity = tangent;
        desc.attractTarget = center;
        desc.attractStrength = attractStrength;
        desc.colorBegin = color;
        desc.colorEnd = {color.x, color.y, color.z, 0.0f};
        desc.scaleBegin = scale * (0.7f + Rand01(rng) * 0.6f);
        desc.scaleEnd = scale * 0.2f;
        desc.lifeTime = lifeTime * (0.8f + Rand01(rng) * 0.4f);
        desc.useSparkTexture = true;
        Emit(desc);
    }
}

void SlimeFx::EmitShockwave(const Vector3& center, float scaleBegin, float scaleEnd,
                            const Vector4& color, float lifeTime)
{
    SlimeFxDesc desc;
    desc.position = center;
    desc.velocity = {0.0f, 0.0f, 0.0f};
    desc.colorBegin = color;
    desc.colorEnd = {color.x, color.y, color.z, 0.0f};
    desc.scaleBegin = scaleBegin;
    desc.scaleEnd = scaleEnd;
    desc.lifeTime = lifeTime;
    desc.shape = SlimeFxShape::Ground;
    desc.useSparkTexture = false;
    Emit(desc);
}

void SlimeFx::EmitFirework(std::mt19937& rng, const Vector3& center, const Vector4& coreColor,
                           const Vector4& shellColor, float power)
{
    // 1. 中心の閃光。短く、一気に開く
    {
        SlimeFxDesc desc;
        desc.position = center;
        desc.velocity = {0.0f, 0.0f, 0.0f};
        desc.colorBegin = coreColor;
        desc.colorEnd = {coreColor.x, coreColor.y, coreColor.z, 0.0f};
        desc.scaleBegin = 0.5f * power;
        desc.scaleEnd = 2.2f * power;
        desc.lifeTime = 0.18f;
        desc.useSparkTexture = false;
        Emit(desc);
    }

    // 2. 内側の殻。速くて短い
    EmitSphereBurst(rng, center, 16, 9.0f * power, 14.0f, 1.6f, coreColor, 0.22f * power, 0.45f,
                    true);

    // 3. 外側の殻。減速しながら重力で落ちる（これが「花火」に見せる本体）
    EmitSphereBurst(rng, center, 22, 13.0f * power, 20.0f, 0.9f, shellColor, 0.26f * power, 0.85f,
                    true);

    // 4. 尾を引く火花。横長に潰して線に見せる
    for (int i = 0; i < 8; ++i)
    {
        const float theta = RandRange(rng, 0.0f, kPi * 2.0f);
        const float cosPhi = RandRange(rng, -0.2f, 1.0f);
        const float sinPhi = std::sqrt((std::max)(0.0f, 1.0f - cosPhi * cosPhi));
        const Vector3 direction = {sinPhi * std::cos(theta), cosPhi, sinPhi * std::sin(theta)};

        SlimeFxDesc desc;
        desc.position = center;
        desc.velocity = direction * (16.0f * power * (0.8f + Rand01(rng) * 0.4f));
        desc.gravity = 22.0f;
        desc.drag = 2.0f;
        desc.colorBegin = shellColor;
        desc.colorEnd = {shellColor.x, shellColor.y, shellColor.z, 0.0f};
        desc.scaleBegin = 0.34f * power;
        desc.scaleEnd = 0.05f;
        desc.scaleAspect = 2.6f;
        desc.lifeTime = 0.5f;
        desc.useSparkTexture = true;
        Emit(desc);
    }
}
