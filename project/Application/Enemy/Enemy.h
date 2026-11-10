#pragma once
#include <memory>
#include <numbers>
#include <vector>
#include "EnemyTuning.h"
#include "Object3d.h"
#include "Object3dCom.h"
#include "Camera.h"
#include "RenderContext.h"
#include "Baziru3_Engine/Framework/Collision/SphereCollider.h"
#include "Baziru3_Engine/Framework/Collision/CollisionManager.h"

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

    enum class TacticalRole
    {
        Garrison,    // 拠点防衛歩哨（遮蔽物からのピーク射撃 ＆ リロード退避）
        Patrol,      // セクター巡回兵（巡回ルート歩行・端点見張り・中距離ストレイフ）
        Flanker,     // 側面強襲兵（コンテナ脇からの高速回り込み・包囲突撃）
        Overwatch,   // 長距離狙撃監視兵（長射程・高精度レーザーロックオン射撃）
        Rookie,      // チュートリアル初級歩哨（単発射撃・長めのレーザー予兆・低HP）
        Warden       // 右ルート隠し強敵「看守長 / THE WARDEN」（高HP・大型装甲・3点バースト）
    };

public:
    virtual ~Enemy() = default;

    virtual void Initialize(Object3dCom* object3dCom, Camera* camera);
    virtual void Update(WindowAPI* windowAPI, const Vector3* targetPosition, const std::vector<std::unique_ptr<Obstacle>>& obstacles, float deltaTime, bool isPlayerInCover = false);
    virtual void Draw(const RenderContext& ctx);
    virtual void Finalize();
    virtual void OnHit(const Vector3& attackerPos);

    virtual std::unique_ptr<Bullet> TryShoot(const Vector3& targetPosition);

    virtual Vector3 GetPosition() const { return position_; }
    virtual void SetPosition(const Vector3& pos);
    void SetBaseYaw(float yaw);
    virtual void SetTacticalRole(TacticalRole role);
    TacticalRole GetTacticalRole() const { return role_; }
    float GetRoleScale() const { return roleScale_; }

    Object3d* GetObject3d() const { return object3d_.get(); }
    Object3d* GetRifleObject() const { return rifleObject_.get(); }
    void SyncCameraTransforms();

    // 敵同士の重なり防止（セパレーション押し出し）
    void ApplySeparation(const Vector3& pushDelta);

    int GetHP() const { return hp_; }
    int GetMaxHP() const { return maxHp_; }
    float GetHPRatio() const { return maxHp_ > 0 ? static_cast<float>(hp_) / static_cast<float>(maxHp_) : 0.0f; }
    float GetDisplayedHPRatio() const { return displayedHpRatio_; }
    bool IsDead() const { return isDead_; }
    bool IsDeathAnimPlaying() const { return isDead_ && deathAnimTimer_ > 0.0f; }
    bool GetJustRespawned() const { return justRespawned_; }
    void ClearJustRespawned() { justRespawned_ = false; }
    virtual void SetHPBarSprites(Sprite* bg, Sprite* fg) { hpBarBg_ = bg; hpBarFg_ = fg; }

    // AI索敵・戦術テレグラフ用ゲッター
    AIState GetAIState() const { return state_; }
    float GetDetectionMeter() const { return detectionMeter_; }
    float GetAlertTimer() const { return alertTimer_; }
    float GetFieldOfView() const { return fovAngle_; }
    float GetMaxSightRange() const { return maxSightRange_; }
    bool IsEnemyReloading() const { return reloadTimer_ > 0.0f; }
    float GetReloadProgress() const { return reloadDuration_ > 0.0f ? (1.0f - reloadTimer_ / reloadDuration_) : 0.0f; }
    float GetAimTelegraphProgress() const;
    bool HasCurrentLineOfSight() const { return hasCurrentLOS_; }
    bool IsMovingHorizontally() const { return horizSpeed_ > EnemyTuning::kMovingHorizSpeedThreshold; }
    Vector3 GetMuzzleWorldPosition() const;

    // 視覚コーンの向き（回転角）を取得する
    float GetYaw() const { return currentYaw_; }

    // 音源検知・分隊無線リンクのトリガー
    virtual void HearNoise(const Vector3& noisePosition);
    virtual void AlertEnemy(const Vector3& targetPos);

protected:
    static bool IsWalkableArenaPos(const Vector3& pos, float agentRadius = EnemyTuning::kStepWalkableRadius);
    static void BuildEnemyRifleModel(Object3d::ModelData& data);
    void InitializeEnemyRifle();
    void UpdateVisualAndWeaponPose(float deltaTime);
    void HideLegacyOverheadSprites();
    bool UpdateCommonPreAI(float deltaTime);
    bool EvaluatePlayerSight(
        const Vector3* targetPosition,
        const std::vector<std::unique_ptr<Obstacle>>& obstacles,
        bool isPlayerInCover,
        float chaseSightMult,
        float& outDistToPlayer);
    void FinalizeCommonFrameState(float deltaTime, const Vector3* targetPosition, bool canSeePlayer);
    bool UpdateTacticalWeaponState(float deltaTime, const Vector3* targetPosition, bool hasLOS);

    bool FaceTarget(const Vector3& targetPosition, float deltaTime = EnemyTuning::kDefaultFaceDeltaTime);
    bool HasLineOfSight(const Vector3& playerPos, const std::vector<std::unique_ptr<Obstacle>>& obstacles);

    std::unique_ptr<Object3d> object3d_;
    std::unique_ptr<Object3d> leftFootObject_;
    std::unique_ptr<Object3d> rightFootObject_;
    std::unique_ptr<Object3d> leftWingObject_;
    std::unique_ptr<Object3d> rightWingObject_;
    std::unique_ptr<Object3d> rifleObject_;
    uint32_t rifleTextureIndex_ = UINT32_MAX;

    Vector3 position_ = { 0.0f, 0.0f, 0.0f };
    Vector3 prevFramePos_ = { 0.0f, 0.0f, 0.0f };
    Vector3 spawnPosition_ = { 0.0f, 0.0f, 0.0f };
    float currentYaw_ = EnemyTuning::kPi;
    float baseYaw_ = EnemyTuning::kPi;
    float scanTime_ = 0.0f;
    float scanPhase_ = 0.0f;

    std::unique_ptr<SphereCollider> collider_;
    Object3dCom* object3dCom_ = nullptr;
    Camera* camera_ = nullptr;
    uint32_t defaultTextureIndex_ = UINT32_MAX;
    float hitFlashTimer_ = 0.0f;
    float hitFlashDuration_ = EnemyTuning::kDefaultHitFlashDuration;

    int hp_ = EnemyTuning::kDefaultMaxHp;
    int maxHp_ = EnemyTuning::kDefaultMaxHp;
    float displayedHpRatio_ = 1.0f;
    bool isDead_ = false;
    bool canRespawn_ = false;
    bool justRespawned_ = false;
    float respawnTimer_ = 0.0f;
    const float respawnDuration_ = EnemyTuning::kDefaultRespawnDuration;

    // 撃破時の転倒アニメーション ＆ 被弾ノックバック・歩行アニメ
    static constexpr float kDeathAnimDuration = 0.45f;
    float deathAnimTimer_ = 0.0f;
    float deathFallDir_ = 1.0f;
    Vector3 knockbackVel_ = { 0.0f, 0.0f, 0.0f };
    float flinchTimer_ = 0.0f;
    float waddlePhase_ = 0.0f;
    float horizSpeed_ = 0.0f;
    float localMoveRight_ = 0.0f;
    float localMoveFwd_ = 1.0f;
    float rifleRecoil_ = 0.0f;

    // 戦術ストレイフ・回避ステップ
    float strafeSign_ = 1.0f;
    float strafeTimer_ = 0.0f;
    float evadeTimer_ = 0.0f;
    float evadeCooldown_ = 0.0f;
    Vector3 evadeVel_ = { 0.0f, 0.0f, 0.0f };

    Sprite* hpBarBg_ = nullptr;
    Sprite* hpBarFg_ = nullptr;

    // 2点バースト射撃 ＆ レーザー照準テレグラフ ＆ マガジンリロード
    TacticalRole role_ = TacticalRole::Garrison;
    float roleScale_ = 1.0f;
    Vector4 baseBodyTint_ = { 1.0f, 1.0f, 1.0f, 1.0f };
    Vector3 bulletSpawnOffset_ = EnemyTuning::kDefaultBulletSpawnOffset;
    float bulletSpeed_ = EnemyTuning::kDefaultBulletSpeed;
    float bulletLifeTime_ = EnemyTuning::kDefaultBulletLifeTime;
    float shotCooldown_ = EnemyTuning::kDefaultShotCooldown;
    float shotCooldownTimer_ = 1.0f;
    float aimTelegraphDuration_ = EnemyTuning::kDefaultAimTelegraphDur;
    float aimTelegraphTimer_ = 0.0f;
    int burstCountPerTrigger_ = 2;
    int burstShotsRemaining_ = 0;
    float burstIntervalTimer_ = 0.0f;
    int ammoInMag_ = EnemyTuning::kDefaultMagAmmo;
    int maxMagAmmo_ = EnemyTuning::kDefaultMagAmmo;
    float reloadTimer_ = 0.0f;
    const float reloadDuration_ = EnemyTuning::kDefaultReloadDuration;
    bool hasCurrentLOS_ = false;
    bool readyToFireBurst_ = false;

    // --- AI索敵パラメータ ---
    AIState state_ = AIState::Patrol;
    float detectionMeter_ = 0.0f; // 0.0f (未感知) 〜 1.0f (発見)
    float alertTimer_ = 0.0f;     // !や?マークの表示用タイマー
    float searchTimer_ = 0.0f;    // 捜索中の待機タイマー
    Vector3 investigateTarget_ = { 0.0f, 0.0f, 0.0f };
    Vector3 lastSeenPlayerPosition_ = { 0.0f, 0.0f, 0.0f };

    float fovAngle_ = EnemyTuning::kDefaultFovRadians;     // 視野角 72度
    float maxSightRange_ = EnemyTuning::kDefaultMaxSightRange; // 最大視認距離
};
