#include "Minion.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3dCom.h"
#include "TextureManager.h"
#include <algorithm>
#include <cmath>

Minion::Minion() {
    object3d_ = std::make_unique<Object3d>();
}

void Minion::Initialize(Object3dCom* object3dCom, Camera* camera, const Vector3& spawnPos, MinionType type) {
    object3dCom_ = object3dCom;
    camera_ = camera;
    position_ = spawnPos;
    type_ = type;
    state_ = MinionState::Following;
    isActive_ = true;

    // モデル読み込み
    modelData_ = Object3d::LoadObjFile("Resources", "suzanne.obj");
    textureIndex_ = TextureManager::GetInstance()->Load("Resources/uvChecker.png");
    modelData_.material.textureIndex = textureIndex_;

    if (object3d_) {
        object3d_->Initialize(object3dCom_, modelData_);
        object3d_->SetCamera(camera_);
        object3d_->SetTranslate(position_);
        object3d_->SetScale(scale_);
        object3d_->SetRotate(rotation_);
        object3d_->SetColor({ 1.0f, 0.3f, 0.3f, 1.0f }); // 赤色ミニオン
        object3d_->Update();
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
    if (!isActive_) return;

    bounceTimer_ += deltaTime;

    switch (state_) {
    case MinionState::Following: {
        // スロット座標に向かってスムーズに移動 (Lerp / 追従)
        Vector3 diff = targetSlotPos_ - position_;
        diff.y = 0.0f;
        float dist = std::sqrt(diff.x * diff.x + diff.z * diff.z);

        if (dist > 0.05f) {
            float step = followSpeed_ * deltaTime;
            if (step > dist) step = dist;
            position_.x += (diff.x / dist) * step;
            position_.z += (diff.z / dist) * step;

            // 移動方向を向く
            rotation_.y = std::atan2(diff.x, diff.z);
        }

        // 接地Y座標の維持 ＋ ピョコピョコ跳ね
        position_.y = groundY_ + std::sin(bounceTimer_ * 14.0f) * 0.08f;
        scale_ = { 0.35f, 0.35f, 0.35f };
        break;
    }

    case MinionState::Merging: {
        // プレイヤー中心に向かって吸引移動
        position_ += velocity_ * deltaTime;
        scale_ = { 0.25f, 0.25f, 0.25f };
        break;
    }

    case MinionState::Thrown: {
        // 放物線移動（重力適用）
        velocity_.y += gravity_ * deltaTime;
        position_ += velocity_ * deltaTime;

        // 地面着地判定
        if (position_.y <= groundY_) {
            position_.y = groundY_;
            // 着地バウンド
            if (std::abs(velocity_.y) > 2.0f) {
                velocity_.y = -velocity_.y * 0.4f;
                velocity_.x *= 0.6f;
                velocity_.z *= 0.6f;
            } else {
                velocity_ = { 0.0f, 0.0f, 0.0f };
                state_ = MinionState::Following;
            }
        }

        // 飛翔中の回転演出
        rotation_.x += 10.0f * deltaTime;
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

    if (object3d_) {
        object3d_->SetTranslate(position_);
        object3d_->SetRotate(rotation_);
        object3d_->SetScale(scale_);
        object3d_->Update();
    }
}

void Minion::Draw(const RenderContext& ctx) {
    if (!isActive_ || !object3d_ || !object3dCom_) return;

    RenderContext localCtx = ctx;
    if (textureIndex_ != TextureManager::kInvalidTextureIndex) {
        localCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(textureIndex_);
    }
    object3dCom_->Draw(object3d_.get(), localCtx, modelData_, true);
}
