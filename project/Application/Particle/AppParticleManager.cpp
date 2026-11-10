#include "AppParticleManager.h"
#include "AppParticlePresets.h"
#include "ParticleEmitter.h"
#include "DirectXCom.h"
#include "RootParam.h"
#include "TextureManager.h"
#include "Light.h"
#include "Camera.h"
#include "Application/Config/GameConfig.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace
{
	using namespace AppParticlePresets;

	constexpr float kPi    = GameConfig::Math::kPi;
	constexpr float kTwoPi = GameConfig::Math::kTwoPi;

	constexpr std::array<uint32_t, 3> kLayerStartSlots = {
		0u, 682u, 1364u
	};
	constexpr std::array<uint32_t, 3> kLayerEndSlots = {
		682u, 1364u, 2048u
	};
	constexpr uint16_t kMaxSlotLayerIndex = 2;
	constexpr size_t   kTranslationZCol   = 2;

	constexpr bool IsEven(int i) { return (i & 1) == 0; }

	float RandRange(std::mt19937& rng, float minVal, float maxVal)
	{
		return std::uniform_real_distribution<float>(minVal, maxVal)(rng);
	}

	float RandRange(std::mt19937& rng, const RangeF& r)
	{
		return std::uniform_real_distribution<float>(r.min, r.max)(rng);
	}

	float RandAngle(std::mt19937& rng)
	{
		return std::uniform_real_distribution<float>(0.0f, kTwoPi)(rng);
	}

	Vector3 RandOffset3D(std::mt19937& rng, float halfExtent)
	{
		std::uniform_real_distribution<float> d(-halfExtent, halfExtent);
		const float ox = d(rng);
		const float oy = d(rng);
		const float oz = d(rng);
		return { ox, oy, oz };
	}

	float CRand01()
	{
		return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
	}

	float CRandRange(const RangeF& r)
	{
		return r.min + CRand01() * (r.max - r.min);
	}

	ColorShiftSpec SolidColor(const Vector4& col)
	{
		return { col, { 0.0f, 0.0f, 0.0f, 0.0f }, false };
	}

	AppParticle BuildParticle(
		const Vector3& pos,
		float rotZ,
		const Vector3& quadScale,
		const Vector3& velocity,
		float lifeTime,
		uint32_t textureIndex,
		const ColorShiftSpec& colorSpec,
		const DynamicsSpec& dyn = {},
		float angularVelocity = 0.0f,
		float twinklePhase = 0.0f)
	{
		AppParticle p{};
		p.transform.Initialize();
		p.transform.SetTranslate(pos);
		p.transform.SetRotate({ 0.0f, 0.0f, rotZ });
		p.transform.SetScale(quadScale);
		p.velocity = velocity;
		p.color = colorSpec.color;
		p.endColor = colorSpec.endColor;
		p.hasColorShift = colorSpec.hasColorShift;
		p.lifeTime = lifeTime;
		p.currentTime = 0.0f;
		p.textureIndex = textureIndex;
		p.gravity = dyn.gravity;
		p.drag = dyn.drag;
		p.bounceElasticity = dyn.bounceElasticity;
		p.angularVelocity = angularVelocity;
		p.curlFreq = dyn.curlFreq;
		p.curlAmp = dyn.curlAmp;
		p.fadeInRatio = dyn.fadeInRatio;
		p.twinkleSpeed = dyn.twinkleSpeed;
		p.twinklePhase = twinklePhase;
		p.slotLayer = dyn.slotLayer;
		return p;
	}

	AppParticle BuildScaledParticle(
		const Vector3& pos,
		float rotZ,
		float baseScale,
		const ScaleSpec& scaleSpec,
		const Vector3& velocity,
		float lifeTime,
		uint32_t textureIndex,
		const ColorShiftSpec& colorSpec,
		const DynamicsSpec& dyn = {},
		float angularVelocity = 0.0f,
		float twinklePhase = 0.0f)
	{
		AppParticle p = BuildParticle(
			pos, rotZ,
			{ baseScale * scaleSpec.aspect.x, baseScale * scaleSpec.aspect.y, 1.0f },
			velocity, lifeTime, textureIndex, colorSpec, dyn, angularVelocity, twinklePhase);
		p.scaleAspect = scaleSpec.aspect;
		if (scaleSpec.initScaleMul > 0.0f)
		{
			p.initialScale = baseScale * scaleSpec.initScaleMul;
			p.targetScale = baseScale * scaleSpec.targetScaleMul;
		}
		return p;
	}

	AppParticle BuildJitteredParticle(
		std::mt19937& rng,
		const Vector3& position,
		const Vector3& baseVel,
		float uniformScaleMul,
		const Vector4& color,
		uint32_t textureIndex,
		const JitterEmitPreset& preset)
	{
		const Vector3 pos = position + RandOffset3D(rng, preset.posJitter);
		const float rotZ = RandAngle(rng);
		Vector3 quadScale = { 1.0f, 1.0f, 1.0f };
		if (preset.uniformScale)
		{
			const float s = RandRange(rng, preset.scaleX) * uniformScaleMul;
			quadScale = { s, s, 1.0f };
		}
		else
		{
			const float sx = RandRange(rng, preset.scaleX);
			const float sy = RandRange(rng, preset.scaleY);
			quadScale = { sx, sy, 1.0f };
		}
		const float vx = RandRange(rng, preset.velXZ);
		const float vy = RandRange(rng, preset.velY);
		const float vz = RandRange(rng, preset.velXZ);
		const float life = RandRange(rng, preset.life);
		const float spin = (preset.spin.min == preset.spin.max) ? preset.spin.min : RandRange(rng, preset.spin);
		return BuildParticle(pos, rotZ, quadScale, baseVel + Vector3{ vx, vy, vz }, life, textureIndex, SolidColor(color), preset.dynamics, spin);
	}
}

void AppParticleManager::Initialize(ParticleManager* enginePM)
{
	enginePM_ = enginePM;
	particles_.clear();

	if (enginePM_ && enginePM_->GetDxCommon())
	{
		DirectXCom* dxCommon = enginePM_->GetDxCommon();

		constexpr Vertex kQuadVertices[Core::kQuadVertexCount] = {
			{ { -0.5f, -0.5f, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } },
			{ { -0.5f,  0.5f, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
			{ {  0.5f, -0.5f, 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } },
			{ { -0.5f,  0.5f, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
			{ {  0.5f,  0.5f, 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
			{ {  0.5f, -0.5f, 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } },
		};

		quadVertexBuffer_ = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), sizeof(kQuadVertices));
		void* mapped = nullptr;
		quadVertexBuffer_->Map(0, nullptr, &mapped);
		std::memcpy(mapped, kQuadVertices, sizeof(kQuadVertices));
		quadVertexBuffer_->Unmap(0, nullptr);

		quadVertexBufferView_.BufferLocation = quadVertexBuffer_->GetGPUVirtualAddress();
		quadVertexBufferView_.SizeInBytes = sizeof(kQuadVertices);
		quadVertexBufferView_.StrideInBytes = sizeof(Vertex);

		perViewResource_ = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), sizeof(ParticleManager::PerView));
		perViewResource_->Map(0, nullptr, reinterpret_cast<void**>(&perViewData_));
		std::memset(perViewData_, 0, sizeof(ParticleManager::PerView));

		struct DefaultMatCB
		{
			Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
			int32_t enableLighting = 0;
			float padding[Core::kMatPaddingFloats]{};
			Matrix4x4 uvTransform = MakeIdentity4x4();
		};
		defaultMaterialResource_ = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), sizeof(DefaultMatCB));
		void* matPtr = nullptr;
		defaultMaterialResource_->Map(0, nullptr, &matPtr);
		DefaultMatCB dMat{};
		std::memcpy(matPtr, &dMat, sizeof(DefaultMatCB));
		defaultMaterialResource_->Unmap(0, nullptr);
	}
}

AppParticleManager::~AppParticleManager()
{
	if (perViewResource_ && perViewData_)
	{
		perViewResource_->Unmap(0, nullptr);
		perViewData_ = nullptr;
	}

	auto* srvManager = TextureManager::GetInstance() ? TextureManager::GetInstance()->GetSRVManager() : nullptr;
	for (uint32_t b = 0; b < kMaxTextureBatches; ++b)
	{
		TextureBatch& batch = textureBatches_[b];
		if (!batch.allocated) continue;

		if (batch.resource && batch.mappedData)
		{
			D3D12_RANGE writtenRange = { 0, static_cast<SIZE_T>(sizeof(ParticleManager::ParticleCS) * kBatchMaxInstances) };
			batch.resource->Unmap(0, &writtenRange);
			batch.mappedData = nullptr;
		}
		if (batch.srvIndex != 0 && srvManager)
		{
			srvManager->Free(batch.srvIndex);
			batch.srvIndex = 0;
		}
		batch.allocated = false;
	}
}

int16_t AppParticleManager::GetOrCreateTextureBatch(uint32_t textureIndex)
{
	for (uint32_t b = 0; b < kMaxTextureBatches; ++b)
	{
		if (textureBatches_[b].allocated && textureBatches_[b].textureIndex == textureIndex)
		{
			return static_cast<int16_t>(b);
		}
	}

	if (!enginePM_ || !enginePM_->GetDxCommon()) return -1;
	DirectXCom* dxCommon = enginePM_->GetDxCommon();
	auto* srvManager = TextureManager::GetInstance() ? TextureManager::GetInstance()->GetSRVManager() : nullptr;
	if (!srvManager) return -1;

	for (uint32_t b = 0; b < kMaxTextureBatches; ++b)
	{
		TextureBatch& batch = textureBatches_[b];
		if (!batch.allocated)
		{
			batch.resource = dxCommon->CreateBufferResource(
				dxCommon->GetDevice().Get(),
				sizeof(ParticleManager::ParticleCS) * kBatchMaxInstances);
			batch.resource->Map(0, nullptr, reinterpret_cast<void**>(&batch.mappedData));
			std::memset(batch.mappedData, 0, sizeof(ParticleManager::ParticleCS) * kBatchMaxInstances);

			batch.srvIndex = srvManager->Allocate();
			srvManager->CreateSRVForStructuredBuffer(
				batch.srvIndex, batch.resource.Get(), kBatchMaxInstances, sizeof(ParticleManager::ParticleCS));
			batch.srvHandleGPU = srvManager->GetGPUDescriptorHandle(batch.srvIndex);

			batch.slotOccupied.assign(kBatchMaxInstances, 0);
			for (uint32_t l = 0; l < kSlotLayerCount; ++l)
			{
				batch.layerSlotCursor[l] = kLayerStartSlots[l];
			}
			batch.textureIndex = textureIndex;
			batch.allocated = true;
			return static_cast<int16_t>(b);
		}
	}

	return 0;
}

void AppParticleManager::AllocateParticleSlot(AppParticle& p)
{
	if (p.batchIndex >= 0 && p.batchIndex < static_cast<int16_t>(kMaxTextureBatches) &&
		p.stableSlot >= 0 && p.stableSlot < static_cast<int32_t>(kBatchMaxInstances))
	{
		if (textureBatches_[p.batchIndex].allocated &&
			textureBatches_[p.batchIndex].textureIndex == p.textureIndex)
		{
			return;
		}
		ReleaseParticleSlot(p);
	}

	const int16_t bIdx = GetOrCreateTextureBatch(p.textureIndex);
	if (bIdx < 0) return;

	TextureBatch& batch = textureBatches_[bIdx];
	if (batch.slotOccupied.size() < kBatchMaxInstances)
	{
		batch.slotOccupied.assign(kBatchMaxInstances, 0);
	}

	const uint16_t clampedLayer = (p.slotLayer >= kMaxSlotLayerIndex) ? kMaxSlotLayerIndex : p.slotLayer;
	const uint32_t rangeBegin = kLayerStartSlots[clampedLayer];
	const uint32_t rangeEnd   = kLayerEndSlots[clampedLayer];
	const uint32_t rangeCount = rangeEnd - rangeBegin;

	uint32_t cursor = batch.layerSlotCursor[clampedLayer];
	if (cursor < rangeBegin || cursor >= rangeEnd) cursor = rangeBegin;

	for (uint32_t i = 0; i < rangeCount; ++i)
	{
		const uint32_t s = rangeBegin + ((cursor - rangeBegin + i) % rangeCount);
		if (batch.slotOccupied[s] == 0)
		{
			batch.slotOccupied[s] = 1;
			batch.layerSlotCursor[clampedLayer] = rangeBegin + ((s - rangeBegin + 1) % rangeCount);
			p.batchIndex = bIdx;
			p.stableSlot = static_cast<int32_t>(s);
			return;
		}
	}

	for (uint32_t s = 0; s < kBatchMaxInstances; ++s)
	{
		if (batch.slotOccupied[s] == 0)
		{
			batch.slotOccupied[s] = 1;
			p.batchIndex = bIdx;
			p.stableSlot = static_cast<int32_t>(s);
			return;
		}
	}

	p.batchIndex = -1;
	p.stableSlot = -1;
}

void AppParticleManager::ReleaseParticleSlot(AppParticle& p)
{
	if (p.batchIndex >= 0 && p.batchIndex < static_cast<int16_t>(kMaxTextureBatches))
	{
		TextureBatch& batch = textureBatches_[p.batchIndex];
		if (batch.allocated && p.stableSlot >= 0 && static_cast<size_t>(p.stableSlot) < batch.slotOccupied.size())
		{
			batch.slotOccupied[static_cast<size_t>(p.stableSlot)] = 0;
		}
	}
	p.batchIndex = -1;
	p.stableSlot = -1;
}

void AppParticleManager::TriggerGPUEmitter(const Vector3& position, float scale, uint32_t count)
{
	if (enginePM_ && enginePM_->GetGPUEmitter() && enginePM_->GetGPUEmitter()->GetEmitterData())
	{
		auto* emitter = enginePM_->GetGPUEmitter()->GetEmitterData();
		emitter->translate = position;
		emitter->radius = (scale > Ballistics::kGpuEmitMinScaleThreshold) ? scale : Ballistics::kGpuEmitFallbackRadius;
		emitter->count = count;
		emitter->emit = 1;
	}
}

void AppParticleManager::AddParticle(const AppParticle& particle)
{
	AppParticle copy = particle;
	AllocateParticleSlot(copy);
	particles_.push_back(copy);
}

void AppParticleManager::ClearParticles()
{
	particles_.clear();
	for (uint32_t b = 0; b < kMaxTextureBatches; ++b)
	{
		TextureBatch& batch = textureBatches_[b];
		if (!batch.allocated) continue;
		if (!batch.slotOccupied.empty())
		{
			std::fill(batch.slotOccupied.begin(), batch.slotOccupied.end(), static_cast<uint8_t>(0));
		}
		for (uint32_t l = 0; l < kSlotLayerCount; ++l)
		{
			batch.layerSlotCursor[l] = kLayerStartSlots[l];
		}
	}
}

void AppParticleManager::Update(float deltaTime, const Vector3& playerPos)
{
	if (!enginePM_) return;

	// パーティクル数が上限を超えた場合、古い順に即時回収して処理落ちを完全防止
	while (particles_.size() > Core::kMaxActiveParticles)
	{
		ReleaseParticleSlot(particles_.front());
		particles_.pop_front();
	}

	auto it = particles_.begin();
	while (it != particles_.end())
	{
		it->currentTime += deltaTime;
		if (it->currentTime >= it->lifeTime)
		{
			ReleaseParticleSlot(*it);
			it = particles_.erase(it);
			continue;
		}

		if (it->stableSlot < 0)
		{
			AllocateParticleSlot(*it);
		}

		// 空気抵抗（Drag）減速
		if (it->drag > 0.0f)
		{
			const float decay = (std::max)(0.0f, 1.0f - it->drag * deltaTime);
			it->velocity = it->velocity * decay;
		}

		Vector3 pos;
		if (it->followPlayer)
		{
			it->offsetFromPlayer.y -= it->gravity * deltaTime;
			it->offsetFromPlayer += it->velocity * deltaTime;
			pos = playerPos + it->offsetFromPlayer;
		}
		else
		{
			it->velocity.y -= it->gravity * deltaTime;
			pos = it->transform.GetTranslate() + it->velocity * deltaTime;

			if (pos.y < groundY_ && it->velocity.y < 0.0f && it->bounceElasticity > 0.0f)
			{
				pos.y = groundY_;
				it->velocity.y = -it->velocity.y * it->bounceElasticity;
				it->velocity.x *= Core::kBounceHorizDamping;
				it->velocity.z *= Core::kBounceHorizDamping;
			}
		}

		// カール乱流（Fluid Curl Turbulence）
		if (it->curlAmp > 0.0f && it->curlFreq > 0.0f)
		{
			const float waveY = std::sin(pos.y * it->curlFreq + it->currentTime * Core::kCurlTimeFreqY);
			const float waveX = std::cos(pos.x * it->curlFreq + it->currentTime * Core::kCurlTimeFreqX);
			pos.x += waveY * it->curlAmp * deltaTime;
			pos.z += waveX * it->curlAmp * deltaTime;
		}

		// 有機的ゆらぎ（Sinusoidal Flutter / Wobble）
		if (it->wobbleFreq > 0.0f && it->wobbleAmp > 0.0f)
		{
			const float sine = std::sin(it->currentTime * it->wobbleFreq);
			pos += it->wobbleAxis * (sine * it->wobbleAmp * deltaTime);
		}
		it->transform.SetTranslate(pos);

		// スケール補間（非一様アスペクト比 scaleAspect を維持しながら膨張・収縮）
		if (it->initialScale > 0.0f && it->targetScale > 0.0f && it->lifeTime > 0.0f)
		{
			const float t = std::clamp(it->currentTime / it->lifeTime, 0.0f, 1.0f);
			const float s = it->initialScale + (it->targetScale - it->initialScale) * t;
			it->transform.SetScale({ s * it->scaleAspect.x, s * it->scaleAspect.y, 1.0f });
		}

		Vector3 rot = it->transform.GetRotate();
		rot.z += it->angularVelocity * deltaTime;
		it->transform.SetRotate(rot);

		++it;
	}
}

void AppParticleManager::EmitSpark(std::mt19937& randomEngine, const Vector3& position, const Vector3& baseVelocity, const Vector4& color, float scale, float lifeTime, uint32_t textureIndex)
{
	const Vector3 vel = baseVelocity + Vector3{
		RandRange(randomEngine, Ballistics::kSparkVelXZ),
		RandRange(randomEngine, Ballistics::kSparkVelY),
		RandRange(randomEngine, Ballistics::kSparkVelXZ)
	};
	EmitSparkWithVelocity(randomEngine, position, vel, color, scale, lifeTime, textureIndex);
	TriggerGPUEmitter(position, scale, Ballistics::kGpuEmitSparkCount);
}

void AppParticleManager::EmitSparkWithVelocity(std::mt19937& randomEngine, const Vector3& position, const Vector3& velocity, const Vector4& color, float scale, float lifeTime, uint32_t textureIndex)
{
	TriggerGPUEmitter(position, scale, Ballistics::kGpuEmitSparkVelCount);
	const float rotZ = RandAngle(randomEngine);
	const float scaleMul = scale / Ballistics::kSparkScaleRef;
	const float sx = RandRange(randomEngine, Ballistics::kSparkScaleX) * scaleMul;
	const float sy = RandRange(randomEngine, Ballistics::kSparkScaleY) * scaleMul;
	particles_.push_back(BuildParticle(
		position, rotZ, { sx, sy, 1.0f }, velocity, lifeTime, textureIndex,
		SolidColor(color), Ballistics::kSparkDyn, RandRange(randomEngine, Ballistics::kSparkSpin)));
}

void AppParticleManager::EmitDust(std::mt19937& randomEngine, const Vector3& position, float scale, const Vector4& color, uint32_t textureIndex)
{
	particles_.push_back(BuildJitteredParticle(randomEngine, position, {}, scale, color, textureIndex, Dust::kGenericDustPreset));
}

void AppParticleManager::EmitDustWithVelocity(std::mt19937& randomEngine, const Vector3& position, float scale, const Vector4& color, const Vector3& velocity, float lifeTime, uint32_t textureIndex)
{
	const float rotZ = RandAngle(randomEngine);
	const float s = RandRange(randomEngine, Dust::kGenericDustPreset.scaleX) * scale;
	particles_.push_back(BuildParticle(position, rotZ, { s, s, 1.0f }, velocity, lifeTime, textureIndex, SolidColor(color)));
}

void AppParticleManager::EmitShellCasing(std::mt19937& randomEngine, const Vector3& position, const Vector3& forward, const Vector4& color, const Vector3& scale, uint32_t textureIndex)
{
	const float rotZ = RandAngle(randomEngine);
	const Vector3 right = { forward.z, 0.0f, -forward.x };
	Vector3 spawnPos = position + right * Ballistics::kShellRightOffset;
	spawnPos.y += Ballistics::kShellUpOffset;

	const float fr = RandRange(randomEngine, Ballistics::kShellForceRight);
	const float fu = RandRange(randomEngine, Ballistics::kShellForceUp);
	const float fb = RandRange(randomEngine, Ballistics::kShellForceBack);
	const Vector3 vel = right * fr + Vector3{ 0.0f, fu, 0.0f } + forward * fb;

	particles_.push_back(BuildParticle(
		spawnPos, rotZ, scale, vel, Ballistics::kShellLifeTime, textureIndex,
		SolidColor(color), Ballistics::kShellDyn, RandRange(randomEngine, Ballistics::kShellSpin)));
}

void AppParticleManager::EmitFeather(std::mt19937& randomEngine, const Vector3& position, const Vector4& color, uint32_t textureIndex)
{
	particles_.push_back(BuildJitteredParticle(randomEngine, position, {}, 1.0f, color, textureIndex, Gore::kFeatherPreset));
}

void AppParticleManager::EmitBloodDrop(std::mt19937& randomEngine, const Vector3& position, const Vector3& baseVelocity, float speed, uint32_t textureIndex)
{
	const float r = RandRange(randomEngine, Gore::kBloodDropRedTone);
	const float g = RandRange(randomEngine, Gore::kBloodDropDarkTone);
	const float b = RandRange(randomEngine, Gore::kBloodDropDarkTone);
	const Vector4 bloodCol = { r, g, b, Gore::kBloodDropAlpha };
	particles_.push_back(BuildJitteredParticle(randomEngine, position, baseVelocity * speed, 1.0f, bloodCol, textureIndex, Gore::kBloodDropPreset));
}

void AppParticleManager::EmitBloodMist(std::mt19937& randomEngine, const Vector3& position, float scale, uint32_t textureIndex)
{
	particles_.push_back(BuildJitteredParticle(randomEngine, position, {}, scale, Gore::kBloodMistColor, textureIndex, Gore::kBloodMistPreset));
}

void AppParticleManager::EmitBloodBurst(std::mt19937& randomEngine, const Vector3& position, const Vector3& hitDirection, uint32_t textureIndex, uint32_t flashTexIndex)
{
	EmitDeathFlash(randomEngine, position, Gore::kBloodBurstFlashScale, Gore::kBloodBurstFlashColor, Gore::kBloodBurstFlashLife, flashTexIndex);

	for (int i = 0; i < Gore::kBloodBurstDropCount; ++i)
	{
		EmitBloodDrop(randomEngine, position, hitDirection, Gore::kBloodBurstDropSpeed, textureIndex);
	}
	for (int i = 0; i < Gore::kBloodBurstRingCount; ++i)
	{
		const float angle = (static_cast<float>(i) / static_cast<float>(Gore::kBloodBurstRingCount)) * kTwoPi;
		const float speed = CRandRange(Gore::kBloodBurstRingSpeed);
		const Vector3 dir = { std::cos(angle) * speed, CRandRange(Gore::kBloodBurstRingUp), std::sin(angle) * speed };
		EmitBloodDrop(randomEngine, position, dir, 1.0f, textureIndex);
	}
	for (int i = 0; i < Gore::kBloodBurstMistCount; ++i)
	{
		EmitBloodMist(randomEngine, position, Gore::kBloodBurstMistScale, textureIndex);
	}
	for (int i = 0; i < Gore::kBloodBurstFeatherCount; ++i)
	{
		const Vector4 featherCol = IsEven(i) ? Gore::kBloodFeatherRed : Gore::kBloodFeatherYellow;
		EmitFeather(randomEngine, position, featherCol, textureIndex);
	}
}

void AppParticleManager::EmitDarkBloodSmoke(std::mt19937& randomEngine, const Vector3& position, float scale, uint32_t smokeTexIndex)
{
	const Vector4 smokeCol = {
		Gore::kDarkSmokeRgb.x, Gore::kDarkSmokeRgb.y, Gore::kDarkSmokeRgb.z,
		RandRange(randomEngine, Gore::kDarkSmokeAlpha)
	};
	particles_.push_back(BuildJitteredParticle(randomEngine, position, {}, scale, smokeCol, smokeTexIndex, Gore::kDarkSmokePreset));
}

void AppParticleManager::EmitViolentBloodSpray(std::mt19937& randomEngine, const Vector3& hitPoint, const Vector3& bulletDir, bool isCritical, uint32_t bloodTexIndex, uint32_t smokeTexIndex)
{
	const int count = isCritical ? Gore::kSprayCritCount : Gore::kSprayNormalCount;
	const float maxFwd = isCritical ? Gore::kSprayCritMaxFwd : Gore::kSprayNormMaxFwd;
	const Vector3 perp1 = { -bulletDir.z, 0.0f, bulletDir.x };

	for (int i = 0; i < count; ++i)
	{
		const Vector3 pos = hitPoint + RandOffset3D(randomEngine, Gore::kSprayPosJitter);
		const float rotZ = RandAngle(randomEngine);
		const float sx = RandRange(randomEngine, Gore::kSprayScaleX);
		const float sy = RandRange(randomEngine, Gore::kSprayScaleY);
		const Vector3 vel = bulletDir * RandRange(randomEngine, Gore::kSprayMinFwdSpeed, maxFwd)
			+ perp1 * RandRange(randomEngine, Gore::kSprayConeSpread)
			+ Vector3{ 0.0f, RandRange(randomEngine, Gore::kSprayUpSpeed), 0.0f };
		const Vector4 col = IsEven(i) ? Gore::kSpraySparkColor : Gore::kSprayDebrisColor;
		particles_.push_back(BuildParticle(
			pos, rotZ, { sx, sy, 1.0f }, vel, RandRange(randomEngine, Gore::kSprayLife),
			bloodTexIndex, SolidColor(col), Gore::kSprayDyn, Gore::kSpraySpin));
	}

	const int smokeCount = isCritical ? Gore::kSprayCritSmoke : Gore::kSprayNormSmoke;
	for (int i = 0; i < smokeCount; ++i)
	{
		EmitDarkBloodSmoke(randomEngine, hitPoint, Gore::kSpraySmokeScale, smokeTexIndex);
	}
}

void AppParticleManager::EmitViolentBloodBurst(std::mt19937& randomEngine, const Vector3& enemyPos, const Vector3& hitDir, uint32_t bloodTexIndex, uint32_t smokeTexIndex, uint32_t flashTexIndex)
{
	EmitDeathFlash(randomEngine, enemyPos, Gore::kViolentFlashScale, Gore::kViolentFlashColor, Gore::kViolentFlashLife, flashTexIndex);

	for (int i = 0; i < Gore::kViolentDirCount; ++i)
	{
		Vector3 pos = enemyPos + RandOffset3D(randomEngine, Gore::kViolentDirJitter);
		pos.y += Gore::kViolentDirOffsetY;
		const float rotZ = RandAngle(randomEngine);
		const float sx = RandRange(randomEngine, Gore::kViolentDirScaleX);
		const float sy = RandRange(randomEngine, Gore::kViolentDirScaleY);
		const Vector3 vel = hitDir * RandRange(randomEngine, Gore::kViolentDirFwd) + Vector3{
			RandRange(randomEngine, Gore::kViolentDirSpreadXZ),
			RandRange(randomEngine, Gore::kViolentDirSpreadY),
			RandRange(randomEngine, Gore::kViolentDirSpreadXZ)
		};
		const Vector4 col = IsEven(i) ? Gore::kViolentDirColorEven : Gore::kViolentDirColorOdd;
		particles_.push_back(BuildParticle(
			pos, rotZ, { sx, sy, 1.0f }, vel, Gore::kViolentDirLife,
			bloodTexIndex, SolidColor(col), Gore::kViolentDirDyn, Gore::kViolentDirSpin));
	}

	const Vector3 ringOrigin = enemyPos + Vector3{ 0.0f, Gore::kViolentDirOffsetY, 0.0f };
	for (int i = 0; i < Gore::kViolentRingCount; ++i)
	{
		const float angle = (static_cast<float>(i) / static_cast<float>(Gore::kViolentRingCount)) * kTwoPi;
		const float speed = CRandRange(Gore::kViolentRingSpeed);
		const Vector3 vel = { std::cos(angle) * speed, CRandRange(Gore::kViolentRingUp), std::sin(angle) * speed };
		const float rotZ = RandAngle(randomEngine);
		const float sx = RandRange(randomEngine, Gore::kViolentRingScale);
		const float sy = RandRange(randomEngine, Gore::kViolentRingScale);
		const Vector4 col = IsEven(i) ? Gore::kViolentRingColorEven : Gore::kViolentRingColorOdd;
		particles_.push_back(BuildParticle(
			ringOrigin, rotZ, { sx, sy, 1.0f }, vel, Gore::kViolentRingLife,
			bloodTexIndex, SolidColor(col), Gore::kViolentRingDyn, Gore::kViolentRingSpin));
	}

	const Vector3 upperOrigin = enemyPos + Vector3{ 0.0f, Gore::kViolentSmokeOffsetY, 0.0f };
	for (int i = 0; i < Gore::kViolentSmokeCount; ++i)
	{
		EmitDarkBloodSmoke(randomEngine, upperOrigin, Gore::kViolentSmokeScale, smokeTexIndex);
	}
	for (int i = 0; i < Gore::kViolentFeatherCount; ++i)
	{
		const Vector4 featherCol = IsEven(i) ? Gore::kViolentFeatherYellow : Gore::kViolentFeatherWhite;
		EmitFeather(randomEngine, upperOrigin, featherCol, bloodTexIndex);
	}
}

void AppParticleManager::EmitMuzzleFlash(std::mt19937& randomEngine, const Vector3& position, const Vector3& direction, const Vector3& right, const Vector3& up, const Vector4& color, float speedMultiplier, uint32_t textureIndex)
{
	const float rotZ = RandAngle(randomEngine);
	const float forwardSpeed = CRandRange(Ballistics::kMuzzleForwardSpeed) * speedMultiplier;
	const float rightSpeed   = (CRand01() * 2.0f - 1.0f) * Ballistics::kMuzzleRightSpread * speedMultiplier;
	const float upSpeed      = (CRand01() * 2.0f - 1.0f) * Ballistics::kMuzzleUpSpread * speedMultiplier;
	const Vector3 vel = direction * forwardSpeed + right * rightSpeed + up * upSpeed;
	const float s = Ballistics::kMuzzleFlashScale;
	particles_.push_back(BuildParticle(
		position, rotZ, { s, s, 1.0f }, vel, RandRange(randomEngine, Ballistics::kMuzzleLifeTime),
		textureIndex, SolidColor(color)));
}

void AppParticleManager::EmitMuzzleFlare(std::mt19937& randomEngine, const Vector3& position, float scale, const Vector4& color, float lifeTime, uint32_t textureIndex)
{
	(void)randomEngine;
	for (const auto& layer : Ballistics::kMuzzleFlareLayers)
	{
		particles_.push_back(BuildParticle(
			position, layer.rotZ, { scale * layer.scaleMul.x, scale * layer.scaleMul.y, 1.0f },
			{}, lifeTime, textureIndex, SolidColor(color)));
	}
}

void AppParticleManager::EmitSparkPlayerRelative(std::mt19937& randomEngine, const Vector3& playerPos, const Vector3& offset, const Vector3& velocity, const Vector4& color, float scale, float lifeTime, uint32_t textureIndex)
{
	AppParticle p = BuildParticle(
		playerPos + offset, RandAngle(randomEngine), { scale, scale, 1.0f },
		velocity, lifeTime, textureIndex, SolidColor(color));
	p.followPlayer = true;
	p.offsetFromPlayer = offset;
	particles_.push_back(p);
}

void AppParticleManager::EmitDeathFlash(std::mt19937& randomEngine, const Vector3& position, float scale, const Vector4& color, float lifeTime, uint32_t textureIndex)
{
	for (const auto& layer : Ballistics::kDeathFlashLayers)
	{
		const float rotZ = layer.randomRot ? RandAngle(randomEngine) : layer.rotZ;
		const float s = scale * layer.scaleMul;
		const Vector4 col = layer.forceWhiteCore ? Vector4{ 1.0f, 1.0f, 1.0f, 1.0f } : color;
		particles_.push_back(BuildParticle(
			position, rotZ, { s, s, 1.0f }, {}, lifeTime * layer.lifeMul,
			textureIndex, SolidColor(col), {}, layer.angularVel));
	}
}

void AppParticleManager::EmitEnemyDestroyGPUBurst(std::mt19937& randomEngine, const Vector3& position, const Vector3& hitDirection, uint32_t particleTexIndex, uint32_t flashTexIndex, uint32_t smokeTexIndex)
{
	EmitDeathFlash(randomEngine, position, Gore::kEnemyDestroyFlashScale, Gore::kEnemyDestroyFlashColor, Gore::kEnemyDestroyFlashLife, flashTexIndex);

	for (int i = 0; i < Gore::kEnemyDestroySparkCount; ++i)
	{
		const float a = RandAngle(randomEngine);
		const float p = RandRange(randomEngine, Gore::kEnemyDestroyPitch);
		const float spd = RandRange(randomEngine, Gore::kEnemyDestroySpeed);
		const float cosP = std::cos(p);
		const Vector3 dir = Vector3{ std::cos(a) * cosP, std::sin(p), std::sin(a) * cosP } + hitDirection * Gore::kEnemyDestroyHitBias;
		const Vector3 vel = { dir.x * spd, dir.y * spd + Gore::kEnemyDestroyUpBoost, dir.z * spd };
		const Vector4 col = (i % Gore::kEnemyDestroyGoldStride == 0) ? Gore::kEnemyDestroySparkGold : Gore::kEnemyDestroySparkOrange;
		EmitSparkWithVelocity(randomEngine, position, vel, col, RandRange(randomEngine, Gore::kEnemyDestroyScale), RandRange(randomEngine, Gore::kEnemyDestroyLife), particleTexIndex);
	}

	for (int i = 0; i < Gore::kEnemyDestroyDustCount; ++i)
	{
		const float a = (static_cast<float>(i) / static_cast<float>(Gore::kEnemyDestroyDustCount)) * kTwoPi;
		const float s = CRandRange(Gore::kEnemyDestroyDustSpeed);
		const Vector3 sVel = { std::cos(a) * s, CRandRange(Gore::kEnemyDestroyDustUp), std::sin(a) * s };
		EmitDustWithVelocity(randomEngine, position, Gore::kEnemyDestroyDustScale, Gore::kEnemyDestroyDustColor, sVel, Gore::kEnemyDestroyDustLife, smokeTexIndex);
	}

	for (int i = 0; i < Gore::kEnemyDestroyFeatherCount; ++i)
	{
		const Vector4 featherCol = IsEven(i) ? Gore::kEnemyDestroyFeatherEven : Gore::kEnemyDestroyFeatherOdd;
		EmitFeather(randomEngine, position, featherCol, particleTexIndex);
	}
}

void AppParticleManager::EmitTargetDestroyGPUBurst(std::mt19937& randomEngine, const Vector3& position, uint32_t particleTexIndex, uint32_t flashTexIndex, uint32_t smokeTexIndex)
{
	(void)smokeTexIndex;

	const uint32_t coreTex = (flashTexIndex != TextureManager::kInvalidTextureIndex) ? flashTexIndex : particleTexIndex;
	AppParticle core = BuildParticle(
		position, 0.0f, { Ballistics::kTargetCoreInitScale, Ballistics::kTargetCoreInitScale, 1.0f },
		{}, Ballistics::kTargetCoreLife, coreTex, Ballistics::kTargetCoreColor);
	core.initialScale = Ballistics::kTargetCoreInitScale;
	core.targetScale = Ballistics::kTargetCoreTargetScale;
	AddParticle(core);

	for (int i = 0; i < Ballistics::kTargetSparkCount; ++i)
	{
		const float a = RandAngle(randomEngine);
		const float p = RandRange(randomEngine, Ballistics::kTargetPitch);
		const float spd = RandRange(randomEngine, Ballistics::kTargetSpeed);
		const float cosP = std::cos(p);
		const Vector3 dir = { std::cos(a) * cosP, std::sin(p), std::sin(a) * cosP };
		const Vector3 vel = { dir.x * spd, dir.y * spd + Ballistics::kTargetUpBoost, dir.z * spd };
		const float rotZ = RandAngle(randomEngine);
		const ScaleSpec scSpec = {
			{ RandRange(randomEngine, Ballistics::kTargetWidth), RandRange(randomEngine, Ballistics::kTargetLen) },
			1.0f, Ballistics::kTargetTargetScale
		};
		const ColorShiftSpec colSpec = {
			(i % Ballistics::kTargetHotColorStride == 0) ? Ballistics::kTargetColorHot : Ballistics::kTargetColorWarm,
			Ballistics::kTargetEndColor, true
		};
		const float spin = IsEven(i) ? Ballistics::kTargetSpinSpeed : -Ballistics::kTargetSpinSpeed;
		AddParticle(BuildScaledParticle(
			position + dir * Ballistics::kTargetSpawnDirOffset, rotZ, 1.0f, scSpec, vel,
			RandRange(randomEngine, Ballistics::kTargetLife), particleTexIndex, colSpec, Ballistics::kTargetSparkDyn, spin));
	}
}

void AppParticleManager::EmitLandingGPUBurst(std::mt19937& randomEngine, const Vector3& position, uint32_t sparkTexIndex, uint32_t smokeTexIndex, uint32_t flashTexIndex)
{
	(void)smokeTexIndex;
	(void)flashTexIndex;

	for (int i = 0; i < Ballistics::kLandingSparkCount; ++i)
	{
		const float a = RandAngle(randomEngine);
		const float cosA = std::cos(a), sinA = std::sin(a);
		const float spd = RandRange(randomEngine, Ballistics::kLandingSpeed);
		AddParticle(BuildScaledParticle(
			position + Vector3{ cosA * Ballistics::kLandingSpawnRadius, Ballistics::kLandingSpawnY, sinA * Ballistics::kLandingSpawnRadius },
			a, 1.0f, Ballistics::kLandingScale,
			{ cosA * spd, CRandRange(Ballistics::kLandingUpVel), sinA * spd },
			RandRange(randomEngine, Ballistics::kLandingLife), sparkTexIndex, Ballistics::kLandingColor, Ballistics::kLandingDyn));
	}
}

void AppParticleManager::EmitDodgeRollDust(std::mt19937& randomEngine, const Vector3& position, const Vector3& moveDirection, uint32_t smokeTexIndex)
{
	const Vector3 baseSpawn = position - Vector3{
		moveDirection.x * Dust::kDodgeBackOffset,
		Dust::kDodgeBaseLiftY,
		moveDirection.z * Dust::kDodgeBackOffset
	};

	for (int i = 0; i < Dust::kDodgeCount; ++i)
	{
		const Vector3 pos = baseSpawn + Vector3{
			RandRange(randomEngine, Dust::kDodgeSpread) * Dust::kDodgeJitterScale,
			Dust::kDodgeSpawnY,
			RandRange(randomEngine, Dust::kDodgeSpread) * Dust::kDodgeJitterScale
		};
		const float rotZ = RandAngle(randomEngine);
		const float initSc = RandRange(randomEngine, Dust::kDodgeInitScale);
		const float spd = RandRange(randomEngine, Dust::kDodgeSpeed);
		const Vector3 vel = {
			(-moveDirection.x + RandRange(randomEngine, Dust::kDodgeSpread)) * spd,
			CRandRange(Dust::kDodgeUpVel),
			(-moveDirection.z + RandRange(randomEngine, Dust::kDodgeSpread)) * spd
		};
		const float life = RandRange(randomEngine, Dust::kDodgeLife);
		const float spin = RandRange(randomEngine, Dust::kDodgeSpread) * Dust::kDodgeSpinScale;
		AddParticle(BuildScaledParticle(pos, rotZ, initSc, Dust::kDodgeScaleSpec, vel, life, smokeTexIndex, Dust::kDodgeColor, Dust::kDodgeDyn, spin));
	}
}

void AppParticleManager::EmitFootstepDust(std::mt19937& randomEngine, const Vector3& position, uint32_t smokeTexIndex)
{
	for (int i = 0; i < Dust::kStepCount; ++i)
	{
		const Vector3 pos = position + Vector3{
			RandRange(randomEngine, Dust::kStepSpread),
			Dust::kStepSpawnY,
			RandRange(randomEngine, Dust::kStepSpread)
		};
		const float rotZ = RandAngle(randomEngine);
		const float initSc = CRandRange(Dust::kStepInitScale);
		const Vector3 vel = {
			RandRange(randomEngine, Dust::kStepSpread) * Dust::kStepHorizVelMul,
			CRandRange(Dust::kStepUpVel),
			RandRange(randomEngine, Dust::kStepSpread) * Dust::kStepHorizVelMul
		};
		const float life = CRandRange(Dust::kStepLife);
		const float spin = RandRange(randomEngine, Dust::kStepSpread) * Dust::kStepSpinScale;
		AddParticle(BuildScaledParticle(pos, rotZ, initSc, Dust::kStepScaleSpec, vel, life, smokeTexIndex, Dust::kStepColor, Dust::kStepDyn, spin));
	}
}

void AppParticleManager::EmitRicochetSparks(std::mt19937& randomEngine, const Vector3& hitPoint, const Vector3& hitNormal, uint32_t sparkTexIndex, uint32_t smokeTexIndex)
{
	(void)smokeTexIndex;

	for (int i = 0; i < Ballistics::kRicochetCount; ++i)
	{
		const Vector3 refl = {
			hitNormal.x + RandRange(randomEngine, Ballistics::kRicochetSpread),
			hitNormal.y + Ballistics::kRicochetUpBias + RandRange(randomEngine, Ballistics::kRicochetSpread) * Ballistics::kRicochetUpSpreadMul,
			hitNormal.z + RandRange(randomEngine, Ballistics::kRicochetSpread)
		};
		const float spd = RandRange(randomEngine, Ballistics::kRicochetSpeed);
		const float rotZ = RandRange(randomEngine, Ballistics::kRicochetSpread) * Ballistics::kRicochetRotScale;
		const ColorShiftSpec colSpec = {
			IsEven(i) ? Ballistics::kRicochetColorEven : Ballistics::kRicochetColorOdd,
			Ballistics::kRicochetEndColor, true
		};
		AddParticle(BuildScaledParticle(
			hitPoint, rotZ, 1.0f, Ballistics::kRicochetScale, refl * spd,
			RandRange(randomEngine, Ballistics::kRicochetLife), sparkTexIndex, colSpec, Ballistics::kRicochetDyn));
	}
}

void AppParticleManager::EmitWaterSplash(std::mt19937& randomEngine, const Vector3& hitPoint, uint32_t waterTexIndex)
{
	for (int i = 0; i < Water::kSplashCount; ++i)
	{
		const float a = RandAngle(randomEngine);
		const float cosA = std::cos(a), sinA = std::sin(a);
		const float spd = RandRange(randomEngine, Water::kSplashSpeed);
		const float sc = CRandRange(Water::kSplashInitScale);
		AddParticle(BuildScaledParticle(
			hitPoint + Vector3{ cosA * Water::kSplashRadius, Water::kSplashSpawnY, sinA * Water::kSplashRadius },
			0.0f, sc, Water::kSplashScale,
			{ cosA * spd, CRandRange(Water::kSplashUpVel), sinA * spd },
			CRandRange(Water::kSplashLife), waterTexIndex, Water::kSplashColor, Water::kSplashDyn));
	}
}

void AppParticleManager::EmitHelipadBeaconMotes(std::mt19937& randomEngine, const Vector3& helipadPos, uint32_t particleTexIndex)
{
	for (int i = 0; i < Minovsky::kBeaconCount; ++i)
	{
		const float r = RandRange(randomEngine, Minovsky::kBeaconRadius);
		const float a = RandAngle(randomEngine);
		const float cosA = std::cos(a), sinA = std::sin(a);
		const float sc = CRandRange(Minovsky::kBeaconInitScale);
		const Vector3 pos = { helipadPos.x + cosA * r, helipadPos.y + Minovsky::kBeaconSpawnY, helipadPos.z + sinA * r };
		const Vector3 vel = {
			Minovsky::kBeaconDriftVelX + cosA * Minovsky::kBeaconRadialVel,
			CRandRange(Minovsky::kBeaconUpVel),
			Minovsky::kBeaconDriftVelZ + sinA * Minovsky::kBeaconRadialVel
		};
		AddParticle(BuildScaledParticle(
			pos, 0.0f, sc, Minovsky::kBeaconScale, vel, CRandRange(Minovsky::kBeaconLife),
			particleTexIndex, Minovsky::kBeaconColor, Minovsky::kBeaconDyn, 0.0f, a));
	}
}

void AppParticleManager::EmitRiverWaveRipples(std::mt19937& randomEngine, uint32_t waterTexIndex)
{
	const RangeF zRange = { GameConfig::Environment::kRiverZMin, GameConfig::Environment::kRiverZMax };
	const RangeF spdRange = { GameConfig::Environment::kRiverWaveSpeedMin, GameConfig::Environment::kRiverWaveSpeedMax };
	const RangeF foamScRange = { GameConfig::Environment::kRiverWaveScaleMin, GameConfig::Environment::kRiverWaveScaleMax };

	for (int i = 0; i < Water::kRippleCount; ++i)
	{
		const float sx = RandRange(randomEngine, Water::kRippleXRange);
		if (std::abs(sx + Water::kBridgeCenterAbsX) < Water::kBridgeExcludeHalf ||
			std::abs(sx - Water::kBridgeCenterAbsX) < Water::kBridgeExcludeHalf)
		{
			continue;
		}

		const float sz = RandRange(randomEngine, zRange);
		const float spd = RandRange(randomEngine, spdRange);
		const Vector3 pos = { sx, GameConfig::Environment::kRiverWaveParticleY, sz };
		const Vector3 vel = { -spd, 0.0f, (CRand01() - 0.5f) * Water::kRippleDriftZ };

		if (i % Water::kRippleFoamStride != 0)
		{
			const float gs = RandRange(randomEngine, Water::kRippleGlintScale);
			const DynamicsSpec glintDyn = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, Water::kRippleFadeIn, Water::kRippleGlintTwinkle, 0 };
			particles_.push_back(BuildScaledParticle(
				pos, 0.0f, gs, Water::kRippleGlintSpec, vel,
				RandRange(randomEngine, Water::kRippleLife) * Water::kRippleGlintLifeMul,
				waterTexIndex, Water::kRippleGlintColor, glintDyn, 0.0f, static_cast<float>(i) * Water::kRipplePhaseStep));
		}
		else
		{
			const float fs = RandRange(randomEngine, foamScRange);
			const DynamicsSpec foamDyn = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, Water::kRippleFadeIn, 0.0f, 0 };
			particles_.push_back(BuildScaledParticle(
				pos, 0.0f, fs, Water::kRippleFoamSpec, vel,
				RandRange(randomEngine, Water::kRippleLife), waterTexIndex, Water::kRippleFoamColor, foamDyn));
		}
	}

	for (float pillarX : Water::kBridgePillarXs)
	{
		if (std::rand() % Water::kPercentModulo < Water::kPillarSprayChance)
		{
			const Vector3 pPos = { pillarX, GameConfig::Environment::kRiverSplashY, RandRange(randomEngine, zRange) };
			EmitRealisticRiverSpray(randomEngine, pPos, { -1.0f, 0.0f, 0.0f }, Water::kPillarFlowSpeed, waterTexIndex, waterTexIndex);
		}
	}
}

void AppParticleManager::EmitRiverSplashDroplet(std::mt19937& randomEngine, const Vector3& position, uint32_t waterTexIndex)
{
	for (int i = 0; i < Water::kRiverDropCount; ++i)
	{
		const float a = RandAngle(randomEngine);
		const float spd = RandRange(randomEngine, Water::kRiverDropSpeed);
		const Vector3 vel = { std::cos(a) * spd, CRandRange(Water::kRiverDropUpVel), std::sin(a) * spd };
		const Vector3 pos = position + Vector3{
			(CRand01() - 0.5f) * Water::kRiverDropJitterXZ,
			Water::kRiverDropSpawnY,
			(CRand01() - 0.5f) * Water::kRiverDropJitterXZ
		};
		const float s = RandRange(randomEngine, Water::kRiverDropScale);
		particles_.push_back(BuildScaledParticle(
			pos, 0.0f, s, Water::kRiverDropScSpec, vel, CRandRange(Water::kRiverDropLife),
			waterTexIndex, Water::kRiverDropColor, Water::kRiverDropDyn));
	}
}

void AppParticleManager::EmitRealisticRiverSpray(std::mt19937& randomEngine, const Vector3& crestPos, const Vector3& currentDir, float currentSpeed, uint32_t dropletTex, uint32_t mistTex)
{
	const Vector3 perp = { -currentDir.z, 0.0f, currentDir.x };
	const uint32_t actualMistTex = (mistTex != UINT32_MAX) ? mistTex : dropletTex;

	for (int i = 0; i < Water::kSprayDropCount; ++i)
	{
		const Vector3 spawn = crestPos + perp * RandRange(randomEngine, Water::kSprayLatJitter) + Vector3{ 0.0f, Water::kSprayDropSpawnY, 0.0f };
		const float sc = RandRange(randomEngine, Water::kSprayDropScale);
		const Vector3 vel = currentDir * (currentSpeed * RandRange(randomEngine, Water::kSpraySpeedJitter))
			+ perp * (RandRange(randomEngine, Water::kSprayLatJitter) * Water::kSprayDropLatVelMul)
			+ Vector3{ 0.0f, RandRange(randomEngine, Water::kSprayUpVel), 0.0f };
		particles_.push_back(BuildScaledParticle(
			spawn, 0.0f, sc, Water::kSprayDropScSpec, vel, RandRange(randomEngine, Water::kSprayDropLife),
			dropletTex, Water::kSprayDropColor, Water::kSprayDropDyn));
	}

	const Vector3 mistVel = currentDir * (currentSpeed * Water::kSprayMistFwdMul) + Vector3{ 0.0f, Water::kSprayMistUpVel, 0.0f };
	for (int i = 0; i < Water::kSprayMistCount; ++i)
	{
		const Vector3 spawn = crestPos + perp * (RandRange(randomEngine, Water::kSprayLatJitter) * Water::kSprayMistLatMul) + Vector3{ 0.0f, Water::kSprayMistSpawnY, 0.0f };
		const float sc = CRandRange(Water::kSprayMistScale);
		particles_.push_back(BuildScaledParticle(
			spawn, 0.0f, sc, Water::kSprayMistScSpec, mistVel, CRandRange(Water::kSprayMistLife),
			actualMistTex, Water::kSprayMistColor, Water::kSprayMistDyn));
	}

	const Vector3 spumeVel = currentDir * (currentSpeed * Water::kSpraySpumeFwdMul);
	for (int i = 0; i < Water::kSpraySpumeCount; ++i)
	{
		const Vector3 spawn = crestPos + perp * (RandRange(randomEngine, Water::kSprayLatJitter) * Water::kSpraySpumeLatMul);
		const float sc = CRandRange(Water::kSpraySpumeScale);
		particles_.push_back(BuildScaledParticle(
			spawn, 0.0f, sc, Water::kSpraySpumeScSpec, spumeVel, CRandRange(Water::kSpraySpumeLife),
			dropletTex, Water::kSpraySpumeColor, Water::kSpraySpumeDyn));
	}
}

void AppParticleManager::EmitDuckHullSplash(std::mt19937& randomEngine, const Vector3& duckPos, const Vector3& currentDir, uint32_t dropletTex, uint32_t mistTex)
{
	(void)randomEngine;
	(void)mistTex;

	const Vector3 perp = { -currentDir.z, 0.0f, currentDir.x };
	for (int i = 0; i < Water::kDuckBowCount; ++i)
	{
		const float flank = (CRand01() - 0.5f) * Water::kDuckBowFlankSpan;
		const Vector3 spawn = duckPos - currentDir * Water::kDuckBowFwdOffset + perp * flank + Vector3{ 0.0f, Water::kDuckBowSpawnY, 0.0f };
		const float sc = CRandRange(Water::kDuckBowScale);
		const Vector3 vel = perp * (flank * Water::kDuckBowLatVelMul) + currentDir * Water::kDuckBowFwdVel + Vector3{ 0.0f, CRandRange(Water::kDuckBowUpVel), 0.0f };
		particles_.push_back(BuildParticle(
			spawn, 0.0f, { sc, sc, 1.0f }, vel, CRandRange(Water::kDuckBowLife),
			dropletTex, Water::kDuckBowColor, Water::kDuckBowDyn));
	}

	for (float side : { -1.0f, 1.0f })
	{
		const float flankBack = CRand01() * Water::kDuckWakeBackSpan;
		const Vector3 spawn = duckPos + perp * (side * Water::kDuckWakeSideOff) + currentDir * flankBack;
		const float sc = CRandRange(Water::kDuckWakeScale);
		const Vector3 vel = currentDir * Water::kDuckWakeFwdVel + perp * (side * Water::kDuckWakeSideVel);
		particles_.push_back(BuildScaledParticle(
			spawn, 0.0f, sc, Water::kDuckWakeScSpec, vel, CRandRange(Water::kDuckWakeLife),
			dropletTex, Water::kDuckWakeColor, Water::kDuckWakeDyn));
	}
}

void AppParticleManager::EmitMouseWaterSplashFling(std::mt19937& randomEngine, const Vector3& waterPos, const Vector3& flingDir, float flingSpeed, uint32_t dropletTex, uint32_t mistTex)
{
	(void)randomEngine;

	const Vector3 perp = { -flingDir.z, 0.0f, flingDir.x };
	const float spdClamped = std::clamp(flingSpeed * Water::kFlingSpdScale, Water::kFlingSpdClamp.min, Water::kFlingSpdClamp.max);

	for (int i = 0; i < Water::kFlingDropCount; ++i)
	{
		const float jitter = (CRand01() - 0.5f) * Water::kFlingPosJitter;
		const Vector3 spawn = waterPos + perp * jitter + Vector3{ 0.0f, Water::kFlingDropSpawnY, 0.0f };
		const float sc = CRandRange(Water::kFlingDropScale);
		const float spread = (CRand01() - 0.5f) * Water::kFlingLatSpread;
		const Vector3 vel = flingDir * spdClamped + perp * spread + Vector3{ 0.0f, CRandRange(Water::kFlingDropUpVel), 0.0f };
		particles_.push_back(BuildParticle(
			spawn, 0.0f, { sc, sc, 1.0f }, vel, CRandRange(Water::kFlingDropLife),
			dropletTex, Water::kFlingDropColor, Water::kFlingDropDyn));
	}

	const uint32_t actualMistTex = (mistTex != UINT32_MAX) ? mistTex : dropletTex;
	const Vector3 mistPos = waterPos + Vector3{ 0.0f, Water::kFlingMistSpawnY, 0.0f };
	const Vector3 mistVel = flingDir * (spdClamped * Water::kFlingMistFwdMul) + Vector3{ 0.0f, Water::kFlingMistUpVel, 0.0f };
	for (int i = 0; i < Water::kFlingMistCount; ++i)
	{
		const float sc = CRandRange(Water::kFlingMistScale);
		particles_.push_back(BuildScaledParticle(
			mistPos, 0.0f, sc, Water::kFlingMistScSpec, mistVel, CRandRange(Water::kFlingMistLife),
			actualMistTex, Water::kFlingMistColor, Water::kFlingMistDyn));
	}
}

void AppParticleManager::EmitShorelineWave(std::mt19937& randomEngine, const Vector3& shorePos, const Vector3& shoreNormal, float waveEnergy, uint32_t dropletTex, uint32_t /*mistTex*/)
{
	(void)waveEnergy;

	Vector3 shoreTangent = { -shoreNormal.z, 0.0f, shoreNormal.x };
	if (shoreTangent.z > 0.0f) { shoreTangent = shoreTangent * -1.0f; }

	const Vector3 spawn = shorePos + shoreTangent * RandRange(randomEngine, Water::kShoreLatJitter) + Vector3{ 0.0f, Water::kShoreSpawnY, 0.0f };
	const float angle = std::atan2(shoreTangent.x, -shoreTangent.z);
	const Vector3 vel = shoreTangent * (Water::kShoreBaseSpeed * RandRange(randomEngine, Water::kShoreSpeedJitter));

	AppParticle p = BuildParticle(
		spawn, angle, { Water::kShoreQuadSize.x, Water::kShoreQuadSize.y, 1.0f },
		vel, CRandRange(Water::kShoreLife), dropletTex, Water::kShoreColor, Water::kShoreDyn);
	p.initialScale = Water::kShoreInitScale;
	p.targetScale = Water::kShoreTargetScale;
	particles_.push_back(p);
}

void AppParticleManager::EmitMinovskySwirl(std::mt19937& randomEngine, const Vector3& center, float radius, float height, float angle, bool isMagenta, uint32_t textureIndex, float scale)
{
	const float r = radius + RandRange(randomEngine, Minovsky::kSwirlJitter);
	const Vector3 pos = {
		center.x + std::cos(angle) * r,
		center.y + height + RandRange(randomEngine, Minovsky::kSwirlJitter),
		center.z + std::sin(angle) * r
	};
	const Vector3 tangent = { -std::sin(angle), Minovsky::kSwirlTangentY, std::cos(angle) };
	const Vector3 vel = tangent * (Minovsky::kSwirlTangentSpd + RandRange(randomEngine, Minovsky::kSwirlJitter) * 2.0f)
		+ Vector3{ 0.0f, Minovsky::kSwirlUpVel, 0.0f };
	const float actualScale = scale * RandRange(randomEngine, Minovsky::kSwirlScaleJitter);
	const float rotZ = RandAngle(randomEngine);
	const ColorShiftSpec& colSpec = isMagenta ? Minovsky::kSwirlMagenta : Minovsky::kSwirlCyan;

	AppParticle p = BuildScaledParticle(
		pos, rotZ, actualScale, Minovsky::kSwirlScale, vel, RandRange(randomEngine, Minovsky::kSwirlLife),
		textureIndex, colSpec, Minovsky::kSwirlDyn, RandRange(randomEngine, Minovsky::kSwirlSpin));
	p.wobbleFreq = Minovsky::kSwirlWobbleFreq + RandRange(randomEngine, Minovsky::kSwirlJitter) * Minovsky::kSwirlWobbleJitter;
	p.wobbleAmp = Minovsky::kSwirlWobbleAmp;
	const float axisAngle = angle * Minovsky::kSwirlAxisAngleMul;
	p.wobbleAxis = { std::sin(axisAngle), Minovsky::kSwirlAxisY, std::cos(axisAngle) };
	p.twinklePhase = RandAngle(randomEngine);
	p.twinkleSpeed = RandRange(randomEngine, Minovsky::kSwirlTwinkleSpeed);
	particles_.push_back(p);
}

void AppParticleManager::EmitMinovskyStream(std::mt19937& randomEngine, const Vector3& origin, const Vector3& velocity, bool isMagenta, uint32_t textureIndex, float scale)
{
	const Vector3 pos = origin + RandOffset3D(randomEngine, Minovsky::kStreamJitter.max);
	const float rotZ = RandAngle(randomEngine);
	const Vector3 vel = velocity + RandOffset3D(randomEngine, Minovsky::kStreamJitter.max) * 2.0f;
	const Vector4& col = isMagenta ? Minovsky::kStreamMagenta : Minovsky::kStreamCyan;

	particles_.push_back(BuildScaledParticle(
		pos, rotZ, scale, Minovsky::kStreamScale, vel, RandRange(randomEngine, Minovsky::kStreamLife),
		textureIndex, SolidColor(col), Minovsky::kStreamDyn, RandRange(randomEngine, Minovsky::kStreamSpin)));
}

void AppParticleManager::EmitMinovskyBurst(std::mt19937& randomEngine, const Vector3& center, int count, uint32_t circleTexIndex, uint32_t starTexIndex, float power)
{
	std::uniform_int_distribution<int> colorDist(0, Minovsky::kBurstColorMaxRoll);
	const RangeF speedRange = { power * Minovsky::kBurstMinSpdMul, power * Minovsky::kBurstMaxSpdMul };

	for (int i = 0; i < count; ++i)
	{
		Vector3 dir = RandOffset3D(randomEngine, 1.0f);
		const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
		dir = (len > Minovsky::kBurstDirEpsilon) ? (dir * (1.0f / len)) : Vector3{ 0.0f, 1.0f, 0.0f };

		const Vector3 vel = dir * RandRange(randomEngine, speedRange);
		float scale = RandRange(randomEngine, Minovsky::kBurstScale);
		const Vector3 initQuadScale = { scale, scale, 1.0f };
		const float rotZ = RandAngle(randomEngine);

		const int colType = colorDist(randomEngine);
		Vector4 col{};
		uint32_t tex = circleTexIndex;
		if (colType <= Minovsky::kBurstStarRollMax && starTexIndex != UINT32_MAX)
		{
			col = { 1.0f, 1.0f, 1.0f, 1.0f };
			tex = starTexIndex;
			scale *= Minovsky::kBurstStarScaleMul;
		}
		else if (colType <= Minovsky::kBurstCyanRollMax)
		{
			col = Minovsky::kBurstCyanColor;
			tex = (starTexIndex != UINT32_MAX && colType == Minovsky::kBurstCyanStarRoll) ? starTexIndex : circleTexIndex;
		}
		else
		{
			col = Minovsky::kBurstMagentaColor;
			tex = (starTexIndex != UINT32_MAX && colType == Minovsky::kBurstMagStarRoll) ? starTexIndex : circleTexIndex;
		}

		AppParticle p = BuildParticle(
			center, rotZ, initQuadScale, vel, RandRange(randomEngine, Minovsky::kBurstLife),
			tex, SolidColor(col), Minovsky::kBurstDyn, RandRange(randomEngine, Minovsky::kBurstSpin));
		p.initialScale = scale * Minovsky::kBurstScaleSpec.initScaleMul;
		p.targetScale = scale * Minovsky::kBurstScaleSpec.targetScaleMul;
		particles_.push_back(p);
	}
}

void AppParticleManager::EmitMinovskyBokeh(std::mt19937& randomEngine, const Vector3& position, float scale, bool isMagenta, uint32_t textureIndex)
{
	const float rotZ = RandAngle(randomEngine);
	const Vector3 vel = {
		RandRange(randomEngine, Minovsky::kBokehDriftVel),
		RandRange(randomEngine, Minovsky::kBokehUpVel),
		RandRange(randomEngine, Minovsky::kBokehDriftVel)
	};
	const Vector4& col = isMagenta ? Minovsky::kBokehMagenta : Minovsky::kBokehCyan;

	AppParticle p = BuildScaledParticle(
		position, rotZ, scale, Minovsky::kBokehScale, vel, RandRange(randomEngine, Minovsky::kBokehLife),
		textureIndex, SolidColor(col), Minovsky::kBokehDyn, RandRange(randomEngine, Minovsky::kBokehSpin));
	p.wobbleFreq = Minovsky::kBokehWobbleFreq;
	p.wobbleAmp = Minovsky::kBokehWobbleAmp;
	p.wobbleAxis = Minovsky::kBokehWobbleAxis;
	particles_.push_back(p);
}

void AppParticleManager::EmitMinovskyDashTrail(std::mt19937& randomEngine, const Vector3& position, const Vector3& moveDirection, uint32_t circleTexIndex, uint32_t starTexIndex)
{
	std::uniform_int_distribution<int> colorDist(0, 1);
	const Vector3 backDir = moveDirection * -1.0f;

	for (int i = 0; i < Minovsky::kDashTrailCount; ++i)
	{
		const Vector3 emitPos = position + Vector3{
			RandRange(randomEngine, Minovsky::kDashJitter),
			Minovsky::kDashOffsetY + RandRange(randomEngine, Minovsky::kDashJitter) * 0.5f,
			RandRange(randomEngine, Minovsky::kDashJitter)
		};
		const Vector3 vel = backDir * (Minovsky::kDashBackSpeed + RandRange(randomEngine, Minovsky::kDashJitter) * 2.0f)
			+ Vector3{ 0.0f, Minovsky::kDashUpVel, 0.0f };
		const bool isMagenta = (colorDist(randomEngine) == 1);
		const uint32_t tex = (i == 0 && starTexIndex != UINT32_MAX) ? starTexIndex : circleTexIndex;
		EmitMinovskyStream(randomEngine, emitPos, vel, isMagenta, tex, RandRange(randomEngine, Minovsky::kDashScale));
	}
}

void AppParticleManager::EmitMinovskyFlightAura(std::mt19937& randomEngine, const Vector3& center, int count, uint32_t circleTexIndex, uint32_t starTexIndex)
{
	std::uniform_int_distribution<int> colorDist(0, Minovsky::kAuraColorMaxRoll);

	for (int i = 0; i < count; ++i)
	{
		const float angle = RandAngle(randomEngine);
		const float r = RandRange(randomEngine, Minovsky::kAuraRadius);
		const float h = RandRange(randomEngine, Minovsky::kAuraHeight);
		const Vector3 pos = { center.x + std::cos(angle) * r, center.y + h, center.z + std::sin(angle) * r };
		const Vector3 tangent = { -std::sin(angle), 0.5f, std::cos(angle) };
		const Vector3 vel = tangent * Minovsky::kAuraTangentSpd + Vector3{ 0.0f, Minovsky::kAuraUpVel, 0.0f };

		float sc = RandRange(randomEngine, Minovsky::kAuraScale);
		const Vector3 initQuadScale = { sc, sc, 1.0f };
		const float rotZ = RandAngle(randomEngine);
		const int c = colorDist(randomEngine);

		ColorShiftSpec colSpec = Minovsky::kAuraMagentaColor;
		uint32_t tex = circleTexIndex;
		if (c <= Minovsky::kAuraStarRollMax && starTexIndex != UINT32_MAX)
		{
			colSpec = Minovsky::kAuraStarColor;
			tex = starTexIndex;
			sc *= Minovsky::kAuraStarScaleMul;
		}
		else if (c <= Minovsky::kAuraCyanRollMax)
		{
			colSpec = Minovsky::kAuraCyanColor;
		}

		AppParticle p = BuildParticle(
			pos, rotZ, initQuadScale, vel, RandRange(randomEngine, Minovsky::kAuraLife),
			tex, colSpec, Minovsky::kAuraDyn, RandRange(randomEngine, Minovsky::kAuraSpin));
		p.initialScale = sc * Minovsky::kAuraScaleSpec.initScaleMul;
		p.targetScale = sc * Minovsky::kAuraScaleSpec.targetScaleMul;
		p.twinklePhase = RandAngle(randomEngine);
		p.twinkleSpeed = RandRange(randomEngine, Minovsky::kAuraTwinkleSpeed);
		particles_.push_back(p);
	}
}

void AppParticleManager::EmitMinovskyGlitter(std::mt19937& randomEngine, const Vector3& position, float scale, uint32_t starTexIndex)
{
	const Vector3 pos = position + RandOffset3D(randomEngine, Minovsky::kGlitterJitter.max);
	const float rotZ = RandAngle(randomEngine);
	const Vector3 vel = {
		RandRange(randomEngine, Minovsky::kGlitterJitter) * Minovsky::kGlitterHorizMul,
		Minovsky::kGlitterUpVel + RandRange(randomEngine, Minovsky::kGlitterJitter) * 0.5f,
		RandRange(randomEngine, Minovsky::kGlitterJitter) * Minovsky::kGlitterHorizMul
	};
	particles_.push_back(BuildScaledParticle(
		pos, rotZ, scale, Minovsky::kGlitterScale, vel, RandRange(randomEngine, Minovsky::kGlitterLife),
		starTexIndex, Minovsky::kGlitterColor, Minovsky::kGlitterDyn,
		RandRange(randomEngine, Minovsky::kGlitterSpin), RandAngle(randomEngine)));
}

void AppParticleManager::EmitMouseWaveWake(std::mt19937& randomEngine, const Vector3& pos, const Vector3& moveDir, float speed, uint32_t circleTex, uint32_t /*starTex*/)
{
	const float dirLen = std::sqrt(moveDir.x * moveDir.x + moveDir.y * moveDir.y);
	const Vector3 perp = (dirLen > Water::kWakeDirEpsilon)
		? Vector3{ -moveDir.y / dirLen, moveDir.x / dirLen, 0.0f }
		: Vector3{ 1.0f, 0.0f, 0.0f };
	const float wavePower = std::clamp(speed * Water::kWakePowerSpeedMul, Water::kWakePowerClamp.min, Water::kWakePowerClamp.max);

	for (float side : { -1.0f, 1.0f })
	{
		for (int i = 0; i < Water::kWakeBandCount; ++i)
		{
			const float spread = RandRange(randomEngine, Water::kWakeSpread);
			const Vector3 spawnPos = pos + perp * (side * (Water::kWakeSideBaseOff + Water::kWakeSideStepOff * i))
				- moveDir * (Water::kWakeBackStepOff * i)
				+ Vector3{ RandRange(randomEngine, Water::kWakeJitter), RandRange(randomEngine, Water::kWakeJitter), 0.0f };
			const Vector3 waveVel = (perp * (side * spread) - moveDir * Water::kWakeBackVelMul) * wavePower
				+ Vector3{ 0.0f, Water::kWakeUpVel, 0.0f };
			const float sc = RandRange(randomEngine, Water::kWakeScale) * (1.0f + wavePower * Water::kWakeScalePowerMul);
			const ColorShiftSpec& colSpec = (i == 0) ? Water::kWakeColorInner : Water::kWakeColorOuter;

			particles_.push_back(BuildScaledParticle(
				spawnPos, RandAngle(randomEngine), sc, Water::kWakeScaleSpec, waveVel,
				RandRange(randomEngine, Water::kWakeLife), circleTex, colSpec, Water::kWakeDyn,
				RandRange(randomEngine, Water::kWakeSpin)));
		}
	}
}

void AppParticleManager::EmitMouseRippleRing(std::mt19937& randomEngine, const Vector3& center, float power, uint32_t circleTex, uint32_t /*starTex*/)
{
	const float expSpeed = Water::kRingExpSpeedMul * power;
	for (int i = 0; i < Water::kRingCount; ++i)
	{
		const float angle = (static_cast<float>(i) / static_cast<float>(Water::kRingCount)) * kTwoPi;
		const Vector3 dir = { std::cos(angle), std::sin(angle), 0.0f };
		const Vector3 spawnPos = center + dir * Water::kRingSpawnRadius
			+ Vector3{ RandRange(randomEngine, Water::kRingJitter), RandRange(randomEngine, Water::kRingJitter), 0.0f };
		const float sc = RandRange(randomEngine, Water::kRingScale);

		particles_.push_back(BuildScaledParticle(
			spawnPos, angle, sc, Water::kRingScaleSpec, dir * expSpeed,
			RandRange(randomEngine, Water::kRingLife), circleTex, Water::kRingColor, Water::kRingDyn));
	}
}

void AppParticleManager::ApplyMouseWaveDisturbance(const Vector3& mouseWorldPos, const Vector3& mouseWorldVel, float radius, float force)
{
	const float radSq = radius * radius;
	for (auto& p : particles_)
	{
		Vector3 diff = p.transform.GetTranslate() - mouseWorldPos;
		diff.z *= 0.5f;
		const float distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
		if (distSq < radSq && distSq > Core::kDisturbMinDistSq)
		{
			const float dist = std::sqrt(distSq);
			const float factor = (1.0f - dist / radius);
			const Vector3 pushDir = diff * (1.0f / dist);
			p.velocity += pushDir * (force * factor * Core::kDisturbPushForceScale) + mouseWorldVel * (Core::kDisturbVelInherit * factor);
			p.wobbleAmp = (std::max)(p.wobbleAmp, Core::kDisturbMinWobbleAmp * factor);
		}
	}
}

void AppParticleManager::ApplyRotorDownwash(const Vector3& rotorPos, float rotorRadius, float strength, float deltaTime)
{
	if (strength <= Core::kDownwashMinEpsilon || rotorRadius <= Core::kDownwashMinEpsilon) return;

	const float maxRadius = rotorRadius * Core::kDownwashRadiusMul;
	const float maxRadiusSq = maxRadius * maxRadius;

	for (auto& p : particles_)
	{
		Vector3 curPos = p.transform.GetTranslate();

		// ローターの下方空間（ローター高度+0.5m〜地上付近）を先に判定し、対象外の std::sqrt を回避
		const float relY = rotorPos.y - curPos.y;
		if (relY < Core::kDownwashMinRelY || relY > Core::kDownwashMaxRelY)
		{
			continue;
		}

		const float dx = curPos.x - rotorPos.x;
		const float dz = curPos.z - rotorPos.z;
		const float distSq = dx * dx + dz * dz;
		if (distSq >= maxRadiusSq || distSq <= Core::kDownwashMinDistSq)
		{
			continue;
		}

		const float dist = std::sqrt(distSq);
		const float hFactor = std::clamp(1.0f - (relY / Core::kDownwashMaxRelY), Core::kDownwashMinHeightFactor, 1.0f);
		const float dirX = dx / dist;
		const float dirZ = dz / dist;

		// 1. ローター直下の強力な下降気流（Downwash）：地面へ向かって吹き下ろす
		if (dist < rotorRadius)
		{
			const float centerFactor = 1.0f - (dist / rotorRadius);
			p.velocity.y -= strength * hFactor * centerFactor * Core::kDownwashVertForceScale * deltaTime;
		}

		// 2. 地面付近での外向き放射流（Ground Outflow / Radial Blast）
		if (curPos.y < Core::kOutflowMaxHeightY)
		{
			const float groundFactor = std::clamp((Core::kOutflowMaxHeightY - curPos.y) / Core::kOutflowMaxHeightY, 0.0f, 1.0f);
			const float normDist = std::clamp(dist / maxRadius, 0.0f, 1.0f);
			const float radialProfile = std::sin(normDist * kPi);
			const float radialSpeed = strength * hFactor * groundFactor * (Core::kOutflowBaseSpeedMul + radialProfile * Core::kOutflowProfileSpeedMul);

			p.velocity.x += dirX * radialSpeed * deltaTime;
			p.velocity.z += dirZ * radialSpeed * deltaTime;
			p.wobbleAmp = (std::max)(p.wobbleAmp, Core::kOutflowWobbleScale * groundFactor);
		}

		// 3. 地面這い（地面以下にめり込まず水平へスライド）
		if (curPos.y < Core::kGroundSlideClampY)
		{
			curPos.y = Core::kGroundSlideClampY;
			if (p.velocity.y < 0.0f)
			{
				p.velocity.y = 0.0f;
			}
			p.transform.SetTranslate(curPos);
		}
	}
}

void AppParticleManager::DisperseAndFadeOutAll(float fadeSpeed, const Vector3& windVector, float deltaTime)
{
	for (auto& p : particles_)
	{
		p.velocity += windVector * deltaTime;
		p.currentTime += deltaTime * fadeSpeed;
	}
}

void AppParticleManager::Draw()
{
	if (!enginePM_) return;

	enginePM_->ClearParticles();

	std::list<ParticleManager::Particle> tempParticles;
	for (const auto& ap : particles_)
	{
		ParticleManager::Particle p;
		p.transform = ap.transform;
		p.velocity = ap.velocity;
		p.color = ap.color;

		const float alpha = 1.0f - (ap.currentTime / ap.lifeTime);
		p.color.w = (std::clamp)(alpha * ap.color.w, 0.0f, 1.0f);

		p.lifeTime = ap.lifeTime;
		p.currentTime = ap.currentTime;
		p.textureIndex = ap.textureIndex;

		tempParticles.push_back(p);
	}

	enginePM_->AddParticles(tempParticles);
}

void AppParticleManager::Draw(const RenderContext& ctx)
{
	if (!ctx.commandList || !enginePM_ || particles_.empty() || !ctx.camera || !perViewData_) return;

	auto BuildGPUData = [&](const AppParticle* ap) -> ParticleManager::ParticleCS {
		ParticleManager::ParticleCS p{};
		p.translate = ap->transform.GetTranslate();
		p.scale = ap->transform.GetScale();
		p.lifeTime = ap->lifeTime;
		p.currentTime = ap->currentTime;
		p.velocity = ap->velocity;

		const float t = (ap->lifeTime > 0.0f) ? std::clamp(ap->currentTime / ap->lifeTime, 0.0f, 1.0f) : 1.0f;
		if (ap->hasColorShift)
		{
			p.color.x = ap->color.x + (ap->endColor.x - ap->color.x) * t;
			p.color.y = ap->color.y + (ap->endColor.y - ap->color.y) * t;
			p.color.z = ap->color.z + (ap->endColor.z - ap->color.z) * t;
		}
		else
		{
			p.color.x = ap->color.x;
			p.color.y = ap->color.y;
			p.color.z = ap->color.z;
		}

		float twinkle = 1.0f;
		if (ap->twinkleSpeed > 0.0f)
		{
			twinkle = Core::kTwinkleBase + Core::kTwinkleAmplitude * std::sin(ap->twinklePhase + ap->currentTime * ap->twinkleSpeed);
		}

		float alpha = 1.0f - t;
		if (ap->fadeInRatio > Core::kFadeInEpsilon)
		{
			float fadeIn = std::clamp(t / ap->fadeInRatio, 0.0f, 1.0f);
			fadeIn = fadeIn * fadeIn * (Core::kSmoothstepCubicC1 - 2.0f * fadeIn);
			const float fadeOut = alpha * alpha * (Core::kSmoothstepCubicC1 - 2.0f * alpha);
			alpha = fadeIn * fadeOut;
		}
		p.color.w = (std::clamp)(alpha * ap->color.w * twinkle, 0.0f, 1.0f);
		if (premultiplyAlpha_ && !alphaBlend_)
		{
			p.color.x *= p.color.w;
			p.color.y *= p.color.w;
			p.color.z *= p.color.w;
		}
		return p;
	};

	int32_t maxSlotPerBatch[kMaxTextureBatches];
	std::fill_n(maxSlotPerBatch, kMaxTextureBatches, -1);

	bool hasAnyActiveBatch = false;
	for (auto& ap : particles_)
	{
		if (ap.stableSlot < 0)
		{
			AllocateParticleSlot(ap);
		}
		if (ap.batchIndex >= 0 && ap.batchIndex < static_cast<int16_t>(kMaxTextureBatches) &&
			ap.stableSlot >= 0 && ap.stableSlot < static_cast<int32_t>(kBatchMaxInstances))
		{
			maxSlotPerBatch[ap.batchIndex] = (std::max)(maxSlotPerBatch[ap.batchIndex], ap.stableSlot);
			hasAnyActiveBatch = true;
		}
	}

	if (!hasAnyActiveBatch) return;

	for (uint32_t b = 0; b < kMaxTextureBatches; ++b)
	{
		if (maxSlotPerBatch[b] >= 0 && textureBatches_[b].allocated && textureBatches_[b].mappedData)
		{
			const uint32_t slotCount = static_cast<uint32_t>(maxSlotPerBatch[b] + 1);
			std::memset(textureBatches_[b].mappedData, 0, sizeof(ParticleManager::ParticleCS) * slotCount);
		}
	}

	for (const auto& ap : particles_)
	{
		if (ap.batchIndex >= 0 && ap.batchIndex < static_cast<int16_t>(kMaxTextureBatches) &&
			ap.stableSlot >= 0 && ap.stableSlot <= maxSlotPerBatch[ap.batchIndex])
		{
			TextureBatch& batch = textureBatches_[ap.batchIndex];
			if (batch.allocated && batch.mappedData)
			{
				batch.mappedData[ap.stableSlot] = BuildGPUData(&ap);
			}
		}
	}

	// 1. PerView CBV の計算と転送
	Matrix4x4 billboardMatrix = Multiply(MakeRotateYMatrix(0.0f), ctx.camera->GetWorldMatrix());
	billboardMatrix.m[Core::kMatrixTranslationRow][0] = 0.0f;
	billboardMatrix.m[Core::kMatrixTranslationRow][1] = 0.0f;
	billboardMatrix.m[Core::kMatrixTranslationRow][kTranslationZCol] = 0.0f;

	perViewData_->viewProjection = Multiply(ctx.camera->GetViewMatrix(), ctx.camera->GetProjectionMatrix());
	perViewData_->billboardMatrix = billboardMatrix;
	perViewData_->deltaTime = Core::kFixedFrameDeltaTime;
	perViewData_->time = 0.0f;
	perViewData_->maxParticles = kBatchMaxInstances;

	// 2. エンジン側の Particle 描画パイプラインをセットアップ
	enginePM_->SetupDraw(ctx.commandList);

	// ストレートアルファ合成モード時は専用PSOへ切り替え（煙・雲・土埃・破片の白飛びを根絶し自然な遮蔽を実現）
	if (alphaBlend_)
	{
		if (!alphaBlendPipelineState_ && enginePM_->GetDxCommon() && enginePM_->GetDxCommon()->GetDevice())
		{
			D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = enginePM_->GetGraphicPipelineStateDesc();
			desc.BlendState.RenderTarget[0].BlendEnable = TRUE;
			desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
			desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
			desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
			desc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
			desc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
			desc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
			enginePM_->GetDxCommon()->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&alphaBlendPipelineState_));
		}
		if (alphaBlendPipelineState_)
		{
			ctx.commandList->SetPipelineState(alphaBlendPipelineState_.Get());
		}
	}

	D3D12_GPU_VIRTUAL_ADDRESS matGpuAddress = ctx.materialGPUAddress;
	if (matGpuAddress == 0 && defaultMaterialResource_)
	{
		matGpuAddress = defaultMaterialResource_->GetGPUVirtualAddress();
	}
	if (matGpuAddress != 0)
	{
		ctx.commandList->SetGraphicsRootConstantBufferView(RootParam::Particle::kMaterial, matGpuAddress);
	}

	if (ctx.light)
	{
		ctx.commandList->SetGraphicsRootConstantBufferView(RootParam::Particle::kLight, ctx.light->GetDirectionalLightResource()->GetGPUVirtualAddress());
	}

	if (ctx.camera && ctx.camera->GetCameraGpuAddress() != 0)
	{
		ctx.commandList->SetGraphicsRootConstantBufferView(RootParam::Particle::kCamera, ctx.camera->GetCameraGpuAddress());
	}

	ctx.commandList->SetGraphicsRootConstantBufferView(RootParam::Particle::kPerView, perViewResource_->GetGPUVirtualAddress());

	if (quadVertexBufferView_.SizeInBytes > 0)
	{
		ctx.commandList->IASetVertexBuffers(0, 1, &quadVertexBufferView_);
	}

	// 各テクスチャ専用 StructuredBuffer と テクスチャ SRV をペアでバインドして DrawInstanced を実行
	for (uint32_t b = 0; b < kMaxTextureBatches; ++b)
	{
		if (maxSlotPerBatch[b] < 0 || !textureBatches_[b].allocated || !textureBatches_[b].mappedData) continue;

		const TextureBatch& batch = textureBatches_[b];
		ctx.commandList->SetGraphicsRootDescriptorTable(RootParam::Particle::kInstancing, batch.srvHandleGPU);

		D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = ctx.textureHandle;
		if (batch.textureIndex != TextureManager::kInvalidTextureIndex && batch.textureIndex != 0)
		{
			srvHandle = TextureManager::GetInstance()->GetSrvHandleGPU(batch.textureIndex);
		}
		else if (srvHandle.ptr == 0)
		{
			const uint32_t whiteTex = TextureManager::GetInstance()->Load("Resources/CG4/human/white.png");
			srvHandle = TextureManager::GetInstance()->GetSrvHandleGPU(whiteTex);
		}

		ctx.commandList->SetGraphicsRootDescriptorTable(RootParam::Particle::kTextureTable, srvHandle);
		ctx.commandList->DrawInstanced(Core::kQuadVertexCount, static_cast<uint32_t>(maxSlotPerBatch[b] + 1), 0, 0);
	}
}
