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
    Capsule, // カプセル
    Mesh,    // メッシュ (ポリゴン精密判定)
    Skeleton // スケルトン (関節ごとの球体コライダー)
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

struct CollisionInfo
{
    class Collider* other = nullptr; // 衝突相手
    Vector3 contactPoint{};          // 衝突点（ワールド座標）
    Vector3 normal{};                // 衝突法線（相手から自分への方向）
    float depth = 0.0f;              // めり込み深さ
};

/**
 * @brief コライダーの基底クラス
 * @details すべての当たり判定形状クラス（Sphere, Box, Capsule）のベースとなります。
 */
class Collider
{
public:
	/**
	 * @brief コンストラクタ
	 * @param type コライダーの形状タイプ
	 * @param attribute 衝突グループ属性
	 */
    Collider(ColliderType type, CollisionAttribute attribute);

    virtual ~Collider();

    // --- ゲッター / セッター ---
    
    ColliderType GetType() const { return type_; }
    CollisionAttribute GetAttribute() const { return attribute_; }

    void SetPositionOffset(const Vector3& offset) { positionOffset_ = offset; }
    const Vector3& GetPositionOffset() const { return positionOffset_; }

	/**
	 * @brief コライダーの現在の世界座標を取得（オフセット含む）
	 * @details 派生クラス側でターゲットの本体座標とオフセットを合成して返します。
	 * @return コライダーのグローバル座標
	 */
    virtual Vector3 GetWorldPosition() const = 0;

	/**
	 * @brief コライダーの現在の世界座標を設定（親オブジェクトの座標を補正）
	 * @param pos 設定する世界座標
	 */
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
    
	/**
	 * @brief 他のコライダーと衝突した際に呼び出されるコールバックを設定します。
	 * @param callback 衝突時コールバック関数ポインタ
	 */
    void SetOnCollision(std::function<void(Collider* other)> callback) { onCollision_ = callback; }
    void SetOnCollision(std::function<void(const CollisionInfo& info)> callback) { onCollisionInfo_ = callback; }

    // --- トリガーイベントコールバック ---
    void SetOnTriggerEnter(std::function<void(Collider* other)> callback) { onTriggerEnter_ = callback; }
    void SetOnTriggerStay(std::function<void(Collider* other)> callback) { onTriggerStay_ = callback; }
    void SetOnTriggerExit(std::function<void(Collider* other)> callback) { onTriggerExit_ = callback; }
    
    /// <summary>
    /// 衝突イベントをトリガーします。
    /// </summary>
    void OnCollision(const CollisionInfo& info)
    {
        if (onCollisionInfo_)
        {
            onCollisionInfo_(info);
        }
        if (onCollision_)
        {
            onCollision_(info.other);
        }
    }

    /// <summary>
    /// トリガーイベントを呼び出します。
    /// </summary>
    void OnTriggerEnter(Collider* other) { if (onTriggerEnter_) onTriggerEnter_(other); }
    void OnTriggerStay(Collider* other)  { if (onTriggerStay_)  onTriggerStay_(other); }
    void OnTriggerExit(Collider* other)  { if (onTriggerExit_)  onTriggerExit_(other); }

private:
    ColliderType type_;                                     // 形状タイプ
    CollisionAttribute attribute_;                           // グループ属性
    Vector3 positionOffset_;                                // 本体からの相対オフセット座標
    bool isTrigger_;                                        // トリガー設定 (trueなら押し出し補正をスキップ)
    bool isEnabled_;                                        // 有効状態フラグ
    std::function<void(Collider* other)> onCollision_;      // 衝突時コールバック関数
    std::function<void(const CollisionInfo& info)> onCollisionInfo_; // 詳細衝突コールバック

    // トリガー用コールバック
    std::function<void(Collider* other)> onTriggerEnter_;
    std::function<void(Collider* other)> onTriggerStay_;
    std::function<void(Collider* other)> onTriggerExit_;
};
