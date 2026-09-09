#define NOMINMAX
#include "MobEnemy.h"
#include "Application/GameObject/SlimePhysics.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace
{
    constexpr float kPi = 3.14159265358979323846f;

    /// @brief 種類ごとの設定テーブル（実機で見ながら ImGui で詰める前提の初期値）
    MobEnemyConfig MakeDefaultConfigs(EnemyType type)
    {
        MobEnemyConfig c;

        switch (type)
        {
        case EnemyType::Slime:
            c.typeName = "Slime";
            c.directory = "Resources/Enemy/Slime";
            c.fileName = "slime.gltf";
            // モデル素の大きさ: 4.21 x 2.36 x 1.50 (min.y = -0.72)
            c.modelScale = 0.50f;
            c.groundOffsetRatio = 0.72f;
            c.hitShape = EnemyCollision::HitShape::Sphere;
            c.hitRadiusRatio = 1.20f;
            c.hitHalfRatio = { 1.40f, 0.90f, 0.70f };
            c.hitOffsetRatio = 0.46f;
            c.canMove = true;
            c.canShoot = false;
            c.isPushable = true;
            c.preventFall = true;
            c.moveSpeed = 2.6f;
            c.chaseRange = 9.0f;
            c.loseRange = 14.0f;
            c.keepDistance = 0.5f;
            c.hopHeight = 0.16f;
            c.hopSpeed = 8.0f;
            c.idleWobble = 0.06f;
            c.strengthMin = 3;
            c.strengthMax = 20;
            c.clipIdle = "wait";
            c.clipWalk = "walk";
            c.clipAttack = "attack";
            c.clipAlert = "discovery";
            break;

        case EnemyType::FlowerClover:
            c.typeName = "FlowerClover";
            c.directory = "Resources/Enemy/FlowerClover";
            c.fileName = "FlowerClover.gltf";
            // モデル素の大きさ: 0.81 x 2.45 x 2.47 (min.y = -0.18)
            c.modelScale = 0.90f;
            c.groundOffsetRatio = 0.18f;
            c.hitShape = EnemyCollision::HitShape::AABB;
            c.hitRadiusRatio = 0.70f;
            c.hitHalfRatio = { 0.55f, 1.10f, 0.60f };
            c.hitOffsetRatio = 0.90f;
            c.canMove = false;
            c.canShoot = false;
            c.isPushable = false;
            c.turnSpeed = 3.0f;
            c.hopHeight = 0.0f;
            c.idleWobble = 0.045f;
            c.strengthMin = 5;
            c.strengthMax = 15;
            c.clipIdle = "Idle";
            c.clipWalk = "";          // 地面に固定なので移動クリップは使わない
            c.clipAttack = "Attack";
            c.clipAlert = "Attack_Wait";
            break;

        case EnemyType::FlowerLotus:
            c.typeName = "FlowerLotus";
            c.directory = "Resources/Enemy/FlowerLotus";
            c.fileName = "FlowerLotus.gltf";
            // モデル素の大きさ: 2.42 x 2.65 x 2.74 (min.y = -0.07)
            c.modelScale = 0.95f;
            c.groundOffsetRatio = 0.07f;
            c.hitShape = EnemyCollision::HitShape::Sphere;
            c.hitRadiusRatio = 0.75f;
            c.hitHalfRatio = { 0.70f, 1.20f, 0.70f };
            c.hitOffsetRatio = 1.00f;
            c.canMove = true;
            c.canShoot = true;
            c.isPushable = true;
            c.preventFall = true;
            c.moveSpeed = 1.5f;
            c.chaseRange = 10.0f;
            c.loseRange = 15.0f;
            c.keepDistance = 3.0f;   // 撃ちたいので近づきすぎない
            c.hopHeight = 0.09f;
            c.hopSpeed = 5.0f;
            c.idleWobble = 0.05f;
            c.shootRange = 11.0f;
            c.shootInterval = 2.2f;
            c.bulletSpeed = 9.0f;
            c.bulletScale = 0.62f;   // 俯瞰カメラ（約30m）でも粒として見えるサイズ
            c.bulletHitRadius = 0.42f;
            c.muzzleHeightRatio = 1.10f;
            c.bulletDirectory = "Resources/Enemy/FlowerLotus";
            c.bulletFileName = "Lotus_Bullet.gltf";
            c.bulletColor = { 1.0f, 0.55f, 0.85f, 1.0f };
            c.strengthMin = 5;
            c.strengthMax = 15;
            c.clipIdle = "Idle";
            c.clipWalk = "Walk";
            c.clipAttack = "Attack";
            c.clipAlert = "Attack_Wait";
            break;

        case EnemyType::FlowerSunward:
            c.typeName = "FlowerSunward";
            c.directory = "Resources/Enemy/FlowerSunward";
            c.fileName = "FlowerSunward.gltf";
            // モデル素の大きさ: 2.32 x 3.17 x 2.85 (min.y = -0.18)
            c.modelScale = 0.95f;
            c.groundOffsetRatio = 0.18f;
            c.hitShape = EnemyCollision::HitShape::AABB;
            c.hitRadiusRatio = 0.75f;
            c.hitHalfRatio = { 0.65f, 1.45f, 0.65f };
            c.hitOffsetRatio = 1.20f;
            c.canMove = false;
            c.canShoot = true;
            c.isPushable = false;
            c.turnSpeed = 3.5f;
            c.hopHeight = 0.0f;
            c.idleWobble = 0.04f;
            c.shootRange = 13.0f;
            c.shootInterval = 1.9f;
            c.bulletSpeed = 11.0f;
            c.bulletScale = 0.66f;   // 同上
            c.bulletHitRadius = 0.45f;
            c.muzzleHeightRatio = 1.60f;
            c.bulletDirectory = "Resources/Enemy/FlowerSunward";
            c.bulletFileName = "Sunward_Bullet.gltf";
            c.bulletColor = { 1.0f, 0.85f, 0.30f, 1.0f };
            c.strengthMin = 5;
            c.strengthMax = 15;
            c.clipIdle = "Idle";
            c.clipWalk = "";
            c.clipAttack = "Attack";
            c.clipAlert = "Attack_Wait";
            // "Death" クリップもあるが、今の仕様は撃破＝即消滅なので使っていない
            break;

        default:
            break;
        }

        return c;
    }

    MobEnemyConfig g_configs[static_cast<size_t>(EnemyType::Count)] = {
        MakeDefaultConfigs(EnemyType::Slime),
        MakeDefaultConfigs(EnemyType::FlowerClover),
        MakeDefaultConfigs(EnemyType::FlowerLotus),
        MakeDefaultConfigs(EnemyType::FlowerSunward),
    };

    /// @brief 発射間隔のばらつき用（同じ種類が並んでも一斉射撃にならないように）
    float RandomJitter(float amount)
    {
        static std::mt19937 rng{ 20250908u };
        if (amount <= 0.0f) return 0.0f;
        std::uniform_real_distribution<float> dist(-amount, amount);
        return dist(rng);
    }
}

MobEnemyConfig& GetMobEnemyConfig(EnemyType type)
{
    size_t index = static_cast<size_t>(type);
    if (index >= static_cast<size_t>(EnemyType::Count)) index = 0;
    return g_configs[index];
}

const char* MobEnemy::GetTypeName() const
{
    return GetConfig().typeName;
}

EnemyBase::ModelSpec MobEnemy::GetModelSpec() const
{
    const MobEnemyConfig& c = GetConfig();

    ModelSpec spec;
    spec.directory = c.directory;
    spec.fileName = c.fileName;
    spec.modelScale = c.modelScale;
    spec.groundOffsetRatio = c.groundOffsetRatio;
    spec.hitRadiusRatio = c.hitRadiusRatio;
    spec.hitHalfRatio = c.hitHalfRatio;
    spec.hitOffsetRatio = c.hitOffsetRatio;
    spec.hitShape = c.hitShape;
    spec.isPushable = c.isPushable;
    spec.useAnimation = c.useAnimation;
    spec.tintColor = c.tintColor;
    return spec;
}

void MobEnemy::OnInitialized()
{
    const MobEnemyConfig& c = GetConfig();
    shootTimer_ = c.shootInterval * 0.5f + RandomJitter(c.shootIntervalJitter);
    hopPhase_ = RandomJitter(kPi);

    if (IsAnimated())
    {
        // 同じ種類が並んでも動きが揃わないよう、再生速度を少しだけばらす
        animator_.SetSpeed(c.animSpeed * (1.0f + RandomJitter(0.08f)));
        animator_.Play(c.clipIdle);
    }
}

const char* MobEnemy::PickBaseClip() const
{
    const MobEnemyConfig& c = GetConfig();

    if (moveAmount_ > 0.01f && c.clipWalk && c.clipWalk[0] != 0)
    {
        return c.clipWalk;
    }
    if (isChasing_ && c.clipAlert && c.clipAlert[0] != 0)
    {
        return c.clipAlert;
    }
    return c.clipIdle;
}

bool MobEnemy::TakeShootRequest(ShootRequest& out)
{
    if (!shootRequest_.fire) return false;
    out = shootRequest_;
    shootRequest_.fire = false;
    return true;
}

bool MobEnemy::IsStepSafe(const Vector3& stepLocal, const EnemyUpdateContext& ctx) const
{
    float lenSq = stepLocal.x * stepLocal.x + stepLocal.z * stepLocal.z;
    if (lenSq < 1e-8f) return true;

    float len = std::sqrt(lenSq);
    Vector3 dir{ stepLocal.x / len, 0.0f, stepLocal.z / len };

    // 敵の体の半径を考慮した前方のチェック距離
    // 足元の半径: scale_.x * hitRadiusRatio_ (スライムで約0.5m〜1.2m)
    // 敵が崖に近づいた時、体の中心が崖のフチより手前であっても足がはみ出て落ちないよう、
    // 移動先 (anchorLocal_ + stepLocal) よりさらに前方へプローブを伸ばす
    float footRadius = scale_.x * hitRadiusRatio_;
    float probeDistance = len + (std::max)(0.30f, footRadius * 0.6f);

    Vector3 probeLocal = anchorLocal_ + dir * probeDistance;
    Vector3 probeWorld = StageLocalToWorld(probeLocal, ctx.stageTilt, ctx.pivot);

    bool hasGround = false;
    Vector3 normal{ 0.0f, 1.0f, 0.0f };
    float groundY = SlimePhysics::CalculateGroundedCenterYEx(
        probeWorld.x, probeWorld.z, position_.y, ctx.stageTilt, groundOffset_,
        &hasGround, &normal, ctx.pivot, true);

    if (!hasGround)
    {
        return false; // 床が存在しない（奈落）
    }

    // 落差（現在の中心Y座標との差）
    float diffY = groundY - position_.y;

    // 登り段差: 0.40m 以上は登れない（壁・高段差）
    if (diffY > 0.40f)
    {
        return false;
    }

    // 降り段差（落差）: 0.50m 以上は崖と判定して歩行停止
    if (diffY < -0.50f)
    {
        return false;
    }

    return true;
}

void MobEnemy::UpdateBehavior(const EnemyUpdateContext& ctx)
{
    const MobEnemyConfig& c = GetConfig();
    const float dt = ctx.deltaTime;

    // プレイヤーをステージローカル座標に持ってきて、同じ土俵で距離を測る
    Vector3 playerLocal = StageWorldToLocal(ctx.playerPos, ctx.stageTilt, ctx.pivot);

    float dx = playerLocal.x - anchorLocal_.x;
    float dz = playerLocal.z - anchorLocal_.z;
    float distSq = dx * dx + dz * dz;
    float dist = std::sqrt(distSq);

    // --- 索敵（ヒステリシス付き）---
    if (!isChasing_)
    {
        if (dist <= c.chaseRange) isChasing_ = true;
    }
    else
    {
        if (dist >= c.loseRange) isChasing_ = false;
    }

    // --- 向き ---
    // 見つけているあいだはプレイヤーの方を向く
    if (dist > 0.05f && (isChasing_ || c.canShoot))
    {
        // yaw_ は rotation_.y（ワールドのオイラー角）にそのまま入るので、
        // ステージローカルの dx/dz ではなくワールドの向きから作る
        float wdx = ctx.playerPos.x - position_.x;
        float wdz = ctx.playerPos.z - position_.z;
        float targetYaw = (wdx * wdx + wdz * wdz > 1e-6f) ? std::atan2(wdx, wdz)
                                                          : std::atan2(dx, dz);

        float diff = targetYaw - yaw_;
        while (diff < -kPi) diff += 2.0f * kPi;
        while (diff > kPi)  diff -= 2.0f * kPi;

        yaw_ += diff * (std::min)(1.0f, c.turnSpeed * dt);
        while (yaw_ < -kPi) yaw_ += 2.0f * kPi;
        while (yaw_ > kPi)  yaw_ -= 2.0f * kPi;
    }

    // --- 移動 ---
    moveAmount_ = 0.0f;
    if (c.canMove && isChasing_ && dist > c.keepDistance && dist > 1e-4f)
    {
        float step = c.moveSpeed * dt;
        // 目標を通り越さない
        float travel = (std::min)(step, dist - c.keepDistance);
        if (travel > 0.0f)
        {
            Vector3 desiredStep{ (dx / dist) * travel, 0.0f, (dz / dist) * travel };

            if (!c.preventFall || IsStepSafe(desiredStep, ctx))
            {
                anchorLocal_.x += desiredStep.x;
                anchorLocal_.z += desiredStep.z;
                moveAmount_ = travel / (std::max)(1e-4f, step); // 0〜1
            }
            else if (c.preventFall)
            {
                // 軸分離（壁ずり・崖沿い移動）:
                // 斜めに崖へ突っ込んだ際、崖に沿う方向へ滑るように進む
                Vector3 stepX{ desiredStep.x, 0.0f, 0.0f };
                Vector3 stepZ{ 0.0f, 0.0f, desiredStep.z };

                bool safeX = (std::abs(desiredStep.x) > 1e-5f) && IsStepSafe(stepX, ctx);
                bool safeZ = (std::abs(desiredStep.z) > 1e-5f) && IsStepSafe(stepZ, ctx);

                if (safeX && safeZ)
                {
                    // 移動量が大きい軸を優先
                    if (std::abs(desiredStep.x) >= std::abs(desiredStep.z))
                    {
                        anchorLocal_.x += stepX.x;
                        moveAmount_ = std::abs(stepX.x) / (std::max)(1e-4f, step);
                    }
                    else
                    {
                        anchorLocal_.z += stepZ.z;
                        moveAmount_ = std::abs(stepZ.z) / (std::max)(1e-4f, step);
                    }
                }
                else if (safeX)
                {
                    anchorLocal_.x += stepX.x;
                    moveAmount_ = std::abs(stepX.x) / (std::max)(1e-4f, step);
                }
                else if (safeZ)
                {
                    anchorLocal_.z += stepZ.z;
                    moveAmount_ = std::abs(stepZ.z) / (std::max)(1e-4f, step);
                }
                // 両方の軸とも崖ならその場で停止（moveAmount_ は 0.0f のまま）
            }
        }
    }

    hopPhase_ += dt * c.hopSpeed;

    // --- 射撃 ---
    if (c.canShoot)
    {
        shootTimer_ -= dt;

        // 射程外ではタイマーを 0 止まりにする。
        // そうしないとマイナスに溜まり続けて、射程に入った瞬間に即撃ちしてしまう
        if (dist > c.shootRange && shootTimer_ < 0.0f)
        {
            shootTimer_ = 0.0f;
        }

        if (shootTimer_ <= 0.0f && dist <= c.shootRange)
        {
            // 発射方向はワールド空間で作る（弾はワールドを直進する）
            Vector3 toPlayer{
                ctx.playerPos.x - position_.x,
                0.0f,
                ctx.playerPos.z - position_.z
            };
            float len = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);
            if (len > 1e-4f)
            {
                Vector3 forward{ toPlayer.x / len, 0.0f, toPlayer.z / len };

                shootRequest_.fire = true;
                shootRequest_.direction = forward;
                shootRequest_.origin = {
                    position_.x + forward.x * c.muzzleForward,
                    position_.y + scale_.y * c.muzzleHeightRatio,
                    position_.z + forward.z * c.muzzleForward
                };

                shootTimer_ = c.shootInterval + RandomJitter(c.shootIntervalJitter);
            }
            else
            {
                shootTimer_ = 0.2f;
            }
        }
    }

    // --- アニメーションの状態選択 ---
    if (IsAnimated())
    {
        const char* base = PickBaseClip();
        if (shootRequest_.fire && c.clipAttack && c.clipAttack[0] != 0)
        {
            // 発射の瞬間だけ攻撃モーションを1回。終わったら base に戻る
            animator_.PlayOneShot(c.clipAttack, base ? base : "");
        }
        else
        {
            animator_.Play(base ? base : "");
        }
    }
}

Vector3 MobEnemy::GetRenderScale() const
{
    // アニメーションが動いているならホップ演出は要らない
    if (IsAnimated()) return scale_;

    const MobEnemyConfig& c = GetConfig();

    // アニメーションが入るまでの繋ぎ。動いているときはスクワッシュ、止まっているときは呼吸
    float wobble = std::sin(lifeTime_ * 3.0f + hopPhase_) * c.idleWobble;
    float squash = 0.0f;
    if (c.hopHeight > 0.0f && moveAmount_ > 0.01f)
    {
        // ホップの底で潰れて頂点で伸びる
        squash = -std::cos(hopPhase_ * 2.0f) * 0.10f * moveAmount_;
    }

    float sy = 1.0f + wobble + squash;
    float sxz = 1.0f - (wobble + squash) * 0.5f;

    return { scale_.x * sxz, scale_.y * sy, scale_.z * sxz };
}

float MobEnemy::GetVisualOffsetY() const
{
    if (IsAnimated()) return 0.0f;

    const MobEnemyConfig& c = GetConfig();
    if (c.hopHeight <= 0.0f || moveAmount_ <= 0.01f) return 0.0f;

    // sin の正側だけ使って「跳ねる」動きにする
    float h = std::sin(hopPhase_);
    if (h < 0.0f) h = 0.0f;
    return h * c.hopHeight * scale_.y * moveAmount_;
}
