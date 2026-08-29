#include "LootSystem.h"
#include "../../Player/Player.h"
#include "Application/Particle/AppParticleManager.h"
#include "RaidStats.h"
#include <Windows.h>
#include <cmath>
#include <cstdlib>

void LootSystem::Initialize()
{
    props_.clear();

    // --- マップ各所に戦略的物資ボックスを配置 ---
    LootableProp crate1;
    crate1.position = { -6.0f, 0.0f, 10.0f };
    crate1.name = "MILITARY CRATE (MEDKIT)";
    crate1.type = LootType::Medkit;
    crate1.value = 1;
    crate1.maxSearchTime = kDefaultSearchDuration;
    props_.push_back(crate1);

    LootableProp crate2;
    crate2.position = { 7.0f, 0.0f, 18.0f };
    crate2.name = "AMMO CRATE (7.62x39mm)";
    crate2.type = LootType::AmmoBox;
    crate2.value = kAmmoBoxSupplyCount;
    crate2.maxSearchTime = kDefaultSearchDuration;
    props_.push_back(crate2);

    LootableProp crate3;
    crate3.position = { 0.0f, 0.0f, 25.0f };
    crate3.name = "GOLDEN DUCK STATUE";
    crate3.type = LootType::GoldDuck;
    crate3.value = kGoldDuckValue;
    crate3.maxSearchTime = kDefaultSearchDuration + 0.3f;
    props_.push_back(crate3);
}

void LootSystem::Reset()
{
    Initialize();
}

void LootSystem::SpawnCorpseLoot(const Vector3& position, const std::string& enemyName)
{
    LootableProp corpse;
    corpse.position = position;
    corpse.name = "DEAD BODY // " + enemyName;
    corpse.maxSearchTime = kCorpseSearchDuration;

    // ランダムな戦利品（弾薬、救急キット、またはルーブル現金）を決定
    int r = rand() % 3;
    if (r == 0)
    {
        corpse.type = LootType::AmmoBox;
        corpse.value = kAmmoBoxSupplyCount;
    }
    else if (r == 1)
    {
        corpse.type = LootType::Medkit;
        corpse.value = 1;
    }
    else
    {
        corpse.type = LootType::Roubles;
        corpse.value = kCashMinRoubles + (rand() % (kCashMaxRoubles - kCashMinRoubles + 1));
    }
    props_.push_back(corpse);
}

void LootSystem::Update(float deltaTime, Player* player, AppParticleManager* particleMgr, std::vector<FloatingText>& floatingTexts)
{
    if (!player || player->IsDead()) return;

    const Vector3 playerPos = player->GetPosition();
    const bool isEKeyDown = (GetAsyncKeyState('E') & 0x8000) != 0;

    for (auto& prop : props_)
    {
        if (prop.isLooted) continue;

        // プレイヤーと物資プロップ間の平面距離
        float dx = prop.position.x - playerPos.x;
        float dz = prop.position.z - playerPos.z;
        float dist = std::sqrt(dx * dx + dz * dz);

        if (dist <= kInteractRadius)
        {
            if (isEKeyDown && !player->IsDodging())
            {
                // [E]キー長押しで探索進捗を加算
                prop.searchTimer += deltaTime;

                // 探索完了時のアイテム獲得処理
                if (prop.searchTimer >= prop.maxSearchTime)
                {
                    prop.isLooted = true;
                    prop.searchTimer = prop.maxSearchTime;

                    if (prop.type == LootType::Medkit)
                    {
                        player->AddMedkits(prop.value);
                        player->AddLootValue(5000);

                        FloatingText ft;
                        ft.position = prop.position + Vector3{ 0.0f, 1.2f, 0.0f };
                        ft.text = "+1 MEDKIT (HP+40)";
                        ft.color = { 0.0f, 1.0f, 0.6f, 1.0f };
                        ft.lifeTime = ft.maxLifeTime = 1.6f;
                        ft.isCritical = true;
                        floatingTexts.push_back(ft);
                    }
                    else if (prop.type == LootType::AmmoBox)
                    {
                        player->AddReserveAmmo(prop.value);
                        player->AddLootValue(3000);

                        FloatingText ft;
                        ft.position = prop.position + Vector3{ 0.0f, 1.2f, 0.0f };
                        char abuf[64];
                        sprintf_s(abuf, "+%d AMMO", prop.value);
                        ft.text = abuf;
                        ft.color = { 0.2f, 0.8f, 1.0f, 1.0f };
                        ft.lifeTime = ft.maxLifeTime = 1.6f;
                        ft.isCritical = true;
                        floatingTexts.push_back(ft);
                    }
                    else if (prop.type == LootType::GoldDuck)
                    {
                        player->AddLootValue(prop.value);

                        FloatingText ft;
                        ft.position = prop.position + Vector3{ 0.0f, 1.2f, 0.0f };
                        char gbuf[64];
                        sprintf_s(gbuf, "✨ GOLDEN DUCK! +$%d", prop.value);
                        ft.text = gbuf;
                        ft.color = { 1.0f, 0.85f, 0.2f, 1.0f };
                        ft.lifeTime = ft.maxLifeTime = 2.2f;
                        ft.isCritical = true;
                        floatingTexts.push_back(ft);
                    }
                    else if (prop.type == LootType::Roubles)
                    {
                        player->AddLootValue(prop.value);

                        FloatingText ft;
                        ft.position = prop.position + Vector3{ 0.0f, 1.2f, 0.0f };
                        char rbuf[64];
                        sprintf_s(rbuf, "+$%d ROUBLES", prop.value);
                        ft.text = rbuf;
                        ft.color = { 1.0f, 0.9f, 0.3f, 1.0f };
                        ft.lifeTime = ft.maxLifeTime = 1.6f;
                        ft.isCritical = false;
                        floatingTexts.push_back(ft);
                    }
                }
            }
            else
            {
                // [E]を離した場合は探索進捗が徐々に減衰
                prop.searchTimer -= deltaTime * 1.5f;
                if (prop.searchTimer < 0.0f) prop.searchTimer = 0.0f;
            }
        }
        else
        {
            // 離れた場合は探索進捗をリセット
            prop.searchTimer = 0.0f;
        }
    }
}
