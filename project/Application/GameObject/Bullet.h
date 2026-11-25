#pragma once

#include <memory>
#include <d3d12.h>
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
    static constexpr float kDefaultSpeed       = 0.35f;
    static constexpr float kDefaultLifeTime    = 2.0f;
    static constexpr float kReferenceFrameRate = 60.0f;
    static constexpr float kDefaultDeltaTime   = 1.0f / kReferenceFrameRate;

    static Vector3 ComputeSpawnPosition(const Vector3& ownerPosition, const Vector3& forward, const Vector3& spawnOffset);

    void Initialize(Object3dCom* object3dCom, Camera* camera, const Vector3& startPosition, const Vector3& direction, float speed = kDefaultSpeed, float lifeTime = kDefaultLifeTime, BulletOwner owner = BulletOwner::Player);
    void Update(float deltaTime = kDefaultDeltaTime);
    void Draw(const RenderContext& ctx, ID3D12PipelineState* additiveTracerPSO = nullptr);
    void Finalize();

    bool IsDead() const { return isDead_; }
    BulletOwner GetOwner() const { return owner_; }
    Vector3 GetPosition() const { return object3d_ ? object3d_->GetTranslate() : Vector3{ 0.0f, 0.0f, 0.0f }; }
    Vector3 GetPrevPosition() const { return prevPosition_; }
    Vector3 GetDirection() const { return direction_; }
    void SetDirection(const Vector3& newDir);
    float GetDistanceTraveled() const { return distanceTraveled_; }
    bool ConsumeWakePulse(float deltaTime);
    bool IsNearMissTriggered() const { return nearMissTriggered_; }
    void MarkNearMissTriggered() { nearMissTriggered_ = true; }
    void TriggerNearMiss() { nearMissTriggered_ = true; }
    void Kill() { isDead_ = true; }

private:
    void UpdateVisualTransforms();

    Object3dCom* object3dCom_ = nullptr;
    Camera* camera_ = nullptr;
    std::unique_ptr<Object3d> object3d_;      // 3D銅被甲スピッツァーFMJ弾頭
    std::unique_ptr<Object3d> tracerObject_;  // 超音速曳光弾（トレーサー）発光ストリーク
    uint32_t ballisticsTexIndex_ = UINT32_MAX;

    Vector3 direction_ = { 0.0f, 0.0f, 1.0f };
    Vector3 prevPosition_ = { 0.0f, 0.0f, 0.0f };
    float speed_ = kDefaultSpeed;
    float lifeTime_ = kDefaultLifeTime;
    float age_ = 0.0f;
    float distanceTraveled_ = 0.0f;
    float wakeTimer_ = 0.0f;
    bool isDead_ = false;
    bool nearMissTriggered_ = false;
    BulletOwner owner_ = BulletOwner::Player;
};
