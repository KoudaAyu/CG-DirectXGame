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
    // 定数定義 (Named Constants) - マジックナンバーの完全排除
    // =========================================================================
    static constexpr float kDefaultMaxHp                  = 100.0f;  // 最大体力
    static constexpr float kDefaultMaxStamina             = 100.0f;  // 最大スタミナ
    static constexpr int   kDefaultMaxMagazineAmmo        = 30;      // 1マガジン装填数 (30発)
    static constexpr int   kDefaultReserveAmmo            = 90;      // 初期予備弾薬 (90発)
    static constexpr int   kDefaultMedkitCount            = 1;       // 初期救急キット所持数
    static constexpr float kMedkitHealAmount              = 40.0f;   // 救急キット1回あたりの回復量 (HP+40)
    static constexpr float kHealDuration                  = 1.8f;    // 治療所要時間 (1.8秒)
    static constexpr float kReloadDuration                = 1.5f;    // リロード所要時間 (1.5秒)
    static constexpr float kReloadCancelledNotifyDuration = 1.2f;    // リロード中断通知の表示秒数
    static constexpr float kDodgeDuration                 = 0.4f;    // 回避ローリング所要時間 (0.4秒)
    static constexpr float kDodgeSpeed                    = 0.15f;   // 回避ローリング移動速度
    static constexpr float kDodgeStaminaCost              = 30.0f;   // 回避1回あたりのスタミナ消費量
    static constexpr float kStaminaRegenRate              = 15.0f;   // 毎秒スタミナ自然回復量
    static constexpr float kMoveSpeed                     = 0.05f;   // 通常移動速度
    static constexpr float kInvincibilityDuration         = 1.0f;    // 被弾後の無敵時間 (1.0秒)
    static constexpr float kHitFlashDuration              = 0.22f;   // 被弾時の高輝度赤フラッシュ秒数
    static constexpr float kBulletSpeed                   = 0.45f;   // 弾丸初速
    static constexpr float kBulletLifeTime                = 2.0f;    // 弾丸生存時間 (2.0秒)
    static constexpr float kShotCooldown                  = 0.12f;   // 連射間隔 (秒)
    static constexpr float kColliderRadius                = 0.55f;   // 衝突判定スフィア半径
    static constexpr float kBaseSpread                    = 0.01f;   // 基礎レティクル拡散角 (rad)
    static constexpr float kMoveSpreadPenalty             = 0.06f;   // 移動時の拡散ペナルティ
    static constexpr float kShootSpreadPenalty            = 0.04f;   // 射撃連射時の拡散ペナルティ
    static constexpr float kMaxSpread                     = 0.25f;   // 最大拡散角
    static constexpr float kSpreadRecoverRate             = 0.3f;    // 照準収束速度

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


