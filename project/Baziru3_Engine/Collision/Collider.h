#pragma once
#include "Vector.h"
#include <functional>
#include <string>

/// <summary>
/// コライダーの形状タイプ
/// </summary>
enum class ColliderType
{
    Sphere,  // 球
    Box,     // 直方体 (OBB/AABB)
    Capsule  // カプセル
};

/// <summary>
/// 衝突判定グループ（属性タグ）
/// 各オブジェクトの衝突カテゴリを分類するために使用します。
/// </summary>
enum class CollisionAttribute
{
    Player,    // プレイヤーアヒル
    Enemy,     // 敵キャラクター
    Bullet,    // 弾丸
    Obstacle   // 障害物（カバー等）
};

/// <summary>
/// コライダーの基底クラス
/// すべての当たり判定形状クラス（Sphere, Box, Capsule）のベースとなります。
/// </summary>
class Collider
{
public:
    /// <summary>
    /// コンストラクタ
    /// </summary>
    /// <param name="type">コライダーの形状タイプ</param>
    /// <param name="attribute">衝突グループ属性</param>
    Collider(ColliderType type, CollisionAttribute attribute)
        : type_(type)
        , attribute_(attribute)
        , positionOffset_({ 0.0f, 0.0f, 0.0f })
        , isTrigger_(false)
        , isEnabled_(true)
    {}

    virtual ~Collider() = default;

    // --- ゲッター / セッター ---
    
    ColliderType GetType() const { return type_; }
    CollisionAttribute GetAttribute() const { return attribute_; }

    void SetPositionOffset(const Vector3& offset) { positionOffset_ = offset; }
    const Vector3& GetPositionOffset() const { return positionOffset_; }

    /// <summary>
    /// コライダーの現在の世界座標を取得（オフセット含む）
    /// 派生クラス側でターゲットの本体座標とオフセットを合成して返します。
    /// </summary>
    virtual Vector3 GetWorldPosition() const = 0;

    /// <summary>
    /// コライダーの現在の世界座標を設定（親オブジェクトの座標を補正）
    /// </summary>
    virtual void SetWorldPosition(const Vector3& pos) = 0;

    /// <summary>
    /// 物理的衝突（押し出し）を行わず、接触イベント検知のみを行うかどうか
    /// </summary>
    void SetIsTrigger(bool isTrigger) { isTrigger_ = isTrigger; }
    bool IsTrigger() const { return isTrigger_; }

    /// <summary>
    /// コライダーの有効・無効化設定
    /// </summary>
    void SetIsEnabled(bool isEnabled) { isEnabled_ = isEnabled; }
    bool IsEnabled() const { return isEnabled_; }

    // --- 衝突イベントコールバック ---
    
    /// <summary>
    /// 他のコライダーと衝突した際に呼び出されるコールバックを設定します。
    /// </summary>
    void SetOnCollision(std::function<void(Collider* other)> callback) { onCollision_ = callback; }
    
    /// <summary>
    /// 衝突イベントをトリガーします。
    /// </summary>
    void OnCollision(Collider* other)
    {
        if (onCollision_)
        {
            onCollision_(other);
        }
    }

private:
    ColliderType type_;                                     // 形状タイプ
    CollisionAttribute attribute_;                           // グループ属性
    Vector3 positionOffset_;                                // 本体からの相対オフセット座標
    bool isTrigger_;                                        // トリガー設定 (trueなら押し出し補正をスキップ)
    bool isEnabled_;                                        // 有効状態フラグ
    std::function<void(Collider* other)> onCollision_;      // 衝突時コールバック関数
};
