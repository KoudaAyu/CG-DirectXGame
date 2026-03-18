#pragma once

#include "BaseScene.h"
#include"DirectXCom.h"
#include"ParticleEmitter.h"
#include"Sound.h"
#include"Sphere.h"

#include <vector>

#include "Object3dCom.h"
#include "Light.h"
#include "MaterialManager.h"
#include "ParticleManager.h"
#include "Sprite.h"

class Camera;
class SpriteCom;

class GamePlayScene : public BaseScene
{
public:
    
    void Initialize(DirectXCom* dxCommon, Camera* camera) override;

    void Finalize() override;

    void Update() override;

    void Draw() override;

    void SetSpriteCom(SpriteCom* spriteCom) { this->spriteCom = spriteCom; }

private:

    Camera* camera_ = nullptr;
    DirectXCom* directXCom = nullptr;
    Emitter emitter;
    Sound* sound = nullptr;
    SpriteCom* spriteCom = nullptr;
    ParticleEmitter particleEmitter;
    Light* light = nullptr;
    MaterialManager* materialManager = nullptr;
    Object3dCom* object3dCom = nullptr;
    ParticleManager* particleManager = nullptr;
    std::unique_ptr<Sphere> sphere;
    std::unique_ptr<Sphere> sphere_;
    std::vector<std::unique_ptr<Sprite>> sprites;
    std::list<ParticleManager::Particle> particles;

    bool pendingSphereInit = false;
    bool sphereInitialized = false;

    // Try to obtain engine resources and initialize the sphere; returns true on success
    bool TryInitializeSphere();

private:
    const float kDeltaTime = 1.0f / 60.0f;
    bool drawSphere = false;
};