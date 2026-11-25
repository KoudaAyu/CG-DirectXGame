#pragma once
#include <string>

/// <summary>
/// レイド戦績データ管理シングルトン
/// 出撃から脱出/死亡までの戦績（時間、撃破数、命中率、獲得物資など）を保持し、
/// クリア画面・ゲームオーバー画面へリアルタイムに伝達します。
/// </summary>
struct RaidStats
{
    static constexpr float kDefaultMaxRaidTime = 300.0f; // 5分 (300秒)
    static constexpr int   kDefaultTotalTargets = 3;
    static constexpr float kDefaultMaxHp = 100.0f;
    static constexpr float kPercentMultiplier = 100.0f;
    static constexpr const char* kDefaultCauseOfDeath = "HOSTILE GUNFIRE (7.62x39mm PS)";
    static constexpr const char* kDefaultKillerName   = "HOSTILE PATROL";

    static RaidStats& GetInstance()
    {
        static RaidStats instance;
        return instance;
    }

    void Reset()
    {
        const bool keepAutoDemo = autoDemoMode;
        const int keepDemoCycle = autoDemoCycleIndex;
        *this = RaidStats{};
        autoDemoMode = keepAutoDemo;
        autoDemoCycleIndex = keepDemoCycle;
    }

    bool autoDemoMode = true;   // 起動時から自動デモON（[F7]で自動デモ⇔手動操作をいつでも切替）
    int autoDemoCycleIndex = 0; // 0: 1周目(即死→GameOver救難演出) / 1: 2周目(全制圧→Clear脱出演出)
    float raidTime = 0.0f;
    float maxRaidTime = kDefaultMaxRaidTime;
    int enemiesKilled = 0;
    int targetsDestroyed = 0;
    int totalTargets = kDefaultTotalTargets;
    int shotsFired = 0;
    int shotsHit = 0;
    int totalLootValue = 0;
    int medkitsUsed = 0;
    float remainingHp = kDefaultMaxHp;
    float maxHp = kDefaultMaxHp;
    int damageTakenCount = 0;
    int lootedGoldDucks = 0;
    int lootedCashCount = 0;
    int lootedCashValue = 0;
    int lootedMedkits = 0;
    int lootedAmmoBoxes = 0;
    bool isSurvived = false;
    bool isMIA = false;
    std::string causeOfDeath = kDefaultCauseOfDeath;
    std::string killerName = kDefaultKillerName;

    float GetAccuracy() const
    {
        return shotsFired > 0 ? (static_cast<float>(shotsHit) / static_cast<float>(shotsFired)) * kPercentMultiplier : 0.0f;
    }

    float GetRemainingTime() const
    {
        float rem = maxRaidTime - raidTime;
        return rem > 0.0f ? rem : 0.0f;
    }
};
