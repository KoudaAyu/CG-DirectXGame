#include "SlimePhysics.h"

namespace SlimePhysics
{
    static float sFriction = 1.3f; // スライム共通の地面摩擦係数（通常・合体・ミニオン共通）
    static Object3d* sGroundObject = nullptr;
    static MeshCollider* sGroundCollider = nullptr;

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
    }

    void ClearGroundMesh()
    {
        sGroundObject = nullptr;
        sGroundCollider = nullptr;
    }

    float CalculateGroundHeight(float x, float z, const Vector2& stageTilt, const Vector2& pivot)
    {
        return CalculateGroundHeightEx(x, z, kIgnoreCurrentY, stageTilt, nullptr, nullptr, pivot);
    }

    float CalculateGroundHeightEx(float x, float z, float currentY, const Vector2& stageTilt, bool* outHasGround, Vector3* outNormal, const Vector2& pivot)
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
            const Matrix4x4& worldMatrix = sGroundObject->GetWorldMatrix();
            Matrix4x4 invWorld = Inverse(worldMatrix);

            // 地面メッシュのスケールから島の最大高さを算出
            const Vector3& s = sGroundObject->GetScale();
            float maxScale = (std::max)({ std::abs(s.x), std::abs(s.y), std::abs(s.z), 0.05f });
            float groundTopWorld = sGroundObject->GetTranslate().y + 250.0f * maxScale;

            // レイ開始Y座標の決定:
            // currentY が指定されている場合は多層足場対応（頭上天井チェックを行い、足元基準で開始）
            // currentY == kIgnoreCurrentY の場合は最上空から開始
            float rayStartY = groundTopWorld + 10.0f;

            if (currentY != kIgnoreCurrentY)
            {
                // 最大登坂可能段差マージン（坂道や段差のスムーズな登坂用）
                const float kMaxStepUp = 1.2f;
                float allowedStepUp = kMaxStepUp;

                // 頭上天井チェック（Ceiling Check）:
                // currentY から上向きにレイを撃ち、頭上に天井や上の階の足場（底面または上面）があるか確認
                Vector3 ceilRayStartWorld = { x, currentY, z };
                Vector3 ceilRayDirWorld = { 0.0f, 1.0f, 0.0f };

                Vector3 localCeilStart = {
                    ceilRayStartWorld.x * invWorld.m[0][0] + ceilRayStartWorld.y * invWorld.m[1][0] + ceilRayStartWorld.z * invWorld.m[2][0] + invWorld.m[3][0],
                    ceilRayStartWorld.x * invWorld.m[0][1] + ceilRayStartWorld.y * invWorld.m[1][1] + ceilRayStartWorld.z * invWorld.m[2][1] + invWorld.m[3][1],
                    ceilRayStartWorld.x * invWorld.m[0][2] + ceilRayStartWorld.y * invWorld.m[1][2] + ceilRayStartWorld.z * invWorld.m[2][2] + invWorld.m[3][2]
                };

                Vector3 localCeilDir = {
                    ceilRayDirWorld.x * invWorld.m[0][0] + ceilRayDirWorld.y * invWorld.m[1][0] + ceilRayDirWorld.z * invWorld.m[2][0],
                    ceilRayDirWorld.x * invWorld.m[0][1] + ceilRayDirWorld.y * invWorld.m[1][1] + ceilRayDirWorld.z * invWorld.m[2][1],
                    ceilRayDirWorld.x * invWorld.m[0][2] + ceilRayDirWorld.y * invWorld.m[1][2] + ceilRayDirWorld.z * invWorld.m[2][2]
                };

                float ceilDirLen = std::sqrt(localCeilDir.x * localCeilDir.x + localCeilDir.y * localCeilDir.y + localCeilDir.z * localCeilDir.z);
                if (ceilDirLen > 1e-6f)
                {
                    localCeilDir.x /= ceilDirLen;
                    localCeilDir.y /= ceilDirLen;
                    localCeilDir.z /= ceilDirLen;

                    float ceilMaxDist = (kMaxStepUp + 0.1f) * ceilDirLen;
                    float ceilHitDist = 0.0f;
                    Vector3 ceilHitNorm, cV0, cV1, cV2;

                    if (sGroundCollider->GetAABBTree().Raycast(localCeilStart, localCeilDir, ceilMaxDist, ceilHitDist, ceilHitNorm, cV0, cV1, cV2))
                    {
                        Vector3 localCeilHit = {
                            localCeilStart.x + localCeilDir.x * ceilHitDist,
                            localCeilStart.y + localCeilDir.y * ceilHitDist,
                            localCeilStart.z + localCeilDir.z * ceilHitDist
                        };
                        float ceilWorldY = localCeilHit.x * worldMatrix.m[0][1] + localCeilHit.y * worldMatrix.m[1][1] + localCeilHit.z * worldMatrix.m[2][1] + worldMatrix.m[3][1];
                        float ceilingDist = ceilWorldY - currentY;
                        if (ceilingDist > 0.05f)
                        {
                            allowedStepUp = (std::min)(allowedStepUp, (std::max)(0.0f, ceilingDist - 0.08f));
                        }
                        else
                        {
                            allowedStepUp = 0.0f;
                        }
                    }
                }

                rayStartY = currentY + allowedStepUp;
            }

            // 下向きレイキャスト（貫通ループ付き）
            // 天井の裏面や急な垂直壁に当たった場合、貫通してその下にある歩行可能地面（上向きポリゴン）を探す
            Vector3 currentRayStartWorld = { x, rayStartY, z };
            Vector3 rayDirWorld = { 0.0f, -1.0f, 0.0f };
            const int kMaxPenetrations = 8;
            float maxDist = 100000.0f;

            for (int iter = 0; iter < kMaxPenetrations; ++iter)
            {
                Vector3 localStart = {
                    currentRayStartWorld.x * invWorld.m[0][0] + currentRayStartWorld.y * invWorld.m[1][0] + currentRayStartWorld.z * invWorld.m[2][0] + invWorld.m[3][0],
                    currentRayStartWorld.x * invWorld.m[0][1] + currentRayStartWorld.y * invWorld.m[1][1] + currentRayStartWorld.z * invWorld.m[2][1] + invWorld.m[3][1],
                    currentRayStartWorld.x * invWorld.m[0][2] + currentRayStartWorld.y * invWorld.m[1][2] + currentRayStartWorld.z * invWorld.m[2][2] + invWorld.m[3][2]
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

                // 上向き（歩行可能地面）ポリゴンか検証:
                // 天井・洞窟裏・垂直崖（Ny <= 0.05f）は地面ではないため貫通して下を探す
                if (worldTriNorm.y > 0.05f)
                {
                    if (outHasGround) *outHasGround = true;
                    if (outNormal) *outNormal = worldTriNorm;
                    return worldY;
                }

                // 非地面ポリゴン（天井裏面など）を貫通してさらに下方を探索
                currentRayStartWorld.y = worldY - 0.02f;
            }

            // 足元に地面メッシュが見つからない（穴や崖・島の外）
            if (outHasGround) *outHasGround = false;
            return (currentY != kIgnoreCurrentY) ? currentY : 0.0f;
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

    float CalculateGroundedCenterYEx(float x, float z, float currentY, const Vector2& stageTilt, float baseOffset, bool* outHasGround, Vector3* outNormal, const Vector2& pivot)
    {
        Vector3 norm{ 0.0f, 1.0f, 0.0f };
        float groundHeight = CalculateGroundHeightEx(x, z, currentY, stageTilt, outHasGround, &norm, pivot);
        if (outNormal) *outNormal = norm;

        // 斜面に対する球体の厳密な接地中心補正: CenterY = GroundHeight + baseOffset / cos(theta)
        // 傾斜面でもスライム底面が地面にジャストフィットし、埋まり・浮きを幾何学的にゼロ化
        float ny = (std::clamp)(norm.y, 0.25f, 1.0f);
        return groundHeight + (baseOffset / ny);
    }

    Vector3 GetGroundNormal(float x, float z, const Vector2& stageTilt, const Vector2& pivot)
    {
        bool hasGround = false;
        Vector3 norm;
        CalculateGroundHeightEx(x, z, kIgnoreCurrentY, stageTilt, &hasGround, &norm, pivot);
        return norm;
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
