#pragma once
#include "../Base/Vector.h"
#include "../Base/Matrix4x4.h"
#include <vector>
#include <algorithm>

namespace BaziruEngine::Collision {

struct BVHTriangle {
    Vector3 v0;
    Vector3 v1;
    Vector3 v2;
    uint32_t index; // 元の三角形のインデックス
    uint32_t vidx0;
    uint32_t vidx1;
    uint32_t vidx2;
};

struct BVHNode {
    Vector3 minBounds;
    Vector3 maxBounds;
    int leftChild = -1;
    int rightChild = -1;
    std::vector<BVHTriangle> triangles; // リーフノードのみデータを持つ
    bool IsLeaf() const { return leftChild == -1 && rightChild == -1; }
};

class AABBTree {
public:
    AABBTree() = default;
    ~AABBTree() = default;

    /// <summary>
    /// メッシュの頂点とインデックス配列からAABB木（BVH）を構築します
    /// </summary>
    void Build(const std::vector<Vector3>& vertices, const std::vector<uint32_t>& indices);

    /// <summary>
    /// 既存のAABB木（BVH）のツリー構造を維持したまま、アニメーション後の頂点に合わせて境界ボックスのみを更新（Refit）します
    /// </summary>
    void Update(const std::vector<Vector3>& vertices);

    /// <summary>
    /// レイとAABB木の交差判定を行い、最も近い交点情報および交差した三角形を取得します
    /// </summary>
    bool Raycast(const Vector3& rayStart, const Vector3& rayDir, float maxDist, float& outHitDist, Vector3& outHitNormal, Vector3& outV0, Vector3& outV1, Vector3& outV2) const;

    /// <summary>
    /// 木全体のルートノードのAABB（モデル全体を包む境界）を取得します
    /// </summary>
    bool GetRootBounds(Vector3& outMin, Vector3& outMax) const;

    /// <summary>
    /// 特定の深さにあるすべてのノードの境界ボックスを取得します（デバッグ用）
    /// </summary>
    void GetNodesAtDepth(int targetDepth, std::vector<std::pair<Vector3, Vector3>>& outBounds) const;

private:
    int BuildRecursive(std::vector<BVHTriangle>& tris, int start, int end);
    void UpdateRecursive(int nodeIndex, const std::vector<Vector3>& vertices);
    bool RaycastRecursive(int nodeIndex, const Vector3& rayStart, const Vector3& rayDir, float maxDist, float& outHitDist, Vector3& outHitNormal, Vector3& outV0, Vector3& outV1, Vector3& outV2) const;
    void GetNodesAtDepthRecursive(int nodeIndex, int currentDepth, int targetDepth, std::vector<std::pair<Vector3, Vector3>>& outBounds) const;

    static bool TestRayAABB(const Vector3& rayStart, const Vector3& rayDir, float maxDist, const Vector3& minBounds, const Vector3& maxBounds);
    static bool TestRayTriangle(const Vector3& rayStart, const Vector3& rayDir, float maxDist, const Vector3& v0, const Vector3& v1, const Vector3& v2, float& outDist, Vector3& outNormal);

private:
    std::vector<BVHNode> nodes_;
};

} // namespace BaziruEngine::Collision
