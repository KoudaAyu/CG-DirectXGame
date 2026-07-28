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

struct GPUEmitterData
{
	Vector3 translate = { 0.0f, 0.0f, 0.0f };    // 位置
	float radius = 0.5f;                         // 発生半径
	uint32_t count = 8;                          // 射出数
	float frequency = 0.016f;                    // 発生頻度
	float frequencyTime = 0.0f;                  // 使用頻度
	uint32_t emit = 1;                           // 射出許可
	uint32_t emitterType = 0;                    // 0: Point, 1: Box, 2: Sphere, 3: Cone
	float initialSpeed = 1.0f;                   // 初速
	Vector3 boxSize = { 1.0f, 1.0f, 1.0f };       // Box領域サイズ
	float coneAngle = 0.523598f;                 // Cone照射角度
	Vector3 direction = { 0.0f, 1.0f, 0.0f };    // Direction / Cone向き
	float particleLifeTime = 1.0f;               // パーティクル寿命
	Vector4 particleColor = { 1.0f, 0.6f, 0.15f, 1.0f }; // パーティクルカラー
};
typedef GPUEmitterData EmitterSphere;

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