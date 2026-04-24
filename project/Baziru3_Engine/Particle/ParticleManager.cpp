#include"ParticleManager.h"
#include"ParticleEmitter.h"
#include"TextureManager.h"

#include "RootParam.h"

#include "Camera.h"
#include "Light.h"

ParticleManager::ParticleManager(std::ostream& logStream, DirectXCom* dxCommon)
	: logStream(logStream), dxCommon(dxCommon)
{
}



ParticleManager::~ParticleManager()
{
}

void ParticleManager::Initialize(Camera* camera)
{
	camera_ = camera;
	SetupDraw(dxCommon->GetCommandList().Get());

    Random::SeedEngine();


    particles.clear();

	// Create a ring mesh and vertex buffer used for particle drawing
	{
		const uint32_t kRingDivide = 64;
		const float kOuterRadius = 1.0f;
		const float kInnerRadius = 0.2f;
		auto verts = CreateRingMesh(kRingDivide, kOuterRadius, kInnerRadius);
		CreateVertexBufferFromVerts(verts);
		vertexCount = static_cast<uint32_t>(verts.size());
	}

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
        }
    }
    catch (...) {}

    instancingSrvHandleGPU = {};
    instancingSrvIndex_ = 0;

   
    instancingResource.Reset();
    pipelineState.Reset();
    rootSignature.Reset();
    signatureBlob.Reset();
    errorBlob.Reset();
    vertexShaderBlob.Reset();
    pixelShaderBlob.Reset();
	vertexBuffer.Reset();
	vertexBufferView = {};

   
    particles.clear();
	effectParticles.clear();
    numInstance = 0;
    writeIndex = 0;
    normalInstanceCount_ = 0;
	effectInstanceCount_ = 0;
    instanceGroups.clear();
    normalInstanceGroups_.clear();
	effectInstanceGroups_.clear();
	vertexCount = 0;

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
}

void ParticleManager::Draw(ID3D12GraphicsCommandList* commandList, const RenderContext& ctx, UINT vertexCount)
{
	if (!commandList) return;

   // Determine requested draw mode for this call.
	DrawMode requestedMode = (vertexCount == 0) ? DrawMode::Ring : DrawMode::External;
	const std::vector<InstanceGroup>* drawGroups = (requestedMode == DrawMode::Ring) ? &effectInstanceGroups_ : &normalInstanceGroups_;
	const uint32_t drawInstanceCount = (requestedMode == DrawMode::Ring) ? effectInstanceCount_ : normalInstanceCount_;

	// PSOとルートシグネチャをセット
	SetupDraw(commandList);

	// マテリアルCBVとインスタンシング用SRVをセット
	BindResources(commandList, ctx.materialGPUAddress);

    // バーテックスバッファをバインド
	// 呼び出し側が vertexCount を渡している場合はそちらが既に頂点バッファをバインドしているはずなので
	// 内部のリング頂点バッファはバインドしない。
	if (vertexCount == 0)
	{
		if (vertexBuffer && vertexBufferView.SizeInBytes > 0)
		{
			commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		}
	}

    // Light と Camera の CBV は呼び出し側でセット済みのはず（Game::Draw では camera を b2、light を b1 にしている）

    // デスクリプタヒープをバインドする（SRVManager／DirectXCom の PreDraw で既にグローバルに設定されている）

    // テクスチャごとに描画する
    if (drawInstanceCount == 0) return;

    // vertexCount が有効か確認する
    UINT vc = vertexCount;
    if (vc == 0)
    {
        // フォールバック: 呼び出し側が vertexCount を渡してないときは単一の四角 (6 頂点) を使う
        vc = (this->vertexCount > 0) ? this->vertexCount : 6u;
    }

    // デバッグ: インスタンスグループと最初のいくつかのインスタンスの textureIndex をログに出す
	{
		std::ostringstream oss;
        oss << "ParticleManager::Draw - mode=" << ((requestedMode == DrawMode::Ring) ? "Ring" : "External")
			<< " numInstance=" << drawInstanceCount << " groups=" << drawGroups->size() << "\n";
		for (size_t gi = 0; gi < drawGroups->size(); ++gi)
		{
            const auto &gg = (*drawGroups)[gi];
			oss << "  group[" << gi << "] start=" << gg.start << " count=" << gg.count << " tex=" << gg.textureIndex << " handle=0x" << std::hex << (unsigned long long)gg.srvHandle.ptr << std::dec << "\n";
		}
        // 最初の最大8個のインスタンスについて、textureIndex とワールド位置/スケールの概略を出力
		oss << "  first textureIndex values:";
       for (uint32_t i = 0; i < std::min<uint32_t>(drawInstanceCount, 8); ++i)
		{
         const uint32_t instanceIndex = (*drawGroups)[0].start + i;
			oss << " " << instanceData[instanceIndex].textureIndex;
		}
		oss << "\n";
		oss << "  instance World (tx,ty,tz) and diag-scale approx for first entries:";
       for (uint32_t i = 0; i < std::min<uint32_t>(drawInstanceCount, 8); ++i)
		{
            const uint32_t instanceIndex = (*drawGroups)[0].start + i;
			auto &W = instanceData[instanceIndex].World;
			float tx = W.m[3][0];
			float ty = W.m[3][1];
			float tz = W.m[3][2];
			float sx = W.m[0][0];
			float sy = W.m[1][1];
			float sz = W.m[2][2];
			oss << " [" << tx << "," << ty << "," << tz << ",s=" << sx << "," << sy << "," << sz << "]";
		}
		oss << "\n";
		OutputDebugStringA(oss.str().c_str());
	}

 // Ring は通常 Particle と描画対象を分離したため、位置オフセットは行わない。
	bool appliedOffset = false;
	std::vector<Matrix4x4> originalWorld;
	std::vector<Matrix4x4> originalWVP;
	if (requestedMode == DrawMode::Ring)
	{
       appliedOffset = false;
	}

    for (const auto& g : *drawGroups)
    {
        if (g.count == 0) continue;
        // このグループのテクスチャ SRV をテクスチャ用ルートパラメータにバインドする
		// 有効な SRV がない場合は GPU 検証エラーを避けるためこのグループの描画をスキップする
        if (g.srvHandle.ptr == 0)
        {
            continue;
        }

        commandList->SetGraphicsRoot32BitConstant(RootParam::Particle::kInstanceOffset, g.start, 0);
        commandList->SetGraphicsRootDescriptorTable(RootParam::Particle::kTextureTable, g.srvHandle);
        commandList->DrawInstanced(vc, g.count, 0, 0);
    }

	// restore instanceData if we modified it for ring offset
	if (appliedOffset)
	{
      const uint32_t startIndex = drawGroups->empty() ? 0 : (*drawGroups)[0].start;
		for (uint32_t i = 0; i < drawInstanceCount; ++i)
		{
            const uint32_t instanceIndex = startIndex + i;
			instanceData[instanceIndex].World = originalWorld[i];
			instanceData[instanceIndex].WVP = originalWVP[i];
		}
	}
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


std::vector<ParticleManager::Vertex> ParticleManager::CreateRingMesh(uint32_t kRingDivide, float kOuterRadius, float kInnerRadius)
{
	std::vector<Vertex> verts;
	verts.reserve(kRingDivide * 6);
	const float twoPi = std::numbers::pi_v<float> * 2.0f;
	for (uint32_t i = 0; i < kRingDivide; ++i)
	{
		float a = float(i) * twoPi / float(kRingDivide);
		float b = float(i + 1) * twoPi / float(kRingDivide);
		float sinA = std::sin(a), cosA = std::cos(a);
		float sinB = std::sin(b), cosB = std::cos(b);
		float u = float(i) / float(kRingDivide);
		float uNext = float(i + 1) / float(kRingDivide);

        Vector3 vOuterA3 = { -sinA * kOuterRadius, cosA * kOuterRadius, 0.0f };
		Vector3 vOuterB3 = { -sinB * kOuterRadius, cosB * kOuterRadius, 0.0f };
		Vector3 vInnerA3 = { -sinA * kInnerRadius, cosA * kInnerRadius, 0.0f };
		Vector3 vInnerB3 = { -sinB * kInnerRadius, cosB * kInnerRadius, 0.0f };
		Vector3 n = { 0.0f, 0.0f, 1.0f };

		Vector4 vOuterA = { vOuterA3.x, vOuterA3.y, vOuterA3.z, 1.0f };
		Vector4 vOuterB = { vOuterB3.x, vOuterB3.y, vOuterB3.z, 1.0f };
		Vector4 vInnerA = { vInnerA3.x, vInnerA3.y, vInnerA3.z, 1.0f };
		Vector4 vInnerB = { vInnerB3.x, vInnerB3.y, vInnerB3.z, 1.0f };

		// tri 1: outerA, outerB, innerA
		verts.push_back({ vOuterA, { u, 1.0f }, n });
		verts.push_back({ vOuterB, { uNext, 1.0f }, n });
		verts.push_back({ vInnerA, { u, 0.0f }, n });

		// tri 2: outerB, innerB, innerA
		verts.push_back({ vOuterB, { uNext, 1.0f }, n });
		verts.push_back({ vInnerB, { uNext, 0.0f }, n });
		verts.push_back({ vInnerA, { u, 0.0f }, n });
	}
	return verts;
}

void ParticleManager::CreateVertexBufferFromVerts(const std::vector<Vertex>& verts)
{
	if (verts.empty()) return;
	size_t sizeInBytes = verts.size() * sizeof(Vertex);
	vertexBuffer = dxCommon->CreateBufferResource(dxCommon->GetDevice(), sizeInBytes);
	void* mapped = nullptr;
	vertexBuffer->Map(0, nullptr, &mapped);
	memcpy(mapped, verts.data(), sizeInBytes);
	vertexBuffer->Unmap(0, nullptr);

	vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
	vertexBufferView.SizeInBytes = static_cast<UINT>(sizeInBytes);
	vertexBufferView.StrideInBytes = static_cast<UINT>(sizeof(Vertex));
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

	dxCommon->SetHr(dxCommon->GetDevice()->CreateGraphicsPipelineState(&graphicPipelineStateDesc,
		IID_PPV_ARGS(&pipelineState)));
	assert(SUCCEEDED(dxCommon->GetHr()));

	// DepthStencilState の設定（パーティクル向け）
	// 深度テストは有効にして、深度書き込みは行わない（描画順やブレンドに依存するため）
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; 
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	// 生成する PSO の設定に適用
	graphicPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
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
