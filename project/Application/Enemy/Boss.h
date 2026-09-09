#pragma once

#include <vector>

#include "Application/Enemy/EnemyBase.h"

/**
 * @brief ボスの設定（ImGui から調整する用。実体は Boss.cpp の static）
 */
struct BossConfig
{
    // --- モデル ---
    const char* directory = "Resources/Enemy/Boss";
    const char* fileName = "BOSS.gltf";
    float modelScale = 1.20f;        //!< BOSS.gltf は高さ約 4.85 なので 1.2 で 6m 弱になる
    float groundOffsetRatio = 0.0f;  //!< モデル原点は足元にある
    Vector4 tintColor{ 1.0f, 1.0f, 1.0f, 1.0f };

    // --- 当たり判定（モデルローカル単位。実寸は modelScale 倍される）---
    float hitRadiusRatio = 1.80f;
    float hitOffsetRatio = 1.80f;

    // --- ステータス ---
    int maxHp = 100;                 //!< 初期HP。HP がそのまま「強さ」になる
    int selfDestructDamage = 5;      //!< プレイヤーの自爆1回で減る量

    // --- 挙動 ---
    float turnSpeed = 1.6f;          //!< プレイヤーへ向き直る速さ
    float idleSpinSpeed = 0.0f;      //!< 常時回転させたいとき用（0 でプレイヤー追従のみ）
    float hopHeight = 0.10f;         //!< 呼吸するような上下（見た目だけ）
    float hopSpeed = 1.8f;
    float idleWobble = 0.04f;        //!< 待機中のぷにぷに量（見た目だけ）

    // --- 全方向弾 ---
    int bulletWays = 12;             //!< 1回に撃つ方向の数（12〜）
    float shootInterval = 2.4f;      //!< 発射間隔 (秒)
    float bulletSpeed = 8.0f;
    float bulletLifeTime = 4.0f;
    float bulletScale = 0.40f;
    float bulletHitRadius = 0.36f;
    float muzzleHeightRatio = 1.60f; //!< 発射口の高さ（モデルローカル単位）
    float muzzleForward = 0.60f;     //!< 中心からの発射半径 (ワールド m)
    float spinPerVolley = 0.26f;     //!< 1回撃つごとに方向をずらす角度 (rad)。渦になる
    const char* bulletDirectory = "Resources/Enemy/FlowerSunward";
    const char* bulletFileName = "Sunward_Bullet.gltf";
    Vector4 bulletColor{ 1.0f, 0.35f, 0.85f, 1.0f };

    // --- アニメーション ---
    // BOSS.gltf のクリップ: Jump_After / Jump_Wait / Walk / Walk_Wait
    bool useAnimation = true;
    const char* clipIdle = "Walk_Wait";
    const char* clipAttack = "Jump_After";
    float animSpeed = 1.0f;
};

/// @brief ボスの設定を取得（書き換え可能。ImGui から調整する用）
BossConfig& GetBossConfig();

/**
 * @brief ボス
 *
 * 雑魚（MobEnemy）との違い:
 *   - **HP を持つ**。プレイヤーが自爆すると HP が減るだけで死なない
 *   - HP がそのまま「強さ」なので、プレイヤーの塊サイズより HP が小さくなると
 *     体当たりで倒せるようになる（EnemyCollision の強弱判定がそのまま効く）
 *   - その場に固定されたまま、全方向に弾をばらまく
 *   - 雑魚よりずっと禍々しいオーラを出す（発生は GamePlaySceneFx 側。
 *     ここは `GetAuraIntensity()` で「どれだけ激しくするか」だけを持つ）
 *
 * @note フェーズ管理（登場フォーカス・戦闘・死亡演出・クリア遷移）は BossFight が持つ。
 *       このクラスは「1体のボスの状態と見た目」だけを持ち、
 *       いつ動いていいか・いつ死ぬかは BossFight から指示される
 */
class Boss : public EnemyBase
{
public:
    /// @brief 1回ぶんの全方向発射要求
    struct ShootBurst
    {
        bool fire = false;
        Vector3 origin{ 0.0f, 0.0f, 0.0f };      //!< 発射口の中心（ワールド）
        std::vector<Vector3> directions;         //!< 正規化済みの水平方向ベクトル
    };

public:
    Boss() = default;
    ~Boss() override = default;

    EnemyType GetType() const override { return EnemyType::Boss; }
    const char* GetTypeName() const override { return "Boss"; }

    // --- HP ---
    int GetHp() const { return hp_; }
    int GetMaxHp() const { return maxHp_; }
    float GetHpRatio() const;

    /// @brief 最大HPごと設定し直す（スポーン時に呼ぶ）
    void ResetHp(int maxHp);

    /**
     * @brief ダメージを与える
     * @return HP が 0 以下になったら true
     * @note ここでは死亡演出に入らない。撃破の判断は BossFight が行う
     */
    bool ApplyDamage(int amount);

    /// @brief HP を直接いじる（ImGui デバッグ用）
    void SetHp(int hp);

    // --- 射撃 ---
    /// @brief 発射要求を取り出してクリアする
    bool TakeShootBurst(ShootBurst& out);

    /// @brief 発射タイマーを止める／再開する（フォーカス演出中は止める）
    void SetShootEnabled(bool enable) { shootEnabled_ = enable; }
    bool IsShootEnabled() const { return shootEnabled_; }

    // --- 演出 ---
    /// @brief オーラの激しさ (1.0 が通常。死亡演出でどんどん上がる)
    float GetAuraIntensity() const { return auraIntensity_; }
    void SetAuraIntensity(float v) { auraIntensity_ = v; }

    /// @brief 死亡演出中の揺れ幅 (m)。0 で揺れない
    void SetDeathShake(float amplitude) { deathShake_ = amplitude; }

    /// @brief 見た目の半径（オーラの湧く範囲・カメラのフォーカス距離に使う）
    float GetVisualRadius() const;

    /// @brief ヒットボックス中心のワールド座標より少し高い「顔の高さ」
    Vector3 GetHeadPosition() const;

    /// @brief いま向いている方向（ワールドの yaw）
    float GetYaw() const { return yaw_; }

protected:
    ModelSpec GetModelSpec() const override;
    void OnInitialized() override;
    void UpdateBehavior(const EnemyUpdateContext& ctx) override;
    Vector3 GetRenderScale() const override;
    float GetVisualOffsetY() const override;

private:
    int hp_ = 100;
    int maxHp_ = 100;

    float shootTimer_ = 0.0f;
    float volleyPhase_ = 0.0f;  //!< 撃つたびにずらす角度。渦を巻いて見える
    bool shootEnabled_ = true;
    ShootBurst burst_;

    float hopPhase_ = 0.0f;
    float auraIntensity_ = 1.0f;
    float deathShake_ = 0.0f;
    float shakePhase_ = 0.0f;
};
