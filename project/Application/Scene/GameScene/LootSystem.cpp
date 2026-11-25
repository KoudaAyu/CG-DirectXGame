#include "LootSystem.h"
#include "../../Player/Player.h"
#include "Application/Audio/TacticalAudio.h"
#include "Application/Config/GameConfig.h"
#include "Application/Particle/AppParticleManager.h"
#include "Application/UI/TacticalUICanvas.h"
#include "RaidStats.h"
#include <Windows.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace
{
    constexpr float kTwoPi = GameConfig::Math::kTwoPi;

    // 非矩形・非対称マルチウイング要塞の探索導線に沿った戦術物資キャッシュ 6箇所
    struct InitialCrateSpec { Vector3 position; const char8_t* name; LootType type; int value; float maxSearchTime; int breakHits; };

    constexpr std::array<InitialCrateSpec, 6> kInitialCrates = { {
        { {  13.8f, 0.0f,  2.4f }, u8"弾薬箱",       LootType::AmmoBox,  LootSystem::kAmmoBoxSupplyCount, 1.0f,                                      2 }, // 1. 東ウイング入口（2発で軽快に破壊＆補給可能）
        { {  28.8f, 0.0f, 20.5f }, u8"現金ケース",   LootType::Roubles,  15000,                           LootSystem::kDefaultSearchDuration,         3 }, // 2. 東突端スパー
        { { -25.2f, 0.0f, 16.8f }, u8"救急キット",   LootType::Medkit,   1,                               LootSystem::kDefaultSearchDuration,         3 }, // 3. 西ドッグレッグ訓練ヤード
        { {  -9.5f, 0.0f, 39.0f }, u8"予備弾薬箱",   LootType::AmmoBox,  LootSystem::kAmmoBoxSupplyCount, LootSystem::kDefaultSearchDuration,         3 }, // 4. 北西ウイング
        { { -23.5f, 0.0f, 59.8f }, u8"黄金アヒル像", LootType::GoldDuck, LootSystem::kGoldDuckValue,      LootSystem::kDefaultSearchDuration + 0.35f, 3 }, // 5. 北西突出ホーン
        { {  23.2f, 0.0f, 54.2f }, u8"医療キット",   LootType::Medkit,   1,                               LootSystem::kDefaultSearchDuration,         3 }, // 6. 北東八角ヘリパッド半島
    } };

    constexpr float kBreakableCrateMatchRadiusSq = 0.75f * 0.75f;
    constexpr float kCrateBreakSeVolume = 0.80f, kCrateHitSeVolume = 0.62f;
    constexpr int   kSmashPlankCount = 28, kSmashDustCount = 14;

    // 死体ドロップ・探索進行・SE・報酬定数
    constexpr int    kWardenGuaranteedRoubles = 12000, kCorpseRollTypeCount = 3, kSingleMedkitCount = 1;
    constexpr int    kMedkitLootBonusValue    = 5000,  kAmmoBoxLootBonusValue = 3000, kRummageSeCooldownMs = 90;
    constexpr float  kInteractRadiusSq        = GameConfig::Math::Square(LootSystem::kInteractRadius);
    constexpr float  kRummageTickRate         = 5.0f,  kSearchReleaseDecayRate = 1.5f, kFloatingTextOffsetY = 1.2f;
    constexpr float  kRummageSeVolume = 0.48f, kLootOpenSeVolume = 0.72f, kPickupItemSeVolume = 0.74f;
    constexpr float  kPickupGoldDuckSeVolume = 0.82f, kPickupDuckQuackSeVolume = 0.62f;
    constexpr size_t kFloatingTextBufferSize  = 64;

    struct RewardTextSpec { Vector4 color; float lifeTime; bool isCritical; };
    constexpr RewardTextSpec kMedkitTextSpec   = { { 0.0f, 1.0f, 0.6f, 1.0f },  1.6f, true  };
    constexpr RewardTextSpec kAmmoTextSpec     = { { 0.2f, 0.8f, 1.0f, 1.0f },  1.6f, true  };
    constexpr RewardTextSpec kGoldDuckTextSpec = { { 1.0f, 0.85f, 0.2f, 1.0f }, 2.2f, true  };
    constexpr RewardTextSpec kRoublesTextSpec  = { { 1.0f, 0.9f, 0.3f, 1.0f },  1.6f, false };

    // 探索・開梱パーティクル共通プリセット
    struct LootParticleStyle
    {
        Vector2 aspect;
        Vector4 color;
        Vector4 endColor;
        float gravity, drag, bounce, fadeInRatio, twinkleSpeed;
        uint16_t slotLayer;
    };

    constexpr LootParticleStyle kRummageStyle   = { { 1.25f, 0.90f }, { 0.68f, 0.60f, 0.48f, 0.24f }, { 0.64f, 0.58f, 0.48f, 0.0f }, 0.0f,  2.6f, 0.0f,  0.14f, 0.0f,  1 };
    constexpr LootParticleStyle kOpenDustStyle  = { { 1.35f, 0.85f }, { 0.70f, 0.62f, 0.48f, 0.34f }, { 0.66f, 0.60f, 0.50f, 0.0f }, 0.0f,  2.8f, 0.0f,  0.12f, 0.0f,  1 };
    constexpr LootParticleStyle kOpenChipStyle  = { { 1.0f,  1.0f  }, { 0.68f, 0.52f, 0.30f, 0.92f }, { 0.52f, 0.38f, 0.22f, 0.0f }, 14.5f, 1.2f, 0.35f, 0.0f,  0.0f,  2 };
    constexpr LootParticleStyle kOpenGlintStyle = { { 0.75f, 1.45f }, { 1.0f,  0.88f, 0.36f, 0.85f }, { 1.0f,  0.55f, 0.12f, 0.0f }, 0.0f,  1.8f, 0.0f,  0.0f,  16.0f, 2 };

    // 各エフェクト固有の放出レンジ定数
    constexpr float kRummageOffsetXZ = 0.22f, kRummageOffsetY = 0.42f, kRummageInitSc = 0.09f, kRummageTargetSc = 0.28f;
    constexpr float kRummageVelXZ = 0.45f, kRummageVelYMin = 0.28f, kRummageVelYMax = 0.62f, kRummageLife = 0.38f;
    constexpr int   kDustCount = 5, kChipCount = 6, kGlintGoldCount = 8, kGlintCashCount = 4;
    constexpr float kDustAngleStep = kTwoPi / static_cast<float>(kDustCount), kDustAngleJitter = 0.2f;
    constexpr float kDustRadius = 0.18f, kDustOffsetY = 0.48f, kDustScMin = 0.14f, kDustScMax = 0.22f, kDustTargetMul = 2.4f;
    constexpr float kDustRadSpdMin = 0.9f, kDustRadSpdMax = 1.8f, kDustUpSpdMin = 0.45f, kDustUpSpdMax = 1.15f, kDustLifeMin = 0.45f, kDustLifeMax = 0.72f;
    constexpr float kChipOffsetXZ = 0.18f, kChipOffsetY = 0.50f, kChipWMin = 0.018f, kChipWMax = 0.032f, kChipLMin = 0.048f, kChipLMax = 0.095f;
    constexpr float kChipTargetSc = 0.80f, kChipRadSpdMin = 1.2f, kChipRadSpdMax = 2.8f, kChipUpSpdMin = 1.4f, kChipUpSpdMax = 3.0f;
    constexpr float kChipSpinMax = 12.0f, kChipLifeMin = 0.35f, kChipLifeMax = 0.62f;
    constexpr float kGlintOffsetXZ = 0.22f, kGlintOffsetY = 0.55f, kGlintScMin = 0.025f, kGlintScMax = 0.045f, kGlintTargetMul = 0.30f;
    constexpr float kGlintRadSpdMin = 0.4f, kGlintRadSpdMax = 1.1f, kGlintUpSpdMin = 0.9f, kGlintUpSpdMax = 1.8f, kGlintLifeMin = 0.42f, kGlintLifeMax = 0.75f;

    float RandRange(float a, float b)
    {
        return a + (b - a) * (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX));
    }

    void EmitStyledParticle(
        AppParticleManager* mgr, const Vector3& pos, float rotZ, const Vector2& aspect,
        float initScale, float targetScale, const Vector3& velocity, float lifeTime,
        uint32_t tex, const LootParticleStyle& style, float angularVel = 0.0f, float twinklePhase = 0.0f)
    {
        AppParticle p{};
        p.transform.Initialize();
        p.transform.SetTranslate(pos);
        p.transform.SetRotate({ 0.0f, 0.0f, rotZ });
        p.scaleAspect = aspect;
        p.initialScale = initScale;
        p.targetScale = targetScale;
        p.transform.SetScale({ initScale * aspect.x, initScale * aspect.y, 1.0f });
        p.velocity = velocity;
        p.color = style.color;
        p.endColor = style.endColor;
        p.hasColorShift = true;
        p.gravity = style.gravity;
        p.drag = style.drag;
        p.bounceElasticity = style.bounce;
        p.angularVelocity = angularVel;
        p.lifeTime = lifeTime;
        p.currentTime = 0.0f;
        p.textureIndex = tex;
        p.fadeInRatio = style.fadeInRatio;
        p.twinkleSpeed = style.twinkleSpeed;
        p.twinklePhase = twinklePhase;
        p.slotLayer = style.slotLayer;
        mgr->AddParticle(p);
    }

    void SpawnLootOpenEffects(
        const LootableProp& prop, AppParticleManager* particleMgr, AppParticleManager* smokeMgr,
        uint32_t dustTex, uint32_t chipTex, uint32_t sparkTex)
    {
        // 木箱開梱時のリアルな埃プルーム＆木片チップ（高額品は控えめなゴールドダストきらめき）
        if (smokeMgr && dustTex != UINT32_MAX)
        {
            for (int k = 0; k < kDustCount; ++k)
            {
                const float a = k * kDustAngleStep + RandRange(-kDustAngleJitter, kDustAngleJitter);
                const float cosA = std::cos(a), sinA = std::sin(a), initSc = RandRange(kDustScMin, kDustScMax);
                EmitStyledParticle(
                    smokeMgr, prop.position + Vector3{ cosA * kDustRadius, kDustOffsetY, sinA * kDustRadius },
                    RandRange(0.0f, kTwoPi), kOpenDustStyle.aspect, initSc, initSc * kDustTargetMul,
                    { cosA * RandRange(kDustRadSpdMin, kDustRadSpdMax), RandRange(kDustUpSpdMin, kDustUpSpdMax), sinA * RandRange(kDustRadSpdMin, kDustRadSpdMax) },
                    RandRange(kDustLifeMin, kDustLifeMax), dustTex, kOpenDustStyle);
            }
        }
        if (smokeMgr && chipTex != UINT32_MAX)
        {
            for (int k = 0; k < kChipCount; ++k)
            {
                const float a = RandRange(0.0f, kTwoPi), cosA = std::cos(a), sinA = std::sin(a);
                EmitStyledParticle(
                    smokeMgr, prop.position + Vector3{ RandRange(-kChipOffsetXZ, kChipOffsetXZ), kChipOffsetY, RandRange(-kChipOffsetXZ, kChipOffsetXZ) },
                    RandRange(0.0f, kTwoPi), { RandRange(kChipWMin, kChipWMax), RandRange(kChipLMin, kChipLMax) }, 1.0f, kChipTargetSc,
                    { cosA * RandRange(kChipRadSpdMin, kChipRadSpdMax), RandRange(kChipUpSpdMin, kChipUpSpdMax), sinA * RandRange(kChipRadSpdMin, kChipRadSpdMax) },
                    RandRange(kChipLifeMin, kChipLifeMax), chipTex, kOpenChipStyle, RandRange(-kChipSpinMax, kChipSpinMax));
            }
        }
        if (particleMgr && sparkTex != UINT32_MAX && (prop.type == LootType::GoldDuck || prop.type == LootType::Roubles))
        {
            const int glintCount = (prop.type == LootType::GoldDuck) ? kGlintGoldCount : kGlintCashCount;
            for (int k = 0; k < glintCount; ++k)
            {
                const float a = RandRange(0.0f, kTwoPi), cosA = std::cos(a), sinA = std::sin(a), sc = RandRange(kGlintScMin, kGlintScMax);
                EmitStyledParticle(
                    particleMgr, prop.position + Vector3{ RandRange(-kGlintOffsetXZ, kGlintOffsetXZ), kGlintOffsetY, RandRange(-kGlintOffsetXZ, kGlintOffsetXZ) },
                    a, kOpenGlintStyle.aspect, sc, sc * kGlintTargetMul,
                    { cosA * RandRange(kGlintRadSpdMin, kGlintRadSpdMax), RandRange(kGlintUpSpdMin, kGlintUpSpdMax), sinA * RandRange(kGlintRadSpdMin, kGlintRadSpdMax) },
                    RandRange(kGlintLifeMin, kGlintLifeMax), sparkTex, kOpenGlintStyle, 0.0f, a);
            }
        }
    }

    void PushRewardFloatingText(std::vector<FloatingText>& floatingTexts, const Vector3& propPos, const char* text, const RewardTextSpec& spec)
    {
        FloatingText ft;
        ft.position = propPos + Vector3{ 0.0f, kFloatingTextOffsetY, 0.0f };
        ft.text = text;
        ft.color = spec.color;
        ft.lifeTime = ft.maxLifeTime = spec.lifeTime;
        ft.isCritical = spec.isCritical;
        floatingTexts.push_back(ft);
    }

    void GrantLootReward(const LootableProp& prop, Player* player, std::vector<FloatingText>& floatingTexts)
    {
        char textBuf[kFloatingTextBufferSize]{};
        auto& stats = RaidStats::GetInstance();

        switch (prop.type)
        {
        case LootType::Medkit:
            TacticalAudio::Play2D("Resources/Audio/SE_Pickup_Medkit.wav", kPickupItemSeVolume);
            player->AddMedkits(prop.value);
            player->AddLootValue(kMedkitLootBonusValue);
            stats.lootedMedkits += prop.value;
            PushRewardFloatingText(floatingTexts, prop.position, "+1 MEDKIT", kMedkitTextSpec);
            break;
        case LootType::AmmoBox:
            TacticalAudio::Play2D("Resources/Audio/SE_Pickup_Ammo.wav", kPickupItemSeVolume);
            player->AddReserveAmmo(prop.value);
            player->AddLootValue(kAmmoBoxLootBonusValue);
            stats.lootedAmmoBoxes++;
            sprintf_s(textBuf, "+%d AMMO", prop.value);
            PushRewardFloatingText(floatingTexts, prop.position, textBuf, kAmmoTextSpec);
            break;
        case LootType::GoldDuck:
            TacticalAudio::Play2D("Resources/Audio/SE_Pickup_GoldDuck.wav", kPickupGoldDuckSeVolume);
            TacticalAudio::Play2D("Resources/Audio/SE_Duck.mp3", kPickupDuckQuackSeVolume);
            player->AddLootValue(prop.value);
            stats.lootedGoldDucks++;
            sprintf_s(textBuf, "+%d RUB", prop.value);
            PushRewardFloatingText(floatingTexts, prop.position, textBuf, kGoldDuckTextSpec);
            break;
        case LootType::Roubles:
            TacticalAudio::Play2D("Resources/Audio/SE_Pickup_Cash.wav", kPickupItemSeVolume);
            player->AddLootValue(prop.value);
            stats.lootedCashCount++;
            stats.lootedCashValue += prop.value;
            sprintf_s(textBuf, "+%d RUB", prop.value);
            PushRewardFloatingText(floatingTexts, prop.position, textBuf, kRoublesTextSpec);
            break;
        }
    }
}

void LootSystem::Initialize()
{
    props_.clear();
    props_.reserve(kInitialCrates.size());

    for (const auto& spec : kInitialCrates)
    {
        LootableProp crate;
        crate.position = spec.position;
        crate.name = reinterpret_cast<const char*>(spec.name);
        crate.type = spec.type;
        crate.value = spec.value;
        crate.isBreakableCrate = true;
        crate.crateHp = crate.maxCrateHp = spec.breakHits;
        crate.maxSearchTime = spec.maxSearchTime;
        props_.push_back(crate);
    }
}

bool LootSystem::TryDamageBreakableCrate(
    const Vector3& obstaclePos,
    Player* player,
    AppParticleManager* particleMgr,
    std::vector<FloatingText>& floatingTexts,
    bool& outBroken,
    AppParticleManager* smokeMgr,
    uint32_t dustTex,
    uint32_t chipTex,
    uint32_t sparkTex)
{
    outBroken = false;
    if (!player || player->IsDead()) return false;

    for (auto& prop : props_)
    {
        if (prop.isLooted || !prop.isBreakableCrate) continue;

        const float dx = prop.position.x - obstaclePos.x;
        const float dz = prop.position.z - obstaclePos.z;
        if (dx * dx + dz * dz > kBreakableCrateMatchRadiusSq) continue;

        prop.crateHp--;
        if (prop.crateHp > 0)
        {
            TacticalAudio::Play2D("Resources/Audio/SE_Crate_Crack.wav", 0.78f, 50, "crate_hit");
            if (smokeMgr && chipTex != UINT32_MAX)
            {
                for (int k = 0; k < 9; ++k)
                {
                    const float a = RandRange(0.0f, kTwoPi);
                    EmitStyledParticle(
                        smokeMgr,
                        prop.position + Vector3{ RandRange(-0.28f, 0.28f), RandRange(0.32f, 0.82f), RandRange(-0.28f, 0.28f) },
                        RandRange(0.0f, kTwoPi), { RandRange(0.026f, 0.050f), RandRange(0.080f, 0.165f) }, 1.0f, 0.80f,
                        { std::cos(a) * RandRange(1.8f, 4.0f), RandRange(1.6f, 3.6f), std::sin(a) * RandRange(1.8f, 4.0f) },
                        RandRange(0.40f, 0.72f), chipTex, kOpenChipStyle, RandRange(-15.0f, 15.0f));
                }
            }
            if (smokeMgr && dustTex != UINT32_MAX)
            {
                for (int k = 0; k < 4; ++k)
                {
                    const float a = RandRange(0.0f, kTwoPi);
                    const float sc = RandRange(0.15f, 0.24f);
                    EmitStyledParticle(
                        smokeMgr,
                        prop.position + Vector3{ std::cos(a) * 0.22f, 0.48f, std::sin(a) * 0.22f },
                        RandRange(0.0f, kTwoPi), kOpenDustStyle.aspect, sc, sc * 2.4f,
                        { std::cos(a) * RandRange(0.9f, 1.9f), RandRange(0.45f, 1.15f), std::sin(a) * RandRange(0.9f, 1.9f) },
                        RandRange(0.42f, 0.68f), dustTex, kOpenDustStyle);
                }
            }
            if (particleMgr && sparkTex != UINT32_MAX)
            {
                for (int k = 0; k < 5; ++k)
                {
                    const float a = RandRange(0.0f, kTwoPi), sc = RandRange(0.024f, 0.042f);
                    EmitStyledParticle(
                        particleMgr,
                        prop.position + Vector3{ RandRange(-0.30f, 0.30f), RandRange(0.42f, 0.80f), RandRange(-0.30f, 0.30f) },
                        a, { 0.55f, 1.65f }, sc, sc * 0.28f,
                        { std::cos(a) * RandRange(2.2f, 4.6f), RandRange(1.2f, 2.8f), std::sin(a) * RandRange(2.2f, 4.6f) },
                        RandRange(0.16f, 0.30f), sparkTex, kOpenGlintStyle, 0.0f, a);
                }
            }
            return true;
        }

        // 木箱破壊完了：破片＆砂煙エフェクトを放出し、その場で中身をプレイヤーへ即時支給！
        outBroken = true;
        prop.isLooted = true;
        prop.searchTimer = prop.maxSearchTime;
        TacticalAudio::Play2D("Resources/Audio/SE_Crate_Break.wav", 0.90f, 60, "crate_break");
        TacticalAudio::Play2D("Resources/Audio/SE_Loot_Open.wav", kLootOpenSeVolume, 60, "crate_open");

        SpawnLootOpenEffects(prop, particleMgr, smokeMgr, dustTex, chipTex, sparkTex);
        // 弾薬箱・救急キット破壊時もアイテム種別カラーのきらめき補給粒子＋破砕金具スパークを放出
        if (particleMgr && sparkTex != UINT32_MAX)
        {
            LootParticleStyle burstStyle = kOpenGlintStyle;
            if (prop.type == LootType::AmmoBox)
            {
                burstStyle.color    = { 0.38f, 0.90f, 1.0f, 0.92f };
                burstStyle.endColor = { 0.15f, 0.55f, 1.0f, 0.0f };
            }
            else if (prop.type == LootType::Medkit)
            {
                burstStyle.color    = { 0.25f, 1.0f, 0.68f, 0.92f };
                burstStyle.endColor = { 0.10f, 0.72f, 0.42f, 0.0f };
            }

            const int glintTotal = (prop.type == LootType::GoldDuck) ? 16 : 13;
            for (int k = 0; k < glintTotal; ++k)
            {
                const float a = RandRange(0.0f, kTwoPi), sc = RandRange(0.034f, 0.064f);
                EmitStyledParticle(
                    particleMgr,
                    prop.position + Vector3{ RandRange(-0.28f, 0.28f), RandRange(0.45f, 0.72f), RandRange(-0.28f, 0.28f) },
                    a, burstStyle.aspect, sc, sc * 0.34f,
                    { std::cos(a) * RandRange(0.8f, 2.4f), RandRange(1.6f, 3.4f), std::sin(a) * RandRange(0.8f, 2.4f) },
                    RandRange(0.55f, 0.98f), sparkTex, burstStyle, 0.0f, a);
            }
        }
        if (smokeMgr && chipTex != UINT32_MAX)
        {
            for (int k = 0; k < kSmashPlankCount; ++k)
            {
                const float a = RandRange(0.0f, kTwoPi), cosA = std::cos(a), sinA = std::sin(a);
                const float radSpd = RandRange(2.2f, 5.4f);
                EmitStyledParticle(
                    smokeMgr,
                    prop.position + Vector3{ RandRange(-0.32f, 0.32f), RandRange(0.22f, 0.72f), RandRange(-0.32f, 0.32f) },
                    RandRange(0.0f, kTwoPi), { RandRange(0.032f, 0.068f), RandRange(0.095f, 0.21f) }, 1.0f, 0.78f,
                    { cosA * radSpd, RandRange(2.0f, 4.8f), sinA * radSpd },
                    RandRange(0.45f, 0.85f), chipTex, kOpenChipStyle, RandRange(-16.0f, 16.0f));
            }
        }
        if (smokeMgr && dustTex != UINT32_MAX)
        {
            for (int k = 0; k < kSmashDustCount; ++k)
            {
                const float a = static_cast<float>(k) * (kTwoPi / static_cast<float>(kSmashDustCount)) + RandRange(-0.2f, 0.2f);
                const float initSc = RandRange(0.20f, 0.34f);
                EmitStyledParticle(
                    smokeMgr,
                    prop.position + Vector3{ std::cos(a) * 0.25f, 0.42f, std::sin(a) * 0.25f },
                    RandRange(0.0f, kTwoPi), kOpenDustStyle.aspect, initSc, initSc * 2.8f,
                    { std::cos(a) * RandRange(1.4f, 2.8f), RandRange(0.6f, 1.6f), std::sin(a) * RandRange(1.4f, 2.8f) },
                    RandRange(0.50f, 0.85f), dustTex, kOpenDustStyle);
            }
        }

        GrantLootReward(prop, player, floatingTexts);
        return true;
    }
    return false;
}

void LootSystem::Reset()
{
    Initialize();
}

void LootSystem::SpawnCorpseLoot(const Vector3& position, const std::string& enemyName)
{
    const bool isWarden = (enemyName == "THE WARDEN");

    LootableProp corpse;
    corpse.position = position;
    corpse.name = isWarden
        ? reinterpret_cast<const char*>(u8"看守長の戦利品")
        : reinterpret_cast<const char*>(u8"戦利品");
    corpse.maxSearchTime = kCorpseSearchDuration;

    if (isWarden)
    {
        // 看守長の死体からは確定で高額ルーブル束をドロップ
        corpse.type = LootType::Roubles;
        corpse.value = kWardenGuaranteedRoubles;
        props_.push_back(corpse);
        return;
    }

    // ランダムな戦利品（弾薬、救急キット、またはルーブル現金）を決定
    const int r = std::rand() % kCorpseRollTypeCount;
    if (r == 0)
    {
        corpse.type = LootType::AmmoBox;
        corpse.value = kAmmoBoxSupplyCount;
    }
    else if (r == 1)
    {
        corpse.type = LootType::Medkit;
        corpse.value = kSingleMedkitCount;
    }
    else
    {
        corpse.type = LootType::Roubles;
        corpse.value = kCashMinRoubles + (std::rand() % (kCashMaxRoubles - kCashMinRoubles + 1));
    }
    props_.push_back(corpse);
}

void LootSystem::Update(
    float deltaTime,
    Player* player,
    AppParticleManager* particleMgr,
    std::vector<FloatingText>& floatingTexts,
    AppParticleManager* smokeMgr,
    uint32_t dustTex,
    uint32_t chipTex,
    uint32_t sparkTex)
{
    if (!player || player->IsDead()) return;

    const Vector3 playerPos = player->GetPosition();
    const bool canSearch = IsAppKeyDown('E') && !player->IsDodging();

    // --- 未回収の物資木箱・黄金アヒル像・戦利品ドロップから立ち昇る常時ビーコン粒子演出 ---
    auraPhase_ += deltaTime;
    auraEmitTimer_ += deltaTime;
    const bool emitAuraTick = (auraEmitTimer_ >= 0.055f);
    if (emitAuraTick)
    {
        auraEmitTimer_ = std::fmod(auraEmitTimer_, 0.055f);
    }

    constexpr float kAuraVisibleDistSq = 32.0f * 32.0f;
    size_t propIdx = 0;

    for (auto& prop : props_)
    {
        ++propIdx;
        if (prop.isLooted) continue;

        // プレイヤーと物資プロップ間の平面距離（平方根を省いた二乗距離判定）
        const float dx = prop.position.x - playerPos.x;
        const float dz = prop.position.z - playerPos.z;
        const float distSq = dx * dx + dz * dz;

        if (emitAuraTick && particleMgr && sparkTex != UINT32_MAX && distSq <= kAuraVisibleDistSq)
        {
            const float baseAng = auraPhase_ * 2.35f + static_cast<float>(propIdx) * 1.37f;
            const float orbitR = prop.isBreakableCrate ? 0.46f : 0.30f;
            const float baseY  = prop.isBreakableCrate ? 0.28f : 0.14f;

            if (prop.type == LootType::GoldDuck)
            {
                // レジェンダリー「黄金アヒル像」：二重螺旋の黄金ホタル粒子＋王冠スターダスト＋金色の陽炎
                for (int s = 0; s < 3; ++s)
                {
                    const float a = baseAng + static_cast<float>(s) * (kTwoPi / 3.0f);
                    const float r = orbitR * (0.65f + 0.35f * std::sin(auraPhase_ * 3.1f + static_cast<float>(s)));
                    AppParticle gp{};
                    gp.transform.Initialize();
                    gp.transform.SetTranslate(prop.position + Vector3{ std::cos(a) * r, baseY + RandRange(0.05f, 0.55f), std::sin(a) * r });
                    gp.scaleAspect = (s == 2) ? Vector2{ 0.95f, 0.95f } : Vector2{ 0.72f, 1.52f };
                    gp.initialScale = (s == 2) ? RandRange(0.052f, 0.078f) : RandRange(0.038f, 0.058f);
                    gp.targetScale = gp.initialScale * 0.24f;
                    gp.transform.SetScale({ gp.initialScale * gp.scaleAspect.x, gp.initialScale * gp.scaleAspect.y, 1.0f });
                    gp.velocity = { -std::sin(a) * 0.72f + RandRange(-0.18f, 0.18f), RandRange(0.95f, 1.85f), std::cos(a) * 0.72f + RandRange(-0.18f, 0.18f) };
                    gp.color = (s == 0)
                        ? Vector4{ 1.0f, 0.90f, 0.32f, 0.95f }
                        : Vector4{ 1.0f, 0.76f, 0.18f, 0.88f };
                    gp.endColor = { 1.0f, 0.42f, 0.06f, 0.0f };
                    gp.hasColorShift = true;
                    gp.drag = 1.2f;
                    gp.curlFreq = 3.2f;
                    gp.curlAmp = 0.22f;
                    gp.lifeTime = RandRange(0.68f, 1.08f);
                    gp.fadeInRatio = 0.14f;
                    gp.twinkleSpeed = 18.0f;
                    gp.twinklePhase = a;
                    gp.textureIndex = sparkTex;
                    gp.slotLayer = 2;
                    particleMgr->AddParticle(gp);
                }
            }
            else
            {
                // 弾薬（シアン＆真鍮）・医療キット（エメラルド）・ルーブル戦利品（アンバーゴールド）の識別オーラ粒子
                Vector4 startCol = { 1.0f, 0.84f, 0.30f, 0.84f };
                Vector4 endCol   = { 0.95f, 0.48f, 0.10f, 0.0f };
                if (prop.type == LootType::AmmoBox)
                {
                    const bool altBrass = ((std::rand() & 1) == 0);
                    startCol = altBrass ? Vector4{ 0.24f, 0.88f, 1.0f, 0.86f } : Vector4{ 1.0f, 0.78f, 0.30f, 0.80f };
                    endCol   = altBrass ? Vector4{ 0.08f, 0.52f, 0.92f, 0.0f } : Vector4{ 0.88f, 0.46f, 0.10f, 0.0f };
                }
                else if (prop.type == LootType::Medkit)
                {
                    startCol = { 0.22f, 1.0f, 0.66f, 0.88f };
                    endCol   = { 0.08f, 0.68f, 0.38f, 0.0f };
                }

                const int count = prop.isBreakableCrate ? 2 : 1;
                for (int s = 0; s < count; ++s)
                {
                    const float a = baseAng + static_cast<float>(s) * 3.14159f + RandRange(-0.35f, 0.35f);
                    const float r = orbitR * RandRange(0.55f, 1.05f);
                    AppParticle lp{};
                    lp.transform.Initialize();
                    lp.transform.SetTranslate(prop.position + Vector3{ std::cos(a) * r, baseY + RandRange(0.02f, 0.40f), std::sin(a) * r });
                    lp.scaleAspect = { 0.72f, 1.48f };
                    lp.initialScale = RandRange(0.032f, 0.048f);
                    lp.targetScale = lp.initialScale * 0.25f;
                    lp.transform.SetScale({ lp.initialScale * lp.scaleAspect.x, lp.initialScale * lp.scaleAspect.y, 1.0f });
                    lp.velocity = { -std::sin(a) * 0.48f, RandRange(0.72f, 1.35f), std::cos(a) * 0.48f };
                    lp.color = startCol;
                    lp.endColor = endCol;
                    lp.hasColorShift = true;
                    lp.drag = 1.4f;
                    lp.curlFreq = 2.6f;
                    lp.curlAmp = 0.16f;
                    lp.lifeTime = RandRange(0.55f, 0.90f);
                    lp.fadeInRatio = 0.16f;
                    lp.twinkleSpeed = 14.0f;
                    lp.twinklePhase = a;
                    lp.textureIndex = sparkTex;
                    lp.slotLayer = 2;
                    particleMgr->AddParticle(lp);
                }

                // 敵ドロップ戦利品には草むらでも見落とさないよう細い戦術シグナル薄煙を添える
                if (!prop.isBreakableCrate && smokeMgr && dustTex != UINT32_MAX && (std::rand() % 3 == 0))
                {
                    AppParticle wisp{};
                    wisp.transform.Initialize();
                    wisp.transform.SetTranslate(prop.position + Vector3{ RandRange(-0.10f, 0.10f), 0.18f, RandRange(-0.10f, 0.10f) });
                    wisp.transform.SetRotate({ 0.0f, 0.0f, RandRange(0.0f, kTwoPi) });
                    wisp.scaleAspect = { 0.80f, 1.40f };
                    wisp.initialScale = 0.10f;
                    wisp.targetScale = 0.28f;
                    wisp.transform.SetScale({ 0.08f, 0.14f, 1.0f });
                    wisp.velocity = { RandRange(-0.14f, 0.14f), RandRange(0.55f, 0.95f), RandRange(-0.14f, 0.14f) };
                    wisp.color = { startCol.x * 0.85f + 0.15f, startCol.y * 0.85f + 0.15f, startCol.z * 0.85f + 0.15f, 0.24f };
                    wisp.endColor = { startCol.x, startCol.y, startCol.z, 0.0f };
                    wisp.hasColorShift = true;
                    wisp.drag = 1.8f;
                    wisp.lifeTime = 0.65f;
                    wisp.fadeInRatio = 0.20f;
                    wisp.textureIndex = dustTex;
                    wisp.slotLayer = 1;
                    smokeMgr->AddParticle(wisp);
                }
            }
        }

        // 木箱物資は射撃で破壊して回収する仕様のため、未破壊状態での [E] すり抜け回収を抑止（敵ドロップ死体のみ [E] 探索）
        if (prop.isBreakableCrate)
        {
            prop.searchTimer = 0.0f;
            continue;
        }

        if (distSq <= kInteractRadiusSq)
        {
            if (canSearch)
            {
                const float prevTimer = prop.searchTimer;
                prop.searchTimer += deltaTime;

                if (static_cast<int>(prop.searchTimer * kRummageTickRate) != static_cast<int>(prevTimer * kRummageTickRate))
                {
                    TacticalAudio::Play2D("Resources/Audio/SE_Loot_Rummage.wav", kRummageSeVolume, kRummageSeCooldownMs);
                    if (smokeMgr && dustTex != UINT32_MAX)
                    {
                        EmitStyledParticle(
                            smokeMgr,
                            prop.position + Vector3{ RandRange(-kRummageOffsetXZ, kRummageOffsetXZ), kRummageOffsetY, RandRange(-kRummageOffsetXZ, kRummageOffsetXZ) },
                            RandRange(0.0f, kTwoPi), kRummageStyle.aspect, kRummageInitSc, kRummageTargetSc,
                            { RandRange(-kRummageVelXZ, kRummageVelXZ), RandRange(kRummageVelYMin, kRummageVelYMax), RandRange(-kRummageVelXZ, kRummageVelXZ) },
                            kRummageLife, dustTex, kRummageStyle);
                    }
                }

                if (prop.searchTimer >= prop.maxSearchTime)
                {
                    prop.isLooted = true;
                    prop.searchTimer = prop.maxSearchTime;
                    TacticalAudio::Play2D("Resources/Audio/SE_Loot_Open.wav", kLootOpenSeVolume);
                    SpawnLootOpenEffects(prop, particleMgr, smokeMgr, dustTex, chipTex, sparkTex);
                    GrantLootReward(prop, player, floatingTexts);
                }
            }
            else
            {
                prop.searchTimer -= deltaTime * kSearchReleaseDecayRate;
                if (prop.searchTimer < 0.0f) prop.searchTimer = 0.0f;
            }
        }
        else
        {
            prop.searchTimer = 0.0f;
        }
    }
}
