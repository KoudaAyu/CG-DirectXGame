#define NOMINMAX
#include "SlimeCollision.h"
#include "Baziru3_Engine/Core/Camera/Camera.h"
#include "Matrix4x4.h"
#include <imgui.h>
#include <cmath>
#include <algorithm>

namespace SlimeCollision
{
    namespace
    {
        inline Vector3 RotateVector(const Vector3& v, const Matrix4x4& m)
        {
            return {
                v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0],
                v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1],
                v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2]
            };
        }

        inline Matrix4x4 BuildRotationMatrix(const Vector3& rot)
        {
            return Multiply(MakeRotateXMatrix(rot.x),
                            Multiply(MakeRotateYMatrix(rot.y), MakeRotateZMatrix(rot.z)));
        }

        inline float CalculateEffectiveRadius(const Vector3& scale, const Vector3& squashStretch, float baseRadius)
        {
            float s = (std::abs(scale.x) + std::abs(scale.z)) * 0.5f * baseRadius;
            if (s < 0.001f) s = 0.4f;

            // スライムモデルの実際の外形メッシュ比率 (0.78f)
            float meshRadius = s * 0.78f;

            // スクワッシュ変形による横幅の微小変化を反映
            float squashFactor = 1.0f / std::sqrt((std::max)(0.25f, 1.0f + squashStretch.y));
            float dynFactor = std::clamp(squashFactor, 0.85f, 1.35f);

            return meshRadius * dynFactor;
        }
    }

    SlimeMultiSphereShape BuildMultiSphere(const Vector3& centerPos,
                                           const Vector3& scale,
                                           const Vector3& squashStretch,
                                           const Vector3& rotation,
                                           float baseRadius)
    {
        SlimeMultiSphereShape shape;
        shape.center = centerPos;
        shape.rotation = rotation;

        float r = CalculateEffectiveRadius(scale, squashStretch, baseRadius);
        shape.maxBoundingRadius = r;

        // メッシュ境界に厳密一致する代表球を格納
        for (size_t i = 0; i < SlimeMultiSphereShape::kSubSphereCount; ++i)
        {
            shape.spheres[i].worldPos = centerPos;
            shape.spheres[i].radius = r;
        }

        return shape;
    }

    bool CheckCollision(const SlimeMultiSphereShape& shapeA, const SlimeMultiSphereShape& shapeB,
                        Vector3& outPushDir, float& outPushLen)
    {
        Vector3 diff = shapeB.center - shapeA.center;
        float distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
        float rSum = shapeA.maxBoundingRadius + shapeB.maxBoundingRadius;

        if (distSq >= rSum * rSum) return false;

        float dist = std::sqrt(distSq);
        if (dist < 1e-4f)
        {
            outPushDir = { 0.0f, 0.0f, 1.0f };
            outPushLen = rSum;
            return true;
        }

        outPushDir = diff * (1.0f / dist);
        outPushLen = rSum - dist;
        return true;
    }

    bool ResolveCollision(Vector3& posA, const Vector3& scaleA, const Vector3& squashA, float weightA,
                          Vector3& posB, const Vector3& scaleB, const Vector3& squashB, float weightB,
                          float& outImpulse,
                          const Vector3& rotA, const Vector3& rotB,
                          const Vector3& planeNormal,
                          float baseRadiusA, float baseRadiusB)
    {
        float rA = CalculateEffectiveRadius(scaleA, squashA, baseRadiusA);
        float rB = CalculateEffectiveRadius(scaleB, squashB, baseRadiusB);

        // 床面法線に沿った無駄な浮き沈みを排除し、面内（XZまたは傾斜接平面）で分離
        Vector3 diff = posB - posA;

        // 法線方向のオフセット成分を除去して同一平面内の相対ベクトルにする
        float normDot = diff.x * planeNormal.x + diff.y * planeNormal.y + diff.z * planeNormal.z;
        Vector3 planarDiff = {
            diff.x - planeNormal.x * normDot,
            diff.y - planeNormal.y * normDot,
            diff.z - planeNormal.z * normDot
        };

        float distSq = planarDiff.x * planarDiff.x + planarDiff.y * planarDiff.y + planarDiff.z * planarDiff.z;
        float rSum = rA + rB;

        if (distSq >= rSum * rSum)
        {
            return false;
        }

        float dist = std::sqrt(distSq);
        Vector3 pushDir{ 0.0f, 0.0f, 1.0f };
        if (dist > 1e-4f)
        {
            pushDir = planarDiff * (1.0f / dist);
        }
        else
        {
            // 完全重合時はランダムまたはローカルZ方向へ分離
            pushDir = { 1.0f, 0.0f, 0.0f };
        }

        float penetration = rSum - dist;

        // 重み比率に応じた押し出しの適用
        posA.x -= pushDir.x * (penetration * weightA);
        posA.y -= pushDir.y * (penetration * weightA);
        posA.z -= pushDir.z * (penetration * weightA);

        posB.x += pushDir.x * (penetration * weightB);
        posB.y += pushDir.y * (penetration * weightB);
        posB.z += pushDir.z * (penetration * weightB);

        outImpulse = std::clamp(penetration * 2.5f, 0.05f, 1.0f);

        return true;
    }

    void DrawDebugMultiSphere(const SlimeMultiSphereShape& shape, Camera* camera, uint32_t color)
    {
        // エンジン側の MeshCollider ワイヤーフレーム描画が自動的に行われるため、
        // 重複描画を防ぎクリーンな表示を維持します。
    }
}
