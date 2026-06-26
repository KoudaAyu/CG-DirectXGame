#pragma once
#include "Collider.h"

/// <summary>
/// 球形コライダー
/// </summary>
class SphereCollider : public Collider
{
public:
    /// <summary>
    /// コンストラクタ
    /// </summary>
    /// <param name="radius">球の半径</param>
    /// <param name="refPos">追従する対象の座標ポインタ</param>
    /// <param name="attribute">衝突属性タグ</param>
    SphereCollider(float radius, const Vector3* refPos, CollisionAttribute attribute)
        : Collider(ColliderType::Sphere, attribute)
        , radius_(radius)
        , referencePosition_(refPos)
    {}

    virtual ~SphereCollider() override = default;

    // --- ゲッター / セッター ---
    
    float GetRadius() const { return radius_; }
    void SetRadius(float radius) { radius_ = radius; }

    /// <summary>
    /// コライダーの現在の世界座標を取得（基準座標ポインタ + オフセット）
    /// </summary>
    virtual Vector3 GetWorldPosition() const override
    {
        if (referencePosition_)
        {
            return *referencePosition_ + GetPositionOffset();
        }
        return GetPositionOffset();
    }

private:
    float radius_;                  // 球の半径
    const Vector3* referencePosition_; // 基準となる位置ポインタ (PlayerやEnemy等の座標ポインタを指定)
};
