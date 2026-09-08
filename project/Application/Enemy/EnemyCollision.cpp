#define NOMINMAX
#include "EnemyCollision.h"

#include <algorithm>
#include <cmath>

namespace EnemyCollision
{
    namespace
    {
        inline float Dot(const Vector3& a, const Vector3& b)
        {
            return a.x * b.x + a.y * b.y + a.z * b.z;
        }

        inline float LengthSq(const Vector3& v)
        {
            return v.x * v.x + v.y * v.y + v.z * v.z;
        }

        /// @brief 法線方向の成分を取り除いて床面内のベクトルにする
        inline Vector3 ProjectOnPlane(const Vector3& v, const Vector3& n)
        {
            float d = Dot(v, n);
            return { v.x - n.x * d, v.y - n.y * d, v.z - n.z * d };
        }
    }

    float CalcSlimeRadius(const Vector3& scale, const Vector3& squashStretch, float baseRadius)
    {
        // SlimeCollision::CalculateEffectiveRadius と同じ式（見た目とズレないよう意図的にそろえている）
        float s = (std::abs(scale.x) + std::abs(scale.z)) * 0.5f * baseRadius;
        if (s < 0.001f) s = 0.4f;

        // スライムモデルの実際の外形メッシュ比率
        float meshRadius = s * 0.78f;

        // スクワッシュ変形による横幅の微小変化を反映
        float squashFactor = 1.0f / std::sqrt((std::max)(0.25f, 1.0f + squashStretch.y));
        float dynFactor = std::clamp(squashFactor, 0.85f, 1.35f);

        return meshRadius * dynFactor;
    }

    bool CheckSphereSphere(const Vector3& centerA, float radiusA,
                           const Vector3& centerB, float radiusB,
                           const Vector3& planeNormal,
                           Vector3& outPushDir, float& outDepth)
    {
        Vector3 diff = centerA - centerB;
        float rSum = radiusA + radiusB;

        // まず床面法線方向の距離で足切りする。
        // これをやらないと、上の階層にいる敵とも「真上から見て重なっている」だけで
        // 当たったことになってしまう
        float normalGap = Dot(diff, planeNormal);
        if (std::abs(normalGap) >= rSum)
        {
            return false;
        }

        // 押し出しは床面内で行う（斜面で上下にがたつかないように）
        Vector3 planar = ProjectOnPlane(diff, planeNormal);

        float distSq = LengthSq(planar);
        if (distSq >= rSum * rSum)
        {
            return false;
        }

        float dist = std::sqrt(distSq);
        if (dist > 1e-4f)
        {
            outPushDir = planar * (1.0f / dist);
        }
        else
        {
            // 完全重合。とりあえず +X 方向へ逃がす
            outPushDir = { 1.0f, 0.0f, 0.0f };
        }
        outDepth = rSum - dist;
        return true;
    }

    bool CheckSphereAABB(const Vector3& sphereCenter, float sphereRadius,
                         const Vector3& boxCenter, const Vector3& halfExtents,
                         const Vector3& planeNormal,
                         Vector3& outPushDir, float& outDepth)
    {
        // ボックス内の最近点
        Vector3 closest{
            std::clamp(sphereCenter.x, boxCenter.x - halfExtents.x, boxCenter.x + halfExtents.x),
            std::clamp(sphereCenter.y, boxCenter.y - halfExtents.y, boxCenter.y + halfExtents.y),
            std::clamp(sphereCenter.z, boxCenter.z - halfExtents.z, boxCenter.z + halfExtents.z)
        };

        Vector3 diff = sphereCenter - closest;
        float distSq = LengthSq(diff);

        if (distSq > sphereRadius * sphereRadius)
        {
            return false;
        }

        if (distSq > 1e-8f)
        {
            float dist = std::sqrt(distSq);
            Vector3 dir = diff * (1.0f / dist);

            // 押し出し方向を床面内へ倒す（斜面でも上下に飛び跳ねないように）
            Vector3 planar = ProjectOnPlane(dir, planeNormal);
            float planarLen = std::sqrt(LengthSq(planar));

            // ほぼ真上／真下からの接触（＝敵の頭の上に乗っている）は横に弾かない
            if (planarLen < 0.30f)
            {
                return false;
            }

            outPushDir = planar * (1.0f / planarLen);
            // 面内に倒した分だけ必要な押し出し量は 1/cos 倍になるが、
            // 角に乗ったときに横へ吹っ飛ばないよう、めり込み量そのものを上限にする
            outDepth = (std::min)(sphereRadius, (sphereRadius - dist) / planarLen);
            return true;
        }

        // 中心がボックス内部にある。最も浅い面へ抜く
        Vector3 d{
            halfExtents.x - std::abs(sphereCenter.x - boxCenter.x),
            halfExtents.y - std::abs(sphereCenter.y - boxCenter.y),
            halfExtents.z - std::abs(sphereCenter.z - boxCenter.z)
        };

        // Y 方向は使わない（床にめり込んだまま真上に打ち上げられるのを防ぐ）
        if (d.x < d.z)
        {
            float sign = (sphereCenter.x >= boxCenter.x) ? 1.0f : -1.0f;
            outPushDir = { sign, 0.0f, 0.0f };
            outDepth = d.x + sphereRadius;
        }
        else
        {
            float sign = (sphereCenter.z >= boxCenter.z) ? 1.0f : -1.0f;
            outPushDir = { 0.0f, 0.0f, sign };
            outDepth = d.z + sphereRadius;
        }
        return true;
    }

    bool Check(const SlimeBody& slime, const EnemyBody& enemy,
               const Vector3& planeNormal,
               Vector3& outPushDir, float& outDepth)
    {
        float slimeRadius = CalcSlimeRadius(slime.scale, slime.squashStretch, slime.baseRadius);

        if (enemy.shape == HitShape::AABB)
        {
            return CheckSphereAABB(slime.position, slimeRadius,
                                   enemy.position, enemy.halfExtents,
                                   planeNormal, outPushDir, outDepth);
        }

        return CheckSphereSphere(slime.position, slimeRadius,
                                 enemy.position, enemy.radius,
                                 planeNormal, outPushDir, outDepth);
    }

    HitResult ResolvePlayerVsEnemy(SlimeBody& slime, Vector3& playerVelocity,
                                   const EnemyBody& enemy,
                                   float bounceSpeed,
                                   const Vector3& planeNormal)
    {
        HitResult result;

        Vector3 pushDir{ 0.0f, 0.0f, 0.0f };
        float depth = 0.0f;
        if (!Check(slime, enemy, planeNormal, pushDir, depth))
        {
            return result;
        }

        result.hit = true;
        result.pushDir = pushDir;
        result.depth = depth;
        result.impulse = std::clamp(depth * 2.5f, 0.08f, 1.0f);

        if (slime.strength > enemy.strength)
        {
            // プレイヤーのほうが強い。押し出さずに敵を潰して突き進む
            result.outcome = HitOutcome::EnemyDefeated;
            result.impulse = (std::max)(result.impulse, 0.30f);
            return result;
        }

        // ここから先は押し出しが要る（同格 or 敵のほうが強い）
        float playerShare = enemy.isPushable ? 0.5f : 1.0f;
        slime.position.x += pushDir.x * (depth * playerShare);
        slime.position.y += pushDir.y * (depth * playerShare);
        slime.position.z += pushDir.z * (depth * playerShare);

        if (enemy.isPushable)
        {
            result.enemyPush = pushDir * (-depth * 0.5f);
        }

        if (slime.strength < enemy.strength)
        {
            // 敵のほうが強い。塊を弾き飛ばす
            result.outcome = HitOutcome::PlayerBounced;

            // 強さ差が大きいほど強く飛ぶ（差1で等倍、差が開くと最大1.8倍まで）
            float diff = static_cast<float>(enemy.strength - slime.strength);
            float power = bounceSpeed * std::clamp(0.85f + diff * 0.15f, 0.85f, 1.8f);

            playerVelocity.x = pushDir.x * power;
            playerVelocity.z = pushDir.z * power;
            // ほんの少しだけ浮かせるとロコロコっぽく気持ちいい
            playerVelocity.y = (std::max)(playerVelocity.y, power * 0.28f);

            result.impulse = (std::max)(result.impulse, 0.55f);
        }
        else
        {
            // 同じ強さ。押し合いのみ。壁向きの速度成分だけ削る
            result.outcome = HitOutcome::Standoff;
            float into = playerVelocity.x * (-pushDir.x) + playerVelocity.z * (-pushDir.z);
            if (into > 0.0f)
            {
                playerVelocity.x += pushDir.x * into;
                playerVelocity.z += pushDir.z * into;
            }
        }

        return result;
    }

    bool CheckBulletVsSlime(const Vector3& bulletPos, float bulletRadius,
                            const SlimeBody& slime, Vector3& outPushDir)
    {
        float slimeRadius = CalcSlimeRadius(slime.scale, slime.squashStretch, slime.baseRadius);

        Vector3 diff = slime.position - bulletPos;
        float rSum = slimeRadius + bulletRadius;
        if (LengthSq(diff) >= rSum * rSum)
        {
            return false;
        }

        // ノックバック方向は水平に倒す
        Vector3 horiz{ diff.x, 0.0f, diff.z };
        float len = std::sqrt(LengthSq(horiz));
        outPushDir = (len > 1e-4f) ? Vector3{ horiz.x / len, 0.0f, horiz.z / len }
                                   : Vector3{ 0.0f, 0.0f, 1.0f };
        return true;
    }
}
