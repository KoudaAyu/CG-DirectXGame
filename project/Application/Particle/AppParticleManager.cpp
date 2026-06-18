#include "AppParticleManager.h"
#include <algorithm>
#include <cmath>

void AppParticleManager::Initialize(ParticleManager* enginePM)
{
	enginePM_ = enginePM;
	particles_.clear();
}

void AppParticleManager::Update(float deltaTime)
{
	if (!enginePM_) return;

	std::list<ParticleManager::Particle> tempParticles;

	auto it = particles_.begin();
	while (it != particles_.end())
	{
		it->currentTime += deltaTime;
		if (it->currentTime >= it->lifeTime)
		{
			it = particles_.erase(it);
			continue;
		}

		// Physics update (gravity)
		it->velocity.y -= it->gravity * deltaTime;

		Vector3 pos = it->transform.GetTranslate();
		pos += it->velocity * deltaTime;

		// Ground bounce (y = 0.0f)
		float groundY = 0.0f;
		if (pos.y < groundY && it->velocity.y < 0.0f)
		{
			pos.y = groundY;
			it->velocity.y = -it->velocity.y * it->bounceElasticity;
			it->velocity.x *= 0.7f;
			it->velocity.z *= 0.7f;
		}
		it->transform.SetTranslate(pos);

		// Angular spin rotation on Z axis
		Vector3 rot = it->transform.GetRotate();
		rot.z += it->angularVelocity * deltaTime;
		it->transform.SetRotate(rot);

		// Construct standard ParticleManager::Particle
		ParticleManager::Particle p;
		p.transform = it->transform;
		p.velocity = { 0.0f, 0.0f, 0.0f }; // Standard PM update doesn't move it

		// Set lifetime and current time to achieve target alpha using formula:
		// alpha = 1.0f - (currentTime / lifeTime)
		float targetAlpha = 1.0f - (it->currentTime / it->lifeTime);
		targetAlpha = std::clamp(targetAlpha, 0.0f, 1.0f);

		// To prevent duplicate trails, we set lifeTime and currentTime such that 
		// it gets drawn this frame and gets deleted exactly in the next frame.
		// We must use the engine's constant timestep (1/60s) for this mapping because 
		// the engine's ParticleManager is always updated with 1/60s, regardless of slow-mo.
		float engineDeltaTime = 1.0f / 60.0f;
		float epsilon = 0.001f;
		float tAlpha = std::clamp(targetAlpha, epsilon, 1.0f - epsilon);
		p.lifeTime = engineDeltaTime / tAlpha;
		p.currentTime = p.lifeTime * (1.0f - tAlpha) - engineDeltaTime;

		p.color = it->color;
		p.textureIndex = it->textureIndex;

		tempParticles.push_back(p);
		++it;
	}

	if (!tempParticles.empty())
	{
		enginePM_->AddParticles(tempParticles);
	}
}

void AppParticleManager::EmitSpark(std::mt19937& randomEngine, const Vector3& position, const Vector3& baseVelocity, const Vector4& color, float scale, float lifeTime, uint32_t textureIndex)
{
	AppParticle p;
	p.transform.Initialize();
	p.transform.SetTranslate(position);

	// Z rotation: 0.0f to 6.2831853f
	std::uniform_real_distribution<float> distRotate(0.0f, 6.2831853f);
	p.transform.SetRotate({ 0.0f, 0.0f, distRotate(randomEngine) });

	// Non-uniform scale: X: 0.02f to 0.06f, Y: 0.15f to 0.45f, adjusted by scale factor
	std::uniform_real_distribution<float> distScaleX(0.02f, 0.06f);
	std::uniform_real_distribution<float> distScaleY(0.15f, 0.45f);
	p.transform.SetScale({ distScaleX(randomEngine) * (scale / 0.12f), distScaleY(randomEngine) * (scale / 0.12f), 1.0f });

	std::uniform_real_distribution<float> velXZ(-4.0f, 4.0f);
	std::uniform_real_distribution<float> velY(2.0f, 6.0f);

	p.velocity = baseVelocity + Vector3{ velXZ(randomEngine), velY(randomEngine), velXZ(randomEngine) };
	p.color = color;
	p.lifeTime = lifeTime;
	p.currentTime = 0.0f;
	p.textureIndex = textureIndex;

	p.gravity = 9.8f; // Heavy gravity for sparks
	p.bounceElasticity = 0.5f;

	std::uniform_real_distribution<float> spinDist(-10.0f, 10.0f);
	p.angularVelocity = spinDist(randomEngine);

	particles_.push_back(p);
}

void AppParticleManager::EmitSparkWithVelocity(std::mt19937& randomEngine, const Vector3& position, const Vector3& velocity, const Vector4& color, float scale, float lifeTime, uint32_t textureIndex)
{
	AppParticle p;
	p.transform.Initialize();
	p.transform.SetTranslate(position);

	std::uniform_real_distribution<float> distRotate(0.0f, 6.2831853f);
	p.transform.SetRotate({ 0.0f, 0.0f, distRotate(randomEngine) });

	std::uniform_real_distribution<float> distScaleX(0.02f, 0.06f);
	std::uniform_real_distribution<float> distScaleY(0.15f, 0.45f);
	p.transform.SetScale({ distScaleX(randomEngine) * (scale / 0.12f), distScaleY(randomEngine) * (scale / 0.12f), 1.0f });

	p.velocity = velocity;
	p.color = color;
	p.lifeTime = lifeTime;
	p.currentTime = 0.0f;
	p.textureIndex = textureIndex;

	p.gravity = 9.8f;
	p.bounceElasticity = 0.5f;

	std::uniform_real_distribution<float> spinDist(-10.0f, 10.0f);
	p.angularVelocity = spinDist(randomEngine);

	particles_.push_back(p);
}

void AppParticleManager::EmitDust(std::mt19937& randomEngine, const Vector3& position, float scale, const Vector4& color, uint32_t textureIndex)
{
	AppParticle p;
	p.transform.Initialize();

	// Tiny spawn offset
	std::uniform_real_distribution<float> distOffset(-0.15f, 0.15f);
	p.transform.SetTranslate({
		position.x + distOffset(randomEngine),
		position.y + distOffset(randomEngine),
		position.z + distOffset(randomEngine)
	});

	std::uniform_real_distribution<float> distRotate(0.0f, 6.2831853f);
	p.transform.SetRotate({ 0.0f, 0.0f, distRotate(randomEngine) });

	// Scale: 0.12f to 0.28f adjusted by scale factor
	std::uniform_real_distribution<float> distScale(0.12f, 0.28f);
	float s = distScale(randomEngine) * scale;
	p.transform.SetScale({ s, s, 1.0f });

	std::uniform_real_distribution<float> velXZ(-0.4f, 0.4f);
	std::uniform_real_distribution<float> velY(0.1f, 0.3f);
	p.velocity = { velXZ(randomEngine), velY(randomEngine), velXZ(randomEngine) };
	p.color = color;

	std::uniform_real_distribution<float> lifeDist(0.3f, 0.55f);
	p.lifeTime = lifeDist(randomEngine);
	p.currentTime = 0.0f;
	p.textureIndex = textureIndex;

	p.gravity = 0.0f;
	p.bounceElasticity = 0.0f;
	p.angularVelocity = 0.0f;

	particles_.push_back(p);
}

void AppParticleManager::EmitDustWithVelocity(std::mt19937& randomEngine, const Vector3& position, float scale, const Vector4& color, const Vector3& velocity, float lifeTime, uint32_t textureIndex)
{
	AppParticle p;
	p.transform.Initialize();
	p.transform.SetTranslate(position);

	std::uniform_real_distribution<float> distRotate(0.0f, 6.2831853f);
	p.transform.SetRotate({ 0.0f, 0.0f, distRotate(randomEngine) });

	// Scale
	std::uniform_real_distribution<float> distScale(0.12f, 0.28f);
	float s = distScale(randomEngine) * scale;
	p.transform.SetScale({ s, s, 1.0f });

	p.velocity = velocity;
	p.color = color;
	p.lifeTime = lifeTime;
	p.currentTime = 0.0f;
	p.textureIndex = textureIndex;

	p.gravity = 0.0f;
	p.bounceElasticity = 0.0f;
	p.angularVelocity = 0.0f;

	particles_.push_back(p);
}

void AppParticleManager::EmitShellCasing(std::mt19937& randomEngine, const Vector3& position, const Vector3& forward, const Vector4& color, const Vector3& scale, uint32_t textureIndex)
{
	AppParticle p;
	p.transform.Initialize();

	// Initial rotation
	std::uniform_real_distribution<float> distRotate(0.0f, 6.2831853f);
	p.transform.SetRotate({ 0.0f, 0.0f, distRotate(randomEngine) });

	// Set scale (normally non-uniform but we respect the passed parameter)
	p.transform.SetScale(scale);

	// Offset relative to forward direction
	Vector3 right = { forward.z, 0.0f, -forward.x };
	Vector3 spawnPos = position + right * 0.15f;
	spawnPos.y += 0.1f;
	p.transform.SetTranslate(spawnPos);

	// Eject velocity
	std::uniform_real_distribution<float> forceRight(1.8f, 3.2f);
	std::uniform_real_distribution<float> forceUp(1.5f, 3.0f);
	std::uniform_real_distribution<float> forceBack(-1.2f, -0.4f);

	float fr = forceRight(randomEngine);
	float fu = forceUp(randomEngine);
	float fb = forceBack(randomEngine);

	p.velocity = right * fr + Vector3{ 0.0f, fu, 0.0f } + forward * fb;
	p.color = color;
	p.lifeTime = 2.0f;
	p.currentTime = 0.0f;
	p.textureIndex = textureIndex;

	p.gravity = 9.8f;
	p.bounceElasticity = 0.4f;

	std::uniform_real_distribution<float> spinDist(15.0f, 30.0f);
	p.angularVelocity = spinDist(randomEngine);

	particles_.push_back(p);
}

void AppParticleManager::EmitFeather(std::mt19937& randomEngine, const Vector3& position, const Vector4& color, uint32_t textureIndex)
{
	AppParticle p;
	p.transform.Initialize();

	// Z rotation: 0.0f to 6.2831853f
	std::uniform_real_distribution<float> distRotate(0.0f, 6.2831853f);
	p.transform.SetRotate({ 0.0f, 0.0f, distRotate(randomEngine) });

	// Non-uniform scale: X: 0.05f to 0.12f, Y: 0.15f to 0.35f
	std::uniform_real_distribution<float> distScaleX(0.05f, 0.12f);
	std::uniform_real_distribution<float> distScaleY(0.15f, 0.35f);
	p.transform.SetScale({ distScaleX(randomEngine), distScaleY(randomEngine), 1.0f });

	// Tiny spawn offset
	std::uniform_real_distribution<float> distOffset(-0.2f, 0.2f);
	p.transform.SetTranslate({
		position.x + distOffset(randomEngine),
		position.y + distOffset(randomEngine),
		position.z + distOffset(randomEngine)
	});

	// Velocities
	std::uniform_real_distribution<float> velXZ(-2.0f, 2.0f);
	std::uniform_real_distribution<float> velY(1.5f, 4.0f);
	p.velocity = { velXZ(randomEngine), velY(randomEngine), velXZ(randomEngine) };
	p.color = color;

	std::uniform_real_distribution<float> lifeDist(1.2f, 2.2f);
	p.lifeTime = lifeDist(randomEngine);
	p.currentTime = 0.0f;
	p.textureIndex = textureIndex;

	p.gravity = 3.5f; // Gentle drift down
	p.bounceElasticity = 0.3f;

	std::uniform_real_distribution<float> spinDist(-5.0f, 5.0f);
	p.angularVelocity = spinDist(randomEngine);

	particles_.push_back(p);
}

void AppParticleManager::EmitMuzzleFlash(std::mt19937& randomEngine, const Vector3& position, const Vector3& direction, const Vector3& right, const Vector3& up, const Vector4& color, float speedMultiplier, uint32_t textureIndex)
{
	AppParticle p;
	p.transform.Initialize();
	p.transform.SetTranslate(position);

	std::uniform_real_distribution<float> distRotate(0.0f, 6.2831853f);
	p.transform.SetRotate({ 0.0f, 0.0f, distRotate(randomEngine) });

	// Uniform scale 0.3f
	p.transform.SetScale({ 0.3f, 0.3f, 1.0f });

	float forwardSpeed = (4.0f + (static_cast<float>(rand()) / RAND_MAX) * 4.0f) * speedMultiplier;
	float rightSpeed = (static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f) * 1.8f * speedMultiplier;
	float upSpeed = (static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f) * 1.2f * speedMultiplier;

	p.velocity = direction * forwardSpeed + right * rightSpeed + up * upSpeed;
	p.color = color;

	std::uniform_real_distribution<float> distTime(0.05f, 0.15f);
	p.lifeTime = distTime(randomEngine);
	p.currentTime = 0.0f;
	p.textureIndex = textureIndex;

	p.gravity = 0.0f;
	p.bounceElasticity = 0.0f;
	p.angularVelocity = 0.0f;

	particles_.push_back(p);
}
