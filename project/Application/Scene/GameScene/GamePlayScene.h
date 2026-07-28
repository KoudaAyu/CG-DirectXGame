#pragma once

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

class Camera;
class SpriteCom;
class SkinningObject3dCom;
struct SceneRenderRequests;

class GamePlayScene : public BaseScene
{
public:
    
    void Initialize(DirectXCom* dxCommon, Camera* camera) override;

    void Finalize() override;

    void Update() override;

   void Draw(SceneRenderRequests& renderRequests) override;
   void DrawUI() override;

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
    SkinningObject3dCom* skinningObject3dCom = nullptr;
    ParticleManager* particleManager = nullptr;
    std::unique_ptr<HitEffect> hitEffect_;
    std::unique_ptr<Object3d> animatedCube_;
    std::unique_ptr<Object3d> multiMeshObject_;
    std::unique_ptr<Object3d> weaponObject_;
    std::unique_ptr<Sphere> sphere_;
    Skeleton skeleton_{};
    Animation animation_{};
    Animation sneakWalkAnimation_{};
    Animator animator_{};
    SkeletonDebug skeletonDebug_{};
    DebugCamera debugCamera_;
    std::vector<std::unique_ptr<Sprite>> sprites;
    std::unique_ptr<SpriteManager> spriteManager_;
    std::list<ParticleManager::Particle> particles;
    std::list<ParticleManager::Particle> hitEffectParticles;

    bool sphereInitialized = false;
    bool hitEffectInitialized = false;
    bool animatedCubeInitialized_ = false;
    bool multiMeshObjectInitialized_ = false;
    bool weaponObjectInitialized_ = false;
    bool isWeaponAttached_ = false;
    int32_t selectedJointIndex_ = 0;
    Vector3 weaponOffsetTranslate_ = { 0.0f, 0.0f, 0.0f };
    Vector3 weaponOffsetRotate_ = { 0.0f, 0.0f, 0.0f };
    Vector3 weaponOffsetScale_ = { 1.0f, 1.0f, 1.0f };

    // Head LookAt 視線追従用
    bool isHeadLookAtEnabled_ = true;
    int headLookAtMode_ = 1; // 0: Off, 1: LookAt Camera, 2: Custom Target
    float headLookAtWeight_ = 0.75f;
    Vector3 headLookAtTargetPos_ = { 0.0f, 2.0f, 5.0f };
    bool showSkeletonDebug_ = true;
    bool isHandParticleEmitting_ = true;
    Emitter handEmitter_{};

	// テクスチャインデックスは TextureManager で管理されるため、ここではインデックスを保持するだけにする
    uint32_t cylinderTextureIndex_ = TextureManager::kInvalidTextureIndex;
    uint32_t particleTextureA = TextureManager::kInvalidTextureIndex;
    uint32_t particleTextureB = TextureManager::kInvalidTextureIndex;

private:
    Sprite::Transform uvTransformSprite;
    Vector2 uiSpritePosition = { 100.0f, 100.0f };
private:
    const float kDeltaTime = 1.0f / 60.0f;
};