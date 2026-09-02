#include "PikminPlayer.h"
#include "Application/Minion/MinionManager.h"
#include "Application/GameObject/SlimeMesh.h"
#include "Application/GameObject/AimGuide.h"
#include "Baziru3_Engine/Core/IO/Mouse/MouseInput.h"
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

float PikminPlayer::CalculateMergedScale(int minionCount) const {
    if (minionCount <= 0) return 0.8f;
    return 0.8f + 0.24f * std::pow(static_cast<float>(minionCount), 0.65f);
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
    if (!merged) {
        lastAbsorbedCount_ = 0;
        currentMergedScale_ = 0.8f;
    }

    // 合体/分裂時に衝撃波紋を発生させる
    slimeParams_.impulseStrength = 0.3f;

    // コライダー半径を初期化
    if (collider_) {
        collider_->SetRadius(merged ? currentMergedScale_ : 0.8f);
    }
}

void PikminPlayer::Update(float deltaTime, KeyInput* keyInput, MinionManager* minionManager, MouseInput* mouseInput, AimGuide* aimGuide, const Vector2& stageTilt) {
    throwCooldownTimer_ -= deltaTime;
    mergeScaleAnimation_ = (std::min)(1.0f, mergeScaleAnimation_ + deltaTime * 4.0f);
    totalTime_ += deltaTime;

    // 衝撃波紋の減衰
    slimeParams_.impulseStrength *= (1.0f - deltaTime * 5.0f);
    if (slimeParams_.impulseStrength < 0.001f) slimeParams_.impulseStrength = 0.0f;

    if (keyInput) {
        if (keyInput->TriggerKey(DIK_E)) {
            ToggleMerge();
        }
    }

    // --- マウスによる投擲 ---
    if (!isMerged_ && mouseInput && minionManager) {
        // マウス左クリックで目標地点へ投擲（クリック単発 または 押しっぱなし連射）
        bool isThrowRequested = mouseInput->TriggerButton(0) || mouseInput->PushButton(0);
        if (isThrowRequested && throwCooldownTimer_ <= 0.0f) {
            Vector3 launchPos = position_;
            launchPos.y += 0.5f;

            if (aimGuide && aimGuide->IsTargetValid()) {
                Vector3 targetVel = aimGuide->GetCalculatedVelocity();
                if (minionManager->ThrowMinionWithVelocity(launchPos, targetVel)) {
                    throwCooldownTimer_ = 0.12f; // リズミカルな連射間隔

                    // 投擲時にプレイヤーの向きを目標地点に向ける
                    Vector3 targetPos = aimGuide->GetTargetPosition();
                    float aimYaw = std::atan2(targetPos.x - position_.x, targetPos.z - position_.z);
                    rotation_.y = aimYaw;
                }
            }
        }
    }

    // --- ステージ傾斜による物理加速度と摩擦（ティルト移動） ---
    // stageTilt.x: ピッチ（手前/奥）、stageTilt.y: ロール（左/右）
    float accelScale = isMerged_ ? (tiltAccel_ * 1.3f) : tiltAccel_;
    float accelX = -std::sin(stageTilt.y) * accelScale;
    float accelZ = std::sin(stageTilt.x) * accelScale;

    velocity_.x += accelX * deltaTime;
    velocity_.z += accelZ * deltaTime;

    // 地面摩擦によるスムーズ減速（合体時は慣性を大きくして滑らかに転がる）
    float currentFriction = isMerged_ ? (friction_ * 0.7f) : friction_;
    float decay = 1.0f - (std::min)(1.0f, currentFriction * deltaTime);
    velocity_.x *= decay;
    velocity_.z *= decay;

    // 物理位置の更新
    position_.x += velocity_.x * deltaTime;
    position_.z += velocity_.z * deltaTime;

    // 球体としての転がり回転（Rolling Rotation）
    float rollSpeedFactor = isMerged_ ? 2.5f : 4.0f;
    rotation_.x += velocity_.z * deltaTime * rollSpeedFactor;
    rotation_.z -= velocity_.x * deltaTime * rollSpeedFactor;

    // 進行方向へのスムーズな旋回（通常時）
    float speed = std::sqrt(velocity_.x * velocity_.x + velocity_.z * velocity_.z);
    if (speed > 0.2f && !isMerged_) {
        float targetYaw = std::atan2(velocity_.x, velocity_.z);
        float diff = targetYaw - rotation_.y;
        while (diff > kPi) diff -= 2.0f * kPi;
        while (diff < -kPi) diff += 2.0f * kPi;
        rotation_.y += diff * (std::min)(1.0f, rotationSpeed_ * deltaTime);
    }

    // --- スクワッシュ＆ストレッチの計算（移動の慣性から） ---
    Vector3 currentVelocity = velocity_;
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
        int mergedCount = minionManager ? minionManager->GetMergedCount() : 0;
        float targetScale = CalculateMergedScale(mergedCount);

        // 新たなミニオンを吸収した時に衝撃波紋パルス
        if (mergedCount > lastAbsorbedCount_) {
            slimeParams_.impulseStrength = (std::min)(0.5f, slimeParams_.impulseStrength + 0.15f);
            lastAbsorbedCount_ = mergedCount;
        }

        // 合体サイズの動的スムーズ補間
        currentMergedScale_ += (targetScale - currentMergedScale_) * (std::min)(1.0f, deltaTime * 8.0f);

        float t = mergeScaleAnimation_;
        float bounce = 1.0f + std::sin(t * kPi) * 0.25f;
        float currentScale = currentMergedScale_ * bounce;
        scale_ = { currentScale, currentScale, currentScale };

        // 合体時のスライムカラー（黄金色）
        slimeParams_.baseColor = { 1.0f, 0.8f, 0.2f, 0.92f };

        // 接地高さ・コライダー半径を動的スケールに追従
        position_.y = currentScale * 0.5f;
        if (collider_) {
            collider_->SetRadius(currentScale);
        }

        if (giantModel_) {
            giantModel_->SetTranslate(position_);
            giantModel_->SetRotate(rotation_);
            giantModel_->SetScale(scale_);
            giantModel_->Update();
        }
    } else {
        lastAbsorbedCount_ = 0;
        currentMergedScale_ = 0.8f;
        scale_ = { 0.8f, 0.8f, 0.8f };

        // 通常時のスライムカラー（水色）
        slimeParams_.baseColor = { 0.2f, 0.85f, 1.0f, 0.9f };

        position_.y = 0.5f;
        if (collider_) {
            collider_->SetRadius(0.8f);
        }

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
