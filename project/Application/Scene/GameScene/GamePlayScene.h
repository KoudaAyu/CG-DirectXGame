#pragma once
#include <chrono>
#include <memory>
#include <vector>
#include <string>

#include "BaseScene.h"
#include "Baziru3_Engine\Framework\Effect\HitEffect.h"
#include "DirectXCom.h"
#include "ParticleEmitter.h"
#include "Object3dCom.h"
#include "Object3d.h"
#include "Light.h"
#include "MaterialManager.h"
#include "ParticleManager.h"
#include "Application/Particle/AppParticleManager.h"
#include "Animation.h"
#include "Animator.h"
#include "Skeleton.h"
#include "SkeletonDebug.h"
#include "Sphere.h"
#include "Sprite.h"
#include "SpriteManager.h"
#include "DebugCamera.h"
#include "Baziru3_Engine/Core/IO/Mouse/MouseInput.h"
#include "Baziru3_Engine/Framework/Particle/Ring.h"
#include "../../Player/Player.h"
#include "../../Enemy/Enemy.h"
#include "../../Enemy/MovingEnemy.h"
#include "Bullet.h"
#include "Obstacle.h"
#include "Target.h"
#include "TutorialSign.h"
#include "../../LevelEditor.h"
#include "LootSystem.h"
#include "GamePlayHUD.h"

#include "Application/Config/GameConfig.h"

class Camera;
class SpriteCom;
class SkinningObject3dCom;
class CombatSystem;
class CollisionSystem;
struct SceneRenderRequests;

/// <summary>
/// メインのゲームプレイシーンクラス (GamePlayScene)
/// 各種サブシステム（LootSystem, GamePlayHUD, CombatSystem, CollisionSystem, LevelEditor）を統括し、
/// レイドの展開、プレイヤー/敵AIの協調動作、脱出判定、クリア/ゲームオーバー遷移を管理します。
/// </summary>
class GamePlayScene : public BaseScene
{
    friend class CombatSystem;
    friend class CollisionSystem;

public:
    // =========================================================================
    // 定数定義 (Named Constants) - GameConfig から一元参照
    // =========================================================================
    static constexpr float kFixedDeltaTime         = 1.0f / 60.0f;                                     // 基準フレーム時間
    static constexpr float kExtractionMaxTime      = GameConfig::Environment::kExtractionMaxTime;     // 脱出パッド滞在所要時間 (3.0秒)
    static constexpr float kClearSlowMoDuration    = GameConfig::Environment::kClearSlowMoDuration;   // 生還クリア時のスロー演出時間 (1.5秒)
    static constexpr float kDeathSequenceDuration  = GameConfig::Environment::kDeathSequenceDuration; // 戦死時の暗転演出時間 (2.0秒)
    static constexpr float kExtractionRadius       = GameConfig::Environment::kExtractionRadius;      // 脱出ゾーン有効半径 (2.2m)
    static constexpr float kEnemyBulletDamage      = GameConfig::Combat::kEnemyBulletDamage;           // 敵の射撃ダメージ
    static constexpr float kContactDamage          = GameConfig::Combat::kContactDamage;               // 敵接触ダメージ
    static constexpr float kBulletHitRadius        = GameConfig::Combat::kBulletHitRadius;             // 弾丸当たり判定半径
    static constexpr float kPlayerHitRadius        = GameConfig::Combat::kPlayerHitRadius;             // プレイヤー当たり判定半径
    static constexpr float kEnemyHitRadius         = GameConfig::Combat::kEnemyHitRadius;              // 敵当たり判定半径

public:
    GamePlayScene();
    ~GamePlayScene() override;

    /// <summary>
    /// シーンの初期化（エンジンコンポーネント、各マネージャー、ステージ、サブシステムのセットアップ）
    /// </summary>
    void InitializeScene() override;

    /// <summary>
    /// シーンの終了処理（メモリ解放と登録解除）
    /// </summary>
    void Finalize() override;

    /// <summary>
    /// 毎フレームの更新（サブシステム・タイマー・AI・演出・UIの更新）
    /// </summary>
    void Update() override;

    /// <summary>
    /// シーンの3D/2D描画要求（レンダリングキューへの登録）
    /// </summary>
    void Draw(SceneRenderRequests& renderRequests) override;

    // --- 外部インターフェース ---
    void SetSpriteCom(SpriteCom* spriteCom) { this->spriteCom = spriteCom; }
    Emitter& GetEmitter() { return emitter; }
    bool IsGameCleared() const { return isGameCleared_; }
    float GetExtractionTimer() const { return extractionTimer_; }
    AppParticleManager* GetAppParticleManager() const { return appParticleManager_.get(); }
    const char* GetSceneType() const { return "GAMEPLAY"; }
    Vector3 GetPlayerPosition() const { return player_ ? player_->GetPosition() : Vector3{ 0.0f, 0.0f, 0.0f }; }
    Player* GetPlayer() const { return player_.get(); }
    Vector3 GetGoalPosition() const { return goalRingTransform_.translate; }
    const std::vector<std::unique_ptr<Target>>& GetTargets() const { return targets_; }
    std::vector<std::unique_ptr<Target>>& GetTargets() { return targets_; }
    const std::vector<std::unique_ptr<TutorialSign>>& GetTutorialSigns() const { return tutorialSigns_; }
    LootSystem* GetLootSystem() const { return lootSystem_.get(); }

    /// <summary>
    /// カメラシェイク（揺れ）エフェクトの誘発
    /// </summary>
    void TriggerCameraShake(float duration, float intensity) {
        cameraShakeTime_ = duration;
        cameraShakeDurationMax_ = duration > 0.0f ? duration : 1.0f;
        cameraShakeIntensity_ = intensity;
    }

    /// <summary>
    /// ヒットストップ（時間一時停止）の誘発
    /// </summary>
    void TriggerHitStop(float duration) {
        hitStopTimer_ = duration;
    }

    /// <summary>
    /// 被弾方向インジケーター（赤いアーク）の誘発
    /// </summary>
    void TriggerDamageIndicator(float worldAngle) {
        hitIndicatorTimer_ = 0.65f;
        hitIndicatorAngle_ = worldAngle;
    }

    /// <summary>
    /// 浮遊ダメージ/物資獲得テキストの追加
    /// </summary>
    void AddFloatingText(const Vector3& worldPos, const std::string& text, const Vector4& color, bool isCritical = false);

    /// <summary>
    /// 配置されている全障害物リストの取得
    /// </summary>
    const std::vector<std::unique_ptr<Obstacle>>& GetObstacles() const { return obstacles_; }

private:
    // --- 内部初期化 & 更新メソッド ---
    float AdvanceDeltaTime();
    void InitializeEnvironment();
    void InitializeCharacters();
    void InitializeSprites();
    void InitializeAudioAndParticles();
    void InitializeObstacles();

    void UpdateExtractionGoal(float deltaTime);
    void UpdateEnvironment();
    void UpdateParticles(float deltaTime);
    void UpdateSprites(float deltaTime);
    void UpdateDebugInput();
    void UpdateCharacters(float deltaTime);
    void UpdatePlayerHpBar();
    void CheckGameOver();
    void UpdateObstacles();
    void UpdateStressTestMode();

    RenderContext BuildRenderContext() const;
    static bool IsWithinRadius(const Vector3& a, const Vector3& b, float radius);

private:
    // --- コアシステム & エンジンコンテキスト ---
    DirectXCom* directXCom = nullptr;
    Camera* camera_ = nullptr;
    Light* light = nullptr;
    MaterialManager* materialManager = nullptr;
    Object3dCom* object3dCom = nullptr;
    SkinningObject3dCom* skinningObject3dCom = nullptr;
    ParticleManager* particleManager = nullptr;
    std::unique_ptr<AppParticleManager> appParticleManager_;
    SpriteCom* spriteCom = nullptr;

    // --- 分割されたモジュール サブシステム ---
    std::unique_ptr<CombatSystem> combatSystem_;
    std::unique_ptr<CollisionSystem> collisionSystem_;
    std::unique_ptr<LootSystem> lootSystem_;
    std::unique_ptr<GamePlayHUD> hud_;
    std::unique_ptr<LevelEditor> levelEditor_;

    // --- ゲームエンティティ ---
    std::unique_ptr<Player> player_;
    std::unique_ptr<Enemy> enemy_;
    std::unique_ptr<MovingEnemy> movingEnemy_;
    std::vector<std::unique_ptr<Obstacle>> obstacles_;
    std::vector<std::unique_ptr<Target>> targets_;
    std::vector<std::unique_ptr<TutorialSign>> tutorialSigns_;
    std::unique_ptr<Sphere> sphere_;
    std::unique_ptr<Ring> goalRing_;
    std::unique_ptr<Object3d> extractionPadObject_;
    uint32_t extractionPadTextureIndex_ = 0;
    std::unique_ptr<HitEffect> hitEffect_;

    // --- パーティクル & スプライト ---
    Emitter emitter;
    ParticleEmitter particleEmitter;
    std::vector<std::unique_ptr<Sprite>> sprites;
    std::unique_ptr<SpriteManager> spriteManager_;
    Sprite::Transform goalRingTransform_{};
    Sprite* playerHpBarBg_ = nullptr;
    Sprite* playerHpBarFg_ = nullptr;
    Sprite* playerStaminaBarBg_ = nullptr;
    Sprite* playerStaminaBarFg_ = nullptr;
    Sprite* playerReloadBarBg_ = nullptr;
    Sprite* playerReloadBarFg_ = nullptr;
    int cursorSpriteIndex = -1;
    Sprite* vignetteSprite_ = nullptr;
    float vignetteAlpha_ = 0.0f;
    std::vector<Sprite*> speedLines_;
    float speedLineAlpha_ = 0.0f;

    // --- インプット ---
    MouseInput mouseInput;
    DebugCamera debugCamera_;

    // --- テクスチャインデックス ---
    uint32_t cylinderTextureIndex_  = TextureManager::kInvalidTextureIndex;
    uint32_t particleTextureA       = TextureManager::kInvalidTextureIndex;
    uint32_t particleTextureB       = TextureManager::kInvalidTextureIndex;
    uint32_t bloodTextureIndex_     = TextureManager::kInvalidTextureIndex;
    uint32_t smokeTextureIndex_     = TextureManager::kInvalidTextureIndex;
    uint32_t fenceTextureIndex_     = TextureManager::kInvalidTextureIndex;
    uint32_t starburstTextureIndex_ = TextureManager::kInvalidTextureIndex;

    // --- ゲームステート & タイマー ---
    bool allTargetsDestroyed_ = false;
    bool isGameCleared_ = false;
    float clearSlowMoTimer_ = kClearSlowMoDuration;
    float extractionTimer_ = kExtractionMaxTime;
    bool isPlayerInExtractionZone_ = false;
    bool isDeathSequenceActive_ = false;
    float deathSequenceTimer_ = 0.0f;
    float sceneEntranceFadeTimer_ = 0.8f; // シーン入場時のフェードインタイマー
    std::chrono::steady_clock::time_point lastTime_;

    // --- 演出パラメータ ---
    float cameraShakeTime_ = 0.0f;
    float cameraShakeIntensity_ = 0.0f;
    float cameraShakeDurationMax_ = 1.0f;
    float hitStopTimer_ = 0.0f;
    float hitIndicatorTimer_ = 0.0f;
    float hitIndicatorAngle_ = 0.0f;
    float playerDustTimer_ = 0.0f;
    float playerReloadCasingTimer_ = 0.0f;
    float escapeSmokeTimer_ = 0.0f;
    float lightFlashTimer_ = 0.0f;
    float clearCelebrateTimer_ = 0.0f;
    bool wasPlayerDodging_ = false;
    bool wasPlayerReloading_ = false;
    bool wasPlayerReloadingPrev_ = false;
    int remainingAmmoOnReload_ = 0;
    int droppedCasingsCount_ = 0;

    // --- 浮遊テキスト & 音響ギズモ ---
    std::vector<FloatingText> floatingTexts_;
    float playerSoundRadius_ = 0.0f;
    float playerSoundMaxRadius_ = 0.0f;
    float playerSoundTimer_ = 0.0f;
    bool showDebugGizmos_ = false;

    // --- パフォーマンストラッカー & ストレステスト ---
    bool showPerformanceTracker_ = true;
    bool isStressTestActive_ = false;
    std::vector<std::unique_ptr<Obstacle>> stressTestObstacles_;
};
