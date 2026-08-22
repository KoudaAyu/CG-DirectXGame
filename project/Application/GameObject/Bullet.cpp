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
    Object3d::ModelData model = Object3d::LoadObjFile("Resources", "sphere.obj");
    object3d_->Initialize(object3dCom_, model);
    object3d_->SetCamera(camera_);

    Vector3 spawnPos = startPosition;
    object3d_->SetTranslate(spawnPos);

    // 飛行方向（進行方向）へ向きを合わせる
    float yaw = std::atan2(direction_.x, direction_.z);
    object3d_->SetRotate({ 0.0f, yaw, 0.0f });

    // 3D立体弾丸カプセル形状（太さ: 0.45m, 進行方向長さ: 1.4m）
    object3d_->SetScale({ 0.45f, 0.45f, 1.4f });

    if (owner_ == BulletOwner::Player)
    {
        object3d_->SetColor({ 1.0f, 0.85f, 0.1f, 1.0f }); // 鮮やかな金黄色（プレイヤーエネルギー弾）
    }
    else
    {
        object3d_->SetColor({ 1.0f, 0.15f, 0.1f, 1.0f });  // 鮮やかな赤色プラズマ（敵弾）
    }

    prevPosition_ = spawnPos;

    char buf[256];
    snprintf(buf, sizeof(buf), "[Bullet::Initialize LOG] Bullet created at pos=(%.2f, %.2f, %.2f), dir=(%.2f, %.2f, %.2f), speed=%.2f\n",
        startPosition.x, startPosition.y, startPosition.z, direction_.x, direction_.y, direction_.z, speed_);
    OutputDebugStringA(buf);
}

void Bullet::Update(float deltaTime)
{
    if (isDead_) return;

    if (object3d_)
    {
        if (camera_)
        {
            object3d_->SetCamera(camera_);
        }

        prevPosition_ = object3d_->GetTranslate();

        Vector3 pos = prevPosition_;
        pos.x += direction_.x * speed_ * (deltaTime * 60.0f);
        pos.y += direction_.y * speed_ * (deltaTime * 60.0f);
        pos.z += direction_.z * speed_ * (deltaTime * 60.0f);

        object3d_->SetTranslate(pos);

        // 進行方向に向きを維持
        float yaw = std::atan2(direction_.x, direction_.z);
        object3d_->SetRotate({ 0.0f, yaw, 0.0f });

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
    if (object3d_ && object3dCom_)
    {
        if (ctx.camera)
        {
            object3d_->SetCamera(ctx.camera);
            object3d_->Update(); // Active render camera WVP recalculation
        }

        RenderContext bulletCtx = ctx;
        uint32_t whiteTex = TextureManager::GetInstance()->Load("Resources/CG4/human/white.png");
        bulletCtx.SetTextureHandle(TextureManager::GetInstance()->GetSrvHandleGPU(whiteTex));
        object3dCom_->Draw(object3d_.get(), bulletCtx, object3d_->GetModelData(), true);
    }
}





void Bullet::Finalize()
{
    object3d_.reset();
    isDead_ = true;
}
