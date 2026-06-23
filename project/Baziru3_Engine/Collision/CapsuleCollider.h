#pragma once
#include "Collider.h"

/// <summary>
/// カプセル型コライダー (線分と半径によるカプセル形状)
/// </summary>
class CapsuleCollider : public Collider
{
public:
    /// <summary>
    /// コンストラクタ
    /// </summary>
    /// <param name="radius">カプセルの円柱半径</param>
    /// <param name="height">カプセルの直線部分の長さ（全高ではない、シリンダー部の長さ）</param>
    /// <param name="refPos">追従する対象の座標ポインタ</param>
    /// <param name="attribute">衝突属性タグ</param>
    CapsuleCollider(float radius, float height, const Vector3* refPos, CollisionAttribute attribute)
        : Collider(ColliderType::Capsule, attribute)
        , radius_(radius)
        , height_(height)
        , referencePosition_(refPos)
    {}

    virtual ~CapsuleCollider() override = default;

    // --- ゲッター / セッター ---
    
    float GetRadius() const { return radius_; }
    void SetRadius(float radius) { radius_ = radius; }

    float GetHeight() const { return height_; }
    void SetHeight(float height) { height_ = height; }

    /// <summary>
    /// コライダーの現在の世界座標を取得
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
    float radius_;                    // カプセルの半径
    float height_;                    // シリンダー部分の高さ
    const Vector3* referencePosition_; // 基準となる位置ポインタ
};
