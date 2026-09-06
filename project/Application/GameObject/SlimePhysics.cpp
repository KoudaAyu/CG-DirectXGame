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

            const int kMaxPenetrations = 32;
            float maxDist = 100000.0f;
            float lastHitWorldY = 1e9f;

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

                // 同一ポリゴンや極小オフセットによる重複ヒットを防止（前回のヒットより少なくとも 0.005f 以上下であること）
                if (worldY < lastHitWorldY - 0.005f)
                {
                    lastHitWorldY = worldY;

                    // 歩行可能地面ポリゴンか判定:
                    // モデル固有法線 localTriNorm.y >= 0.55f（傾斜約54.5度以下）で壁・垂直面（ny <= 0.31f）を完全に除外。
                    // 緩やかな坂道や正規のスロープのみを地面候補として収集する。
                    if (localTriNorm.y >= 0.55f && worldTriNorm.y > 0.15f)
                    {
                        groundCandidates.push_back({ worldY, worldTriNorm });
                    }
                }

                // 次の貫通探索のため、ヒット地点より十分に下（0.05f）からレイを開始
                currentRayStartWorld.y = worldY - 0.05f;
            }

            if (groundCandidates.empty())
            {
                // 地面が見つからない（島の外、または完全な垂直壁のみ）
                if (outHasGround) *outHasGround = false;
                return (currentY != kIgnoreCurrentY) ? currentY : 0.0f;
            }

            // --- 候補の中からプレイヤーの現在位置に最適な床を選択 ---

            // 1. currentY が未指定の場合（カメラや照準など最上面を取得したい場合）
            if (currentY == kIgnoreCurrentY)
            {
                if (outHasGround) *outHasGround = true;
                if (outNormal) *outNormal = groundCandidates[0].worldNormal;
                return groundCandidates[0].worldY;
            }

            // 2. 接地中（isGrounded == true）の場合:
            // ステージ傾斜の追従変位を反映し、自力登坂限界（kMaxStepUp = 0.35m）で上の階層へのテレポートを遮断
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

                // ステージ傾斜追従による今フレームの予想足元床高さ
                float expectedFloorY = currentY + deltaYTilt;

                // kMaxStepUp: スライムがジャンプやスロープ無しに段差を自力で登れる最大高さ (35cm)
                // expectedFloorY + kMaxStepUp を超える床は、頭上の天井・上の階層（Z字足場の上段など）であるため
                // 候補から除外し、テレポート・瞬間移動を100%防止する。
                const float kMaxStepUp = 0.35f;
                float maxAllowedFloorY = expectedFloorY + kMaxStepUp;

                int bestIdx = -1;
                for (size_t i = 0; i < groundCandidates.size(); ++i)
                {
                    if (groundCandidates[i].worldY <= maxAllowedFloorY)
                    {
                        bestIdx = static_cast<int>(i);
                        break;
                    }
                }

                if (bestIdx != -1)
                {
                    if (outHasGround) *outHasGround = true;
                    if (outNormal) *outNormal = groundCandidates[bestIdx].worldNormal;
                    return groundCandidates[bestIdx].worldY;
                }

                // 全候補が expectedFloorY + kMaxStepUp より上にある場合:
                // （下の足場の端で壁にぶつかり、上の足場しか存在しない領域に入り込んだケースなど）
                // 上の足場への瞬間移動を絶対に起こさせないため、hasGround = false とする
                if (outHasGround) *outHasGround = false;
                return expectedFloorY;
            }

            // 3. 空中・落下中（isGrounded == false）の場合:
            // 頭上（天井）以外の「足元・下にある最も高い床」を着地対象として検出する。
            // currentY は足元座標。スライム頭部（足元 + 全高 + マージン）以下にある床候補の中から最も高い床を選択。
            // 高速落下で1フレーム内に床を突き抜けた場合でも、頭部より下にある床なら100%確実に着地床として捕捉！
            float bodyHeight = (baseOffset > 0.0f) ? (baseOffset * 2.0f + 1.0f) : 2.5f;
            float maxAllowedLandingFloorY = currentY + bodyHeight;

            int bestIdx = -1;
            for (size_t i = 0; i < groundCandidates.size(); ++i)
            {
                if (groundCandidates[i].worldY <= maxAllowedLandingFloorY)
                {
                    bestIdx = static_cast<int>(i);
                    break;
                }
            }

            if (bestIdx != -1)
            {
                if (outHasGround) *outHasGround = true;
                if (outNormal) *outNormal = groundCandidates[bestIdx].worldNormal;
                return groundCandidates[bestIdx].worldY;
            }

            // 全候補が頭上より上にある場合（頭上にしか天井・足場がない場合）
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

    bool ResolveWallCollision(Vector3& position, Vector3& velocity, float radius, float heightOffset)
    {
        if (!sGroundObject || !sGroundCollider) return false;

        const Matrix4x4& worldMatrix = sGroundObject->GetWorldMatrix();
        Matrix4x4 invWorld = Inverse(worldMatrix);

        Vector3 waistPos = { position.x, position.y + heightOffset, position.z };

        // 判定方向の準備:
        // 進行方向（移動中の場合）+ 水平8方向（四方・対角線）
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

        bool collided = false;
        const float checkDist = radius + 0.15f;

        for (int i = 0; i < numDirs; ++i)
        {
            const Vector3& dirWorld = testDirs[i];

            Vector3 localStart = {
                waistPos.x * invWorld.m[0][0] + waistPos.y * invWorld.m[1][0] + waistPos.z * invWorld.m[2][0] + invWorld.m[3][0],
                waistPos.x * invWorld.m[0][1] + waistPos.y * invWorld.m[1][1] + waistPos.z * invWorld.m[2][1] + invWorld.m[3][1],
                waistPos.x * invWorld.m[0][2] + waistPos.y * invWorld.m[1][2] + waistPos.z * invWorld.m[2][2] + invWorld.m[3][2]
            };

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
                // ヒットしたポリゴンの幾何法線
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
                    float worldHitDist = hitDist / localDirLen;
                    if (worldHitDist < radius)
                    {
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

                        Vector2 wallPush = { worldTriNorm.x, worldTriNorm.z };
                        float pushLen = std::sqrt(wallPush.x * wallPush.x + wallPush.y * wallPush.y);
                        if (pushLen > 1e-4f)
                        {
                            wallPush = { wallPush.x / pushLen, wallPush.y / pushLen };

                            float penetration = radius - worldHitDist;
                            position.x += wallPush.x * penetration;
                            position.z += wallPush.y * penetration;
                            waistPos.x = position.x;
                            waistPos.z = position.z;

                            float vDotN = velocity.x * wallPush.x + velocity.z * wallPush.y;
                            if (vDotN < 0.0f)
                            {
                                velocity.x -= wallPush.x * vDotN;
                                velocity.z -= wallPush.y * vDotN;
                            }
                            collided = true;
                        }
                    }
                }
            }
        }

        return collided;
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
