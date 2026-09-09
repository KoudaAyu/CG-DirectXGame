#define NOMINMAX
#include "Application/Enemy/Boss.h"

#include "Application/GameObject/SlimePhysics.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace
{
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kTwoPi = kPi * 2.0f;

    BossConfig g_bossConfig{};

    /// @brief min..max の一様乱数（演出のばらつき用。再現性は要らないので rand() で十分）
    float RandomRange(float minValue, float maxValue)
    {
        const float t = static_cast<float>(std::rand() % 1001) / 1000.0f;
        return minValue + (maxValue - minValue) * t;
    }
}

BossConfig& GetBossConfig()
{
    return g_bossConfig;
}

// ===================================================================
// モデル設定
// ===================================================================

EnemyBase::ModelSpec Boss::GetModelSpec() const
{
    const BossConfig& c = GetBossConfig();

    ModelSpec spec;
    spec.directory = c.directory;
    spec.fileName = c.fileName;
    spec.modelScale = c.modelScale;
    spec.groundOffsetRatio = c.groundOffsetRatio;
    spec.hitRadiusRatio = c.hitRadiusRatio;
    spec.hitOffsetRatio = c.hitOffsetRatio;
    spec.hitShape = EnemyCollision::HitShape::Sphere;
    spec.isPushable = false; // ボスは押されない
    spec.useAnimation = c.useAnimation;
    spec.tintColor = c.tintColor;
    return spec;
}

void Boss::OnInitialized()
{
    const BossConfig& c = GetBossConfig();

    // 【重要】ボスの見た目の大きさは HP に連動させない。
    // EnemyBase は既定で「強さ -> スケール」を掛けるが、ボスは強さ = HP = 100 なので
    // そのままだと巨大になりすぎる。この個体だけ定数を返す関数に差し替える
    SetScaleFromStrength([](int) { return 1.0f; });

    ResetHp(c.maxHp);

    // 「配置された場所」を覚えておく。ここから roamRadius 以上は離れない
    homeLocal_ = anchorLocal_;
    strafeDir_ = (std::rand() % 2 == 0) ? 1.0f : -1.0f;
    strafeTimer_ = RandomRange(c.strafeSwitchMin, c.strafeSwitchMax);
    shootStopTimer_ = 0.0f;
    moveAmount_ = 0.0f;
    moveEnabled_ = true;

    shootTimer_ = c.shootInterval * 0.6f;
    volleyPhase_ = 0.0f;
    auraIntensity_ = 1.0f;
    deathShake_ = 0.0f;
    shakePhase_ = 0.0f;
    hopPhase_ = 0.0f;

    if (IsAnimated())
    {
        animator_.SetSpeed(c.animSpeed);
        animator_.Play(c.clipIdle);
    }
}

// ===================================================================
// HP
// ===================================================================

float Boss::GetHpRatio() const
{
    if (maxHp_ <= 0) return 0.0f;
    return std::clamp(static_cast<float>(hp_) / static_cast<float>(maxHp_), 0.0f, 1.0f);
}

void Boss::ResetHp(int maxHp)
{
    maxHp_ = (std::max)(1, maxHp);
    hp_ = maxHp_;

    // HP をそのまま「強さ」として EnemyBase に持たせる。
    // これで EnemyCollision の強弱判定（プレイヤーの塊サイズ vs 敵の強さ）が
    // 「HP がプレイヤーより小さくなったら体当たりで倒せる」にそのまま化ける。
    // スケールは OnInitialized() で定数に差し替えてあるので変わらない
    SetStrength(hp_);
}

bool Boss::ApplyDamage(int amount)
{
    if (amount <= 0) return hp_ <= 0;

    hp_ -= amount;
    if (hp_ < 0) hp_ = 0;
    SetStrength((std::max)(1, hp_)); // 強さ0だと比較が壊れるので下限1

    return hp_ <= 0;
}

void Boss::SetHp(int hp)
{
    hp_ = std::clamp(hp, 0, maxHp_);
    SetStrength((std::max)(1, hp_));
}

// ===================================================================
// 射撃
// ===================================================================

bool Boss::TakeShootBurst(ShootBurst& out)
{
    if (!burst_.fire) return false;
    out = burst_;
    burst_.fire = false;
    burst_.directions.clear();
    return true;
}

// ===================================================================
// 挙動
// ===================================================================

void Boss::UpdateBehavior(const EnemyUpdateContext& ctx)
{
    const BossConfig& c = GetBossConfig();
    const float dt = ctx.deltaTime;

    // --- 向き ---
    const float wdx = ctx.playerPos.x - position_.x;
    const float wdz = ctx.playerPos.z - position_.z;
    if (wdx * wdx + wdz * wdz > 1e-6f)
    {
        float targetYaw = std::atan2(wdx, wdz);

        float diff = targetYaw - yaw_;
        while (diff < -kPi) diff += kTwoPi;
        while (diff > kPi)  diff -= kTwoPi;

        yaw_ += diff * (std::min)(1.0f, c.turnSpeed * dt);
    }
    yaw_ += c.idleSpinSpeed * dt;
    while (yaw_ < -kPi) yaw_ += kTwoPi;
    while (yaw_ > kPi)  yaw_ -= kTwoPi;

    hopPhase_ += dt * c.hopSpeed;
    shakePhase_ += dt;

    // ===================================================================
    // 移動（追跡 ＋ 間合い取り ＋ 回り込み）
    //
    // 位置はステージローカル（anchorLocal_）で動かすのが EnemyBase の作法。
    // ワールド座標（position_）は毎フレーム導出されるだけの結果でしかない。
    // ここをワールドで動かすと、ステージを傾けた瞬間に地形だけ動いて取り残される
    // ===================================================================
    if (shootStopTimer_ > 0.0f) shootStopTimer_ -= dt;

    moveAmount_ = 0.0f;
    if (c.canMove && moveEnabled_ && shootStopTimer_ <= 0.0f && dt > 1e-5f)
    {
        // プレイヤーを同じ土俵（ステージローカル）へ持ってきて距離を測る
        const Vector3 playerLocal = StageWorldToLocal(ctx.playerPos, ctx.stageTilt, ctx.pivot);
        const float lx = playerLocal.x - anchorLocal_.x;
        const float lz = playerLocal.z - anchorLocal_.z;
        const float dist = std::sqrt(lx * lx + lz * lz);

        float moveX = 0.0f;
        float moveZ = 0.0f;

        if (dist > 1e-4f)
        {
            const float nx = lx / dist;
            const float nz = lz / dist;

            if (dist > c.keepDistance)
            {
                // 遠い: 追いかける（間合いを通り越さない）
                const float travel = (std::min)(c.moveSpeed * dt, dist - c.keepDistance);
                moveX = nx * travel;
                moveZ = nz * travel;
            }
            else if (dist < c.backOffDistance)
            {
                // 近すぎ: 後ろへ下がる
                const float travel = (std::min)(c.moveSpeed * dt, c.backOffDistance - dist);
                moveX = -nx * travel;
                moveZ = -nz * travel;
            }
            else
            {
                // ちょうどいい間合い: プレイヤーのまわりを回り込む
                strafeTimer_ -= dt;
                if (strafeTimer_ <= 0.0f)
                {
                    strafeDir_ = -strafeDir_;
                    strafeTimer_ = RandomRange(c.strafeSwitchMin, c.strafeSwitchMax);
                }
                // 接線方向（プレイヤーへの向きを90度回したもの）
                moveX = -nz * strafeDir_ * c.strafeSpeed * dt;
                moveZ = nx * strafeDir_ * c.strafeSpeed * dt;
            }
        }

        // 【落下防止】動いた先に床があるか先に確かめる。
        // 無ければその移動は無かったことにする（崖から落ちて y < -80 で
        // 消えてしまうと、ボス戦がそのまま詰む）
        if (moveX != 0.0f || moveZ != 0.0f)
        {
            const Vector3 nextLocal = { anchorLocal_.x + moveX, anchorLocal_.y, anchorLocal_.z + moveZ };
            const Vector3 nextWorld = StageLocalToWorld(nextLocal, ctx.stageTilt, ctx.pivot);

            bool hasGround = false;
            SlimePhysics::CalculateGroundHeightEx(nextWorld.x, nextWorld.z, position_.y,
                                                  ctx.stageTilt, &hasGround, nullptr,
                                                  ctx.pivot, false, 0.0f);
            if (!hasGround)
            {
                // 崖の縁。前進はやめて、次のフレームは逆向きに回り込む
                moveX = 0.0f;
                moveZ = 0.0f;
                strafeDir_ = -strafeDir_;
                strafeTimer_ = RandomRange(c.strafeSwitchMin, c.strafeSwitchMax);
            }
        }

        anchorLocal_.x += moveX;
        anchorLocal_.z += moveZ;

        // 配置された場所から roamRadius 以上は離れない（ボス部屋から出ていかないように）
        if (c.roamRadius > 0.1f)
        {
            const float hx = anchorLocal_.x - homeLocal_.x;
            const float hz = anchorLocal_.z - homeLocal_.z;
            const float homeDist = std::sqrt(hx * hx + hz * hz);
            if (homeDist > c.roamRadius)
            {
                const float shrink = c.roamRadius / homeDist;
                anchorLocal_.x = homeLocal_.x + hx * shrink;
                anchorLocal_.z = homeLocal_.z + hz * shrink;
            }
        }

        const float moved = std::sqrt(moveX * moveX + moveZ * moveZ);
        const float maxStep = (std::max)(0.01f, c.moveSpeed) * dt;
        moveAmount_ = std::clamp(moved / maxStep, 0.0f, 1.0f);
    }

    // --- 歩き／待機の切り替え（攻撃のワンショット中は触らない）---
    if (IsAnimated() && !animator_.IsOneShotPlaying())
    {
        const bool walking = (moveAmount_ > 0.05f);
        const char* clip = (walking && c.clipWalk && c.clipWalk[0] != 0) ? c.clipWalk : c.clipIdle;
        if (clip && clip[0] != 0)
        {
            animator_.Play(clip);
        }
    }

    // --- 全方向弾 ---
    if (!shootEnabled_)
    {
        // フォーカス演出中。タイマーは進めず、次に動き出したらすぐ撃たせない
        shootTimer_ = c.shootInterval * 0.5f;
        return;
    }

    shootTimer_ -= dt;
    if (shootTimer_ > 0.0f) return;

    shootTimer_ = (std::max)(0.2f, c.shootInterval);
    shootStopTimer_ = (std::max)(0.0f, c.shootStopSeconds); // 撃つ瞬間は足を止める

    const int ways = (std::max)(3, c.bulletWays);
    const float muzzleY = position_.y + c.muzzleHeightRatio * modelScale_;

    burst_.fire = true;
    burst_.origin = { position_.x, muzzleY, position_.z };
    burst_.directions.clear();
    burst_.directions.reserve(static_cast<size_t>(ways));

    for (int i = 0; i < ways; ++i)
    {
        const float angle = volleyPhase_ + kTwoPi * static_cast<float>(i) / static_cast<float>(ways);
        burst_.directions.push_back({ std::sin(angle), 0.0f, std::cos(angle) });
    }

    // 撃つたびに少しずらす。連射すると弾幕が渦を巻いて見える
    volleyPhase_ += c.spinPerVolley;
    while (volleyPhase_ > kTwoPi) volleyPhase_ -= kTwoPi;

    if (IsAnimated() && c.clipAttack && c.clipAttack[0] != 0)
    {
        animator_.PlayOneShot(c.clipAttack, c.clipIdle);
    }
}

// ===================================================================
// 見た目
// ===================================================================

Vector3 Boss::GetRenderScale() const
{
    const BossConfig& c = GetBossConfig();

    // 呼吸するようなぷにぷに。当たり判定には影響しない
    const float wobble = std::sin(hopPhase_ * 1.7f) * c.idleWobble;

    // 死亡演出中の震え。オーラが激しくなるほど大きく速く震わせる
    float shakeX = 0.0f;
    float shakeZ = 0.0f;
    if (deathShake_ > 0.0f)
    {
        shakeX = std::sin(shakePhase_ * 47.0f) * deathShake_;
        shakeZ = std::cos(shakePhase_ * 41.0f) * deathShake_;
    }

    return { scale_.x * (1.0f + wobble + shakeX),
             scale_.y * (1.0f - wobble * 0.8f),
             scale_.z * (1.0f + wobble + shakeZ) };
}

float Boss::GetVisualOffsetY() const
{
    const BossConfig& c = GetBossConfig();

    float y = std::abs(std::sin(hopPhase_)) * c.hopHeight * modelScale_;

    // 死亡演出中は上下にも小刻みに震わせる
    if (deathShake_ > 0.0f)
    {
        y += std::sin(shakePhase_ * 53.0f) * deathShake_ * modelScale_;
    }
    return y;
}

float Boss::GetVisualRadius() const
{
    const BossConfig& c = GetBossConfig();
    return (std::max)(0.5f, c.hitRadiusRatio * modelScale_);
}

Vector3 Boss::GetHeadPosition() const
{
    const BossConfig& c = GetBossConfig();
    return { position_.x,
             position_.y + c.hitOffsetRatio * modelScale_ * 1.35f,
             position_.z };
}
