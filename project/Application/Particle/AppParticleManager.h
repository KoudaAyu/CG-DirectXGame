#pragma once
#include <list>
#include <random>
#include "Vector.h"
#include "Transform.h"
#include "ParticleManager.h"

struct AppParticle
{
	Transform transform;
	Vector3 velocity;
	Vector4 color;
	float lifeTime;
	float currentTime;
	uint32_t textureIndex;

	// Physics properties
	float gravity = 0.0f;
	float bounceElasticity = 0.0f;
	float angularVelocity = 0.0f;
};

class AppParticleManager
{
public:
	AppParticleManager() = default;
	~AppParticleManager() = default;

	void Initialize(ParticleManager* enginePM);
	void Update(float deltaTime);

	// Custom particle emission methods
	void EmitSpark(std::mt19937& randomEngine, const Vector3& position, const Vector3& baseVelocity, const Vector4& color, float scale, float lifeTime, uint32_t textureIndex);
	void EmitSparkWithVelocity(std::mt19937& randomEngine, const Vector3& position, const Vector3& velocity, const Vector4& color, float scale, float lifeTime, uint32_t textureIndex);
	void EmitDust(std::mt19937& randomEngine, const Vector3& position, float scale, const Vector4& color, uint32_t textureIndex);
	void EmitDustWithVelocity(std::mt19937& randomEngine, const Vector3& position, float scale, const Vector4& color, const Vector3& velocity, float lifeTime, uint32_t textureIndex);
	void EmitShellCasing(std::mt19937& randomEngine, const Vector3& position, const Vector3& forward, const Vector4& color, const Vector3& scale, uint32_t textureIndex);
	void EmitFeather(std::mt19937& randomEngine, const Vector3& position, const Vector4& color, uint32_t textureIndex);
	void EmitMuzzleFlash(std::mt19937& randomEngine, const Vector3& position, const Vector3& direction, const Vector3& right, const Vector3& up, const Vector4& color, float speedMultiplier, uint32_t textureIndex);

private:
	ParticleManager* enginePM_ = nullptr;
	std::list<AppParticle> particles_;
};
