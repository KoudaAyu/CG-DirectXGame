#include "Minion.h"
#include "Application/GameObject/SlimeMesh.h"
#include "Application/GameObject/SlimePhysics.h"
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
    slimeParams_.wobbleStrength = 0.0f;
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

    if (info.other && info.other->GetAttribute() == CollisionAttribute::Obstacle) {
        // 同一フレーム内の多重衝突および直後の連続ヒットを防止（中心部でのピンボール・振動を完全封殺）
        if (obstacleCooldown_ > 0.0f) return;

        // 障害物（プロペラなど）の基準位置（回転中心）を正確に取得
        // BoxCollider の場合、GetWorldPosition() - GetPositionOffset() により中心のワールド座標を取得可能
        Vector3 obstacleBasePos = info.other->GetWorldPosition() - info.other->GetPositionOffset();

        // プロペラ中心からミニオンへ向かう動径ベクトル（水平面）
        Vector3 radial = { position_.x - obstacleBasePos.x, 0.0f, position_.z - obstacleBasePos.z };
        float rLen = std::sqrt(radial.x * radial.x + radial.z * radial.z);

        Vector3 escapeDir{ 0.0f, 0.0f, 0.0f };

        if (rLen > 0.15f) {
            // 中心から十分に離れている場合は、純粋な外向き動径方向へ弾き出す
            escapeDir = { radial.x / rLen, 0.0f, radial.z / rLen };
        } else {
            // プロペラ回転中心へのド直撃（特異点）の場合:
            // 飛んできた入射方向の逆向き（跳ね返り反射ベクトル）を優先採用
            Vector3 incoming = { -velocity_.x, 0.0f, -velocity_.z };
            float incSpeed = std::sqrt(incoming.x * incoming.x + incoming.z * incoming.z);
            if (incSpeed > 0.1f) {
                escapeDir = { incoming.x / incSpeed, 0.0f, incoming.z / incSpeed };
            } else if (rLen > 1e-4f) {
                escapeDir = { radial.x / rLen, 0.0f, radial.z / rLen };
            } else {
                // 静止して中心にある場合の安全フォールバック（手前向き）
                escapeDir = { 0.0f, 0.0f, -1.0f };
            }
        }

        // 1. めり込みの強制解消（エンジン側の押し出しに加えて、外向きへ安全マージンを補正）
        if (info.depth > 0.005f) {
            position_.x += escapeDir.x * (info.depth * 0.5f);
            position_.z += escapeDir.z * (info.depth * 0.5f);
        }

        // 2. 爽快な放物線バウンドによる弾き飛ばし初速を付与
        float launchSpeed = 11.0f; // 外向き水平初速
        velocity_.x = escapeDir.x * launchSpeed;
        velocity_.z = escapeDir.z * launchSpeed;
        velocity_.y = 3.8f;        // 上向き跳ね上げ初速（ポーンと小さく放物線を描く）

        // 空中バウンド状態へ移行（床に着地した瞬間に自然に Rolling へ復帰）
        state_ = MinionState::Thrown;

        // クールダウン設定（0.15秒間、他の羽根からの重複ヒットや速度上書きを無効化）
        obstacleCooldown_ = 0.15f;

        // 衝突時のスライム変形（ペチャッと潰れてから弾かれる演出）
        slimeParams_.impulseStrength = 0.35f;
        slimeParams_.squashStretch = { 0.18f, -0.22f, 0.18f };
    }

    // 高速衝突時の衝撃波紋
    float impactSpeed = std::sqrt(velocity_.x * velocity_.x + velocity_.y * velocity_.y + velocity_.z * velocity_.z);
    if (impactSpeed > 1.5f) {
        float strength = (std::min)(0.35f, impactSpeed * 0.03f);
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
        object3d_->Update();
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

    if (obstacleCooldown_ > 0.0f) {
        obstacleCooldown_ -= deltaTime;
    }

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

        // 床の傾斜に沿った姿勢（まな板の上に密着して床に沿って潰れる）
        // Y軸回転を0に固定することで、オイラー角の積による法線ズレ・底面浮きを100%防止
        rotation_.x = stageTilt.x;
        rotation_.y = 0.0f;
        rotation_.z = -stageTilt.y;

        // 傾斜面の上に乗る（床の傾斜に沿って底面がピタッと接地）
        position_.y = SlimePhysics::CalculateGroundedCenterY(position_.x, position_.z, stageTilt, groundY_);
        scale_ = { 0.35f, 0.35f, 0.35f };

        // --- 液体スライムの動的変形（SlimePhysics ユーティリティで一元計算） ---
        SlimePhysics::DeformInput deformInput;
        deformInput.velocity = velocity_;
        deformInput.prevVelocity = prevVelocity_;
        deformInput.stageTilt = stageTilt;
        deformInput.deltaTime = deltaTime;
        deformInput.isGrounded = true;
        deformInput.isMerged = false;
        deformInput.massScale = 1.0f;
        SlimePhysics::UpdateDeformation(slimeParams_, deformInput);
        prevVelocity_ = velocity_;

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
        // 空中では板の影響を受けず、純粋な鉛直真下への重力のみが働く
        velocity_.y += gravity_ * deltaTime;

        // 物理位置の更新
        position_.x += velocity_.x * deltaTime;
        position_.y += velocity_.y * deltaTime;
        position_.z += velocity_.z * deltaTime;

        // 飛翔中も通常スケールを維持
        scale_ = { 0.35f, 0.35f, 0.35f };

        // 空中での姿勢（板の傾きは受けず、進行方向を向く）
        rotation_.x = 0.0f;
        rotation_.z = 0.0f;
        float horizSpeed = std::sqrt(velocity_.x * velocity_.x + velocity_.z * velocity_.z);
        if (horizSpeed > 0.2f) {
            rotation_.y = std::atan2(velocity_.x, velocity_.z);
        }

        // 空中での水滴・涙型変形（SlimePhysics ユーティリティで計算）
        SlimePhysics::DeformInput airDeformInput;
        airDeformInput.velocity = velocity_;
        airDeformInput.prevVelocity = prevVelocity_;
        airDeformInput.stageTilt = stageTilt;
        airDeformInput.deltaTime = deltaTime;
        airDeformInput.isGrounded = false;
        airDeformInput.isMerged = false;
        airDeformInput.massScale = 1.0f;
        SlimePhysics::UpdateDeformation(slimeParams_, airDeformInput);
        prevVelocity_ = velocity_;

        // 地面着地判定（傾いた板との接触判定）
        float landingY = SlimePhysics::CalculateGroundedCenterY(position_.x, position_.z, stageTilt, groundY_);
        if (position_.y <= landingY) {
            position_.y = landingY;
            velocity_.y = 0.0f;
            velocity_.x *= 0.7f;
            velocity_.z *= 0.7f;
            state_ = MinionState::Rolling;

            // 着地時の弾力スクワッシュと衝撃波紋（もっこり感を保ちつつプルンとバウンド）
            slimeParams_.squashStretch.y = -0.10f;
            slimeParams_.squashStretch.x = 0.05f;
            slimeParams_.squashStretch.z = 0.05f;
            slimeParams_.impulseStrength = 0.18f;
        }

        break;
    }

    case MinionState::Idle: {
        position_.y = SlimePhysics::CalculateGroundedCenterY(position_.x, position_.z, stageTilt, groundY_);
        scale_ = { 0.35f, 0.35f, 0.35f };
        break;
    }

    case MinionState::Carrying: {
        break;
    }
    }

    // 傾斜面の高さ変動に対する絶対安全クランプ（角度変更時にも地面の下に100%埋まらない）
    float currentGroundSurfaceY = SlimePhysics::CalculateGroundedCenterY(position_.x, position_.z, stageTilt, groundY_);
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
