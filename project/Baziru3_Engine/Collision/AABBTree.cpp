#include "AABBTree.h"
#include <cmath>
#include <algorithm>

namespace BaziruEngine::Collision {

namespace {

inline float Dot(const Vector3& a, const Vector3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vector3 Cross(const Vector3& a, const Vector3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

inline Vector3 Normalize(const Vector3& v)
{
    float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len > 1e-5f)
    {
        return { v.x / len, v.y / len, v.z / len };
    }
    return { 0.0f, 0.0f, 0.0f };
}

} // namespace

void AABBTree::Build(const std::vector<Vector3>& vertices, const std::vector<uint32_t>& indices)
{
    nodes_.clear();
    if (vertices.empty() || indices.empty())
        return;

    // 三角形リストを作成
    std::vector<BVHTriangle> tris;
    tris.reserve(indices.size() / 3);
    for (size_t i = 0; i < indices.size(); i += 3)
    {
        if (i + 2 >= indices.size()) break;
        BVHTriangle tri;
        tri.v0 = vertices[indices[i]];
        tri.v1 = vertices[indices[i + 1]];
        tri.v2 = vertices[indices[i + 2]];
        tri.index = static_cast<uint32_t>(i / 3);
        tris.push_back(tri);
    }

    if (!tris.empty())
    {
        BuildRecursive(tris, 0, static_cast<int>(tris.size()));
    }
}

int AABBTree::BuildRecursive(std::vector<BVHTriangle>& tris, int start, int end)
{
    BVHNode node;
    node.minBounds = { 1e9f, 1e9f, 1e9f };
    node.maxBounds = { -1e9f, -1e9f, -1e9f };

    // 全ての三角形のバウンディングボックスを計算
    for (int i = start; i < end; ++i)
    {
        const auto& tri = tris[i];
        auto envelop = [&](const Vector3& v) {
            node.minBounds.x = (std::min)(node.minBounds.x, v.x);
            node.minBounds.y = (std::min)(node.minBounds.y, v.y);
            node.minBounds.z = (std::min)(node.minBounds.z, v.z);
            node.maxBounds.x = (std::max)(node.maxBounds.x, v.x);
            node.maxBounds.y = (std::max)(node.maxBounds.y, v.y);
            node.maxBounds.z = (std::max)(node.maxBounds.z, v.z);
        };
        envelop(tri.v0);
        envelop(tri.v1);
        envelop(tri.v2);
    }

    int count = end - start;
    if (count <= 4)
    {
        // リーフノードとして作成
        for (int i = start; i < end; ++i)
        {
            node.triangles.push_back(tris[i]);
        }
        nodes_.push_back(node);
        return static_cast<int>(nodes_.size()) - 1;
    }

    // 最も長い軸を選択してソート
    Vector3 size = node.maxBounds - node.minBounds;
    int axis = 0; // 0=X, 1=Y, 2=Z
    if (size.y > size.x && size.y > size.z) axis = 1;
    else if (size.z > size.x && size.z > size.y) axis = 2;

    auto getCenterVal = [&](const BVHTriangle& t) {
        if (axis == 1) return (t.v0.y + t.v1.y + t.v2.y) / 3.0f;
        else if (axis == 2) return (t.v0.z + t.v1.z + t.v2.z) / 3.0f;
        return (t.v0.x + t.v1.x + t.v2.x) / 3.0f;
    };

    std::sort(tris.begin() + start, tris.begin() + end, [&](const BVHTriangle& a, const BVHTriangle& b) {
        return getCenterVal(a) < getCenterVal(b);
    });

    int mid = start + count / 2;

    // ダミーの親ノードをプッシュしてインデックスを予約
    int parentIdx = static_cast<int>(nodes_.size());
    nodes_.push_back(node);

    int left = BuildRecursive(tris, start, mid);
    int right = BuildRecursive(tris, mid, end);

    nodes_[parentIdx].leftChild = left;
    nodes_[parentIdx].rightChild = right;
    nodes_[parentIdx].minBounds = node.minBounds;
    nodes_[parentIdx].maxBounds = node.maxBounds;

    return parentIdx;
}

bool AABBTree::Raycast(const Vector3& rayStart, const Vector3& rayDir, float maxDist, float& outHitDist, Vector3& outHitNormal, Vector3& outV0, Vector3& outV1, Vector3& outV2) const
{
    if (nodes_.empty()) return false;
    return RaycastRecursive(0, rayStart, rayDir, maxDist, outHitDist, outHitNormal, outV0, outV1, outV2);
}

bool AABBTree::GetRootBounds(Vector3& outMin, Vector3& outMax) const
{
    if (nodes_.empty()) return false;
    outMin = nodes_[0].minBounds;
    outMax = nodes_[0].maxBounds;
    return true;
}

void AABBTree::GetNodesAtDepth(int targetDepth, std::vector<std::pair<Vector3, Vector3>>& outBounds) const
{
    outBounds.clear();
    if (nodes_.empty()) return;
    GetNodesAtDepthRecursive(0, 0, targetDepth, outBounds);
}

bool AABBTree::RaycastRecursive(int nodeIndex, const Vector3& rayStart, const Vector3& rayDir, float maxDist, float& outHitDist, Vector3& outHitNormal, Vector3& outV0, Vector3& outV1, Vector3& outV2) const
{
    const auto& node = nodes_[nodeIndex];
    if (!TestRayAABB(rayStart, rayDir, maxDist, node.minBounds, node.maxBounds))
        return false;

    if (node.IsLeaf())
    {
        bool hit = false;
        float nearestDist = maxDist;
        Vector3 nearestNormal = { 0, 1, 0 };
        Vector3 nearestV0, nearestV1, nearestV2;

        for (const auto& tri : node.triangles)
        {
            float dist;
            Vector3 normal;
            if (TestRayTriangle(rayStart, rayDir, nearestDist, tri.v0, tri.v1, tri.v2, dist, normal))
            {
                hit = true;
                nearestDist = dist;
                nearestNormal = normal;
                nearestV0 = tri.v0;
                nearestV1 = tri.v1;
                nearestV2 = tri.v2;
            }
        }

        if (hit)
        {
            outHitDist = nearestDist;
            outHitNormal = nearestNormal;
            outV0 = nearestV0;
            outV1 = nearestV1;
            outV2 = nearestV2;
            return true;
        }
        return false;
    }

    float leftDist = maxDist;
    Vector3 leftNormal;
    Vector3 leftV0, leftV1, leftV2;
    bool leftHit = RaycastRecursive(node.leftChild, rayStart, rayDir, maxDist, leftDist, leftNormal, leftV0, leftV1, leftV2);

    float rightDist = maxDist;
    Vector3 rightNormal;
    Vector3 rightV0, rightV1, rightV2;
    bool rightHit = RaycastRecursive(node.rightChild, rayStart, rayDir, leftHit ? leftDist : maxDist, rightDist, rightNormal, rightV0, rightV1, rightV2);

    if (leftHit && rightHit)
    {
        if (leftDist < rightDist)
        {
            outHitDist = leftDist;
            outHitNormal = leftNormal;
            outV0 = leftV0;
            outV1 = leftV1;
            outV2 = leftV2;
        }
        else
        {
            outHitDist = rightDist;
            outHitNormal = rightNormal;
            outV0 = rightV0;
            outV1 = rightV1;
            outV2 = rightV2;
        }
        return true;
    }
    else if (leftHit)
    {
        outHitDist = leftDist;
        outHitNormal = leftNormal;
        outV0 = leftV0;
        outV1 = leftV1;
        outV2 = leftV2;
        return true;
    }
    else if (rightHit)
    {
        outHitDist = rightDist;
        outHitNormal = rightNormal;
        outV0 = rightV0;
        outV1 = rightV1;
        outV2 = rightV2;
        return true;
    }

    return false;
}

void AABBTree::GetNodesAtDepthRecursive(int nodeIndex, int currentDepth, int targetDepth, std::vector<std::pair<Vector3, Vector3>>& outBounds) const
{
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(nodes_.size())) return;

    const auto& node = nodes_[nodeIndex];
    if (currentDepth == targetDepth || node.IsLeaf())
    {
        outBounds.push_back({ node.minBounds, node.maxBounds });
        return;
    }

    GetNodesAtDepthRecursive(node.leftChild, currentDepth + 1, targetDepth, outBounds);
    GetNodesAtDepthRecursive(node.rightChild, currentDepth + 1, targetDepth, outBounds);
}

bool AABBTree::TestRayAABB(const Vector3& rayStart, const Vector3& rayDir, float maxDist, const Vector3& minBounds, const Vector3& maxBounds)
{
    float tmin = 0.0f;
    float tmax = maxDist;

    // X軸
    if (std::abs(rayDir.x) < 1e-6f)
    {
        if (rayStart.x < minBounds.x || rayStart.x > maxBounds.x) return false;
    }
    else
    {
        float ood = 1.0f / rayDir.x;
        float t1 = (minBounds.x - rayStart.x) * ood;
        float t2 = (maxBounds.x - rayStart.x) * ood;
        if (t1 > t2) std::swap(t1, t2);
        tmin = (std::max)(tmin, t1);
        tmax = (std::min)(tmax, t2);
        if (tmin > tmax) return false;
    }

    // Y軸
    if (std::abs(rayDir.y) < 1e-6f)
    {
        if (rayStart.y < minBounds.y || rayStart.y > maxBounds.y) return false;
    }
    else
    {
        float ood = 1.0f / rayDir.y;
        float t1 = (minBounds.y - rayStart.y) * ood;
        float t2 = (maxBounds.y - rayStart.y) * ood;
        if (t1 > t2) std::swap(t1, t2);
        tmin = (std::max)(tmin, t1);
        tmax = (std::min)(tmax, t2);
        if (tmin > tmax) return false;
    }

    // Z軸
    if (std::abs(rayDir.z) < 1e-6f)
    {
        if (rayStart.z < minBounds.z || rayStart.z > maxBounds.z) return false;
    }
    else
    {
        float ood = 1.0f / rayDir.z;
        float t1 = (minBounds.z - rayStart.z) * ood;
        float t2 = (maxBounds.z - rayStart.z) * ood;
        if (t1 > t2) std::swap(t1, t2);
        tmin = (std::max)(tmin, t1);
        tmax = (std::min)(tmax, t2);
        if (tmin > tmax) return false;
    }

    return true;
}

bool AABBTree::TestRayTriangle(const Vector3& rayStart, const Vector3& rayDir, float maxDist, const Vector3& v0, const Vector3& v1, const Vector3& v2, float& outDist, Vector3& outNormal)
{
    Vector3 edge1 = v1 - v0;
    Vector3 edge2 = v2 - v0;
    Vector3 h = Cross(rayDir, edge2);
    float a = Dot(edge1, h);
    if (a > -1e-6f && a < 1e-6f)
        return false;

    float f = 1.0f / a;
    Vector3 s = rayStart - v0;
    float u = f * Dot(s, h);
    if (u < 0.0f || u > 1.0f)
        return false;

    Vector3 q = Cross(s, edge1);
    float v = f * Dot(rayDir, q);
    if (v < 0.0f || u + v > 1.0f)
        return false;

    float t = f * Dot(edge2, q);
    if (t > 1e-6f && t < maxDist)
    {
        outDist = t;
        outNormal = Normalize(Cross(edge1, edge2));
        if (Dot(outNormal, rayDir) > 0.0f)
        {
            outNormal = outNormal * -1.0f;
        }
        return true;
    }
    return false;
}

} // namespace BaziruEngine::Collision
