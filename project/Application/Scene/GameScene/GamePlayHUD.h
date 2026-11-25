#pragma once
#include <vector>
#include <string>
#include <memory>
#include "Vector.h"
#include "Matrix4x4.h"
#include "LootSystem.h"
#include "Application/Config/GameConfig.h"

class Camera;
class Player;
class Enemy;
class MovingEnemy;
class Obstacle;
class Target;
class TutorialSign;

/// <summary>
/// HUD描画に必要なシーン情報のコンテキスト構造体
/// </summary>
struct GamePlayHUDContext
{
    Camera* camera                           = nullptr;
    Player* player                           = nullptr;
    const std::vector<std::unique_ptr<Enemy>>* enemies = nullptr;
    const std::vector<std::unique_ptr<Obstacle>>* obstacles = nullptr;
    const std::vector<std::unique_ptr<Target>>* targets     = nullptr;
    const std::vector<std::unique_ptr<TutorialSign>>* tutorialSigns = nullptr;
    const std::vector<LootableProp>* lootProps              = nullptr;
    const std::vector<FloatingText>* floatingTexts          = nullptr;

    Vector3 extractionGoalPos                = GameConfig::Environment::kExtractionPadPosition;
    bool isReadyToExtract                    = false;
    bool isPlayerInExtractionZone            = false;
    float extractionTimer                    = GameConfig::Environment::kExtractionMaxTime;
    float extractionMaxTime                  = GameConfig::Environment::kExtractionMaxTime;
    float extractionUnlockBannerTimer        = 0.0f;
    float clearClimaxProgress                = 0.0f;
    bool isGameCleared                       = false;
    bool isDeathSequenceActive               = false;
    float deathSequenceTimer                 = 0.0f;
    float hitIndicatorTimer                  = 0.0f;
    float hitIndicatorAngle                  = 0.0f;
    float playerSoundRadius                  = 0.0f;
    float playerSoundMaxRadius               = 0.0f;
    float playerSoundTimer                   = 0.0f;
    bool showDebugGizmos                     = false;
    bool showPerformanceTracker              = false;
    bool isStressTestActive                  = false;
    int stressTestCount                      = 0;
    float sceneEntranceFadeTimer             = 0.0f;
    bool isOverheadCameraMode                = false;
    bool isFullMapOpen                       = false;
    float cameraZoom                         = 1.0f;
    bool isAutoDemoPlay                      = false;
    int autoDemoCycleIndex                   = 0;
    bool isBulletTimeActive                  = false;
};

/// <summary>
/// タクティカルゲームプレイHUD描画マネージャークラス
/// 画面上部の作戦バー・タイマー、左上ミニマップ＆全体作戦マップ、右上クエスト進行ガイド、
/// 3Dルート案内シェブロン、下部操作ガイドバー、右下兵装インベントリ等を一元描画します。
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
    void DrawNavigationGuideHUD(const GamePlayHUDContext& ctx, float deltaTime);
    void DrawMissionObjectiveHUD(const GamePlayHUDContext& ctx);
    void DrawQuestTrackerHUD(const GamePlayHUDContext& ctx);
    void DrawControlsGuideHUD(const GamePlayHUDContext& ctx);
    void DrawTacticalMapHUD(const GamePlayHUDContext& ctx);
    void DrawExtractionSequenceHUD(const GamePlayHUDContext& ctx, float deltaTime);
    void DrawTutorialSignHUD(const GamePlayHUDContext& ctx);
    void DrawPlayerVitalsHUD(const GamePlayHUDContext& ctx, float deltaTime);
    void DrawPlayerAmmoHUD(const GamePlayHUDContext& ctx, float deltaTime);
    void DrawEnemyTacticalHUD(const GamePlayHUDContext& ctx);
    void DrawLootingHUD(const GamePlayHUDContext& ctx);
    void DrawDamageIndicator(const GamePlayHUDContext& ctx);
    void DrawLowHpRedVignetteEffect(const GamePlayHUDContext& ctx);
    void DrawDeathSequenceHUD(const GamePlayHUDContext& ctx);
    void DrawLaserSight(const GamePlayHUDContext& ctx);
    void DrawTacticalReticle(const GamePlayHUDContext& ctx);
    void DrawFloatingTexts(const GamePlayHUDContext& ctx);
    void DrawVisionConesAndGizmos(const GamePlayHUDContext& ctx);
    void DrawPerformanceTrackerUI(const GamePlayHUDContext& ctx, float deltaTime);

private:
    // --- 2Dスクリーン投影ヘルパー ---
    static bool Project3DTo2D(const Vector3& pos3D, const Matrix4x4& vp, float screenW, float screenH, Vector2& outPos);

private:
    // --- UIテクスチャインデックス ---
    uint32_t medkitTextureIndex_      = UINT32_MAX;
    uint32_t ammoTextureIndex_        = UINT32_MAX;
    uint32_t goldDuckTextureIndex_    = UINT32_MAX;
    uint32_t roublesTextureIndex_     = UINT32_MAX;
    uint32_t cineAtlasTextureIndex_   = UINT32_MAX;
    uint32_t hudAtlasTextureIndex_    = UINT32_MAX;
    uint32_t clearDigitsTextureIndex_ = UINT32_MAX;
    float displayedPlayerHpLag_       = 1.0f;
    bool laserLockedOnTarget_         = false;

    // --- NavMesh ルート案内 ＆ 画面外ポインター平滑化キャッシュ ---
    std::vector<Vector3> cachedGuidePath_;
    std::vector<Vector3> smoothGuidePath_;
    std::vector<Vector3> scratchGuidePath_;
    Vector3 cachedGuideGoal_          = { 0.0f, 0.0f, 0.0f };
    float guidePathRefreshTimer_      = 0.0f;
    Vector2 smoothedOffscreenDir_     = { 0.0f, -1.0f };
    bool hasOffscreenDir_             = false;
    int lastActiveTargetIdx_          = -1;
};
