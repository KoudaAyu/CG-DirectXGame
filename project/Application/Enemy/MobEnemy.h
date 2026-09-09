#pragma once

#include "Application/Enemy/EnemyBase.h"

/**
 * @brief モブ敵4種の設定
 *
 * 挙動の違い（動く/固定、撃つ/撃たない）はフラグで表現し、
 * クラスは MobEnemy 1つで済ませている。
 * 値は実機で見ながら詰める前提なので、EnemyManager の ImGui から書き換えられる。
 */
struct MobEnemyConfig
{
    const char* typeName = "";

    // --- モデル ---
    const char* directory = "";
    const char* fileName = "";
    float modelScale = 1.0f;          //!< モデル固有の基準スケール
    float groundOffsetRatio = 0.0f;   //!< モデル原点を地面から持ち上げる量（モデルローカル単位）
    Vector4 tintColor{ 1.0f, 1.0f, 1.0f, 1.0f };

    // --- 当たり判定（モデルローカル単位。実寸は scale 倍される）---
    EnemyCollision::HitShape hitShape = EnemyCollision::HitShape::Sphere;
    float hitRadiusRatio = 0.7f;
    Vector3 hitHalfRatio{ 0.6f, 1.0f, 0.6f };
    float hitOffsetRatio = 0.5f;

    // --- 挙動 ---
    bool canMove = false;             //!< 動きまわるか
    bool canShoot = false;            //!< 弾を撃つか
    bool isPushable = false;          //!< プレイヤーに押されて動くか
    bool preventFall = true;          //!< 崖落ち防止（足場の端や急な段差で自発的に落ちないようにする）
    float moveSpeed = 2.0f;           //!< 追跡速度 (m/s)
    float chaseRange = 9.0f;          //!< この距離まで近づかれたら追いかけ始める
    float loseRange = 14.0f;          //!< この距離まで離れられたら諦める（ヒステリシス）
    float keepDistance = 0.6f;        //!< これ以上は近づかない（押し合いのがたつき防止）
    float turnSpeed = 6.0f;           //!< 向き直りの速さ (rad/s 相当の補間係数)

    // --- ホップ演出（アニメーションが入るまでの繋ぎ）---
    float hopHeight = 0.10f;          //!< 跳ねる高さ（スケール比）
    float hopSpeed = 7.0f;            //!< 跳ねる速さ
    float idleWobble = 0.05f;         //!< 待機中のぷにぷに量

    // --- 射撃 ---
    float shootRange = 11.0f;
    float shootInterval = 2.2f;
    float shootIntervalJitter = 0.6f; //!< 同時発射を避けるためのばらつき
    float bulletSpeed = 9.0f;
    float bulletLifeTime = 3.0f;
    float bulletScale = 0.30f;
    float bulletHitRadius = 0.28f;
    float muzzleHeightRatio = 0.9f;   //!< 発射口の高さ（モデルローカル単位）
    float muzzleForward = 0.35f;      //!< 発射口の前方オフセット（ワールド m）
    const char* bulletDirectory = "";
    const char* bulletFileName = "";
    Vector4 bulletColor{ 1.0f, 1.0f, 1.0f, 1.0f };

    // --- スポーン時の強さ範囲（ランダム）---
    int strengthMin = 1;
    int strengthMax = 5;

    // --- アニメーション ---
    // gltf のクリップ名。モデルごとに大文字小文字も名前もバラバラなのでここで対応づける。
    // 空文字なら「そのクリップは無い」扱い（Idle にフォールバックする）
    bool useAnimation = true;
    const char* clipIdle = "";    //!< 待機
    const char* clipWalk = "";    //!< 移動中
    const char* clipAttack = "";  //!< 発射の瞬間（ワンショット）
    const char* clipAlert = "";   //!< プレイヤーを見つけて構えている状態
    float animSpeed = 1.0f;
};

/// @brief 種類ごとの設定を取得（書き換え可能。ImGui から調整する用）
MobEnemyConfig& GetMobEnemyConfig(EnemyType type);

/**
 * @brief モブ敵（Slime / FlowerClover / FlowerLotus / FlowerSunward）
 *
 * - Slime, FlowerLotus     : 近くのプレイヤーの塊を追いかける
 * - FlowerClover, FlowerSunward : 地面に固定
 * - FlowerLotus, FlowerSunward  : 弾を撃つ
 */
class MobEnemy : public EnemyBase
{
public:
    /// @brief 発射要求。EnemyManager が拾って実際の弾を生成する
    struct ShootRequest
    {
        bool fire = false;
        Vector3 origin{ 0.0f, 0.0f, 0.0f };
        Vector3 direction{ 0.0f, 0.0f, 1.0f };
    };

    explicit MobEnemy(EnemyType type) : type_(type) {}
    ~MobEnemy() override = default;

    EnemyType GetType() const override { return type_; }
    const char* GetTypeName() const override;

    const MobEnemyConfig& GetConfig() const { return GetMobEnemyConfig(type_); }

    bool IsChasing() const { return isChasing_; }

    /// @brief 発射要求を取り出してクリアする
    bool TakeShootRequest(ShootRequest& out);

protected:
    ModelSpec GetModelSpec() const override;

    /// @brief 今の状態でループさせるべきクリップ名を返す（移動中 / 警戒中 / 待機）
    const char* PickBaseClip() const;

    void OnInitialized() override;
    void UpdateBehavior(const EnemyUpdateContext& ctx) override;
    Vector3 GetRenderScale() const override;
    float GetVisualOffsetY() const override;

private:
    /// @brief 指定のローカル移動量が崖落ち・急な落差・高すぎる段差にならないか判定する
    bool IsStepSafe(const Vector3& stepLocal, const EnemyUpdateContext& ctx) const;

    EnemyType type_ = EnemyType::Slime;

    bool isChasing_ = false;
    float shootTimer_ = 0.0f;
    float moveAmount_ = 0.0f;   //!< 直近の移動量（ホップ演出の強さ）
    float hopPhase_ = 0.0f;

    ShootRequest shootRequest_;
};
