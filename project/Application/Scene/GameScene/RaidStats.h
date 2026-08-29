#pragma once
#include <string>

/// <summary>
/// レイド戦績データ管理シングルトン
/// 出撃から脱出/死亡までの戦績（時間、撃破数、命中率、獲得物資など）を保持し、
/// クリア画面・ゲームオーバー画面へリアルタイムに伝達します。
/// </summary>
struct RaidStats
{
    static RaidStats& GetInstance()
    {
        static RaidStats instance;
        return instance;
    }

    void Reset()
    {
        raidTime = 0.0f;
        maxRaidTime = 300.0f; // 5分 (300秒)
        enemiesKilled = 0;
        targetsDestroyed = 0;
        totalTargets = 3;
        shotsFired = 0;
        shotsHit = 0;
        totalLootValue = 0;
        medkitsUsed = 0;
        isSurvived = false;
        isMIA = false;
        causeOfDeath = "HOSTILE GUNFIRE (7.62x39mm PS)";
        killerName = "HOSTILE PATROL";
    }

    float raidTime = 0.0f;
    float maxRaidTime = 300.0f; // 5分
    int enemiesKilled = 0;
    int targetsDestroyed = 0;
    int totalTargets = 3;
    int shotsFired = 0;
    int shotsHit = 0;
    int totalLootValue = 0;
    int medkitsUsed = 0;
    bool isSurvived = false;
    bool isMIA = false;
    std::string causeOfDeath = "HOSTILE GUNFIRE (7.62x39mm PS)";
    std::string killerName = "HOSTILE PATROL";

    float GetAccuracy() const
    {
        return shotsFired > 0 ? (static_cast<float>(shotsHit) / static_cast<float>(shotsFired)) * 100.0f : 0.0f;
    }

    float GetRemainingTime() const
    {
        float rem = maxRaidTime - raidTime;
        return rem > 0.0f ? rem : 0.0f;
    }
};
