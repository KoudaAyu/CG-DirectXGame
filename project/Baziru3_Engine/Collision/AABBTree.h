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
    /// レイとAABB木の交差判定を行い、最も近い交点情報を取得します
    /// </summary>
    bool Raycast(const Vector3& rayStart, const Vector3& rayDir, float maxDist, float& outHitDist, Vector3& outHitNormal) const;

    /// <summary>
    /// 木全体のルートノードのAABB（モデル全体を包む境界）を取得します
    /// </summary>
    bool GetRootBounds(Vector3& outMin, Vector3& outMax) const;

private:
    int BuildRecursive(std::vector<BVHTriangle>& tris, int start, int end);
    bool RaycastRecursive(int nodeIndex, const Vector3& rayStart, const Vector3& rayDir, float maxDist, float& outHitDist, Vector3& outHitNormal) const;

    static bool TestRayAABB(const Vector3& rayStart, const Vector3& rayDir, float maxDist, const Vector3& minBounds, const Vector3& maxBounds);
    static bool TestRayTriangle(const Vector3& rayStart, const Vector3& rayDir, float maxDist, const Vector3& v0, const Vector3& v1, const Vector3& v2, float& outDist, Vector3& outNormal);

private:
    std::vector<BVHNode> nodes_;
};

} // namespace BaziruEngine::Collision
