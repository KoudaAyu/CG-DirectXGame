#pragma once
#include "Collider.h"

/// <summary>
/// 直方体コライダー (AABB / OBB 判定用の形状データを保持)
/// </summary>
class BoxCollider : public Collider
{
public:
    /// <summary>
    /// コンストラクタ
    /// </summary>
    /// <param name="size">直方体の幅・高・奥行の各辺のサイズ（全幅）</param>
    /// <param name="refPos">追従する対象の座標ポインタ</param>
    /// <param name="refRot">追従する対象の回転ポインタ (NULLの場合はAABBとして扱います)</param>
    /// <param name="attribute">衝突属性タグ</param>
    BoxCollider(const Vector3& size, Vector3* refPos, const Vector3* refRot, CollisionAttribute attribute)
        : Collider(ColliderType::Box, attribute)
        , size_(size)
        , referencePosition_(refPos)
        , referenceRotation_(refRot)
    {}

    virtual ~BoxCollider() override = default;

    // --- ゲッター / セッター ---
    
    /// <summary>
    /// 箱の中心からの各軸への半サイズ (Extents) を取得します。
    /// </summary>
    Vector3 GetExtents() const
    {
        return { size_.x * 0.5f, size_.y * 0.5f, size_.z * 0.5f };
    }

    const Vector3& GetSize() const { return size_; }
    void SetSize(const Vector3& size) { size_ = size; }

    /// <summary>
    /// 追従対象の回転（オイラー角）を取得します。
    /// </summary>
    Vector3 GetWorldRotation() const
    {
        if (referenceRotation_)
        {
            return *referenceRotation_;
        }
        return { 0.0f, 0.0f, 0.0f };
    }

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
    Vector3 size_;                    // 直方体のサイズ (幅, 高さ, 奥行き)
    Vector3* referencePosition_; // 基準となる位置ポインタ
    const Vector3* referenceRotation_; // 基準となる回転ポインタ (オイラー角)
};
