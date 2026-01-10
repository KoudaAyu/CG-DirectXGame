#include "ParticleManager.h"
#include "DirectXCom.h"
#include "SrvManager.h"
#include <cassert>
#include <cstring> // std::memcpy
#include <iostream>

using Microsoft::WRL::ComPtr;

ParticleManager* ParticleManager::instance = nullptr;

ParticleManager* ParticleManager::GetInstance()
{
	if (!instance) instance = new ParticleManager();
	return instance;
}

void ParticleManager::Initialize(DirectXCom* dx, SrvManager* srvMgr, Object3dCom* object3dCom)
{
	// 引数記録
	dx_ = dx;
	srvMgr_ = srvMgr;
	object3dCom_ = object3dCom;

	// ランダム初期化
	std::random_device rd;
	rng_ = std::mt19937(rd());

	// ライトCB
	meshLightCB_ = dx_->CreateBufferResource(dx_->GetDevice(), sizeof(DirectionalLight));
	meshLightCB_->Map(0, nullptr, reinterpret_cast<void**>(&meshLightPtr_));
	meshLightPtr_->color = { 1,1,1,1 };
	meshLightPtr_->direction = { 0,-1,0 };
	meshLightPtr_->intensity = 0.0f;

	// パイプライン生成
	CreatePipeline_();

	// 頂点データ（板ポリ1枚）
	vertices_.clear();
	vertices_.reserve(4);
	vertices_.push_back({ -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f });
	vertices_.push_back({ -0.5f,  0.5f, 0.0f, 1.0f, 0.0f, 0.0f });
	vertices_.push_back({  0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 1.0f });
	vertices_.push_back({  0.5f,  0.5f, 0.0f, 1.0f, 1.0f, 0.0f });

	// 頂点リソース
	const size_t vbSize = sizeof(Vertex) * vertices_.size();
	vertexBuffer_ = dx_->CreateBufferResource(dx_->GetDevice(), vbSize);
	assert(vertexBuffer_ != nullptr);

	void* mapped = nullptr;
	HRESULT hr = vertexBuffer_->Map(0, nullptr, &mapped);
	assert(SUCCEEDED(hr));
	std::memcpy(mapped, vertices_.data(), vbSize);
	vertexBuffer_->Unmap(0, nullptr);

	// VBV
	vbv_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
	vbv_.SizeInBytes = static_cast<UINT>(vbSize);
	vbv_.StrideInBytes = sizeof(Vertex);

	// メッシュ用Transform CB（将来用）
	meshTransformCB_ = dx_->CreateBufferResource(dx_->GetDevice(), AlignedCBSize * kMaxMeshCB);
	meshTransformCB_->Map(0, nullptr, reinterpret_cast<void**>(&meshTransformCBBase_));
	meshTransformPtr_ = reinterpret_cast<TransformationMatrix*>(meshTransformCBBase_);
	*meshTransformPtr_ = { MakeIdentity4x4(), MakeIdentity4x4() };
}

void ParticleManager::Update(const Matrix4x4& view, const Matrix4x4& projection)
{
	viewProj_ = Multiply(view, projection);
	meshCBWriteIndex_ = 0;

	// ビルボード用
	Matrix4x4 matBillboard = view;
	matBillboard.m[3][0] = matBillboard.m[3][1] = matBillboard.m[3][2] = 0.0f;
	matBillboard.m[0][3] = matBillboard.m[1][3] = matBillboard.m[2][3] = 0.0f;
	matBillboard.m[3][3] = 1.0f;
	matBillboard = Inverse(matBillboard);

	for (auto& [name, group] : particleGroups)
	{
		// メッシュ粒子（未対応）は位置更新のみ
		if (group.useMesh)
		{
			for (auto it = group.particles.begin(); it != group.particles.end();) {
				Particle& p = *it;
				p.current += 1.0f / 60.0f;
				if (p.current >= p.lifeTime) { it = group.particles.erase(it); continue; }
				if (name == "up_gravity") { p.velocity += Vector3{ 0.0f, -0.01f, 0.0f }; }
				p.position += p.velocity;
				++it;
			}
			continue;
		}

		// 板ポリ粒子
		group.instanceCount = 0;
		constexpr UINT kMaxInstance = 1024;

		for (auto it = group.particles.begin(); it != group.particles.end();) {
			Particle& p = *it;
			p.current += 1.0f / 60.0f;
			if (p.current >= p.lifeTime) { it = group.particles.erase(it); continue; }
			if (name == "up_gravity") { p.velocity += Vector3{ 0.0f, -0.01f, 0.0f }; }
			p.position += p.velocity;
			if (group.instanceCount >= kMaxInstance) { ++it; continue; }
			if (!group.instanceMappedPtr) { ++it; continue; }

			Matrix4x4 S = MakeScaleMatrix({ p.scale, p.scale, p.scale });
			Matrix4x4 T = MakeTranslateMatrix(p.position);
			Matrix4x4 W = Multiply(S, Multiply(matBillboard, T));
			Matrix4x4 WVP = Multiply(W, viewProj_);

			auto* instData = reinterpret_cast<Matrix4x4*>(group.instanceMappedPtr);
			instData[group.instanceCount] = WVP;
			++group.instanceCount;
			++it;
		}
	}
}

void ParticleManager::Draw()
{
	ID3D12GraphicsCommandList* cl = dx_->GetCommandList().Get();

	// テクスチャ粒子（板ポリ）
	srvMgr_->PreDraw();
	cl->SetGraphicsRootSignature(rootSignature_.Get());
	if (pso_) { cl->SetPipelineState(pso_.Get()); }
	cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	cl->IASetVertexBuffers(0, 1, &vbv_);

	for (auto& [name, g] : particleGroups)
	{
		if (g.useMesh) continue;
		if (g.instanceCount == 0) continue;
		if (!g.instanceMappedPtr) continue;

		// t0: 粒子テクスチャ, t1: インスタンス(WVP)
		srvMgr_->SetGraphicsRootDescriptorTable(0, g.textureSrvIndex);
		srvMgr_->SetGraphicsRootDescriptorTable(1, g.instanceSrvIndex);
		cl->DrawInstanced(4, g.instanceCount, 0, 0);
	}

	// メッシュ粒子は未対応のため描画スキップ
}

void ParticleManager::CreatePipeline_()
{
	using Microsoft::WRL::ComPtr;

	D3D12_DESCRIPTOR_RANGE ranges[2]{};
	ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // t0
	ranges[0].NumDescriptors = 1;
	ranges[0].BaseShaderRegister = 0;
	ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // t1
	ranges[1].NumDescriptors = 1;
	ranges[1].BaseShaderRegister = 1;

	D3D12_ROOT_PARAMETER params[2]{};
	params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	params[0].DescriptorTable = { 1, &ranges[0] };
	params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	params[1].DescriptorTable = { 1, &ranges[1] };

	D3D12_STATIC_SAMPLER_DESC samp{};
	samp.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samp.AddressU = samp.AddressV = samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samp.ShaderRegister = 0;
	samp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC rsDesc{};
	rsDesc.NumParameters = _countof(params);
	rsDesc.pParameters = params;
	rsDesc.NumStaticSamplers = 1;
	rsDesc.pStaticSamplers = &samp;
	rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ComPtr<ID3DBlob> sigBlob, errBlob;
	HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob);
	if (FAILED(hr))
	{
		if (errBlob) OutputDebugStringA((const char*)errBlob->GetBufferPointer());
		assert(false);
	}
	hr = dx_->GetDevice()->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));

	ComPtr<IDxcBlob> vs = dx_->CompileShader(L"Resources/shaders/Particle.VS.hlsl", L"vs_6_0",
		dx_->GetDxcUtils(), dx_->GetDxcCompiler(), dx_->GetIncludeHandler(), std::cerr);
	ComPtr<IDxcBlob> ps = dx_->CompileShader(L"Resources/shaders/Particle.PS.hlsl", L"ps_6_0",
		dx_->GetDxcUtils(), dx_->GetDxcCompiler(), dx_->GetIncludeHandler(), std::cerr);

	D3D12_INPUT_ELEMENT_DESC inputElems[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	D3D12_BLEND_DESC blend{};
	blend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blend.RenderTarget[0].BlendEnable = TRUE;
	blend.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blend.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blend.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blend.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	blend.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

	D3D12_RASTERIZER_DESC rast{};
	rast.FillMode = D3D12_FILL_MODE_SOLID;
	rast.CullMode = D3D12_CULL_MODE_NONE;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignature_.Get();
	psoDesc.InputLayout = { inputElems, _countof(inputElems) };
	psoDesc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
	psoDesc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
	psoDesc.BlendState = blend;
	psoDesc.RasterizerState = rast;
	psoDesc.DepthStencilState.DepthEnable = FALSE;
	psoDesc.DepthStencilState.StencilEnable = FALSE;
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc.SampleDesc.Count = 1;

	hr = dx_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso_));
	assert(SUCCEEDED(hr));
}

void ParticleManager::CreateParticleGroup(const std::string name, const std::string textureFilePath)
{
	assert(particleGroups.find(name) == particleGroups.end());

	ParticleGroup newGroup{};
	newGroup.textureFilePath = textureFilePath;
	TextureManager::GetInstance()->LoadTexture(textureFilePath);
	newGroup.textureSrvIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath);

	const UINT kMaxInstance = 1024;
	size_t instanceBufferSize = sizeof(Matrix4x4) * kMaxInstance;
	newGroup.instanceResource = dx_->CreateBufferResource(dx_->GetDevice(), instanceBufferSize);
	newGroup.instanceSrvIndex = srvMgr_->Allocate();
	srvMgr_->CreateSRVforStructureBuffer(newGroup.instanceSrvIndex, newGroup.instanceResource.Get(), kMaxInstance, sizeof(Matrix4x4));

	HRESULT hr = newGroup.instanceResource->Map(0, nullptr, &newGroup.instanceMappedPtr);
	assert(SUCCEEDED(hr));
	newGroup.instanceCount = 0;

	particleGroups[name] = std::move(newGroup);
}

void ParticleManager::Emit(const std::string name, const Vector3& position, uint32_t count)
{
	assert(particleGroups.find(name) != particleGroups.end());
	ParticleGroup& group = particleGroups[name];

	std::uniform_real_distribution<float> u01(0.0f, 1.0f);
	auto rand01 = [&] { return u01(rng_); };

	std::uniform_real_distribution<float> speedDist(0.08f, 0.15f);
	std::uniform_real_distribution<float> scaleDist(0.10f, 0.20f);
	std::uniform_real_distribution<float> lifeDist(1.0f, 2.0f);

	if (name == "default")
	{
		speedDist = std::uniform_real_distribution<float>{ 0.01f, 0.10f };
		scaleDist = std::uniform_real_distribution<float>{ 0.5f, 0.5f };
		lifeDist = std::uniform_real_distribution<float>{ 0.5f , 1.0f };
	}

	auto randomDirOnSphere = [&]() {
		float u = rand01();
		float v = rand01();
		float cosT = 2.0f * u - 1.0f;
		float sinT = std::sqrt(std::max(0.0f, 1.0f - cosT * cosT));
		float phi = 6.283185307f * v;
		return Vector3{ sinT * std::cos(phi), cosT, sinT * std::sin(phi) };
	};

	for (uint32_t i = 0; i < count; ++i)
	{
		Particle p{};
		p.position = position;
		p.lifeTime = lifeDist(rng_);
		p.current = 0.0f;
		p.color = { 1,1,1,0.1f };
		p.scale = scaleDist(rng_);

		if (name == "default" || name == "defaultMesh")
		{
			Vector3 dir = randomDirOnSphere();
			float spd = speedDist(rng_);
			p.velocity = { dir.x * spd, dir.y * spd, dir.z * spd };
		}
		else
		{
			float spd = speedDist(rng_);
			p.velocity = { 0.0f, spd, 0.0f };
		}

		group.particles.push_back(p);
	}
}

void ParticleManager::Finalize()
{
	for (auto& [name, group] : particleGroups)
	{
		if (!group.useMesh)
		{
			if (group.instanceResource)
			{
				if (group.instanceMappedPtr)
				{
					group.instanceResource->Unmap(0, nullptr);
					group.instanceMappedPtr = nullptr;
				}
				group.instanceResource.Reset();
			}
		}
	}

	if (meshTransformCB_)
	{
		if (meshTransformCBBase_)
		{
			meshTransformCB_->Unmap(0, nullptr);
			meshTransformCBBase_ = nullptr;
			meshTransformPtr_ = nullptr;
		}
		meshTransformCB_.Reset();
	}
	if (meshLightCB_)
	{
		if (meshLightPtr_)
		{
			meshLightCB_->Unmap(0, nullptr);
			meshLightPtr_ = nullptr;
		}
		meshLightCB_.Reset();
	}

	vertexBuffer_.Reset();
	pso_.Reset();
	rootSignature_.Reset();
	particleGroups.clear();
}

void ParticleManager::CreateParticleGroupFromModel(const std::string& name, const std::string& modelPath)
{
	assert(particleGroups.find(name) == particleGroups.end());
	ParticleGroup g{};
	g.useMesh = false; // メッシュ粒子は未対応
	particleGroups.emplace(name, std::move(g));
}