#define NOMINMAX
#include "Application/Scene/GameScene/GamePlaySceneFX.h"

#include "Application/GameObject/FireworkFx.h"
#include "Application/GameObject/CoinManager.h"
#include "Application/GameObject/SlimeManager.h"
#include "Application/Enemy/EnemyManager.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <vector>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace
{
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kTwoPi = kPi * 2.0f;

    /// @brief 同時に生きている数と寿命から、1秒あたりの発生数を出す
    float EmitRate(int concurrentCount, float lifeTime)
    {
        if (lifeTime <= 0.001f) return 0.0f;
        return static_cast<float>(concurrentCount) / lifeTime;
    }

    Vector4 LerpColor(const Vector4& a, const Vector4& b, float t)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        return { a.x + (b.x - a.x) * t,
                 a.y + (b.y - a.y) * t,
                 a.z + (b.z - a.z) * t,
                 a.w + (b.w - a.w) * t };
    }

    /// @brief ストップ列を巡らない線形グラデーションとしてサンプルする
    Vector4 SamplePalette(const Vector4* stops, int count, float t)
    {
        if (count <= 0) return { 1.0f, 1.0f, 1.0f, 1.0f };
        if (count == 1) return stops[0];

        t = std::clamp(t, 0.0f, 0.99999f) * static_cast<float>(count - 1);
        const int index = static_cast<int>(t);
        return LerpColor(stops[index], stops[index + 1], t - static_cast<float>(index));
    }

    // 淡い黄 → 緑 → 水色
    constexpr Vector4 kAmbientStops[] = {
        { 1.00f, 0.95f, 0.55f, 1.0f },
        { 0.60f, 1.00f, 0.62f, 1.0f },
        { 0.50f, 0.90f, 1.00f, 1.0f },
    };

    // 紫 → 赤
    constexpr Vector4 kEnemyStops[] = {
        { 0.66f, 0.20f, 0.95f, 1.0f },
        { 0.92f, 0.16f, 0.62f, 1.0f },
        { 1.00f, 0.22f, 0.22f, 1.0f },
    };
}

GamePlaySceneFx::GamePlaySceneFx() = default;

GamePlaySceneFx::~GamePlaySceneFx()
{
    Finalize();
}

void GamePlaySceneFx::Initialize(DirectXCom* dxCommon, Camera* camera)
{
    camera_ = camera;

    fx_ = std::make_unique<FireworkFx>();
    // ミニオン9体 + コイン + 敵 + 環境で 2000 粒前後を見込む。
    // 溢れても Emit() が黙って捨てるだけで落ちない
    fx_->Initialize(dxCommon, camera, 6144);
    fx_->SetAdditive(additive_);

    // ゲーミング（虹色）の場。useColorField を立てた粒だけがここを通る。
    // コインの光芒とプレイヤー分裂だけが使う
    fx_->SetColorField([this](float time, const Vector3& position) {
        return EvaluateGamingField(time, position);
    });

    ambientAccum_ = 0.0f;
    playerGlowAccum_ = 0.0f;
    playerTrailAccum_ = 0.0f;
    footRingTimer_ = 0.0f;
    minionGlowAccum_ = 0.0f;
    coinShineAccum_ = 0.0f;
    enemyAuraAccum_ = 0.0f;
    hasPrevPlayerPos_ = false;
}

void GamePlaySceneFx::Finalize()
{
    if (fx_)
    {
        fx_->Finalize();
        fx_.reset();
    }
    camera_ = nullptr;
}

void GamePlaySceneFx::SetCamera(Camera* camera)
{
    camera_ = camera;
    if (fx_) fx_->SetCamera(camera);
}

bool GamePlaySceneFx::IsReady() const
{
    return fx_ && fx_->IsReady();
}

void GamePlaySceneFx::BeginFrame(const Vector3& focusCenter)
{
    focusCenter_ = focusCenter;
}

float GamePlaySceneFx::RandomRange(float minValue, float maxValue)
{
    std::uniform_real_distribution<float> dist(minValue, maxValue);
    return dist(rng_);
}

bool GamePlaySceneFx::IsInRange(const Vector3& position) const
{
    const float dx = position.x - focusCenter_.x;
    const float dz = position.z - focusCenter_.z;
    const float range = (std::max)(coinRange_, enemyRange_);
    return (dx * dx + dz * dz) <= (range * range);
}

Vector4 GamePlaySceneFx::SampleAmbientColor(float t) const
{
    return SamplePalette(kAmbientStops, static_cast<int>(std::size(kAmbientStops)), t);
}

Vector4 GamePlaySceneFx::SampleEnemyColor(float t) const
{
    return SamplePalette(kEnemyStops, static_cast<int>(std::size(kEnemyStops)), t);
}

Vector4 GamePlaySceneFx::EvaluateGamingField(float time, const Vector3& position) const
{
    // Inigo Quilez のコサインパレット。位置も効かせているので、
    // 同じコインの光芒でも外へ広がるほど色がずれていく
    const float u = time * gamingTimeScale_
                  + (position.x + position.y * 0.6f + position.z) * gamingSpaceScale_;

    const float r = 0.5f + 0.5f * std::cos(kTwoPi * u);
    const float g = 0.5f + 0.5f * std::cos(kTwoPi * (u + 1.0f / 3.0f));
    const float b = 0.5f + 0.5f * std::cos(kTwoPi * (u + 2.0f / 3.0f));

    // 加算合成なので上限を切らないと白飛びする
    return { std::clamp(r * gamingGain_, 0.0f, 1.0f),
             std::clamp(g * gamingGain_, 0.0f, 1.0f),
             std::clamp(b * gamingGain_, 0.0f, 1.0f),
             1.0f };
}

// ===================================================================
// 画面全体にゆらゆら舞い上がる光
// ===================================================================

void GamePlaySceneFx::UpdateAmbient(float deltaTime)
{
    if (!fx_ || !enableAmbient_) return;

    ambientAccum_ += EmitRate(ambientCount_, ambientLife_) * deltaTime;

    while (ambientAccum_ >= 1.0f)
    {
        ambientAccum_ -= 1.0f;

        // 視点中心の周りの円盤からランダムに湧かせる。
        // sqrt を掛けて面積で一様にする（中心に固まらせない）
        const float angle = RandomRange(0.0f, kTwoPi);
        const float radius = ambientAreaRadius_ * std::sqrt(RandomRange(0.0f, 1.0f));

        FireworkFxDesc desc{};
        desc.position = { focusCenter_.x + std::cos(angle) * radius,
                          focusCenter_.y - ambientStartBelow_ + RandomRange(0.0f, 2.0f),
                          focusCenter_.z + std::sin(angle) * radius };

        // y はずっと増加。横は「ゆらぎ」の代わりに、粒ごとに違う向きへ少しだけ流す
        desc.velocity = { RandomRange(-ambientSway_, ambientSway_),
                          RandomRange(ambientRiseMin_, ambientRiseMax_),
                          RandomRange(-ambientSway_, ambientSway_) };
        desc.gravity = 0.0f;
        desc.drag = 0.35f; // 横の流れだけがだんだん止まって、上昇が残る

        Vector4 color = SampleAmbientColor(RandomRange(0.0f, 1.0f));
        color.w = ambientAlpha_;
        desc.colorBegin = color;
        desc.colorEnd = { color.x, color.y, color.z, 0.0f };

        const float scale = RandomRange(ambientScaleMin_, ambientScaleMax_);
        desc.scaleBegin = scale * 0.55f; // ふわっと膨らんでから消える
        desc.scaleEnd = scale;
        desc.lifeTime = ambientLife_ * RandomRange(0.75f, 1.25f);
        desc.useSparkTexture = false;

        fx_->Emit(desc);
    }
}

// ===================================================================
// スライム（プレイヤー / ミニオン）の体内から染み出す光
// ===================================================================

void GamePlaySceneFx::EmitSlimeGlow(const Vector3& center, float radius, const Vector4& baseColor)
{
    if (!fx_) return;

    // 球の内側から湧いて外へ抜ける。半径は cbrt 分布で体積一様にする
    const float theta = RandomRange(0.0f, kTwoPi);
    const float cosPhi = RandomRange(-1.0f, 1.0f);
    const float sinPhi = std::sqrt((std::max)(0.0f, 1.0f - cosPhi * cosPhi));
    const float r = radius * std::cbrt(RandomRange(0.05f, 1.0f));

    const Vector3 dir = { sinPhi * std::cos(theta), cosPhi, sinPhi * std::sin(theta) };

    FireworkFxDesc desc{};
    desc.position = { center.x + dir.x * r, center.y + dir.y * r, center.z + dir.z * r };

    // 中心から遠いところで湧いた粒ほど速い ＝ 内側からじわっと染み出す感じ
    const float speed = slimeGlowSpeed_ * (0.35f + 0.65f * (r / (std::max)(0.01f, radius)));
    desc.velocity = { dir.x * speed, dir.y * speed + 0.25f, dir.z * speed };
    desc.gravity = -0.6f; // ゆるく持ち上がる
    desc.drag = 1.8f;

    // 自分の色を中心に、一定範囲で振る
    const float jitter = slimeGlowColorRange_;
    Vector4 color = {
        std::clamp(baseColor.x + RandomRange(-jitter, jitter), 0.0f, 1.0f),
        std::clamp(baseColor.y + RandomRange(-jitter, jitter), 0.0f, 1.0f),
        std::clamp(baseColor.z + RandomRange(-jitter, jitter), 0.0f, 1.0f),
        slimeGlowAlpha_,
    };
    desc.colorBegin = color;
    desc.colorEnd = { color.x, color.y, color.z, 0.0f };

    desc.scaleBegin = slimeGlowScale_ * RandomRange(0.7f, 1.3f);
    desc.scaleEnd = desc.scaleBegin * 0.25f;
    desc.lifeTime = slimeGlowLife_ * RandomRange(0.7f, 1.3f);
    desc.useSparkTexture = false;

    fx_->Emit(desc);
}

void GamePlaySceneFx::EmitFootRing(const Vector3& center, float radius, const Vector4& color)
{
    if (!fx_) return;

    // 地面に寝かせた板を円周に並べて、外へ広げる ＝ 拡大しつつ消える同心円
    const int count = (std::max)(3, footRingParticles_);
    const float phase = RandomRange(0.0f, kTwoPi);

    for (int i = 0; i < count; ++i)
    {
        const float angle = phase + kTwoPi * static_cast<float>(i) / static_cast<float>(count);
        const float cs = std::cos(angle);
        const float sn = std::sin(angle);

        FireworkFxDesc desc{};
        desc.position = { center.x + cs * radius, center.y + 0.05f, center.z + sn * radius };
        desc.velocity = { cs * footRingExpand_, 0.0f, sn * footRingExpand_ };
        desc.gravity = 0.0f;
        desc.drag = 1.2f;

        desc.colorBegin = { color.x, color.y, color.z, 0.7f };
        desc.colorEnd = { color.x, color.y, color.z, 0.0f };
        desc.scaleBegin = footRingScale_;
        desc.scaleEnd = footRingScale_ * 0.4f;
        desc.lifeTime = footRingLife_;
        desc.shape = FireworkFxShape::Ground; // 地面に貼り付ける
        desc.useSparkTexture = false;

        fx_->Emit(desc);
    }
}

void GamePlaySceneFx::UpdatePlayer(float deltaTime, Slime* player)
{
    if (!fx_ || !player) return;

    const Vector3 position = player->GetPosition();
    const float radius = player->GetCurrentScale() * 0.78f; // SlimeCollision と同じ見た目半径の規約
    const Vector4 baseColor = player->GetSlimeParams().baseColor;

    // --- ぽよぽよ光 ---
    if (enableSlimeGlow_)
    {
        playerGlowAccum_ += EmitRate(slimeGlowCount_, slimeGlowLife_) * deltaTime;
        while (playerGlowAccum_ >= 1.0f)
        {
            playerGlowAccum_ -= 1.0f;
            EmitSlimeGlow(position, radius, baseColor);
        }
    }

    // --- 移動軌跡（ぽよぽよ光より白い尾）---
    if (enablePlayerTrail_)
    {
        const Vector3 velocity = player->GetVelocity();
        const float speed = std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);

        if (speed >= trailMinSpeed_ && hasPrevPlayerPos_)
        {
            playerTrailAccum_ += EmitRate(trailCount_, trailLife_) * deltaTime;
            while (playerTrailAccum_ >= 1.0f)
            {
                playerTrailAccum_ -= 1.0f;

                // 前フレームと今フレームの間に置いていく。速く動いても途切れない
                const float t = RandomRange(0.0f, 1.0f);
                const Vector3 spot = {
                    prevPlayerPos_.x + (position.x - prevPlayerPos_.x) * t,
                    prevPlayerPos_.y + (position.y - prevPlayerPos_.y) * t,
                    prevPlayerPos_.z + (position.z - prevPlayerPos_.z) * t,
                };

                Vector4 color = LerpColor(baseColor, { 1.0f, 1.0f, 1.0f, 1.0f }, trailWhiteness_);
                color.w = 0.7f;

                FireworkFxDesc desc{};
                desc.position = { spot.x + RandomRange(-0.2f, 0.2f) * radius,
                                  spot.y + RandomRange(-0.1f, 0.3f) * radius,
                                  spot.z + RandomRange(-0.2f, 0.2f) * radius };
                // 進行方向の逆へ少し流す ＝ 尾を曳いて見える
                desc.velocity = { -velocity.x * 0.12f, 0.3f, -velocity.z * 0.12f };
                desc.gravity = -0.4f;
                desc.drag = 2.4f;
                desc.colorBegin = color;
                desc.colorEnd = { color.x, color.y, color.z, 0.0f };
                desc.scaleBegin = trailScale_ * RandomRange(0.8f, 1.2f);
                desc.scaleEnd = 0.0f;
                desc.lifeTime = trailLife_ * RandomRange(0.8f, 1.2f);

                fx_->Emit(desc);
            }
        }
        else
        {
            playerTrailAccum_ = 0.0f;
        }
    }

    // --- 足元の同心円 ---
    if (enableFootRing_)
    {
        footRingTimer_ -= deltaTime;
        if (footRingTimer_ <= 0.0f)
        {
            footRingTimer_ = (std::max)(0.05f, footRingInterval_);

            // 足は無いので、見た目半径の少し外側から広げる
            Vector3 foot = position;
            foot.y -= radius * 0.85f;
            EmitFootRing(foot, radius * footRingRadiusScale_, baseColor);
        }
    }

    prevPlayerPos_ = position;
    hasPrevPlayerPos_ = true;
}

void GamePlaySceneFx::UpdateMinions(float deltaTime, SlimeManager* slimeManager)
{
    if (!fx_ || !slimeManager || !enableSlimeGlow_) return;

    // 代表（一番大きい個体）は UpdatePlayer() が別に光らせているので、ここでは除く
    const Slime* leader = slimeManager->GetLeader();

    // 光らせる対象を先に集める。ここでランダムに1体選んで1粒ずつ出す
    struct Target { Vector3 position; float radius; Vector4 color; };
    std::vector<Target> targets;
    targets.reserve(slimeManager->GetSlimes().size());

    for (const auto& slimePtr : slimeManager->GetSlimes())
    {
        const Slime* slime = slimePtr.get();
        if (!slime || !slime->IsActive()) continue;
        if (slime == leader) continue;
        if (slime->GetState() == SlimeState::Merging) continue; // 吸われている最中は座標が飛ぶ

        targets.push_back({ slime->GetPosition(),
                            (std::max)(0.15f, slime->GetRadius()),
                            slime->GetSlimeParams().baseColor });
    }

    if (targets.empty())
    {
        minionGlowAccum_ = 0.0f;
        return;
    }

    const float rate = EmitRate(slimeGlowCount_, slimeGlowLife_) * static_cast<float>(targets.size());
    minionGlowAccum_ += rate * deltaTime;

    std::uniform_int_distribution<size_t> pick(0, targets.size() - 1);
    while (minionGlowAccum_ >= 1.0f)
    {
        minionGlowAccum_ -= 1.0f;
        const Target& target = targets[pick(rng_)];
        EmitSlimeGlow(target.position, target.radius, target.color);
    }
}

// ===================================================================
// コインのゲーミング光芒
// ===================================================================

void GamePlaySceneFx::EmitCoinShine(const Vector3& center, float radius, bool isVanishing)
{
    if (!fx_) return;

    const float angle = RandomRange(0.0f, kTwoPi);
    const float cosPhi = RandomRange(-0.35f, 1.0f); // 少し上向きに寄せる
    const float sinPhi = std::sqrt((std::max)(0.0f, 1.0f - cosPhi * cosPhi));
    const Vector3 dir = { sinPhi * std::cos(angle), cosPhi, sinPhi * std::sin(angle) };

    const float speed = coinShineSpeed_ * RandomRange(0.6f, 1.4f) * (isVanishing ? 1.5f : 1.0f);

    FireworkFxDesc desc{};
    desc.position = { center.x + dir.x * radius * 0.4f,
                      center.y + dir.y * radius * 0.4f,
                      center.z + dir.z * radius * 0.4f };
    desc.velocity = { dir.x * speed, dir.y * speed + 0.4f, dir.z * speed };
    desc.gravity = 0.0f;
    desc.drag = 2.2f;

    // 色はベクター場（ゲーミング）から毎フレーム引き直す。
    // colorBegin/End の RGB は使われないが、α の補間には効く
    desc.useColorField = true;
    desc.colorBegin = { 1.0f, 1.0f, 1.0f, 0.9f };
    desc.colorEnd = { 1.0f, 1.0f, 1.0f, 0.0f };

    // 若干長く細く。速度方向へ向けると、そのまま光芒に見える
    desc.scaleBegin = coinShineScale_ * RandomRange(0.8f, 1.3f);
    desc.scaleEnd = desc.scaleBegin * 0.3f;
    desc.scaleAspect = coinShineAspect_;
    desc.alignToVelocity = true;
    desc.useSparkTexture = true;
    desc.lifeTime = coinShineLife_ * RandomRange(0.8f, 1.2f);

    fx_->Emit(desc);
}

void GamePlaySceneFx::UpdateCoins(float deltaTime, CoinManager* coinManager)
{
    if (!fx_ || !coinManager || !enableCoinShine_) return;

    struct Target { Vector3 position; float radius; bool vanishing; };
    std::vector<Target> targets;

    const CoinConfig& config = CoinManager::GetConfig();
    const float coinRadius = (std::max)(0.1f, config.radius);
    const float rangeSq = coinRange_ * coinRange_;

    for (const auto& coinPtr : coinManager->GetCoins())
    {
        Coin* coin = coinPtr.get();
        if (!coin) continue;

        // 取得済みでも、上昇しながら消えるまでは撒き続ける
        const bool vanishing = coin->IsVanishing();
        if (coin->IsCollected() && !vanishing) continue;

        const Vector3& position = coin->GetPosition();
        const float dx = position.x - focusCenter_.x;
        const float dz = position.z - focusCenter_.z;
        if (!vanishing && (dx * dx + dz * dz) > rangeSq) continue;

        targets.push_back({ position, coinRadius * coin->GetScale(), vanishing });
        if (static_cast<int>(targets.size()) >= coinMaxEmitters_) break;
    }

    if (targets.empty())
    {
        coinShineAccum_ = 0.0f;
        return;
    }

    float rate = 0.0f;
    for (const Target& target : targets)
    {
        rate += EmitRate(coinShineCount_, coinShineLife_) * (target.vanishing ? coinVanishBoost_ : 1.0f);
    }
    coinShineAccum_ += rate * deltaTime;

    std::uniform_int_distribution<size_t> pick(0, targets.size() - 1);
    while (coinShineAccum_ >= 1.0f)
    {
        coinShineAccum_ -= 1.0f;
        const Target& target = targets[pick(rng_)];
        EmitCoinShine(target.position, target.radius, target.vanishing);
    }
}

// ===================================================================
// 敵の不気味な光
// ===================================================================

void GamePlaySceneFx::EmitEnemyAura(const Vector3& center, float radius)
{
    if (!fx_) return;

    const float angle = RandomRange(0.0f, kTwoPi);
    const float r = radius * std::sqrt(RandomRange(0.0f, 1.0f));

    FireworkFxDesc desc{};
    desc.position = { center.x + std::cos(angle) * r,
                      center.y + RandomRange(-radius * 0.5f, radius * 0.5f),
                      center.z + std::sin(angle) * r };
    desc.velocity = { RandomRange(-0.25f, 0.25f),
                      enemyAuraRise_ * RandomRange(0.7f, 1.4f),
                      RandomRange(-0.25f, 0.25f) };
    desc.gravity = 0.0f;
    desc.drag = 0.6f;

    Vector4 color = SampleEnemyColor(RandomRange(0.0f, 1.0f));
    color.w = 0.65f;
    desc.colorBegin = color;
    desc.colorEnd = { color.x, color.y, color.z, 0.0f };

    desc.scaleBegin = enemyAuraScale_ * RandomRange(0.7f, 1.3f);
    desc.scaleEnd = desc.scaleBegin * 1.4f; // 上るほどぼやけて消える
    desc.lifeTime = enemyAuraLife_ * RandomRange(0.75f, 1.25f);

    fx_->Emit(desc);
}

void GamePlaySceneFx::UpdateEnemies(float deltaTime, EnemyManager* enemyManager)
{
    if (!fx_ || !enemyManager || !enableEnemyAura_) return;

    struct Target { Vector3 position; float radius; };
    std::vector<Target> targets;

    const float rangeSq = enemyRange_ * enemyRange_;

    for (const auto& enemyPtr : enemyManager->GetEnemies())
    {
        MobEnemy* enemy = enemyPtr.get();
        if (!enemy || enemy->IsDead()) continue;

        const Vector3 center = enemy->GetHitCenter();
        const float dx = center.x - focusCenter_.x;
        const float dz = center.z - focusCenter_.z;
        if ((dx * dx + dz * dz) > rangeSq) continue;

        const Vector3& scale = enemy->GetScale();
        targets.push_back({ center, (std::max)(0.2f, scale.x * 0.8f) });
        if (static_cast<int>(targets.size()) >= enemyMaxEmitters_) break;
    }

    if (targets.empty())
    {
        enemyAuraAccum_ = 0.0f;
        return;
    }

    const float rate = EmitRate(enemyAuraCount_, enemyAuraLife_) * static_cast<float>(targets.size());
    enemyAuraAccum_ += rate * deltaTime;

    std::uniform_int_distribution<size_t> pick(0, targets.size() - 1);
    while (enemyAuraAccum_ >= 1.0f)
    {
        enemyAuraAccum_ -= 1.0f;
        const Target& target = targets[pick(rng_)];
        EmitEnemyAura(target.position, target.radius);
    }
}

// ===================================================================
// 単発
// ===================================================================

void GamePlaySceneFx::EmitEnemyHitSplash(const Vector3& position, const Vector4& slimeColor)
{
    if (!fx_) return;

    // 色は「ぽよぽよ光の色 + 敵の紫赤」の中間
    const Vector4 enemyColor = SampleEnemyColor(0.5f);
    const Vector4 mixed = {
        (slimeColor.x + enemyColor.x) * 0.5f,
        (slimeColor.y + enemyColor.y) * 0.5f,
        (slimeColor.z + enemyColor.z) * 0.5f,
        1.0f,
    };

    for (int i = 0; i < hitSplashCount_; ++i)
    {
        // 水しぶき。上向き半球へ勢いよく飛ばして重力で落とす
        const float angle = RandomRange(0.0f, kTwoPi);
        const float cosPhi = RandomRange(0.05f, 1.0f);
        const float sinPhi = std::sqrt((std::max)(0.0f, 1.0f - cosPhi * cosPhi));
        const float speed = hitSplashSpeed_ * RandomRange(0.4f, 1.3f);

        FireworkFxDesc desc{};
        desc.position = position;
        desc.velocity = { sinPhi * std::cos(angle) * speed,
                          cosPhi * speed,
                          sinPhi * std::sin(angle) * speed };
        desc.gravity = 14.0f;
        desc.drag = 0.7f;
        desc.colorBegin = { mixed.x, mixed.y, mixed.z, 0.95f };
        desc.colorEnd = { mixed.x, mixed.y, mixed.z, 0.0f };
        desc.scaleBegin = 0.90f * RandomRange(0.6f, 1.3f);
        desc.scaleEnd = 0.15f;
        desc.scaleAspect = 0.7f;
        desc.alignToVelocity = true;
        desc.lifeTime = RandomRange(0.35f, 0.7f);

        fx_->Emit(desc);
    }
}

void GamePlaySceneFx::EmitEnemyDefeat(const Vector3& position, int strength)
{
    if (!fx_) return;

    // 強いほど派手にする（粒数は上限を切っておく）
    const float power = 1.0f + 0.12f * static_cast<float>((std::max)(1, strength) - 1);
    const int count = (std::min)(defeatSplashCount_ * 2,
                                 static_cast<int>(defeatSplashCount_ * power));

    for (int i = 0; i < count; ++i)
    {
        const float angle = RandomRange(0.0f, kTwoPi);
        const float cosPhi = RandomRange(-0.2f, 1.0f);
        const float sinPhi = std::sqrt((std::max)(0.0f, 1.0f - cosPhi * cosPhi));
        const float speed = defeatSplashSpeed_ * power * RandomRange(0.3f, 1.2f);

        Vector4 color = SampleEnemyColor(RandomRange(0.0f, 1.0f));

        FireworkFxDesc desc{};
        desc.position = position;
        desc.velocity = { sinPhi * std::cos(angle) * speed,
                          cosPhi * speed + 1.5f,
                          sinPhi * std::sin(angle) * speed };
        desc.gravity = 16.0f;
        desc.drag = 0.5f;
        desc.colorBegin = { color.x, color.y, color.z, 1.0f };
        desc.colorEnd = { color.x, color.y, color.z, 0.0f };
        desc.scaleBegin = 1.02f * RandomRange(0.5f, 1.4f);
        desc.scaleEnd = 0.18f;
        desc.scaleAspect = 0.6f;
        desc.alignToVelocity = true;
        desc.lifeTime = RandomRange(0.45f, 0.95f);
        // 血しぶきらしく、飛びながら細かい粒を撒く
        desc.trailInterval = 0.035f;
        desc.trailLifeTime = 0.22f;
        desc.trailScale = 0.30f;

        fx_->Emit(desc);
    }
}

void GamePlaySceneFx::EmitPlayerSplit(const Vector3& position, int sizeBefore)
{
    if (!fx_) return;

    // コインの光芒と同じゲーミングだが、こっちはもっと勢いよく・もっと派手に
    const float power = 1.0f + 0.15f * static_cast<float>((std::max)(1, sizeBefore) - 1);
    const int count = (std::min)(splitBurstCount_ * 2,
                                 static_cast<int>(splitBurstCount_ * power));

    for (int i = 0; i < count; ++i)
    {
        const float angle = RandomRange(0.0f, kTwoPi);
        const float cosPhi = RandomRange(-0.6f, 1.0f);
        const float sinPhi = std::sqrt((std::max)(0.0f, 1.0f - cosPhi * cosPhi));
        const float speed = splitBurstSpeed_ * power * RandomRange(0.5f, 1.3f);

        FireworkFxDesc desc{};
        desc.position = position;
        desc.velocity = { sinPhi * std::cos(angle) * speed,
                          cosPhi * speed + 2.0f,
                          sinPhi * std::sin(angle) * speed };
        desc.gravity = 9.0f;
        desc.drag = 0.45f;
        desc.useColorField = true;
        desc.colorBegin = { 1.0f, 1.0f, 1.0f, 1.0f };
        desc.colorEnd = { 1.0f, 1.0f, 1.0f, 0.0f };
        desc.scaleBegin = 1.26f * RandomRange(0.7f, 1.4f);
        desc.scaleEnd = 0.24f;
        desc.scaleAspect = 0.38f; // コインより細長く、光芒を強調
        desc.alignToVelocity = true;
        desc.useSparkTexture = true;
        desc.lifeTime = RandomRange(0.55f, 1.05f);
        desc.trailInterval = 0.028f;
        desc.trailLifeTime = 0.3f;
        desc.trailScale = 0.39f;

        fx_->Emit(desc);
    }

    // 中心に一瞬だけ強い閃光
    for (int i = 0; i < 8; ++i)
    {
        FireworkFxDesc flash{};
        flash.position = position;
        flash.velocity = { RandomRange(-1.0f, 1.0f), RandomRange(-0.5f, 1.5f), RandomRange(-1.0f, 1.0f) };
        flash.drag = 3.0f;
        flash.useColorField = true;
        flash.colorBegin = { 1.0f, 1.0f, 1.0f, 1.0f };
        flash.colorEnd = { 1.0f, 1.0f, 1.0f, 0.0f };
        flash.scaleBegin = 4.8f * power;
        flash.scaleEnd = 0.6f;
        flash.lifeTime = 0.22f;
        fx_->Emit(flash);
    }
}

// ===================================================================
// 更新 / 描画
// ===================================================================

void GamePlaySceneFx::Update(float deltaTime)
{
    if (fx_) fx_->Update(deltaTime);
}

void GamePlaySceneFx::UpdateAll(float deltaTime, const Vector3& focusCenter, Slime* player,
                                SlimeManager* slimeManager, CoinManager* coinManager,
                                EnemyManager* enemyManager)
{
    BeginFrame(focusCenter);
    UpdateAmbient(deltaTime);
    UpdatePlayer(deltaTime, player);
    UpdateMinions(deltaTime, slimeManager);
    UpdateCoins(deltaTime, coinManager);
    UpdateEnemies(deltaTime, enemyManager);
    Update(deltaTime);
}

void GamePlaySceneFx::Draw(ID3D12GraphicsCommandList* commandList)
{
    // FireworkFx は自前のルートシグネチャ / PSO / 頂点バッファで完結しているので、
    // コマンドリストを渡すだけでいい（ライトもカメラ CBV も要らない）。
    // PSO はデプステスト有・書き込み無なので、3D の後・UI の前に描く
    if (fx_) fx_->Draw(commandList);
}

void GamePlaySceneFx::Clear()
{
    if (fx_) fx_->Clear();
    ambientAccum_ = 0.0f;
    playerGlowAccum_ = 0.0f;
    playerTrailAccum_ = 0.0f;
    minionGlowAccum_ = 0.0f;
    coinShineAccum_ = 0.0f;
    enemyAuraAccum_ = 0.0f;
}

int GamePlaySceneFx::GetActiveParticleCount() const
{
    return fx_ ? fx_->GetActiveCount() : 0;
}

void GamePlaySceneFx::DrawImGui()
{
#ifdef USE_IMGUI
    if (!ImGui::CollapsingHeader("Game FX")) return;

    if (!fx_)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "FireworkFx is not created.");
        return;
    }

    if (!fx_->IsReady())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                           "Shader compile failed. Nothing will be drawn.\n"
                           "Resources/shaders/FireworkFx.VS.hlsl / .PS.hlsl");
    }

    ImGui::Text("Particles: %d / %u", fx_->GetActiveCount(), fx_->GetCapacity());
    ImGui::Text("Quads drawn: %u", fx_->GetDrawnQuadCount());

    if (ImGui::Checkbox("Additive", &additive_)) fx_->SetAdditive(additive_);
    ImGui::SameLine();
    if (ImGui::Button("Clear Particles")) Clear();

    ImGui::SeparatorText("Toggles");
    ImGui::Checkbox("Ambient", &enableAmbient_);
    ImGui::SameLine();
    ImGui::Checkbox("Slime Glow", &enableSlimeGlow_);
    ImGui::SameLine();
    ImGui::Checkbox("Trail", &enablePlayerTrail_);
    ImGui::Checkbox("Foot Ring", &enableFootRing_);
    ImGui::SameLine();
    ImGui::Checkbox("Coin Shine", &enableCoinShine_);
    ImGui::SameLine();
    ImGui::Checkbox("Enemy Aura", &enableEnemyAura_);

    ImGui::SeparatorText("Ambient (rising light)");
    ImGui::DragInt("Ambient Count", &ambientCount_, 1.0f, 0, 1024);
    ImGui::DragFloat("Ambient Life", &ambientLife_, 0.05f, 0.2f, 12.0f);
    ImGui::DragFloatRange2("Ambient Scale", &ambientScaleMin_, &ambientScaleMax_, 0.01f, 0.05f, 6.0f);
    ImGui::DragFloat("Ambient Area", &ambientAreaRadius_, 0.5f, 4.0f, 80.0f);
    ImGui::DragFloatRange2("Rise Speed", &ambientRiseMin_, &ambientRiseMax_, 0.05f, 0.0f, 8.0f);
    ImGui::DragFloat("Sway", &ambientSway_, 0.02f, 0.0f, 3.0f);
    ImGui::DragFloat("Ambient Alpha", &ambientAlpha_, 0.01f, 0.0f, 1.0f);

    ImGui::SeparatorText("Slime Glow (player / minions)");
    ImGui::DragInt("Glow Count / slime", &slimeGlowCount_, 1.0f, 0, 256);
    ImGui::DragFloat("Glow Life", &slimeGlowLife_, 0.01f, 0.05f, 4.0f);
    ImGui::DragFloat("Glow Scale", &slimeGlowScale_, 0.01f, 0.02f, 6.0f);
    ImGui::DragFloat("Glow Speed", &slimeGlowSpeed_, 0.05f, 0.0f, 6.0f);
    ImGui::DragFloat("Color Range", &slimeGlowColorRange_, 0.01f, 0.0f, 0.6f);
    ImGui::DragFloat("Glow Alpha", &slimeGlowAlpha_, 0.01f, 0.0f, 1.0f);

    ImGui::SeparatorText("Player Trail");
    ImGui::DragInt("Trail Count", &trailCount_, 1.0f, 0, 128);
    ImGui::DragFloat("Trail Life", &trailLife_, 0.01f, 0.05f, 2.0f);
    ImGui::DragFloat("Trail Scale", &trailScale_, 0.01f, 0.02f, 6.0f);
    ImGui::DragFloat("Whiteness", &trailWhiteness_, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Min Speed", &trailMinSpeed_, 0.05f, 0.0f, 10.0f);

    ImGui::SeparatorText("Foot Ring");
    ImGui::DragFloat("Ring Interval", &footRingInterval_, 0.01f, 0.05f, 3.0f);
    ImGui::DragInt("Ring Particles", &footRingParticles_, 1.0f, 3, 128);
    ImGui::DragFloat("Ring Radius x", &footRingRadiusScale_, 0.01f, 0.2f, 4.0f);
    ImGui::DragFloat("Ring Expand", &footRingExpand_, 0.05f, 0.0f, 12.0f);
    ImGui::DragFloat("Ring Life", &footRingLife_, 0.01f, 0.05f, 3.0f);
    ImGui::DragFloat("Ring Scale", &footRingScale_, 0.01f, 0.02f, 6.0f);

    ImGui::SeparatorText("Coin Shine (gaming)");
    ImGui::DragInt("Shine Count / coin", &coinShineCount_, 1.0f, 0, 256);
    ImGui::DragFloat("Shine Life", &coinShineLife_, 0.01f, 0.05f, 4.0f);
    ImGui::DragFloat("Shine Scale", &coinShineScale_, 0.01f, 0.02f, 6.0f);
    ImGui::DragFloat("Shine Aspect", &coinShineAspect_, 0.01f, 0.05f, 2.0f);
    ImGui::DragFloat("Shine Speed", &coinShineSpeed_, 0.05f, 0.0f, 10.0f);
    ImGui::DragFloat("Vanish Boost", &coinVanishBoost_, 0.05f, 1.0f, 8.0f);
    ImGui::DragFloat("Coin Range", &coinRange_, 0.5f, 2.0f, 80.0f);
    ImGui::DragInt("Coin Emitters", &coinMaxEmitters_, 1.0f, 1, 64);

    ImGui::SeparatorText("Enemy Aura");
    ImGui::DragInt("Aura Count / enemy", &enemyAuraCount_, 1.0f, 0, 128);
    ImGui::DragFloat("Aura Life", &enemyAuraLife_, 0.01f, 0.05f, 5.0f);
    ImGui::DragFloat("Aura Scale", &enemyAuraScale_, 0.01f, 0.02f, 6.0f);
    ImGui::DragFloat("Aura Rise", &enemyAuraRise_, 0.05f, 0.0f, 6.0f);
    ImGui::DragFloat("Enemy Range", &enemyRange_, 0.5f, 2.0f, 80.0f);
    ImGui::DragInt("Enemy Emitters", &enemyMaxEmitters_, 1.0f, 1, 64);

    ImGui::SeparatorText("Bursts");
    ImGui::DragInt("Hit Splash", &hitSplashCount_, 1.0f, 0, 256);
    ImGui::DragFloat("Hit Speed", &hitSplashSpeed_, 0.1f, 0.0f, 30.0f);
    ImGui::DragInt("Defeat Splash", &defeatSplashCount_, 1.0f, 0, 256);
    ImGui::DragFloat("Defeat Speed", &defeatSplashSpeed_, 0.1f, 0.0f, 30.0f);
    ImGui::DragInt("Split Burst", &splitBurstCount_, 1.0f, 0, 512);
    ImGui::DragFloat("Split Speed", &splitBurstSpeed_, 0.1f, 0.0f, 40.0f);

    ImGui::SeparatorText("Gaming Color Field");
    ImGui::DragFloat("Time Scale", &gamingTimeScale_, 0.01f, 0.0f, 4.0f);
    ImGui::DragFloat("Space Scale", &gamingSpaceScale_, 0.005f, 0.0f, 0.5f);
    ImGui::DragFloat("Gain", &gamingGain_, 0.01f, 0.05f, 1.5f);

    // 手動で単発を確認する
    ImGui::SeparatorText("Test");
    if (ImGui::Button("Hit Splash"))
    {
        EmitEnemyHitSplash({ focusCenter_.x, focusCenter_.y + 0.5f, focusCenter_.z },
                           { 0.2f, 0.85f, 1.0f, 1.0f });
    }
    ImGui::SameLine();
    if (ImGui::Button("Defeat"))
    {
        EmitEnemyDefeat({ focusCenter_.x, focusCenter_.y + 0.5f, focusCenter_.z }, 3);
    }
    ImGui::SameLine();
    if (ImGui::Button("Split Burst"))
    {
        EmitPlayerSplit({ focusCenter_.x, focusCenter_.y + 0.5f, focusCenter_.z }, 5);
    }
#endif
}
