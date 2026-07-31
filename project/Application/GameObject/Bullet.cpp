#include "Bullet.h"
#include <cmath>
#include "TextureManager.h"

Vector3 Bullet::ComputeSpawnPosition(const Vector3& ownerPosition, const Vector3& forward, const Vector3& spawnOffset)
{
    const Vector3 right = { forward.z, 0.0f, -forward.x };
    return {
        ownerPosition.x + right.x * spawnOffset.x + forward.x * spawnOffset.z,
        ownerPosition.y + spawnOffset.y,
        ownerPosition.z + right.z * spawnOffset.x + forward.z * spawnOffset.z
    };
}

void Bullet::Initialize(Object3dCom* object3dCom, Camera* camera, const Vector3& startPosition, const Vector3& direction, float speed, float lifeTime, BulletOwner owner)
{
    object3dCom_ = object3dCom;
    camera_ = camera;
    speed_ = speed;
    lifeTime_ = lifeTime;
    age_ = 0.0f;
    isDead_ = false;
    nearMissTriggered_ = false;
    owner_ = owner;

    float len = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    if (len > 0.0001f)
    {
        direction_ = { direction.x / len, direction.y / len, direction.z / len };
    }
    else
    {
        direction_ = { 0.0f, 0.0f, 1.0f };
    }

    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(object3dCom_, "sphere.obj");
    object3d_->SetCamera(camera_);
    object3d_->SetTranslate(startPosition);
    object3d_->SetScale({ 0.3f, 0.3f, 0.3f });

    if (owner_ == BulletOwner::Player)
    {
        object3d_->SetColor({ 0.2f, 0.8f, 1.0f, 1.0f });
    }
    else
    {
        object3d_->SetColor({ 1.0f, 0.3f, 0.2f, 1.0f });
    }

    prevPosition_ = startPosition;
}

void Bullet::Update(float deltaTime)
{
    if (isDead_) return;

    if (object3d_)
    {
        prevPosition_ = object3d_->GetTranslate();

        Vector3 pos = prevPosition_;
        pos.x += direction_.x * speed_ * (deltaTime * 60.0f);
        pos.y += direction_.y * speed_ * (deltaTime * 60.0f);
        pos.z += direction_.z * speed_ * (deltaTime * 60.0f);

        object3d_->SetTranslate(pos);

        Vector3 rot = object3d_->GetRotate();
        rot.z += 0.1f;
        object3d_->SetRotate(rot);

        object3d_->Update();
    }

    age_ += deltaTime;
    if (age_ >= lifeTime_)
    {
        isDead_ = true;
    }
}

void Bullet::Draw(const RenderContext& ctx)
{
    if (isDead_) return;
    if (object3d_)
    {
        object3d_->Draw(ctx);
    }
}

void Bullet::Finalize()
{
    object3d_.reset();
    isDead_ = true;
}
