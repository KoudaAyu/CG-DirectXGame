w#include "Minion.h"
#include "Application/GameObject/SlimeMesh.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3dCom.h"
#include "Baziru3_Engine/Core/Base/Pipeline/PipelineStateManager.h"
#include "Baziru3_Engine/Core/Base/Allocator/ConstantBufferAllocator.h"
#include "Baziru3_Engine/Framework/Collision/CollisionManager.h"
#include "TextureManager.h"
#include "DirectXCom.h"
#include "SceneManager.h"
#include "Light.h"
#include <algorithm>
#include <cmath>
#include <cstring>

Minion::Minion() {
    object3d_ = std::make_unique<Object3d>();
}

Minion::~Minion() {
    if (collider_) {
        CollisionManager::GetInstance()->UnregisterCollider(collider_.get());
    }
}

void Minion::Initialize(Object3dCom* object3dCom, Camera* camera, const Vector3& spawnPos, MinionType type) {
    object3dCom_ = object3dCom;
    camera_ = camera;
    position_ = spawnPos;
    type_ = type;
    state_ = MinionState::Following;
    isActive_ = true;

    // スライム球体メッシュを生成（滑らかな小スライム）
    modelData_ = SlimeMesh::GenerateSphere(48, 24, 1.0f);
    textureIndex_ = TextureManager::GetInstance()->Load("Resources/uvChecker.png");
    modelData_.material.textureIndex = textureIndex_;

    if (object3d_) {
        object3d_->Initialize(object3dCom_, modelData_);
        object3d_->SetCamera(camera_);
        object3d_->SetTranslate(position_);
        object3d_->SetScale(scale_);
        object3d_->SetRotate(rotation_);
        object3d_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f }); // シェーダー側baseColorで制御
        object3d_->SetEnableLighting(true);
        object3d_->Update();
    }

    // タイプに応じたスライムカラー
    switch (type_) {
    case MinionType::Red:
        slimeParams_.baseColor = { 1.0f, 0.3f, 0.25f, 0.88f }; // 赤スライム
        break;
    case MinionType::Yellow:
        slimeParams_.baseColor = { 1.0f, 0.9f, 0.2f, 0.88f };  // 黄スライム
        break;
    case MinionType::Blue:
        slimeParams_.baseColor = { 0.2f, 0.5f, 1.0f, 0.88f };  // 青スライム
        break;
    }
    slimeParams_.wobbleStrength = 0.1f;
    slimeParams_.wobbleFrequency = 5.0f;
    slimeParams_.fresnelPower = 3.0f;
    slimeParams_.envReflection = 0.3f;
    slimeParams_.innerGlow = 0.35f;
    slimeParams_.specularShininess = 48.0f;

    // ミニオン単体の当たり判定（SphereCollider）を生成・登録
    collider_ = std::make_unique<SphereCollider>(radius_, &position_, CollisionAttribute::Player);
    CollisionManager::GetInstance()->RegisterCollider(collider_.get());
}

void Minion::SetActive(bool active) {
    isActive_ = active;
    if (collider_) {
        collider_->SetIsEnabled(active && state_ != MinionState::Merging);
    }
}

void Minion::SetPosition(const Vector3& pos) {
    position_ = pos;
    if (object3d_) {
        object3d_->SetTranslate(position_);
    }
}

void Minion::Launch(const Vector3& velocity) {
    velocity_ = velocity;
    state_ = MinionState::Thrown;
    bounceTimer_ = 0.0f;
    scale_ = { 0.35f, 0.35f, 0.35f }; // スケールを通常サイズに確実に復帰

    // 投げ飛ばし時のスクワッシュ（進行方向に引き伸ばし）
    slimeParams_.impulseStrength = 0.25f;
    slimeParams_.squashStretch = { 0.0f, 0.15f, 0.0f }; // びよーん
}

void Minion::AttractTo(const Vector3& attractCenter, float attractSpeed) {
    state_ = MinionState::Merging;
    Vector3 diff = attractCenter - position_;
    float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
    if (dist > 0.01f) {
        float invDist = 1.0f / dist;
        velocity_ = { diff.x * invDist * attractSpeed, diff.y * invDist * attractSpeed, diff.z * invDist * attractSpeed };
    }
}

void Minion::Update(float deltaTime) {
    if (!isActive_) {
        if (collider_) {
            collider_->SetIsEnabled(false);
        }
        return;
    }

    if (collider_) {
        bool isColliderActive = isActive_ && (state_ != MinionState::Merging);
        collider_->SetIsEnabled(isColliderActive);
        collider_->SetRadius(radius_);
    }

    bounceTimer_ += deltaTime;
    totalTime_ += deltaTime;

    // 衝撃波紋の減衰
    slimeParams_.impulseStrength *= (1.0f - deltaTime * 5.0f);
    if (slimeParams_.impulseStrength < 0.001f) slimeParams_.impulseStrength = 0.0f;

    // スクワッシュの減衰
    slimeParams_.squashStretch.x *= 0.9f;
    slimeParams_.squashStretch.y *= 0.9f;
    slimeParams_.squashStretch.z *= 0.9f;

    switch (state_) {
    case MinionState::Following: {
        Vector3 diff = targetSlotPos_ - position_;
        diff.y = 0.0f;
        float dist = std::sqrt(diff.x * diff.x + diff.z * diff.z);

        if (dist > 0.05f) {
            // 画面外や遠距離でも一切クランプせず、ワールド空間で一定の自然な速度で移動
            float targetSpeed = followSpeed_;
            if (dist < 0.8f) {
                targetSpeed = (dist / 0.8f) * followSpeed_; // スロット直前のみ滑らかに減速
            }
            Vector3 desiredVel = { (diff.x / dist) * targetSpeed, 0.0f, (diff.z / dist) * targetSpeed };

            // 慣性を持った速度補間
            velocity_.x += (desiredVel.x - velocity_.x) * (std::min)(1.0f, deltaTime * 10.0f);
            velocity_.z += (desiredVel.z - velocity_.z) * (std::min)(1.0f, deltaTime * 10.0f);

            position_.x += velocity_.x * deltaTime;
            position_.z += velocity_.z * deltaTime;

            // スムーズな旋回
            float moveSpeed = std::sqrt(velocity_.x * velocity_.x + velocity_.z * velocity_.z);
            if (moveSpeed > 0.1f) {
                float targetYaw = std::atan2(velocity_.x, velocity_.z);
                float diffYaw = targetYaw - rotation_.y;
                while (diffYaw > 3.14159265f) diffYaw -= 2.0f * 3.14159265f;
                while (diffYaw < -3.14159265f) diffYaw += 2.0f * 3.14159265f;
                rotation_.y += diffYaw * (std::min)(1.0f, deltaTime * 12.0f);
            }
        } else {
            velocity_.x *= 0.8f;
            velocity_.z *= 0.8f;
        }

        // 接地Y座標の維持 ＋ ピョコピョコ跳ね
        position_.y = groundY_ + std::sin(bounceTimer_ * 14.0f) * 0.08f;
        scale_ = { 0.35f, 0.35f, 0.35f };

        // 跳ねに連動したスクワッシュ（微小な上下潰れ）
        float bouncePhase = std::sin(bounceTimer_ * 14.0f);
        slimeParams_.squashStretch.y = -bouncePhase * 0.04f;
        slimeParams_.squashStretch.x = bouncePhase * 0.02f;
        slimeParams_.squashStretch.z = bouncePhase * 0.02f;

        break;
    }

    case MinionState::Merging: {
        // プレイヤー中心に向かって吸引移動
        position_ += velocity_ * deltaTime;

        // 吸引中はスケールを縮小（吸い込まれる表現）
        float shrink = (std::max)(0.1f, scale_.x - deltaTime * 0.8f);
        scale_ = { shrink, shrink, shrink };
        break;
    }

    case MinionState::Thrown: {
        // 放物線移動（重力適用）
        velocity_.y += gravity_ * deltaTime;
        position_ += velocity_ * deltaTime;

        // 飛翔中も通常スケールを維持
        scale_ = { 0.35f, 0.35f, 0.35f };

        // 地面着地判定
        if (position_.y <= groundY_) {
            position_.y = groundY_;
            // 着地バウンド
            if (std::abs(velocity_.y) > 2.0f) {
                velocity_.y = -velocity_.y * 0.4f;
                velocity_.x *= 0.6f;
                velocity_.z *= 0.6f;

                // 着地時の潰れスクワッシュ
                slimeParams_.squashStretch.y = -0.2f;
                slimeParams_.squashStretch.x = 0.1f;
                slimeParams_.squashStretch.z = 0.1f;
                slimeParams_.impulseStrength = 0.15f;
            } else {
                velocity_ = { 0.0f, 0.0f, 0.0f };
                state_ = MinionState::Following;
            }
        }

        // 飛翔中の回転演出
        rotation_.x += 10.0f * deltaTime;

        // 飛翔中は進行方向に引き伸ばし
        float speed = std::sqrt(velocity_.x * velocity_.x + velocity_.z * velocity_.z);
        slimeParams_.squashStretch.y = (std::min)(speed * 0.01f, 0.15f);

        break;
    }

    case MinionState::Idle: {
        position_.y = groundY_;
        scale_ = { 0.35f, 0.35f, 0.35f };
        break;
    }

    case MinionState::Carrying: {
        break;
    }
    }

    // シェーダー時間の更新
    slimeParams_.time = totalTime_;

    if (object3d_) {
        object3d_->SetTranslate(position_);
        object3d_->SetRotate(rotation_);
        object3d_->SetScale(scale_);
        object3d_->Update();
    }
}

void Minion::DrawSlime(const RenderContext& ctx) {
    if (!object3d_ || !object3dCom_ || !ctx.commandList) return;

    DirectXCom* dx = object3dCom_->GetDirectXCom();
    if (!dx) return;

    auto* cbAllocator = dx->GetCBAllocator();
    if (!cbAllocator) return;

    // スライム専用ルートシグネチャとPSOを取得
    auto rootSig = PipelineStateManager::GetInstance()->GetRootSignature("Slime");
    auto slimePSO = PipelineStateManager::GetInstance()->GetPipelineState("Slime_Normal");
    if (!rootSig || !slimePSO) {
        // フォールバック：通常描画
        RenderContext localCtx = ctx;
        if (textureIndex_ != TextureManager::kInvalidTextureIndex) {
            localCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(textureIndex_);
        }
        object3dCom_->Draw(object3d_.get(), localCtx, modelData_, true);
        return;
    }

    // 定数バッファの準備（Object3d内部でマテリアル・変換行列をアロケート）
    object3d_->PrepareConstantBuffers(dx);

    // スライム定数バッファをGPUに書き込み
    auto slimeAlloc = cbAllocator->Allocate(sizeof(SlimeParamsCPU));
    std::memcpy(slimeAlloc.cpuAddress, &slimeParams_, sizeof(SlimeParamsCPU));

    // ルートシグネチャとPSOを設定
    ctx.commandList->SetGraphicsRootSignature(rootSig.Get());
    ctx.commandList->SetPipelineState(slimePSO.Get());

    // 0: Material (b0 Pixel)
    ctx.commandList->SetGraphicsRootConstantBufferView(0, object3d_->GetMaterialGPUAddress());

    // 1: TransformationMatrix (b0 Vertex)
    ctx.commandList->SetGraphicsRootConstantBufferView(1, object3d_->GetTransformationMatrixGPUAddress());

    // 2: Main Texture (t3 All)
    D3D12_GPU_DESCRIPTOR_HANDLE texHandle{};
    if (textureIndex_ != TextureManager::kInvalidTextureIndex) {
        texHandle = TextureManager::GetInstance()->GetSrvHandleGPU(textureIndex_);
    } else {
        texHandle = TextureManager::GetInstance()->GetSrvHandleGPU(
            TextureManager::GetInstance()->GetTextureIndexByFilePath("Resources/CG4/human/white.png"));
    }
    if (texHandle.ptr != 0) {
        ctx.commandList->SetGraphicsRootDescriptorTable(2, texHandle);
    }

    // 3: SlimeParams (b1 All: Vertex & Pixel)
    ctx.commandList->SetGraphicsRootConstantBufferView(3, slimeAlloc.gpuAddress);

    // 4: DirectionalLight (b2 Pixel)
    if (ctx.light && ctx.light->GetDirectionalLightResource()) {
        ctx.commandList->SetGraphicsRootConstantBufferView(4, ctx.light->GetDirectionalLightResource()->GetGPUVirtualAddress());
    } else {
        ctx.commandList->SetGraphicsRootConstantBufferView(4, object3d_->GetDirectionalLightGPUAddress());
    }

    // 5: Camera (b3 Pixel)
    if (ctx.camera && ctx.camera->GetCameraGpuAddress() != 0) {
        ctx.commandList->SetGraphicsRootConstantBufferView(5, ctx.camera->GetCameraGpuAddress());
    }

    // 6: Cube Environment Map (t4 Pixel)
    uint32_t skyboxIndex = SceneManager::GetInstance()->GetSkyboxTextureIndex();
    D3D12_GPU_DESCRIPTOR_HANDLE skyboxHandle{};
    if (skyboxIndex != TextureManager::kInvalidTextureIndex) {
        skyboxHandle = TextureManager::GetInstance()->GetSrvHandleGPU(skyboxIndex);
    } else {
        skyboxHandle = texHandle;
    }
    if (skyboxHandle.ptr != 0) {
        ctx.commandList->SetGraphicsRootDescriptorTable(6, skyboxHandle);
    }

    // 頂点バッファとインデックスバッファを設定して描画
    auto vbv = object3d_->GetVertexBufferView();
    ctx.commandList->IASetVertexBuffers(0, 1, &vbv);
    ctx.commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    if (object3d_->HasIndexBuffer()) {
        auto ibv = object3d_->GetIndexBufferView();
        ctx.commandList->IASetIndexBuffer(&ibv);
        ctx.commandList->DrawIndexedInstanced(static_cast<UINT>(modelData_.indices.size()), 1, 0, 0, 0);
    } else {
        ctx.commandList->DrawInstanced(static_cast<UINT>(modelData_.vertices.size()), 1, 0, 0);
    }
}

void Minion::Draw(const RenderContext& ctx) {
    if (!isActive_ || !object3d_ || !object3dCom_) return;
    DrawSlime(ctx);
}
