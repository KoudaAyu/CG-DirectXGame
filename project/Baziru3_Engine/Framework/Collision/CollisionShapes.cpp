#define NOMINMAX
#include "CollisionShapes.h"
#include "CollisionManager.h"
#include "SoftBodyDeformer.h"
#include "SphereCollider.h"
#include "BoxCollider.h"
#include "CapsuleCollider.h"
#include "MeshCollider.h"
#include "SkeletonCollider.h"
#include "Matrix4x4.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3d.h"
#include <cmath>
#include <algorithm>
#include <vector>

namespace CollisionShapes
{
    static inline Vector3 Cross(const Vector3& a, const Vector3& b)
    {
        return {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    }

    static inline float Dot(const Vector3& a, const Vector3& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    static inline float LengthSq(const Vector3& v)
    {
        return Dot(v, v);
    }

    static inline float Length(const Vector3& v)
    {
        return std::sqrt(LengthSq(v));
    }

    static inline Vector3 Normalize(const Vector3& v)
    {
        float len = Length(v);
        if (len > 1e-5f)
        {
            return v * (1.0f / len);
        }
        return { 0.0f, 0.0f, 0.0f };
    }

    static inline float Clamp(float value, float min, float max)
    {
        return (std::max)(min, (std::min)(value, max));
    }

    // 形状ペアごとの衝突判定ディスパッチ
    bool CheckCollision(const CollisionData& a, const CollisionData& b, Vector3& outPushDir, float& outPushLen)
    {
        ColliderType typeA = a.type;
        ColliderType typeB = b.type;

        // 1. 球 vs 球
        if (typeA == ColliderType::Sphere && typeB == ColliderType::Sphere)
        {
            return CheckSphereSphere(a, b, outPushDir, outPushLen);
        }
        // 2. 球 vs ボックス
        if (typeA == ColliderType::Sphere && typeB == ColliderType::Box)
        {
            return CheckSphereBox(a, b, outPushDir, outPushLen);
        }
        if (typeA == ColliderType::Box && typeB == ColliderType::Sphere)
        {
            bool hit = CheckSphereBox(b, a, outPushDir, outPushLen);
            outPushDir = outPushDir * -1.0f;
            return hit;
        }
        // 3. 球 vs カプセル
        if (typeA == ColliderType::Sphere && typeB == ColliderType::Capsule)
        {
            return CheckSphereCapsule(a, b, outPushDir, outPushLen);
        }
        if (typeA == ColliderType::Capsule && typeB == ColliderType::Sphere)
        {
            bool hit = CheckSphereCapsule(b, a, outPushDir, outPushLen);
            outPushDir = outPushDir * -1.0f;
            return hit;
        }
        // 4. ボックス vs ボックス (OBB)
        if (typeA == ColliderType::Box && typeB == ColliderType::Box)
        {
            return CheckBoxBox(a, b, outPushDir, outPushLen);
        }
        // 5. ボックス vs カプセル
        if (typeA == ColliderType::Box && typeB == ColliderType::Capsule)
        {
            return CheckBoxCapsule(a, b, outPushDir, outPushLen);
        }
        if (typeA == ColliderType::Capsule && typeB == ColliderType::Box)
        {
            bool hit = CheckBoxCapsule(b, a, outPushDir, outPushLen);
            outPushDir = outPushDir * -1.0f;
            return hit;
        }
        // 6. カプセル vs カプセル
        if (typeA == ColliderType::Capsule && typeB == ColliderType::Capsule)
        {
            return CheckCapsuleCapsule(a, b, outPushDir, outPushLen);
        }

        return false;
    }

    // 球 vs 球 の衝突判定
    bool CheckSphereSphere(const CollisionData& a, const CollisionData& b, Vector3& outPushDir, float& outPushLen)
    {
        Vector3 posA = a.worldPosition;
        Vector3 posB = b.worldPosition;

        Vector3 dir = posB - posA;
        float dist = Length(dir);

        Vector3 unitDir = (dist > 1e-4f) ? Normalize(dir) : Vector3{ 0.0f, 0.0f, 1.0f };

        auto getEllipsoidRadiusInDir = [](const CollisionData& col, const Vector3& d) -> float {
            Vector3 s = col.shape.scale;
            float rx = col.shape.radius * (s.x > 0.001f ? s.x : 1.0f);
            float ry = col.shape.radius * (s.y > 0.001f ? s.y : 1.0f);
            float rz = col.shape.radius * (s.z > 0.001f ? s.z : 1.0f);

            Vector3 rot = col.shape.rotation;
            float cosP = std::cos(-rot.x), sinP = std::sin(-rot.x);
            float cosY = std::cos(-rot.y), sinY = std::sin(-rot.y);
            float cosR = std::cos(-rot.z), sinR = std::sin(-rot.z);

            Vector3 ld = { d.x * cosR - d.y * sinR, d.x * sinR + d.y * cosR, d.z };
            ld = { ld.x, ld.y * cosP - ld.z * sinP, ld.y * sinP + ld.z * cosP };
            ld = { ld.x * cosY + ld.z * sinY, ld.y, -ld.x * sinY + ld.z * cosY };

            bool isSoftBody = (col.attribute == CollisionAttribute::Player || col.attribute == CollisionAttribute::Minion);
            if (isSoftBody && col.shape.radius > 0.001f)
            {
                Vector3 unitLd = Normalize(ld);
                Vector3 defCenter = SoftBodyDeformer::CalculateDeformedPosition(unitLd, unitLd, SoftBodyDeformer::GetGlobalTime());
                float maxR = Length(defCenter);

                Vector3 t1 = Normalize(Cross(unitLd, (std::abs(unitLd.y) < 0.9f ? Vector3{ 0.0f, 1.0f, 0.0f } : Vector3{ 1.0f, 0.0f, 0.0f })));
                Vector3 t2 = Cross(unitLd, t1);
                const float offsetAngle = 0.12f;

                for (int sIdx = 0; sIdx < 4; ++sIdx)
                {
                    float angle = sIdx * 1.5707963f;
                    Vector3 sampleDir = Normalize(unitLd + (t1 * std::cos(angle) + t2 * std::sin(angle)) * offsetAngle);
                    Vector3 defSample = SoftBodyDeformer::CalculateDeformedPosition(sampleDir, sampleDir, SoftBodyDeformer::GetGlobalTime());
                    float sampleR = Length(defSample);
                    if (sampleR > maxR) maxR = sampleR;
                }

                return col.shape.radius * maxR * 1.28f;
            }

            float effR = std::sqrt(ld.x * ld.x * rx * rx + ld.y * ld.y * ry * ry + ld.z * ld.z * rz * rz);
            return (effR > 0.001f) ? effR : col.shape.radius;
        };

        float radA = getEllipsoidRadiusInDir(a, unitDir);
        float radB = getEllipsoidRadiusInDir(b, unitDir * -1.0f);
        float minDist = radA + radB;

        if (dist < minDist)
        {
            outPushLen = minDist - dist;
            outPushDir = unitDir;
            return true;
        }
        return false;
    }

    // 球 vs ボックス の衝突判定
    bool CheckSphereBox(const CollisionData& sphere, const CollisionData& box, Vector3& outPushDir, float& outPushLen)
    {
        Vector3 sPos = sphere.worldPosition;
        Vector3 bPos = box.worldPosition;
        Vector3 size = box.shape.size;
        Vector3 extents = { size.x * 0.5f, size.y * 0.5f, size.z * 0.5f };

        Vector3 bRot = box.shape.rotation;
        Matrix4x4 R = Multiply(MakeRotateXMatrix(bRot.x), Multiply(MakeRotateYMatrix(bRot.y), MakeRotateZMatrix(bRot.z)));

        Vector3 axisX = { R.m[0][0], R.m[0][1], R.m[0][2] };
        Vector3 axisY = { R.m[1][0], R.m[1][1], R.m[1][2] };
        Vector3 axisZ = { R.m[2][0], R.m[2][1], R.m[2][2] };

        Vector3 offset = sPos - bPos;
        Vector3 localSphPos = { Dot(offset, axisX), Dot(offset, axisY), Dot(offset, axisZ) };

        Vector3 closestPointOnBox;
        closestPointOnBox.x = Clamp(localSphPos.x, -extents.x, extents.x);
        closestPointOnBox.y = Clamp(localSphPos.y, -extents.y, extents.y);
        closestPointOnBox.z = Clamp(localSphPos.z, -extents.z, extents.z);

        if (std::abs(localSphPos.x) <= extents.x &&
            std::abs(localSphPos.y) <= extents.y &&
            std::abs(localSphPos.z) <= extents.z)
        {
            float distL = extents.x + localSphPos.x; 
            float distR = extents.x - localSphPos.x; 
            float distB = extents.y + localSphPos.y; 
            float distT = extents.y - localSphPos.y; 
            float distF = extents.z + localSphPos.z; 
            float distN = extents.z - localSphPos.z; 

            float minDist = distL;
            Vector3 localPushDir = { 1.0f, 0.0f, 0.0f }; 

            if (distR < minDist) { minDist = distR; localPushDir = { -1.0f, 0.0f, 0.0f }; }
            if (distB < minDist) { minDist = distB; localPushDir = { 0.0f, 1.0f, 0.0f }; }
            if (distT < minDist) { minDist = distT; localPushDir = { 0.0f, -1.0f, 0.0f }; }
            if (distF < minDist) { minDist = distF; localPushDir = { 0.0f, 0.0f, 1.0f }; }
            if (distN < minDist) { minDist = distN; localPushDir = { 0.0f, 0.0f, -1.0f }; }

            outPushLen = sphere.shape.radius + minDist;
            outPushDir = axisX * localPushDir.x + axisY * localPushDir.y + axisZ * localPushDir.z;
            return true;
        }

        Vector3 localDir = closestPointOnBox - localSphPos;
        float dist = Length(localDir);

        if (dist < sphere.shape.radius)
        {
            outPushLen = sphere.shape.radius - dist;
            if (dist > 1e-4f)
            {
                Vector3 localPushDir = Normalize(localDir);
                outPushDir = axisX * localPushDir.x + axisY * localPushDir.y + axisZ * localPushDir.z;
            }
            else
            {
                outPushDir = axisZ * -1.0f;
            }
            return true;
        }

        return false;
    }

    // 球 vs カプセル の衝突判定
    bool CheckSphereCapsule(const CollisionData& sphere, const CollisionData& capsule, Vector3& outPushDir, float& outPushLen)
    {
        Vector3 sPos = sphere.worldPosition;
        Vector3 cPos = capsule.worldPosition;

        float halfH = capsule.shape.height * 0.5f;
        Vector3 segA = cPos - Vector3{ 0.0f, halfH, 0.0f };
        Vector3 segB = cPos + Vector3{ 0.0f, halfH, 0.0f };

        Vector3 ab = segB - segA;
        Vector3 as = sPos - segA;

        float abLenSq = Dot(ab, ab);
        float t = (abLenSq > 1e-5f) ? (Dot(as, ab) / abLenSq) : 0.0f;
        t = Clamp(t, 0.0f, 1.0f);
        
        Vector3 closestPointOnSegment = segA + ab * t;

        Vector3 dir = sPos - closestPointOnSegment;
        float dist = Length(dir);
        float minDist = sphere.shape.radius + capsule.shape.radius;

        if (dist < minDist)
        {
            outPushLen = minDist - dist;
            outPushDir = (dist > 1e-4f) ? Normalize(dir) : Vector3{ 0.0f, 0.0f, 1.0f };
            return true;
        }

        return false;
    }

    // ボックス vs ボックス（OBB）の衝突判定（分離軸定理 SAT）
    bool CheckBoxBox(const CollisionData& a, const CollisionData& b, Vector3& outPushDir, float& outPushLen)
    {
        Vector3 rotA = a.shape.rotation;
        Matrix4x4 RA = Multiply(MakeRotateXMatrix(rotA.x), Multiply(MakeRotateYMatrix(rotA.y), MakeRotateZMatrix(rotA.z)));
        Vector3 uA[3] = {
            { RA.m[0][0], RA.m[0][1], RA.m[0][2] },
            { RA.m[1][0], RA.m[1][1], RA.m[1][2] },
            { RA.m[2][0], RA.m[2][1], RA.m[2][2] }
        };

        Vector3 rotB = b.shape.rotation;
        Matrix4x4 RB = Multiply(MakeRotateXMatrix(rotB.x), Multiply(MakeRotateYMatrix(rotB.y), MakeRotateZMatrix(rotB.z)));
        Vector3 uB[3] = {
            { RB.m[0][0], RB.m[0][1], RB.m[0][2] },
            { RB.m[1][0], RB.m[1][1], RB.m[1][2] },
            { RB.m[2][0], RB.m[2][1], RB.m[2][2] }
        };

        Vector3 T = b.worldPosition - a.worldPosition;
        Vector3 hA = a.shape.size * 0.5f;
        Vector3 hB = b.shape.size * 0.5f;

        Vector3 axes[15];
        int axisCount = 0;

        for (int i = 0; i < 3; ++i) axes[axisCount++] = uA[i];
        for (int i = 0; i < 3; ++i) axes[axisCount++] = uB[i];

        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                Vector3 crossAxis = Cross(uA[i], uB[j]);
                if (LengthSq(crossAxis) > 1e-5f)
                {
                    axes[axisCount++] = Normalize(crossAxis);
                }
            }
        }

        float minOverlap = 1e30f;
        Vector3 bestAxis = { 0.0f, 0.0f, 0.0f };

        for (int i = 0; i < axisCount; ++i)
        {
            Vector3 L = axes[i];
            if (LengthSq(L) < 1e-5f) continue;
            L = Normalize(L);

            float rA = hA.x * std::abs(Dot(uA[0], L)) + hA.y * std::abs(Dot(uA[1], L)) + hA.z * std::abs(Dot(uA[2], L));
            float rB = hB.x * std::abs(Dot(uB[0], L)) + hB.y * std::abs(Dot(uB[1], L)) + hB.z * std::abs(Dot(uB[2], L));
            float distance = std::abs(Dot(T, L));
            float overlap = (rA + rB) - distance;

            if (overlap < 0.0f)
            {
                return false;
            }

            if (overlap < minOverlap)
            {
                minOverlap = overlap;
                bestAxis = L;
            }
        }

        outPushLen = minOverlap;
        outPushDir = (Dot(T, bestAxis) < 0.0f) ? (bestAxis * -1.0f) : bestAxis;
        return true;
    }

    // ボックス vs カプセル の衝突判定
    bool CheckBoxCapsule(const CollisionData& box, const CollisionData& capsule, Vector3& outPushDir, float& outPushLen)
    {
        Vector3 bPos = box.worldPosition;
        Vector3 rot = box.shape.rotation;
        Matrix4x4 R = Multiply(MakeRotateXMatrix(rot.x), Multiply(MakeRotateYMatrix(rot.y), MakeRotateZMatrix(rot.z)));
        Vector3 uA[3] = {
            { R.m[0][0], R.m[0][1], R.m[0][2] },
            { R.m[1][0], R.m[1][1], R.m[1][2] },
            { R.m[2][0], R.m[2][1], R.m[2][2] }
        };

        Vector3 extents = box.shape.size * 0.5f;

        float halfH = capsule.shape.height * 0.5f;
        Vector3 P0 = capsule.worldPosition - Vector3{ 0.0f, halfH, 0.0f };
        Vector3 P1 = capsule.worldPosition + Vector3{ 0.0f, halfH, 0.0f };

        Vector3 offset0 = P0 - bPos;
        Vector3 localP0 = { Dot(offset0, uA[0]), Dot(offset0, uA[1]), Dot(offset0, uA[2]) };
        Vector3 offset1 = P1 - bPos;
        Vector3 localP1 = { Dot(offset1, uA[0]), Dot(offset1, uA[1]), Dot(offset1, uA[2]) };

        std::vector<float> tCandidates;
        tCandidates.push_back(0.0f);
        tCandidates.push_back(1.0f);

        Vector3 segmentDir = localP1 - localP0;

        auto checkPlaneIntersection = [&](float value, float p0Val, float dirVal) {
            if (std::abs(dirVal) > 1e-5f)
            {
                float t = (value - p0Val) / dirVal;
                if (t >= 0.0f && t <= 1.0f)
                {
                    tCandidates.push_back(t);
                }
            }
        };

        checkPlaneIntersection(extents.x, localP0.x, segmentDir.x);
        checkPlaneIntersection(-extents.x, localP0.x, segmentDir.x);
        checkPlaneIntersection(extents.y, localP0.y, segmentDir.y);
        checkPlaneIntersection(-extents.y, localP0.y, segmentDir.y);
        checkPlaneIntersection(extents.z, localP0.z, segmentDir.z);
        checkPlaneIntersection(-extents.z, localP0.z, segmentDir.z);

        float bestT = 0.0f;
        float minSqDist = 1e30f;

        for (float t : tCandidates)
        {
            Vector3 pt = localP0 + segmentDir * t;
            Vector3 closest = {
                Clamp(pt.x, -extents.x, extents.x),
                Clamp(pt.y, -extents.y, extents.y),
                Clamp(pt.z, -extents.z, extents.z)
            };
            float sqDist = LengthSq(pt - closest);
            if (sqDist < minSqDist)
            {
                minSqDist = sqDist;
                bestT = t;
            }
        }

        Vector3 Q = P0 + (P1 - P0) * bestT;

        CollisionData sphereData;
        sphereData.originalCollider = capsule.originalCollider;
        sphereData.type = ColliderType::Sphere;
        sphereData.attribute = capsule.attribute;
        sphereData.worldPosition = Q;
        sphereData.shape.radius = capsule.shape.radius;

        return CheckSphereBox(sphereData, box, outPushDir, outPushLen);
    }

    // カプセル vs カプセル の衝突判定
    bool CheckCapsuleCapsule(const CollisionData& a, const CollisionData& b, Vector3& outPushDir, float& outPushLen)
    {
        float halfHA = a.shape.height * 0.5f;
        Vector3 P0 = a.worldPosition - Vector3{ 0.0f, halfHA, 0.0f };
        Vector3 P1 = a.worldPosition + Vector3{ 0.0f, halfHA, 0.0f };

        float halfHB = b.shape.height * 0.5f;
        Vector3 Q0 = b.worldPosition - Vector3{ 0.0f, halfHB, 0.0f };
        Vector3 Q1 = b.worldPosition + Vector3{ 0.0f, halfHB, 0.0f };

        Vector3 u = P1 - P0;
        Vector3 v = Q1 - Q0;
        Vector3 w = P0 - Q0;
        float a_val = Dot(u, u);
        float b_val = Dot(u, v);
        float c_val = Dot(v, v);
        float d_val = Dot(u, w);
        float e_val = Dot(v, w);
        float D = a_val * c_val - b_val * b_val;
        float sc, sN, sD = D;
        float tc, tN, tD = D;

        if (D < 1e-5f)
        {
            sN = 0.0f;
            sD = 1.0f;
            tN = e_val;
            tD = c_val;
        }
        else
        {
            sN = (b_val * e_val - c_val * d_val);
            tN = (a_val * e_val - b_val * d_val);
            if (sN < 0.0f)
            {
                sN = 0.0f;
                tN = e_val;
                tD = c_val;
            }
            else if (sN > sD)
            {
                sN = sD;
                tN = e_val + b_val;
                tD = c_val;
            }
        }

        if (tN < 0.0f)
        {
            tN = 0.0f;
            if (-d_val < 0.0f)
                sN = 0.0f;
            else if (-d_val > a_val)
                sN = sD;
            else {
                sN = -d_val;
                sD = a_val;
            }
        }
        else if (tN > tD)
        {
            tN = tD;
            if ((-d_val + b_val) < 0.0f)
                sN = 0.0f;
            else if ((-d_val + b_val) > a_val)
                sN = sD;
            else {
                sN = (-d_val + b_val);
                sD = a_val;
            }
        }

        sc = (std::abs(sN) < 1e-5f ? 0.0f : sN / sD);
        tc = (std::abs(tN) < 1e-5f ? 0.0f : tN / tD);

        Vector3 dP = w + (u * sc) - (v * tc);
        float dist = Length(dP);
        float minDist = a.shape.radius + b.shape.radius;

        if (dist < minDist)
        {
            outPushLen = minDist - dist;
            outPushDir = (dist > 1e-4f) ? Normalize(dP) : Vector3{ 0.0f, 0.0f, 1.0f };
            return true;
        }
        return false;
    }
}
