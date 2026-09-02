#include "Minion.h"
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
    state_ = MinionState::Rolling;
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
    slimeParams_.wobbleStrength = 0.22f;
    slimeParams_.wobbleFrequency = 6.0f;
    slimeParams_.fresnelPower = 2.5f;
    slimeParams_.envReflection = 0.4f;
    slimeParams_.innerGlow = 0.5f;
    slimeParams_.specularShininess = 48.0f;

    // ミニオン単体の当たり判定（SphereCollider）を生成・登録 (属性: Minion)
    collider_ = std::make_unique<SphereCollider>(radius_, &position_, CollisionAttribute::Minion);
    collider_->SetOnCollision([this](const CollisionInfo& info) {
        OnCollision(info);
    });
    CollisionManager::GetInstance()->RegisterCollider(collider_.get());
}

void Minion::OnCollision(const CollisionInfo& info) {
    if (!isActive_ || state_ == MinionState::Merging) return;

    // 高速衝突時のみ控えめに衝撃波紋を付与（形状は急変させない）
    float impactSpeed = std::sqrt(velocity_.x * velocity_.x + velocity_.y * velocity_.y + velocity_.z * velocity_.z);
    if (impactSpeed > 2.0f) {
        float strength = (std::min)(0.15f, impactSpeed * 0.015f);
        slimeParams_.impulseStrength = (std::max)(slimeParams_.impulseStrength, strength);
    }
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

    // 投げ飛ばし時のスクワッシュ（進行方向にびよーんと引き伸ばし）
    slimeParams_.impulseStrength = 0.28f;
    slimeParams_.squashStretch = { 0.05f, 0.22f, 0.05f };
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

void Minion::Update(float deltaTime, const Vector2& stageTilt) {
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

    // 衝撃波紋のスムーズ減衰
    slimeParams_.impulseStrength *= (1.0f - deltaTime * 4.5f);
    if (slimeParams_.impulseStrength < 0.001f) slimeParams_.impulseStrength = 0.0f;

    switch (state_) {
    case MinionState::Rolling: {
        // ステージ傾斜による下り坂重力加速度の適用
        // stageTilt.x: ピッチ（手前/奥）、stageTilt.y: ロール（左/右）
        float accelX = std::sin(stageTilt.y) * tiltAccel_;
        float accelZ = std::sin(stageTilt.x) * tiltAccel_;

        velocity_.x += accelX * deltaTime;
        velocity_.z += accelZ * deltaTime;

        // 地面摩擦による減速
        float decay = 1.0f - (std::min)(1.0f, friction_ * deltaTime);
        velocity_.x *= decay;
        velocity_.z *= decay;

        // 物理位置更新
        position_.x += velocity_.x * deltaTime;
        position_.z += velocity_.z * deltaTime;

        // スライムとしての滑走（回転せず滑る・重力変形は常に下向き）
        rotation_.x = 0.0f;
        rotation_.z = 0.0f;

        // 進行方向への緩やかな向き変え
        float currentSpeed = std::sqrt(velocity_.x * velocity_.x + velocity_.z * velocity_.z);
        if (currentSpeed > 0.2f) {
            float targetYaw = std::atan2(velocity_.x, velocity_.z);
            float diffYaw = targetYaw - rotation_.y;
            while (diffYaw > 3.14159265f) diffYaw -= 2.0f * 3.14159265f;
            while (diffYaw < -3.14159265f) diffYaw += 2.0f * 3.14159265f;
            rotation_.y += diffYaw * (std::min)(1.0f, deltaTime * 5.0f);
        }

        // 傾斜面の上に乗る（まな板の上のスライム）
        float groundHeight = -position_.z * std::sin(stageTilt.x) - position_.x * std::sin(stageTilt.y);
        position_.y = groundHeight + groundY_;
        scale_ = { 0.35f, 0.35f, 0.35f };

        // --- ゼリースライムの動的変形（スクワッシュ＆ストレッチ） ---
        Vector3 currentVel = velocity_;
        Vector3 accel = {
            (currentVel.x - prevVelocity_.x) / (std::max)(deltaTime, 0.001f),
            0.0f,
            (currentVel.z - prevVelocity_.z) / (std::max)(deltaTime, 0.001f)
        };
        prevVelocity_ = currentVel;

        float accelMag = std::sqrt(accel.x * accel.x + accel.z * accel.z);
        float speedStretch = (std::min)(currentSpeed * 0.024f, 0.24f);
        float accelSquash = (std::min)(accelMag * 0.005f, 0.2f);
        float sag = -0.12f; // 接地重力による常時ポテッとした強い潰れ

        float targetSquashY = sag - accelSquash * 0.55f - speedStretch * 0.35f;
        float targetSquashXZ = speedStretch * 0.7f + accelSquash * 0.35f - sag * 0.6f;

        // スムーズ補間
        slimeParams_.squashStretch.y += (targetSquashY - slimeParams_.squashStretch.y) * (std::min)(1.0f, deltaTime * 10.0f);
        slimeParams_.squashStretch.x += (targetSquashXZ - slimeParams_.squashStretch.x) * (std::min)(1.0f, deltaTime * 10.0f);
        slimeParams_.squashStretch.z += (targetSquashXZ - slimeParams_.squashStretch.z) * (std::min)(1.0f, deltaTime * 10.0f);

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

        // 地面着地判定（傾斜面に追従：跳ねずにペタッと着地）
        float groundHeight = -position_.z * std::sin(stageTilt.x) - position_.x * std::sin(stageTilt.y);
        float landingY = groundHeight + groundY_;
        if (position_.y <= landingY) {
            position_.y = landingY;
            velocity_.y = 0.0f;
            velocity_.x *= 0.7f;
            velocity_.z *= 0.7f;
            state_ = MinionState::Rolling;

            // 着地時の潰れスクワッシュと衝撃波紋
            slimeParams_.squashStretch.y = -0.18f;
            slimeParams_.squashStretch.x = 0.09f;
            slimeParams_.squashStretch.z = 0.09f;
            slimeParams_.impulseStrength = 0.15f;
        }

        // 飛翔中の回転演出
        rotation_.x += 10.0f * deltaTime;

        // 飛翔中は進行方向に引き伸ばし
        float speed = std::sqrt(velocity_.x * velocity_.x + velocity_.z * velocity_.z);
        slimeParams_.squashStretch.y = (std::min)(speed * 0.01f, 0.15f);

        break;
    }

    case MinionState::Idle: {
        float groundHeight = -position_.z * std::sin(stageTilt.x) - position_.x * std::sin(stageTilt.y);
        position_.y = groundHeight + groundY_;
        scale_ = { 0.35f, 0.35f, 0.35f };
        break;
    }

    case MinionState::Carrying: {
        break;
    }
    }

    // 傾斜面の高さ変動に対する絶対安全クランプ（角度変更時にも地面の下に100%埋まらない）
    float currentGroundSurfaceY = -position_.z * std::sin(stageTilt.x) - position_.x * std::sin(stageTilt.y) + groundY_;
    if (position_.y < currentGroundSurfaceY) {
        position_.y = currentGroundSurfaceY;
        if (state_ == MinionState::Thrown) {
            velocity_.y = 0.0f;
            state_ = MinionState::Rolling;
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
