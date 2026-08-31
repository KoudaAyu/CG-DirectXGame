#include "PikminPlayer.h"
#include "Application/Minion/MinionManager.h"
#include "Application/GameObject/SlimeMesh.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3dCom.h"
#include "Baziru3_Engine/Core/Base/Pipeline/PipelineStateManager.h"
#include "Baziru3_Engine/Core/Base/Allocator/ConstantBufferAllocator.h"
#include "Baziru3_Engine/Framework/Collision/CollisionManager.h"
#include "TextureManager.h"
#include "DirectXCom.h"
#include "SceneManager.h"
#include "Light.h"
#include "KeyInput.h"
#include <cmath>
#include <algorithm>
#include <cstring>

namespace {
    constexpr float kPi = 3.14159265358979323846f;
}

PikminPlayer::PikminPlayer() {
    normalModel_ = std::make_unique<Object3d>();
    giantModel_ = std::make_unique<Object3d>();
}

PikminPlayer::~PikminPlayer() {
    if (collider_) {
        CollisionManager::GetInstance()->UnregisterCollider(collider_.get());
    }
}

void PikminPlayer::Initialize(Object3dCom* object3dCom, Camera* camera, const Vector3& startPos) {
    object3dCom_ = object3dCom;
    camera_ = camera;
    position_ = startPos;
    position_.y = 0.5f;
    rotation_ = { 0.0f, 0.0f, 0.0f };
    scale_ = { 0.8f, 0.8f, 0.8f };
    isMerged_ = false;

    // スライム球体メッシュを生成（滑らかな高密度メッシュ）
    normalModelData_ = SlimeMesh::GenerateSphere(64, 32, 1.0f);
    normalTextureIndex_ = TextureManager::GetInstance()->Load("Resources/uvChecker.png");
    normalModelData_.material.textureIndex = normalTextureIndex_;

    if (normalModel_) {
        normalModel_->Initialize(object3dCom_, normalModelData_);
        normalModel_->SetCamera(camera_);
        normalModel_->SetTranslate(position_);
        normalModel_->SetScale(scale_);
        normalModel_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f }); // カラーはシェーダー側のbaseColorで制御
        normalModel_->SetEnableLighting(true);
        normalModel_->Update();
    }

    // 巨大化モデル（合体モード用：さらに高密度）
    giantModelData_ = SlimeMesh::GenerateSphere(80, 40, 1.0f);
    giantTextureIndex_ = normalTextureIndex_;
    giantModelData_.material.textureIndex = giantTextureIndex_;

    if (giantModel_) {
        giantModel_->Initialize(object3dCom_, giantModelData_);
        giantModel_->SetCamera(camera_);
        giantModel_->SetTranslate(position_);
        giantModel_->SetScale({ 2.0f, 2.0f, 2.0f });
        giantModel_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        giantModel_->SetEnableLighting(true);
        giantModel_->Update();
    }

    // スライムパラメータの初期設定
    slimeParams_.baseColor = { 0.2f, 0.85f, 1.0f, 0.9f }; // 水色スライム
    slimeParams_.wobbleStrength = 0.12f;
    slimeParams_.wobbleFrequency = 4.0f;
    slimeParams_.fresnelPower = 3.0f;
    slimeParams_.envReflection = 0.4f;
    slimeParams_.innerGlow = 0.4f;
    slimeParams_.specularShininess = 64.0f;

    // 合体時の当たり判定（SphereCollider）
    collider_ = std::make_unique<SphereCollider>(2.0f, &position_, CollisionAttribute::Player);
    CollisionManager::GetInstance()->RegisterCollider(collider_.get());
}

void PikminPlayer::SetPosition(const Vector3& pos) {
    position_ = pos;
    if (normalModel_) normalModel_->SetTranslate(pos);
    if (giantModel_) giantModel_->SetTranslate(pos);
}

Vector3 PikminPlayer::GetForwardVector() const {
    return { std::sin(rotation_.y), 0.0f, std::cos(rotation_.y) };
}

void PikminPlayer::ToggleMerge() {
    SetMerged(!isMerged_);
}

void PikminPlayer::SetMerged(bool merged) {
    isMerged_ = merged;
    mergeScaleAnimation_ = 0.0f;

    // 合体/分裂時に衝撃波紋を発生させる
    slimeParams_.impulseStrength = 0.3f;

    // コライダー半径を切り替え
    if (collider_) {
        collider_->SetRadius(merged ? 2.0f : 0.8f);
    }
}

void PikminPlayer::Update(float deltaTime, KeyInput* keyInput, MinionManager* minionManager) {
    throwCooldownTimer_ -= deltaTime;
    mergeScaleAnimation_ = (std::min)(1.0f, mergeScaleAnimation_ + deltaTime * 4.0f);
    totalTime_ += deltaTime;

    // 衝撃波紋の減衰
    slimeParams_.impulseStrength *= (1.0f - deltaTime * 5.0f);
    if (slimeParams_.impulseStrength < 0.001f) slimeParams_.impulseStrength = 0.0f;

    Vector3 moveDir = { 0.0f, 0.0f, 0.0f };

    if (keyInput) {
        if (keyInput->PushKey(DIK_W) || keyInput->PushKey(DIK_UP)) moveDir.z += 1.0f;
        if (keyInput->PushKey(DIK_S) || keyInput->PushKey(DIK_DOWN)) moveDir.z -= 1.0f;
        if (keyInput->PushKey(DIK_A) || keyInput->PushKey(DIK_LEFT)) moveDir.x -= 1.0f;
        if (keyInput->PushKey(DIK_D) || keyInput->PushKey(DIK_RIGHT)) moveDir.x += 1.0f;

        if (keyInput->TriggerKey(DIK_E)) {
            ToggleMerge();
        }

        if (keyInput->PushKey(DIK_F) || keyInput->TriggerKey(DIK_J)) {
            if (throwCooldownTimer_ <= 0.0f && minionManager && !isMerged_) {
                Vector3 launchPos = position_ + GetForwardVector() * 0.8f;
                launchPos.y += 0.5f;
                if (minionManager->ThrowMinion(launchPos, GetForwardVector(), throwPower_, throwUpPower_)) {
                    throwCooldownTimer_ = throwCooldown_;
                }
            }
        }

        if (keyInput->PushKey(DIK_Q)) {
            if (minionManager) {
                minionManager->Whistle(position_, 10.0f);
            }
        }
    }

    float currentSpeed = isMerged_ ? mergedMoveSpeed_ : normalMoveSpeed_;
    float len = std::sqrt(moveDir.x * moveDir.x + moveDir.z * moveDir.z);

    // 現在の速度を記録（スクワッシュ計算用）
    Vector3 currentVelocity = { 0.0f, 0.0f, 0.0f };

    if (len > 0.001f) {
        moveDir.x /= len;
        moveDir.z /= len;

        currentVelocity.x = moveDir.x * currentSpeed;
        currentVelocity.z = moveDir.z * currentSpeed;

        position_.x += moveDir.x * currentSpeed * deltaTime;
        position_.z += moveDir.z * currentSpeed * deltaTime;

        float targetYaw = std::atan2(moveDir.x, moveDir.z);
        float diff = targetYaw - rotation_.y;
        while (diff > kPi) diff -= 2.0f * kPi;
        while (diff < -kPi) diff += 2.0f * kPi;
        rotation_.y += diff * (std::min)(1.0f, rotationSpeed_ * deltaTime);

        if (isMerged_) {
            rotation_.x += currentSpeed * deltaTime * 1.5f;
        } else {
            rotation_.x = 0.0f;
        }
    } else {
        if (!isMerged_) {
            rotation_.x = 0.0f;
        }
    }

    position_.y = isMerged_ ? 1.0f : 0.5f;

    // --- スクワッシュ＆ストレッチの計算（移動の慣性から） ---
    Vector3 accel = {
        (currentVelocity.x - prevVelocity_.x) / (std::max)(deltaTime, 0.001f),
        0.0f,
        (currentVelocity.z - prevVelocity_.z) / (std::max)(deltaTime, 0.001f)
    };
    prevVelocity_ = currentVelocity;

    // 加速度の大きさに基づいてスクワッシュを計算
    float accelMag = std::sqrt(accel.x * accel.x + accel.z * accel.z);
    float squashFactor = (std::min)(accelMag * 0.003f, 0.15f);

    // スクワッシュのスムーズ減衰
    slimeParams_.squashStretch.x = slimeParams_.squashStretch.x * 0.85f + squashFactor * 0.15f;
    slimeParams_.squashStretch.z = slimeParams_.squashStretch.z * 0.85f + squashFactor * 0.15f;
    slimeParams_.squashStretch.y = slimeParams_.squashStretch.y * 0.85f + (-squashFactor * 0.5f) * 0.15f;

    // 合体・分裂アニメーション時の強いスクワッシュ
    if (mergeScaleAnimation_ < 0.5f) {
        float t = mergeScaleAnimation_ * 2.0f;
        float bounce = std::sin(t * kPi * 2.0f) * 0.2f * (1.0f - t);
        slimeParams_.squashStretch.y += bounce;
    }

    // シェーダー時間の更新
    slimeParams_.time = totalTime_;

    if (isMerged_) {
        float t = mergeScaleAnimation_;
        float bounce = 1.0f + std::sin(t * kPi) * 0.4f;
        float baseScale = 2.0f * (0.5f + 0.5f * t);
        scale_ = { baseScale * bounce, baseScale * bounce, baseScale * bounce };

        // 合体時のスライムカラー（黄金色）
        slimeParams_.baseColor = { 1.0f, 0.8f, 0.2f, 0.92f };

        if (giantModel_) {
            giantModel_->SetTranslate(position_);
            giantModel_->SetRotate(rotation_);
            giantModel_->SetScale(scale_);
            giantModel_->Update();
        }
    } else {
        scale_ = { 0.8f, 0.8f, 0.8f };

        // 通常時のスライムカラー（水色）
        slimeParams_.baseColor = { 0.2f, 0.85f, 1.0f, 0.9f };

        if (normalModel_) {
            normalModel_->SetTranslate(position_);
            normalModel_->SetRotate(rotation_);
            normalModel_->SetScale(scale_);
            normalModel_->Update();
        }
    }
}

void PikminPlayer::DrawSlime(Object3d* object, const Object3d::ModelData& modelData,
                              const RenderContext& ctx, uint32_t textureIndex)
{
    if (!object || !object3dCom_ || !ctx.commandList) return;

    DirectXCom* dx = object3dCom_->GetDirectXCom();
    if (!dx) return;

    auto* cbAllocator = dx->GetCBAllocator();
    if (!cbAllocator) return;

    // スライム専用ルートシグネチャとPSOを取得
    auto rootSig = PipelineStateManager::GetInstance()->GetRootSignature("Slime");
    auto slimePSO = PipelineStateManager::GetInstance()->GetPipelineState("Slime_Normal");
    if (!rootSig || !slimePSO) {
        // フォールバック：通常のObject3D描画
        RenderContext localCtx = ctx;
        if (textureIndex != TextureManager::kInvalidTextureIndex) {
            localCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(textureIndex);
        }
        object3dCom_->Draw(object, localCtx, modelData, true);
        return;
    }

    // 定数バッファの準備（Object3d内部でマテリアル・変換行列をアロケート）
    object->PrepareConstantBuffers(dx);

    // スライム定数バッファをGPUに書き込み
    auto slimeAlloc = cbAllocator->Allocate(sizeof(SlimeParamsCPU));
    std::memcpy(slimeAlloc.cpuAddress, &slimeParams_, sizeof(SlimeParamsCPU));

    // ルートシグネチャとPSOを設定
    ctx.commandList->SetGraphicsRootSignature(rootSig.Get());
    ctx.commandList->SetPipelineState(slimePSO.Get());

    // 0: Material (b0 Pixel)
    ctx.commandList->SetGraphicsRootConstantBufferView(0, object->GetMaterialGPUAddress());

    // 1: TransformationMatrix (b0 Vertex)
    ctx.commandList->SetGraphicsRootConstantBufferView(1, object->GetTransformationMatrixGPUAddress());

    // 2: Main Texture (t3 All)
    D3D12_GPU_DESCRIPTOR_HANDLE texHandle{};
    if (textureIndex != TextureManager::kInvalidTextureIndex) {
        texHandle = TextureManager::GetInstance()->GetSrvHandleGPU(textureIndex);
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
        ctx.commandList->SetGraphicsRootConstantBufferView(4, object->GetDirectionalLightGPUAddress());
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
    auto vbv = object->GetVertexBufferView();
    ctx.commandList->IASetVertexBuffers(0, 1, &vbv);
    ctx.commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    if (object->HasIndexBuffer()) {
        auto ibv = object->GetIndexBufferView();
        ctx.commandList->IASetIndexBuffer(&ibv);
        ctx.commandList->DrawIndexedInstanced(static_cast<UINT>(modelData.indices.size()), 1, 0, 0, 0);
    } else {
        ctx.commandList->DrawInstanced(static_cast<UINT>(modelData.vertices.size()), 1, 0, 0);
    }
}

void PikminPlayer::Draw(const RenderContext& ctx) {
    if (!object3dCom_) return;

    if (isMerged_) {
        if (giantModel_) {
            DrawSlime(giantModel_.get(), giantModelData_, ctx, giantTextureIndex_);
        }
    } else {
        if (normalModel_) {
            DrawSlime(normalModel_.get(), normalModelData_, ctx, normalTextureIndex_);
        }
    }
}
