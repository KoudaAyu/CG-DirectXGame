#pragma once
#include <memory>
#include <vector>
#include <string>
#include <algorithm>
#include "Object3d.h"
#include "Object3dCom.h"
#include "Camera.h"
#include "RenderContext.h"
#include "Baziru3_Engine/Framework/Collision/SphereCollider.h"
#include "Baziru3_Engine/Framework/Collision/CollisionManager.h"
#include "Application/Config/GameConfig.h"

class Bullet;
class MouseInput;

/// <summary>
/// プレイヤーキャラクター（ダック主人公）クラス
/// 移動・よちよち歩き・ローリング回避・射撃反動・弾薬リロード・治療アクション・被弾処理を管理します。
/// </summary>
class Player
{
public:
    // =========================================================================
    // 定数定義 (Named Constants) - GameConfig から一元参照
    // =========================================================================
    static constexpr float kDefaultMaxHp                  = GameConfig::Player::kDefaultMaxHp;
    static constexpr float kDefaultMaxStamina             = GameConfig::Player::kDefaultMaxStamina;
    static constexpr int   kDefaultMaxMagazineAmmo        = GameConfig::Player::kMaxMagazineAmmo;
    static constexpr int   kDefaultReserveAmmo            = GameConfig::Player::kDefaultReserveAmmo;
    static constexpr int   kDefaultMedkitCount            = GameConfig::Player::kDefaultMedkitCount;
    static constexpr float kMedkitHealAmount              = GameConfig::Player::kMedkitHealAmount;
    static constexpr float kHealDuration                  = GameConfig::Player::kHealDuration;
    static constexpr float kReloadDuration                = GameConfig::Player::kReloadDuration;
    static constexpr float kReloadCancelledNotifyDuration = GameConfig::Player::kReloadCancelledNotifyDuration;
    static constexpr float kDodgeDuration                 = GameConfig::Player::kDodgeDuration;
    static constexpr float kDodgeSpeed                    = GameConfig::Player::kDodgeSpeed;
    static constexpr float kDodgeStaminaCost              = GameConfig::Player::kDodgeStaminaCost;
    static constexpr float kStaminaRegenRate              = GameConfig::Player::kStaminaRegenRate;
    static constexpr float kMoveSpeed                     = GameConfig::Player::kMoveSpeed;
    static constexpr float kHolsterSpeedMultiplier        = GameConfig::Player::kHolsterSpeedMultiplier;
    static constexpr float kInvincibilityDuration         = GameConfig::Player::kInvincibilityDuration;
    static constexpr float kHitFlashDuration              = GameConfig::Player::kHitFlashDuration;
    static constexpr float kBulletSpeed                   = GameConfig::Combat::kBulletSpeed;
    static constexpr float kBulletLifeTime                = GameConfig::Combat::kBulletLifeTime;
    static constexpr float kShotCooldown                  = GameConfig::Combat::kShotCooldown;
    static constexpr float kColliderRadius                = GameConfig::Player::kColliderRadius;
    static constexpr float kBaseSpread                    = GameConfig::Combat::kBaseSpread;
    static constexpr float kMoveSpreadPenalty             = GameConfig::Combat::kMoveSpreadPenalty;
    static constexpr float kShootSpreadPenalty            = GameConfig::Combat::kShootSpreadPenalty;
    static constexpr float kMaxSpread                     = GameConfig::Combat::kMaxSpread;
    static constexpr float kSpreadRecoverRate             = GameConfig::Combat::kSpreadRecoverRate;

public:
    /// <summary>プレイヤーの初期化（3Dモデル・コライダー・初期ステータスのセットアップ）</summary>
    void Initialize(Object3dCom* object3dCom, Camera* camera);
    /// <summary>毎フレームの更新（入力検知・移動・回避・リロード・治療・カメラ追従）</summary>
    void Update(float deltaTime, MouseInput* mouseInput = nullptr);
    /// <summary>衝突解決後の最終トランスフォーム同期</summary>
    void PostCollisionUpdate();
    /// <summary>最新カメラViewProjection行列およびリコイル変位に基づく3D銃口照準・レティクル座標の完全同期</summary>
    void SyncAimWithCamera(const MouseInput* mouseInput);
    /// <summary>プレイヤー3Dモデルの描画要求（被弾仰け反り演出適用）</summary>
    void Draw(const RenderContext& ctx);
    /// <summary>メモリ解放・コライダー登録解除</summary>
    void Finalize();
    /// <summary>射撃入力の判定と弾丸インスタンスの生成</summary>
    std::vector<std::unique_ptr<Bullet>> TryShoot(const MouseInput* mouseInput, float deltaTime);

    // --- 座標 & トランスフォーム関連 ---
    Vector3 GetPosition() const { return position_; }
    Vector3 GetRotation() const
    {
        if (isDodging_ || isInIntroPose_ || isDead_) return object3d_ ? object3d_->GetRotate() : Vector3{ 0.0f, aimYaw_, 0.0f };
        return { 0.0f, aimYaw_, 0.0f };
    }
    void SetPosition(const Vector3& pos);
    SphereCollider* GetCollider() const { return collider_.get(); }
    Object3d* GetObject3d() const { return object3d_.get(); }
    void SetTextureIndex(uint32_t texIdx) { defaultTextureIndex_ = texIdx; }
    uint32_t GetTextureIndex() const { return defaultTextureIndex_; }
    bool ConsumeFootstepDust(Vector3& outWorldPos);

    // --- ゲーム開始演出制御 ---
    void SetIntroTransform(const Vector3& offset, const Vector3& scale, const Vector3& rotate);
    void RestoreIntroPose();

    // --- 装備武器 (3Dライフル & 着脱式3D湾曲マガジン & [C] 銃収納ダッシュ) ---
    static void BuildRifleModel(Object3d::ModelData& data, bool includeMagazine = true);
    Object3d* GetRifleObject3d() const { return rifleObject_.get(); }
    Vector3 GetRiflePosition() const { return riflePosition_; }
    Vector3 GetMuzzleWorldPosition() const;
    Vector3 GetMagwellWorldPosition() const;
    bool IsWeaponHolstered() const { return isWeaponHolstered_; }
    bool WasDryReload() const { return wasDryReload_; }

    // --- 体力 & ダメージ関連 ---
    void TakeDamage(float damage, const std::string& attackerName = "HOSTILE PATROL", const std::string& cause = "7.62x39mm PS CARTRIDGE (CHEST PENETRATION)", const Vector3* attackerPos = nullptr);
    float GetHP() const { return hp_; }
    float GetMaxHP() const { return maxHp_; }
    float GetHPRatio() const { return maxHp_ > 0.0f ? hp_ / maxHp_ : 0.0f; }
    bool IsDead() const { return isDead_; }
    const std::string& GetCauseOfDeath() const { return causeOfDeath_; }
    const std::string& GetLastAttackerName() const { return lastAttackerName_; }
    const Vector3& GetLastAttackerPosition() const { return lastAttackerPosition_; }
    bool HasAttackerPosition() const { return hasAttackerPosition_; }
    void Reset();

    // --- 弾薬 & リロード関連 ---
    int GetMagazineAmmo() const { return magazineAmmo_; }
    int GetMaxMagazineAmmo() const { return maxMagazineAmmo_; }
    int GetReserveAmmo() const { return reserveAmmo_; }
    void AddReserveAmmo(int count) { reserveAmmo_ += count; }
    bool IsReloading() const { return isReloading_; }
    float GetReloadProgress() const { return isReloading_ && kReloadDuration > 0.0f ? (kReloadDuration - reloadTimer_) / kReloadDuration : 0.0f; }
    float GetReloadCancelledTimer() const { return reloadCancelledTimer_; }

    // --- 物資 & インベントリ関連 ---
    int GetMedkitCount() const { return medkitCount_; }
    void AddMedkits(int count) { medkitCount_ += count; }
    int GetLootValue() const { return lootValue_; }
    void AddLootValue(int val) { lootValue_ += val; }

    // --- 治療アクション関連 ---
    bool IsHealing() const { return isHealing_; }
    float GetHealProgress() const { return isHealing_ && kHealDuration > 0.0f ? (kHealDuration - healTimer_) / kHealDuration : 0.0f; }
    bool StartHealing();
    void Heal(float amount) { hp_ = (std::min)(maxHp_, hp_ + amount); }

    // --- スタミナ & 回避関連 ---
    float GetStamina() const { return stamina_; }
    float GetMaxStamina() const { return maxStamina_; }
    float GetStaminaRatio() const { return maxStamina_ > 0.0f ? stamina_ / maxStamina_ : 0.0f; }
    bool IsDodging() const { return isDodging_; }
    bool IsMoving() const { return isMoving_; }
    float GetDodgeTimer() const { return dodgeTimer_; }
    float GetDodgeDuration() const { return kDodgeDuration; }

    // --- 照準・レティクルリコイル & 遮蔽 ---
    float GetCurrentSpread() const { return currentSpread_; }
    Vector2 GetReticleScreenPos() const { return effectiveReticleScreenPos_; }
    Vector2 GetReticleRecoilOffset() const { return reticleRecoilOffset_; }
    float GetAimRayDistance() const { return aimRayDistance_; }
    float GetRecoilKickAmount() const { return recoilKickVisual_; }
    bool IsInCover() const { return isInCover_; }
    void SetInCover(bool inCover) { isInCover_ = inCover; }

private:
    static constexpr Vector3 kDefaultBulletSpawnOffset = { 0.42f, 0.707f, 0.98f };
    static constexpr Vector2 kDefaultReticleScreenPos  = { 640.0f, 360.0f };
    static constexpr float   kDefaultAimRayDistance    = 12.0f;

    // --- 内部更新メソッド ---
    void UpdateActionStates(float deltaTime, const MouseInput* mouseInput);
    void UpdateDamageVisualTimers(float deltaTime);
    bool UpdateDodgeAndMovement(float deltaTime, float frameScale);
    void CancelReloadWithNotify();
    void ApplyWeaponTransforms(const Vector3& riflePos, const Vector3& rifleRot, const Vector3& magPos, const Vector3& magRot, const Vector3& scale);
    void UpdateSpread(float deltaTime, bool isMoving);
    void UpdateReticleRecoil(float deltaTime);
    void ApplyReticleRecoilKick();
    void UpdateLocomotionState(float deltaTime, const Vector3& moveDirWorld);
    void SyncDuckVisualAndWeaponPose();
    void SyncRifleAndMagazinePose();

private:
    // --- 3Dオブジェクト & コンポーネント (胴体＋独立稼働する左右水かき足・左右の翼) ---
    std::unique_ptr<Object3d> object3d_;
    std::unique_ptr<Object3d> leftFootObject_;
    std::unique_ptr<Object3d> rightFootObject_;
    std::unique_ptr<Object3d> leftWingObject_;
    std::unique_ptr<Object3d> rightWingObject_;
    Vector3 position_ = { 0.0f, 0.0f, 0.0f };
    Vector3 drawPos_ = { 0.0f, 0.0f, 0.0f };
    std::unique_ptr<SphereCollider> collider_;
    Object3dCom* object3dCom_ = nullptr;
    Camera* camera_ = nullptr;
    uint32_t defaultTextureIndex_ = UINT32_MAX;

    // --- アヒル歩行・ダッシュ・待機ロコモーション状態 ---
    float aimYaw_ = 0.0f;
    float walkPhase_ = 0.0f;
    float walkBlend_ = 0.0f;
    float sprintBlend_ = 0.0f;
    float idleAnimTime_ = 0.0f;
    Vector3 moveInputDir_ = { 0.0f, 0.0f, 0.0f };
    float smoothedLocalMoveRight_ = 0.0f;
    float smoothedLocalMoveFwd_ = 1.0f;
    bool isInIntroPose_ = false;
    bool pendingFootstepDust_ = false;
    Vector3 lastFootstepWorldPos_ = { 0.0f, 0.0f, 0.0f };
    Vector4 currentBodyColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };

    // --- 体力 & 被弾ステート ---
    float hp_ = kDefaultMaxHp;
    float maxHp_ = kDefaultMaxHp;
    bool isDead_ = false;
    float invincibilityTimer_ = 0.0f;
    float hitFlashTimer_ = 0.0f;
    Vector3 lastAttackerPosition_ = { 0.0f, 0.0f, 0.0f };
    bool hasAttackerPosition_ = false;

    // --- 射撃パラメータ (3Dライフルのマズルブレーキ銃口中心に完全一致) ---
    Vector3 bulletSpawnOffset_ = kDefaultBulletSpawnOffset;
    float shotCooldownTimer_ = 0.0f;
    bool suppressShootUntilMouseRelease_ = true;

    // --- 装備ライフル & 着脱式3D湾曲PMAG (3D武器 & [C] 銃収納ダッシュ) ---
    std::unique_ptr<Object3d> rifleObject_;
    std::unique_ptr<Object3d> rifleMagObject_;
    Vector3 riflePosition_ = { 0.0f, 0.0f, 0.0f };
    Vector3 rifleRotation_ = { 0.0f, 0.0f, 0.0f };
    float rifleRecoil_ = 0.0f;
    uint32_t rifleTextureIndex_ = UINT32_MAX;
    uint32_t rifleMagTextureIndex_ = UINT32_MAX;
    bool rifleMagVisible_ = true;
    bool isWeaponHolstered_ = false;
    bool prevCPressed_ = false;

    // --- 弾薬 & リロード ---
    int maxMagazineAmmo_ = kDefaultMaxMagazineAmmo;
    int magazineAmmo_ = kDefaultMaxMagazineAmmo;
    int reserveAmmo_ = kDefaultReserveAmmo;
    bool isReloading_ = false;
    bool wasDryReload_ = false;
    float reloadTimer_ = 0.0f;
    float reloadCancelledTimer_ = 0.0f;

    // --- 救急キット & 治療 ---
    int medkitCount_ = kDefaultMedkitCount;
    int lootValue_ = 0;
    bool isHealing_ = false;
    float healTimer_ = 0.0f;

    // --- 回避 & スタミナ ---
    bool isDodging_ = false;
    float dodgeTimer_ = 0.0f;
    Vector3 dodgeDirection_ = { 0.0f, 0.0f, 1.0f };
    float stamina_ = kDefaultMaxStamina;
    float maxStamina_ = kDefaultMaxStamina;
    bool prevSpacePressed_ = false;

    // --- 照準・レティクルリコイル & 遮蔽 ---
    float currentSpread_ = 0.0f;
    Vector2 reticleRecoilOffset_ = { 0.0f, 0.0f };
    Vector2 reticleRecoilVel_ = { 0.0f, 0.0f };
    Vector2 effectiveReticleScreenPos_ = kDefaultReticleScreenPos;
    float aimRayDistance_ = kDefaultAimRayDistance;
    float recoilHeat_ = 0.0f;
    float recoilKickVisual_ = 0.0f;
    int recoilSideSign_ = 1;
    bool isMoving_ = false;
    bool isInCover_ = false;

    // --- 死因レポート用 ---
    std::string causeOfDeath_ = "ENEMY GUNFIRE (7.62x39mm PS)";
    std::string lastAttackerName_ = "HOSTILE PATROL";
};
