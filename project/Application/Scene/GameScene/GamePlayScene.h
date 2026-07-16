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
#include "../../../Bullet.h"
#include "../../Obstacle.h"
#include "Target.h"
#include "TutorialSign.h"

class Camera;
class SpriteCom;
class SkinningObject3dCom;
class CombatSystem;
class CollisionSystem;
struct SceneRenderRequests;

/// <summary>
/// メインのゲームプレイシーンクラス
/// キャラクター、ステージ環境、サブシステムの初期化・更新・描画の統括を行います。
/// </summary>
class GamePlayScene : public BaseScene
{
	friend class CombatSystem;
	friend class CollisionSystem;

public:
	/// <summary>
	/// コンストラクタ
	/// </summary>
	GamePlayScene();

	/// <summary>
	/// シーンの初期化処理
	/// エンジンの各コンポーネント取得、キャラ・背景・サブシステムの初期設定を行います。
	/// </summary>
    void InitializeScene() override;

	/// <summary>
	/// シーンの終了処理（メモリ解放）
	/// 各種オブジェクト・サブシステムのクリーンアップを行います。
	/// </summary>
    void Finalize() override;

	/// <summary>
	/// デストラクタ
	/// </summary>
    ~GamePlayScene() override;

	/// <summary>
	/// シーンの更新処理
	/// サブシステム、演出タイマー、カメラ演出、UIの毎フレーム更新を行います。
	/// </summary>
    void Update() override;

	/// <summary>
	/// シーンの描画処理
	/// プレイヤー、敵、障害物、エフェクト、UIスプライトの描画を要求します。
	/// </summary>
	/// <param name="renderRequests">レンダリングリクエストマネージャー</param>
    void Draw(SceneRenderRequests& renderRequests) override;

    void SetSpriteCom(SpriteCom* spriteCom) { this->spriteCom = spriteCom; }
    Emitter& GetEmitter() { return emitter; }

	// --- ゲッター & ユーティリティ ---
    bool IsGameCleared() const { return isGameCleared_; }
    float GetExtractionTimer() const { return extractionTimer_; }
    AppParticleManager* GetAppParticleManager() const { return appParticleManager_.get(); }
    const char* GetSceneType() const { return "GAMEPLAY"; }
    Vector3 GetPlayerPosition() const { return player_ ? player_->GetPosition() : Vector3{ 0.0f, 0.0f, 0.0f }; }
    Player* GetPlayer() const { return player_.get(); }
    Vector3 GetGoalPosition() const { return goalRingTransform_.translate; }
    const std::vector<std::unique_ptr<Target>>& GetTargets() const { return targets_; }
    std::vector<std::unique_ptr<Target>>& GetTargets() { return targets_; }

	/// <summary>
	/// カメラシェイク（揺れ）エフェクトを誘発します
	/// </summary>
	/// <param name="duration">効果時間(秒)</param>
	/// <param name="intensity">揺れの強さ</param>
    void TriggerCameraShake(float duration, float intensity) {
        cameraShakeTime_ = duration;
        cameraShakeDurationMax_ = duration > 0.0f ? duration : 1.0f;
        cameraShakeIntensity_ = intensity;
    }

    /// <summary>
    /// ヒットストップ（時間一時停止）を誘発します
    /// </summary>
    /// <param name="duration">効果時間(秒)</param>
    void TriggerHitStop(float duration) {
        hitStopTimer_ = duration;
    }

private:
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

    // ヘルパーメソッド
    RenderContext BuildRenderContext() const;
    static bool IsWithinRadius(const Vector3& a, const Vector3& b, float radius);

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

    // --- ゲームエンティティ & オブジェクト ---
    std::unique_ptr<Player> player_;
    std::unique_ptr<Enemy> enemy_;
    std::unique_ptr<MovingEnemy> movingEnemy_;
    std::vector<std::unique_ptr<Obstacle>> obstacles_;
    std::unique_ptr<Sphere> sphere_;
    std::unique_ptr<Ring> goalRing_;
    std::unique_ptr<HitEffect> hitEffect_;
    std::vector<std::unique_ptr<Target>> targets_;
    bool allTargetsDestroyed_ = false;

    // --- 分割されたサブシステム ---
    std::unique_ptr<CombatSystem> combatSystem_;
    std::unique_ptr<CollisionSystem> collisionSystem_;

    // --- パーティクルエミッター ---
    Emitter emitter;
    ParticleEmitter particleEmitter;

    // --- スプライト & UI ---
    std::vector<std::unique_ptr<Sprite>> sprites;
    std::unique_ptr<SpriteManager> spriteManager_;
    Sprite::Transform goalRingTransform_{};
    Sprite* playerHpBarBg_ = nullptr;
    Sprite* playerHpBarFg_ = nullptr;
    Sprite* playerReloadBarBg_ = nullptr;
    Sprite* playerReloadBarFg_ = nullptr;
    int cursorSpriteIndex = -1;

    // --- インプット ---
    MouseInput mouseInput;
    DebugCamera debugCamera_;

    // --- 初期化フラグ ---
    bool sphereInitialized = false;
    bool hitEffectInitialized = false;

    // --- テクスチャインデックス ---
    uint32_t cylinderTextureIndex_ = TextureManager::kInvalidTextureIndex;
    uint32_t particleTextureA = TextureManager::kInvalidTextureIndex;
    uint32_t particleTextureB = TextureManager::kInvalidTextureIndex;
    uint32_t fenceTextureIndex_ = TextureManager::kInvalidTextureIndex;
    uint32_t starburstTextureIndex_ = TextureManager::kInvalidTextureIndex;

    // --- ステート & タイマー ---
    bool isGameCleared_ = false;
    float clearSlowMoTimer_ = 1.5f;
    float extractionTimer_ = 5.0f;
    std::chrono::steady_clock::time_point lastTime_;
    float playerDustTimer_ = 0.0f;
    float playerReloadCasingTimer_ = 0.0f;
    float escapeSmokeTimer_ = 0.0f;
    float lightFlashTimer_ = 0.0f;
    float clearCelebrateTimer_ = 0.0f;
    bool wasPlayerDodging_ = false;
    std::vector<Sprite*> speedLines_;
    float speedLineAlpha_ = 0.0f;

    // --- カメラシェイク用パラメータ ---
    float cameraShakeTime_ = 0.0f;
    float cameraShakeIntensity_ = 0.0f;
    float cameraShakeDurationMax_ = 1.0f;

    // --- ヒットストップ用パラメータ ---
    float hitStopTimer_ = 0.0f;

    // --- ビネット＆リロード完了時の演出パラメータ ---
    Sprite* vignetteSprite_ = nullptr;
    float vignetteAlpha_ = 0.0f;
    bool wasPlayerReloading_ = false;
    bool wasPlayerReloadingPrev_ = false;
    int remainingAmmoOnReload_ = 0;
    int droppedCasingsCount_ = 0;

    // --- 浮遊ダメージテキスト構造体 ---
    struct FloatingText {
        Vector3 position;
        std::string text;
        Vector4 color;
        float lifeTime;
        float maxLifeTime;
        bool isCritical;
    };
    std::vector<FloatingText> floatingTexts_;
    void AddFloatingText(const Vector3& worldPos, const std::string& text, const Vector4& color, bool isCritical = false);

    // --- ゲームプレイ定数 ---
    static constexpr float kFixedDeltaTime = 1.0f / 60.0f;
    static constexpr float kEnemyBulletDamage = 10.0f;
    static constexpr float kContactDamage = 20.0f;
    static constexpr float bulletHitRadius_ = 0.25f;
    static constexpr float playerHitRadius_ = 0.6f;
    static constexpr float enemyHitRadius_ = 0.6f;

    // --- タクティカル音響 ＆ デバッグ表示 ---
    float playerSoundRadius_ = 0.0f;
    float playerSoundMaxRadius_ = 0.0f;
    float playerSoundTimer_ = 0.0f;
    bool showDebugGizmos_ = true; // デバッグコーンと音波リングの表示フラグ (F1でトグル)
};

