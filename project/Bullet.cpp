#include "Bullet.h"
#include <cmath>
#include "TextureManager.h"
#include "CustomObject3dRenderer.h"

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
    elapsed_ = 0.0f;
    isDead_ = false;
    owner_ = owner;

    const float len = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    if (len > 1e-6f)
    {
        direction_ = { direction.x / len, direction.y / len, direction.z / len };
    }
    else
    {
        direction_ = { 0.0f, 0.0f, 1.0f };
    }

    Object3d::ModelData model = Object3d::LoadObjFile("Resources", "plane.obj");
    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(object3dCom_, model);

    object3d_->SetTranslate(startPosition);
    object3d_->SetScale({ 0.2f, 0.2f, 0.2f });

    Vector3 r = object3d_->GetRotate();
    r.y = std::atan2(direction_.x, direction_.z);
    object3d_->SetRotate(r);

    if (model.material.textureFilePath.empty())
    {
        defaultTextureIndex_ = TextureManager::GetInstance()->Load("Resources/uvChecker.png");
    }

    // 敵の弾は赤色にして視覚的に判別しやすくする
    if (owner_ == BulletOwner::Enemy)
    {
        object3d_->SetColor({ 1.0f, 0.2f, 0.2f, 1.0f });
    }

    object3d_->Update();
}

void Bullet::Update(float deltaTime)
{
    if (isDead_ || !object3d_)
    {
        return;
    }

    Vector3 pos = object3d_->GetTranslate();
    const float frameScale = deltaTime * 60.0f;
    pos.x += direction_.x * speed_ * frameScale;
    pos.y += direction_.y * speed_ * frameScale;
    pos.z += direction_.z * speed_ * frameScale;
    object3d_->SetTranslate(pos);

    elapsed_ += deltaTime;
    if (elapsed_ >= lifeTime_)
    {
        isDead_ = true;
    }

    object3d_->Update();
}

void Bullet::Draw(const RenderContext& ctx)
{
    if (isDead_ || !object3dCom_ || !object3d_)
    {
        return;
    }

    RenderContext bulletCtx = ctx;
    const Object3d::ModelData& modelData = object3d_->GetModelData();
    uint32_t texIdx = modelData.material.textureIndex;
    if (texIdx == 0 || texIdx == UINT32_MAX)
    {
        texIdx = defaultTextureIndex_;
    }
    if (bulletCtx.textureHandle.ptr == 0 && texIdx != 0 && texIdx != UINT32_MAX)
    {
        bulletCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(texIdx);
    }

    CustomObject3dRenderer::GetInstance()->Draw(object3d_.get(), bulletCtx, modelData, true);
}

void Bullet::Finalize()
{
    if (object3d_)
    {
        object3d_.reset();
    }
    isDead_ = true;
}
