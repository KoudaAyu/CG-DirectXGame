#include "HitEffect.h"

#include <sstream>
#include <Windows.h>

std::unique_ptr<HitEffect> HitEffect::Create(const Desc& desc)
{
    auto effect = std::make_unique<HitEffect>();
    effect->Initialize(
        desc.dxCommon, desc.object3dCom, desc.materialManager, desc.light, desc.camera,
        desc.ringDivide, desc.outerRadius, desc.innerRadius,
        desc.cylinderDivide, desc.topRadius, desc.bottomRadius, desc.height
    );
    if (desc.particleManager)
    {
        effect->SetParticleManager(desc.particleManager);
    }
    effect->SetRingEnabled(desc.ringEnabled);
    effect->SetCylinderEnabled(desc.cylinderEnabled);
    effect->SetEffectDuration(desc.effectDuration);
    effect->GetCylinderTransform().scale = desc.cylinderScale;
    effect->Update(1.0f / 60.0f); // 初期アップデート
    return effect;
}

void HitEffect::Initialize(DirectXCom* dxCommon, Object3dCom* object3dCom, MaterialManager* materialManager, Light* light, Camera* camera,
    uint32_t ringDivide, float outerRadius, float innerRadius,
    uint32_t cylinderDivide, float topRadius, float bottomRadius, float height)
{
    Finalize();

    ring_ = std::make_unique<Ring>();
    ring_->Initialize(dxCommon, object3dCom, materialManager, light, camera, ringDivide, outerRadius, innerRadius);

    cylinder_ = std::make_unique<Cylinder>();
    cylinder_->Initialize(dxCommon, object3dCom, materialManager, light, camera, cylinderDivide, topRadius, bottomRadius, height);

    initialized_ = true;
}

void HitEffect::Finalize()
{
    if (ring_)
    {
        ring_->Finalize();
        ring_.reset();
    }

    if (cylinder_)
    {
        cylinder_->Finalize();
        cylinder_.reset();
    }

    initialized_ = false;
    textureIndex_ = TextureManager::kInvalidTextureIndex;
}

void HitEffect::Update(float deltaTime)
{
    if (!initialized_)
    {
        return;
    }

    if (ringEnabled_ && active_)
    {
        // 毎フレームのアニメーションを適用する。Play() が呼ばれた同フレームで初期拡大を適用しないようにする
        // （最初のフレームは意図した初期スケールで表示されるようにするため）。
        if (elapsedTime_ > 0.0f)
        {
            ringTransform_.scale.x += ringScaleSpeed_ * deltaTime;
            ringTransform_.scale.y += ringScaleSpeed_ * deltaTime;
            ringTransform_.rotate.z += ringRotationSpeedZ_ * deltaTime;

            cylinderTransform_.scale.x += cylinderScaleSpeedX_ * deltaTime;
            cylinderTransform_.scale.z += cylinderScaleSpeedZ_ * deltaTime;
            cylinderTransform_.rotate.y += cylinderRotationSpeedY_ * deltaTime;
        }

        if (ring_)
        {
            ring_->SetTransform(ringTransform_);
            ring_->Update();
        }

        if (cylinder_)
        {
            cylinder_->SetTransform(cylinderTransform_);
            cylinder_->Update();
        }

        elapsedTime_ += deltaTime;
        if (elapsedTime_ >= effectDuration_)
        {
            active_ = false;
        }
    }

    // Debug update log removed to reduce log spam
}

void HitEffect::Play(const Vector3& translate)
{
    if (!initialized_)
    {
        return;
    }

    active_ = true;
    elapsedTime_ = 0.0f;
    ringTransform_.translate = translate;
    ringTransform_.scale = initialRingScale_;
    ringTransform_.rotate = { 0.0f, 0.0f, 0.0f };

    cylinderTransform_.translate = translate;
    cylinderTransform_.scale = initialCylinderScale_;
    cylinderTransform_.rotate = { 0.0f, 0.0f, 0.0f };

    // 初期変換を即座に適用する。これにより Play() が呼ばれた同フレームでも
    // 意図した初期スケールで描画されるようにする。
    if (ringEnabled_ && ring_)
    {
        ring_->SetTransform(ringTransform_);
        ring_->Update();
    }

    if (cylinderEnabled_ && cylinder_)
    {
        cylinder_->SetTransform(cylinderTransform_);
        cylinder_->Update();
    }

    if (particleManager_ && planeParticleTextureIndex_ != TextureManager::kInvalidTextureIndex)
    {
        std::list<ParticleManager::Particle> newParticles;
        for (uint32_t i = 0; i < planeParticleCount_; ++i)
        {
            auto p = particleManager_->MakeNewParticles(particleManager_->GetRandomEngine(), translate);
            p.textureIndex = planeParticleTextureIndex_;
            p.lifeTime = effectDuration_;
            newParticles.push_back(p);
        }

        particleManager_->AddParticles(newParticles);
    }

    // Debug play log removed to reduce log spam
}

void HitEffect::PlayRing(const Vector3& translate)
{
    if (!initialized_)
    {
        return;
    }

    ringTransform_.translate = translate;
    ringTransform_.scale = initialRingScale_;
    ringTransform_.rotate = { 0.0f, 0.0f, 0.0f };

    if (ring_)
    {
        ring_->SetTransform(ringTransform_);
        ring_->Update();
    }

    // 描画対象として扱われるように全体をアクティブにマークする
    active_ = true;
    elapsedTime_ = 0.0f;
}

void HitEffect::PlayCylinder(const Vector3& translate)
{
    if (!initialized_)
    {
        return;
    }

    cylinderTransform_.translate = translate;
    cylinderTransform_.scale = initialCylinderScale_;
    cylinderTransform_.rotate = { 0.0f, 0.0f, 0.0f };

    if (cylinder_)
    {
        cylinder_->SetTransform(cylinderTransform_);
        cylinder_->Update();
    }

    // 描画対象として扱われるように全体をアクティブにマークする
    active_ = true;
    elapsedTime_ = 0.0f;
}

void HitEffect::SpawnPlaneParticles(const Vector3& translate)
{
    if (!initialized_ || !particleManager_ || planeParticleTextureIndex_ == TextureManager::kInvalidTextureIndex)
    {
        return;
    }

    std::list<ParticleManager::Particle> newParticles;
    for (uint32_t i = 0; i < planeParticleCount_; ++i)
    {
        auto p = particleManager_->MakeNewParticles(particleManager_->GetRandomEngine(), translate);
        p.textureIndex = planeParticleTextureIndex_;
        p.lifeTime = effectDuration_;
        newParticles.push_back(p);
    }

    particleManager_->AddParticles(newParticles);
}

void HitEffect::Draw() const
{
    if (!initialized_ || textureIndex_ == TextureManager::kInvalidTextureIndex)
    {
        return;
    }

    // リングとシリンダーは別のテクスチャを使うことがあるため、ハンドルを個別に取得する。
    if (ringEnabled_ && active_ && ring_)
    {
        if (ringTextureIndex_ != TextureManager::kInvalidTextureIndex)
        {
            D3D12_GPU_DESCRIPTOR_HANDLE ringHandle = TextureManager::GetInstance()->GetSrvHandleGPU(ringTextureIndex_);
            if (ringHandle.ptr != 0)
            {
                ring_->Draw(ringHandle);
            }
        }
    }

    if (cylinderEnabled_ && active_ && cylinder_)
    {
        if (textureIndex_ != TextureManager::kInvalidTextureIndex)
        {
            D3D12_GPU_DESCRIPTOR_HANDLE cylinderHandle = TextureManager::GetInstance()->GetSrvHandleGPU(textureIndex_);
            if (cylinderHandle.ptr != 0)
            {
                cylinder_->Draw(cylinderHandle);
            }
        }
    }
}
