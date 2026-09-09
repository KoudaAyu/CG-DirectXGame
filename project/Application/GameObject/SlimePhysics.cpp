#include "SlimePhysics.h"
#include "Baziru3_Engine/Framework/Collision/CollisionManager.h"
#include <vector>

namespace SlimePhysics
{
    static float sFriction = 1.3f; // スライム共通の地面摩擦係数（通常・合体・ミニオン共通）

    // 歩行可能床と壁の排他分離境界（傾斜角約56.6度: これ未満の急峻な面を壁として押し出し、
    // これ以上の緩やかな面を歩行可能床として判定。床側と壁側で同じ定数を使うことで
    // 「どちらでもない」「どちらでもある」隙間が生まれないようにしている）
    static constexpr float kWalkableSlopeLimitNy = 0.55f;

    /// @brief 登録された地形メッシュ1枚分の情報
    /// @note 地形は複数メッシュに分割されることがあるので配列で保持する。
    ///       すべてのメッシュは GamePlayScene から同じピボット回転を掛けられている前提。
    struct GroundMeshEntry
    {
        Object3d* object = nullptr;
        MeshCollider* collider = nullptr;

        // 地面メッシュのフレーム追従用トランスフォーム履歴
        Matrix4x4 prevWorld{};
        Matrix4x4 currWorld{};
        Matrix4x4 invPrevWorld{};
        bool hasPrev = false;
    };

    static std::vector<GroundMeshEntry> sGroundMeshes;
    static uint32_t sLastFrameCount = 0xFFFFFFFF;

    // 地形全体のワールド AABB（フレームごとに1回だけ再計算する）
    static Vector3 sBoundsMin{ 0.0f, 0.0f, 0.0f };
    static Vector3 sBoundsMax{ 0.0f, 0.0f, 0.0f };
    static bool sBoundsValid = false;
    static uint32_t sBoundsFrame = 0xFFFFFFFF;

    float GetFriction()
    {
        return sFriction;
    }

    void SetFriction(float friction)
    {
        sFriction = friction;
    }

    void AddGroundMesh(Object3d* groundObject, MeshCollider* groundCollider)
    {
        if (!groundObject || !groundCollider) return;

        // 二重登録は無視（同じメッシュを2回撃つと床候補が重複する）
        for (const auto& gm : sGroundMeshes)
        {
            if (gm.object == groundObject) return;
        }

        GroundMeshEntry entry;
        entry.object = groundObject;
        entry.collider = groundCollider;
        sGroundMeshes.push_back(entry);

        sLastFrameCount = 0xFFFFFFFF;
        sBoundsFrame = 0xFFFFFFFF;
        sBoundsValid = false;
    }

    void SetGroundMesh(Object3d* groundObject, MeshCollider* groundCollider)
    {
        ClearGroundMeshes();
        AddGroundMesh(groundObject, groundCollider);
    }

    void ClearGroundMeshes()
    {
        sGroundMeshes.clear();
        sLastFrameCount = 0xFFFFFFFF;
        sBoundsFrame = 0xFFFFFFFF;
        sBoundsValid = false;
    }

    void ClearGroundMesh()
    {
        ClearGroundMeshes();
    }

    int GetGroundMeshCount()
    {
        return static_cast<int>(sGroundMeshes.size());
    }

    int FindGroundMeshIndex(const Object3d* groundObject)
    {
        if (!groundObject) return -1;
        for (size_t i = 0; i < sGroundMeshes.size(); ++i)
        {
            if (sGroundMeshes[i].object == groundObject) return static_cast<int>(i);
        }
        return -1;
    }

    // ------------------------------------------------------------------
    // 内部ヘルパー
    // ------------------------------------------------------------------

    static inline Vector3 TransformPointM(const Vector3& p, const Matrix4x4& m)
    {
        return {
            p.x * m.m[0][0] + p.y * m.m[1][0] + p.z * m.m[2][0] + m.m[3][0],
            p.x * m.m[0][1] + p.y * m.m[1][1] + p.z * m.m[2][1] + m.m[3][1],
            p.x * m.m[0][2] + p.y * m.m[1][2] + p.z * m.m[2][2] + m.m[3][2]
        };
    }

    static inline Vector3 TransformDirM(const Vector3& d, const Matrix4x4& m)
    {
        return {
            d.x * m.m[0][0] + d.y * m.m[1][0] + d.z * m.m[2][0],
            d.x * m.m[0][1] + d.y * m.m[1][1] + d.z * m.m[2][1],
            d.x * m.m[0][2] + d.y * m.m[1][2] + d.z * m.m[2][2]
        };
    }

    static inline Vector3 NormalizeSafeV3(const Vector3& v)
    {
        float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        return (len > 1e-6f) ? v * (1.0f / len) : Vector3{ 0.0f, 1.0f, 0.0f };
    }

    /// @brief 各メッシュのワールド行列を1フレームに1回だけ更新する
    /// @note 同フレーム内で何度呼ばれても更新は1回。前フレームの行列は
    ///       「傾斜追従で床がどれだけ持ち上がったか」の逆算に使う
    static void UpdateGroundMatrices()
    {
        uint32_t currentFrame = CollisionManager::GetInstance()->GetFrameCount();
        if (currentFrame == sLastFrameCount) return;

        for (auto& gm : sGroundMeshes)
        {
            if (!gm.object) continue;

            if (gm.hasPrev)
            {
                gm.prevWorld = gm.currWorld;
                gm.invPrevWorld = Inverse(gm.prevWorld);
            }
            gm.currWorld = gm.object->GetWorldMatrix();
            if (!gm.hasPrev)
            {
                gm.prevWorld = gm.currWorld;
                gm.invPrevWorld = Inverse(gm.prevWorld);
                gm.hasPrev = true;
            }
        }
        sLastFrameCount = currentFrame;
    }

    bool GetGroundWorldBounds(Vector3& outMin, Vector3& outMax)
    {
        if (sGroundMeshes.empty()) return false;

        UpdateGroundMatrices();

        uint32_t currentFrame = CollisionManager::GetInstance()->GetFrameCount();
        if (sBoundsFrame == currentFrame && sBoundsValid)
        {
            outMin = sBoundsMin;
            outMax = sBoundsMax;
            return true;
        }

        bool any = false;
        Vector3 mn{ 0.0f, 0.0f, 0.0f };
        Vector3 mx{ 0.0f, 0.0f, 0.0f };

        for (const auto& gm : sGroundMeshes)
        {
            if (!gm.object || !gm.collider) continue;

            Vector3 lmin, lmax;
            if (!gm.collider->GetAABBTree().GetRootBounds(lmin, lmax)) continue;

            // ローカル AABB の8頂点をワールドへ変換して包み直す（回転しているため）
            for (int i = 0; i < 8; ++i)
            {
                Vector3 corner{
                    (i & 1) ? lmax.x : lmin.x,
                    (i & 2) ? lmax.y : lmin.y,
                    (i & 4) ? lmax.z : lmin.z
                };
                Vector3 w = TransformPointM(corner, gm.currWorld);
                if (!any)
                {
                    mn = w;
                    mx = w;
                    any = true;
                }
                else
                {
                    mn.x = (std::min)(mn.x, w.x); mn.y = (std::min)(mn.y, w.y); mn.z = (std::min)(mn.z, w.z);
                    mx.x = (std::max)(mx.x, w.x); mx.y = (std::max)(mx.y, w.y); mx.z = (std::max)(mx.z, w.z);
                }
            }
        }

        if (!any) return false;

        sBoundsMin = mn;
        sBoundsMax = mx;
        sBoundsValid = true;
        sBoundsFrame = currentFrame;

        outMin = mn;
        outMax = mx;
        return true;
    }


    /// @brief (x, z) の真下にある「歩ける床」を全メッシュから集めて Y 降順で返す
    /// @note 旧実装は 250.0f * maxScale というマジックナンバーで最上空を決めていたが、
    ///       地形を差し替えて背が高くなるとレイの開始点が地形の中に入って床を取り逃がす。
    ///       ここでは AABB ツリーの実測バウンズから開始高さを決めている。
    ///       1点だけのレイキャストだとポリゴンの継ぎ目（シーム）をすり抜けることがあるので、
    ///       空振りしたときだけ十字に微小ジッターをかけて拾い直す
    static void CollectGroundCandidates(float x, float z, std::vector<GroundLayer>& out)
    {
        out.clear();
        if (sGroundMeshes.empty()) return;

        UpdateGroundMatrices();

        Vector3 bmin, bmax;
        float topY = 0.0f;
        if (GetGroundWorldBounds(bmin, bmax))
        {
            topY = bmax.y + 10.0f;
        }
        else
        {
            return;
        }

        const int kMaxPenetrations = 32;
        const float kMaxRayDist = 100000.0f;

        auto PerformRaycastAt = [&](float rayX, float rayZ)
        {
            for (size_t meshIdx = 0; meshIdx < sGroundMeshes.size(); ++meshIdx)
            {
                const GroundMeshEntry& gm = sGroundMeshes[meshIdx];
                if (!gm.object || !gm.collider) continue;

                const Matrix4x4& worldMatrix = gm.currWorld;
                Matrix4x4 invWorld = Inverse(worldMatrix);

                Vector3 rayStartWorld = { rayX, topY, rayZ };
                const Vector3 rayDirWorld = { 0.0f, -1.0f, 0.0f };
                float lastHitWorldY = 1e9f;

                for (int iter = 0; iter < kMaxPenetrations; ++iter)
                {
                    Vector3 localStart = TransformPointM(rayStartWorld, invWorld);
                    Vector3 localDir = TransformDirM(rayDirWorld, invWorld);

                    float dirLen = std::sqrt(localDir.x * localDir.x + localDir.y * localDir.y + localDir.z * localDir.z);
                    if (dirLen < 1e-6f) break;
                    localDir = localDir * (1.0f / dirLen);

                    float hitDist = 0.0f;
                    Vector3 hitNormal, v0, v1, v2;

                    if (!gm.collider->GetAABBTree().Raycast(localStart, localDir, kMaxRayDist, hitDist, hitNormal, v0, v1, v2))
                    {
                        break; // これ以上下にメッシュが存在しない
                    }

                    // ヒットしたポリゴンの幾何法線を算出
                    Vector3 e1 = v1 - v0;
                    Vector3 e2 = v2 - v0;
                    Vector3 localTriNorm = NormalizeSafeV3({
                        e1.y * e2.z - e1.z * e2.y,
                        e1.z * e2.x - e1.x * e2.z,
                        e1.x * e2.y - e1.y * e2.x
                    });
                    Vector3 worldTriNorm = NormalizeSafeV3(TransformDirM(localTriNorm, worldMatrix));

                    Vector3 localHit = {
                        localStart.x + localDir.x * hitDist,
                        localStart.y + localDir.y * hitDist,
                        localStart.z + localDir.z * hitDist
                    };
                    float worldY = localHit.x * worldMatrix.m[0][1] + localHit.y * worldMatrix.m[1][1] + localHit.z * worldMatrix.m[2][1] + worldMatrix.m[3][1];

                    // 同一ポリゴンや極小オフセットによる重複ヒットを防止
                    if (worldY < lastHitWorldY - 0.005f)
                    {
                        lastHitWorldY = worldY;

                        // 歩行可能地面ポリゴンか判定:
                        // 最大登坂限界（kWalkableSlopeLimitNy）以上の緩やかな面のみを地面候補として収集。
                        // 壁判定（localTriNorm.y < kWalkableSlopeLimitNy）と完全排他なので、
                        // 同じ面が「床でも壁でもある」状態にならずジッターが出ない
                        if (localTriNorm.y >= kWalkableSlopeLimitNy && worldTriNorm.y > 0.10f)
                        {
                            out.push_back({ worldY, worldTriNorm, static_cast<int>(meshIdx) });
                        }
                    }

                    rayStartWorld.y = worldY - 0.05f;
                }
            }
        };

        PerformRaycastAt(x, z);

        // ポリゴン同士の継ぎ目（シーム）でレイがすり抜けた場合のフォールバック（十字微小ジッター探索）
        if (out.empty())
        {
            const float jitter = 0.035f;
            PerformRaycastAt(x + jitter, z);
            if (out.empty()) PerformRaycastAt(x - jitter, z);
            if (out.empty()) PerformRaycastAt(x, z + jitter);
            if (out.empty()) PerformRaycastAt(x, z - jitter);
        }

        if (out.size() > 1)
        {
            // 複数メッシュ分をまとめて Y 降順に並べ直す
            std::sort(out.begin(), out.end(),
                      [](const GroundLayer& a, const GroundLayer& b) { return a.y > b.y; });

            // メッシュ同士が接している継ぎ目で床が二重に出るのを潰す
            out.erase(std::unique(out.begin(), out.end(),
                                  [](const GroundLayer& a, const GroundLayer& b) { return std::abs(a.y - b.y) < 0.02f; }),
                      out.end());
        }
    }

    int QueryGroundLayers(float x, float z, GroundLayer* outLayers, int maxLayers)
    {
        static std::vector<GroundLayer> candidates;
        CollectGroundCandidates(x, z, candidates);

        int count = static_cast<int>(candidates.size());
        if (outLayers && maxLayers > 0)
        {
            int n = (std::min)(count, maxLayers);
            for (int i = 0; i < n; ++i) outLayers[i] = candidates[i];
        }
        return count;
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
            *outNormal = NormalizeSafeV3(defaultNorm);
        }

        // 1. 地面メッシュが登録されている場合、AABBTree による多層対応レイキャストで精密メッシュ表面を判定
        if (!sGroundMeshes.empty())
        {
            static std::vector<GroundLayer> groundCandidates;
            CollectGroundCandidates(x, z, groundCandidates);

            if (groundCandidates.empty())
            {
                // 地面が見つからない（完全に島の外の奈落）
                if (outHasGround) *outHasGround = false;
                return (currentY != kIgnoreCurrentY) ? currentY : 0.0f;
            }

            // CollectGroundCandidates() の時点で Y 降順に並んでいる

            // 1. currentY が未指定の場合（カメラや照準など最上面を取得したい場合）
            if (currentY == kIgnoreCurrentY)
            {
                if (outHasGround) *outHasGround = true;
                if (outNormal) *outNormal = groundCandidates[0].normal;
                return groundCandidates[0].y;
            }

            // 2. 接地中（isGrounded == true）の場合:
            // ステージ傾斜・揺らしの追従変位を反映しつつ、段差・坂道を適切に追従
            if (isGrounded)
            {
                float deltaYTilt = 0.0f;

                // 地形メッシュはすべて同じピボット回転を掛けられている前提なので、
                // 傾斜による床の持ち上がり量は先頭メッシュの行列から代表して求める
                const GroundMeshEntry& primary = sGroundMeshes[0];
                if (primary.hasPrev)
                {
                    Vector3 localPt = TransformPointM({ x, currentY, z }, primary.invPrevWorld);
                    float newWorldY = localPt.x * primary.currWorld.m[0][1] + localPt.y * primary.currWorld.m[1][1] + localPt.z * primary.currWorld.m[2][1] + primary.currWorld.m[3][1];
                    deltaYTilt = newWorldY - currentY;
                }

                float expectedFloorY = currentY + deltaYTilt;
                // 自力登坂・段差許容マージン（頭上の天井を誤認しないよう最大0.40mまでに厳格制限）
                float stepMargin = (std::min)(0.40f, (std::max)(0.15f, baseOffset * 0.40f));
                float maxAllowedFloorY = expectedFloorY + stepMargin;

                // 現在位置（足元）に最も近い床候補を探索（上空の天井はスキップ）
                int bestIdx = -1;
                float bestDist = 1e9f;
                for (size_t i = 0; i < groundCandidates.size(); ++i)
                {
                    if (groundCandidates[i].y <= maxAllowedFloorY)
                    {
                        float dist = std::abs(groundCandidates[i].y - expectedFloorY);
                        if (dist < bestDist)
                        {
                            bestDist = dist;
                            bestIdx = static_cast<int>(i);
                        }
                    }
                }

                // めり込み復帰救済:
                // 合体や激突で足元が一時的に床下にめり込んでいる場合（全床候補が expectedFloorY + stepMargin より上にある）
                if (bestIdx == -1 && !groundCandidates.empty())
                {
                    float ceilingY = 0.0f;
                    float maxRecoveryY = expectedFloorY + (std::max)(3.0f, baseOffset * 2.5f);
                    if (FindCeilingY(x, z, expectedFloorY, baseOffset * 3.0f, ceilingY))
                    {
                        maxRecoveryY = (std::min)(maxRecoveryY, ceilingY - 0.10f);
                    }

                    for (int i = static_cast<int>(groundCandidates.size()) - 1; i >= 0; --i)
                    {
                        if (groundCandidates[i].y <= maxRecoveryY)
                        {
                            bestIdx = i;
                            break;
                        }
                    }
                }

                if (bestIdx != -1)
                {
                    if (outHasGround) *outHasGround = true;
                    if (outNormal) *outNormal = groundCandidates[bestIdx].normal;
                    return groundCandidates[bestIdx].y;
                }

                if (outHasGround) *outHasGround = false;
                return expectedFloorY;
            }

            // 3. 空中・落下中（isGrounded == false）の場合:
            // スライムの足元以下にある床候補の中で最も高いもの（直下の床）を着地面として選定
            // ※ 頭上の天井に着地するのを絶対に防ぐため、currentY + 0.15f 以下に厳格制限
            float maxAllowedLandingFloorY = currentY + 0.15f;

            int bestIdx = -1;
            for (size_t i = 0; i < groundCandidates.size(); ++i)
            {
                if (groundCandidates[i].y <= maxAllowedLandingFloorY)
                {
                    bestIdx = static_cast<int>(i); // 降順ソートなので最初に見つかったものが直下の最上床
                    break;
                }
            }

            // 落下中のめり込み着地救済（高速落下・合体直後のすり抜け完全防止）:
            if (bestIdx == -1 && !groundCandidates.empty())
            {
                float ceilingY = 0.0f;
                float maxRecoveryY = currentY + (std::max)(3.0f, baseOffset * 2.5f);
                if (FindCeilingY(x, z, currentY, baseOffset * 3.0f, ceilingY))
                {
                    maxRecoveryY = (std::min)(maxRecoveryY, ceilingY - 0.10f);
                }
                for (int i = static_cast<int>(groundCandidates.size()) - 1; i >= 0; --i)
                {
                    if (groundCandidates[i].y <= maxRecoveryY)
                    {
                        bestIdx = i;
                        break;
                    }
                }
            }

            if (bestIdx != -1)
            {
                if (outHasGround) *outHasGround = true;
                if (outNormal) *outNormal = groundCandidates[bestIdx].normal;
                return groundCandidates[bestIdx].y;
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
            float denom = d1 - d3;
            float v = (std::abs(denom) > 1e-7f) ? (d1 / denom) : 0.0f;
            return a + ab * v;
        }

        Vector3 cp = p - c;
        float d5 = DotV3(ab, cp);
        float d6 = DotV3(ac, cp);
        if (d6 >= 0.0f && d5 <= d6) return c;

        float vb = d5 * d2 - d1 * d6;
        if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
            float denom = d2 - d6;
            float w = (std::abs(denom) > 1e-7f) ? (d2 / denom) : 0.0f;
            return a + ac * w;
        }

        float va = d3 * d6 - d5 * d4;
        if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
            float denom = (d4 - d3) + (d5 - d6);
            float w = (std::abs(denom) > 1e-7f) ? ((d4 - d3) / denom) : 0.0f;
            return b + (c - b) * w;
        }

        float denomSum = va + vb + vc;
        if (std::abs(denomSum) < 1e-7f) {
            // 縮退三角形（面積0・同一直線上の頂点等）の場合は三角形の重心を安全に返す
            return (a + b + c) * (1.0f / 3.0f);
        }

        float denom = 1.0f / denomSum;
        float v = vb * denom;
        float w = vc * denom;
        return a + ab * v + ac * w;
    }

    bool ResolveWallCollision(Vector3& position, Vector3& velocity, float radius, float heightOffset, const Vector3* prevPos)
    {
        if (sGroundMeshes.empty()) return false;

        UpdateGroundMatrices();

        auto TransformPt = [](const Vector3& p, const Matrix4x4& m) -> Vector3 {
            return {
                p.x * m.m[0][0] + p.y * m.m[1][0] + p.z * m.m[2][0] + m.m[3][0],
                p.x * m.m[0][1] + p.y * m.m[1][1] + p.z * m.m[2][1] + m.m[3][1],
                p.x * m.m[0][2] + p.y * m.m[1][2] + p.z * m.m[2][2] + m.m[3][2]
            };
        };

        bool anyCollided = false;

        // =========================================================================
        // Phase 1: 連続衝突判定（CCD: Continuous Collision Detection）
        // 前フレーム位置（prevPos）から現在位置（position）への移動経路をスイープ探査し、
        // 薄い壁や高速移動（吹き飛び・落下）による1フレームでの壁すり抜けを完全防止
        // =========================================================================
        if (prevPos)
        {
            Vector3 moveWorld = position - *prevPos;
            float moveDistWorld = std::sqrt(moveWorld.x * moveWorld.x + moveWorld.y * moveWorld.y + moveWorld.z * moveWorld.z);

            if (moveDistWorld > 1e-4f)
            {
                Vector3 dirWorld = moveWorld * (1.0f / moveDistWorld);

                // 横幅（左右）および高さをカバーする4本の探査レイ（中心、左右、上方）
                Vector3 perpHoriz = { -dirWorld.z, 0.0f, dirWorld.x };
                float perpLen = std::sqrt(perpHoriz.x * perpHoriz.x + perpHoriz.z * perpHoriz.z);
                if (perpLen > 1e-4f)
                {
                    perpHoriz = perpHoriz * ((radius * 0.55f) / perpLen);
                }
                else
                {
                    perpHoriz = { radius * 0.55f, 0.0f, 0.0f };
                }

                Vector3 sweepOrigins[4] = {
                    { prevPos->x, prevPos->y + heightOffset, prevPos->z },
                    { prevPos->x + perpHoriz.x, prevPos->y + heightOffset, prevPos->z + perpHoriz.z },
                    { prevPos->x - perpHoriz.x, prevPos->y + heightOffset, prevPos->z - perpHoriz.z },
                    { prevPos->x, prevPos->y + heightOffset + radius * 0.35f, prevPos->z }
                };

                float minEarliestHit = moveDistWorld + radius * 0.95f;
                Vector3 bestHitWallNorm{ 0.0f, 0.0f, 0.0f };
                bool hitSweepWall = false;

                for (const auto& gm : sGroundMeshes)
                {
                    if (!gm.object || !gm.collider) continue;
                    const Matrix4x4& worldMatrix = gm.currWorld;
                    Matrix4x4 invWorld = Inverse(worldMatrix);

                    for (const auto& rayStartWorld : sweepOrigins)
                    {
                        Vector3 localStart = TransformPt(rayStartWorld, invWorld);
                        Vector3 localDir = {
                            dirWorld.x * invWorld.m[0][0] + dirWorld.y * invWorld.m[1][0] + dirWorld.z * invWorld.m[2][0],
                            dirWorld.x * invWorld.m[0][1] + dirWorld.y * invWorld.m[1][1] + dirWorld.z * invWorld.m[2][1],
                            dirWorld.x * invWorld.m[0][2] + dirWorld.y * invWorld.m[1][2] + dirWorld.z * invWorld.m[2][2]
                        };
                        float localDirLen = std::sqrt(localDir.x * localDir.x + localDir.y * localDir.y + localDir.z * localDir.z);
                        if (localDirLen < 1e-6f) continue;
                        localDir = localDir * (1.0f / localDirLen);

                        float sweepMaxDistWorld = moveDistWorld + radius * 0.95f;
                        float maxLocalDist = sweepMaxDistWorld * localDirLen;
                        float hitDist = 0.0f;
                        Vector3 hitNormal, v0, v1, v2;

                        if (gm.collider->GetAABBTree().Raycast(localStart, localDir, maxLocalDist, hitDist, hitNormal, v0, v1, v2))
                        {
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

                            // 壁ポリゴン（kWalkableSlopeLimitNy 未満の急斜面・垂直壁）のみを対象
                            if (localTriNorm.y < kWalkableSlopeLimitNy)
                            {
                                float worldHitDist = hitDist / localDirLen;

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
                                if (worldTriNorm.x * dirWorld.x + worldTriNorm.y * dirWorld.y + worldTriNorm.z * dirWorld.z > 0.0f)
                                {
                                    worldTriNorm = worldTriNorm * -1.0f;
                                }

                                if (worldHitDist < minEarliestHit)
                                {
                                    minEarliestHit = worldHitDist;
                                    bestHitWallNorm = worldTriNorm;
                                    hitSweepWall = true;
                                }
                            }
                        }
                    }
                }

                if (hitSweepWall)
                {
                    // 壁手前にクランプ（安全停止位置）
                    float safeAdvance = (std::max)(0.0f, minEarliestHit - radius * 0.95f);
                    position = *prevPos + dirWorld * safeAdvance;

                    // 壁法線方向への微小オフセット
                    position += bestHitWallNorm * 0.02f;

                    // 壁に向かう速度成分を除去
                    float vDotN = velocity.x * bestHitWallNorm.x + velocity.y * bestHitWallNorm.y + velocity.z * bestHitWallNorm.z;
                    if (vDotN < 0.0f)
                    {
                        velocity.x -= bestHitWallNorm.x * vDotN;
                        velocity.y -= bestHitWallNorm.y * vDotN;
                        velocity.z -= bestHitWallNorm.z * vDotN;
                    }
                    anyCollided = true;
                }
            }
        }

        // =========================================================================
        // Phase 2: 離散近接・めり込み押し出し（多方向探査と反復緩和）
        // =========================================================================
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

            // 探索範囲: radius + マージン（深めり込み時でも確実に捕捉）
            const float checkDist = radius + (std::max)(0.45f, radius * 0.6f);

            float maxPenetration = 0.0f;
            Vector2 bestPushDir = { 0.0f, 0.0f };

            // スライムの体積（中心とやや上方）を捉えるため2つの高さで探査
            float yOffsets[] = { 0.0f, radius * 0.20f };

            for (const auto& gm : sGroundMeshes)
            {
                if (!gm.object || !gm.collider) continue;
                const Matrix4x4& worldMatrix = gm.currWorld;
                Matrix4x4 invWorld = Inverse(worldMatrix);

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

                        if (gm.collider->GetAABBTree().Raycast(localStart, localDir, maxLocalDist, hitDist, hitNormal, v0, v1, v2))
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

                            // 壁判定: 急峻な面のみ壁として押し出す（localTriNorm.y < kWalkableSlopeLimitNy）
                            if (localTriNorm.y < kWalkableSlopeLimitNy)
                            {
                                Vector3 wV0 = TransformPt(v0, worldMatrix);
                                Vector3 wV1 = TransformPt(v1, worldMatrix);
                                Vector3 wV2 = TransformPt(v2, worldMatrix);

                                // スライム中心（探査点）から三角形への最近接点 Q を厳密に計算（面・エッジ・頂点すべて対応）
                                Vector3 probePos = { waistPos.x, waistPos.y + yOff, waistPos.z };
                                Vector3 closestQ = ClosestPointOnTriangle(probePos, wV0, wV1, wV2);

                                // 探査点と最近接点の3D差分ベクトル D = probePos - closestQ
                                Vector3 diff3D = probePos - closestQ;
                                float dist3DSq = diff3D.x * diff3D.x + diff3D.y * diff3D.y + diff3D.z * diff3D.z;

                                // スライム球体（半径 radius）との真の3D幾何学的交差判定（signedDist を完全廃止し最短距離で判定）
                                if (dist3DSq < radius * radius)
                                {
                                    float dist3D = std::sqrt(dist3DSq);
                                    float penetration = radius - dist3D; // 面・エッジ・頂点からの真のめり込み深さ

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

                                    // 水平押し出し方向の算出（最近接点から離れる水平ベクトル）
                                    Vector2 pushDir = { 0.0f, 0.0f };
                                    Vector2 diffXZ = { diff3D.x, diff3D.z };
                                    float lenXZ = std::sqrt(diffXZ.x * diffXZ.x + diffXZ.y * diffXZ.y);

                                    if (lenXZ > 1e-4f)
                                    {
                                        pushDir = { diffXZ.x / lenXZ, diffXZ.y / lenXZ };
                                    }
                                    else
                                    {
                                        // 水平オフセットが極小の場合、壁ポリゴンの水平法線方向へ退避
                                        float normXZLen = std::sqrt(worldTriNorm.x * worldTriNorm.x + worldTriNorm.z * worldTriNorm.z);
                                        if (normXZLen > 1e-4f)
                                        {
                                            pushDir = { worldTriNorm.x / normXZLen, worldTriNorm.z / normXZLen };
                                        }
                                        else
                                        {
                                            continue;
                                        }
                                    }

                                    // 三角形の裏面に入り込んでいる場合（法線と逆側にいる場合）のフェイルセーフ:
                                    // 押し出し方向がポリゴン法線と逆を向いていたら、表面向きに補正
                                    float dotWithNormal = pushDir.x * worldTriNorm.x + pushDir.y * worldTriNorm.z;
                                    if (dotWithNormal < 0.0f)
                                    {
                                        pushDir.x = -pushDir.x;
                                        pushDir.y = -pushDir.y;
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

    float GetVoidY(float margin)
    {
        Vector3 bmin, bmax;
        if (GetGroundWorldBounds(bmin, bmax))
        {
            return bmin.y - margin;
        }
        // 地形が未登録のときだけ従来の決め打ちに落とす
        return -12.0f;
    }

    bool FindCeilingY(float x, float z, float startY, float maxSearchDist, float& outCeilingY)
    {
        if (sGroundMeshes.empty()) return false;

        UpdateGroundMatrices();

        float nearestCeilingY = startY + maxSearchDist;
        bool found = false;

        for (const auto& gm : sGroundMeshes)
        {
            if (!gm.object || !gm.collider) continue;

            const Matrix4x4& worldMatrix = gm.currWorld;
            Matrix4x4 invWorld = Inverse(worldMatrix);

            float currentRayY = startY + 0.05f;
            float searchLimitY = startY + maxSearchDist;

            for (int iter = 0; iter < 16; ++iter)
            {
                if (currentRayY >= searchLimitY) break;

                Vector3 rayOriginWorld = { x, currentRayY, z };
                Vector3 rayDirWorld = { 0.0f, 1.0f, 0.0f }; // 真上向き

                Vector3 localStart = {
                    rayOriginWorld.x * invWorld.m[0][0] + rayOriginWorld.y * invWorld.m[1][0] + rayOriginWorld.z * invWorld.m[2][0] + invWorld.m[3][0],
                    rayOriginWorld.x * invWorld.m[0][1] + rayOriginWorld.y * invWorld.m[1][1] + rayOriginWorld.z * invWorld.m[2][1] + invWorld.m[3][1],
                    rayOriginWorld.x * invWorld.m[0][2] + rayOriginWorld.y * invWorld.m[1][2] + rayOriginWorld.z * invWorld.m[2][2] + invWorld.m[3][2]
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
                float remDist = searchLimitY - currentRayY;
                if (!gm.collider->GetAABBTree().Raycast(localStart, localDir, remDist * 2.5f, hitDist, hitNormal, v0, v1, v2))
                {
                    break; // これ以上上にメッシュが存在しない
                }

                Vector3 localHit = {
                    localStart.x + localDir.x * hitDist,
                    localStart.y + localDir.y * hitDist,
                    localStart.z + localDir.z * hitDist
                };
                float worldY = localHit.x * worldMatrix.m[0][1] + localHit.y * worldMatrix.m[1][1] + localHit.z * worldMatrix.m[2][1] + worldMatrix.m[3][1];

                // startY より少なくとも 0.10m 以上上にある遮蔽面を天井として検出
                if (worldY > startY + 0.10f && worldY < nearestCeilingY)
                {
                    nearestCeilingY = worldY;
                    found = true;
                }

                // 次の貫通探索のため、ヒット地点より上（0.04m）からレイを再開
                currentRayY = worldY + 0.04f;
            }
        }

        if (found)
        {
            outCeilingY = nearestCeilingY;
            return true;
        }
        return false;
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
