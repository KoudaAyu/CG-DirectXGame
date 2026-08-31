#include "Minion.h"
#include <algorithm>
#include <cmath>

Minion::Minion() {
    object3d_ = std::make_unique<Object3d>();
}

void Minion::Initialize(Object3dCom* object3dCom, const Vector3& spawnPos, MinionType type) {
    position_ = spawnPos;
    type_ = type;
    state_ = MinionState::Following;
    isActive_ = true;

    if (object3d_) {
        bool loaded = object3d_->Initialize("Resources", "suzanne.obj");
        if (!loaded) {
            object3d_->Initialize("Resources", "plane.obj");
        }
        object3d_->SetTranslate(position_);
        object3d_->SetScale(scale_);
        object3d_->SetRotate(rotation_);
        object3d_->SetColor({ 1.0f, 0.3f, 0.3f, 1.0f });
        if (object3dCom) {
            object3d_->SetObject3dCom(object3dCom);
        }
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
        Vector3 diff = targetSlotPos_ - position_;
        diff.y = 0.0f;
        float dist = std::sqrt(diff.x * diff.x + diff.z * diff.z);

        if (dist > 0.05f) {
            float step = followSpeed_ * deltaTime;
            if (step > dist) step = dist;
            position_.x += (diff.x / dist) * step;
            position_.z += (diff.z / dist) * step;

            rotation_.y = std::atan2(diff.x, diff.z);
        }

        position_.y = groundY_ + std::sin(bounceTimer_ * 14.0f) * 0.08f;
        scale_ = { 0.35f, 0.35f, 0.35f };
        break;
    }

    case MinionState::Merging: {
        position_ += velocity_ * deltaTime;
        scale_ = { 0.25f, 0.25f, 0.25f };
        break;
    }

    case MinionState::Thrown: {
        velocity_.y += gravity_ * deltaTime;
        position_ += velocity_ * deltaTime;

        if (position_.y <= groundY_) {
            position_.y = groundY_;
            if (std::abs(velocity_.y) > 2.0f) {
                velocity_.y = -velocity_.y * 0.4f;
                velocity_.x *= 0.6f;
                velocity_.z *= 0.6f;
            } else {
                velocity_ = { 0.0f, 0.0f, 0.0f };
                state_ = MinionState::Following;
            }
        }

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
    if (!isActive_ || !object3d_) return;
    object3d_->Draw(ctx);
}
