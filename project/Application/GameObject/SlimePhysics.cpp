#include "SlimePhysics.h"
#include "Baziru3_Engine/Framework/Collision/CollisionManager.h"

namespace SlimePhysics
{
    static float sFriction = 1.3f; // スライム共通の地面摩擦係数（通常・合体・ミニオン共通）
    static Object3d* sGroundObject = nullptr;
    static MeshCollider* sGroundCollider = nullptr;

    // 地面メッシュのフレーム追従用トランスフォーム履歴
    static Matrix4x4 sPrevGroundWorldMatrix;
    static Matrix4x4 sCurrGroundWorldMatrix;
    static Matrix4x4 sInvPrevGroundWorldMatrix;
    static bool sHasPrevGroundMatrix = false;
    static uint32_t sLastFrameCount = 0xFFFFFFFF;

    float GetFriction()
    {
        return sFriction;
    }

    void SetFriction(float friction)
    {
        sFriction = friction;
    }

    void SetGroundMesh(Object3d* groundObject, MeshCollider* groundCollider)
    {
        sGroundObject = groundObject;
        sGroundCollider = groundCollider;
        sHasPrevGroundMatrix = false;
        sLastFrameCount = 0xFFFFFFFF;
    }

    void ClearGroundMesh()
    {
        sGroundObject = nullptr;
        sGroundCollider = nullptr;
        sHasPrevGroundMatrix = false;
        sLastFrameCount = 0xFFFFFFFF;
    }

    float CalculateGroundHeight(float x, float z, const Vector2& stageTilt, const Vector2& pivot)
    {
        return CalculateGroundHeightEx(x, z, kIgnoreCurrentY, stageTilt, nullptr, nullptr, pivot);
    }

    float CalculateGroundHeightEx(float x, float z, float currentY, const Vector2& stageTilt, bool* outHasGround, Vector3* outNormal, const Vector2& pivot, bool isGrounded, float baseOffset)
    {
        if (outHasGround) *outHasGround = false;
        if (outNormal)
        {
            Vector3 defaultNorm = { -std::sin(stageTilt.y), std::cos(stageTilt.x) * std::cos(stageTilt.y), -std::sin(stageTilt.x) };
            float len = std::sqrt(defaultNorm.x * defaultNorm.x + defaultNorm.y * defaultNorm.y + defaultNorm.z * defaultNorm.z);
            *outNormal = (len > 1e-5f) ? defaultNorm * (1.0f / len) : Vector3{ 0.0f, 1.0f, 0.0f };
        }

        // 1. 地面メッシュが登録されている場合、AABBTree による多層対応レイキャストで精密メッシュ表面を判定
        if (sGroundObject && sGroundCollider)
        {
            // フレーム進行時の世界行列更新（同フレーム内の複数回呼び出しでは更新せず共有）
            uint32_t currentFrame = CollisionManager::GetInstance()->GetFrameCount();
            if (currentFrame != sLastFrameCount)
            {
                if (sHasPrevGroundMatrix)
                {
                    sPrevGroundWorldMatrix = sCurrGroundWorldMatrix;
                    sInvPrevGroundWorldMatrix = Inverse(sPrevGroundWorldMatrix);
                }
                sCurrGroundWorldMatrix = sGroundObject->GetWorldMatrix();
                if (!sHasPrevGroundMatrix)
                {
                    sPrevGroundWorldMatrix = sCurrGroundWorldMatrix;
                    sInvPrevGroundWorldMatrix = Inverse(sPrevGroundWorldMatrix);
                    sHasPrevGroundMatrix = true;
                }
                sLastFrameCount = currentFrame;
            }

            const Matrix4x4& worldMatrix = sCurrGroundWorldMatrix;
            Matrix4x4 invWorld = Inverse(worldMatrix);

            // 地面メッシュのスケールから島の最大高さを算出
            const Vector3& s = sGroundObject->GetScale();
            float maxScale = (std::max)({ std::abs(s.x), std::abs(s.y), std::abs(s.z), 0.05f });
            float groundTopWorld = sGroundObject->GetTranslate().y + 250.0f * maxScale;

            // 確実にステージ全体の最上空から真下にレイを撃つ（ステージ傾斜による急激な床の持ち上がりも100%捕捉）
            float topY = groundTopWorld + 10.0f;
            Vector3 currentRayStartWorld = { x, topY, z };
            Vector3 rayDirWorld = { 0.0f, -1.0f, 0.0f };

            struct GroundCandidate {
                float worldY;
                Vector3 worldNormal;
            };
            std::vector<GroundCandidate> groundCandidates;

            auto PerformRaycastAt = [&](float rayX, float rayZ) {
                float currentRayY = topY;
                Vector3 rayDirWorld = { 0.0f, -1.0f, 0.0f };
                float lastHitWorldY = 1e9f;
                const float maxDist = 5000.0f;

                for (int iter = 0; iter < 16; ++iter)
                {
                    Vector3 localStart = {
                        rayX * invWorld.m[0][0] + currentRayY * invWorld.m[1][0] + rayZ * invWorld.m[2][0] + invWorld.m[3][0],
                        rayX * invWorld.m[0][1] + currentRayY * invWorld.m[1][1] + rayZ * invWorld.m[2][1] + invWorld.m[3][1],
                        rayX * invWorld.m[0][2] + currentRayY * invWorld.m[1][2] + rayZ * invWorld.m[2][2] + invWorld.m[3][2]
                    };

                    Vector3 localDir = {
                        rayDirWorld.x * invWorld.m[0][0] + rayDirWorld.y * invWorld.m[1][0] + rayDirWorld.z * invWorld.m[2][0],
                        rayDirWorld.x * invWorld.m[0][1] + rayDirWorld.y * invWorld.m[1][1] + rayDirWorld.z * invWorld.m[2][1],
                        rayDirWorld.x * invWorld.m[0][2] + rayDirWorld.y * invWorld.m[1][2] + rayDirWorld.z * invWorld.m[2][2]
                    };

                    float dirLen = std::sqrt(localDir.x * localDir.x + localDir.y * localDir.y + localDir.z * localDir.z);
                    if (dirLen > 1e-6f)
                    {
                        localDir.x /= dirLen;
                        localDir.y /= dirLen;
                        localDir.z /= dirLen;
                    }

                    float hitDist = 0.0f;
                    Vector3 hitNormal, v0, v1, v2;

                    if (!sGroundCollider->GetAABBTree().Raycast(localStart, localDir, maxDist, hitDist, hitNormal, v0, v1, v2))
                    {
                        break; // これ以上下にメッシュが存在しない
                    }

                    // ヒットしたポリゴンの幾何法線を算出
                    Vector3 e1 = v1 - v0;
                    Vector3 e2 = v2 - v0;
                    Vector3 localTriNorm = {
                        e1.y * e2.z - e1.z * e2.y,
                        e1.z * e2.x - e1.x * e2.z,
                        e1.x * e2.y - e1.y * e2.x
                    };
                    float triNormLen = std::sqrt(localTriNorm.x * localTriNorm.x + localTriNorm.y * localTriNorm.y + localTriNorm.z * localTriNorm.z);
                    if (triNormLen > 1e-6f)
                    {
                        localTriNorm = localTriNorm * (1.0f / triNormLen);
                    }

                    Vector3 worldTriNorm = {
                        localTriNorm.x * worldMatrix.m[0][0] + localTriNorm.y * worldMatrix.m[1][0] + localTriNorm.z * worldMatrix.m[2][0],
                        localTriNorm.x * worldMatrix.m[0][1] + localTriNorm.y * worldMatrix.m[1][1] + localTriNorm.z * worldMatrix.m[2][1],
                        localTriNorm.x * worldMatrix.m[0][2] + localTriNorm.y * worldMatrix.m[1][2] + localTriNorm.z * worldMatrix.m[2][2]
                    };
                    float wNormLen = std::sqrt(worldTriNorm.x * worldTriNorm.x + worldTriNorm.y * worldTriNorm.y + worldTriNorm.z * worldTriNorm.z);
                    if (wNormLen > 1e-6f)
                    {
                        worldTriNorm = worldTriNorm * (1.0f / wNormLen);
                    }

                    // ヒット地点のワールド座標
                    Vector3 localHit = {
                        localStart.x + localDir.x * hitDist,
                        localStart.y + localDir.y * hitDist,
                        localStart.z + localDir.z * hitDist
                    };
                    float worldY = localHit.x * worldMatrix.m[0][1] + localHit.y * worldMatrix.m[1][1] + localHit.z * worldMatrix.m[2][1] + worldMatrix.m[3][1];

                    // 同一ポリゴンや極小オフセットによる重複ヒットを防止（前回のヒットより少なくとも 0.005f 以上下であること）
                    if (worldY < lastHitWorldY - 0.005f)
                    {
                        lastHitWorldY = worldY;

                        // 歩行可能地面ポリゴンか判定:
                        // 傾斜約78度までの面をすべて地面候補として収集（急斜面や崖縁でのすり抜けを完全防止）
                        if (localTriNorm.y >= 0.20f && worldTriNorm.y > 0.05f)
                        {
                            groundCandidates.push_back({ worldY, worldTriNorm });
                        }
                    }

                    // 次の貫通探索のため、ヒット地点より十分に下（0.05f）からレイを開始
                    currentRayY = worldY - 0.05f;
                }
            };

            PerformRaycastAt(x, z);

            // ポリゴン同士の継ぎ目（シーム）でレイがすり抜けた場合のフォールバック（十字微小ジッター探索）
            if (groundCandidates.empty())
            {
                const float jitter = 0.035f;
                PerformRaycastAt(x + jitter, z);
                if (groundCandidates.empty()) PerformRaycastAt(x - jitter, z);
                if (groundCandidates.empty()) PerformRaycastAt(x, z + jitter);
                if (groundCandidates.empty()) PerformRaycastAt(x, z - jitter);
            }

            if (groundCandidates.empty())
            {
                // 地面が見つからない（完全に島の外の奈落）
                if (outHasGround) *outHasGround = false;
                return (currentY != kIgnoreCurrentY) ? currentY : 0.0f;
            }

            // 上から下へ降順ソート
            std::sort(groundCandidates.begin(), groundCandidates.end(), [](const GroundCandidate& a, const GroundCandidate& b) {
                return a.worldY > b.worldY;
            });

            // 1. currentY が未指定の場合（カメラや照準など最上面を取得したい場合）
            if (currentY == kIgnoreCurrentY)
            {
                if (outHasGround) *outHasGround = true;
                if (outNormal) *outNormal = groundCandidates[0].worldNormal;
                return groundCandidates[0].worldY;
            }

            // 2. 接地中（isGrounded == true）の場合:
            // ステージ傾斜・揺らしの追従変位を反映しつつ、段差・坂道を適切に追従
            if (isGrounded)
            {
                float deltaYTilt = 0.0f;
                if (sHasPrevGroundMatrix)
                {
                    Vector3 localPt = {
                        x * sInvPrevGroundWorldMatrix.m[0][0] + currentY * sInvPrevGroundWorldMatrix.m[1][0] + z * sInvPrevGroundWorldMatrix.m[2][0] + sInvPrevGroundWorldMatrix.m[3][0],
                        x * sInvPrevGroundWorldMatrix.m[0][1] + currentY * sInvPrevGroundWorldMatrix.m[1][1] + z * sInvPrevGroundWorldMatrix.m[2][1] + sInvPrevGroundWorldMatrix.m[3][1],
                        x * sInvPrevGroundWorldMatrix.m[0][2] + currentY * sInvPrevGroundWorldMatrix.m[1][2] + z * sInvPrevGroundWorldMatrix.m[2][2] + sInvPrevGroundWorldMatrix.m[3][2]
                    };
                    float newWorldY = localPt.x * sCurrGroundWorldMatrix.m[0][1] + localPt.y * sCurrGroundWorldMatrix.m[1][1] + localPt.z * sCurrGroundWorldMatrix.m[2][1] + sCurrGroundWorldMatrix.m[3][1];
                    deltaYTilt = newWorldY - currentY;
                }

                float expectedFloorY = currentY + deltaYTilt;
                // 自力登坂・段差許容マージン（スライムのスケール baseOffset に応じて動的に拡張し、急成長時でも床を見失わない）
                float stepMargin = (std::max)(1.6f, baseOffset * 1.6f);
                float maxAllowedFloorY = expectedFloorY + stepMargin;

                int bestIdx = -1;
                for (size_t i = 0; i < groundCandidates.size(); ++i)
                {
                    if (groundCandidates[i].worldY <= maxAllowedFloorY)
                    {
                        bestIdx = static_cast<int>(i);
                        break;
                    }
                }

                // もし全候補が expectedFloorY + stepMargin より上にある場合（急激なサイズアップや激突でめり込んでいる場合）
                // 最上面の床（groundCandidates[0]）がスライムの体内（baseOffset * 2.5f以内）にあれば、
                // 沈み込みとみなして即座に最上面の床に復帰救済
                if (bestIdx == -1 && !groundCandidates.empty())
                {
                    float embedRecoveryLimit = expectedFloorY + (std::max)(3.5f, baseOffset * 2.5f);
                    if (groundCandidates[0].worldY <= embedRecoveryLimit)
                    {
                        bestIdx = 0; // 最上面の床へ復帰
                    }
                    else if (groundCandidates.back().worldY <= embedRecoveryLimit)
                    {
                        bestIdx = static_cast<int>(groundCandidates.size() - 1);
                    }
                }

                if (bestIdx != -1)
                {
                    if (outHasGround) *outHasGround = true;
                    if (outNormal) *outNormal = groundCandidates[bestIdx].worldNormal;
                    return groundCandidates[bestIdx].worldY;
                }

                if (outHasGround) *outHasGround = false;
                return expectedFloorY;
            }

            // 3. 空中・落下中（isGrounded == false）の場合:
            // 高速落下・飛び降り時および合体時のすり抜け（トンネリング）を完全に防止
            // スライムの足元または上空まで探索範囲を広げ、着地可能な床を確実に捕捉
            float maxAllowedLandingFloorY = currentY + (std::max)(5.0f, baseOffset * 2.2f);

            int bestIdx = -1;
            for (size_t i = 0; i < groundCandidates.size(); ++i)
            {
                if (groundCandidates[i].worldY <= maxAllowedLandingFloorY)
                {
                    bestIdx = static_cast<int>(i);
                    break;
                }
            }

            // 万一高速落下で床を突き抜けた場合でも、島内に床候補が存在するなら最上面の床に救済着地
            if (bestIdx == -1 && !groundCandidates.empty())
            {
                bestIdx = 0; // 最上面の床に安全着地
            }

            if (bestIdx != -1)
            {
                if (outHasGround) *outHasGround = true;
                if (outNormal) *outNormal = groundCandidates[bestIdx].worldNormal;
                return groundCandidates[bestIdx].worldY;
            }

            if (outHasGround) *outHasGround = false;
            return currentY;
        }

        // 2. 地面メッシュ未登録時のフォールバック: 傾斜平面の数式
        float relX = x - pivot.x;
        float relZ = z - pivot.y;

        float cosRoll = std::cos(stageTilt.y);
        float safeCosRoll = (std::max)(cosRoll, 0.01f);

        if (outHasGround) *outHasGround = true;
        return -std::tan(stageTilt.y) * relX - (std::tan(stageTilt.x) / safeCosRoll) * relZ;
    }

    float CalculateGroundedCenterY(float x, float z, const Vector2& stageTilt, float baseOffset, const Vector2& pivot)
    {
        return CalculateGroundedCenterYEx(x, z, 0.0f, stageTilt, baseOffset, nullptr, nullptr, pivot);
    }

    float CalculateGroundedCenterYEx(float x, float z, float currentY, const Vector2& stageTilt, float baseOffset, bool* outHasGround, Vector3* outNormal, const Vector2& pivot, bool isGrounded)
    {
        Vector3 norm{ 0.0f, 1.0f, 0.0f };
        // currentY はスライムの中心Y座標なので、足元（床面）の高さに変換して判定
        float footY = (currentY != kIgnoreCurrentY) ? (currentY - baseOffset) : kIgnoreCurrentY;
        float groundHeight = CalculateGroundHeightEx(x, z, footY, stageTilt, outHasGround, &norm, pivot, isGrounded, baseOffset);
        if (outNormal) *outNormal = norm;

        // 斜面に対する球体・平べったいスライムの幾何学的接地中心補正
        // スライムは横幅が広く（下部横半径 ~1.35倍）、傾斜時に下り坂方向へ内容物が流動するため、
        // 傾斜角に応じて下り坂側の縁（エッジ）が地面にめり込むのを幾何学的に完全に防ぐリフト補正を適用
        float ny = (std::clamp)(norm.y, 0.25f, 1.0f);
        float sinTheta = std::sqrt((std::max)(0.0f, 1.0f - ny * ny));
        float effectiveOffset = baseOffset * (1.0f + 0.40f * sinTheta);
        return groundHeight + (effectiveOffset / ny);
    }

    Vector3 GetGroundNormal(float x, float z, const Vector2& stageTilt, const Vector2& pivot)
    {
        bool hasGround = false;
        Vector3 norm;
        CalculateGroundHeightEx(x, z, kIgnoreCurrentY, stageTilt, &hasGround, &norm, pivot);
        return norm;
    }

    // 点 p と 3D三角形 (a, b, c) の幾何学的最近接点を算出（Ericson's Point to Triangle Algorithm）
    static inline Vector3 ClosestPointOnTriangle(const Vector3& p, const Vector3& a, const Vector3& b, const Vector3& c)
    {
        auto DotV3 = [](const Vector3& u, const Vector3& v) -> float {
            return u.x * v.x + u.y * v.y + u.z * v.z;
        };

        Vector3 ab = b - a;
        Vector3 ac = c - a;
        Vector3 ap = p - a;
        float d1 = DotV3(ab, ap);
        float d2 = DotV3(ac, ap);
        if (d1 <= 0.0f && d2 <= 0.0f) return a;

        Vector3 bp = p - b;
        float d3 = DotV3(ab, bp);
        float d4 = DotV3(ac, bp);
        if (d3 >= 0.0f && d4 <= d3) return b;

        float vc = d1 * d4 - d3 * d2;
        if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
            float v = d1 / (d1 - d3);
            return a + ab * v;
        }

        Vector3 cp = p - c;
        float d5 = DotV3(ab, cp);
        float d6 = DotV3(ac, cp);
        if (d6 >= 0.0f && d5 <= d6) return c;

        float vb = d5 * d2 - d1 * d6;
        if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
            float w = d2 / (d2 - d6);
            return a + ac * w;
        }

        float va = d3 * d6 - d5 * d4;
        if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
            float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
            return b + (c - b) * w;
        }

        float denom = 1.0f / (va + vb + vc);
        float v = vb * denom;
        float w = vc * denom;
        return a + ab * v + ac * w;
    }

    bool ResolveWallCollision(Vector3& position, Vector3& velocity, float radius, float heightOffset)
    {
        if (!sGroundObject || !sGroundCollider) return false;

        const Matrix4x4& worldMatrix = sGroundObject->GetWorldMatrix();
        Matrix4x4 invWorld = Inverse(worldMatrix);

        auto TransformPt = [](const Vector3& p, const Matrix4x4& m) -> Vector3 {
            return {
                p.x * m.m[0][0] + p.y * m.m[1][0] + p.z * m.m[2][0] + m.m[3][0],
                p.x * m.m[0][1] + p.y * m.m[1][1] + p.z * m.m[2][1] + m.m[3][1],
                p.x * m.m[0][2] + p.y * m.m[1][2] + p.z * m.m[2][2] + m.m[3][2]
            };
        };

        bool anyCollided = false;

        // 最大2回の反復解決（コーナーや直角壁での多重壁押し出しを過剰反発なしに滑らかに収束）
        for (int iter = 0; iter < 2; ++iter)
        {
            Vector3 waistPos = { position.x, position.y + heightOffset, position.z };

            // 判定方向: 進行方向 + 水平8方向（四方・対角線）
            Vector3 testDirs[9];
            int numDirs = 0;

            float horizSpeedSq = velocity.x * velocity.x + velocity.z * velocity.z;
            if (horizSpeedSq > 1e-4f)
            {
                float invSpd = 1.0f / std::sqrt(horizSpeedSq);
                testDirs[numDirs++] = { velocity.x * invSpd, 0.0f, velocity.z * invSpd };
            }

            testDirs[numDirs++] = {  1.0f, 0.0f,  0.0f };
            testDirs[numDirs++] = { -1.0f, 0.0f,  0.0f };
            testDirs[numDirs++] = {  0.0f, 0.0f,  1.0f };
            testDirs[numDirs++] = {  0.0f, 0.0f, -1.0f };
            testDirs[numDirs++] = {  0.7071f, 0.0f,  0.7071f };
            testDirs[numDirs++] = { -0.7071f, 0.0f,  0.7071f };
            testDirs[numDirs++] = {  0.7071f, 0.0f, -0.7071f };
            testDirs[numDirs++] = { -0.7071f, 0.0f, -0.7071f };

            // 探索範囲: radius + マージン
            const float checkDist = radius + 0.25f;

            float maxPenetration = 0.0f;
            Vector2 bestPushDir = { 0.0f, 0.0f };

            // スライムの体積（中心とやや上方）を捉えるため2つの高さで探査（地下地形への誤ヒットを防止）
            float yOffsets[] = { 0.0f, radius * 0.20f };

            for (float yOff : yOffsets)
            {
                Vector3 rayOriginWorld = { waistPos.x, waistPos.y + yOff, waistPos.z };
                Vector3 localStart = TransformPt(rayOriginWorld, invWorld);

                for (int i = 0; i < numDirs; ++i)
                {
                    const Vector3& dirWorld = testDirs[i];

                    Vector3 localDir = {
                        dirWorld.x * invWorld.m[0][0] + dirWorld.y * invWorld.m[1][0] + dirWorld.z * invWorld.m[2][0],
                        dirWorld.x * invWorld.m[0][1] + dirWorld.y * invWorld.m[1][1] + dirWorld.z * invWorld.m[2][1],
                        dirWorld.x * invWorld.m[0][2] + dirWorld.y * invWorld.m[1][2] + dirWorld.z * invWorld.m[2][2]
                    };

                    float localDirLen = std::sqrt(localDir.x * localDir.x + localDir.y * localDir.y + localDir.z * localDir.z);
                    if (localDirLen < 1e-6f) continue;
                    localDir = localDir * (1.0f / localDirLen);

                    float maxLocalDist = checkDist * localDirLen;
                    float hitDist = 0.0f;
                    Vector3 hitNormal, v0, v1, v2;

                    if (sGroundCollider->GetAABBTree().Raycast(localStart, localDir, maxLocalDist, hitDist, hitNormal, v0, v1, v2))
                    {
                        // 三角形の幾何法線
                        Vector3 e1 = v1 - v0;
                        Vector3 e2 = v2 - v0;
                        Vector3 localTriNorm = {
                            e1.y * e2.z - e1.z * e2.y,
                            e1.z * e2.x - e1.x * e2.z,
                            e1.x * e2.y - e1.y * e2.x
                        };
                        float triNormLen = std::sqrt(localTriNorm.x * localTriNorm.x + localTriNorm.y * localTriNorm.y + localTriNorm.z * localTriNorm.z);
                        if (triNormLen > 1e-6f)
                        {
                            localTriNorm = localTriNorm * (1.0f / triNormLen);
                        }

                        // 壁判定: 急峻な面のみ壁として押し出す（localTriNorm.y < 0.55f）
                        if (localTriNorm.y < 0.55f)
                        {
                            Vector3 wV0 = TransformPt(v0, worldMatrix);
                            Vector3 wV1 = TransformPt(v1, worldMatrix);
                            Vector3 wV2 = TransformPt(v2, worldMatrix);

                            // スライム中心（探査点）から三角形への最近接点 Q を厳密に計算
                            Vector3 probePos = { waistPos.x, waistPos.y + yOff, waistPos.z };
                            Vector3 closestQ = ClosestPointOnTriangle(probePos, wV0, wV1, wV2);

                            // 最近接点とスライム中心の水平ベクトル
                            Vector2 diffXZ = { probePos.x - closestQ.x, probePos.z - closestQ.z };
                            float distXZ = std::sqrt(diffXZ.x * diffXZ.x + diffXZ.y * diffXZ.y);

                            // 壁のワールド法線
                            Vector3 worldTriNorm = {
                                localTriNorm.x * worldMatrix.m[0][0] + localTriNorm.y * worldMatrix.m[1][0] + localTriNorm.z * worldMatrix.m[2][0],
                                localTriNorm.x * worldMatrix.m[0][1] + localTriNorm.y * worldMatrix.m[1][1] + localTriNorm.z * worldMatrix.m[2][1],
                                localTriNorm.x * worldMatrix.m[0][2] + localTriNorm.y * worldMatrix.m[1][2] + localTriNorm.z * worldMatrix.m[2][2]
                            };
                            float wNormLen = std::sqrt(worldTriNorm.x * worldTriNorm.x + worldTriNorm.y * worldTriNorm.y + worldTriNorm.z * worldTriNorm.z);
                            if (wNormLen > 1e-6f)
                            {
                                worldTriNorm = worldTriNorm * (1.0f / wNormLen);
                            }

                            Vector2 pushDir = { worldTriNorm.x, worldTriNorm.z };
                            float pushLen = std::sqrt(pushDir.x * pushDir.x + pushDir.y * pushDir.y);
                            if (pushLen > 1e-4f)
                            {
                                pushDir = { pushDir.x / pushLen, pushDir.y / pushLen };
                            }
                            else
                            {
                                continue;
                            }

                            // 壁平面からの符号付き垂直距離
                            float signedDist = (probePos.x - wV0.x) * worldTriNorm.x +
                                              (probePos.y - wV0.y) * worldTriNorm.y +
                                              (probePos.z - wV0.z) * worldTriNorm.z;

                            float penetration = 0.0f;
                            // 幾何学的に正確なめり込み深さの算出:
                            // 1. 壁の表面/裏側にめり込んでいる場合（符号付き距離）
                            if (signedDist < radius && signedDist > -radius * 1.5f)
                            {
                                penetration = radius - (std::max)(0.0f, signedDist);
                            }
                            // 2. 三角形のエッジ/頂点に接している場合
                            else if (distXZ < radius)
                            {
                                penetration = radius - distXZ;
                                if (distXZ > 1e-4f)
                                {
                                    pushDir = { diffXZ.x / distXZ, diffXZ.y / distXZ };
                                }
                            }

                            if (penetration > maxPenetration)
                            {
                                maxPenetration = penetration;
                                bestPushDir = pushDir;
                            }
                        }
                    }
                }
            }

            // このイテレーションで最大のめり込みを1回のみ正確に解消（多重加算による振動・ジッターを完全排除）
            if (maxPenetration > 1e-4f)
            {
                position.x += bestPushDir.x * maxPenetration;
                position.z += bestPushDir.y * maxPenetration;

                // 速度の壁法線方向成分を除去（壁に沿って滑らかにスライド）
                float vDotN = velocity.x * bestPushDir.x + velocity.z * bestPushDir.y;
                if (vDotN < 0.0f)
                {
                    velocity.x -= bestPushDir.x * vDotN;
                    velocity.z -= bestPushDir.y * vDotN;
                }

                anyCollided = true;
            }
            else
            {
                // めり込みがなければ反復終了
                break;
            }
        }

        return anyCollided;
    }

    void UpdateDeformation(SlimeParamsCPU& params, const DeformInput& input)
    {
        float dt = (std::max)(input.deltaTime, 0.0001f);

        if (input.isGrounded)
        {
            // --- 接地中（板の上でのスライム挙動） ---
            Vector3 accel = {
                (input.velocity.x - input.prevVelocity.x) / dt,
                0.0f,
                (input.velocity.z - input.prevVelocity.z) / dt
            };

            float speedMag = std::sqrt(input.velocity.x * input.velocity.x + input.velocity.z * input.velocity.z);
            float tiltMag = std::sqrt(input.stageTilt.x * input.stageTilt.x + input.stageTilt.y * input.stageTilt.y);

            // 1. 板の傾斜による下り坂方向への内容物移動ベクトル（もっこり感を保つため適度に調整）
            float tiltFlowFactor = input.isMerged ? 0.9f : (0.65f * input.massScale);
            float targetFlowX = std::sin(input.stageTilt.y) * tiltFlowFactor + input.velocity.x * 0.012f;
            float targetFlowZ = std::sin(input.stageTilt.x) * tiltFlowFactor + input.velocity.z * 0.012f;

            // 2. 接地重力および傾斜による上下の潰れ（ほどよく平べったい弾力スクワッシュ）
            float sag = input.isMerged ? -0.12f : -0.09f;
            float targetSquashY = sag - (std::min)(tiltMag * 0.22f + speedMag * 0.015f, 0.18f);

            // 3. スムーズスプリング補間（流動と潰れの追従）
            params.squashStretch.x += (targetFlowX - params.squashStretch.x) * (std::min)(1.0f, dt * 10.0f);
            params.squashStretch.z += (targetFlowZ - params.squashStretch.z) * (std::min)(1.0f, dt * 10.0f);
            params.squashStretch.y += (targetSquashY - params.squashStretch.y) * (std::min)(1.0f, dt * 10.0f);
        }
        else
        {
            // --- 空中（投擲・自由飛翔中） ---
            // 板の傾斜影響を受けず、飛行速度と鉛直重力による進行方向への伸び・水滴変形
            float targetFlowX = input.velocity.x * 0.025f;
            float targetFlowZ = input.velocity.z * 0.025f;
            float targetSquashY = (std::min)((std::max)(input.velocity.y * 0.018f, -0.15f), 0.25f);

            params.squashStretch.x += (targetFlowX - params.squashStretch.x) * (std::min)(1.0f, dt * 12.0f);
            params.squashStretch.z += (targetFlowZ - params.squashStretch.z) * (std::min)(1.0f, dt * 12.0f);
            params.squashStretch.y += (targetSquashY - params.squashStretch.y) * (std::min)(1.0f, dt * 12.0f);
        }

        // 4. 移動速度および衝撃に連動した表面波打ち強度（静止時は完全にゼロにして不要な波打ちを停止）
        float totalSpeed = std::sqrt(input.velocity.x * input.velocity.x + input.velocity.y * input.velocity.y + input.velocity.z * input.velocity.z);
        float targetWobble = 0.0f;
        if (totalSpeed > 0.15f || params.impulseStrength > 0.02f)
        {
            targetWobble = (std::min)(0.20f, totalSpeed * 0.025f + params.impulseStrength * 0.5f);
        }
        params.wobbleStrength += (targetWobble - params.wobbleStrength) * (std::min)(1.0f, dt * 8.0f);
        if (params.wobbleStrength < 0.001f) params.wobbleStrength = 0.0f;
    }
}
