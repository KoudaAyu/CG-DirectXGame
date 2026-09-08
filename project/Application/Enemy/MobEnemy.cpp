#define NOMINMAX
#include "MobEnemy.h"

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
            c.moveSpeed = 2.6f;
            c.chaseRange = 9.0f;
            c.loseRange = 14.0f;
            c.keepDistance = 0.5f;
            c.hopHeight = 0.16f;
            c.hopSpeed = 8.0f;
            c.idleWobble = 0.06f;
            c.strengthMin = 1;
            c.strengthMax = 5;
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
            c.strengthMin = 2;
            c.strengthMax = 7;
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
            c.bulletScale = 0.28f;
            c.bulletHitRadius = 0.28f;
            c.muzzleHeightRatio = 1.10f;
            c.bulletDirectory = "Resources/Enemy/FlowerLotus";
            c.bulletFileName = "Lotus_Bullet.gltf";
            c.bulletColor = { 1.0f, 0.55f, 0.85f, 1.0f };
            c.strengthMin = 2;
            c.strengthMax = 8;
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
            c.bulletScale = 0.30f;
            c.bulletHitRadius = 0.30f;
            c.muzzleHeightRatio = 1.60f;
            c.bulletDirectory = "Resources/Enemy/FlowerSunward";
            c.bulletFileName = "Sunward_Bullet.gltf";
            c.bulletColor = { 1.0f, 0.85f, 0.30f, 1.0f };
            c.strengthMin = 3;
            c.strengthMax = 9;
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
    spec.tintColor = c.tintColor;
    return spec;
}

void MobEnemy::OnInitialized()
{
    const MobEnemyConfig& c = GetConfig();
    shootTimer_ = c.shootInterval * 0.5f + RandomJitter(c.shootIntervalJitter);
    hopPhase_ = RandomJitter(kPi);
}

bool MobEnemy::TakeShootRequest(ShootRequest& out)
{
    if (!shootRequest_.fire) return false;
    out = shootRequest_;
    shootRequest_.fire = false;
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
            anchorLocal_.x += (dx / dist) * travel;
            anchorLocal_.z += (dz / dist) * travel;
            moveAmount_ = travel / (std::max)(1e-4f, step); // 0〜1
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
}

Vector3 MobEnemy::GetRenderScale() const
{
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
    const MobEnemyConfig& c = GetConfig();
    if (c.hopHeight <= 0.0f || moveAmount_ <= 0.01f) return 0.0f;

    // sin の正側だけ使って「跳ねる」動きにする
    float h = std::sin(hopPhase_);
    if (h < 0.0f) h = 0.0f;
    return h * c.hopHeight * scale_.y * moveAmount_;
}
