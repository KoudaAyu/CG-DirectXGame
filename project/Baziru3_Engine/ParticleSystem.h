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
	void Update(float dt, Camera* camera);
	void Draw(D3D12_GPU_DESCRIPTOR_HANDLE textureSrvGPU, D3D12_GPU_DESCRIPTOR_HANDLE textureSrvGPU2,
		Microsoft::WRL::ComPtr<ID3D12Resource> materialResource,
		Microsoft::WRL::ComPtr<ID3D12Resource> directionalLight,
		D3D12_GPU_DESCRIPTOR_HANDLE instanceSrvHandleGPU,
		bool useMonsterBall);

	// Added getters so caller can create SRV for the instance buffer
	const Microsoft::WRL::ComPtr<ID3D12Resource>& GetInstanceResource() const { return instanceResource_; }
	uint32_t GetNumInstances() const { return numInstances_; }
	bool HasAliveParticles() const;
	// Whether the current live particles came from the firework spawn (key 2)
	bool IsColorCycleActive() const;

private:
	struct Particle
	{
		Vector3 pos;
		Vector3 vel;
		float life;
	};

	uint32_t numInstances_ = 8;
	Particle* particles_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> instanceResource_ = nullptr;
	TransformationMatrix* instanceData_ = nullptr;
	DirectXCom* dxCommon_ = nullptr;
	Sprite* quad_ = nullptr;

	// RNG for varied firework behavior
	std::mt19937 rng_;

	// Whether to animate particle color (set when SpawnFirework called)
	bool colorCycleActive_ = false;
};

// Inline implementations
inline ParticleSystem::ParticleSystem()
{
}

inline ParticleSystem::~ParticleSystem()
{
	if (particles_)
	{
		delete[] particles_;
	}
	// instanceResource_ will be released by ComPtr
}

inline void ParticleSystem::Initialize(DirectXCom* dxCommon, Sprite* quadSprite, uint32_t numInstances)
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
}

inline void ParticleSystem::Spawn8()
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
}

inline void ParticleSystem::SpawnFirework()
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

	colorCycleActive_ = true; // Enable color cycling for firework particles
}

inline void ParticleSystem::Update(float dt, Camera* camera)
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

		Vector3 scale = { 0.5f,0.5f,1.0f };
		Vector3 rotate = { 0.0f,0.0f,0.0f };
		Vector3 translate = particles_[i].pos;
		Matrix4x4 world = MakeAffineMatrix(scale, rotate, translate);
		Matrix4x4 wvp = Multiply(world, camera->GetViewProjectionMatrix());
		instanceData_[i].World = world;
		instanceData_[i].WVP = wvp;
	}
}

inline void ParticleSystem::Draw(D3D12_GPU_DESCRIPTOR_HANDLE textureSrvGPU, D3D12_GPU_DESCRIPTOR_HANDLE textureSrvGPU2,
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource,
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

	cmd->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
	cmd->SetGraphicsRootConstantBufferView(1, quad_->GetTransformationMatrixResourceSprite()->GetGPUVirtualAddress());
	cmd->SetGraphicsRootDescriptorTable(2, useMonsterBall ? textureSrvGPU2 : textureSrvGPU);
	cmd->SetGraphicsRootConstantBufferView(3, directionalLight->GetGPUVirtualAddress());
	cmd->SetGraphicsRootDescriptorTable(4, instanceSrvHandleGPU);

	// Count alive particles so we only draw those instances
	uint32_t aliveCount = 0;
	for (uint32_t i = 0; i < numInstances_; ++i)
	{
		if (particles_[i].life > 0.0f) ++aliveCount;
	}

	if (aliveCount == 0) return; // nothing to draw

	cmd->DrawIndexedInstanced(6, aliveCount, 0, 0, 0);
}

inline bool ParticleSystem::HasAliveParticles() const
{
	for (uint32_t i = 0; i < numInstances_; ++i)
	{
		if (particles_[i].life > 0.0f) return true;
	}
	return false;
}

inline bool ParticleSystem::IsColorCycleActive() const
{
	return colorCycleActive_;
}
