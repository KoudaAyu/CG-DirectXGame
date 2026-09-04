#include "AppParticleManager.h"
#include "ParticleEmitter.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include <unordered_map>
#include "DirectXCom.h"
#include <cassert>
#include "RootParam.h"
#include "TextureManager.h"
#include "Light.h"
#include "Camera.h"
#include <iostream>
#include "Application/Config/GameConfig.h"


void AppParticleManager::Initialize(ParticleManager* enginePM)
{
	enginePM_ = enginePM;
	particles_.clear();

	if (enginePM_ && enginePM_->GetDxCommon())
	{
		DirectXCom* dxCommon = enginePM_->GetDxCommon();
		instancingResource_ = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), sizeof(ParticleManager::ParticleCS) * kNumMaxInstances);
		instancingResource_->Map(0, nullptr, reinterpret_cast<void**>(&instanceData_));

		auto* srvManager = TextureManager::GetInstance()->GetSRVManager();
		if (srvManager)
		{
			instancingSrvIndex_ = srvManager->Allocate();
			srvManager->CreateSRVForStructuredBuffer(
				instancingSrvIndex_,
				instancingResource_.Get(),
				kNumMaxInstances,
				sizeof(ParticleManager::ParticleCS)
			);
			instancingSrvHandleGPU_ = srvManager->GetGPUDescriptorHandle(instancingSrvIndex_);
		}

		Vertex vertices[6] = {
			{ { -0.5f, -0.5f, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } },
			{ { -0.5f,  0.5f, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
			{ {  0.5f, -0.5f, 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } },
			{ { -0.5f,  0.5f, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
			{ {  0.5f,  0.5f, 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
			{ {  0.5f, -0.5f, 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } },
		};

		quadVertexBuffer_ = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), sizeof(vertices));
		void* mapped = nullptr;
		quadVertexBuffer_->Map(0, nullptr, &mapped);
		std::memcpy(mapped, vertices, sizeof(vertices));
		quadVertexBuffer_->Unmap(0, nullptr);

		quadVertexBufferView_.BufferLocation = quadVertexBuffer_->GetGPUVirtualAddress();
		quadVertexBufferView_.SizeInBytes = sizeof(vertices);
		quadVertexBufferView_.StrideInBytes = sizeof(Vertex);

		perViewResource_ = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), sizeof(ParticleManager::PerView));
		perViewResource_->Map(0, nullptr, reinterpret_cast<void**>(&perViewData_));
		std::memset(perViewData_, 0, sizeof(ParticleManager::PerView));
	}
}


AppParticleManager::~AppParticleManager()
{
	if (perViewResource_ && perViewData_)
	{
		perViewResource_->Unmap(0, nullptr);
		perViewData_ = nullptr;
	}

	if (instancingResource_ && instanceData_)
	{
		D3D12_RANGE writtenRange = { 0, static_cast<SIZE_T>(sizeof(ParticleManager::ParticleCS) * kNumMaxInstances) };
		instancingResource_->Unmap(0, &writtenRange);
		instanceData_ = nullptr;
	}

	if (instancingSrvIndex_ != 0)
	{
		auto* srvManager = TextureManager::GetInstance()->GetSRVManager();
		if (srvManager)
		{
			srvManager->Free(instancingSrvIndex_);
		}
	}
}


void AppParticleManager::Update(float deltaTime, const Vector3& playerPos)
{
	if (!enginePM_) return;

	// パーティクル数が上限を超えた場合、古い順に即時回収して処理落ちを完全防止
	while (particles_.size() > 512)
	{
		particles_.pop_front();
	}

	auto it = particles_.begin();
	while (it != particles_.end())
	{
		it->currentTime += deltaTime;
		if (it->currentTime >= it->lifeTime)
		{
			it = particles_.erase(it);
			continue;
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

			float groundY = 0.0f;
			if (pos.y < groundY && it->velocity.y < 0.0f)
			{
				pos.y = groundY;
				it->velocity.y = -it->velocity.y * it->bounceElasticity;
				it->velocity.x *= 0.7f;
				it->velocity.z *= 0.7f;
			}
		}
		it->transform.SetTranslate(pos);

		Vector3 rot = it->transform.GetRotate();
		rot.z += it->angularVelocity * deltaTime;
		it->transform.SetRotate(rot);

		++it;
	}
}

void AppParticleManager::EmitSpark(std::mt19937& randomEngine, const Vector3& position, const Vector3& baseVelocity, const Vector4& color, float scale, float lifeTime, uint32_t textureIndex)
{
	if (enginePM_ && enginePM_->GetGPUEmitter() && enginePM_->GetGPUEmitter()->GetEmitterData())
	{
		auto* emitter = enginePM_->GetGPUEmitter()->GetEmitterData();
		emitter->translate = position;
		emitter->radius = (scale > 0.1f) ? scale : 0.4f;
		emitter->count = 6;
		emitter->emit = 1;
	}

	AppParticle p;
	p.transform.Initialize();
	p.transform.SetTranslate(position);

	std::uniform_real_distribution<float> distRotate(0.0f, 6.2831853f);
	p.transform.SetRotate({ 0.0f, 0.0f, distRotate(randomEngine) });

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

	p.gravity = 9.8f;
	p.bounceElasticity = 0.5f;

	std::uniform_real_distribution<float> spinDist(-10.0f, 10.0f);
	p.angularVelocity = spinDist(randomEngine);

	particles_.push_back(p);
}

void AppParticleManager::EmitSparkWithVelocity(std::mt19937& randomEngine, const Vector3& position, const Vector3& velocity, const Vector4& color, float scale, float lifeTime, uint32_t textureIndex)
{
	if (enginePM_ && enginePM_->GetGPUEmitter() && enginePM_->GetGPUEmitter()->GetEmitterData())
	{
		auto* emitter = enginePM_->GetGPUEmitter()->GetEmitterData();
		emitter->translate = position;
		emitter->radius = (scale > 0.1f) ? scale : 0.4f;
		emitter->count = 4;
		emitter->emit = 1;
	}

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

void AppParticleManager::EmitBloodDrop(std::mt19937& randomEngine, const Vector3& position, const Vector3& baseVelocity, float speed, uint32_t textureIndex)
{
	AppParticle p;
	p.transform.Initialize();

	// 飛び散る血液ドロップ（微細な位置オフセット）
	std::uniform_real_distribution<float> distOffset(-0.1f, 0.1f);
	p.transform.SetTranslate({
		position.x + distOffset(randomEngine),
		position.y + distOffset(randomEngine),
		position.z + distOffset(randomEngine)
	});

	std::uniform_real_distribution<float> distRotate(0.0f, 6.2831853f);
	p.transform.SetRotate({ 0.0f, 0.0f, distRotate(randomEngine) });

	// 液滴のサイズ（大小さまざま、引き伸ばされた滴）
	std::uniform_real_distribution<float> distScaleX(0.04f, 0.14f);
	std::uniform_real_distribution<float> distScaleY(0.08f, 0.28f);
	p.transform.SetScale({ distScaleX(randomEngine), distScaleY(randomEngine), 1.0f });

	// ランダムな拡散ベクトル
	std::uniform_real_distribution<float> spreadXZ(-2.5f, 2.5f);
	std::uniform_real_distribution<float> spreadY(1.0f, 5.5f);
	p.velocity = baseVelocity * speed + Vector3{ spreadXZ(randomEngine), spreadY(randomEngine), spreadXZ(randomEngine) };

	// リアルな深紅・クリムゾンレッド〜鮮血のグラデーション
	std::uniform_real_distribution<float> redTone(0.65f, 0.95f);
	std::uniform_real_distribution<float> darkTone(0.02f, 0.08f);
	p.color = { redTone(randomEngine), darkTone(randomEngine), darkTone(randomEngine), 0.95f };

	std::uniform_real_distribution<float> lifeDist(0.8f, 1.8f);
	p.lifeTime = lifeDist(randomEngine);
	p.currentTime = 0.0f;
	p.textureIndex = textureIndex;

	p.gravity = 14.0f; // 強い重力で放物線を描いて地面に落ちる
	p.bounceElasticity = 0.08f; // 地面にビチャッと落ちて弾まない

	std::uniform_real_distribution<float> spinDist(-8.0f, 8.0f);
	p.angularVelocity = spinDist(randomEngine);

	particles_.push_back(p);
}

void AppParticleManager::EmitBloodMist(std::mt19937& randomEngine, const Vector3& position, float scale, uint32_t textureIndex)
{
	AppParticle p;
	p.transform.Initialize();

	std::uniform_real_distribution<float> distOffset(-0.2f, 0.2f);
	p.transform.SetTranslate({
		position.x + distOffset(randomEngine),
		position.y + distOffset(randomEngine),
		position.z + distOffset(randomEngine)
	});

	std::uniform_real_distribution<float> distRotate(0.0f, 6.2831853f);
	p.transform.SetRotate({ 0.0f, 0.0f, distRotate(randomEngine) });

	// 霧状の広がり
	std::uniform_real_distribution<float> distScale(0.3f, 0.8f);
	float s = distScale(randomEngine) * scale;
	p.transform.SetScale({ s, s, 1.0f });

	std::uniform_real_distribution<float> velXZ(-0.8f, 0.8f);
	std::uniform_real_distribution<float> velY(0.2f, 1.2f);
	p.velocity = { velXZ(randomEngine), velY(randomEngine), velXZ(randomEngine) };

	// 鮮血色の半透明ミスト
	p.color = { 0.85f, 0.05f, 0.05f, 0.65f };

	std::uniform_real_distribution<float> lifeDist(0.6f, 1.2f);
	p.lifeTime = lifeDist(randomEngine);
	p.currentTime = 0.0f;
	p.textureIndex = textureIndex;

	p.gravity = 0.8f;
	p.bounceElasticity = 0.0f;
	p.angularVelocity = 0.5f;

	particles_.push_back(p);
}

void AppParticleManager::EmitBloodBurst(std::mt19937& randomEngine, const Vector3& position, const Vector3& hitDirection, uint32_t textureIndex, uint32_t flashTexIndex)
{
	// 1. 鮮血の大閃光 (Blood Flash)
	EmitDeathFlash(randomEngine, position, 5.0f, { 0.95f, 0.05f, 0.05f, 1.0f }, 0.35f, flashTexIndex);

	// 2. 大量の血液ドロップ飛沫 (60個)
	for (int i = 0; i < 60; ++i)
	{
		EmitBloodDrop(randomEngine, position, hitDirection, 2.0f, textureIndex);
	}

	// 3. 全方位への高圧血しぶきスプラッター (30個)
	for (int i = 0; i < 30; ++i)
	{
		float angle = (static_cast<float>(i) / 30.0f) * 6.2831853f;
		float speed = 2.5f + (static_cast<float>(rand()) / RAND_MAX) * 3.5f;
		Vector3 dir = { std::cos(angle) * speed, 1.5f + (static_cast<float>(rand()) / RAND_MAX) * 4.0f, std::sin(angle) * speed };
		EmitBloodDrop(randomEngine, position, dir, 1.0f, textureIndex);
	}

	// 4. 血霧・血煙 (25個)
	for (int i = 0; i < 25; ++i)
	{
		EmitBloodMist(randomEngine, position, 1.8f, textureIndex);
	}

	// 5. 飛び散る羽毛 (40個)
	for (int i = 0; i < 40; ++i)
	{
		// 敵の羽毛＋血染めの羽毛
		Vector4 featherCol = (i % 2 == 0) ? Vector4{ 0.9f, 0.1f, 0.1f, 1.0f } : Vector4{ 1.0f, 0.85f, 0.2f, 1.0f };
		EmitFeather(randomEngine, position, featherCol, textureIndex);
	}
}

void AppParticleManager::EmitDarkBloodSmoke(std::mt19937& randomEngine, const Vector3& position, float scale, uint32_t smokeTexIndex)
{
	AppParticle p;
	p.transform.Initialize();

	std::uniform_real_distribution<float> distOffset(-0.25f, 0.25f);
	p.transform.SetTranslate({
		position.x + distOffset(randomEngine),
		position.y + distOffset(randomEngine),
		position.z + distOffset(randomEngine)
	});

	std::uniform_real_distribution<float> distRotate(0.0f, 6.2831853f);
	p.transform.SetRotate({ 0.0f, 0.0f, distRotate(randomEngine) });

	std::uniform_real_distribution<float> distScale(0.4f, 0.9f);
	float s = distScale(randomEngine) * scale;
	p.transform.SetScale({ s, s, 1.0f });

	std::uniform_real_distribution<float> velXZ(-0.6f, 0.6f);
	std::uniform_real_distribution<float> velY(0.3f, 1.5f);
	p.velocity = { velXZ(randomEngine), velY(randomEngine), velXZ(randomEngine) };

	// 暗赤色の硝煙・血煙
	std::uniform_real_distribution<float> alphaDist(0.4f, 0.75f);
	p.color = { 0.5f, 0.08f, 0.08f, alphaDist(randomEngine) };

	std::uniform_real_distribution<float> lifeDist(0.7f, 1.4f);
	p.lifeTime = lifeDist(randomEngine);
	p.currentTime = 0.0f;
	p.textureIndex = smokeTexIndex;

	p.gravity = -0.2f; // ふわりと上昇
	p.bounceElasticity = 0.0f;
	p.angularVelocity = 0.4f;

	particles_.push_back(p);
}

void AppParticleManager::EmitViolentBloodSpray(std::mt19937& randomEngine, const Vector3& hitPoint, const Vector3& bulletDir, bool isCritical, uint32_t bloodTexIndex, uint32_t smokeTexIndex)
{
	// 弾丸の貫通方向奥へ噴き出す高初速指向性スプラッター
	int count = isCritical ? 24 : 12;
	for (int i = 0; i < count; ++i)
	{
		AppParticle p;
		p.transform.Initialize();

		std::uniform_real_distribution<float> offsetDist(-0.08f, 0.08f);
		p.transform.SetTranslate(hitPoint + Vector3{ offsetDist(randomEngine), offsetDist(randomEngine), offsetDist(randomEngine) });

		std::uniform_real_distribution<float> distRotate(0.0f, 6.2831853f);
		p.transform.SetRotate({ 0.0f, 0.0f, distRotate(randomEngine) });

		// 不規則な細長い血飛沫
		std::uniform_real_distribution<float> sx(0.06f, 0.18f);
		std::uniform_real_distribution<float> sy(0.12f, 0.40f);
		p.transform.SetScale({ sx(randomEngine), sy(randomEngine), 1.0f });

		// 貫通方向を主軸とした円錐状の鋭い噴出
		std::uniform_real_distribution<float> coneSpread(-1.2f, 1.2f);
		std::uniform_real_distribution<float> forwardSpeed(3.0f, isCritical ? 8.0f : 5.5f);
		std::uniform_real_distribution<float> upSpeed(0.8f, 3.5f);

		Vector3 perp1 = { -bulletDir.z, 0.0f, bulletDir.x };
		p.velocity = bulletDir * forwardSpeed(randomEngine) + perp1 * coneSpread(randomEngine) + Vector3{ 0.0f, upSpeed(randomEngine), 0.0f };

		std::uniform_real_distribution<float> redTone(0.7f, 1.0f);
		p.color = { redTone(randomEngine), 0.02f, 0.02f, 0.95f };

		std::uniform_real_distribution<float> lifeDist(0.5f, 1.2f);
		p.lifeTime = lifeDist(randomEngine);
		p.currentTime = 0.0f;
		p.textureIndex = bloodTexIndex;

		p.gravity = 16.0f; // 強力な重力で地面にビシャッと落ちる
		p.bounceElasticity = 0.05f;
		p.angularVelocity = 6.0f;

		particles_.push_back(p);
	}

	// 立ち込める血煙
	int smokeCount = isCritical ? 4 : 2;
	for (int i = 0; i < smokeCount; ++i)
	{
		EmitDarkBloodSmoke(randomEngine, hitPoint, 0.7f, smokeTexIndex);
	}
}

void AppParticleManager::EmitViolentBloodBurst(std::mt19937& randomEngine, const Vector3& enemyPos, const Vector3& hitDir, uint32_t bloodTexIndex, uint32_t smokeTexIndex, uint32_t flashTexIndex)
{
	// 1. 鮮血の大閃光フラッシュ
	EmitDeathFlash(randomEngine, enemyPos, 6.0f, { 0.9f, 0.05f, 0.05f, 1.0f }, 0.40f, flashTexIndex);

	// 2. 貫通方向への超強力な指向性ブラッドバースト (50個)
	for (int i = 0; i < 50; ++i)
	{
		AppParticle p;
		p.transform.Initialize();

		std::uniform_real_distribution<float> offsetDist(-0.15f, 0.15f);
		p.transform.SetTranslate(enemyPos + Vector3{ offsetDist(randomEngine), offsetDist(randomEngine) + 0.4f, offsetDist(randomEngine) });

		std::uniform_real_distribution<float> distRotate(0.0f, 6.2831853f);
		p.transform.SetRotate({ 0.0f, 0.0f, distRotate(randomEngine) });

		std::uniform_real_distribution<float> sx(0.08f, 0.22f);
		std::uniform_real_distribution<float> sy(0.15f, 0.50f);
		p.transform.SetScale({ sx(randomEngine), sy(randomEngine), 1.0f });

		std::uniform_real_distribution<float> spreadXZ(-3.5f, 3.5f);
		std::uniform_real_distribution<float> spreadY(2.0f, 7.0f);
		std::uniform_real_distribution<float> forwardP(2.0f, 6.5f);

		p.velocity = hitDir * forwardP(randomEngine) + Vector3{ spreadXZ(randomEngine), spreadY(randomEngine), spreadXZ(randomEngine) };
		p.color = { 0.85f, 0.02f, 0.02f, 1.0f };
		p.lifeTime = 1.2f;
		p.currentTime = 0.0f;
		p.textureIndex = bloodTexIndex;

		p.gravity = 15.0f;
		p.bounceElasticity = 0.05f;
		p.angularVelocity = 8.0f;

		particles_.push_back(p);
	}

	// 3. 360度全方位への激しい肉片・血滴スプラッター (40個)
	for (int i = 0; i < 40; ++i)
	{
		float angle = (static_cast<float>(i) / 40.0f) * 6.2831853f;
		float speed = 3.0f + (static_cast<float>(rand()) / RAND_MAX) * 4.5f;
		Vector3 vel = { std::cos(angle) * speed, 1.8f + (static_cast<float>(rand()) / RAND_MAX) * 5.0f, std::sin(angle) * speed };

		AppParticle p;
		p.transform.Initialize();
		p.transform.SetTranslate(enemyPos + Vector3{ 0.0f, 0.4f, 0.0f });

		std::uniform_real_distribution<float> distRotate(0.0f, 6.2831853f);
		p.transform.SetRotate({ 0.0f, 0.0f, distRotate(randomEngine) });

		std::uniform_real_distribution<float> s(0.08f, 0.20f);
		p.transform.SetScale({ s(randomEngine), s(randomEngine), 1.0f });

		p.velocity = vel;
		p.color = { 0.75f, 0.02f, 0.02f, 1.0f };
		p.lifeTime = 1.4f;
		p.currentTime = 0.0f;
		p.textureIndex = bloodTexIndex;

		p.gravity = 14.0f;
		p.bounceElasticity = 0.05f;
		p.angularVelocity = 10.0f;

		particles_.push_back(p);
	}

	// 4. 立ち込める血煙・暗赤色の濃霧 (20個)
	for (int i = 0; i < 20; ++i)
	{
		EmitDarkBloodSmoke(randomEngine, enemyPos + Vector3{ 0.0f, 0.5f, 0.0f }, 1.5f, smokeTexIndex);
	}

	// 5. 吹き飛ぶ血染めの黒羽毛 (35個)
	for (int i = 0; i < 35; ++i)
	{
		Vector4 featherCol = (i % 2 == 0) ? Vector4{ 0.7f, 0.05f, 0.05f, 1.0f } : Vector4{ 0.15f, 0.15f, 0.15f, 1.0f };
		EmitFeather(randomEngine, enemyPos + Vector3{ 0.0f, 0.5f, 0.0f }, featherCol, bloodTexIndex);
	}
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

void AppParticleManager::EmitMuzzleFlare(std::mt19937& randomEngine, const Vector3& position, float scale, const Vector4& color, float lifeTime, uint32_t textureIndex)
{
	// Emit particles to form a cross shape + one center glow
	// Flare 1: Horizontal-ish (rotated 0)
	{
		AppParticle p;
		p.transform.Initialize();
		p.transform.SetTranslate(position);
		p.transform.SetRotate({ 0.0f, 0.0f, 0.0f });
		p.transform.SetScale({ scale * 1.5f, scale * 0.3f, 1.0f });
		p.velocity = { 0.0f, 0.0f, 0.0f };
		p.color = color;
		p.lifeTime = lifeTime;
		p.currentTime = 0.0f;
		p.textureIndex = textureIndex;
		p.gravity = 0.0f;
		p.bounceElasticity = 0.0f;
		p.angularVelocity = 0.0f;
		particles_.push_back(p);
	}
	// Flare 2: Vertical-ish (rotated 90 deg / 1.570796f)
	{
		AppParticle p;
		p.transform.Initialize();
		p.transform.SetTranslate(position);
		p.transform.SetRotate({ 0.0f, 0.0f, 1.570796f });
		p.transform.SetScale({ scale * 1.5f, scale * 0.3f, 1.0f });
		p.velocity = { 0.0f, 0.0f, 0.0f };
		p.color = color;
		p.lifeTime = lifeTime;
		p.currentTime = 0.0f;
		p.textureIndex = textureIndex;
		p.gravity = 0.0f;
		p.bounceElasticity = 0.0f;
		p.angularVelocity = 0.0f;
		particles_.push_back(p);
	}
	// Flare 3: Center glow (rotated 45 deg, uniform scale)
	{
		AppParticle p;
		p.transform.Initialize();
		p.transform.SetTranslate(position);
		p.transform.SetRotate({ 0.0f, 0.0f, 0.785398f });
		p.transform.SetScale({ scale * 0.7f, scale * 0.7f, 1.0f });
		p.velocity = { 0.0f, 0.0f, 0.0f };
		p.color = color;
		p.lifeTime = lifeTime;
		p.currentTime = 0.0f;
		p.textureIndex = textureIndex;
		p.gravity = 0.0f;
		p.bounceElasticity = 0.0f;
		p.angularVelocity = 0.0f;
		particles_.push_back(p);
	}
}

void AppParticleManager::EmitSparkPlayerRelative(std::mt19937& randomEngine, const Vector3& playerPos, const Vector3& offset, const Vector3& velocity, const Vector4& color, float scale, float lifeTime, uint32_t textureIndex)
{
	AppParticle p;
	p.transform.Initialize();
	p.transform.SetTranslate(playerPos + offset);

	std::uniform_real_distribution<float> distRotate(0.0f, 6.2831853f);
	p.transform.SetRotate({ 0.0f, 0.0f, distRotate(randomEngine) });

	// Uniform scale for clean energy particles (glow rings)
	p.transform.SetScale({ scale, scale, 1.0f });

	p.velocity = velocity;
	p.color = color;
	p.lifeTime = lifeTime;
	p.currentTime = 0.0f;
	p.textureIndex = textureIndex;

	p.gravity = 0.0f; // No gravity for clean energy ring expansion
	p.bounceElasticity = 0.0f;
	p.angularVelocity = 0.0f;

	p.followPlayer = true;
	p.offsetFromPlayer = offset;

	particles_.push_back(p);
}

void AppParticleManager::EmitDeathFlash(std::mt19937& randomEngine, const Vector3& position, float scale, const Vector4& color, float lifeTime, uint32_t textureIndex)
{
	// 敵死亡時の超強力なレンズフレア・スターバースト型閃光エフェクト
	// 異なる角度とサイズで3枚のスターバースト画像を重ねることで、動きと立体感を出す
	
	// 1枚目：中心の主ビーム（大）
	{
		AppParticle p;
		p.transform.Initialize();
		p.transform.SetTranslate(position);
		p.transform.SetRotate({ 0.0f, 0.0f, 0.0f });
		p.transform.SetScale({ scale * 1.5f, scale * 1.5f, 1.0f });
		p.velocity = { 0.0f, 0.0f, 0.0f };
		p.color = color;
		p.lifeTime = lifeTime;
		p.currentTime = 0.0f;
		p.textureIndex = textureIndex;
		p.gravity = 0.0f;
		p.bounceElasticity = 0.0f;
		p.angularVelocity = 0.5f; // 少し回転させる
		particles_.push_back(p);
	}
	
	// 2枚目：斜め45度回転（中）
	{
		AppParticle p;
		p.transform.Initialize();
		p.transform.SetTranslate(position);
		p.transform.SetRotate({ 0.0f, 0.0f, 0.785398f }); // 45 deg
		p.transform.SetScale({ scale * 1.1f, scale * 1.1f, 1.0f });
		p.velocity = { 0.0f, 0.0f, 0.0f };
		p.color = color;
		p.lifeTime = lifeTime * 0.8f; // 少し早く消す
		p.currentTime = 0.0f;
		p.textureIndex = textureIndex;
		p.gravity = 0.0f;
		p.bounceElasticity = 0.0f;
		p.angularVelocity = -0.8f; // 逆方向に回転
		particles_.push_back(p);
	}

	// 3枚目：コアの超高輝度フラッシュ（小、白め）
	{
		AppParticle p;
		p.transform.Initialize();
		p.transform.SetTranslate(position);
		std::uniform_real_distribution<float> angleDist(0.0f, 6.2831853f);
		p.transform.SetRotate({ 0.0f, 0.0f, angleDist(randomEngine) });
		p.transform.SetScale({ scale * 0.7f, scale * 0.7f, 1.0f });
		p.velocity = { 0.0f, 0.0f, 0.0f };
		p.color = { 1.0f, 1.0f, 1.0f, 1.0f }; // コアは白
		p.lifeTime = lifeTime * 0.6f;
		p.currentTime = 0.0f;
		p.textureIndex = textureIndex;
		p.gravity = 0.0f;
		p.bounceElasticity = 0.0f;
		p.angularVelocity = 1.2f;
		particles_.push_back(p);
	}
}

void AppParticleManager::EmitEnemyDestroyGPUBurst(std::mt19937& randomEngine, const Vector3& position, const Vector3& hitDirection, uint32_t particleTexIndex, uint32_t flashTexIndex, uint32_t smokeTexIndex)
{
	// 1. 巨大スターバースト衝撃閃光
	EmitDeathFlash(randomEngine, position, 4.8f, { 1.0f, 0.85f, 0.3f, 1.0f }, 0.28f, flashTexIndex);

	// 2. 超高速GPUスパーク火花（全方位＆指向性 80個）
	std::uniform_real_distribution<float> angleDist(0.0f, 6.2831853f);
	std::uniform_real_distribution<float> pitchDist(-0.8f, 1.2f);
	std::uniform_real_distribution<float> speedDist(3.5f, 9.5f);
	std::uniform_real_distribution<float> scaleDist(0.08f, 0.22f);
	std::uniform_real_distribution<float> lifeDist(0.5f, 1.4f);

	for (int i = 0; i < 80; ++i)
	{
		float a = angleDist(randomEngine);
		float p = pitchDist(randomEngine);
		float spd = speedDist(randomEngine);

		Vector3 dir = { std::cos(a) * std::cos(p), std::sin(p), std::sin(a) * std::cos(p) };
		// 弾丸の直撃方向へバイアスを加算
		dir.x += hitDirection.x * 0.8f;
		dir.y += hitDirection.y * 0.8f;
		dir.z += hitDirection.z * 0.8f;

		Vector3 vel = { dir.x * spd, dir.y * spd + 1.5f, dir.z * spd };
		Vector4 col = (i % 3 == 0) ? Vector4{ 1.0f, 0.95f, 0.5f, 1.0f } : Vector4{ 1.0f, 0.45f, 0.1f, 1.0f };

		EmitSparkWithVelocity(randomEngine, position, vel, col, scaleDist(randomEngine), lifeDist(randomEngine), particleTexIndex);
	}

	// 3. 衝撃波ダストスモーク（30個）
	for (int i = 0; i < 30; ++i)
	{
		float a = (static_cast<float>(i) / 30.0f) * 6.2831853f;
		float s = 1.5f + (static_cast<float>(rand()) / RAND_MAX) * 2.0f;
		Vector3 sVel = { std::cos(a) * s, 0.3f + (static_cast<float>(rand()) / RAND_MAX) * 0.8f, std::sin(a) * s };
		Vector4 sCol = { 0.4f, 0.35f, 0.3f, 0.55f };
		EmitDustWithVelocity(randomEngine, position, 0.9f, sCol, sVel, 0.8f, smokeTexIndex);
	}

	// 4. アヒル羽毛デブリ飛散（40個）
	for (int i = 0; i < 40; ++i)
	{
		Vector4 featherCol = (i % 2 == 0) ? Vector4{ 1.0f, 0.88f, 0.25f, 1.0f } : Vector4{ 0.95f, 0.2f, 0.1f, 1.0f };
		EmitFeather(randomEngine, position, featherCol, particleTexIndex);
	}
}

void AppParticleManager::EmitTargetDestroyGPUBurst(std::mt19937& randomEngine, const Vector3& position, uint32_t particleTexIndex, uint32_t flashTexIndex, uint32_t smokeTexIndex)
{
	// 1. 標的破壊の強烈な閃光
	EmitDeathFlash(randomEngine, position, 4.2f, { 1.0f, 0.9f, 0.4f, 1.0f }, 0.25f, flashTexIndex);

	// 2. 超高速破砕スパーク・破片（100個）
	std::uniform_real_distribution<float> angleDist(0.0f, 6.2831853f);
	std::uniform_real_distribution<float> pitchDist(-0.6f, 1.4f);
	std::uniform_real_distribution<float> speedDist(4.0f, 11.0f);
	std::uniform_real_distribution<float> scaleDist(0.06f, 0.20f);
	std::uniform_real_distribution<float> lifeDist(0.4f, 1.2f);

	for (int i = 0; i < 100; ++i)
	{
		float a = angleDist(randomEngine);
		float p = pitchDist(randomEngine);
		float spd = speedDist(randomEngine);

		Vector3 dir = { std::cos(a) * std::cos(p), std::sin(p), std::sin(a) * std::cos(p) };
		Vector3 vel = { dir.x * spd, dir.y * spd + 2.0f, dir.z * spd };

		Vector4 col;
		if (i % 4 == 0) col = { 1.0f, 0.2f, 0.2f, 1.0f };      // 標的の赤
		else if (i % 4 == 1) col = { 1.0f, 1.0f, 1.0f, 1.0f }; // 標的の白
		else col = { 1.0f, 0.7f, 0.15f, 1.0f };                // 破砕スパーク黄金

		EmitSparkWithVelocity(randomEngine, position, vel, col, scaleDist(randomEngine), lifeDist(randomEngine), particleTexIndex);
	}

	// 3. 破砕ダスト衝撃波リング（35個）
	for (int i = 0; i < 35; ++i)
	{
		float a = (static_cast<float>(i) / 35.0f) * 6.2831853f;
		float s = 2.0f + (static_cast<float>(rand()) / RAND_MAX) * 2.5f;
		Vector3 sVel = { std::cos(a) * s, 0.2f + (static_cast<float>(rand()) / RAND_MAX) * 0.6f, std::sin(a) * s };
		Vector4 sCol = { 0.7f, 0.6f, 0.45f, 0.5f };
		EmitDustWithVelocity(randomEngine, position, 0.8f, sCol, sVel, 0.7f, smokeTexIndex);
	}
}

void AppParticleManager::EmitDodgeRollDust(std::mt19937& randomEngine, const Vector3& position, const Vector3& moveDirection, uint32_t smokeTexIndex)
{
	// 回避時にプレイヤー足元後方から進行方向逆向きに吹き出す軽快な土煙（5個）
	std::uniform_real_distribution<float> spreadDist(-0.35f, 0.35f);
	std::uniform_real_distribution<float> speedDist(1.2f, 2.8f);
	std::uniform_real_distribution<float> scaleDist(0.35f, 0.65f);
	std::uniform_real_distribution<float> lifeDist(0.3f, 0.55f);

	Vector3 baseSpawn = position - Vector3{ moveDirection.x * 0.45f, -0.08f, moveDirection.z * 0.45f };

	for (int i = 0; i < 5; ++i)
	{
		Vector3 oppDir = { -moveDirection.x + spreadDist(randomEngine), 0.25f + (static_cast<float>(rand()) / RAND_MAX) * 0.35f, -moveDirection.z + spreadDist(randomEngine) };
		float spd = speedDist(randomEngine);
		Vector3 vel = { oppDir.x * spd, oppDir.y * spd, oppDir.z * spd };
		Vector4 col = { 0.70f, 0.65f, 0.55f, 0.45f };

		Vector3 p = baseSpawn + Vector3{ spreadDist(randomEngine) * 0.2f, 0.0f, spreadDist(randomEngine) * 0.2f };
		EmitDustWithVelocity(randomEngine, p, scaleDist(randomEngine), col, vel, lifeDist(randomEngine), smokeTexIndex);
	}
}

void AppParticleManager::EmitFootstepDust(std::mt19937& randomEngine, const Vector3& position, uint32_t smokeTexIndex)
{
	// よちよち歩き・ダッシュ時の足元土埃（3個）
	std::uniform_real_distribution<float> spreadDist(-0.25f, 0.25f);
	for (int i = 0; i < 3; ++i)
	{
		Vector3 vel = { spreadDist(randomEngine) * 0.8f, 0.2f + (static_cast<float>(rand()) / RAND_MAX) * 0.4f, spreadDist(randomEngine) * 0.8f };
		Vector4 col = { 0.6f, 0.55f, 0.48f, 0.4f };
		EmitDustWithVelocity(randomEngine, position + Vector3{ spreadDist(randomEngine), 0.05f, spreadDist(randomEngine) }, 0.35f, col, vel, 0.4f, smokeTexIndex);
	}
}

void AppParticleManager::EmitRicochetSparks(std::mt19937& randomEngine, const Vector3& hitPoint, const Vector3& hitNormal, uint32_t sparkTexIndex, uint32_t smokeTexIndex)
{
	// 弾丸着弾時の激しい跳弾火花（35個）＋着弾スモーク（8個）
	std::uniform_real_distribution<float> spreadDist(-0.8f, 0.8f);
	std::uniform_real_distribution<float> speedDist(3.0f, 8.5f);
	std::uniform_real_distribution<float> lifeDist(0.25f, 0.7f);

	for (int i = 0; i < 35; ++i)
	{
		Vector3 refl = {
			hitNormal.x + spreadDist(randomEngine),
			hitNormal.y + 0.3f + spreadDist(randomEngine) * 0.5f,
			hitNormal.z + spreadDist(randomEngine)
		};
		float spd = speedDist(randomEngine);
		Vector3 vel = { refl.x * spd, refl.y * spd, refl.z * spd };
		Vector4 col = (i % 2 == 0) ? Vector4{ 1.0f, 0.85f, 0.3f, 1.0f } : Vector4{ 1.0f, 0.4f, 0.05f, 1.0f };

		EmitSparkWithVelocity(randomEngine, hitPoint, vel, col, 0.08f, lifeDist(randomEngine), sparkTexIndex);
	}

	for (int i = 0; i < 8; ++i)
	{
		Vector3 sVel = { hitNormal.x * 1.5f + spreadDist(randomEngine) * 0.5f, hitNormal.y * 1.5f + 0.3f, hitNormal.z * 1.5f + spreadDist(randomEngine) * 0.5f };
		Vector4 sCol = { 0.5f, 0.48f, 0.45f, 0.45f };
		EmitDustWithVelocity(randomEngine, hitPoint, 0.45f, sCol, sVel, 0.5f, smokeTexIndex);
	}
}

void AppParticleManager::EmitWaterSplash(std::mt19937& randomEngine, const Vector3& hitPoint, uint32_t waterTexIndex)
{
	// 川面着弾時の高圧水飛沫（30個）
	std::uniform_real_distribution<float> angleDist(0.0f, 6.2831853f);
	std::uniform_real_distribution<float> speedDist(2.0f, 6.0f);

	for (int i = 0; i < 30; ++i)
	{
		float a = angleDist(randomEngine);
		float spd = speedDist(randomEngine);
		Vector3 vel = { std::cos(a) * spd, 3.5f + (static_cast<float>(rand()) / RAND_MAX) * 3.5f, std::sin(a) * spd };
		Vector4 col = { 0.65f, 0.85f, 1.0f, 0.75f };

		EmitDustWithVelocity(randomEngine, hitPoint, 0.3f, col, vel, 0.6f, waterTexIndex);
	}
}

void AppParticleManager::EmitHelipadBeaconMotes(std::mt19937& randomEngine, const Vector3& helipadPos, uint32_t particleTexIndex)
{
	// 脱出ヘリパッドから天へ舞い上がるエメラルドグリーンの光粒子（毎フレーム数個）
	std::uniform_real_distribution<float> rDist(0.0f, 2.8f);
	std::uniform_real_distribution<float> angleDist(0.0f, 6.2831853f);

	for (int i = 0; i < 4; ++i)
	{
		float r = rDist(randomEngine);
		float a = angleDist(randomEngine);
		Vector3 p = { helipadPos.x + std::cos(a) * r, helipadPos.y + 0.1f, helipadPos.z + std::sin(a) * r };
		Vector3 vel = { std::cos(a) * 0.2f, 1.5f + (static_cast<float>(rand()) / RAND_MAX) * 2.0f, std::sin(a) * 0.2f };
		Vector4 col = { 0.2f, 1.0f, 0.6f, 0.8f };

		EmitSparkWithVelocity(randomEngine, p, vel, col, 0.12f, 1.6f, particleTexIndex);
	}
}

void AppParticleManager::EmitRiverWaveRipples(std::mt19937& randomEngine, uint32_t waterTexIndex)
{
	// 川の上流 (X: +22m) から下流 (X: -22m) へ勢いよく流れる水流リップル＆白泡 (5個)
	std::uniform_real_distribution<float> zDist(GameConfig::Environment::kRiverZMin, GameConfig::Environment::kRiverZMax);
	std::uniform_real_distribution<float> xDist(-20.0f, 22.0f);
	std::uniform_real_distribution<float> speedDist(GameConfig::Environment::kRiverWaveSpeedMin, GameConfig::Environment::kRiverWaveSpeedMax);
	std::uniform_real_distribution<float> scaleDist(GameConfig::Environment::kRiverWaveScaleMin, GameConfig::Environment::kRiverWaveScaleMax);
	std::uniform_real_distribution<float> lifeDist(1.5f, 3.0f);

	for (int i = 0; i < 5; ++i)
	{
		// 水面高さに自動連動したY座標で生成（深度遮蔽を原理的に防止）
		Vector3 spawnPos = { xDist(randomEngine), GameConfig::Environment::kRiverWaveParticleY, zDist(randomEngine) };
		float spd = speedDist(randomEngine);
		// X軸マイナス方向（左）へ流れる
		Vector3 vel = { -spd, 0.0f, (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 0.3f };

		// 白波・青白い水流リップル（白泡とクリアブルー）
		Vector4 col = (i % 2 == 0) ? Vector4{ 0.95f, 0.98f, 1.0f, 0.85f } : Vector4{ 0.5f, 0.85f, 1.0f, 0.75f };

		EmitDustWithVelocity(randomEngine, spawnPos, scaleDist(randomEngine), col, vel, lifeDist(randomEngine), waterTexIndex);
	}

	// 橋脚 (X: -1.2m, +1.2m) に当たって跳ねる水流ウェーブ (白泡)
	for (float pillarX : { -1.2f, 1.2f })
	{
		if (rand() % 100 < 50)
		{
			Vector3 pPos = { pillarX, GameConfig::Environment::kRiverFoamPillarY, zDist(randomEngine) };
			Vector3 pVel = { -2.5f, 0.8f + (static_cast<float>(rand()) / RAND_MAX) * 1.0f, (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 1.5f };
			Vector4 pCol = { 0.95f, 0.98f, 1.0f, 0.9f };
			EmitDustWithVelocity(randomEngine, pPos, 1.8f, pCol, pVel, 0.7f, waterTexIndex);
		}
	}
}

void AppParticleManager::EmitRiverSplashDroplet(std::mt19937& randomEngine, const Vector3& position, uint32_t waterTexIndex)
{
	// 川面でパシャッと跳ねる水しぶき・水滴 (10個)
	std::uniform_real_distribution<float> angleDist(0.0f, 6.2831853f);
	std::uniform_real_distribution<float> speedDist(1.5f, 3.5f);

	for (int i = 0; i < 10; ++i)
	{
		float a = angleDist(randomEngine);
		float spd = speedDist(randomEngine);
		Vector3 vel = { std::cos(a) * spd, 1.8f + (static_cast<float>(rand()) / RAND_MAX) * 2.2f, std::sin(a) * spd };
		Vector4 col = { 0.85f, 0.95f, 1.0f, 0.85f };

		// 水面高さに自動連動したY座標
		Vector3 sPos = position;
		sPos.y = GameConfig::Environment::kRiverSplashY;
		EmitDustWithVelocity(randomEngine, sPos, 1.2f, col, vel, 0.55f, waterTexIndex);
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

		float alpha = 1.0f - (ap.currentTime / ap.lifeTime);
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
	if (!ctx.commandList || !enginePM_ || !instanceData_ || particles_.empty() || !ctx.camera || !perViewData_) return;

	// テクスチャインデックスごとにパーティクルをグループ化してバッチ描画
	std::unordered_map<uint32_t, std::vector<const AppParticle*>> textureGroups;
	for (const auto& ap : particles_)
	{
		textureGroups[ap.textureIndex].push_back(&ap);
	}

	struct BatchDrawInfo
	{
		uint32_t textureIndex;
		uint32_t startInstance;
		uint32_t count;
	};
	std::vector<BatchDrawInfo> batches;
	batches.reserve(textureGroups.size());

	uint32_t currentWriteIndex = 0;
	for (auto& pair : textureGroups)
	{
		if (currentWriteIndex >= kNumMaxInstances) break;

		BatchDrawInfo batch{};
		batch.textureIndex = pair.first;
		batch.startInstance = currentWriteIndex;

		for (const auto* ap : pair.second)
		{
			if (currentWriteIndex >= kNumMaxInstances) break;

			ParticleManager::ParticleCS p{};
			p.translate = ap->transform.GetTranslate();
			p.scale = ap->transform.GetScale();
			p.lifeTime = ap->lifeTime;
			p.currentTime = ap->currentTime;
			p.velocity = ap->velocity;

			float alpha = 1.0f - (ap->currentTime / ap->lifeTime);
			p.color = ap->color;
			p.color.w = (std::clamp)(alpha * ap->color.w, 0.0f, 1.0f);

			instanceData_[currentWriteIndex++] = p;
		}

		batch.count = currentWriteIndex - batch.startInstance;
		if (batch.count > 0)
		{
			batches.push_back(batch);
		}
	}

	if (currentWriteIndex == 0 || batches.empty()) return;

	// 1. PerView CBV の計算と転送
	Matrix4x4 backToFrontMatrix = MakeRotateYMatrix(0.0f);
	Matrix4x4 billboardMatrix = Multiply(backToFrontMatrix, ctx.camera->GetWorldMatrix());
	billboardMatrix.m[3][0] = 0.0f;
	billboardMatrix.m[3][1] = 0.0f;
	billboardMatrix.m[3][2] = 0.0f;

	Matrix4x4 viewProjection = Multiply(ctx.camera->GetViewMatrix(), ctx.camera->GetProjectionMatrix());

	perViewData_->viewProjection = viewProjection;
	perViewData_->billboardMatrix = billboardMatrix;
	perViewData_->deltaTime = 1.0f / 60.0f;
	perViewData_->time = 0.0f;
	perViewData_->maxParticles = kNumMaxInstances;

	// 2. エンジン側の Particle 描画パイプラインをセットアップ
	enginePM_->SetupDraw(ctx.commandList);

	ctx.commandList->SetGraphicsRootConstantBufferView(RootParam::Particle::kMaterial, ctx.materialGPUAddress);
	ctx.commandList->SetGraphicsRootDescriptorTable(RootParam::Particle::kInstancing, instancingSrvHandleGPU_);

	if (ctx.light)
	{
		ctx.commandList->SetGraphicsRootConstantBufferView(RootParam::Particle::kLight, ctx.light->GetDirectionalLightResource()->GetGPUVirtualAddress());
	}

	if (ctx.camera && ctx.camera->GetCameraGpuAddress() != 0)
	{
		ctx.commandList->SetGraphicsRootConstantBufferView(RootParam::Particle::kCamera, ctx.camera->GetCameraGpuAddress());
	}

	// PerView CBV をルートパラメーターkPerViewへバインド
	ctx.commandList->SetGraphicsRootConstantBufferView(RootParam::Particle::kPerView, perViewResource_->GetGPUVirtualAddress());

	// ユニットビルボードクアッドの頂点バッファをIASetVertexBuffersでバインド
	if (quadVertexBufferView_.SizeInBytes > 0)
	{
		ctx.commandList->IASetVertexBuffers(0, 1, &quadVertexBufferView_);
	}

	// 各テクスチャグループごとにSRVを切り替えてDrawInstancedを実行
	for (const auto& batch : batches)
	{
		D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = ctx.textureHandle;
		if (batch.textureIndex != TextureManager::kInvalidTextureIndex && batch.textureIndex != 0)
		{
			srvHandle = TextureManager::GetInstance()->GetSrvHandleGPU(batch.textureIndex);
		}
		else if (srvHandle.ptr == 0)
		{
			uint32_t whiteTex = TextureManager::GetInstance()->Load("Resources/CG4/human/white.png");
			srvHandle = TextureManager::GetInstance()->GetSrvHandleGPU(whiteTex);
		}

		ctx.commandList->SetGraphicsRootDescriptorTable(RootParam::Particle::kTextureTable, srvHandle);
		ctx.commandList->DrawInstanced(6, batch.count, 0, batch.startInstance);
	}
}





