#pragma once
#include <memory>
#include "Object3d.h"
#include "Object3dCom.h"
#include "Camera.h"
#include "RenderContext.h"
#include "Baziru3_Engine/Collision/SphereCollider.h"
#include "Baziru3_Engine/Collision/CollisionManager.h"

class Bullet;
class Sprite;
class WindowAPI;
class Obstacle;

class Enemy
{
public:
    enum class AIState
    {
        Patrol,      // 定点監視・索敵
        Investigate, // 音源の捜索
        Chase        // 発見・戦闘
    };

public:
    void Initialize(Object3dCom* object3dCom, Camera* camera);
    void Update(WindowAPI* windowAPI, const Vector3* targetPosition, const std::vector<std::unique_ptr<Obstacle>>& obstacles, float deltaTime);
    void Draw(const RenderContext& ctx);
    void Finalize();
    void OnHit(const Vector3& attackerPos);

    std::unique_ptr<Bullet> TryShoot(const Vector3& targetPosition);

    Vector3 GetPosition() const { return object3d_ ? object3d_->GetTranslate() : Vector3{ 0.0f, 0.0f, 0.0f }; }
    void SetPosition(const Vector3& pos) { if (object3d_) object3d_->SetTranslate(pos); }

    int GetHP() const { return hp_; }
    int GetMaxHP() const { return maxHp_; }
    bool IsDead() const { return isDead_; }
    bool GetJustRespawned() const { return justRespawned_; }
    void ClearJustRespawned() { justRespawned_ = false; }
    void SetHPBarSprites(Sprite* bg, Sprite* fg) { hpBarBg_ = bg; hpBarFg_ = fg; }

    // AI索敵ゲッター
    AIState GetAIState() const { return state_; }
    float GetDetectionMeter() const { return detectionMeter_; }
    float GetAlertTimer() const { return alertTimer_; }
    float GetFieldOfView() const { return fovAngle_; }
    float GetMaxSightRange() const { return maxSightRange_; }

    // 視覚コーンの向き（回転角）を取得する
    float GetYaw() const { return object3d_ ? object3d_->GetRotate().y : 0.0f; }

    // 音源検知のトリガー
    void HearNoise(const Vector3& noisePosition);
    void AlertEnemy(const Vector3& targetPos);

private:
    bool FaceTarget(const Vector3& targetPosition, float deltaTime = 0.016f);
    bool HasLineOfSight(const Vector3& playerPos, const std::vector<std::unique_ptr<Obstacle>>& obstacles);

    std::unique_ptr<Object3d> object3d_;
    Vector3 position_ = { 0.0f, 0.0f, 0.0f };
    std::unique_ptr<SphereCollider> collider_;
    Object3dCom* object3dCom_ = nullptr;
    Camera* camera_ = nullptr;
    uint32_t defaultTextureIndex_ = UINT32_MAX;
    float hitFlashTimer_ = 0.0f;
    float hitFlashDuration_ = 0.12f;

    int hp_ = 5;
    int maxHp_ = 5;
    bool isDead_ = false;
    bool justRespawned_ = false;
    float respawnTimer_ = 0.0f;
    const float respawnDuration_ = 3.0f;

    Sprite* hpBarBg_ = nullptr;
    Sprite* hpBarFg_ = nullptr;

    Vector3 bulletSpawnOffset_ = { 0.0f, 0.2f, 0.5f };
    float bulletSpeed_ = 0.45f;
    float bulletLifeTime_ = 2.0f;
    float shotCooldown_ = 2.0f;
    float shotCooldownTimer_ = 2.0f;

    // --- AI索敵パラメータ ---
    AIState state_ = AIState::Patrol;
    float detectionMeter_ = 0.0f; // 0.0f (未感知) 〜 1.0f (発見)
    float alertTimer_ = 0.0f;     // !や?マークの表示用タイマー
    float searchTimer_ = 0.0f;    // 捜索中の待機タイマー
    Vector3 investigateTarget_ = { 0.0f, 0.0f, 0.0f };
    Vector3 lastSeenPlayerPosition_ = { 0.0f, 0.0f, 0.0f };
    
    const float fovAngle_ = 65.0f * (3.14159265f / 180.0f); // 視野角 65度 (ラジアン)
    const float maxSightRange_ = 10.0f; // 最大視認距離
};

