#include "SlimeFx.h"

#include "Baziru3_Engine/Graphics/3D/Object/Object3dCom.h"
#include "DirectXCom.h"
#include "Light.h"
#include "RootParam.h"
#include "SceneManager.h"
#include "TextureManager.h"

#include <algorithm>
#include <cmath>

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

    activeCount_ = 0;
    nextSearchIndex_ = 0;
}

void SlimeFx::Finalize()
{
    objects_.clear();
    particles_.clear();
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
    // デプス書き込み無効の PSO。粒同士が隠し合わず、3D の前後関係だけ効く
    const auto& pipelineState = object3dCom_->GetEffectPipelineState();
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
        // 球面上のランダムな一点
        const float theta = RandRange(rng, 0.0f, kPi * 2.0f);
        const float cosPhi = RandRange(rng, -1.0f, 1.0f);
        const float sinPhi = std::sqrt((std::max)(0.0f, 1.0f - cosPhi * cosPhi));

        const Vector3 direction = {sinPhi * std::cos(theta), cosPhi, sinPhi * std::sin(theta)};
        const float distance = radius * (0.85f + Rand01(rng) * 0.3f);

        SlimeFxDesc desc;
        desc.position = {center.x + direction.x * distance, center.y + direction.y * distance,
                         center.z + direction.z * distance};
        // ふわっと上に昇りながら、外向きにも少し広がる
        desc.velocity = {direction.x * 0.25f, 0.5f + Rand01(rng) * 0.5f, direction.z * 0.25f};
        desc.drag = 1.2f;
        desc.colorBegin = color;
        desc.colorEnd = {color.x, color.y, color.z, 0.0f};
        desc.scaleBegin = scale * (0.6f + Rand01(rng) * 0.8f);
        desc.scaleEnd = 0.0f;
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
