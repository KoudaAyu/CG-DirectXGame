#pragma once
#include "Collider.h"
#include <algorithm>
#include <cmath>

/// <summary>
/// 球形・楕円体コライダー（スライムの伸縮変形・3軸スケール追従対応）
/// </summary>
class SphereCollider : public Collider
{
public:
    /// <summary>
    /// コンストラクタ
    /// </summary>
    /// <param name="radius">球の基本半径</param>
    /// <param name="refPos">追従する対象の座標ポインタ</param>
    /// <param name="attribute">衝突属性タグ</param>
    SphereCollider(float radius, Vector3* refPos, CollisionAttribute attribute)
        : Collider(ColliderType::Sphere, attribute)
        , radius_(radius)
        , referencePosition_(refPos)
    {}

    virtual ~SphereCollider() override = default;

    // --- ゲッター / セッター ---
    
    float GetRadius() const { return radius_; }
    void SetRadius(float radius) { radius_ = radius; }

    void SetScale(const Vector3& scale) { scale_ = scale; }
    Vector3 GetScale() const { return scaleRef_ ? *scaleRef_ : scale_; }
    void SetScaleRef(const Vector3* sRef) { scaleRef_ = sRef; }

    void SetRotation(const Vector3& rot) { rotation_ = rot; }
    Vector3 GetRotation() const { return rotationRef_ ? *rotationRef_ : rotation_; }
    void SetRotationRef(const Vector3* rRef) { rotationRef_ = rRef; }

    /// <summary>
    /// 3軸変形を考慮した実効バウンディング半径を取得
    /// </summary>
    float GetEffectiveRadius() const
    {
        Vector3 s = GetScale();
        float maxS = (std::max)({ std::abs(s.x), std::abs(s.y), std::abs(s.z) });
        return radius_ * (maxS > 0.001f ? maxS : 1.0f);
    }

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

    /// <summary>
    /// コライダーの現在の世界座標を設定（親オブジェクトの座標を補正）
    /// </summary>
    virtual void SetWorldPosition(const Vector3& pos) override
    {
        if (referencePosition_)
        {
            *referencePosition_ = pos - GetPositionOffset();
        }
    }

private:
    float radius_;                  // 球の基本半径
    Vector3* referencePosition_ = nullptr; // 基準となる位置ポインタ

    Vector3 scale_{ 1.0f, 1.0f, 1.0f };       // 3軸変形スケール（ストレッチ・スクワッシュ）
    const Vector3* scaleRef_ = nullptr;        // スケール追従ポインタ

    Vector3 rotation_{ 0.0f, 0.0f, 0.0f };    // 回転オイラー角
    const Vector3* rotationRef_ = nullptr;     // 回転追従ポインタ
};
