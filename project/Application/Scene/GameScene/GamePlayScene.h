#pragma once
#include <chrono>

#include "BaseScene.h"
#include"Baziru3_Engine\Effect\HitEffect.h"
#include"DirectXCom.h"
#include"ParticleEmitter.h"

#include <vector>

#include "Object3dCom.h"
#include "Object3d.h"
#include "Light.h"
#include "MaterialManager.h"
#include "ParticleManager.h"
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

class Camera;
class SpriteCom;
struct SceneRenderRequests;
// Player クラスの定義
#include "../../Player/Player.h"
#include "../../Enemy/Enemy.h"
#include "../../../Bullet.h"

class GamePlayScene : public BaseScene
{
public:
    
    void Initialize(DirectXCom* dxCommon, Camera* camera) override;

    void Finalize() override;

    void Update() override;

   void Draw(SceneRenderRequests& renderRequests) override;

    void SetSpriteCom(SpriteCom* spriteCom) { this->spriteCom = spriteCom; }

private:

    Camera* camera_ = nullptr;
    DirectXCom* directXCom = nullptr;
    Emitter emitter;
    SpriteCom* spriteCom = nullptr;
    ParticleEmitter particleEmitter;
    Light* light = nullptr;
    MaterialManager* materialManager = nullptr;
    Object3dCom* object3dCom = nullptr;
    ParticleManager* particleManager = nullptr;
    std::unique_ptr<HitEffect> hitEffect_;
    std::unique_ptr<Object3d> animatedCube_;
    std::unique_ptr<Player> player_;
    std::unique_ptr<Enemy> enemy_;
    std::vector<std::unique_ptr<Bullet>> bullets_;
    std::unique_ptr<Sphere> sphere_;
    Skeleton skeleton_{};
    Animation animation_{};
    Animator animator_{};
    SkeletonDebug skeletonDebug_{};
    DebugCamera debugCamera_;
    std::vector<std::unique_ptr<Sprite>> sprites;
    std::unique_ptr<SpriteManager> spriteManager_;
    // mouse input for cursor sprite
    MouseInput mouseInput;
    // index of cursor sprite in sprites vector, -1 if none
    int cursorSpriteIndex = -1;
    std::list<ParticleManager::Particle> particles;
    std::list<ParticleManager::Particle> hitEffectParticles;

    bool sphereInitialized = false;
    bool hitEffectInitialized = false;
    bool animatedCubeInitialized_ = false;

	// テクスチャインデックスは TextureManager で管理されるため、ここではインデックスを保持するだけにする
    uint32_t cylinderTextureIndex_ = TextureManager::kInvalidTextureIndex;
    uint32_t particleTextureA = TextureManager::kInvalidTextureIndex;
    uint32_t particleTextureB = TextureManager::kInvalidTextureIndex;

public:
    bool IsGameCleared() const { return isGameCleared_; }
    float GetExtractionTimer() const { return extractionTimer_; }
    const char* GetSceneType() const override { return "GAMEPLAY"; }
    Vector3 GetPlayerPosition() const { return player_ ? player_->GetPosition() : Vector3{0.0f, 0.0f, 0.0f}; }
    Vector3 GetGoalPosition() const { return goalRingTransform_.translate; }

private:
    Sprite::Transform uvTransformSprite;
    Vector2 uiSpritePosition = { 100.0f, 100.0f };
    Vector3 bulletSpawnOffset_ = { 0.0f, 0.2f, 0.5f };
    float bulletSpeed_ = 0.45f;
    float bulletLifeTime_ = 2.0f;
    float playerShotCooldown_ = 0.12f;
    float playerShotCooldownTimer_ = 0.0f;
    float enemyShotCooldown_ = 2.0f; // 2.0秒間隔
    float enemyShotCooldownTimer_ = 0.0f;
    float bulletHitRadius_ = 0.25f;
    float playerHitRadius_ = 0.6f;
    float enemyHitRadius_ = 0.6f;
private:
    const float kDeltaTime = 1.0f / 60.0f;

    std::unique_ptr<Ring> goalRing_;
    Sprite::Transform goalRingTransform_{};
    bool isGameCleared_ = false;
    float extractionTimer_ = 5.0f;
    std::chrono::steady_clock::time_point lastTime_;

    // プレイヤーHPバー用のスプライト
    Sprite* playerHpBarBg_ = nullptr;
    Sprite* playerHpBarFg_ = nullptr;
};