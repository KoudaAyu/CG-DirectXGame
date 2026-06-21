#pragma once
#include <chrono>
#include <memory>
#include <vector>
#include <string>

#include "BaseScene.h"
#include "Baziru3_Engine\Effect\HitEffect.h"
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
#include "Baziru3_Engine/IO/Mouse/MouseInput.h"
#include "Baziru3_Engine/Particle/Ring.h"
#include "../../Player/Player.h"
#include "../../Enemy/Enemy.h"
#include "../../Enemy/MovingEnemy.h"
#include "../../../Bullet.h"
#include "../../Obstacle.h"

class Camera;
class SpriteCom;
struct SceneRenderRequests;

class GamePlayScene : public BaseScene
{
public:
    void Initialize(DirectXCom* dxCommon, Camera* camera) override;
    void Finalize() override;
    void Update() override;
    void Draw(SceneRenderRequests& renderRequests) override;

    void SetSpriteCom(SpriteCom* spriteCom) { this->spriteCom = spriteCom; }

    bool IsGameCleared() const { return isGameCleared_; }
    float GetExtractionTimer() const { return extractionTimer_; }
    const char* GetSceneType() const override { return "GAMEPLAY"; }
    Vector3 GetPlayerPosition() const { return player_ ? player_->GetPosition() : Vector3{ 0.0f, 0.0f, 0.0f }; }
    Player* GetPlayer() const { return player_.get(); }
    Vector3 GetGoalPosition() const { return goalRingTransform_.translate; }
    void TriggerCameraShake(float duration, float intensity) {
        cameraShakeTime_ = duration;
        cameraShakeDurationMax_ = duration > 0.0f ? duration : 1.0f;
        cameraShakeIntensity_ = intensity;
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
    void UpdateCombat(float deltaTime);
    void UpdatePlayerHpBar();
    void CheckGameOver();
    void UpdateObstacles();

    void AddBullet(std::unique_ptr<Bullet> bullet);
    void UpdateBullets(float deltaTime);
    void RemoveDeadBullets();
    void ResolveBulletCollisions();
    void ResolveObstacleCollisions();
    void ResolveContactDamage();

    // ヘルパーメソッド
    RenderContext BuildRenderContext() const;
    static bool IsWithinRadius(const Vector3& a, const Vector3& b, float radius);

    // --- コアシステム & エンジンコンテキスト ---
    DirectXCom* directXCom = nullptr;
    Camera* camera_ = nullptr;
    Light* light = nullptr;
    MaterialManager* materialManager = nullptr;
    Object3dCom* object3dCom = nullptr;
    ParticleManager* particleManager = nullptr;
    std::unique_ptr<AppParticleManager> appParticleManager_;
    SpriteCom* spriteCom = nullptr;

    // --- ゲームエンティティ & オブジェクト ---
    std::unique_ptr<Player> player_;
    std::unique_ptr<Enemy> enemy_;
    std::unique_ptr<MovingEnemy> movingEnemy_;
    std::vector<std::unique_ptr<Bullet>> bullets_;
    std::vector<std::unique_ptr<Obstacle>> obstacles_;
    std::unique_ptr<Sphere> sphere_;
    std::unique_ptr<Object3d> animatedCube_;
    std::unique_ptr<Ring> goalRing_;
    std::unique_ptr<HitEffect> hitEffect_;

    // --- アニメーション & スケルトン ---
    Skeleton skeleton_{};
    Animation animation_{};
    Animator animator_{};
    SkeletonDebug skeletonDebug_{};

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
    bool animatedCubeInitialized_ = false;

    // --- テクスチャ ---
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

    // --- カメラシェイク ---
    float cameraShakeTime_ = 0.0f;
    float cameraShakeIntensity_ = 0.0f;
    float cameraShakeDurationMax_ = 1.0f;

    // --- ビネット＆リロード完了 ---
    Sprite* vignetteSprite_ = nullptr;
    float vignetteAlpha_ = 0.0f;
    bool wasPlayerReloading_ = false;
    bool wasPlayerReloadingPrev_ = false;
    int remainingAmmoOnReload_ = 0;
    int droppedCasingsCount_ = 0;

    // --- 浮遊テキスト ---
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

