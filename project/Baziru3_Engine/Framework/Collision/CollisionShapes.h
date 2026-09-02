#pragma once

#include "Collider.h"
#include "Vector.h"

struct CollisionData;
struct Ray;
struct RaycastHit;

/// <summary>
/// 各種コライダー形状の交差判定と押し出し計算
/// </summary>
namespace CollisionShapes
{
    /// <summary>
    /// 2つのコライダー間の衝突判定と押し出しベクトルの算出
    /// </summary>
    /// <param name="a">コライダーA</param>
    /// <param name="b">コライダーB</param>
    /// <param name="outPushDir">Aを押し出す方向ベクトル（出力）</param>
    /// <param name="outPushLen">めり込み深さ・押し出し量（出力）</param>
    /// <returns>衝突している場合 true</returns>
    bool CheckCollision(const CollisionData& a, const CollisionData& b, Vector3& outPushDir, float& outPushLen);

    /// <summary>
    /// 球 vs 球 の衝突判定
    /// </summary>
    bool CheckSphereSphere(const CollisionData& a, const CollisionData& b, Vector3& outPushDir, float& outPushLen);

    /// <summary>
    /// 球 vs ボックス の衝突判定
    /// </summary>
    bool CheckSphereBox(const CollisionData& sphere, const CollisionData& box, Vector3& outPushDir, float& outPushLen);

    /// <summary>
    /// 球 vs カプセル の衝突判定
    /// </summary>
    bool CheckSphereCapsule(const CollisionData& sphere, const CollisionData& capsule, Vector3& outPushDir, float& outPushLen);

    /// <summary>
    /// ボックス vs ボックス（OBB）の衝突判定（分離軸定理 SAT）
    /// </summary>
    bool CheckBoxBox(const CollisionData& a, const CollisionData& b, Vector3& outPushDir, float& outPushLen);

    /// <summary>
    /// ボックス vs カプセル の衝突判定
    /// </summary>
    bool CheckBoxCapsule(const CollisionData& box, const CollisionData& capsule, Vector3& outPushDir, float& outPushLen);

    /// <summary>
    /// カプセル vs カプセル の衝突判定
    /// </summary>
    bool CheckCapsuleCapsule(const CollisionData& a, const CollisionData& b, Vector3& outPushDir, float& outPushLen);
}
