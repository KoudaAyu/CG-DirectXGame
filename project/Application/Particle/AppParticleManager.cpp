#include "AppParticleManager.h"
#include <algorithm>
#include <cmath>
#include "DirectXCom.h"
#include <cassert>
#include "RootParam.h"
#include "TextureManager.h"
#include "Light.h"
#include "Camera.h"
#include <iostream>

void AppParticleManager::Initialize(ParticleManager* enginePM)
{
	enginePM_ = enginePM;
	particles_.clear();

	if (!enginePM_) return;

	DirectXCom* dxCommon = enginePM_->GetDxCommon();
	if (!dxCommon) return;

	// ルートシグネチャはエンジン側のものをそのまま使い回す
	rootSignature_ = enginePM_->GetRootSignature();

	// 自前シェーダーのコンパイル
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon->CompileShader(
		L"Application/Shaders/AppParticle.VS.hlsl",
		L"vs_6_0",
		dxCommon->GetDxcUtils().Get(),
		dxCommon->GetDxcCompiler(),
		dxCommon->GetIncludeHandler(),
		std::clog
	);
	assert(vertexShaderBlob != nullptr && "AppParticle VS Compile Failed");

	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon->CompileShader(
		L"Application/Shaders/AppParticle.PS.hlsl",
		L"ps_6_0",
		dxCommon->GetDxcUtils().Get(),
		dxCommon->GetDxcCompiler(),
		dxCommon->GetIncludeHandler(),
		std::clog
	);
	assert(pixelShaderBlob != nullptr && "AppParticle PS Compile Failed");

	// PSO Desc の作成
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignature_.Get();
	psoDesc.InputLayout = enginePM_->GetInputLayoutDesc();
	psoDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
	psoDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
	psoDesc.BlendState = enginePM_->GetBlendDesc();
	psoDesc.RasterizerState = enginePM_->GetRasterizerDesc();
	
	// パーティクル用の深度ステンシル設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	psoDesc.DepthStencilState = depthStencilDesc;
	
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	HRESULT hr = dxCommon->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
	assert(SUCCEEDED(hr) && "CreateGraphicsPipelineState failed for AppParticle");

	// 自前インスタンシング用リソースの作成とマップ
	instancingResource_ = dxCommon->CreateBufferResource(dxCommon->GetDevice(), sizeof(ParticleManager::ParticleForGPU) * kNumMaxInstances);
	instancingResource_->Map(0, nullptr, reinterpret_cast<void**>(&instanceData_));

	// SRV の作成
	auto* srvManager = TextureManager::GetInstance()->GetSRVManager();
	assert(srvManager);
	instancingSrvIndex_ = srvManager->Allocate();
	srvManager->CreateSRVForStructuredBuffer(
		instancingSrvIndex_,
		instancingResource_.Get(),
		kNumMaxInstances,
		sizeof(ParticleManager::ParticleForGPU)
	);
	instancingSrvHandleGPU_ = srvManager->GetGPUDescriptorHandle(instancingSrvIndex_);
}

AppParticleManager::~AppParticleManager()
{
	if (instancingResource_ && instanceData_)
	{
		D3D12_RANGE writtenRange = { 0, static_cast<SIZE_T>(sizeof(ParticleManager::ParticleForGPU) * kNumMaxInstances) };
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
			// Physics update to relative offset
			it->offsetFromPlayer.y -= it->gravity * deltaTime;
			it->offsetFromPlayer += it->velocity * deltaTime;
			pos = playerPos + it->offsetFromPlayer;
		}
		else
		{
			// Physics update (gravity)
			it->velocity.y -= it->gravity * deltaTime;

			pos = it->transform.GetTranslate();
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
		}
		it->transform.SetTranslate(pos);

		// Angular spin rotation on Z axis
		Vector3 rot = it->transform.GetRotate();
		rot.z += it->angularVelocity * deltaTime;
		it->transform.SetRotate(rot);

		++it;
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

void AppParticleManager::Draw(const RenderContext& ctx, Model* model, UINT externalVertexCount)
{
	if (!ctx.commandList || !enginePM_ || !pipelineState_ || !ctx.camera) return;

	uint32_t totalParticles = static_cast<uint32_t>(particles_.size());
	if (totalParticles == 0) return;

	// 1. ビルボード行列とビュープロジェクション行列の作成
	Matrix4x4 backToFrontMatrix = MakeRotateYMatrix(0.0f);
	Matrix4x4 billboardMatrix = Multiply(backToFrontMatrix, ctx.camera->GetWorldMatrix());
	billboardMatrix.m[3][0] = 0.0f;
	billboardMatrix.m[3][1] = 0.0f;
	billboardMatrix.m[3][2] = 0.0f;

	Matrix4x4 viewProjection = Multiply(ctx.camera->GetViewMatrix(), ctx.camera->GetProjectionMatrix());

	// 2. アクティブなパーティクルを収集してソート
	std::vector<const AppParticle*> activeParticles;
	activeParticles.reserve(totalParticles);
	for (const auto& p : particles_)
	{
		activeParticles.push_back(&p);
	}

	std::sort(activeParticles.begin(), activeParticles.end(), [](const AppParticle* a, const AppParticle* b) {
		return a->textureIndex < b->textureIndex;
	});

	// 3. インスタンスデータとグループの書き込み
	instanceGroups_.clear();
	uint32_t currentWriteIndex = 0;

	for (size_t i = 0; i < activeParticles.size(); ++i)
	{
		if (currentWriteIndex >= kNumMaxInstances)
		{
			break;
		}

		const auto& p = *activeParticles[i];

		if (instanceGroups_.empty() || instanceGroups_.back().textureIndex != p.textureIndex)
		{
			ParticleManager::InstanceGroup newGroup;
			newGroup.textureIndex = p.textureIndex;
			newGroup.start = currentWriteIndex;
			newGroup.count = 0;
			newGroup.srvHandle = TextureManager::GetInstance()->GetSrvHandleGPU(p.textureIndex);
			instanceGroups_.push_back(newGroup);
		}

		instanceGroups_.back().count++;

		// ビルボード＆Z回転行列を乗算
		Matrix4x4 localRot = MakeRotateZMatrix(p.transform.GetRotate().z);
		Matrix4x4 finalRot = Multiply(localRot, billboardMatrix);
		Matrix4x4 worldMatrix = MakeAffineMatrix(p.transform.GetScale(), finalRot, p.transform.GetTranslate());
		Matrix4x4 WVP = Multiply(worldMatrix, viewProjection);

		// アルファ値を考慮して色を設定
		Vector4 color = p.color;
		float alpha = 1.0f - (p.currentTime / p.lifeTime);
		color.w = (std::clamp)(alpha * p.color.w, 0.0f, 1.0f);

		instanceData_[currentWriteIndex].WVP = WVP;
		instanceData_[currentWriteIndex].World = worldMatrix;
		instanceData_[currentWriteIndex].color = color;
		instanceData_[currentWriteIndex].textureIndex = p.textureIndex;

		currentWriteIndex++;
	}

	numInstance_ = currentWriteIndex;
	if (numInstance_ == 0) return;

	// 残りのインスタンスデータをクリア
	for (uint32_t i = numInstance_; i < kNumMaxInstances; ++i)
	{
		instanceData_[i].WVP = MakeIdentity4x4();
		instanceData_[i].World = MakeIdentity4x4();
		instanceData_[i].color = { 0,0,0,0 };
		instanceData_[i].textureIndex = TextureManager::kInvalidTextureIndex;
	}

	// 4. PSOとルートシグネチャをセット
	ctx.commandList->SetGraphicsRootSignature(rootSignature_.Get());
	ctx.commandList->SetPipelineState(pipelineState_.Get());
	ctx.commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 5. 各パラメーターをセット
	ctx.commandList->SetGraphicsRootConstantBufferView(RootParam::Particle::kMaterial, ctx.materialGPUAddress);
	ctx.commandList->SetGraphicsRootDescriptorTable(RootParam::Particle::kInstancing, instancingSrvHandleGPU_);

	if (ctx.light)
	{
		ctx.commandList->SetGraphicsRootConstantBufferView(
			RootParam::Particle::kLight,
			ctx.light->GetDirectionalLightResource()->GetGPUVirtualAddress());
	}
	else
	{
		ctx.commandList->SetGraphicsRootConstantBufferView(RootParam::Particle::kLight, 0);
	}

	ctx.commandList->SetGraphicsRootConstantBufferView(
		RootParam::Particle::kCamera,
		ctx.camera->GetCameraResource() ? ctx.camera->GetCameraResource()->GetGPUVirtualAddress() : 0);

	// 6. 頂点バッファのバインド
	UINT vc = externalVertexCount;
	if (model && vc > 0)
	{
		model->Bind(ctx.commandList);
	}
	else
	{
		vc = 6;
	}

	// 7. テクスチャグループごとに描画
	for (const auto& g : instanceGroups_)
	{
		if (g.count == 0) continue;
		if (g.srvHandle.ptr != 0)
		{
			ctx.commandList->SetGraphicsRootDescriptorTable(RootParam::Particle::kTextureTable, g.srvHandle);
		}

		ctx.commandList->SetGraphicsRoot32BitConstant(RootParam::Particle::kInstanceOffset, g.start, 0);
		ctx.commandList->DrawInstanced(vc, g.count, 0, 0);
	}
}


