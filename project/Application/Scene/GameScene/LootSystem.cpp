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
    struct InitialCrateSpec { Vector3 position; const char8_t* name; LootType type; int value; float maxSearchTime; };

    constexpr std::array<InitialCrateSpec, 6> kInitialCrates = { {
        { {  13.8f, 0.0f,  2.4f }, u8"弾薬箱",       LootType::AmmoBox,  LootSystem::kAmmoBoxSupplyCount, 1.0f },                                     // 1. 東ウイング入口
        { {  28.8f, 0.0f, 20.5f }, u8"現金ケース",   LootType::Roubles,  15000,                           LootSystem::kDefaultSearchDuration },        // 2. 東突端スパー
        { { -25.2f, 0.0f, 16.8f }, u8"救急キット",   LootType::Medkit,   1,                               LootSystem::kDefaultSearchDuration },        // 3. 西ドッグレッグ訓練ヤード
        { {  -9.5f, 0.0f, 39.0f }, u8"予備弾薬箱",   LootType::AmmoBox,  LootSystem::kAmmoBoxSupplyCount, LootSystem::kDefaultSearchDuration },        // 4. 北西ウイング
        { { -23.5f, 0.0f, 59.8f }, u8"黄金アヒル像", LootType::GoldDuck, LootSystem::kGoldDuckValue,      LootSystem::kDefaultSearchDuration + 0.35f },// 5. 北西突出ホーン
        { {  23.2f, 0.0f, 54.2f }, u8"医療キット",   LootType::Medkit,   1,                               LootSystem::kDefaultSearchDuration },        // 6. 北東八角ヘリパッド半島
    } };

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
        crate.maxSearchTime = spec.maxSearchTime;
        props_.push_back(crate);
    }
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

    for (auto& prop : props_)
    {
        if (prop.isLooted) continue;

        // プレイヤーと物資プロップ間の平面距離（平方根を省いた二乗距離判定）
        const float dx = prop.position.x - playerPos.x;
        const float dz = prop.position.z - playerPos.z;

        if (dx * dx + dz * dz <= kInteractRadiusSq)
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
