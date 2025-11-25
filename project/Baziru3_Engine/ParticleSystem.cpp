#include "ParticleSystem.h"
#include <DirectXMath.h>
#include <cassert>
#include <cmath>
#include <random>

ParticleSystem::ParticleSystem()
{
}

ParticleSystem::~ParticleSystem()
{
    if (particles_)
    {
        delete[] particles_;
    }
    // ComPtrs release resources automatically
}

void ParticleSystem::Initialize(DirectXCom* dxCommon, Sprite* quadSprite, uint32_t numInstances)
{
    dxCommon_ = dxCommon;
    quad_ = quadSprite;
    numInstances_ = numInstances;

    // Seed RNG
    rng_.seed(static_cast<unsigned int>(std::chrono::high_resolution_clock::now().time_since_epoch().count()));

    particles_ = new Particle[numInstances_];
    for (uint32_t i = 0; i < numInstances_; ++i)
    {
        particles_[i].pos = { 0.0f,0.0f,0.0f };
        particles_[i].vel = { 0.0f,0.0f,0.0f };
        particles_[i].life = 0.0f;
    }

    instanceResource_ = dxCommon_->CreateBufferResource(dxCommon_->GetDevice(), sizeof(TransformationMatrix) * numInstances_);
    instanceResource_->Map(0, nullptr, reinterpret_cast<void**>(&instanceData_));
    for (uint32_t i = 0; i < numInstances_; ++i)
    {
        instanceData_[i].WVP = MakeIdentity4x4();
        instanceData_[i].World = MakeIdentity4x4();
    }

    // Create internal material resource (so particle tint doesn't overwrite global material)
    materialResource_ = dxCommon_->CreateBufferResource(dxCommon_->GetDevice(), sizeof(Sprite::Material));
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialDataLocal_));
    materialDataLocal_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    materialDataLocal_->enableLighting = false;
    materialDataLocal_->uvTransform = MakeIdentity4x4();
}

void ParticleSystem::Spawn8()
{
    const float twoPi = DirectX::XM_2PI;
    const float speed = 5.0f;
    for (uint32_t i = 0; i < numInstances_; ++i)
    {
        float angle = twoPi * float(i) / float(numInstances_);
        particles_[i].pos = { 0.0f,0.0f,0.0f };
        particles_[i].vel = { cosf(angle) * speed, sinf(angle) * speed, 0.0f };
        particles_[i].life = 3.0f;
    }
    // distinct visuals
    particleColor_ = {1.0f, 0.5f, 0.2f, 1.0f};
    particleScale_ = 0.45f;
    useTexture2_ = false;
    colorCycleActive_ = false;
}

void ParticleSystem::SpawnFirework()
{
    // Firework: particles start near bottom, shoot up a bit and spread outward
    std::uniform_real_distribution<float> angleDist(0.0f, DirectX::XM_2PI);
    std::uniform_real_distribution<float> speedDist(2.0f, 8.0f);
    std::uniform_real_distribution<float> biasDist(-1.0f, 1.0f);

    // Launch origin slightly randomized horizontally
    float originX = biasDist(rng_) * 2.0f;
    float originY = -3.0f; // bottom area

    for (uint32_t i = 0; i < numInstances_; ++i)
    {
        float angle = angleDist(rng_);
        float speed = speedDist(rng_);
        // upward bias so that particles also go up
        float upBias = 6.0f + biasDist(rng_) * 2.0f;
        particles_[i].pos = { originX + biasDist(rng_) * 0.2f, originY + biasDist(rng_) * 0.2f, 0.0f };
        particles_[i].vel = { cosf(angle) * speed * 0.7f, sinf(angle) * speed * 0.7f + upBias, 0.0f };
        particles_[i].life = 2.5f + biasDist(rng_) * 0.8f; // vary lifetime
    }

    // vivid color cycling for firework
    particleColor_ = {1.0f, 0.85f, 0.4f, 1.0f};
    particleScale_ = 0.35f;
    useTexture2_ = false;
    colorCycleActive_ = true; // Enable color cycling for firework particles
}

void ParticleSystem::SpawnSpiral()
{
    // Spiral: particles rotate outward while slowly rising
    const float twoPi = DirectX::XM_2PI;
    std::uniform_real_distribution<float> radiusDist(0.1f, 0.6f);
    for (uint32_t i = 0; i < numInstances_; ++i)
    {
        float t = float(i) / float(numInstances_);
        float angle = twoPi * t * 4.0f; // several turns
        float r = radiusDist(rng_) + t * 1.2f;
        particles_[i].pos = { 0.0f, 0.0f, 0.0f };
        particles_[i].vel = { cosf(angle) * r * 2.5f, sinf(angle) * r * 2.5f + 1.0f * t, 0.0f };
        particles_[i].life = 3.0f + t * 2.0f;
    }
    particleColor_ = {0.4f, 0.8f, 1.0f, 1.0f}; // cyan-ish
    particleScale_ = 0.4f;
    useTexture2_ = false;
    colorCycleActive_ = false;
}

void ParticleSystem::SpawnFountain()
{
    // Fountain: particles shoot upward with gravity, spread moderately
    std::uniform_real_distribution<float> angleDist(-0.5f, 0.5f);
    std::uniform_real_distribution<float> speedDist(6.0f, 10.0f);
    for (uint32_t i = 0; i < numInstances_; ++i)
    {
        float ang = angleDist(rng_);
        float speed = speedDist(rng_);
        particles_[i].pos = { 0.0f, -1.0f, 0.0f };
        particles_[i].vel = { ang * 2.0f, speed, 0.0f };
        particles_[i].life = 2.0f + (float(i) / numInstances_) * 1.5f;
    }
    particleColor_ = {0.8f, 0.9f, 0.3f, 1.0f}; // lime-ish
    particleScale_ = 0.5f;
    useTexture2_ = false;
    colorCycleActive_ = false;
}

void ParticleSystem::SpawnBurst()
{
    // Burst: sudden outward burst with randomly varying speed
    std::uniform_real_distribution<float> angleDist(0.0f, DirectX::XM_2PI);
    std::uniform_real_distribution<float> speedDist(3.0f, 12.0f);
    for (uint32_t i = 0; i < numInstances_; ++i)
    {
        float a = angleDist(rng_);
        float s = speedDist(rng_);
        particles_[i].pos = { 0.0f, 0.0f, 0.0f };
        particles_[i].vel = { cosf(a) * s, sinf(a) * s, 0.0f };
        particles_[i].life = 1.2f + (s / 12.0f) * 1.5f;
    }
    particleColor_ = {1.0f, 0.3f, 0.9f, 1.0f}; // magenta
    particleScale_ = 0.3f;
    useTexture2_ = false;
    colorCycleActive_ = false;
}

void ParticleSystem::SpawnSmoke()
{
    // Smoke: slow upward drift with spread and longer lifetime
    std::uniform_real_distribution<float> spread(-0.5f, 0.5f);
    std::uniform_real_distribution<float> up(0.5f, 1.2f);
    for (uint32_t i = 0; i < numInstances_; ++i)
    {
        particles_[i].pos = { spread(rng_) * 0.5f, -1.0f + spread(rng_) * 0.2f, 0.0f };
        particles_[i].vel = { spread(rng_) * 0.2f, up(rng_) * 0.5f + 0.2f, 0.0f };
        particles_[i].life = 4.0f + (float(i) / numInstances_) * 2.0f;
    }
    particleColor_ = {0.15f, 0.15f, 0.15f, 0.9f}; // grayish, semi-transparent
    particleScale_ = 0.9f;
    useTexture2_ = false;
    colorCycleActive_ = false;
}

void ParticleSystem::SpawnRain()
{
    // Rain: particles come from above downward
    std::uniform_real_distribution<float> xDist(-6.0f, 6.0f);
    std::uniform_real_distribution<float> yDist(4.0f, 8.0f);
    for (uint32_t i = 0; i < numInstances_; ++i)
    {
        particles_[i].pos = { xDist(rng_), yDist(rng_), 0.0f };
        particles_[i].vel = { 0.0f, -6.0f - (float(i) / numInstances_) * 2.0f, 0.0f };
        particles_[i].life = 1.5f + (float(i) / numInstances_) * 0.8f;
    }
    particleColor_ = {0.4f, 0.6f, 1.0f, 1.0f}; // blue
    particleScale_ = 0.25f;
    useTexture2_ = false;
    colorCycleActive_ = false;
}

void ParticleSystem::SpawnRing()
{
    // Ring: particles arranged in a circle moving outward
    const float twoPi = DirectX::XM_2PI;
    std::uniform_real_distribution<float> radiusNoise(0.9f, 1.1f);
    for (uint32_t i = 0; i < numInstances_; ++i)
    {
        float a = twoPi * float(i) / float(numInstances_);
        float r = radiusNoise(rng_);
        particles_[i].pos = { cosf(a) * r * 0.1f, sinf(a) * r * 0.1f, 0.0f };
        particles_[i].vel = { cosf(a) * r * 5.0f, sinf(a) * r * 5.0f, 0.0f };
        particles_[i].life = 2.2f + (float(i) / numInstances_) * 1.0f;
    }
    particleColor_ = {1.0f, 0.6f, 0.2f, 1.0f}; // orange
    particleScale_ = 0.5f;
    useTexture2_ = false;
    colorCycleActive_ = false;
}

void ParticleSystem::SpawnHelix()
{
    // Helix: corkscrew moving upward while rotating around the origin
    const float twoPi = DirectX::XM_2PI;
    for (uint32_t i = 0; i < numInstances_; ++i)
    {
        float t = float(i) / float(numInstances_);
        float angle = twoPi * t * 3.0f; // turns
        float height = t * 4.0f;
        particles_[i].pos = { cosf(angle) * 0.2f, -1.0f + height, sinf(angle) * 0.2f };
        particles_[i].vel = { -sinf(angle) * 2.5f, 2.0f + t * 1.5f, cosf(angle) * 2.5f };
        particles_[i].life = 3.0f + t * 2.0f;
    }
    particleColor_ = {0.9f, 0.4f, 0.7f, 1.0f}; // pink-purple
    particleScale_ = 0.45f;
    useTexture2_ = false;
    colorCycleActive_ = false;
}

void ParticleSystem::SpawnExpanding()
{
    // Expanding: slowly expanding cloud
    std::uniform_real_distribution<float> ang(0.0f, DirectX::XM_2PI);
    std::uniform_real_distribution<float> sp(0.2f, 1.2f);
    for (uint32_t i = 0; i < numInstances_; ++i)
    {
        float a = ang(rng_);
        float s = sp(rng_);
        particles_[i].pos = { cosf(a) * 0.2f * s, sinf(a) * 0.2f * s, 0.0f };
        particles_[i].vel = { cosf(a) * s * 1.5f, sinf(a) * s * 1.5f, 0.0f };
        particles_[i].life = 3.0f + s;
    }
    particleColor_ = {0.6f, 1.0f, 0.6f, 0.9f}; // light green cloud
    particleScale_ = 0.6f;
    useTexture2_ = false;
    colorCycleActive_ = false;
}

void ParticleSystem::Update(float dt, Camera* camera)
{
    for (uint32_t i = 0; i < numInstances_; ++i)
    {
        if (particles_[i].life > 0.0f)
        {
            // apply simple gravity
            particles_[i].vel.y -= 9.8f * dt * 0.5f; // scaled gravity for visual effect
            particles_[i].pos.x += particles_[i].vel.x * dt;
            particles_[i].pos.y += particles_[i].vel.y * dt;
            particles_[i].pos.z += particles_[i].vel.z * dt;
            particles_[i].life -= dt;
        }

        float lifeFactor = particles_[i].life > 0.0f ? std::max(0.0f, particles_[i].life) : 0.0f;
        float scaleFactor = particleScale_ * (0.5f + 0.5f * (lifeFactor / 3.0f));

        Vector3 scale = { scaleFactor, scaleFactor, 1.0f };
        Vector3 rotate = { 0.0f,0.0f,0.0f };
        Vector3 translate = particles_[i].pos;
        Matrix4x4 world = MakeAffineMatrix(scale, rotate, translate);
        Matrix4x4 wvp = Multiply(world, camera->GetViewProjectionMatrix());
        instanceData_[i].World = world;
        instanceData_[i].WVP = wvp;
    }
}

void ParticleSystem::Draw(D3D12_GPU_DESCRIPTOR_HANDLE textureSrvGPU, D3D12_GPU_DESCRIPTOR_HANDLE textureSrvGPU2,
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

    // Update internal material color. If color cycling is active, caller may mutate particleColor_ externally.
    materialDataLocal_->color = { particleColor_.x, particleColor_.y, particleColor_.z, particleColor_.w };
    materialDataLocal_->enableLighting = 0;

    cmd->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
    cmd->SetGraphicsRootConstantBufferView(1, quad_->GetTransformationMatrixResourceSprite()->GetGPUVirtualAddress());

    // Always use the first texture (e.g. White.png) for particles
    D3D12_GPU_DESCRIPTOR_HANDLE chosenTexture = textureSrvGPU;

    cmd->SetGraphicsRootDescriptorTable(2, chosenTexture);
    cmd->SetGraphicsRootConstantBufferView(3, directionalLight->GetGPUVirtualAddress());
    cmd->SetGraphicsRootDescriptorTable(4, instanceSrvHandleGPU);

    uint32_t aliveCount = 0;
    for (uint32_t i = 0; i < numInstances_; ++i)
    {
        if (particles_[i].life > 0.0f) ++aliveCount;
    }

    if (aliveCount == 0) return;

    cmd->DrawIndexedInstanced(6, aliveCount, 0, 0, 0);
}

bool ParticleSystem::HasAliveParticles() const
{
    for (uint32_t i = 0; i < numInstances_; ++i)
    {
        if (particles_[i].life > 0.0f) return true;
    }
    return false;
}

bool ParticleSystem::IsColorCycleActive() const
{
    return colorCycleActive_;
}
