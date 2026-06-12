#pragma once
#include <memory>
#include "Object3d.h"
#include "Object3dCom.h"
#include "Camera.h"
#include "RenderContext.h"

class Bullet;
class Sprite;
class WindowAPI;

class Enemy
{
public:
    void Initialize(Object3dCom* object3dCom, Camera* camera);
    void Update(WindowAPI* windowAPI, const Vector3* targetPosition, float deltaTime);
    void Draw(const RenderContext& ctx);
    void Finalize();
    void OnHit();

    std::unique_ptr<Bullet> TryShoot(const Vector3& targetPosition);

    Vector3 GetPosition() const { return object3d_ ? object3d_->GetTranslate() : Vector3{ 0.0f, 0.0f, 0.0f }; }

    int GetHP() const { return hp_; }
    int GetMaxHP() const { return maxHp_; }
    bool IsDead() const { return isDead_; }
    void SetHPBarSprites(Sprite* bg, Sprite* fg) { hpBarBg_ = bg; hpBarFg_ = fg; }

private:
    bool FaceTarget(const Vector3& targetPosition);

    std::unique_ptr<Object3d> object3d_;
    Object3dCom* object3dCom_ = nullptr;
    Camera* camera_ = nullptr;
    uint32_t defaultTextureIndex_ = UINT32_MAX;
    float hitFlashTimer_ = 0.0f;
    float hitFlashDuration_ = 0.12f;

    int hp_ = 5;
    int maxHp_ = 5;
    bool isDead_ = false;
    float respawnTimer_ = 0.0f;
    const float respawnDuration_ = 3.0f;

    Sprite* hpBarBg_ = nullptr;
    Sprite* hpBarFg_ = nullptr;

    Vector3 bulletSpawnOffset_ = { 0.0f, 0.2f, 0.5f };
    float bulletSpeed_ = 0.45f;
    float bulletLifeTime_ = 2.0f;
    float shotCooldown_ = 2.0f;
    float shotCooldownTimer_ = 2.0f;
};

