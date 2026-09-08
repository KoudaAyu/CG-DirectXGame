#include "Slime.h"
#include "Application/GameObject/SlimeCollision.h"
#include "Application/GameObject/SlimeMesh.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3dCom.h"
#include "Baziru3_Engine/Core/Camera/Camera.h"
#include "Baziru3_Engine/Core/Base/DirectXCom.h"
#include "Baziru3_Engine/Core/Base/Allocator/ConstantBufferAllocator.h"
#include "Baziru3_Engine/Core/Base/Pipeline/PipelineStateManager.h"
#include "Baziru3_Engine/Graphics/2D/Texture/TextureManager.h"
#include "Baziru3_Engine/Framework/Collision/CollisionManager.h"
#include "SceneManager.h"
#include "Light.h"
#include <cmath>
#include <algorithm>
#include <cstring>

Slime::Slime() {
    object3d_ = std::make_unique<Object3d>();
}

Slime::~Slime() {
    if (meshCollider_) {
        CollisionManager::GetInstance()->UnregisterCollider(meshCollider_.get());
    }
}

void Slime::Initialize(Object3dCom* object3dCom, Camera* camera, const Vector3& startPos, int initialSize) {
    object3dCom_ = object3dCom;
    camera_ = camera;
    position_ = startPos;
    spawnPos_ = startPos;
    rotation_ = { 0.0f, 0.0f, 0.0f };
    velocity_ = { 0.0f, 0.0f, 0.0f };
    state_ = SlimeState::Rolling;
    isActive_ = true;
    isGrounded_ = false;

    // 滑らかなスライム球体メッシュを生成
    modelData_ = SlimeMesh::GenerateSphere(64, 32, 1.0f);
    textureIndex_ = TextureManager::GetInstance()->Load("Resources/uvChecker.png");
    modelData_.material.textureIndex = textureIndex_;

    if (object3d_) {
        object3d_->Initialize(object3dCom_, modelData_);
        object3d_->SetCamera(camera_);
        object3d_->SetTranslate(position_);
        object3d_->SetScale(scale_);
        object3d_->SetRotate(rotation_);
        object3d_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        object3d_->SetEnableLighting(true);
        object3d_->Update();
    }

    // スライムパラメータの初期設定
    slimeParams_.wobbleStrength = 0.0f;
    slimeParams_.wobbleFrequency = 5.5f;
    slimeParams_.fresnelPower = 2.5f;
    slimeParams_.envReflection = 0.45f;
    slimeParams_.innerGlow = 0.5f;
    slimeParams_.specularShininess = 52.0f;

    // メッシュ当たり判定コライダー
    meshCollider_ = std::make_unique<MeshCollider>(object3d_.get(), CollisionAttribute::Minion);
    meshCollider_->SetOnCollision([this](const CollisionInfo& info) {
        OnCollision(info);
    });
    CollisionManager::GetInstance()->RegisterCollider(meshCollider_.get());

    SetSize(initialSize);
}

float Slime::CalculateScaleBySize(int s) const {
    if (isTitleException_) return 0.8f;
    if (s <= 1) return 0.40f;
    return 0.40f + 0.11f * (s - 1) + 0.05f * std::pow(static_cast<float>(s - 1), 1.25f);
}

void Slime::SetSize(int s) {
    if (isTitleException_) {
        size_ = 1;
        scale_ = { 0.8f, 0.8f, 0.8f };
        radius_ = 0.8f * 0.78f;
        groundY_ = 0.8f * 0.75f;
        slimeParams_.baseColor = { 0.2f, 0.85f, 1.0f, 0.95f };
    } else {
        size_ = (std::max)(1, s);
        float sVal = CalculateScaleBySize(size_);
        scale_ = { sVal, sVal, sVal };
        radius_ = sVal * 0.78f;
        groundY_ = sVal * 0.75f;
        slimeParams_.baseColor = SlimePhysics::GetColorBySize(size_);
    }

    currentMergedScale_ = scale_.x;

    if (object3d_) {
        object3d_->SetScale(scale_);
        object3d_->Update();
    }
    if (meshCollider_) {
        meshCollider_->SetWorldPosition(position_);
        meshCollider_->Update();
    }
}

void Slime::SetTitleException(bool isTitle) {
    isTitleException_ = isTitle;
    if (isTitle) {
        SetSize(1);
    }
}

void Slime::SetPosition(const Vector3& pos) {
    position_ = pos;
    if (object3d_) {
        object3d_->SetTranslate(position_);
        object3d_->Update();
    }
    if (meshCollider_) {
        meshCollider_->SetWorldPosition(position_);
        meshCollider_->Update();
    }
}

void Slime::SetActive(bool active) {
    isActive_ = active;
    if (meshCollider_) {
        meshCollider_->SetIsEnabled(active && state_ != SlimeState::Merging);
    }
}

void Slime::Jump(float jumpVelocity) {
    BounceFromStage(Vector3{ 0.0f, 1.0f, 0.0f }, jumpVelocity);
}

void Slime::BounceFromStage(const Vector3& groundNormal, float bouncePower) {
    // 法線方向（床に対して垂直）を主体としつつ、上方向成分(+Y)をブレンド
    Vector3 launchDir = groundNormal * 0.70f + Vector3{ 0.0f, 1.0f, 0.0f } * 0.30f;
    float len = std::sqrt(launchDir.x * launchDir.x + launchDir.y * launchDir.y + launchDir.z * launchDir.z);
    if (len > 1e-4f) {
        launchDir = launchDir * (1.0f / len);
    }

    // 床の突き上げ速度を慣性に加算（斜面を転がっている勢いを活かす）
    Vector3 addVel = launchDir * bouncePower;
    velocity_.x += addVel.x * 0.60f;
    velocity_.z += addVel.z * 0.60f;
    // 上方向の打ち上げ速度（最低保証付き）
    velocity_.y = (std::max)(velocity_.y + addVel.y, bouncePower * 0.85f);

    isGrounded_ = false;
    state_ = SlimeState::Thrown;

    // 床からの強烈な突き上げによる瞬間的な平べったい潰れ変形（Squash）と衝撃波
    slimeParams_.squashStretch = { 0.28f, -0.38f, 0.28f };
    slimeParams_.impulseStrength = 0.85f;
}

void Slime::Launch(const Vector3& velocity) {
    velocity_ = velocity;
    isGrounded_ = false;
    state_ = SlimeState::Thrown;
}

void Slime::AttractTo(const Vector3& targetPos, float speed) {
    state_ = SlimeState::Merging;
    Vector3 diff = targetPos - position_;
    float len = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
    if (len > 0.05f) {
        velocity_ = diff * (speed / len);
    }
    if (meshCollider_) {
        meshCollider_->SetIsEnabled(false);
    }
}

void Slime::OnCollision(const CollisionInfo& info) {
    if (!isActive_ || obstacleCooldown_ > 0.0f) return;

    // 衝突時の反発
    float impactSpeed = std::sqrt(velocity_.x * velocity_.x + velocity_.y * velocity_.y + velocity_.z * velocity_.z);
    if (impactSpeed > 1.2f) {
        float strength = (std::min)(0.35f, impactSpeed * 0.03f);
        slimeParams_.impulseStrength = (std::max)(slimeParams_.impulseStrength, strength);
        slimeParams_.squashStretch = { 0.16f, -0.20f, 0.16f };
    }
}

void Slime::Update(float deltaTime, const Vector2& stageTilt, const Vector2& pivot) {
    if (!isActive_) return;

    totalTime_ += deltaTime;
    bounceTimer_ += deltaTime;

    if (obstacleCooldown_ > 0.0f) {
        obstacleCooldown_ -= deltaTime;
    }
    if (mergeCooldown_ > 0.0f) {
        mergeCooldown_ -= deltaTime;
    }

    // 衝撃波紋の減衰
    slimeParams_.impulseStrength *= (1.0f - (std::min)(1.0f, deltaTime * 4.8f));
    if (slimeParams_.impulseStrength < 0.001f) slimeParams_.impulseStrength = 0.0f;

    UpdatePhysics(deltaTime, stageTilt, pivot);

    // 奈落への落下防止セーフティ（島から落下した場合は安全に上空から復帰）
    if (position_.y < -35.0f) {
        position_ = { spawnPos_.x, spawnPos_.y + 2.5f, spawnPos_.z };
        velocity_ = { 0.0f, 0.0f, 0.0f };
        isGrounded_ = false;
        state_ = SlimeState::Thrown;
    }

    // --- 液体スライムの動的変形（空中/接地状態を正しく反映） ---
    SlimePhysics::DeformInput deformInput;
    deformInput.velocity = velocity_;
    deformInput.prevVelocity = prevVelocity_;
    deformInput.stageTilt = stageTilt;
    deformInput.deltaTime = deltaTime;
    deformInput.isGrounded = isGrounded_;
    deformInput.isMerged = IsMerged();
    deformInput.massScale = scale_.x;
    SlimePhysics::UpdateDeformation(slimeParams_, deformInput);
    prevVelocity_ = velocity_;

    slimeParams_.time = totalTime_;

    // モデル座標・スケール同期
    if (object3d_) {
        object3d_->SetTranslate(position_);
        object3d_->SetScale(scale_);
        object3d_->SetRotate(rotation_);
        object3d_->Update();
    }
    if (meshCollider_) {
        meshCollider_->SetWorldPosition(position_);
        meshCollider_->Update();
    }
}

void Slime::UpdatePhysics(float deltaTime, const Vector2& stageTilt, const Vector2& pivot) {
    const float kGravity = -32.0f;

    switch (state_) {
    case SlimeState::Rolling: {
        // ステージ傾斜による下り坂重力加速度
        float accelX = std::sin(stageTilt.y) * tiltAccel_;
        float accelZ = std::sin(stageTilt.x) * tiltAccel_;

        velocity_.x += accelX * deltaTime;
        velocity_.z += accelZ * deltaTime;

        // 地面摩擦による減速
        float currentFriction = isGrounded_ ? SlimePhysics::GetFriction() : (SlimePhysics::GetFriction() * 0.20f);
        float decay = 1.0f - (std::min)(1.0f, currentFriction * deltaTime);
        velocity_.x *= decay;
        velocity_.z *= decay;

        // 水平位置更新
        position_.x += velocity_.x * deltaTime;
        position_.z += velocity_.z * deltaTime;

        // 壁メッシュとの衝突解決
        SlimePhysics::ResolveWallCollision(position_, velocity_, scale_.x * 0.92f);

        // 傾斜面・地面メッシュとの接地判定 (接地中なので isGrounded = true を明示的に指定)
        bool hasGround = false;
        Vector3 groundNormal{ 0.0f, 1.0f, 0.0f };
        float targetGroundY = SlimePhysics::CalculateGroundedCenterYEx(
            position_.x, position_.z, position_.y, stageTilt, groundY_, &hasGround, &groundNormal, pivot, true);

        if (!hasGround || (targetGroundY - position_.y < -1.5f)) {
            // 足場から飛び出した（崖や段差からの飛び降り） -> 空中放物線状態へ移行
            state_ = SlimeState::Thrown;
            isGrounded_ = false;
        } else {
            isGrounded_ = true;

            // スロープスライディング
            float slopeHorizLen = std::sqrt(groundNormal.x * groundNormal.x + groundNormal.z * groundNormal.z);
            if (groundNormal.y < 0.82f && slopeHorizLen > 0.01f) {
                Vector2 slopeDown = { groundNormal.x / slopeHorizLen, groundNormal.z / slopeHorizLen };
                float slideStrength = (1.0f - groundNormal.y) * 22.0f;
                velocity_.x += slopeDown.x * slideStrength * deltaTime;
                velocity_.z += slopeDown.y * slideStrength * deltaTime;

                float velDotDown = velocity_.x * slopeDown.x + velocity_.z * slopeDown.y;
                if (velDotDown < 0.0f) {
                    float cancelFactor = std::clamp((0.82f - groundNormal.y) / 0.18f, 0.0f, 1.0f);
                    velocity_.x -= slopeDown.x * (velDotDown * cancelFactor);
                    velocity_.z -= slopeDown.y * (velDotDown * cancelFactor);
                }
            }

            float dy = targetGroundY - position_.y;
            if (dy > 0.0f) {
                position_.y = targetGroundY;
            } else {
                position_.y += dy * (std::min)(1.0f, deltaTime * 35.0f);
            }
            velocity_.y = 0.0f;

            // 接地中の姿勢（局所地形法線に正しく沿って密着）
            float targetRotX = std::atan2(groundNormal.z, groundNormal.y);
            float targetRotZ = -std::atan2(groundNormal.x, groundNormal.y);
            rotation_.x += (targetRotX - rotation_.x) * (std::min)(1.0f, deltaTime * 35.0f);
            rotation_.y = 0.0f;
            rotation_.z += (targetRotZ - rotation_.z) * (std::min)(1.0f, deltaTime * 35.0f);
        }
        break;
    }

    case SlimeState::Thrown: {
        // 重力加速
        velocity_.y += kGravity * deltaTime;
        if (velocity_.y < -45.0f) velocity_.y = -45.0f; // 終端落下速度制限

        // 水平減速（空中慣性）
        float decay = 1.0f - (std::min)(1.0f, SlimePhysics::GetFriction() * 0.15f * deltaTime);
        velocity_.x *= decay;
        velocity_.z *= decay;

        Vector3 prevPos = position_;
        position_.x += velocity_.x * deltaTime;
        position_.y += velocity_.y * deltaTime;
        position_.z += velocity_.z * deltaTime;

        // 壁衝突
        SlimePhysics::ResolveWallCollision(position_, velocity_, scale_.x * 0.92f);

        // 空中での姿勢: 進行方向を向く
        rotation_.x = 0.0f;
        rotation_.z = 0.0f;
        float horizSpeed = std::sqrt(velocity_.x * velocity_.x + velocity_.z * velocity_.z);
        if (horizSpeed > 0.3f) {
            rotation_.y = std::atan2(velocity_.x, velocity_.z);
        }

        // 着地判定 (空中・落下中なので isGrounded = false を明示的に指定！)
        bool hasGround = false;
        Vector3 groundNormal{ 0.0f, 1.0f, 0.0f };
        float targetGroundY = SlimePhysics::CalculateGroundedCenterYEx(
            position_.x, position_.z, position_.y, stageTilt, groundY_, &hasGround, &groundNormal, pivot, false);

        // 着地判定:
        // 1. 通常着地: 足元が目標地面以下に到達 (position_.y <= targetGroundY)
        // 2. 高速落下・飛び降り時の貫通検出 (CCD): 前フレームで地面より上（または至近）にいて、今フレームで床面以下に突き抜けた
        bool isCrossingGround = (prevPos.y >= targetGroundY - 0.35f && position_.y <= targetGroundY + 0.10f);
        if (hasGround && velocity_.y <= 0.0f && (position_.y <= targetGroundY || isCrossingGround)) {
            position_.y = targetGroundY;
            float impactSpeed = -velocity_.y;
            isGrounded_ = true;

            // スライム特有の弾性着地バウンド
            if (impactSpeed > 7.0f) {
                velocity_.y = impactSpeed * 0.22f; // 小バウンド
                isGrounded_ = false;
            } else {
                velocity_.y = 0.0f;
                state_ = SlimeState::Rolling;
            }

            // 着地時の弾力スクワッシュ（ぷるんと潰れて復元）
            float squashAmount = std::clamp(impactSpeed * 0.02f, 0.08f, 0.35f);
            slimeParams_.squashStretch = { squashAmount * 0.5f, -squashAmount, squashAmount * 0.5f };
            slimeParams_.impulseStrength = std::clamp(impactSpeed * 0.04f, 0.15f, 0.60f);
        }
        break;
    }

    case SlimeState::Merging: {
        position_.x += velocity_.x * deltaTime;
        position_.y += velocity_.y * deltaTime;
        position_.z += velocity_.z * deltaTime;
        break;
    }

    case SlimeState::Idle:
    default:
        break;
    }
}

void Slime::Draw(const RenderContext& ctx) {
    if (!isActive_ || !object3d_ || !object3dCom_) return;
    DrawSlime(ctx);
}

void Slime::DrawSlime(const RenderContext& ctx) {
    if (!object3d_ || !object3dCom_ || !ctx.commandList) return;

    DirectXCom* dx = object3dCom_->GetDirectXCom();
    if (!dx) return;

    auto* cbAllocator = dx->GetCBAllocator();
    if (!cbAllocator) return;

    // スライム専用ルートシグネチャとPSOを取得
    auto rootSig = PipelineStateManager::GetInstance()->GetRootSignature("Slime");
    auto slimePSO = PipelineStateManager::GetInstance()->GetPipelineState("Slime_Normal");
    if (!rootSig || !slimePSO) {
        // フォールバック描画
        RenderContext localCtx = ctx;
        if (textureIndex_ != TextureManager::kInvalidTextureIndex) {
            localCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(textureIndex_);
        }
        object3dCom_->Draw(object3d_.get(), localCtx, modelData_, true);
        return;
    }

    object3d_->PrepareConstantBuffers(dx);

    auto slimeAlloc = cbAllocator->Allocate(sizeof(SlimeParamsCPU));
    std::memcpy(slimeAlloc.cpuAddress, &slimeParams_, sizeof(SlimeParamsCPU));

    ctx.commandList->SetGraphicsRootSignature(rootSig.Get());
    ctx.commandList->SetPipelineState(slimePSO.Get());

    // 0: Material
    ctx.commandList->SetGraphicsRootConstantBufferView(0, object3d_->GetMaterialGPUAddress());

    // 1: TransformationMatrix
    ctx.commandList->SetGraphicsRootConstantBufferView(1, object3d_->GetTransformationMatrixGPUAddress());

    // 2: Main Texture
    D3D12_GPU_DESCRIPTOR_HANDLE texHandle{};
    if (textureIndex_ != TextureManager::kInvalidTextureIndex) {
        texHandle = TextureManager::GetInstance()->GetSrvHandleGPU(textureIndex_);
    } else {
        texHandle = TextureManager::GetInstance()->GetSrvHandleGPU(
            TextureManager::GetInstance()->GetTextureIndexByFilePath("Resources/uvChecker.png"));
    }
    if (texHandle.ptr != 0) {
        ctx.commandList->SetGraphicsRootDescriptorTable(2, texHandle);
    }

    // 3: SlimeParams
    ctx.commandList->SetGraphicsRootConstantBufferView(3, slimeAlloc.gpuAddress);

    // 4: DirectionalLight
    if (ctx.light && ctx.light->GetDirectionalLightResource()) {
        ctx.commandList->SetGraphicsRootConstantBufferView(4, ctx.light->GetDirectionalLightResource()->GetGPUVirtualAddress());
    } else {
        ctx.commandList->SetGraphicsRootConstantBufferView(4, object3d_->GetDirectionalLightGPUAddress());
    }

    // 5: Camera
    if (ctx.camera && ctx.camera->GetCameraGpuAddress() != 0) {
        ctx.commandList->SetGraphicsRootConstantBufferView(5, ctx.camera->GetCameraGpuAddress());
    }

    // 6: Cube Environment Map
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

void Slime::DrawDebug(Camera* cam) {
#ifdef _DEBUG
    if (!cam || !isActive_ || state_ == SlimeState::Merging) return;
    auto shape = SlimeCollision::BuildMultiSphere(position_, scale_, slimeParams_.squashStretch, rotation_);
    uint32_t color = 0xFF00FF7F;
    if (size_ >= 8) color = 0xFF4040FF;         // 赤 (大)
    else if (size_ >= 3) color = 0xFF00FFFF;    // 黄 (中)
    else color = 0xFFFF8000;                    // 青 (小)
    SlimeCollision::DrawDebugMultiSphere(shape, cam, color);
#endif
}
