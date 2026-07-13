#pragma once
#include <memory>
#include <vector>
#include "Object3d.h"
#include "Object3dCom.h"
#include "Camera.h"
#include "RenderContext.h"
#include "Baziru3_Engine/Collision/SphereCollider.h"
#include "Baziru3_Engine/Collision/CollisionManager.h"

class Bullet;
class MouseInput;

class Player
{
public:
    void Initialize(Object3dCom* object3dCom, Camera* camera);
    void Update(float deltaTime, MouseInput* mouseInput = nullptr);
    void Draw(const RenderContext& ctx);
    void Finalize();

    std::vector<std::unique_ptr<Bullet>> TryShoot(const MouseInput* mouseInput, float deltaTime);

    Vector3 GetPosition() const { return object3d_ ? object3d_->GetTranslate() : Vector3{ 0.0f, 0.0f, 0.0f }; }
    Vector3 GetRotation() const { return object3d_ ? object3d_->GetRotate() : Vector3{ 0.0f, 0.0f, 0.0f }; }
    void SetPosition(const Vector3& pos) { if (object3d_) object3d_->SetTranslate(pos); }
    SphereCollider* GetCollider() const { return collider_.get(); }

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

    float GetStamina() const { return stamina_; }
    float GetMaxStamina() const { return maxStamina_; }
    float GetStaminaRatio() const { return maxStamina_ > 0.0f ? stamina_ / maxStamina_ : 0.0f; }

    float GetCurrentSpread() const { return currentSpread_; }
    bool IsDodging() const { return isDodging_; }
    bool IsMoving() const { return isMoving_; }
    float GetDodgeTimer() const { return dodgeTimer_; }
    float GetDodgeDuration() const { return dodgeDuration_; }

private:
    std::unique_ptr<Object3d> object3d_;
    Vector3 position_ = { 0.0f, 0.0f, 0.0f };
    std::unique_ptr<SphereCollider> collider_;
    Object3dCom* object3dCom_ = nullptr;
    Camera* camera_ = nullptr;
    uint32_t defaultTextureIndex_ = UINT32_MAX;

    float hp_ = 100.0f;
    float maxHp_ = 100.0f;
    bool isDead_ = false;
    float invincibilityTimer_ = 0.0f;
    const float invincibilityDuration_ = 1.0f;
    float hitFlashTimer_ = 0.0f;
    const float hitFlashDuration_ = 0.12f;

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

    bool isDodging_ = false;
    float dodgeTimer_ = 0.0f;
    const float dodgeDuration_ = 0.4f;
    Vector3 dodgeDirection_ = { 0.0f, 0.0f, 1.0f };
    const float dodgeSpeed_ = 0.15f;

    float stamina_ = 100.0f;
    float maxStamina_ = 100.0f;
    const float dodgeStaminaCost_ = 30.0f;
    const float staminaRegenRate_ = 15.0f;

    // Weapon Recoil & Reticle Spread parameters
    float currentSpread_ = 0.0f;
    const float kBaseSpread = 0.01f;
    const float kMoveSpreadPenalty = 0.06f;
    const float kShootSpreadPenalty = 0.04f;
    const float kMaxSpread = 0.25f;
    const float kSpreadRecoverRate = 0.3f;

    void UpdateSpread(float deltaTime, bool isMoving);
    bool isMoving_ = false;
};

