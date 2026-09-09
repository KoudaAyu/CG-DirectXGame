#include "SlimeMesh.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3dCom.h"
#include "Baziru3_Engine/Graphics/2D/Texture/TextureManager.h"
#include "Baziru3_Engine/Core/Camera/Camera.h"
#include "SceneManager.h"
#include "Light.h"
#include "Application/GameObject/SlimePhysics.h"
#include <cmath>
#include <algorithm>

namespace {
    constexpr float kPi = 3.14159265358979323846f;
}

Object3d::ModelData SlimeMesh::GenerateSphere(uint32_t sliceCount, uint32_t stackCount, float radius)
{
    Object3d::ModelData modelData;

    // --- 頂点の生成 ---
    // 北極点
    {
        Sprite::VertexData v{};
        v.position = { 0.0f, radius, 0.0f, 1.0f };
        v.normal   = { 0.0f, 1.0f, 0.0f };
        v.texcoord = { 0.5f, 0.0f };
        modelData.vertices.push_back(v);
    }

    // 中間リング
    for (uint32_t i = 1; i < stackCount; ++i)
    {
        float phi = kPi * static_cast<float>(i) / static_cast<float>(stackCount);
        float sinPhi = std::sin(phi);
        float cosPhi = std::cos(phi);

        for (uint32_t j = 0; j <= sliceCount; ++j)
        {
            float theta = 2.0f * kPi * static_cast<float>(j) / static_cast<float>(sliceCount);
            float sinTheta = std::sin(theta);
            float cosTheta = std::cos(theta);

            Sprite::VertexData v{};
            float nx = sinPhi * cosTheta;
            float ny = cosPhi;
            float nz = sinPhi * sinTheta;

            v.position = { nx * radius, ny * radius, nz * radius, 1.0f };
            v.normal   = { nx, ny, nz };
            v.texcoord = {
                static_cast<float>(j) / static_cast<float>(sliceCount),
                static_cast<float>(i) / static_cast<float>(stackCount)
            };
            modelData.vertices.push_back(v);
        }
    }

    // 南極点
    {
        Sprite::VertexData v{};
        v.position = { 0.0f, -radius, 0.0f, 1.0f };
        v.normal   = { 0.0f, -1.0f, 0.0f };
        v.texcoord = { 0.5f, 1.0f };
        modelData.vertices.push_back(v);
    }

    // --- インデックスの生成 ---
    // 北極キャップ（三角形ファン）
    for (uint32_t j = 0; j < sliceCount; ++j)
    {
        modelData.indices.push_back(0);
        modelData.indices.push_back(1 + j + 1);
        modelData.indices.push_back(1 + j);
    }

    // 中間リング（クワッド→2三角形）
    uint32_t ringVertexCount = sliceCount + 1;
    for (uint32_t i = 0; i < stackCount - 2; ++i)
    {
        uint32_t ringStart = 1 + i * ringVertexCount;
        uint32_t nextRingStart = ringStart + ringVertexCount;

        for (uint32_t j = 0; j < sliceCount; ++j)
        {
            // 上三角形
            modelData.indices.push_back(ringStart + j);
            modelData.indices.push_back(nextRingStart + j);
            modelData.indices.push_back(nextRingStart + j + 1);

            // 下三角形
            modelData.indices.push_back(ringStart + j);
            modelData.indices.push_back(nextRingStart + j + 1);
            modelData.indices.push_back(ringStart + j + 1);
        }
    }

    // 南極キャップ（三角形ファン）
    uint32_t southPoleIndex = static_cast<uint32_t>(modelData.vertices.size()) - 1;
    uint32_t lastRingStart = 1 + (stackCount - 2) * ringVertexCount;
    for (uint32_t j = 0; j < sliceCount; ++j)
    {
        modelData.indices.push_back(southPoleIndex);
        modelData.indices.push_back(lastRingStart + j);
        modelData.indices.push_back(lastRingStart + j + 1);
    }

    // バウンディング半径の設定
    modelData.boundingRadius = radius;

    return modelData;
}

Object3d::ModelData SlimeMesh::GenerateDisc(uint32_t sliceCount, float radius)
{
    Object3d::ModelData modelData;

    // 中心頂点
    {
        Sprite::VertexData v{};
        v.position = { 0.0f, 0.0f, 0.0f, 1.0f };
        v.normal   = { 0.0f, 1.0f, 0.0f };
        v.texcoord = { 0.5f, 0.5f }; // テクスチャ中心
        modelData.vertices.push_back(v);
    }

    // 外周頂点
    for (uint32_t i = 0; i <= sliceCount; ++i)
    {
        float theta = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(sliceCount);
        float cosT = std::cos(theta);
        float sinT = std::sin(theta);

        Sprite::VertexData v{};
        v.position = { cosT * radius, 0.0f, sinT * radius, 1.0f };
        v.normal   = { 0.0f, 1.0f, 0.0f };
        // UV: 外周に配置（シェーダーで中心からの距離としてフェードに使える）
        v.texcoord = { 0.5f + cosT * 0.5f, 0.5f + sinT * 0.5f };
        modelData.vertices.push_back(v);
    }

    // インデックス（三角形ファン）
    for (uint32_t i = 0; i < sliceCount; ++i)
    {
        modelData.indices.push_back(0);         // 中心
        modelData.indices.push_back(1 + i);     // 現在の外周頂点
        modelData.indices.push_back(1 + i + 1); // 次の外周頂点
    }

    modelData.boundingRadius = radius;
    return modelData;
}

void CharacterShadow::Initialize(Object3dCom* object3dCom, Camera* camera)
{
    object3dCom_ = object3dCom;
    camera_ = camera;
    modelData_ = SlimeMesh::GenerateDisc(24, 1.0f);
    textureIndex_ = TextureManager::GetInstance()->Load("Resources/CG4/circle2.png");
    modelData_.material.textureIndex = textureIndex_;

    object_ = std::make_unique<Object3d>();
    object_->Initialize(object3dCom_, modelData_);
    object_->SetCamera(camera_);
    object_->SetEnableLighting(false);
    object_->SetColor(Vector4{ 0.0f, 0.0f, 0.0f, 0.55f });
    object_->Update();
}

void CharacterShadow::Update(const Vector3& charWorldPos, float shadowRadius, float charFootOffset,
                            const Vector2& stageTilt, const Vector2& pivot)
{
    if (!object_) return;

    bool hasGround = false;
    Vector3 groundNormal{ 0.0f, 1.0f, 0.0f };
    float surfaceY = SlimePhysics::CalculateGroundHeight(
        charWorldPos.x, charWorldPos.z, charWorldPos.y, stageTilt, &hasGround, &groundNormal, pivot);
    hasGround_ = hasGround;

    if (!hasGround) return;

    // 地面表面よりほんの少し法線方向に浮かせて Z-fighting を完全に防ぐ
    Vector3 shadowPos{ charWorldPos.x, surfaceY, charWorldPos.z };
    shadowPos.x += groundNormal.x * 0.015f;
    shadowPos.y += groundNormal.y * 0.015f;
    shadowPos.z += groundNormal.z * 0.015f;

    // 空中ジャンプ時の高さ計算
    float actualFootY = charWorldPos.y - charFootOffset;
    float heightAboveGround = (std::max)(0.0f, actualFootY - surfaceY);
    // 高く跳ぶほど影が少し小さく、薄くなる
    float heightFactor = std::clamp(1.0f - heightAboveGround * 0.08f, 0.20f, 1.0f);

    float finalRadius = shadowRadius * heightFactor;
    object_->SetTranslate(shadowPos);
    object_->SetScale({ finalRadius, 1.0f, finalRadius });

    // 地面の傾斜（法線）に沿って影の円盤を傾ける
    Vector3 rot{ 0.0f, 0.0f, 0.0f };
    rot.x = std::atan2(groundNormal.z, groundNormal.y);
    rot.z = -std::atan2(groundNormal.x, groundNormal.y);
    object_->SetRotate(rot);

    // 影の濃さ（中心最大 0.55、空中では薄くなる）
    float opacity = 0.55f * heightFactor;
    object_->SetColor(Vector4{ 0.0f, 0.0f, 0.0f, opacity });
    object_->Update();
}

void CharacterShadow::Draw(const RenderContext& ctx)
{
    if (!hasGround_ || !object_ || !object3dCom_ || !ctx.commandList || !ctx.camera) return;
    if (ctx.camera->GetCameraGpuAddress() == 0) return;

    auto effectPSO = object3dCom_->GetEffectPipelineState();
    if (!effectPSO) return;

    // デプス書き込みを切った半透明パイプライン（Object3D_Effect）で丸影を描画
    ctx.commandList->SetGraphicsRootSignature(object3dCom_->GetRootSignature().Get());
    ctx.commandList->SetPipelineState(effectPSO.Get());

    D3D12_GPU_DESCRIPTOR_HANDLE mainTextureHandle = ctx.textureHandle;
    if (textureIndex_ != TextureManager::kInvalidTextureIndex)
    {
        mainTextureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(textureIndex_);
    }
    else if (mainTextureHandle.ptr == 0)
    {
        mainTextureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(
            TextureManager::GetInstance()->GetTextureIndexByFilePath("Resources/CG4/circle2.png"));
    }
    if (mainTextureHandle.ptr != 0)
    {
        ctx.commandList->SetGraphicsRootDescriptorTable(2, mainTextureHandle);
    }

    uint32_t skyboxIndex = SceneManager::GetInstance()->GetSkyboxTextureIndex();
    D3D12_GPU_DESCRIPTOR_HANDLE skyboxHandle = mainTextureHandle;
    if (skyboxIndex != TextureManager::kInvalidTextureIndex)
    {
        skyboxHandle = TextureManager::GetInstance()->GetSrvHandleGPU(skyboxIndex);
    }
    if (skyboxHandle.ptr != 0)
    {
        ctx.commandList->SetGraphicsRootDescriptorTable(5, skyboxHandle);
    }

    if (ctx.light && ctx.light->GetDirectionalLightResource())
    {
        ctx.commandList->SetGraphicsRootConstantBufferView(
            3, ctx.light->GetDirectionalLightResource()->GetGPUVirtualAddress());
    }
    else
    {
        ctx.commandList->SetGraphicsRootConstantBufferView(3, 0);
    }

    ctx.commandList->SetGraphicsRootConstantBufferView(4, ctx.camera->GetCameraGpuAddress());

    object_->DrawInternal(ctx);
}
