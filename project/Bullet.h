#pragma once

#include <memory>
#include "Object3d.h"
#include "Object3dCom.h"
#include "Camera.h"
#include "RenderContext.h"

enum class BulletOwner
{
    Player,
    Enemy
};

class Bullet
{
public:
    static Vector3 ComputeSpawnPosition(const Vector3& ownerPosition, const Vector3& forward, const Vector3& spawnOffset);

    void Initialize(Object3dCom* object3dCom, Camera* camera, const Vector3& startPosition, const Vector3& direction, float speed = 0.35f, float lifeTime = 2.0f, BulletOwner owner = BulletOwner::Player);
    void Update(float deltaTime = 1.0f / 60.0f);
    void Draw(const RenderContext& ctx);
    void Finalize();

    bool IsDead() const { return isDead_; }
    BulletOwner GetOwner() const { return owner_; }
    Vector3 GetPosition() const { return object3d_ ? object3d_->GetTranslate() : Vector3{ 0.0f, 0.0f, 0.0f }; }
    Vector3 GetPrevPosition() const { return prevPosition_; }
    Vector3 GetDirection() const { return direction_; }
    bool IsNearMissTriggered() const { return nearMissTriggered_; }
    void TriggerNearMiss() { nearMissTriggered_ = true; }

private:
    std::unique_ptr<Object3d> object3d_;
    Vector3 prevPosition_ = { 0.0f, 0.0f, 0.0f };
    Object3dCom* object3dCom_ = nullptr;
    Camera* camera_ = nullptr;

    Vector3 direction_ = { 0.0f, 0.0f, 1.0f };
    float speed_ = 0.35f;
    float lifeTime_ = 2.0f;
    float elapsed_ = 0.0f;
    bool isDead_ = false;
    BulletOwner owner_ = BulletOwner::Player;
    bool nearMissTriggered_ = false;

    uint32_t defaultTextureIndex_ = UINT32_MAX;
};

