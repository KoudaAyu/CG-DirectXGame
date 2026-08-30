#pragma once
#include "Transform.h"
#include <cstdint>
#include <list>
#include <random>
#include "ParticleManager.h"
#include <wrl.h>

struct Emitter
{
	Transform transform;
	uint32_t count;
	float frequency; //発生頻度
	float frequencyTime; //使用頻度
};

struct EmitterSphere
{
	Vector3 translate; //位置
	float radius; //射出範囲
	uint32_t count; //射出数
	float frequency; //発生頻度
	float frequencyTime; //使用頻度
	uint32_t emit; //許可
};

class ParticleEmitter
{
public:
	ParticleEmitter() = default;
	~ParticleEmitter() = default;

	void Initialize(DirectXCom* dxCommon);
	void Update(float deltaTime);

	std::list<ParticleManager::Particle> Emit(const Emitter& emitter, std::mt19937& randomEngine, ParticleManager& particleManager);

	ID3D12Resource* GetResource() const { return emitterResource_.Get(); }
	D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const { return emitterResource_->GetGPUVirtualAddress(); }
	EmitterSphere* GetEmitterData() { return emitterSphere_; }

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> emitterResource_;
	EmitterSphere* emitterSphere_ = nullptr;
};