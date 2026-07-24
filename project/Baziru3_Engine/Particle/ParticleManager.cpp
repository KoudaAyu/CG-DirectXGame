#include"ParticleManager.h"
#include"ParticleEmitter.h"
#include"Ring.h"
#include"TextureManager.h"

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

void ParticleManager::Initialize(Camera* camera)
{
	camera_ = camera;
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
	SRVManager* srvManager = TextureManager::GetInstance()->GetSRVManager();
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
	// 1. パティクル1024個分のDEFAULTヒープバッファを作成
	D3D12_HEAP_PROPERTIES heapProps{};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC resDesc{};
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resDesc.Width = sizeof(ParticleCS) * 1024;
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
		1024,
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

	// === GPU Particle カウンタ用リソースと UAV の作成 ===
	D3D12_HEAP_PROPERTIES counterHeapProps{};
	counterHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC counterResDesc{};
	counterResDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	counterResDesc.Width = sizeof(int32_t); // 4バイト
	counterResDesc.Height = 1;
	counterResDesc.DepthOrArraySize = 1;
	counterResDesc.MipLevels = 1;
	counterResDesc.SampleDesc.Count = 1;
	counterResDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	counterResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	dxCommon->GetDevice()->CreateCommittedResource(
		&counterHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&counterResDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&freeCounterResource_)
	);

	freeCounterUavIndex_ = srvManager->Allocate();
	freeCounterUavHandle_.first = dxCommon->GetSRVHandleCPU(freeCounterUavIndex_);
	freeCounterUavHandle_.second = dxCommon->GetSRVHandleGPU(freeCounterUavIndex_);
	dxCommon->CreateUnroaderedAccessView(
		freeCounterResource_,
		1, // 要素数1
		sizeof(int32_t),
		freeCounterUavHandle_.first
	);

	// GPU Particle FreeList 用リソースと UAV の作成 (スライド2枚目)
	D3D12_HEAP_PROPERTIES freeListHeapProps{};
	freeListHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC freeListResDesc{};
	freeListResDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	freeListResDesc.Width = sizeof(uint32_t) * 1024;
	freeListResDesc.Height = 1;
	freeListResDesc.DepthOrArraySize = 1;
	freeListResDesc.MipLevels = 1;
	freeListResDesc.SampleDesc.Count = 1;
	freeListResDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	freeListResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	dxCommon->GetDevice()->CreateCommittedResource(
		&freeListHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&freeListResDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&freeListResource_)
	);

	freeListUavIndex_ = srvManager->Allocate();
	freeListUavHandle_.first = dxCommon->GetSRVHandleCPU(freeListUavIndex_);
	freeListUavHandle_.second = dxCommon->GetSRVHandleGPU(freeListUavIndex_);
	dxCommon->CreateUnroaderedAccessView(
		freeListResource_,
		1024,
		sizeof(uint32_t),
		freeListUavHandle_.first
	);

	// 4. PerView 用定数バッファの作成と Map (スライド5枚目)
	perViewResource_ = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), sizeof(PerView));
	perViewResource_->Map(0, nullptr, reinterpret_cast<void**>(&perViewData_));

	// PerFrame 用定数バッファの作成と Map
	perFrameResource_ = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), sizeof(PerFrame));
	perFrameResource_->Map(0, nullptr, reinterpret_cast<void**>(&perFrameData_));

	// GPUエミッターの初期化
	gpuEmitter_ = std::make_unique<ParticleEmitter>();
	gpuEmitter_->Initialize(dxCommon);

	// Compute Pipeline の作成と初期化CSの実行
	CreateComputePipelineState();

	ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList().Get();
	
	// UAVを割り当て・バインドするためにSRVManager経由でディスクリプタヒープを設定する
	srvManager->PreDraw();

	D3D12_RESOURCE_BARRIER barriers[3]{};
	barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barriers[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barriers[0].Transition.pResource = gpuParticleResource_.Get();
	barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
	barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barriers[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barriers[1].Transition.pResource = freeCounterResource_.Get();
	barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
	barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	barriers[2].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barriers[2].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barriers[2].Transition.pResource = freeListResource_.Get();
	barriers[2].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
	barriers[2].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barriers[2].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	commandList->ResourceBarrier(3, barriers);

	commandList->SetComputeRootSignature(computeRootSignature_.Get());
	commandList->SetPipelineState(computePipelineState_.Get());
	commandList->SetComputeRootDescriptorTable(0, gpuParticleUavHandle_.second);
	commandList->SetComputeRootDescriptorTable(1, freeCounterUavHandle_.second);
	commandList->SetComputeRootDescriptorTable(2, freeListUavHandle_.second);
	uint32_t groupCount = (kMaxGPUParticles + 1023) / 1024;
	commandList->Dispatch(groupCount, 1, 1);

	barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

	barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

	barriers[2].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barriers[2].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

	commandList->ResourceBarrier(3, barriers);
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
        SRVManager* srvManager = TextureManager::GetInstance()->GetSRVManager();
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
            if (freeCounterUavIndex_ >= 3 && freeCounterUavIndex_ < SRVManager::kMaxSRVCount)
            {
                srvManager->Free(freeCounterUavIndex_);
            }
            if (freeListUavIndex_ >= 3 && freeListUavIndex_ < SRVManager::kMaxSRVCount)
            {
                srvManager->Free(freeListUavIndex_);
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
    emitRootSignature_.Reset();
    emitPipelineState_.Reset();
    emitShaderBlob_.Reset();
    gpuEmitter_.reset();
    gpuParticleResource_.Reset();
    freeCounterResource_.Reset();
    freeListResource_.Reset();
    perViewResource_.Reset();
    perFrameResource_.Reset();
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
	if (gpuEmitter_)
	{
		gpuEmitter_->Update(deltaTime);
	}

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

	if (perFrameData_)
	{
		perFrameData_->time = accumulatedTime;
		perFrameData_->deltaTime = deltaTime;
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

			Vector3 rot = it->transform.GetRotate();
			Matrix4x4 rotZ = MakeRotateZMatrix(rot.z);
			Matrix4x4 finalRotation = Multiply(rotZ, billboardMatrix);
			Matrix4x4 worldMatrix = MakeAffineMatrix(
				it->transform.GetScale(), finalRotation, it->transform.GetTranslate());
			Matrix4x4 wvpMatrix = Multiply(
				worldMatrix, Multiply(camera_->GetViewMatrix(), camera_->GetProjectionMatrix()));

			if (writeIndex < kNumMaxInstances)
			{
				instanceData[writeIndex].WVP = wvpMatrix;
				instanceData[writeIndex].World = worldMatrix;
				instanceData[writeIndex].color = it->color;
				float alpha = 1.0f - (it->currentTime / it->lifeTime);
				instanceData[writeIndex].color.w = alpha;
				instanceData[writeIndex].textureIndex = it->textureIndex;
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

   auto buildInstanceGroups = [&](uint32_t start, uint32_t count, std::vector<InstanceGroup>& outGroups)
	{
		outGroups.clear();
		if (count == 0)
		{
			return;
		}

		uint32_t curStart = start;
		uint32_t curTex = instanceData[start].textureIndex;
		for (uint32_t i = start + 1; i < start + count; ++i)
		{
			if (instanceData[i].textureIndex != curTex)
			{
				InstanceGroup g;
				g.textureIndex = curTex;
				g.start = curStart;
				g.count = i - curStart;
				if (curTex == TextureManager::kInvalidTextureIndex)
				{
					g.srvHandle = {};
				}
				else
				{
					g.srvHandle = TextureManager::GetInstance()->GetSrvHandleGPU(curTex);
				}
				outGroups.push_back(g);

				curStart = i;
				curTex = instanceData[i].textureIndex;
			}
		}

		InstanceGroup g;
		g.textureIndex = curTex;
		g.start = curStart;
		g.count = start + count - curStart;
		if (curTex == TextureManager::kInvalidTextureIndex)
		{
			g.srvHandle = {};
		}
		else
		{
			g.srvHandle = TextureManager::GetInstance()->GetSrvHandleGPU(curTex);
		}
		outGroups.push_back(g);
	};

	buildInstanceGroups(normalStart, normalInstanceCount_, normalInstanceGroups_);
	buildInstanceGroups(effectStart, effectInstanceCount_, effectInstanceGroups_);
	instanceGroups.clear();
	instanceGroups.insert(instanceGroups.end(), normalInstanceGroups_.begin(), normalInstanceGroups_.end());
	instanceGroups.insert(instanceGroups.end(), effectInstanceGroups_.begin(), effectInstanceGroups_.end());

	// Update CSの実行
	ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList().Get();

	D3D12_RESOURCE_BARRIER transitionBarriers[3]{};
	transitionBarriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	transitionBarriers[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	transitionBarriers[0].Transition.pResource = gpuParticleResource_.Get();
	transitionBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	transitionBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	transitionBarriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	transitionBarriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	transitionBarriers[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	transitionBarriers[1].Transition.pResource = freeCounterResource_.Get();
	transitionBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	transitionBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	transitionBarriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	transitionBarriers[2].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	transitionBarriers[2].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	transitionBarriers[2].Transition.pResource = freeListResource_.Get();
	transitionBarriers[2].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	transitionBarriers[2].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	transitionBarriers[2].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	commandList->ResourceBarrier(3, transitionBarriers);

	SRVManager* srvManager = TextureManager::GetInstance()->GetSRVManager();
	if (srvManager)
	{
		srvManager->PreDraw();
	}

	// 1. EmitParticle CSの実行
	if (emitPipelineState_ && gpuEmitter_)
	{
		commandList->SetComputeRootSignature(emitRootSignature_.Get());
		commandList->SetPipelineState(emitPipelineState_.Get());
		commandList->SetComputeRootConstantBufferView(0, gpuEmitter_->GetGPUVirtualAddress());
		commandList->SetComputeRootConstantBufferView(1, perFrameResource_->GetGPUVirtualAddress());
		commandList->SetComputeRootDescriptorTable(2, gpuParticleUavHandle_.second);
		commandList->SetComputeRootDescriptorTable(3, freeCounterUavHandle_.second);
		commandList->SetComputeRootDescriptorTable(4, freeListUavHandle_.second);
		commandList->Dispatch(1, 1, 1);

		// UAVバリアを挿入して、Emitの書き込みがUpdateの読み書きと競合するのを防ぐ
		D3D12_RESOURCE_BARRIER uavBarriers[3]{};
		uavBarriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		uavBarriers[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		uavBarriers[0].UAV.pResource = gpuParticleResource_.Get();

		uavBarriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		uavBarriers[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		uavBarriers[1].UAV.pResource = freeCounterResource_.Get();

		uavBarriers[2].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		uavBarriers[2].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		uavBarriers[2].UAV.pResource = freeListResource_.Get();

		commandList->ResourceBarrier(3, uavBarriers);
	}

	// 2. UpdateParticle CSの実行
	commandList->SetComputeRootSignature(updateRootSignature_.Get());
	commandList->SetPipelineState(updatePipelineState_.Get());
	commandList->SetComputeRootConstantBufferView(0, perViewResource_->GetGPUVirtualAddress());
	commandList->SetComputeRootDescriptorTable(1, gpuParticleUavHandle_.second);
	commandList->SetComputeRootDescriptorTable(2, freeCounterUavHandle_.second);
	commandList->SetComputeRootDescriptorTable(3, freeListUavHandle_.second);
	commandList->Dispatch(1, 1, 1);

	transitionBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	transitionBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

	transitionBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	transitionBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

	transitionBarriers[2].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	transitionBarriers[2].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

	commandList->ResourceBarrier(3, transitionBarriers);
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
	if (ctx.camera && ctx.camera->GetCameraResource())
	{
		commandList->SetGraphicsRootConstantBufferView(RootParam::Particle::kCamera, ctx.camera->GetCameraResource()->GetGPUVirtualAddress());
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

	commandList->DrawInstanced(vc, kMaxGPUParticles, 0, 0);
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

	//--ノーマルブレンド------------------------------
	/*blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;*/
	//--------------------------------------------

	//--加算ブレンド------------------------------
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	//--------------------------------------------

	//--減算ブレンド------------------------------
	/*blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;*/
	//--------------------------------------------

	//--乗算ブレンド------------------------------
	/*blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_DEST_COLOR;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;*/
	//--------------------------------------------

	//--スクリーン合成------------------------------
	/*blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_INV_DEST_COLOR;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;*/
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

	D3D12_ROOT_PARAMETER computeParams[3] = {};

	D3D12_DESCRIPTOR_RANGE uavRange0{};
	uavRange0.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	uavRange0.NumDescriptors = 1;
	uavRange0.BaseShaderRegister = 0; // u0
	uavRange0.RegisterSpace = 0;
	uavRange0.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	computeParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	computeParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	computeParams[0].DescriptorTable.pDescriptorRanges = &uavRange0;
	computeParams[0].DescriptorTable.NumDescriptorRanges = 1;

	D3D12_DESCRIPTOR_RANGE uavRange1{};
	uavRange1.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	uavRange1.NumDescriptors = 1;
	uavRange1.BaseShaderRegister = 1; // u1
	uavRange1.RegisterSpace = 0;
	uavRange1.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	computeParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	computeParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	computeParams[1].DescriptorTable.pDescriptorRanges = &uavRange1;
	computeParams[1].DescriptorTable.NumDescriptorRanges = 1;

	D3D12_DESCRIPTOR_RANGE uavRange2{};
	uavRange2.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	uavRange2.NumDescriptors = 1;
	uavRange2.BaseShaderRegister = 2; // u2
	uavRange2.RegisterSpace = 0;
	uavRange2.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	computeParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	computeParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	computeParams[2].DescriptorTable.pDescriptorRanges = &uavRange2;
	computeParams[2].DescriptorTable.NumDescriptorRanges = 1;

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

	D3D12_ROOT_PARAMETER updateParams[4] = {};
	// b0: PerView (CBV)
	updateParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	updateParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	updateParams[0].Descriptor.ShaderRegister = 0; // b0

	// u0: gParticles (UAV)
	D3D12_DESCRIPTOR_RANGE updateUavRange0{};
	updateUavRange0.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	updateUavRange0.NumDescriptors = 1;
	updateUavRange0.BaseShaderRegister = 0; // u0
	updateUavRange0.RegisterSpace = 0;
	updateUavRange0.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	updateParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	updateParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	updateParams[1].DescriptorTable.pDescriptorRanges = &updateUavRange0;
	updateParams[1].DescriptorTable.NumDescriptorRanges = 1;

	// u1: gFreeListIndex (UAV)
	D3D12_DESCRIPTOR_RANGE updateUavRange1{};
	updateUavRange1.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	updateUavRange1.NumDescriptors = 1;
	updateUavRange1.BaseShaderRegister = 1; // u1
	updateUavRange1.RegisterSpace = 0;
	updateUavRange1.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	updateParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	updateParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	updateParams[2].DescriptorTable.pDescriptorRanges = &updateUavRange1;
	updateParams[2].DescriptorTable.NumDescriptorRanges = 1;

	// u2: gFreeList (UAV)
	D3D12_DESCRIPTOR_RANGE updateUavRange2{};
	updateUavRange2.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	updateUavRange2.NumDescriptors = 1;
	updateUavRange2.BaseShaderRegister = 2; // u2
	updateUavRange2.RegisterSpace = 0;
	updateUavRange2.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	updateParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	updateParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	updateParams[3].DescriptorTable.pDescriptorRanges = &updateUavRange2;
	updateParams[3].DescriptorTable.NumDescriptorRanges = 1;

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

	// Emit Shader 用の PipelineState 作成
	emitShaderBlob_ = dxCommon->CompileShader(L"Resources/shaders/EmitParticle.CS.hlsl",
		L"cs_6_0", dxCommon->GetDxcUtils().Get(), dxCommon->GetDxcCompiler(), dxCommon->GetIncludeHandler(), logStream);
	assert(emitShaderBlob_ != nullptr);

	D3D12_ROOT_SIGNATURE_DESC emitRootSigDesc{};
	emitRootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	D3D12_ROOT_PARAMETER emitParams[5] = {};
	// b0: EmitterSphere (CBV)
	emitParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	emitParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	emitParams[0].Descriptor.ShaderRegister = 0; // b0

	// b1: PerFrame (CBV)
	emitParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	emitParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	emitParams[1].Descriptor.ShaderRegister = 1; // b1

	// u0: gParticles (UAV)
	D3D12_DESCRIPTOR_RANGE emitUavRange0{};
	emitUavRange0.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	emitUavRange0.NumDescriptors = 1;
	emitUavRange0.BaseShaderRegister = 0; // u0
	emitUavRange0.RegisterSpace = 0;
	emitUavRange0.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	emitParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	emitParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	emitParams[2].DescriptorTable.pDescriptorRanges = &emitUavRange0;
	emitParams[2].DescriptorTable.NumDescriptorRanges = 1;

	// u1: gFreeListIndex (UAV)
	D3D12_DESCRIPTOR_RANGE emitUavRange1{};
	emitUavRange1.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	emitUavRange1.NumDescriptors = 1;
	emitUavRange1.BaseShaderRegister = 1; // u1
	emitUavRange1.RegisterSpace = 0;
	emitUavRange1.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	emitParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	emitParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	emitParams[3].DescriptorTable.pDescriptorRanges = &emitUavRange1;
	emitParams[3].DescriptorTable.NumDescriptorRanges = 1;

	// u2: gFreeList (UAV)
	D3D12_DESCRIPTOR_RANGE emitUavRange2{};
	emitUavRange2.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	emitUavRange2.NumDescriptors = 1;
	emitUavRange2.BaseShaderRegister = 2; // u2
	emitUavRange2.RegisterSpace = 0;
	emitUavRange2.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	emitParams[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	emitParams[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	emitParams[4].DescriptorTable.pDescriptorRanges = &emitUavRange2;
	emitParams[4].DescriptorTable.NumDescriptorRanges = 1;

	emitRootSigDesc.pParameters = emitParams;
	emitRootSigDesc.NumParameters = _countof(emitParams);

	Microsoft::WRL::ComPtr<ID3DBlob> emitSigBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> emitErrorBlob = nullptr;
	dxCommon->SetHr(D3D12SerializeRootSignature(&emitRootSigDesc,
		D3D_ROOT_SIGNATURE_VERSION_1, &emitSigBlob, &emitErrorBlob));

	if (FAILED(dxCommon->GetHr()))
	{
		if (emitErrorBlob)
		{
			Logger::Log(logStream, reinterpret_cast<char*>(emitErrorBlob->GetBufferPointer()));
		}
		assert(false);
	}

	dxCommon->SetHr(dxCommon->GetDevice()->CreateRootSignature(0,
		emitSigBlob->GetBufferPointer(), emitSigBlob->GetBufferSize(),
		IID_PPV_ARGS(&emitRootSignature_)));
	assert(SUCCEEDED(dxCommon->GetHr()));

	D3D12_COMPUTE_PIPELINE_STATE_DESC emitPipelineStateDesc{};
	emitPipelineStateDesc.CS = {
		emitShaderBlob_->GetBufferPointer(),
		emitShaderBlob_->GetBufferSize()
	};
	emitPipelineStateDesc.pRootSignature = emitRootSignature_.Get();

	dxCommon->SetHr(dxCommon->GetDevice()->CreateComputePipelineState(
		&emitPipelineStateDesc,
		IID_PPV_ARGS(&emitPipelineState_)
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
