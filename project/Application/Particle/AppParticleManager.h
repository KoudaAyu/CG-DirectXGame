#pragma once
#include <list>
#include <random>
#include "Vector.h"
#include "Transform.h"
#include "ParticleManager.h"
#include <wrl.h>
#include <d3d12.h>
#include "RenderContext.h"
#include "Model.h"

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

	// Relative movement
	bool followPlayer = false;
	Vector3 offsetFromPlayer;

	// Advanced dynamics (for Minovsky & aerodynamic flight effects)
	float drag = 0.0f;
	float initialScale = 0.0f;
	float targetScale = 0.0f;
	float wobbleFreq = 0.0f;
	float wobbleAmp = 0.0f;
	Vector3 wobbleAxis = { 0.0f, 1.0f, 0.0f };

	// Advanced optical & GPU dynamics
	float twinklePhase = 0.0f;
	float twinkleSpeed = 0.0f;
	Vector4 endColor = { 0.0f, 0.0f, 0.0f, 0.0f };
	bool hasColorShift = false;
	float curlFreq = 0.0f;
	float curlAmp = 0.0f;
};

class AppParticleManager
{
public:
	AppParticleManager() = default;
	~AppParticleManager();

	void Initialize(ParticleManager* enginePM);
	void Update(float deltaTime, const Vector3& playerPos = { 0.0f, 0.0f, 0.0f });

	// Custom particle emission methods
	void EmitSpark(std::mt19937& randomEngine, const Vector3& position, const Vector3& baseVelocity, const Vector4& color, float scale, float lifeTime, uint32_t textureIndex);
	void EmitSparkWithVelocity(std::mt19937& randomEngine, const Vector3& position, const Vector3& velocity, const Vector4& color, float scale, float lifeTime, uint32_t textureIndex);
	void EmitSparkPlayerRelative(std::mt19937& randomEngine, const Vector3& playerPos, const Vector3& offset, const Vector3& velocity, const Vector4& color, float scale, float lifeTime, uint32_t textureIndex);
	void EmitDust(std::mt19937& randomEngine, const Vector3& position, float scale, const Vector4& color, uint32_t textureIndex);
	void EmitDustWithVelocity(std::mt19937& randomEngine, const Vector3& position, float scale, const Vector4& color, const Vector3& velocity, float lifeTime, uint32_t textureIndex);
	void EmitShellCasing(std::mt19937& randomEngine, const Vector3& position, const Vector3& forward, const Vector4& color, const Vector3& scale, uint32_t textureIndex);
	void EmitFeather(std::mt19937& randomEngine, const Vector3& position, const Vector4& color, uint32_t textureIndex);
	void EmitBloodDrop(std::mt19937& randomEngine, const Vector3& position, const Vector3& baseVelocity, float speed, uint32_t textureIndex);
	void EmitBloodMist(std::mt19937& randomEngine, const Vector3& position, float scale, uint32_t textureIndex);
	void EmitBloodBurst(std::mt19937& randomEngine, const Vector3& position, const Vector3& hitDirection, uint32_t textureIndex, uint32_t flashTexIndex);
	void EmitViolentBloodSpray(std::mt19937& randomEngine, const Vector3& hitPoint, const Vector3& bulletDir, bool isCritical, uint32_t bloodTexIndex, uint32_t smokeTexIndex);
	void EmitViolentBloodBurst(std::mt19937& randomEngine, const Vector3& enemyPos, const Vector3& hitDir, uint32_t bloodTexIndex, uint32_t smokeTexIndex, uint32_t flashTexIndex);
	void EmitDarkBloodSmoke(std::mt19937& randomEngine, const Vector3& position, float scale, uint32_t smokeTexIndex);
	void EmitMuzzleFlash(std::mt19937& randomEngine, const Vector3& position, const Vector3& direction, const Vector3& right, const Vector3& up, const Vector4& color, float speedMultiplier, uint32_t textureIndex);
	void EmitMuzzleFlare(std::mt19937& randomEngine, const Vector3& position, float scale, const Vector4& color, float lifeTime, uint32_t textureIndex);
	void EmitDeathFlash(std::mt19937& randomEngine, const Vector3& position, float scale, const Vector4& color, float lifeTime, uint32_t textureIndex);

	// 撃破時の爆散パーティクル（GPUインスタンシング）
	void EmitEnemyDestroyGPUBurst(std::mt19937& randomEngine, const Vector3& position, const Vector3& hitDirection, uint32_t particleTexIndex, uint32_t flashTexIndex, uint32_t smokeTexIndex);
	void EmitTargetDestroyGPUBurst(std::mt19937& randomEngine, const Vector3& position, uint32_t particleTexIndex, uint32_t flashTexIndex, uint32_t smokeTexIndex);

	// プレイヤーアクション & 環境GPUパーティクル
	void EmitDodgeRollDust(std::mt19937& randomEngine, const Vector3& position, const Vector3& moveDirection, uint32_t smokeTexIndex);
	void EmitFootstepDust(std::mt19937& randomEngine, const Vector3& position, uint32_t smokeTexIndex);
	void EmitRicochetSparks(std::mt19937& randomEngine, const Vector3& hitPoint, const Vector3& hitNormal, uint32_t sparkTexIndex, uint32_t smokeTexIndex);
	void EmitWaterSplash(std::mt19937& randomEngine, const Vector3& hitPoint, uint32_t waterTexIndex);
	void EmitHelipadBeaconMotes(std::mt19937& randomEngine, const Vector3& helipadPos, uint32_t particleTexIndex);
	void EmitRiverWaveRipples(std::mt19937& randomEngine, uint32_t waterTexIndex);
	void EmitRiverSplashDroplet(std::mt19937& randomEngine, const Vector3& position, uint32_t waterTexIndex);

	// ミノフスキー粒子エフェクト（閃光のハサウェイ・キルケーの魔女風 / 3D空間加算合成）
	void EmitMinovskySwirl(std::mt19937& randomEngine, const Vector3& center, float radius, float height, float angle, bool isMagenta, uint32_t textureIndex, float scale = 0.22f);
	void EmitMinovskyStream(std::mt19937& randomEngine, const Vector3& origin, const Vector3& velocity, bool isMagenta, uint32_t textureIndex, float scale = 0.16f);
	void EmitMinovskyBurst(std::mt19937& randomEngine, const Vector3& center, int count, uint32_t circleTexIndex, uint32_t starTexIndex, float power = 4.0f);
	void EmitMinovskyBokeh(std::mt19937& randomEngine, const Vector3& position, float scale, bool isMagenta, uint32_t textureIndex);
	void EmitMinovskyDashTrail(std::mt19937& randomEngine, const Vector3& position, const Vector3& moveDirection, uint32_t circleTexIndex, uint32_t starTexIndex);
	void EmitMinovskyFlightAura(std::mt19937& randomEngine, const Vector3& center, int count, uint32_t circleTexIndex, uint32_t starTexIndex);
	void EmitMinovskyGlitter(std::mt19937& randomEngine, const Vector3& position, float scale, uint32_t starTexIndex);

	// マウス水面波紋インタラクション（川の波・引き波・波紋）
	void EmitMouseWaveWake(std::mt19937& randomEngine, const Vector3& pos, const Vector3& moveDir, float speed, uint32_t circleTex, uint32_t starTex);
	void EmitMouseRippleRing(std::mt19937& randomEngine, const Vector3& center, float power, uint32_t circleTex, uint32_t starTex);
	void ApplyMouseWaveDisturbance(const Vector3& mouseWorldPos, const Vector3& mouseWorldVel, float radius, float force);

private:
	struct Vertex
	{
		Vector4 pos;
		Vector2 uv;
		Vector3 normal;
	};

	ParticleManager* enginePM_ = nullptr;
	std::list<AppParticle> particles_;

	static const uint32_t kNumMaxInstances = 8192;
	Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource_ = nullptr;
	ParticleManager::ParticleCS* instanceData_ = nullptr;
	uint32_t instancingSrvIndex_ = 0;
	D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU_{};

	Microsoft::WRL::ComPtr<ID3D12Resource> quadVertexBuffer_ = nullptr;
	D3D12_VERTEX_BUFFER_VIEW quadVertexBufferView_{};

	Microsoft::WRL::ComPtr<ID3D12Resource> perViewResource_ = nullptr;
	ParticleManager::PerView* perViewData_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> defaultMaterialResource_ = nullptr;

public:
	void Draw();
	void Draw(const RenderContext& ctx);
};



