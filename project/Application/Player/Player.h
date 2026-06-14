#pragma once
#include <memory>
#include "Object3d.h"
#include "Object3dCom.h"
#include "Camera.h"
#include "RenderContext.h"

class Bullet;
class MouseInput;

class Player
{
public:
    void Initialize(Object3dCom* object3dCom, Camera* camera);
    void Update(MouseInput* mouseInput = nullptr);
    void Draw(const RenderContext& ctx);
    void Finalize();

    std::unique_ptr<Bullet> TryShoot(const MouseInput* mouseInput, float deltaTime);

    Vector3 GetPosition() const { return object3d_ ? object3d_->GetTranslate() : Vector3{ 0.0f, 0.0f, 0.0f }; }
    Vector3 GetRotation() const { return object3d_ ? object3d_->GetRotate() : Vector3{ 0.0f, 0.0f, 0.0f }; }

    void TakeDamage(float damage);
    float GetHP() const { return hp_; }
    float GetMaxHP() const { return maxHp_; }
    float GetHPRatio() const { return maxHp_ > 0.0f ? hp_ / maxHp_ : 0.0f; }
    bool IsDead() const { return isDead_; }
    void Reset();

    int GetMagazineAmmo() const { return magazineAmmo_; }
    int GetMaxMagazineAmmo() const { return maxMagazineAmmo_; }
    bool IsReloading() const { return isReloading_; }
    float GetReloadProgress() const { return isReloading_ && reloadDuration_ > 0.0f ? (reloadDuration_ - reloadTimer_) / reloadDuration_ : 0.0f; }

private:
    std::unique_ptr<Object3d> object3d_;
    Object3dCom* object3dCom_ = nullptr;
    Camera* camera_ = nullptr;
    uint32_t defaultTextureIndex_ = UINT32_MAX;

    float hp_ = 100.0f;
    float maxHp_ = 100.0f;
    bool isDead_ = false;
    float invincibilityTimer_ = 0.0f;
    const float invincibilityDuration_ = 1.0f;

    Vector3 bulletSpawnOffset_ = { 0.0f, 0.2f, 0.5f };
    float bulletSpeed_ = 0.45f;
    float bulletLifeTime_ = 2.0f;
    float shotCooldown_ = 0.12f;
    float shotCooldownTimer_ = 0.0f;

    int maxMagazineAmmo_ = 30;
    int magazineAmmo_ = 30;
    bool isReloading_ = false;
    float reloadTimer_ = 0.0f;
    const float reloadDuration_ = 1.5f;
};

