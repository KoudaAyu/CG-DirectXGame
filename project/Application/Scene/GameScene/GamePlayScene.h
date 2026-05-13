#pragma once

#include "BaseScene.h"
#include"Baziru3_Engine\Effect\HitEffect.h"
#include"DirectXCom.h"
#include"ParticleEmitter.h"
#include"Cylinder.h"
#include"Sphere.h"

#include <vector>

#include "Object3dCom.h"
#include "Object3d.h"
#include "Light.h"
#include "MaterialManager.h"
#include "ParticleManager.h"
#include "Animation.h"
#include "Skeleton.h"
#include "Sprite.h"
#include "SpriteManager.h" 
#include "DebugCamera.h"

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
    std::unique_ptr<Sphere> sphere_;
    Skeleton skeleton_{};
    Animation animation_{};
    std::vector<std::unique_ptr<Sphere>> jointDebugSpheres_;
    std::vector<std::unique_ptr<Cylinder>> jointDebugCylinders_;
    DebugCamera debugCamera_;
    std::vector<std::unique_ptr<Sprite>> sprites;
    std::unique_ptr<SpriteManager> spriteManager_;
    std::list<ParticleManager::Particle> particles;
    std::list<ParticleManager::Particle> hitEffectParticles;

    bool sphereInitialized = false;
    bool hitEffectInitialized = false;
    bool animatedCubeInitialized_ = false;
    bool animationInitialized_ = false;
    bool skeletonInitialized_ = false;

	// テクスチャインデックスは TextureManager で管理されるため、ここではインデックスを保持するだけにする
    uint32_t cylinderTextureIndex_ = TextureManager::kInvalidTextureIndex;
    uint32_t particleTextureA = TextureManager::kInvalidTextureIndex;
    uint32_t particleTextureB = TextureManager::kInvalidTextureIndex;

private:
    Sprite::Transform uvTransformSprite;
    Vector2 uiSpritePosition = { 100.0f, 100.0f };
private:
    const float kDeltaTime = 1.0f / 60.0f;
    float animationTime_ = 0.0f;
};