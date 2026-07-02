#pragma once
#pragma once

#include <list>
#include <memory>

#include "Camera.h"
#include "Cylinder.h"
#include "DirectXCom.h"
#include "Light.h"
#include "MaterialManager.h"
#include "Object3dCom.h"
#include "ParticleManager.h"
#include "Ring.h"
#include "Sprite.h"
#include "TextureManager.h"
#include <sstream>
#include <Windows.h>

class HitEffect
{
public:
    struct Desc
    {
        DirectXCom* dxCommon = nullptr;
        Object3dCom* object3dCom = nullptr;
        MaterialManager* materialManager = nullptr;
        Light* light = nullptr;
        Camera* camera = nullptr;
        ParticleManager* particleManager = nullptr;
        uint32_t ringDivide = 64;
        float outerRadius = 1.0f;
        float innerRadius = 0.2f;
        uint32_t cylinderDivide = 32;
        float topRadius = 1.0f;
        float bottomRadius = 1.0f;
        float height = 3.0f;
        bool ringEnabled = false;
        bool cylinderEnabled = true;
        float effectDuration = 0.35f;
        Vector3 cylinderScale = { 1.0f, 1.0f, 1.0f };
    };

    static std::unique_ptr<HitEffect> Create(const Desc& desc);

    void Initialize(DirectXCom* dxCommon, Object3dCom* object3dCom, MaterialManager* materialManager, Light* light, Camera* camera,
        uint32_t ringDivide = 64, float outerRadius = 1.0f, float innerRadius = 0.2f,
        uint32_t cylinderDivide = 32, float topRadius = 1.0f, float bottomRadius = 1.0f, float height = 3.0f);

    void Finalize();

    void Update(float deltaTime = 1.0f / 60.0f);

    void Play(const Vector3& translate);
    // 各要素（リング、シリンダー、平面パーティクル）を個別に発生させるための便宜的なAPI。
    void PlayRing(const Vector3& translate);
    void PlayCylinder(const Vector3& translate);
    void SpawnPlaneParticles(const Vector3& translate);

    void Draw() const;

    void SetTextureIndex(uint32_t textureIndex) { textureIndex_ = textureIndex; }
    void SetRingEnabled(bool enabled) { ringEnabled_ = enabled; }
    void SetCylinderEnabled(bool enabled) { cylinderEnabled_ = enabled; }
    void SetEffectDuration(float duration) { effectDuration_ = duration; }
    void SetParticleManager(ParticleManager* particleManager) { particleManager_ = particleManager; }
    void SetPlaneParticleTextureIndex(uint32_t textureIndex) { planeParticleTextureIndex_ = textureIndex; }
    void SetPlaneParticleCount(uint32_t count) { planeParticleCount_ = count; }
    void SetRingTextureIndex(uint32_t textureIndex) { ringTextureIndex_ = textureIndex; }
    bool IsInitialized() const { return initialized_; }
    bool IsActive() const { return active_; }

    Sprite::Transform& GetRingTransform() { return ringTransform_; }
    Sprite::Transform& GetCylinderTransform() { return cylinderTransform_; }
    const Sprite::Transform& GetRingTransform() const { return ringTransform_; }
    const Sprite::Transform& GetCylinderTransform() const { return cylinderTransform_; }

private:
    std::unique_ptr<Ring> ring_;
    std::unique_ptr<Cylinder> cylinder_;
    ParticleManager* particleManager_ = nullptr;
    Sprite::Transform ringTransform_ = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
    Sprite::Transform cylinderTransform_ = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
    uint32_t textureIndex_ = TextureManager::kInvalidTextureIndex;
    uint32_t ringTextureIndex_ = TextureManager::kInvalidTextureIndex;
    uint32_t planeParticleTextureIndex_ = TextureManager::kInvalidTextureIndex;
    uint32_t planeParticleCount_ = 3;
    bool ringEnabled_ = false;
    bool cylinderEnabled_ = true;
    bool initialized_ = false;
    bool active_ = false;
    float elapsedTime_ = 0.0f;
    float effectDuration_ = 0.35f;
    float ringScaleSpeed_ = 4.0f;
    float ringRotationSpeedZ_ = 8.0f;
    float cylinderScaleSpeedX_ = 1.5f;
    float cylinderScaleSpeedZ_ = 1.5f;
    float cylinderRotationSpeedY_ = 6.0f;
    Vector3 initialRingScale_ = { 0.2f, 0.2f, 1.0f };
    Vector3 initialCylinderScale_ = { 0.35f, 1.0f, 0.35f };
};
