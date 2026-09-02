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

    // スライムパラメータの初期設定（ゼリー感のあるぷるぷるスライム）
    slimeParams_.baseColor = { 0.2f, 0.85f, 1.0f, 0.9f }; // 水色スライム
    slimeParams_.wobbleStrength = 0.22f;
    slimeParams_.wobbleFrequency = 5.0f;
    slimeParams_.fresnelPower = 2.5f;
    slimeParams_.envReflection = 0.5f;
    slimeParams_.innerGlow = 0.5f;
    slimeParams_.specularShininess = 64.0f;

    // プレイヤーの当たり判定（SphereCollider）
    collider_ = std::make_unique<SphereCollider>(0.8f, &position_, CollisionAttribute::Player);
    collider_->SetOnCollision([this](const CollisionInfo& info) {
        OnCollision(info);
    });
    CollisionManager::GetInstance()->RegisterCollider(collider_.get());
}

void PikminPlayer::OnCollision(const CollisionInfo& info) {
    // 高速衝突時のみ控えめに衝撃波紋を付与（形状は急変させない）
    float impactSpeed = std::sqrt(velocity_.x * velocity_.x + velocity_.z * velocity_.z);
    if (impactSpeed > 2.5f) {
        float strength = (std::min)(0.2f, impactSpeed * 0.02f);
        slimeParams_.impulseStrength = (std::max)(slimeParams_.impulseStrength, strength);
    }
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
                }
            }
        }
    }

    // --- ステージ傾斜による物理加速度と摩擦（ティルト移動） ---
    // stageTilt.x: ピッチ（手前/奥）、stageTilt.y: ロール（左/右）
    float accelScale = isMerged_ ? (tiltAccel_ * 1.3f) : tiltAccel_;
    float accelX = std::sin(stageTilt.y) * accelScale;
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

    // 床の傾斜に沿った姿勢（まな板の上に密着して床に沿って潰れる）
    // Y軸回転を0に固定することで、オイラー角の積による法線ズレ・底面浮きを100%防止
    rotation_.x = stageTilt.x;
    rotation_.y = 0.0f;
    rotation_.z = -stageTilt.y;

    // --- 液体スライムの傾斜流動＆スクワッシュ変形 ---
    Vector3 currentVelocity = velocity_;
    Vector3 accel = {
        (currentVelocity.x - prevVelocity_.x) / (std::max)(deltaTime, 0.001f),
        0.0f,
        (currentVelocity.z - prevVelocity_.z) / (std::max)(deltaTime, 0.001f)
    };
    prevVelocity_ = currentVelocity;

    float speedMag = std::sqrt(currentVelocity.x * currentVelocity.x + currentVelocity.z * currentVelocity.z);
    float tiltMag = std::sqrt(stageTilt.x * stageTilt.x + stageTilt.y * stageTilt.y);

    // ステージ傾斜による液体流動ベクトル（傾けた下り坂方向へのドロッとした内容物移動）
    float tiltFlowFactor = isMerged_ ? 2.2f : 1.8f;
    float targetFlowX = std::sin(stageTilt.y) * tiltFlowFactor + currentVelocity.x * 0.02f;
    float targetFlowZ = std::sin(stageTilt.x) * tiltFlowFactor + currentVelocity.z * 0.02f;

    // 傾斜および接地重力による上下の強い平坦化（傾けるほど平たく潰れて流れる）
    float sag = isMerged_ ? -0.22f : -0.16f;
    float targetSquashY = sag - (std::min)(tiltMag * 0.45f + speedMag * 0.02f, 0.35f);

    // スムーズスプリング補間（流動と潰れの滑らかな追従）
    slimeParams_.squashStretch.x += (targetFlowX - slimeParams_.squashStretch.x) * (std::min)(1.0f, deltaTime * 12.0f);
    slimeParams_.squashStretch.z += (targetFlowZ - slimeParams_.squashStretch.z) * (std::min)(1.0f, deltaTime * 12.0f);
    slimeParams_.squashStretch.y += (targetSquashY - slimeParams_.squashStretch.y) * (std::min)(1.0f, deltaTime * 12.0f);

    // シェーダー時間の更新
    slimeParams_.time = totalTime_;

    // 傾斜面の法線補正
    float nx = std::sin(stageTilt.y);
    float nz = std::sin(stageTilt.x);
    float nLen = std::sqrt(nx * nx + 1.0f + nz * nz);
    float groundHeight = -position_.z * nz - position_.x * nx;

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

        float currentScale = currentMergedScale_;
        scale_ = { currentScale, currentScale, currentScale };

        // 合体時のスライムカラー（黄金色）
        slimeParams_.baseColor = { 1.0f, 0.8f, 0.2f, 0.92f };

        // 傾斜面の上に乗る（床に沿って底面がピタッと完全接地）
        position_.y = groundHeight + (currentScale * 0.65f) / nLen;
        if (collider_) {
            collider_->SetRadius(currentScale * 0.8f);
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

        position_.y = groundHeight + (scale_.x * 0.65f) / nLen;
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
