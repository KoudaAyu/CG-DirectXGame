#pragma once

#include "DirectXCom.h"
#include "Sprite.h"
#include "Camera.h"
#include "Matrix4x4.h"
#include "Vector.h"
#include <wrl.h>
#include <DirectXMath.h>
#include <cassert>
#include <cmath>
#include <random>

class ParticleSystem
{
public:
    ParticleSystem();
    ~ParticleSystem();

    void Initialize(DirectXCom* dxCommon, Sprite* quadSprite, uint32_t numInstances = 8);
    void Spawn8();
    void SpawnFirework();
    // New spawns for keys 3..0
    void SpawnSpiral();   // 3
    void SpawnFountain(); // 4
    void SpawnBurst();    // 5
    void SpawnSmoke();    // 6
    void SpawnRain();     // 7
    void SpawnRing();     // 8
    void SpawnHelix();    // 9
    void SpawnExpanding();// 0

    void Update(float dt, Camera* camera);
    void Draw(D3D12_GPU_DESCRIPTOR_HANDLE textureSrvGPU, D3D12_GPU_DESCRIPTOR_HANDLE textureSrvGPU2,
        Microsoft::WRL::ComPtr<ID3D12Resource> materialResource,
        Microsoft::WRL::ComPtr<ID3D12Resource> directionalLight,
        D3D12_GPU_DESCRIPTOR_HANDLE instanceSrvHandleGPU,
        bool useMonsterBall);

    const Microsoft::WRL::ComPtr<ID3D12Resource>& GetInstanceResource() const { return instanceResource_; }
    uint32_t GetNumInstances() const { return numInstances_; }
    bool HasAliveParticles() const;
    bool IsColorCycleActive() const;

    // public instance data layout so external code can create SRV with correct stride
    struct InstanceData
    {
        TransformationMatrix tm; // WVP + World
        float alpha;             // alpha value (0..1)
        float pad[3];            // padding to align to 16 bytes
    };

    // Setter to control fade speed (exponent > 1 -> faster near end)
    void SetAlphaExponent(float e) { alphaExponent_ = e; }

private:
    struct Particle
    {
        Vector3 pos;
        Vector3 vel;
        float life;
        float initialLife; // store initial life for alpha calculation
    };

    uint32_t numInstances_ = 8;
    Particle* particles_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> instanceResource_ = nullptr;
    InstanceData* instanceData_ = nullptr;
    DirectXCom* dxCommon_ = nullptr;
    Sprite* quad_ = nullptr;

    std::mt19937 rng_;

    bool colorCycleActive_ = false;

    DirectX::XMFLOAT4 particleColor_ = {1.0f,1.0f,1.0f,1.0f};
    float particleScale_ = 0.5f;
    bool useTexture2_ = false;

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_ = nullptr;
    Sprite::Material* materialDataLocal_ = nullptr;

    // exponent applied to normalized life when computing alpha. >1 => faster fade
    float alphaExponent_ = 2.0f;
};

// Inline implementations
inline ParticleSystem::ParticleSystem() {}
inline ParticleSystem::~ParticleSystem()
{
    if (particles_) delete[] particles_;
}

inline void ParticleSystem::Initialize(DirectXCom* dxCommon, Sprite* quadSprite, uint32_t numInstances)
{
    dxCommon_ = dxCommon;
    quad_ = quadSprite;
    numInstances_ = numInstances;

    rng_.seed(static_cast<unsigned int>(std::chrono::high_resolution_clock::now().time_since_epoch().count()));

    particles_ = new Particle[numInstances_];
    for (uint32_t i = 0; i < numInstances_; ++i)
    {
        particles_[i].pos = {0.0f,0.0f,0.0f};
        particles_[i].vel = {0.0f,0.0f,0.0f};
        particles_[i].life = 0.0f;
        particles_[i].initialLife = 1.0f;
    }

    instanceResource_ = dxCommon_->CreateBufferResource(dxCommon_->GetDevice(), sizeof(InstanceData) * numInstances_);
    instanceResource_->Map(0, nullptr, reinterpret_cast<void**>(&instanceData_));
    for (uint32_t i = 0; i < numInstances_; ++i)
    {
        instanceData_[i].tm.WVP = MakeIdentity4x4();
        instanceData_[i].tm.World = MakeIdentity4x4();
        instanceData_[i].alpha = 0.0f;
    }

    materialResource_ = dxCommon_->CreateBufferResource(dxCommon_->GetDevice(), sizeof(Sprite::Material));
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialDataLocal_));
    materialDataLocal_->color = {1.0f,1.0f,1.0f,1.0f};
    materialDataLocal_->enableLighting = false;
    materialDataLocal_->uvTransform = MakeIdentity4x4();
}

inline void ParticleSystem::Spawn8()
{
    const float twoPi = DirectX::XM_2PI;
    const float speed = 5.0f;
    for (uint32_t i = 0; i < numInstances_; ++i)
    {
        float angle = twoPi * float(i) / float(numInstances_);
        particles_[i].pos = {0.0f,0.0f,0.0f};
        particles_[i].vel = {cosf(angle) * speed, sinf(angle) * speed, 0.0f};
        particles_[i].life = 3.0f;
        particles_[i].initialLife = particles_[i].life;
    }
    particleColor_ = {1.0f,0.5f,0.2f,1.0f};
    particleScale_ = 0.45f;
    useTexture2_ = false;
    colorCycleActive_ = false;
}

inline void ParticleSystem::SpawnFirework()
{
    std::uniform_real_distribution<float> angleDist(0.0f, DirectX::XM_2PI);
    std::uniform_real_distribution<float> speedDist(2.0f, 8.0f);
    std::uniform_real_distribution<float> biasDist(-1.0f, 1.0f);

    float originX = biasDist(rng_) * 2.0f;
    float originY = -3.0f;

    for (uint32_t i = 0; i < numInstances_; ++i)
    {
        float angle = angleDist(rng_);
        float speed = speedDist(rng_);
        float upBias = 6.0f + biasDist(rng_) * 2.0f;
        particles_[i].pos = { originX + biasDist(rng_) * 0.2f, originY + biasDist(rng_) * 0.2f, 0.0f };
        particles_[i].vel = { cosf(angle) * speed * 0.7f, sinf(angle) * speed * 0.7f + upBias, 0.0f };
        particles_[i].life = 2.5f + biasDist(rng_) * 0.8f;
        particles_[i].initialLife = particles_[i].life;
    }
    particleColor_ = {1.0f,0.85f,0.4f,1.0f};
    particleScale_ = 0.35f;
    useTexture2_ = true;
    colorCycleActive_ = true;
}

inline void ParticleSystem::SpawnSpiral()
{
    const float twoPi = DirectX::XM_2PI;
    std::uniform_real_distribution<float> radiusDist(0.1f, 0.6f);
    for (uint32_t i = 0; i < numInstances_; ++i)
    {
        float t = float(i) / float(numInstances_);
        float angle = twoPi * t * 4.0f;
        float r = radiusDist(rng_) + t * 1.2f;
        particles_[i].pos = {0.0f,0.0f,0.0f};
        particles_[i].vel = {cosf(angle) * r * 2.5f, sinf(angle) * r * 2.5f + 1.0f * t, 0.0f};
        particles_[i].life = 3.0f + t * 2.0f;
        particles_[i].initialLife = particles_[i].life;
    }
    particleColor_ = {0.4f,0.8f,1.0f,1.0f};
    particleScale_ = 0.4f;
    useTexture2_ = false;
    colorCycleActive_ = false;
}

inline void ParticleSystem::SpawnFountain()
{
    std::uniform_real_distribution<float> angleDist(-0.5f, 0.5f);
    std::uniform_real_distribution<float> speedDist(6.0f, 10.0f);
    for (uint32_t i = 0; i < numInstances_; ++i)
    {
        float ang = angleDist(rng_);
        float speed = speedDist(rng_);
        particles_[i].pos = {0.0f,-1.0f,0.0f};
        particles_[i].vel = {ang * 2.0f, speed, 0.0f};
        particles_[i].life = 2.0f + (float(i) / numInstances_) * 1.5f;
        particles_[i].initialLife = particles_[i].life;
    }
    particleColor_ = {0.8f,0.9f,0.3f,1.0f};
    particleScale_ = 0.5f;
    useTexture2_ = false;
    colorCycleActive_ = false;
}

inline void ParticleSystem::SpawnBurst()
{
    std::uniform_real_distribution<float> angleDist(0.0f, DirectX::XM_2PI);
    std::uniform_real_distribution<float> speedDist(3.0f, 12.0f);
    for (uint32_t i = 0; i < numInstances_; ++i)
    {
        float a = angleDist(rng_);
        float s = speedDist(rng_);
        particles_[i].pos = {0.0f,0.0f,0.0f};
        particles_[i].vel = {cosf(a) * s, sinf(a) * s, 0.0f};
        particles_[i].life = 1.2f + (s / 12.0f) * 1.5f;
        particles_[i].initialLife = particles_[i].life;
    }
    particleColor_ = {1.0f,0.3f,0.9f,1.0f};
    particleScale_ = 0.3f;
    useTexture2_ = true;
    colorCycleActive_ = false;
}

inline void ParticleSystem::SpawnSmoke()
{
    std::uniform_real_distribution<float> spread(-0.5f, 0.5f);
    std::uniform_real_distribution<float> up(0.5f, 1.2f);
    for (uint32_t i = 0; i < numInstances_; ++i)
    {
        particles_[i].pos = {spread(rng_) * 0.5f, -1.0f + spread(rng_) * 0.2f, 0.0f};
        particles_[i].vel = {spread(rng_) * 0.2f, up(rng_) * 0.5f + 0.2f, 0.0f};
        particles_[i].life = 4.0f + (float(i) / numInstances_) * 2.0f;
        particles_[i].initialLife = particles_[i].life;
    }
    particleColor_ = {0.15f,0.15f,0.15f,0.9f};
    particleScale_ = 0.9f;
    useTexture2_ = false;
    colorCycleActive_ = false;
}

inline void ParticleSystem::SpawnRain()
{
    std::uniform_real_distribution<float> xDist(-6.0f, 6.0f);
    std::uniform_real_distribution<float> yDist(4.0f, 8.0f);
    for (uint32_t i = 0; i < numInstances_; ++i)
    {
        particles_[i].pos = {xDist(rng_), yDist(rng_), 0.0f};
        particles_[i].vel = {0.0f, -6.0f - (float(i) / numInstances_) * 2.0f, 0.0f};
        particles_[i].life = 1.5f + (float(i) / numInstances_) * 0.8f;
        particles_[i].initialLife = particles_[i].life;
    }
    particleColor_ = {0.4f,0.6f,1.0f,1.0f};
    particleScale_ = 0.25f;
    useTexture2_ = false;
    colorCycleActive_ = false;
}

inline void ParticleSystem::SpawnRing()
{
    const float twoPi = DirectX::XM_2PI;
    std::uniform_real_distribution<float> radiusNoise(0.9f, 1.1f);
    for (uint32_t i = 0; i < numInstances_; ++i)
    {
        float a = twoPi * float(i) / float(numInstances_);
        float r = radiusNoise(rng_);
        particles_[i].pos = {cosf(a) * r * 0.1f, sinf(a) * r * 0.1f, 0.0f};
        particles_[i].vel = {cosf(a) * r * 5.0f, sinf(a) * r * 5.0f, 0.0f};
        particles_[i].life = 2.2f + (float(i) / numInstances_) * 1.0f;
        particles_[i].initialLife = particles_[i].life;
    }
    particleColor_ = {1.0f,0.6f,0.2f,1.0f};
    particleScale_ = 0.5f;
    useTexture2_ = true;
    colorCycleActive_ = false;
}

inline void ParticleSystem::SpawnHelix()
{
    const float twoPi = DirectX::XM_2PI;
    for (uint32_t i = 0; i < numInstances_; ++i)
    {
        float t = float(i) / float(numInstances_);
        float angle = twoPi * t * 3.0f;
        float height = t * 4.0f;
        particles_[i].pos = {cosf(angle) * 0.2f, -1.0f + height, sinf(angle) * 0.2f};
        particles_[i].vel = {-sinf(angle) * 2.5f, 2.0f + t * 1.5f, cosf(angle) * 2.5f};
        particles_[i].life = 3.0f + t * 2.0f;
        particles_[i].initialLife = particles_[i].life;
    }
    particleColor_ = {0.9f,0.4f,0.7f,1.0f};
    particleScale_ = 0.45f;
    useTexture2_ = false;
    colorCycleActive_ = false;
}

inline void ParticleSystem::SpawnExpanding()
{
    std::uniform_real_distribution<float> ang(0.0f, DirectX::XM_2PI);
    std::uniform_real_distribution<float> sp(0.2f, 1.2f);
    for (uint32_t i = 0; i < numInstances_; ++i)
    {
        float a = ang(rng_);
        float s = sp(rng_);
        particles_[i].pos = {cosf(a) * 0.2f * s, sinf(a) * 0.2f * s, 0.0f};
        particles_[i].vel = {cosf(a) * s * 1.5f, sinf(a) * s * 1.5f, 0.0f};
        particles_[i].life = 3.0f + s;
        particles_[i].initialLife = particles_[i].life;
    }
    particleColor_ = {0.6f,1.0f,0.6f,0.9f};
    particleScale_ = 0.6f;
    useTexture2_ = false;
    colorCycleActive_ = false;
}

inline void ParticleSystem::Update(float dt, Camera* camera)
{
    for (uint32_t i = 0; i < numInstances_; ++i)
    {
        if (particles_[i].life > 0.0f)
        {
            particles_[i].vel.y -= 9.8f * dt * 0.5f;
            particles_[i].pos.x += particles_[i].vel.x * dt;
            particles_[i].pos.y += particles_[i].vel.y * dt;
            particles_[i].pos.z += particles_[i].vel.z * dt;
            particles_[i].life -= dt;
        }

        float lifeFactor = (particles_[i].life > 0.0f) ? particles_[i].life : 0.0f;
        float scaleFactor = particleScale_ * (0.5f + 0.5f * (lifeFactor / 3.0f));

        Vector3 scale = {scaleFactor, scaleFactor, 1.0f};
        Vector3 rotate = {0.0f,0.0f,0.0f};
        Vector3 translate = particles_[i].pos;
        Matrix4x4 world = MakeAffineMatrix(scale, rotate, translate);
        Matrix4x4 wvp = Multiply(world, camera->GetViewProjectionMatrix());
        instanceData_[i].tm.World = world;
        instanceData_[i].tm.WVP = wvp;

        // compute alpha based on remaining life and initialLife (clamped)
        float alpha = 0.0f;
        if (particles_[i].initialLife > 0.0f)
        {
            float t = particles_[i].life / particles_[i].initialLife;
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
            // apply exponent to make fade faster when exponent > 1
            alpha = powf(t, alphaExponent_);
        }
        instanceData_[i].alpha = alpha;
    }
}

inline void ParticleSystem::Draw(D3D12_GPU_DESCRIPTOR_HANDLE textureSrvGPU, D3D12_GPU_DESCRIPTOR_HANDLE textureSrvGPU2,
    Microsoft::WRL::ComPtr<ID3D12Resource> /*materialResource*/,
    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLight,
    D3D12_GPU_DESCRIPTOR_HANDLE instanceSrvHandleGPU,
    bool useMonsterBall)
{
    if (!quad_) return;
    auto cmd = dxCommon_->GetCommandList();
    const D3D12_VERTEX_BUFFER_VIEW& quadVB = quad_->GetVertexBufferViewSprite();
    const D3D12_INDEX_BUFFER_VIEW& quadIB = quad_->GetIndexBufferViewSprite();
    cmd->IASetVertexBuffers(0, 1, &quadVB);
    cmd->IASetIndexBuffer(&quadIB);
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    materialDataLocal_->color = {particleColor_.x, particleColor_.y, particleColor_.z, particleColor_.w};
    materialDataLocal_->enableLighting = 0;

    cmd->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
    cmd->SetGraphicsRootConstantBufferView(1, quad_->GetTransformationMatrixResourceSprite()->GetGPUVirtualAddress());

    D3D12_GPU_DESCRIPTOR_HANDLE chosenTexture = useTexture2_ ? textureSrvGPU2 : textureSrvGPU;
    if (useMonsterBall) chosenTexture = textureSrvGPU2;

    cmd->SetGraphicsRootDescriptorTable(2, chosenTexture);
    cmd->SetGraphicsRootConstantBufferView(3, directionalLight->GetGPUVirtualAddress());
    cmd->SetGraphicsRootDescriptorTable(4, instanceSrvHandleGPU);

    uint32_t aliveCount = 0;
    for (uint32_t i = 0; i < numInstances_; ++i) if (particles_[i].life > 0.0f) ++aliveCount;
    if (aliveCount == 0) return;

    cmd->DrawIndexedInstanced(6, aliveCount, 0, 0, 0);
}

inline bool ParticleSystem::HasAliveParticles() const
{
    for (uint32_t i = 0; i < numInstances_; ++i) if (particles_[i].life > 0.0f) return true;
    return false;
}

inline bool ParticleSystem::IsColorCycleActive() const { return colorCycleActive_; }
