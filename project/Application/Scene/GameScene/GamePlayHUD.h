#pragma once
#include <vector>
#include <string>
#include <memory>
#include "Vector.h"
#include "Matrix4x4.h"
#include "LootSystem.h"

class Camera;
class Player;
class Enemy;
class MovingEnemy;
class Obstacle;
class Target;

/// <summary>
/// HUD描画に必要なシーン情報のコンテキスト構造体
/// </summary>
struct GamePlayHUDContext
{
    Camera* camera                           = nullptr;
    Player* player                           = nullptr;
    Enemy* enemy                             = nullptr;
    MovingEnemy* movingEnemy                 = nullptr;
    const std::vector<std::unique_ptr<Obstacle>>* obstacles = nullptr;
    const std::vector<std::unique_ptr<Target>>* targets     = nullptr;
    const std::vector<LootableProp>* lootProps              = nullptr;
    const std::vector<FloatingText>* floatingTexts          = nullptr;

    Vector3 extractionGoalPos                = { 0.0f, 0.0f, 32.0f };
    bool isReadyToExtract                    = false;
    bool isGameCleared                       = false;
    bool isDeathSequenceActive               = false;
    float deathSequenceTimer                 = 0.0f;
    float hitIndicatorTimer                  = 0.0f;
    float hitIndicatorAngle                  = 0.0f;
    float playerSoundRadius                  = 0.0f;
    float playerSoundMaxRadius               = 0.0f;
    float playerSoundTimer                   = 0.0f;
    bool showDebugGizmos                     = false;
    bool showPerformanceTracker              = true;
    bool isStressTestActive                  = false;
    int stressTestCount                      = 0;
    float sceneEntranceFadeTimer             = 0.0f;
};

/// <summary>
/// タクティカルゲームプレイHUD描画マネージャークラス
/// 画面上部の作戦バー・タイマー、右下の弾薬・回復インベントリ、3D探索カード、被弾インジケーター、レーザーサイト、低体力ビネット等を一元描画します。
/// </summary>
class GamePlayHUD
{
public:
    /// <summary>
    /// HUDで使用するアイテムイラストテクスチャ等の読み込み
    /// </summary>
    void Initialize();

    /// <summary>
    /// 全HUDレイヤーの統合描画
    /// </summary>
    void Draw(const GamePlayHUDContext& ctx, float deltaTime);

public:
    // --- 個別HUD描画メソッド ---
    void DrawMissionObjectiveHUD(const GamePlayHUDContext& ctx);
    void DrawPlayerAmmoHUD(const GamePlayHUDContext& ctx, float deltaTime);
    void DrawLootingHUD(const GamePlayHUDContext& ctx);
    void DrawDamageIndicator(const GamePlayHUDContext& ctx);
    void DrawLowHpRedVignetteEffect(const GamePlayHUDContext& ctx);
    void DrawDeathSequenceHUD(const GamePlayHUDContext& ctx);
    void DrawLaserSight(const GamePlayHUDContext& ctx);
    void DrawFloatingTexts(const GamePlayHUDContext& ctx);
    void DrawVisionConesAndGizmos(const GamePlayHUDContext& ctx);
    void DrawPerformanceTrackerUI(const GamePlayHUDContext& ctx, float deltaTime);

private:
    // --- 2Dスクリーン投影ヘルパー ---
    static bool Project3DTo2D(const Vector3& pos3D, const Matrix4x4& vp, float screenW, float screenH, Vector2& outPos);

private:
    // --- UIテクスチャインデックス ---
    uint32_t medkitTextureIndex_   = UINT32_MAX;
    uint32_t ammoTextureIndex_     = UINT32_MAX;
    uint32_t goldDuckTextureIndex_ = UINT32_MAX;
    uint32_t roublesTextureIndex_  = UINT32_MAX;
};
