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
    /// <summary>
    /// プレイヤーの初期化（3Dモデル・コライダー・初期ステータスのセットアップ）
    /// </summary>
    void Initialize(Object3dCom* object3dCom, Camera* camera);

    /// <summary>
    /// 毎フレームの更新（入力検知・移動・回避・リロード・治療・カメラ追従）
    /// </summary>
    void Update(float deltaTime, MouseInput* mouseInput = nullptr);

    /// <summary>
    /// 衝突解決後の最終トランスフォーム同期
    /// </summary>
    void PostCollisionUpdate();

    /// <summary>
    /// プレイヤー3Dモデルの描画要求（被弾仰け反り演出適用）
    /// </summary>
    void Draw(const RenderContext& ctx);

    /// <summary>
    /// メモリ解放・コライダー登録解除
    /// </summary>
    void Finalize();

    /// <summary>
    /// 射撃入力の判定と弾丸インスタンスの生成
    /// </summary>
    std::vector<std::unique_ptr<Bullet>> TryShoot(const MouseInput* mouseInput, float deltaTime);

    // --- 座標 & トランスフォーム関連 ---
    Vector3 GetPosition() const { return position_; }
    Vector3 GetRotation() const { return object3d_ ? object3d_->GetRotate() : Vector3{ 0.0f, 0.0f, 0.0f }; }
    void SetPosition(const Vector3& pos) { position_ = pos; if (object3d_) object3d_->SetTranslate(pos); }
    SphereCollider* GetCollider() const { return collider_.get(); }

    // --- 体力 & ダメージ関連 ---
    void TakeDamage(float damage, const std::string& attackerName = "HOSTILE PATROL", const std::string& cause = "7.62x39mm PS CARTRIDGE (CHEST PENETRATION)");
    float GetHP() const { return hp_; }
    float GetMaxHP() const { return maxHp_; }
    float GetHPRatio() const { return maxHp_ > 0.0f ? hp_ / maxHp_ : 0.0f; }
    bool IsDead() const { return isDead_; }
    const std::string& GetCauseOfDeath() const { return causeOfDeath_; }
    const std::string& GetLastAttackerName() const { return lastAttackerName_; }
    void Reset();

    // --- 弾薬 & リロード関連 ---
    int GetMagazineAmmo() const { return magazineAmmo_; }
    int GetMaxMagazineAmmo() const { return maxMagazineAmmo_; }
    int GetReserveAmmo() const { return reserveAmmo_; }
    void AddReserveAmmo(int count) { reserveAmmo_ += count; }
    bool IsReloading() const { return isReloading_; }
    float GetReloadProgress() const { return isReloading_ && reloadDuration_ > 0.0f ? (reloadDuration_ - reloadTimer_) / reloadDuration_ : 0.0f; }
    float GetReloadCancelledTimer() const { return reloadCancelledTimer_; }

    // --- 物資 & インベントリ関連 ---
    int GetMedkitCount() const { return medkitCount_; }
    void AddMedkits(int count) { medkitCount_ += count; }
    int GetLootValue() const { return lootValue_; }
    void AddLootValue(int val) { lootValue_ += val; }

    // --- 治療アクション関連 ---
    bool IsHealing() const { return isHealing_; }
    float GetHealProgress() const { return isHealing_ && healDuration_ > 0.0f ? (healDuration_ - healTimer_) / healDuration_ : 0.0f; }
    bool StartHealing();
    void Heal(float amount) { hp_ = (std::min)(maxHp_, hp_ + amount); }

    // --- スタミナ & 回避関連 ---
    float GetStamina() const { return stamina_; }
    float GetMaxStamina() const { return maxStamina_; }
    float GetStaminaRatio() const { return maxStamina_ > 0.0f ? stamina_ / maxStamina_ : 0.0f; }
    bool IsDodging() const { return isDodging_; }
    bool IsMoving() const { return isMoving_; }
    float GetDodgeTimer() const { return dodgeTimer_; }
    float GetDodgeDuration() const { return dodgeDuration_; }

    // --- 照準拡散 & 遮蔽関連 ---
    float GetCurrentSpread() const { return currentSpread_; }
    bool IsInCover() const { return isInCover_; }
    void SetInCover(bool inCover) { isInCover_ = inCover; }

private:
    // --- 内部更新メソッド ---
    void UpdateSpread(float deltaTime, bool isMoving);

private:
    // --- 3Dオブジェクト & コンポーネント ---
    std::unique_ptr<Object3d> object3d_;
    Vector3 position_ = { 0.0f, 0.0f, 0.0f };
    Vector3 drawPos_ = { 0.0f, 0.0f, 0.0f };
    std::unique_ptr<SphereCollider> collider_;
    Object3dCom* object3dCom_ = nullptr;
    Camera* camera_ = nullptr;
    uint32_t defaultTextureIndex_ = UINT32_MAX;

    // --- 体力 & 被弾ステート ---
    float hp_ = kDefaultMaxHp;
    float maxHp_ = kDefaultMaxHp;
    bool isDead_ = false;
    float invincibilityTimer_ = 0.0f;
    const float invincibilityDuration_ = kInvincibilityDuration;
    float hitFlashTimer_ = 0.0f;
    const float hitFlashDuration_ = kHitFlashDuration;
    Vector3 hitFlinchOffset_ = { 0.0f, 0.0f, 0.0f };

    // --- 射撃パラメータ ---
    Vector3 bulletSpawnOffset_ = { 0.0f, 0.70f, 0.95f };
    float bulletSpeed_ = kBulletSpeed;
    float bulletLifeTime_ = kBulletLifeTime;
    float shotCooldown_ = kShotCooldown;
    float shotCooldownTimer_ = 0.0f;

    // --- 弾薬 & リロード ---
    int maxMagazineAmmo_ = kDefaultMaxMagazineAmmo;
    int magazineAmmo_ = kDefaultMaxMagazineAmmo;
    int reserveAmmo_ = kDefaultReserveAmmo;
    bool isReloading_ = false;
    float reloadTimer_ = 0.0f;
    const float reloadDuration_ = kReloadDuration;
    float reloadCancelledTimer_ = 0.0f;

    // --- 救急キット & 治療 ---
    int medkitCount_ = kDefaultMedkitCount;
    int lootValue_ = 0;
    bool isHealing_ = false;
    float healTimer_ = 0.0f;
    const float healDuration_ = kHealDuration;

    // --- 回避 & スタミナ ---
    bool isDodging_ = false;
    float dodgeTimer_ = 0.0f;
    const float dodgeDuration_ = kDodgeDuration;
    Vector3 dodgeDirection_ = { 0.0f, 0.0f, 1.0f };
    const float dodgeSpeed_ = kDodgeSpeed;
    float stamina_ = kDefaultMaxStamina;
    float maxStamina_ = kDefaultMaxStamina;
    const float dodgeStaminaCost_ = kDodgeStaminaCost;
    const float staminaRegenRate_ = kStaminaRegenRate;

    // --- 照準拡散 & 遮蔽 ---
    float currentSpread_ = 0.0f;
    bool isMoving_ = false;
    bool isInCover_ = false;

    // --- 死因レポート用 ---
    std::string causeOfDeath_ = "ENEMY GUNFIRE (7.62x39mm PS)";
    std::string lastAttackerName_ = "HOSTILE PATROL";
};


