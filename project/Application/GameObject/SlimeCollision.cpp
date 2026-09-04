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

        float sxz = 0.5f * (std::abs(scale.x) + std::abs(scale.z)) * baseRadius;
        float sy  = std::abs(scale.y) * baseRadius;
        if (sxz < 0.001f) sxz = 1.0f;
        if (sy < 0.001f) sy = 1.0f;

        // Slime.VS.hlsl の体積保存スクワッシュ (1 / sqrt(1 + squash.y)) に完全同期
        // 基準潰れ量 squash.y = -0.16f に対する動的な横幅拡大倍率
        float baseVolXZ = 1.0f / std::sqrt(1.0f - 0.16f);
        float currentVolXZ = 1.0f / std::sqrt((std::max)(0.2f, 1.0f + squashStretch.y));
        float dynSquashFactor = currentVolXZ / baseVolXZ;
        sxz *= dynSquashFactor;

        float flowX = squashStretch.x;
        float flowZ = squashStretch.z;
        float flowMag = std::sqrt(flowX * flowX + flowZ * flowZ);

        Vector3 fDirLocal = { 0.0f, 0.0f, 1.0f };
        Vector3 sideDirLocal = { 1.0f, 0.0f, 0.0f };
        if (flowMag > 0.001f)
        {
            fDirLocal.x = flowX / flowMag;
            fDirLocal.z = flowZ / flowMag;
            sideDirLocal.x = -fDirLocal.z;
            sideDirLocal.z = fDirLocal.x;
        }
        shape.flowDirLocal = fDirLocal;

        Matrix4x4 rotMat = BuildRotationMatrix(rotation);

        // --- Slime.VS.hlsl の頂点シェーダー変形に 100% 幾何一致する7連球 ---
        // 静止時の外周限界: xFront = +1.73, xRear = -1.73, zMax = 1.73（完全対称円形）
        // 傾斜流動時の外周限界: xFront = 1.73 + 2.50 * flowMag, xRear = -(1.73 + 0.12 * flowMag), zMax = 1.73 + 0.12 * flowMag
        float xFront = 1.73f + 2.50f * flowMag;
        float xRear  = -(1.73f + 0.12f * flowMag);
        float zMax   = 1.73f + 0.12f * flowMag;

        // [0] 前方下り坂舌先（Tongue Tip）: 最先端 xFront に外周が厳密一致
        float r0     = (1.10f + flowMag * 0.20f) * sxz;
        float distF0 = (xFront - (1.10f + flowMag * 0.20f)) * sxz;
        float distS0 = 0.0f;
        float distY0 = (-0.12f + squashStretch.y * 0.12f) * sy;

        // [5] 後方上り坂基底（Rear Heel）: 最後尾 xRear に外周が厳密一致
        float r5     = (1.10f + flowMag * 0.05f) * sxz;
        float distF5 = (xRear + (1.10f + flowMag * 0.05f)) * sxz;
        float distS5 = 0.0f;
        float distY5 = (-0.12f + squashStretch.y * 0.12f) * sy;

        // [2] 中央体積中核（Central Core）: 体積中心
        float r2     = (1.55f + flowMag * 0.15f) * sxz;
        float distF2 = (flowMag * 0.78f) * sxz;
        float distS2 = 0.0f;
        float distY2 = (-0.10f + squashStretch.y * 0.10f) * sy;

        // [1] 前方水溜まり基部（Tongue Mid）: 先端(0)と中央コア(2)の中間を滑らかに補間
        float r1     = (1.30f + flowMag * 0.15f) * sxz;
        float distF1 = (distF0 + distF2) * 0.5f;
        float distS1 = 0.0f;
        float distY1 = (-0.12f + squashStretch.y * 0.12f) * sy;

        // [3] 左側面お腹（Left Flank）: 最大横幅 +zMax に外周が厳密一致
        float r3     = (1.15f + flowMag * 0.07f) * sxz;
        float distF3 = distF2;
        float distS3 = (zMax - (1.15f + flowMag * 0.07f)) * sxz;
        float distY3 = (-0.10f + squashStretch.y * 0.10f) * sy;

        // [4] 右側面お腹（Right Flank）: 最大横幅 -zMax に外周が厳密一致
        float r4     = r3;
        float distF4 = distF2;
        float distS4 = -distS3;
        float distY4 = (-0.10f + squashStretch.y * 0.10f) * sy;

        // [6] 頭部ドーム頂点（Upper Head Dome）: スライム上部の丸い山型
        float r6     = (0.95f - flowMag * 0.10f) * sxz;
        float distF6 = distF2 * 0.5f;
        float distS6 = 0.0f;
        float distY6 = (+0.18f + squashStretch.y * 0.10f) * sy;

        const float distF[SlimeMultiSphereShape::kSubSphereCount] = { distF0, distF1, distF2, distF3, distF4, distF5, distF6 };
        const float distS[SlimeMultiSphereShape::kSubSphereCount] = { distS0, distS1, distS2, distS3, distS4, distS5, distS6 };
        const float distY[SlimeMultiSphereShape::kSubSphereCount] = { distY0, distY1, distY2, distY3, distY4, distY5, distY6 };
        const float rad[SlimeMultiSphereShape::kSubSphereCount]   = { r0, r1, r2, r3, r4, r5, r6 };

        float maxReach = 0.0f;
        for (size_t i = 0; i < SlimeMultiSphereShape::kSubSphereCount; ++i)
        {
            Vector3 localOffset = {
                fDirLocal.x * distF[i] + sideDirLocal.x * distS[i],
                distY[i],
                fDirLocal.z * distF[i] + sideDirLocal.z * distS[i]
            };
            shape.spheres[i].worldPos = centerPos + RotateVector(localOffset, rotMat);
            shape.spheres[i].radius   = rad[i];

            Vector3 delta = shape.spheres[i].worldPos - centerPos;
            float d = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
            maxReach = (std::max)(maxReach, d + shape.spheres[i].radius);
        }

        shape.maxBoundingRadius = maxReach * 1.05f;
        return shape;
    }

    bool CheckCollision(const SlimeMultiSphereShape& shapeA, const SlimeMultiSphereShape& shapeB,
                        Vector3& outPushDir, float& outPushLen)
    {
        // 1. ブロードフェーズ（中心間距離による超高速アーリーアウト）
        Vector3 diff = shapeB.center - shapeA.center;
        float distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
        float maxCombinedRadius = shapeA.maxBoundingRadius + shapeB.maxBoundingRadius;

        if (distSq > maxCombinedRadius * maxCombinedRadius)
        {
            return false;
        }

        // 2. ナローフェーズ（7球 × 7球 = 49ペアの精密交差探索）
        float maxPenetration = 0.0f;
        Vector3 bestPushDir = { 0.0f, 0.0f, 0.0f };

        for (size_t i = 0; i < SlimeMultiSphereShape::kSubSphereCount; ++i)
        {
            const auto& sA = shapeA.spheres[i];
            for (size_t j = 0; j < SlimeMultiSphereShape::kSubSphereCount; ++j)
            {
                const auto& sB = shapeB.spheres[j];

                Vector3 delta = sB.worldPos - sA.worldPos;
                float dSq = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
                float rSum = sA.radius + sB.radius;

                if (dSq < rSum * rSum)
                {
                    float d = std::sqrt(dSq);
                    float penetration = rSum - d;

                    if (penetration > maxPenetration)
                    {
                        maxPenetration = penetration;
                        if (d > 1e-4f)
                        {
                            bestPushDir = delta * (1.0f / d);
                        }
                        else
                        {
                            Vector3 hDiff = { diff.x, 0.0f, diff.z };
                            float hLen = std::sqrt(hDiff.x * hDiff.x + hDiff.z * hDiff.z);
                            if (hLen > 1e-4f)
                            {
                                bestPushDir = hDiff * (1.0f / hLen);
                            }
                            else
                            {
                                bestPushDir = { 0.0f, 1.0f, 0.0f };
                            }
                        }
                    }
                }
            }
        }

        if (maxPenetration > 0.0f)
        {
            outPushLen = maxPenetration;
            outPushDir = bestPushDir;
            return true;
        }

        return false;
    }

    bool ResolveCollision(Vector3& posA, const Vector3& scaleA, const Vector3& squashA, float weightA,
                          Vector3& posB, const Vector3& scaleB, const Vector3& squashB, float weightB,
                          float& outImpulse,
                          const Vector3& rotA,
                          const Vector3& rotB,
                          const Vector3& planeNormal,
                          float baseRadiusA, float baseRadiusB)
    {
        SlimeMultiSphereShape shapeA = BuildMultiSphere(posA, scaleA, squashA, rotA, baseRadiusA);
        SlimeMultiSphereShape shapeB = BuildMultiSphere(posB, scaleB, squashB, rotB, baseRadiusB);

        Vector3 pushDir;
        float pushLen = 0.0f;

        if (CheckCollision(shapeA, shapeB, pushDir, pushLen))
        {
            // 傾斜面に沿った押し出しベクトル（法線成分を除去して面内分離を完全保証）
            float normalDot = pushDir.x * planeNormal.x + pushDir.y * planeNormal.y + pushDir.z * planeNormal.z;
            Vector3 planeDir = pushDir - planeNormal * normalDot;
            float planeLenSq = planeDir.x * planeDir.x + planeDir.y * planeDir.y + planeDir.z * planeDir.z;

            if (planeLenSq > 1e-6f)
            {
                pushDir = planeDir * (1.0f / std::sqrt(planeLenSq));
            }

            posA = posA - pushDir * (pushLen * weightA);
            posB = posB + pushDir * (pushLen * weightB);

            outImpulse = (std::min)(0.35f, pushLen * 1.8f);
            return true;
        }

        outImpulse = 0.0f;
        return false;
    }

    void DrawDebugMultiSphere(const SlimeMultiSphereShape& shape, Camera* camera, uint32_t color)
    {
#ifdef _DEBUG
        if (!camera) return;

        ImGuiIO& io = ImGui::GetIO();
        float width = io.DisplaySize.x;
        float height = io.DisplaySize.y;
        if (width <= 0.0f || height <= 0.0f) return;

        const Matrix4x4& vp = camera->GetViewProjectionMatrix();
        ImDrawList* drawList = ImGui::GetForegroundDrawList();

        auto project3DTo2D = [&](const Vector3& pos3D, ImVec2& outPos) -> bool {
            float w = pos3D.x * vp.m[0][3] + pos3D.y * vp.m[1][3] + pos3D.z * vp.m[2][3] + vp.m[3][3];
            if (w <= 0.0f) return false;
            float x = (pos3D.x * vp.m[0][0] + pos3D.y * vp.m[1][0] + pos3D.z * vp.m[2][0] + vp.m[3][0]) / w;
            float y = (pos3D.x * vp.m[0][1] + pos3D.y * vp.m[1][1] + pos3D.z * vp.m[2][1] + vp.m[3][1]) / w;
            outPos.x = (x + 1.0f) * 0.5f * width;
            outPos.y = (1.0f - y) * 0.5f * height;
            return true;
        };

        Matrix4x4 rotMat = BuildRotationMatrix(shape.rotation);
        Vector3 boardX = { rotMat.m[0][0], rotMat.m[0][1], rotMat.m[0][2] }; // 床面X軸
        Vector3 boardY = { rotMat.m[1][0], rotMat.m[1][1], rotMat.m[1][2] }; // 床面法線Y軸
        Vector3 boardZ = { rotMat.m[2][0], rotMat.m[2][1], rotMat.m[2][2] }; // 床面Z軸
        Vector3 flowDirWorld = RotateVector(shape.flowDirLocal, rotMat);

        constexpr int kSegments = 28;
        constexpr float kAngleStep = 6.2831853f / static_cast<float>(kSegments);

        for (size_t sIdx = 0; sIdx < SlimeMultiSphereShape::kSubSphereCount; ++sIdx)
        {
            const auto& sph = shape.spheres[sIdx];

            // 1. 床面に沿った円周リング (boardX - boardZ 平面: スライムの接地外周に100%一致)
            ImVec2 xzPoints[kSegments + 1];
            bool allInsideXZ = true;
            for (int i = 0; i <= kSegments; ++i)
            {
                float angle = i * kAngleStep;
                Vector3 pt = sph.worldPos + (boardX * std::cos(angle) + boardZ * std::sin(angle)) * sph.radius;
                if (!project3DTo2D(pt, xzPoints[i]))
                {
                    allInsideXZ = false;
                    break;
                }
            }
            if (allInsideXZ)
            {
                drawList->AddPolyline(xzPoints, kSegments + 1, color, ImDrawFlags_None, 2.0f);
            }

            // 2. 流動・進行方向断面リング (flowDirWorld - boardY 平面: スライムの偏平な厚みに合わせた縦楕円)
            float radiusY = (std::max)(0.06f, sph.radius * 0.38f);
            ImVec2 zyPoints[kSegments + 1];
            bool allInsideZY = true;
            for (int i = 0; i <= kSegments; ++i)
            {
                float angle = i * kAngleStep;
                Vector3 pt = sph.worldPos + flowDirWorld * (std::cos(angle) * sph.radius) + boardY * (std::sin(angle) * radiusY);
                if (!project3DTo2D(pt, zyPoints[i]))
                {
                    allInsideZY = false;
                    break;
                }
            }
            if (allInsideZY)
            {
                drawList->AddPolyline(zyPoints, kSegments + 1, color, ImDrawFlags_None, 1.4f);
            }
        }

        // 3. 部分球同士を滑らかに繋ぐ外周ハル接続ライン（カプセル状シルエットの可視化）
        auto drawTangentLine = [&](const SlimeSubSphere& s1, const SlimeSubSphere& s2) {
            Vector3 diff = s2.worldPos - s1.worldPos;
            Vector3 perp = {
                diff.y * boardY.z - diff.z * boardY.y,
                diff.z * boardY.x - diff.x * boardY.z,
                diff.x * boardY.y - diff.y * boardY.x
            };
            float perpLen = std::sqrt(perp.x * perp.x + perp.y * perp.y + perp.z * perp.z);
            if (perpLen > 0.001f)
            {
                perp.x /= perpLen;
                perp.y /= perpLen;
                perp.z /= perpLen;

                Vector3 p1Left  = s1.worldPos - perp * s1.radius;
                Vector3 p2Left  = s2.worldPos - perp * s2.radius;
                Vector3 p1Right = s1.worldPos + perp * s1.radius;
                Vector3 p2Right = s2.worldPos + perp * s2.radius;

                ImVec2 ptA, ptB;
                if (project3DTo2D(p1Left, ptA) && project3DTo2D(p2Left, ptB)) {
                    drawList->AddLine(ptA, ptB, color, 1.6f);
                }
                if (project3DTo2D(p1Right, ptA) && project3DTo2D(p2Right, ptB)) {
                    drawList->AddLine(ptA, ptB, color, 1.6f);
                }
            }
        };

        // 主軸ライン: 舌先(0) - 前方中間(1) - 中央コア(2) - 後方基底(5)
        drawTangentLine(shape.spheres[0], shape.spheres[1]);
        drawTangentLine(shape.spheres[1], shape.spheres[2]);
        drawTangentLine(shape.spheres[2], shape.spheres[5]);

        // 中央コア(2) - 左右の脇腹(3, 4)
        drawTangentLine(shape.spheres[3], shape.spheres[2]);
        drawTangentLine(shape.spheres[2], shape.spheres[4]);

        // 外周輪郭ライン: 舌先(0)・後方基底(5) と 左右脇腹(3, 4)
        drawTangentLine(shape.spheres[0], shape.spheres[3]);
        drawTangentLine(shape.spheres[0], shape.spheres[4]);
        drawTangentLine(shape.spheres[5], shape.spheres[3]);
        drawTangentLine(shape.spheres[5], shape.spheres[4]);

        // 頭部ドーム(6) - 中央コア(2)
        drawTangentLine(shape.spheres[6], shape.spheres[2]);
#endif
    }
}
