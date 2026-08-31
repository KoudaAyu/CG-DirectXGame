#include "PikminPlayer.h"
#include "Application/Minion/MinionManager.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3dCom.h"
#include "TextureManager.h"
#include "KeyInput.h"
#include <cmath>
#include <algorithm>

namespace {
    constexpr float kPi = 3.14159265358979323846f;
}

PikminPlayer::PikminPlayer() {
    normalModel_ = std::make_unique<Object3d>();
    giantModel_ = std::make_unique<Object3d>();
}

void PikminPlayer::Initialize(Object3dCom* object3dCom, Camera* camera, const Vector3& startPos) {
    object3dCom_ = object3dCom;
    camera_ = camera;
    position_ = startPos;
    position_.y = 0.5f;
    rotation_ = { 0.0f, 0.0f, 0.0f };
    scale_ = { 0.8f, 0.8f, 0.8f };
    isMerged_ = false;

    // 通常モデル (suzanne.obj)
    normalModelData_ = Object3d::LoadObjFile("Resources", "suzanne.obj");
    normalTextureIndex_ = TextureManager::GetInstance()->Load("Resources/uvChecker.png");
    normalModelData_.material.textureIndex = normalTextureIndex_;

    if (normalModel_) {
        normalModel_->Initialize(object3dCom_, normalModelData_);
        normalModel_->SetCamera(camera_);
        normalModel_->SetTranslate(position_);
        normalModel_->SetScale(scale_);
        normalModel_->SetColor({ 0.2f, 0.9f, 1.0f, 1.0f }); // 水色リーダー
        normalModel_->Update();
    }

    // 巨大化モデル (suzanne.obj)
    giantModelData_ = Object3d::LoadObjFile("Resources", "suzanne.obj");
    giantTextureIndex_ = normalTextureIndex_;
    giantModelData_.material.textureIndex = giantTextureIndex_;

    if (giantModel_) {
        giantModel_->Initialize(object3dCom_, giantModelData_);
        giantModel_->SetCamera(camera_);
        giantModel_->SetTranslate(position_);
        giantModel_->SetScale({ 2.0f, 2.0f, 2.0f });
        giantModel_->SetColor({ 1.0f, 0.8f, 0.2f, 1.0f }); // 黄金巨大スライム
        giantModel_->Update();
    }
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
}

void PikminPlayer::Update(float deltaTime, KeyInput* keyInput, MinionManager* minionManager) {
    throwCooldownTimer_ -= deltaTime;
    mergeScaleAnimation_ = (std::min)(1.0f, mergeScaleAnimation_ + deltaTime * 4.0f);

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

    if (len > 0.001f) {
        moveDir.x /= len;
        moveDir.z /= len;

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

    if (isMerged_) {
        float t = mergeScaleAnimation_;
        float bounce = 1.0f + std::sin(t * kPi) * 0.4f;
        float baseScale = 2.0f * (0.5f + 0.5f * t);
        scale_ = { baseScale * bounce, baseScale * bounce, baseScale * bounce };

        if (giantModel_) {
            giantModel_->SetTranslate(position_);
            giantModel_->SetRotate(rotation_);
            giantModel_->SetScale(scale_);
            giantModel_->Update();
        }
    } else {
        scale_ = { 0.8f, 0.8f, 0.8f };
        if (normalModel_) {
            normalModel_->SetTranslate(position_);
            normalModel_->SetRotate(rotation_);
            normalModel_->SetScale(scale_);
            normalModel_->Update();
        }
    }
}

void PikminPlayer::Draw(const RenderContext& ctx) {
    if (!object3dCom_) return;

    RenderContext localCtx = ctx;

    if (isMerged_) {
        if (giantModel_) {
            if (giantTextureIndex_ != TextureManager::kInvalidTextureIndex) {
                localCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(giantTextureIndex_);
            }
            object3dCom_->Draw(giantModel_.get(), localCtx, giantModelData_, true);
        }
    } else {
        if (normalModel_) {
            if (normalTextureIndex_ != TextureManager::kInvalidTextureIndex) {
                localCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(normalTextureIndex_);
            }
            object3dCom_->Draw(normalModel_.get(), localCtx, normalModelData_, true);
        }
    }
}
