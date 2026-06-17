#include "ParticleEmitter.h"

void ParticleEmitter::Initialize(DirectXCom* dxCommon)
{
	emitterResource_ = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), sizeof(EmitterSphere));
	emitterResource_->Map(0, nullptr, reinterpret_cast<void**>(&emitterSphere_));

	// 初期値設定
	emitterSphere_->count = 10;
	emitterSphere_->frequency = 0.5f;
	emitterSphere_->frequencyTime = 0.0f;
	emitterSphere_->translate = Vector3(0.0f, 0.0f, 0.0f);
	emitterSphere_->radius = 1.0f;
	emitterSphere_->emit = 0;
}

std::list<ParticleManager::Particle> ParticleEmitter::Emit(const Emitter& emitter, std::mt19937& randomEngine, ParticleManager& particleManager)
{
	std::list<ParticleManager::Particle> particles;
	for (uint32_t i = 0; i < emitter.count; ++i)
	{
		particles.push_back(
			particleManager.MakeNewParticles(
				randomEngine,
				emitter.transform.GetTranslate()
			)
		);
	}
	return particles;
}
