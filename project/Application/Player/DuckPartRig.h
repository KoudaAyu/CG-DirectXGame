#pragma once
#include "Object3d.h"
#include "Matrix4x4.h"
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <numbers>

namespace DuckPartRig
{
    constexpr float kPi    = std::numbers::pi_v<float>;
    constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;

    // player.obj 内の各関節ローカルピボット座標
    inline constexpr Vector3 kLeftFootPivot  = { -0.275f, 0.0f, 0.08f };
    inline constexpr Vector3 kRightFootPivot = {  0.275f, 0.0f, 0.08f };
    inline constexpr Vector3 kLeftWingPivot  = { -0.500f, 0.78f, 0.12f };
    inline constexpr Vector3 kRightWingPivot = {  0.500f, 0.78f, 0.12f };

    // プロシージャル直方体メッシュ生成・部位分割・接地足IK定数
    struct BoxBounds { Vector3 minP; Vector3 maxP; };

    inline constexpr int      kBoxCornerCount = 8, kBoxFaceCount = 6, kBoxFaceVertCount = 4;
    inline constexpr size_t   kTriVertexCount = 3;
    inline constexpr float    kTriCentroidDivisor = 3.0f, kAxisCountFloat = 3.0f;
    inline constexpr float    kDuckPartBoundingRadius = 4.0f, kGimbalLockCosThreshold = 0.9999f;

    inline constexpr Vector2  kDefaultFootUV = { 0.375f, 0.875f }, kWingTargetUV = { 0.125f, 0.625f };
    inline constexpr float    kFootMaxCentroidY = 0.18f, kWingUVTolerance = 0.03f, kWingMinAbsCentroidX = 0.38f;
    inline constexpr Vector3  kFootStemMin = { -0.052f, 0.055f, -0.055f }, kFootStemMax = { 0.052f, 0.360f, 0.055f };

    inline constexpr float    kFootLiftCurveExp = 0.82f, kFootLateralStrideRate = 0.72f, kFootStanceOutward = 0.024f;
    inline constexpr float    kFootBackpedalThresh = -0.15f, kFootStridePitchScale = -0.44f, kFootSwingPitchLift = 0.14f;
    inline constexpr float    kFootIdleToeOutYaw = 0.04f, kFootWalkToeOutYaw = 0.05f;
    inline constexpr float    kFootStrafeYawScale = 0.18f, kFootStrafeRollScale = 0.12f;

    inline constexpr Vector3  kBoxNormals[kBoxFaceCount] = {
        { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f },
        { 1.0f, 0.0f,  0.0f }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }
    };
    inline constexpr int      kBoxFaces[kBoxFaceCount][kBoxFaceVertCount] = {
        { 0, 3, 2, 1 }, { 4, 5, 6, 7 }, { 0, 4, 7, 3 }, { 1, 2, 6, 5 }, { 0, 1, 5, 4 }, { 3, 7, 6, 2 }
    };
    inline constexpr Vector2  kQuadFaceUVs[kBoxFaceVertCount] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
    inline constexpr uint32_t kQuadTriIndices[kBoxFaceCount]  = { 0u, 1u, 2u, 0u, 2u, 3u };

    struct SplitMeshSet
    {
        Object3d::ModelData body, leftFoot, rightFoot, leftWing, rightWing;
    };

    inline void AppendBox(Object3d::ModelData& dst, const Vector3& minP, const Vector3& maxP, const Vector2* overrideUV = nullptr)
    {
        const Vector3 corners[kBoxCornerCount] = {
            { minP.x, minP.y, minP.z }, { maxP.x, minP.y, minP.z }, { maxP.x, maxP.y, minP.z }, { minP.x, maxP.y, minP.z },
            { minP.x, minP.y, maxP.z }, { maxP.x, minP.y, maxP.z }, { maxP.x, maxP.y, maxP.z }, { minP.x, maxP.y, maxP.z },
        };
        for (int f = 0; f < kBoxFaceCount; ++f)
        {
            const uint32_t base = static_cast<uint32_t>(dst.vertices.size());
            for (int v = 0; v < kBoxFaceVertCount; ++v)
            {
                const Vector3& c = corners[kBoxFaces[f][v]];
                Sprite::VertexData vert{};
                vert.position = { c.x, c.y, c.z, 1.0f };
                vert.texcoord = overrideUV ? *overrideUV : kQuadFaceUVs[v];
                vert.normal   = kBoxNormals[f];
                dst.vertices.push_back(vert);
            }
            for (uint32_t idx : kQuadTriIndices) dst.indices.push_back(base + idx);
        }
    }

    inline void AppendPivotBox(Object3d::ModelData& dst, const Vector3& minP, const Vector3& maxP, const Vector2& uv)
    {
        AppendBox(dst, minP, maxP, &uv);
    }

    inline void PushTriangleRecentered(
        Object3d::ModelData& dst,
        const Sprite::VertexData& v0, const Sprite::VertexData& v1, const Sprite::VertexData& v2,
        const Vector3& pivot)
    {
        const uint32_t base = static_cast<uint32_t>(dst.vertices.size());
        Sprite::VertexData verts[kTriVertexCount] = { v0, v1, v2 };
        for (size_t i = 0; i < kTriVertexCount; ++i)
        {
            verts[i].position.x -= pivot.x;
            verts[i].position.y -= pivot.y;
            verts[i].position.z -= pivot.z;
            dst.vertices.push_back(verts[i]);
            dst.indices.push_back(base + static_cast<uint32_t>(i));
        }
    }

    inline const SplitMeshSet& GetCachedDuckSplitMeshes()
    {
        static SplitMeshSet sCache{};
        static bool sBuilt = false;
        if (sBuilt) return sCache;

        const Object3d::ModelData full = Object3d::LoadObjFile("Resources", "player.obj");
        Object3d::ModelData* allParts[] = { &sCache.body, &sCache.leftFoot, &sCache.rightFoot, &sCache.leftWing, &sCache.rightWing };
        for (Object3d::ModelData* part : allParts) part->material = full.material;

        Vector2 footUV = kDefaultFootUV;
        bool foundFootUV = false;
        const size_t triCount = full.indices.size() / kTriVertexCount;
        for (size_t t = 0; t < triCount; ++t)
        {
            const uint32_t i0 = full.indices[t * kTriVertexCount + 0];
            const uint32_t i1 = full.indices[t * kTriVertexCount + 1];
            const uint32_t i2 = full.indices[t * kTriVertexCount + 2];
            if (i0 >= full.vertices.size() || i1 >= full.vertices.size() || i2 >= full.vertices.size()) continue;

            const Sprite::VertexData& v0 = full.vertices[i0];
            const Sprite::VertexData& v1 = full.vertices[i1];
            const Sprite::VertexData& v2 = full.vertices[i2];
            const float cx = (v0.position.x + v1.position.x + v2.position.x) / kTriCentroidDivisor;
            const float cy = (v0.position.y + v1.position.y + v2.position.y) / kTriCentroidDivisor;

            // 水かき足メッシュ (Y < 0.18m: 胴体最下端 0.243m と完全に分離)
            if (cy < kFootMaxCentroidY)
            {
                if (!foundFootUV) { footUV = v0.texcoord; foundFootUV = true; }
                PushTriangleRecentered((cx < 0.0f) ? sCache.leftFoot : sCache.rightFoot, v0, v1, v2, (cx < 0.0f) ? kLeftFootPivot : kRightFootPivot);
                continue;
            }

            // 左右の翼メッシュ (|X| > 0.38m かつ Wing専用ダークイエローUVセル)
            const bool isWingUV = (std::abs(v0.texcoord.x - kWingTargetUV.x) < kWingUVTolerance) && (std::abs(v0.texcoord.y - kWingTargetUV.y) < kWingUVTolerance);
            if (std::abs(cx) > kWingMinAbsCentroidX && isWingUV)
            {
                PushTriangleRecentered((cx < 0.0f) ? sCache.leftWing : sCache.rightWing, v0, v1, v2, (cx < 0.0f) ? kLeftWingPivot : kRightWingPivot);
                continue;
            }

            // 胴体・頭部・くちばし・目・チーク・頭頂羽毛
            PushTriangleRecentered(sCache.body, v0, v1, v2, { 0.0f, 0.0f, 0.0f });
        }

        // ホップ・前傾時にもお腹と水かき足が自然に繋がる短い脚ステムを追加
        AppendPivotBox(sCache.leftFoot,  kFootStemMin, kFootStemMax, footUV);
        AppendPivotBox(sCache.rightFoot, kFootStemMin, kFootStemMax, footUV);
        if (sCache.body.vertices.empty()) sCache.body = full;
        for (Object3d::ModelData* part : allParts) part->boundingRadius = kDuckPartBoundingRadius;

        sBuilt = true;
        return sCache;
    }

    // 任意の3x3回転行列 R を Object3d 標準の Rx(x) * Ry(y) * Rz(z) オイラー角へ厳密分解
    inline Vector3 DecomposeRotationToEulerXYZ(const Matrix4x4& R)
    {
        const float rotY = std::asin((std::clamp)(-R.m[0][2], -1.0f, 1.0f));
        if (std::abs(R.m[0][2]) < kGimbalLockCosThreshold) return { std::atan2(R.m[1][2], R.m[2][2]), rotY, std::atan2(R.m[0][1], R.m[0][0]) };
        return { std::atan2(-R.m[2][1], R.m[1][1]), rotY, 0.0f };
    }

    inline Matrix4x4 MakeEulerMatrixXYZ(const Vector3& euler)
    {
        return Multiply(MakeRotateXMatrix(euler.x), Multiply(MakeRotateYMatrix(euler.y), MakeRotateZMatrix(euler.z)));
    }

    // アヒル自身のローカル座標系でピッチ・ロールを適用してからワールドYawへ向ける回転行列
    inline Matrix4x4 MakeLocalPitchRollThenYawMatrix(float localPitchX, float localRollZ, float worldYawY)
    {
        return Multiply(Multiply(MakeRotateZMatrix(localRollZ), MakeRotateXMatrix(localPitchX)), MakeRotateYMatrix(worldYawY));
    }

    inline Vector3 TransformVec3ByRotation(const Vector3& v, const Matrix4x4& R)
    {
        return {
            v.x * R.m[0][0] + v.y * R.m[1][0] + v.z * R.m[2][0],
            v.x * R.m[0][1] + v.y * R.m[1][1] + v.z * R.m[2][1],
            v.x * R.m[0][2] + v.y * R.m[1][2] + v.z * R.m[2][2]
        };
    }

    // 親ボディの姿勢に対して関節パーツ(翼・空中時の足)を追従配置
    inline void AttachLimbToBody(
        Object3d* limbObj, const Vector3& pivotLocal, const Vector3& extraLocalOffset, const Vector3& localJointRot,
        const Vector3& bodyPos, const Vector3& bodyScale, const Matrix4x4& bodyRotMat)
    {
        if (!limbObj || limbObj->GetModelData().vertices.empty()) return;

        const Vector3 scaledLocal = {
            (pivotLocal.x + extraLocalOffset.x) * bodyScale.x,
            (pivotLocal.y + extraLocalOffset.y) * bodyScale.y,
            (pivotLocal.z + extraLocalOffset.z) * bodyScale.z
        };
        const Matrix4x4 jointRotMat = Multiply(MakeRotateZMatrix(localJointRot.z), Multiply(MakeRotateXMatrix(localJointRot.x), MakeRotateYMatrix(localJointRot.y)));
        const float uniformScale    = (bodyScale.x + bodyScale.y + bodyScale.z) / kAxisCountFloat;

        limbObj->SetTranslate(bodyPos + TransformVec3ByRotation(scaledLocal, bodyRotMat));
        limbObj->SetRotate(DecomposeRotationToEulerXYZ(Multiply(jointRotMat, bodyRotMat)));
        limbObj->SetScale({ uniformScale, uniformScale, uniformScale });
        limbObj->Update();
    }

    // 地上歩行・ダッシュ時の水かき足ソルバ（接地中はY=0に密着し、遊脚時は半円アークで踏み出す）
    inline Vector3 SolveGroundedFoot(
        Object3d* footObj, const Vector3& pivotLocal, float sideSign, float footPhase,
        float walkBlend, float strideLength, float liftHeight, float moveLocalRight, float moveLocalFwd,
        const Vector3& rootGroundPos, float worldYaw, float baseScale)
    {
        const float swing01    = (std::max)(0.0f, std::sin(footPhase));
        const float strideCos  = -std::cos(footPhase);
        const float liftY      = std::pow(swing01, kFootLiftCurveExp) * liftHeight * walkBlend * baseScale;
        const float strideDist = strideCos * strideLength * walkBlend;
        const float lx         = (pivotLocal.x + moveLocalRight * strideDist * kFootLateralStrideRate + sideSign * (kFootStanceOutward * walkBlend)) * baseScale;
        const float lz         = (pivotLocal.z + moveLocalFwd * strideDist) * baseScale;
        const float cy         = std::cos(worldYaw);
        const float sy         = std::sin(worldYaw);
        const Vector3 worldFootPos = { rootGroundPos.x + lx * cy + lz * sy, rootGroundPos.y + liftY, rootGroundPos.z - lx * sy + lz * cy };

        if (footObj && !footObj->GetModelData().vertices.empty())
        {
            const float fwdSign    = (moveLocalFwd >= kFootBackpedalThresh) ? 1.0f : -1.0f;
            const float footPitchX = swing01 * (kFootStridePitchScale * strideCos * fwdSign + kFootSwingPitchLift * swing01) * walkBlend;
            const float footYawY   = sideSign * (kFootIdleToeOutYaw + kFootWalkToeOutYaw * walkBlend) + moveLocalRight * kFootStrafeYawScale * swing01 * walkBlend;
            const float footRollZ  = -moveLocalRight * kFootStrafeRollScale * swing01 * walkBlend;

            const Matrix4x4 localRot = Multiply(MakeRotateZMatrix(footRollZ), Multiply(MakeRotateXMatrix(footPitchX), MakeRotateYMatrix(footYawY)));
            footObj->SetTranslate(worldFootPos);
            footObj->SetRotate(DecomposeRotationToEulerXYZ(Multiply(localRot, MakeRotateYMatrix(worldYaw))));
            footObj->SetScale({ baseScale, baseScale, baseScale });
            footObj->Update();
        }
        return worldFootPos;
    }
}
