#define NOMINMAX
#include "EnemyBullet.h"

#include "Baziru3_Engine/Graphics/3D/Object/Object3dCom.h"

#include <cmath>

void EnemyBullet::Setup(Object3dCom* object3dCom, Camera* camera,
                        const Object3d::ModelData& modelData, const std::string& modelKey,
                        const Vector4& color)
{
    modelKey_ = modelKey;
    color_ = color;

    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(object3dCom, modelData);
    object3d_->SetCamera(camera);
    object3d_->SetColor(color);
    object3d_->SetEnableLighting(true);
    object3d_->SetAllowWireframeOverlay(false);

    isAlive_ = false;
}

void EnemyBullet::Fire(const Vector3& startPos, const Vector3& direction,
                       float speed, float lifeTime, float scale, float hitRadius)
{
    if (!object3d_) return;

    float len = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    Vector3 dir = (len > 1e-4f) ? Vector3{ direction.x / len, direction.y / len, direction.z / len }
                                : Vector3{ 0.0f, 0.0f, 1.0f };

    position_ = startPos;
    velocity_ = dir * speed;
    rotation_ = { 0.0f, std::atan2(dir.x, dir.z), 0.0f };

    scale_ = scale;
    hitRadius_ = hitRadius;
    lifeTime_ = lifeTime;
    age_ = 0.0f;
    isAlive_ = true;

    object3d_->SetTranslate(position_);
    object3d_->SetRotate(rotation_);
    object3d_->SetScale({ scale_, scale_, scale_ });
    object3d_->Update();
}

void EnemyBullet::Update(float deltaTime)
{
    if (!isAlive_ || !object3d_) return;

    position_ = position_ + velocity_ * deltaTime;

    // 飛んでいる間くるくる回して弾らしさを出す
    rotation_.z += 7.0f * deltaTime;

    age_ += deltaTime;
    if (age_ >= lifeTime_)
    {
        isAlive_ = false;
        return;
    }

    object3d_->SetTranslate(position_);
    object3d_->SetRotate(rotation_);
    object3d_->SetScale({ scale_, scale_, scale_ });
    object3d_->SetDeltaTime(deltaTime);
    object3d_->Update();
}

void EnemyBullet::Draw(const RenderContext& ctx)
{
    if (!isAlive_ || !object3d_) return;
    object3d_->Draw(ctx);
}
