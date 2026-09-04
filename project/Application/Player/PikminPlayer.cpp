#include "PikminPlayer.h"
#include "Application/Minion/MinionManager.h"
#include "Application/GameObject/SlimeMesh.h"
#include "Application/GameObject/SlimePhysics.h"
#include "Application/GameObject/SlimeCollision.h"
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
    slimeParams_.wobbleStrength = 0.0f;
    slimeParams_.wobbleFrequency = 5.0f;
    slimeParams_.fresnelPower = 2.5f;
    slimeParams_.envReflection = 0.5f;
    slimeParams_.innerGlow = 0.5f;
    slimeParams_.specularShininess = 64.0f;

    // プレイヤーの当たり判定（SphereCollider: 分裂時は小ロコロコサイズ 0.35f）
    collider_ = std::make_unique<SphereCollider>(0.35f, &position_, CollisionAttribute::Player);
    collider_->SetOnCollision([this](const CollisionInfo& info) {
        OnCollision(info);
    });
    CollisionManager::GetInstance()->RegisterCollider(collider_.get());
}

void PikminPlayer::OnCollision(const CollisionInfo& info) {
    if (info.other && info.other->GetAttribute() == CollisionAttribute::Obstacle) {
        // 同一フレーム内の多重衝突および連続ヒットを防止（中心部での振動・多重加速を防止）
        if (obstacleCooldown_ > 0.0f) return;

        // 障害物（プロペラなど）の基準位置（回転中心）を正確に取得
        Vector3 obstacleBasePos = info.other->GetWorldPosition() - info.other->GetPositionOffset();

        // プロペラ中心からプレイヤーへ向かう動径ベクトル（水平面）
        Vector3 radial = { position_.x - obstacleBasePos.x, 0.0f, position_.z - obstacleBasePos.z };
        float rLen = std::sqrt(radial.x * radial.x + radial.z * radial.z);

        Vector3 escapeDir{ 0.0f, 0.0f, 0.0f };

        if (rLen > 0.2f) {
            // 外向き動径方向へ弾き出す
            escapeDir = { radial.x / rLen, 0.0f, radial.z / rLen };
        } else {
            // プロペラ回転中心へのド直撃（特異点）の場合:
            // 進行方向の逆向き（跳ね返り反射ベクトル）を優先採用
            Vector3 incoming = { -velocity_.x, 0.0f, -velocity_.z };
            float incSpeed = std::sqrt(incoming.x * incoming.x + incoming.z * incoming.z);
            if (incSpeed > 0.1f) {
                escapeDir = { incoming.x / incSpeed, 0.0f, incoming.z / incSpeed };
            } else if (rLen > 1e-4f) {
                escapeDir = { radial.x / rLen, 0.0f, radial.z / rLen };
            } else {
                escapeDir = { 0.0f, 0.0f, -1.0f };
            }
        }

        // 1. めり込みの強制解消（エンジン側の押し出しに加えて、外向きへ安全マージンを補正）
        if (info.depth > 0.01f) {
            position_.x += escapeDir.x * (info.depth * 0.5f);
            position_.z += escapeDir.z * (info.depth * 0.5f);
        }

        // 2. 外向き脱出初速の付与（プロペラから勢いよく弾き出される慣性）
        float launchSpeed = isMerged_ ? 6.5f : 8.5f;
        velocity_.x = escapeDir.x * launchSpeed;
        velocity_.z = escapeDir.z * launchSpeed;

        // クールダウン設定（0.12秒間、重複ヒットを無効化）
        obstacleCooldown_ = 0.12f;

        // 衝突時のスライム変形（衝撃波紋とスクワッシュ）
        slimeParams_.impulseStrength = (std::max)(slimeParams_.impulseStrength, 0.4f);
        slimeParams_.squashStretch = { 0.25f, -0.2f, 0.25f };
    }

    // 高速衝突時の衝撃波紋
    float impactSpeed = std::sqrt(velocity_.x * velocity_.x + velocity_.z * velocity_.z);
    if (impactSpeed > 1.5f) {
        float strength = (std::min)(0.35f, impactSpeed * 0.03f);
        slimeParams_.impulseStrength = (std::max)(slimeParams_.impulseStrength, strength);
    }
}

float PikminPlayer::CalculateScaleBySize(int size) const {
    if (size <= 1) return 0.40f;
    // 1から10にかけて滑らかに0.40fから約1.85fへ拡大
    return 0.40f + 0.11f * (size - 1) + 0.05f * std::pow(static_cast<float>(size - 1), 1.25f);
}

void PikminPlayer::SetSize(int s) {
    size_ = (std::max)(1, s);
    isMerged_ = (size_ > 1);
    lastAbsorbedCount_ = size_ - 1;
    currentMergedScale_ = CalculateScaleBySize(size_);
    scale_ = { currentMergedScale_, currentMergedScale_, currentMergedScale_ };
    slimeParams_.baseColor = SlimePhysics::GetColorBySize(size_);
    if (collider_) {
        collider_->SetRadius(currentMergedScale_ * 0.78f);
    }
    if (size_ >= 3 && giantModel_) {
        giantModel_->SetScale(scale_);
        giantModel_->Update();
    } else if (normalModel_) {
        normalModel_->SetScale(scale_);
        normalModel_->Update();
    }
}

float PikminPlayer::CalculateMergedScale(int minionCount) const {
    return CalculateScaleBySize(minionCount + 1);
}

void PikminPlayer::SetPosition(const Vector3& pos) {
    position_ = pos;
    if (normalModel_) {
        normalModel_->SetTranslate(pos);
        normalModel_->Update();
    }
    if (giantModel_) {
        giantModel_->SetTranslate(pos);
        giantModel_->Update();
    }
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
        size_ = 1;
        currentMergedScale_ = 0.4f;
    }

    // 合体/分裂時に衝撃波紋を発生させる
    slimeParams_.impulseStrength = 0.3f;

    // コライダー半径を初期化
    if (collider_) {
        collider_->SetRadius(merged ? currentMergedScale_ : 0.35f);
    }
}

void PikminPlayer::Update(float deltaTime, KeyInput* keyInput, MinionManager* minionManager, MouseInput* mouseInput, AimGuide* aimGuide, const Vector2& stageTilt) {
    throwCooldownTimer_ -= deltaTime;
    if (obstacleCooldown_ > 0.0f) {
        obstacleCooldown_ -= deltaTime;
    }
    mergeScaleAnimation_ = (std::min)(1.0f, mergeScaleAnimation_ + deltaTime * 4.0f);
    totalTime_ += deltaTime;

    // 衝撃波紋の減衰
    slimeParams_.impulseStrength *= (1.0f - deltaTime * 5.0f);
    if (slimeParams_.impulseStrength < 0.001f) slimeParams_.impulseStrength = 0.0f;

    if (keyInput) {
        if (keyInput->TriggerKey(DIK_E)) {
            // ロコロコ方式分裂: 合体中ならパァンと全員飛び散って小ロコロコに分裂！
            if (minionManager && minionManager->GetAbsorbedCount() > 0) {
                minionManager->TriggerSplit(position_);
                isMerged_ = false;
                lastAbsorbedCount_ = 0;
                size_ = 1;
                currentMergedScale_ = 0.4f;
                scale_ = { 0.4f, 0.4f, 0.4f };
                slimeParams_.baseColor = SlimePhysics::GetColorBySize(1); // 小 (1-2) は青
                slimeParams_.impulseStrength = 0.45f;
                slimeParams_.squashStretch = { 0.25f, -0.2f, 0.25f };
            }
        }
    }

    // --- ステージ傾斜による物理加速度と摩擦（ティルト移動） ---
    // stageTilt.x: ピッチ（手前/奥）、stageTilt.y: ロール（左/右）
    float accelScale = isMerged_ ? (tiltAccel_ * 1.25f) : tiltAccel_;
    float accelX = std::sin(stageTilt.y) * accelScale;
    float accelZ = std::sin(stageTilt.x) * accelScale;

    velocity_.x += accelX * deltaTime;
    velocity_.z += accelZ * deltaTime;

    // 地面摩擦によるスムーズ減速（全スライム共通摩擦係数）
    float currentFriction = SlimePhysics::GetFriction();
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

    // --- 液体スライムの動的変形（SlimePhysics ユーティリティで一元計算） ---
    SlimePhysics::DeformInput deformInput;
    deformInput.velocity = velocity_;
    deformInput.prevVelocity = prevVelocity_;
    deformInput.stageTilt = stageTilt;
    deformInput.deltaTime = deltaTime;
    deformInput.isGrounded = true;
    deformInput.isMerged = isMerged_;
    deformInput.massScale = scale_.x;
    SlimePhysics::UpdateDeformation(slimeParams_, deformInput);
    prevVelocity_ = velocity_;

    slimeParams_.time = totalTime_;

    // --- 大きさ（1-10）と色（小:青, 中:黄, 大:赤）の管理 ---
    int absorbedCount = minionManager ? minionManager->GetAbsorbedCount() : 0;
    int newSize = 1 + absorbedCount;
    isMerged_ = (newSize > 1);

    if (newSize > size_) {
        // 新たなくっつきが発生！（1+1=2、2+1=3...）
        bool tierChanged = (size_ <= 2 && newSize >= 3) || (size_ <= 7 && newSize >= 8);
        slimeParams_.impulseStrength = tierChanged ? 0.40f : (std::min)(0.5f, slimeParams_.impulseStrength + 0.22f);
    }
    size_ = newSize;

    // 大きさに応じた色設定（小 1-2: 青, 中 3-7: 黄色, 大 8-10以上: 赤）
    slimeParams_.baseColor = SlimePhysics::GetColorBySize(size_);

    float targetScale = CalculateScaleBySize(size_);
    currentMergedScale_ += (targetScale - currentMergedScale_) * (std::min)(1.0f, deltaTime * 10.0f);
    float currentScale = currentMergedScale_;
    scale_ = { currentScale, currentScale, currentScale };

    // 傾斜面の上に乗る（床にピタッと完全接地）
    position_.y = SlimePhysics::CalculateGroundedCenterY(position_.x, position_.z, stageTilt, currentScale * 0.73f, { position_.x, position_.z });
    if (collider_) {
        collider_->SetRadius(currentScale * 0.78f);
    }

    // サイズに応じたモデル選択（小 1-2: normalModel_, 中・大 3-10: giantModel_）
    if (size_ >= 3 && giantModel_) {
        giantModel_->SetTranslate(position_);
        giantModel_->SetRotate(rotation_);
        giantModel_->SetScale(scale_);
        giantModel_->Update();
    } else if (normalModel_) {
        normalModel_->SetTranslate(position_);
        normalModel_->SetRotate(rotation_);
        normalModel_->SetScale(scale_);
        normalModel_->Update();
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

void PikminPlayer::DrawDebug(Camera* camera) {
#ifdef _DEBUG
    if (camera) {
        auto shape = SlimeCollision::BuildMultiSphere(position_, scale_, slimeParams_.squashStretch, rotation_);
        uint32_t color = isMerged_ ? 0xFF00D7FF : 0xFFFFFF00; // 金色 or 水色 (ABGR/ImGui)
        SlimeCollision::DrawDebugMultiSphere(shape, camera, color);
    }
#endif
}
