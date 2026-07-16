#include"ParticleManager.h"
#include"ParticleEmitter.h"
#include"Ring.h"
#include"TextureManager.h"
#include<imgui.h>

#include "RootParam.h"

#include "Camera.h"
#include "Light.h"

ParticleManager* ParticleManager::instance_ = nullptr;

ParticleManager::ParticleManager(std::ostream& logStream, DirectXCom* dxCommon)
	: logStream(logStream), dxCommon(dxCommon)
{
	instance_ = this;
}



ParticleManager::~ParticleManager()
{
	if (instance_ == this)
	{
		instance_ = nullptr;
	}
}

void ParticleManager::Initialize(Camera* camera, TextureManager* textureManager)
{
	camera_ = camera;
	textureManager_ = textureManager ? textureManager : TextureManager::GetInstance();
	SetupDraw(dxCommon->GetCommandList().Get());

    Random::SeedEngine();


    particles.clear();

   ring_ = std::make_unique<Ring>();
	ring_->Initialize(dxCommon);

	instancingResource =
		dxCommon->CreateBufferResource(dxCommon->GetDevice(), sizeof(ParticleManager::ParticleForGPU) * kNumMaxInstances);

	instancingResource->Map(0, nullptr, reinterpret_cast<void**>(&instanceData));


	{
		uint32_t writeIndex = 0;
		for (const auto& p : particles)
		{
			if (writeIndex >= kNumMaxInstances) { break; }
			instanceData[writeIndex].WVP = MakeIdentity4x4();
			instanceData[writeIndex].World = MakeIdentity4x4();
			instanceData[writeIndex].color = p.color;
			++writeIndex;
		}

		for (; writeIndex < kNumMaxInstances; ++writeIndex)
		{
			instanceData[writeIndex].WVP = MakeIdentity4x4();
			instanceData[writeIndex].World = MakeIdentity4x4();
			instanceData[writeIndex].color = { 0,0,0,0 };
			instanceData[writeIndex].textureIndex = TextureManager::kInvalidTextureIndex;
		}
	}

	// インスタンシング用 StructuredBuffer の SRV を SRVManager 経由で作成する
	SRVManager* srvManager = textureManager_->GetSRVManager();
	assert(srvManager);

	instancingSrvIndex_ = srvManager->Allocate();
	// StructuredBuffer 用 SRV を生成
	srvManager->CreateSRVForStructuredBuffer(
		instancingSrvIndex_,
		instancingResource.Get(),
		kNumMaxInstances,
		sizeof(ParticleManager::ParticleForGPU));

	// Draw で利用する GPU ハンドルを保持
	instancingSrvHandleGPU = srvManager->GetGPUDescriptorHandle(instancingSrvIndex_);

	// === GPU Particle 用リソースと UAV/SRV の作成 (スライド3枚目) ===
	// 1. パティクル個数分のDEFAULTヒープバッファを作成
	D3D12_HEAP_PROPERTIES heapProps{};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC resDesc{};
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resDesc.Width = sizeof(ParticleCS) * kMaxGPUParticles;
	resDesc.Height = 1;
	resDesc.DepthOrArraySize = 1;
	resDesc.MipLevels = 1;
	resDesc.SampleDesc.Count = 1;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS; // UAV用のフラグを立てる

	dxCommon->GetDevice()->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&gpuParticleResource_)
	);

	// 2. UAV のディスクリプタを割り当てて作成
	gpuParticleUavIndex_ = srvManager->Allocate();
	gpuParticleUavHandle_.first = dxCommon->GetSRVHandleCPU(gpuParticleUavIndex_);
	gpuParticleUavHandle_.second = dxCommon->GetSRVHandleGPU(gpuParticleUavIndex_);
	dxCommon->CreateUnroaderedAccessView(
		gpuParticleResource_,
		kMaxGPUParticles,
		sizeof(ParticleCS),
		gpuParticleUavHandle_.first
	);

	// 3. SRV のディスクリプタを割り当てて作成
	gpuParticleSrvIndex_ = srvManager->Allocate();
	gpuParticleSrvHandle_.first = dxCommon->GetSRVHandleCPU(gpuParticleSrvIndex_);
	gpuParticleSrvHandle_.second = dxCommon->GetSRVHandleGPU(gpuParticleSrvIndex_);
	srvManager->CreateSRVForStructuredBuffer(
		gpuParticleSrvIndex_,
		gpuParticleResource_.Get(),
		kMaxGPUParticles,
		sizeof(ParticleCS)
	);

	// 4. PerView 用定数バッファの作成と Map (スライド5枚目)
	perViewResource_ = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), sizeof(PerView));
	perViewResource_->Map(0, nullptr, reinterpret_cast<void**>(&perViewData_));

	// Compute Pipeline の作成と初期化CSの実行
	CreateComputePipelineState();

	ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList().Get();
	
	// UAVを割り当て・バインドするためにSRVManager経由でディスクリプタヒープを設定する
	srvManager->PreDraw();

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = gpuParticleResource_.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &barrier);

	commandList->SetComputeRootSignature(computeRootSignature_.Get());
	commandList->SetPipelineState(computePipelineState_.Get());
	commandList->SetComputeRootDescriptorTable(0, gpuParticleUavHandle_.second);
	uint32_t groupCount = (kMaxGPUParticles + 1023) / 1024;
	commandList->Dispatch(groupCount, 1, 1);

	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	commandList->ResourceBarrier(1, &barrier);
}

void ParticleManager::Finalize()
{
    if (finalized_) return;
    finalized_ = true;

   
    if (instancingResource && instanceData != nullptr)
    {
        D3D12_RANGE writtenRange = { 0, static_cast<SIZE_T>(sizeof(ParticleManager::ParticleForGPU) * kNumMaxInstances) };
        instancingResource->Unmap(0, &writtenRange);
        instanceData = nullptr;
    }

    
    try
    {
        SRVManager* srvManager = textureManager_->GetSRVManager();
        if (srvManager)
        {
            if (instancingSrvIndex_ >= 3 && instancingSrvIndex_ < SRVManager::kMaxSRVCount)
            {
                srvManager->Free(instancingSrvIndex_);
            }
            if (gpuParticleUavIndex_ >= 3 && gpuParticleUavIndex_ < SRVManager::kMaxSRVCount)
            {
                srvManager->Free(gpuParticleUavIndex_);
            }
            if (gpuParticleSrvIndex_ >= 3 && gpuParticleSrvIndex_ < SRVManager::kMaxSRVCount)
            {
                srvManager->Free(gpuParticleSrvIndex_);
            }
        }
    }
    catch (...) {}

    instancingSrvHandleGPU = {};
    instancingSrvIndex_ = 0;

   
    instancingResource.Reset();
    pipelineState.Reset();
    rootSignature.Reset();
    computeRootSignature_.Reset();
    computePipelineState_.Reset();
    computeShaderBlob_.Reset();
    updateRootSignature_.Reset();
    updatePipelineState_.Reset();
    updateShaderBlob_.Reset();
    gpuParticleResource_.Reset();
    perViewResource_.Reset();
    signatureBlob.Reset();
    errorBlob.Reset();
    vertexShaderBlob.Reset();
    pixelShaderBlob.Reset();
   if (ring_)
	{
		ring_->Finalize();
		ring_.reset();
	}

   
    particles.clear();
	effectParticles.clear();
    numInstance = 0;
    writeIndex = 0;
    normalInstanceCount_ = 0;
	effectInstanceCount_ = 0;
    instanceGroups.clear();
    normalInstanceGroups_.clear();
	effectInstanceGroups_.clear();

    Logger::Log(logStream, "ParticleManager finalized\n");
}

void ParticleManager::ClearParticles()
{
	particles.clear();
	effectParticles.clear();
	numInstance = 0;
	writeIndex = 0;
 normalInstanceCount_ = 0;
	effectInstanceCount_ = 0;
	instanceGroups.clear();
	normalInstanceGroups_.clear();
	effectInstanceGroups_.clear();

	if (instanceData)
	{
		for (uint32_t i = 0; i < kNumMaxInstances; ++i)
		{
			instanceData[i].WVP = MakeIdentity4x4();
			instanceData[i].World = MakeIdentity4x4();
			instanceData[i].color = { 0,0,0,0 };
			instanceData[i].textureIndex = TextureManager::kInvalidTextureIndex;
		}
	}
}

void ParticleManager::AddParticles(std::list<Particle>& newParticles)
{
	
	for (auto& p : newParticles)
	{
		particles.push_back(std::move(p));
	}
}

void ParticleManager::AddEffectParticles(std::list<Particle>& newParticles)
{
	for (auto& p : newParticles)
	{
		effectParticles.push_back(std::move(p));
	}
}

void ParticleManager::Update(float deltaTime)
{
	numInstance = 0;
	writeIndex = 0;
	normalInstanceCount_ = 0;
	effectInstanceCount_ = 0;

	Matrix4x4 backToFrontMatrix = MakeRotateYMatrix(0.0f);
	Matrix4x4 billboardMatrix = Multiply(backToFrontMatrix, camera_->GetWorldMatrix());
	billboardMatrix.m[3][0] = 0.0f;
	billboardMatrix.m[3][1] = 0.0f;
	billboardMatrix.m[3][2] = 0.0f;

	Matrix4x4 viewProjection = Multiply(camera_->GetViewMatrix(), camera_->GetProjectionMatrix());

	static float accumulatedTime = 0.0f;
	accumulatedTime += deltaTime;

	if (perViewData_)
	{
		perViewData_->viewProjection = viewProjection;
		perViewData_->billboardMatrix = billboardMatrix;
		perViewData_->deltaTime = deltaTime;
		perViewData_->time = accumulatedTime;
		perViewData_->maxParticles = kMaxGPUParticles;
	}

	auto updateParticleList = [&](std::list<Particle>& targetParticles)
	{
		auto it = targetParticles.begin();
		while (it != targetParticles.end())
		{
			it->currentTime += deltaTime;

			if (it->currentTime >= it->lifeTime)
			{
				it = targetParticles.erase(it);
				continue;
			}

			if (writeIndex < kMaxGPUParticles)
			{
				++writeIndex;
			}

			it->transform.SetTranslate(
				it->transform.GetTranslate() + it->velocity * deltaTime);

			++it;
		}
	};

	const uint32_t normalStart = writeIndex;
	updateParticleList(particles);
	normalInstanceCount_ = writeIndex - normalStart;

	const uint32_t effectStart = writeIndex;
	updateParticleList(effectParticles);
	effectInstanceCount_ = writeIndex - effectStart;
	numInstance = writeIndex;

	// Update CSの実行
	ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList().Get();

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = gpuParticleResource_.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &barrier);

	SRVManager* srvManager = textureManager_->GetSRVManager();
	if (srvManager)
	{
		srvManager->PreDraw();
	}

	commandList->SetComputeRootSignature(updateRootSignature_.Get());
	commandList->SetPipelineState(updatePipelineState_.Get());
	commandList->SetComputeRootConstantBufferView(0, perViewResource_->GetGPUVirtualAddress());
	commandList->SetComputeRootDescriptorTable(1, gpuParticleUavHandle_.second);
	uint32_t groupCount = (kMaxGPUParticles + 1023) / 1024;
	commandList->Dispatch(groupCount, 1, 1);

	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	commandList->ResourceBarrier(1, &barrier);
}

void ParticleManager::Draw(ID3D12GraphicsCommandList* commandList, const RenderContext& ctx, UINT vertexCount)
{
	if (!commandList) return;

	// PSOとルートシグネチャをセット
	SetupDraw(commandList);

	// マテリアルCBVとインスタンシング用SRVをセット
	commandList->SetGraphicsRootConstantBufferView(RootParam::Particle::kMaterial, ctx.materialGPUAddress);
	
	// GPUパーティクル用の SRV をルートパラメーターにバインド
	commandList->SetGraphicsRootDescriptorTable(RootParam::Particle::kInstancing, gpuParticleSrvHandle_.second);

	// テクスチャ
	if (ctx.textureHandle.ptr != 0)
	{
		commandList->SetGraphicsRootDescriptorTable(RootParam::Particle::kTextureTable, ctx.textureHandle);
	}
	
	// Light
	if (ctx.light)
	{
		commandList->SetGraphicsRootConstantBufferView(RootParam::Particle::kLight, ctx.light->GetDirectionalLightResource()->GetGPUVirtualAddress());
	}
	
	// Camera
	if (ctx.camera && ctx.camera->GetCameraGpuAddress() != 0)
	{
		commandList->SetGraphicsRootConstantBufferView(RootParam::Particle::kCamera, ctx.camera->GetCameraGpuAddress());
	}
	
	// PerView
	commandList->SetGraphicsRootConstantBufferView(RootParam::Particle::kPerView, perViewResource_->GetGPUVirtualAddress());

	// バーテックスバッファをバインド
	if (vertexCount == 0)
	{
		if (ring_ && ring_->GetVertexBufferView().SizeInBytes > 0)
		{
			const auto& ringVertexBufferView = ring_->GetVertexBufferView();
			commandList->IASetVertexBuffers(0, 1, &ringVertexBufferView);
		}
	}

	UINT vc = vertexCount;
	if (vc == 0)
	{
		vc = (ring_ && ring_->GetVertexCount() > 0) ? ring_->GetVertexCount() : 6u;
	}

	commandList->DrawInstanced(vc, numInstance, 0, 0);
}

ParticleManager::Particle ParticleManager::MakeNewParticles(std::mt19937& randomEngine, const Vector3& translate)
{
	std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
	Particle particle;
	particle.transform.SetScale({ 1.0f,1.0f,1.0f });
	particle.transform.SetRotate({ 0.0f,0.0f,0.0f });
	Vector3 randomTranslate{ distribution(randomEngine), distribution(randomEngine), distribution(randomEngine) };
	particle.transform.SetTranslate({ translate + randomTranslate });
	particle.velocity = { distribution(randomEngine), distribution(randomEngine), distribution(randomEngine) };
	std::uniform_real_distribution<float> distColor(0.0f, 1.0f);
	particle.color = { distColor(randomEngine), distColor(randomEngine), distColor(randomEngine), 1.0f };
	std::uniform_real_distribution<float> distTime(1.0f, 3.0f);
	particle.lifeTime = distTime(randomEngine);
	particle.currentTime = 0.0f;
	return particle;
}

ParticleManager::Particle ParticleManager::MakeHieEffect(std::mt19937& randomEngine, const Vector3& translate)
{
    Particle particle;

    // ランダム回転
    std::uniform_real_distribution<float> distRotate(0.0f, std::numbers::pi_v<float> * 2.0f);
    float rotZ = distRotate(randomEngine);
    particle.transform.SetRotate({ 0.0f, 0.0f, rotZ });

    // 縦方向にばらつきを持たせた縦長パーティクル
    std::uniform_real_distribution<float> distScaleX(0.03f, 0.08f); // 横幅
    std::uniform_real_distribution<float> distScaleY(0.6f, 1.6f);   // 縦幅
    float sx = distScaleX(randomEngine);
    float sy = distScaleY(randomEngine);
    particle.transform.SetScale({ sx, sy, 1.0f }); // Z は 1

    // 発生位置
    particle.transform.SetTranslate(translate);

    // velocity を 0 に固定
    particle.velocity = { 0.0f, 0.0f, 0.0f };

    // 色は白
    particle.color = { 1.0f, 1.0f, 1.0f, 1.0f };

    // 寿命に少しばらつきを持たせる
    std::uniform_real_distribution<float> distTime(0.7f, 1.3f);
    particle.lifeTime = distTime(randomEngine);
    particle.currentTime = 0.0f;

    return particle;
}



void ParticleManager::RootSignature()
{
	//RootSignatureの作成

	descriptionRootSignature.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT; //入力アセンブラでの使用を許可
}

void ParticleManager::CreateGraphicsPipeline()
{
	RootSignature();
	Descriptor();
	CreateRootParameters();
	StaticSamplers();
	SignatureBlob();
	
	if (!signatureBlob)
	{
		Logger::Log(logStream, "ParticleManager: Failed to serialize root signature. Aborting pipeline creation.\n");
		return;
	}
	RootSignatureFromBlob();
	InputLayer();
	InitializeBlend();
	RasterizerState();
	ShaderCompile();
	InitializeGraphicPipeline();
}

void ParticleManager::Descriptor()
{

	descriptorRangeForInstancing[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeForInstancing[0].NumDescriptors = 1;
	descriptorRangeForInstancing[0].BaseShaderRegister = 0; // t0
	descriptorRangeForInstancing[0].RegisterSpace = 0;
	descriptorRangeForInstancing[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;


	descriptorRangeForInstancing[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeForInstancing[1].NumDescriptors = 1;
	descriptorRangeForInstancing[1].BaseShaderRegister = 3; // t3
	descriptorRangeForInstancing[1].RegisterSpace = 0;
	descriptorRangeForInstancing[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
}

void ParticleManager::CreateRootParameters()
{
	//RootParameters生成PixelShaderのMaterialとVertexShaderのTransform

	rootParameters[RootParam::Particle::kMaterial].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;//CBVを使う
	rootParameters[RootParam::Particle::kMaterial].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; //PixelShaderで使う
	rootParameters[RootParam::Particle::kMaterial].Descriptor.ShaderRegister = 0;


	rootParameters[RootParam::Particle::kInstancing].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[RootParam::Particle::kInstancing].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[RootParam::Particle::kInstancing].DescriptorTable.pDescriptorRanges = &descriptorRangeForInstancing[0];
	rootParameters[RootParam::Particle::kInstancing].DescriptorTable.NumDescriptorRanges = 1;

	rootParameters[RootParam::Particle::kTextureTable].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[RootParam::Particle::kTextureTable].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[RootParam::Particle::kTextureTable].DescriptorTable.pDescriptorRanges = &descriptorRangeForInstancing[1];
	rootParameters[RootParam::Particle::kTextureTable].DescriptorTable.NumDescriptorRanges = 1;

	rootParameters[RootParam::Particle::kLight].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[RootParam::Particle::kLight].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[RootParam::Particle::kLight].Descriptor.ShaderRegister = 1; 

	rootParameters[RootParam::Particle::kCamera].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[RootParam::Particle::kCamera].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[RootParam::Particle::kCamera].Descriptor.ShaderRegister = 2; // b2:

	rootParameters[RootParam::Particle::kInstanceOffset].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParameters[RootParam::Particle::kInstanceOffset].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[RootParam::Particle::kInstanceOffset].Constants.ShaderRegister = 3; // b3
	rootParameters[RootParam::Particle::kInstanceOffset].Constants.RegisterSpace = 0;
	rootParameters[RootParam::Particle::kInstanceOffset].Constants.Num32BitValues = 1;

	rootParameters[RootParam::Particle::kPerView].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[RootParam::Particle::kPerView].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[RootParam::Particle::kPerView].Descriptor.ShaderRegister = 0; // b0
	rootParameters[RootParam::Particle::kPerView].Descriptor.RegisterSpace = 0;

	descriptionRootSignature.pParameters = rootParameters; //ルートパラメーター配列へのポインタ
	descriptionRootSignature.NumParameters = _countof(rootParameters);//配列の長さ
}

void ParticleManager::StaticSamplers()
{

	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;//バイアリニアフィルタ
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//0~1の範囲外をリピート
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;//比較しない
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;//ありったけのMipmapを使う
	staticSamplers[0].ShaderRegister = 0;//レジスタ番号0を使う
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//PixelShaderで使う
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);


}

void ParticleManager::SignatureBlob()
{
	//シリアライズしてバイナリにする

	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1, signatureBlob.GetAddressOf(), errorBlob.GetAddressOf());

		dxCommon->SetHr(hr);

	if (FAILED(hr))
	{
		if (errorBlob)
		{
			Logger::Log(logStream, reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		}
		else
		{
			Logger::Log(logStream, "D3D12SerializeRootSignature failed but error blob is null.\n");
		}
		
		return;
	}
}

void ParticleManager::RootSignatureFromBlob()
{
	//バイナリをもとに生成

	dxCommon->SetHr(dxCommon->GetDevice()->CreateRootSignature(0,
		signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature)));
	assert(SUCCEEDED(dxCommon->GetHr()));
}

void ParticleManager::InputLayer()
{
    // 入力レイアウトの設定

	inputElementDiscs[0].SemanticName = "POSITION"; //セマンティック名
	inputElementDiscs[0].SemanticIndex = 0; //セマンティックインデックス
	inputElementDiscs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT; //頂点のフォーマット
	inputElementDiscs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDiscs[1].SemanticName = "TEXCOORD";
	inputElementDiscs[1].SemanticIndex = 0;
	inputElementDiscs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDiscs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDiscs[2].SemanticName = "NORMAL";
	inputElementDiscs[2].SemanticIndex = 0;
	inputElementDiscs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDiscs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputLayoutDesc.pInputElementDescs = inputElementDiscs; //入力要素の配列
	inputLayoutDesc.NumElements = _countof(inputElementDiscs); //入力要素の数
}

void ParticleManager::InitializeBlend()
{
	//BlendStateの設定
	//すべての色要素を書き込む
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;

	//--加算ブレンド------------------------------
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	//--------------------------------------------


	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
}

void ParticleManager::RasterizerState()
{

	//カリングしない
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	//三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
}

void ParticleManager::ShaderCompile()
{
	//Shaderをコンパイルする
	vertexShaderBlob = dxCommon->CompileShader(L"Resources/shaders/Particle.VS.hlsl",
		L"vs_6_0", dxCommon->GetDxcUtils().Get(), dxCommon->GetDxcCompiler(), dxCommon->GetIncludeHandler(), logStream);
	assert(vertexShaderBlob != nullptr);

	pixelShaderBlob = dxCommon->CompileShader(L"Resources/shaders/Particle.PS.hlsl",
		L"ps_6_0", dxCommon->GetDxcUtils().Get(), dxCommon->GetDxcCompiler(), dxCommon->GetIncludeHandler(), logStream);
	assert(pixelShaderBlob != nullptr);
}

void ParticleManager::InitializeGraphicPipeline()
{


	graphicPipelineStateDesc.pRootSignature = rootSignature.Get(); //ルートシグネチャ
	graphicPipelineStateDesc.InputLayout = inputLayoutDesc; //入力レイアウト
	graphicPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),
	vertexShaderBlob->GetBufferSize() }; //頂点シェーダーの設定
	graphicPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(),
		pixelShaderBlob->GetBufferSize() }; //ピクセルシェーダーの設定
	graphicPipelineStateDesc.BlendState = blendDesc; //ブレンドステートの設定
	graphicPipelineStateDesc.RasterizerState = rasterizerDesc; //ラスタライザーステートの設定
	//書き込むRTVの情報
	graphicPipelineStateDesc.NumRenderTargets = 1;
	graphicPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; //RTVのフォーマット
	//利用するトロポジ(形状)のタイプ。三角形
	graphicPipelineStateDesc.PrimitiveTopologyType =
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	//どのように画面に色を打ち込むか設定(気にしなくていい？)
	graphicPipelineStateDesc.SampleDesc.Count = 1; //マルチサンプルしない
	graphicPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK; //サンプルマスクはデフォルト

	// DepthStencilState の設定（パーティクル向け）
	// 深度テストは有効にして、深度書き込みは行わない（描画順やブレンドに依存するため）
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	graphicPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	dxCommon->SetHr(dxCommon->GetDevice()->CreateGraphicsPipelineState(&graphicPipelineStateDesc,
		IID_PPV_ARGS(&pipelineState)));
	assert(SUCCEEDED(dxCommon->GetHr()));
}

void ParticleManager::CreateComputePipelineState()
{
	computeShaderBlob_ = dxCommon->CompileShader(L"Resources/shaders/InitializeParticle.CS.hlsl",
		L"cs_6_0", dxCommon->GetDxcUtils().Get(), dxCommon->GetDxcCompiler(), dxCommon->GetIncludeHandler(), logStream);
	assert(computeShaderBlob_ != nullptr);

	D3D12_ROOT_SIGNATURE_DESC computeRootSigDesc{};
	computeRootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	D3D12_DESCRIPTOR_RANGE uavRange{};
	uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	uavRange.NumDescriptors = 1;
	uavRange.BaseShaderRegister = 0; // u0
	uavRange.RegisterSpace = 0;
	uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER computeParams[1] = {};
	computeParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	computeParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	computeParams[0].DescriptorTable.pDescriptorRanges = &uavRange;
	computeParams[0].DescriptorTable.NumDescriptorRanges = 1;

	computeRootSigDesc.pParameters = computeParams;
	computeRootSigDesc.NumParameters = _countof(computeParams);

	Microsoft::WRL::ComPtr<ID3DBlob> computeSigBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> computeErrorBlob = nullptr;
	dxCommon->SetHr(D3D12SerializeRootSignature(&computeRootSigDesc,
		D3D_ROOT_SIGNATURE_VERSION_1, &computeSigBlob, &computeErrorBlob));

	if (FAILED(dxCommon->GetHr()))
	{
		if (computeErrorBlob)
		{
			Logger::Log(logStream, reinterpret_cast<char*>(computeErrorBlob->GetBufferPointer()));
		}
		assert(false);
	}

	dxCommon->SetHr(dxCommon->GetDevice()->CreateRootSignature(0,
		computeSigBlob->GetBufferPointer(), computeSigBlob->GetBufferSize(),
		IID_PPV_ARGS(&computeRootSignature_)));
	assert(SUCCEEDED(dxCommon->GetHr()));

	D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineStateDesc{};
	computePipelineStateDesc.CS = {
		computeShaderBlob_->GetBufferPointer(),
		computeShaderBlob_->GetBufferSize()
	};
	computePipelineStateDesc.pRootSignature = computeRootSignature_.Get();

	dxCommon->SetHr(dxCommon->GetDevice()->CreateComputePipelineState(
		&computePipelineStateDesc,
		IID_PPV_ARGS(&computePipelineState_)
	));
	assert(SUCCEEDED(dxCommon->GetHr()));

	// Update Shader 用の RootSignature の作成
	updateShaderBlob_ = dxCommon->CompileShader(L"Resources/shaders/UpdateParticle.CS.hlsl",
		L"cs_6_0", dxCommon->GetDxcUtils().Get(), dxCommon->GetDxcCompiler(), dxCommon->GetIncludeHandler(), logStream);
	assert(updateShaderBlob_ != nullptr);

	D3D12_ROOT_SIGNATURE_DESC updateRootSigDesc{};
	updateRootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	D3D12_DESCRIPTOR_RANGE updateUavRange{};
	updateUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	updateUavRange.NumDescriptors = 1;
	updateUavRange.BaseShaderRegister = 0; // u0
	updateUavRange.RegisterSpace = 0;
	updateUavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER updateParams[2] = {};
	// b0: PerView (CBV)
	updateParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	updateParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	updateParams[0].Descriptor.ShaderRegister = 0; // b0

	// u0: gParticles (UAV)
	updateParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	updateParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	updateParams[1].DescriptorTable.pDescriptorRanges = &updateUavRange;
	updateParams[1].DescriptorTable.NumDescriptorRanges = 1;

	updateRootSigDesc.pParameters = updateParams;
	updateRootSigDesc.NumParameters = _countof(updateParams);

	Microsoft::WRL::ComPtr<ID3DBlob> updateSigBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> updateErrorBlob = nullptr;
	dxCommon->SetHr(D3D12SerializeRootSignature(&updateRootSigDesc,
		D3D_ROOT_SIGNATURE_VERSION_1, &updateSigBlob, &updateErrorBlob));

	if (FAILED(dxCommon->GetHr()))
	{
		if (updateErrorBlob)
		{
			Logger::Log(logStream, reinterpret_cast<char*>(updateErrorBlob->GetBufferPointer()));
		}
		assert(false);
	}

	dxCommon->SetHr(dxCommon->GetDevice()->CreateRootSignature(0,
		updateSigBlob->GetBufferPointer(), updateSigBlob->GetBufferSize(),
		IID_PPV_ARGS(&updateRootSignature_)));
	assert(SUCCEEDED(dxCommon->GetHr()));

	D3D12_COMPUTE_PIPELINE_STATE_DESC updatePipelineStateDesc{};
	updatePipelineStateDesc.CS = {
		updateShaderBlob_->GetBufferPointer(),
		updateShaderBlob_->GetBufferSize()
	};
	updatePipelineStateDesc.pRootSignature = updateRootSignature_.Get();

	dxCommon->SetHr(dxCommon->GetDevice()->CreateComputePipelineState(
		&updatePipelineStateDesc,
		IID_PPV_ARGS(&updatePipelineState_)
	));
	assert(SUCCEEDED(dxCommon->GetHr()));
}

void ParticleManager::SetupDraw(ID3D12GraphicsCommandList* commandList)
{
	if (!pipelineState)
	{
		CreateGraphicsPipeline();
		assert(pipelineState != nullptr && "ParticleManager pipeline state creation failed");
	}

	
	commandList->SetGraphicsRootSignature(rootSignature.Get());
	commandList->SetPipelineState(pipelineState.Get()); // パイプラインステートを設定
	// Set primitive topology expected by the input layout / PSO
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

}

void ParticleManager::BindResources(ID3D12GraphicsCommandList* commandList, D3D12_GPU_VIRTUAL_ADDRESS materialCBV)
{
	assert(commandList != nullptr);

	commandList->SetGraphicsRootConstantBufferView(RootParam::Particle::kMaterial, materialCBV);

	// インスタンシング用の SRV をルートパラメーターにバインド
	commandList->SetGraphicsRootDescriptorTable(RootParam::Particle::kInstancing, instancingSrvHandleGPU);
}
