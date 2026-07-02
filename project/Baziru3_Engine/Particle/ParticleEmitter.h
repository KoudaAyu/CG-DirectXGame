#pragma once
#include"Transform.h"
#include <cstdint>
#include <list>
#include <random>
#include "ParticleManager.h"

struct Emitter
{
	Transform transform;
	uint32_t count;
	float frequency; //発生頻度
	float frequencyTime; //使用頻度
};

class ParticleEmitter
{
public:
	ParticleEmitter() = default;
	~ParticleEmitter() = default;


	std::list<ParticleManager::Particle> Emit(const Emitter& emitter, std::mt19937& randomEngine, ParticleManager& particleManager)
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
};