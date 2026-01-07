#include"ParticleManager.h"
#include"Log.h"

ParticleManager* ParticleManager::instance_ = nullptr;

ParticleManager::ParticleManager(std::ostream& logStream, DirectXCom* dxCommon)
	:logStream(logStream), dxCommon_(dxCommon)
{
	lastUpdateTime_ = std::chrono::steady_clock::now();

	instance_ = this;
}

ParticleManager::~ParticleManager()
{

	for (auto& kv : particleGroups_)
	{
		if (kv.second.instancingResource_ && kv.second.instanceMapped_)
		{
			kv.second.instancingResource_->Unmap(0, nullptr);
			kv.second.instanceMapped_ = nullptr;
		}
	}

	
	if (instance_ == this) instance_ = nullptr;
}



void ParticleManager::Initialize(DirectXCom* dxCommon, SRVManager* srvManager)
{
	// DirectXCom を保存
	assert(dxCommon != nullptr);
	dxCommon_ = dxCommon;

	srvManager_ = srvManager;

	//ランダムエンジンの初期化
	std::random_device random;
	random_ = std::mt19937(random());

	//パイプラインの生成
	CreatePipeline();

	//頂点データの初期化
	InitializeVertexData();

	//頂点リソース生成
	CreateVertexResource();

	//頂点バッファビュー(VBV)生成
	CreateVertexBufferView();

	//頂点リソースに頂点データを書き込む
	vbv_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
	vbv_.SizeInBytes = static_cast<UINT>(vertexBufferSize);
	vbv_.StrideInBytes = sizeof(VertexData);

	lastUpdateTime_ = std::chrono::steady_clock::now();

	directionalLightResource_ = dxCommon_->CreateBufferResource(dxCommon_->GetDevice(), sizeof(DirectionalLight));
	DirectionalLight* dlData = nullptr;
	if (SUCCEEDED(directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&dlData))))
	{
		dlData->color = {1.0f,1.0f,1.0f,1.0f};
		dlData->direction = {0.0f,-1.0f,0.0f};
		dlData->intensity = 1.0f;
		directionalLightResource_->Unmap(0, nullptr);
	}
}

void ParticleManager::Update()
{
	using namespace std::chrono;
	auto now = steady_clock::now();
	auto dt = duration_cast<duration<float>>(now - lastUpdateTime_).count();
	lastUpdateTime_ = now;
	if (dt <= 0.0f) return;

	for (auto& kv : particleGroups_)
	{
		ParticleGroup& group = kv.second;
		if (!group.instanceMapped_) { continue; }

		for (auto it = group.particles.begin(); it != group.particles.end(); )
		{
			Particle& p = *it;
			p.currentTime += dt;
			if (p.currentTime >= p.lifeTime)
			{
				it = group.particles.erase(it);
				continue;
			}
			Vector3 t = p.transform.GetTranslate();
			t.x += p.velocity.x * dt;
			t.y += p.velocity.y * dt;
			t.z += p.velocity.z * dt;
			p.transform.SetTranslate(t);
			++it;
		}

		uint32_t count = static_cast<uint32_t>(std::min<size_t>(group.particles.size(), group.instanceCount_));
		InstanceData* dst = reinterpret_cast<InstanceData*>(group.instanceMapped_);
		uint32_t i = 0;
		for (auto& p : group.particles)
		{
			if (i >= count) break;
			Vector3 pos = p.transform.GetTranslate();
			pos.x += globalOffset_.x;
			pos.y += globalOffset_.y;
			pos.z += globalOffset_.z;
			Matrix4x4 world = MakeAffineMatrix(p.transform.GetScale(), p.transform.GetRotate(), pos);
			Matrix4x4 wvp = world;
			dst[i].World = world;
			dst[i].WVP = wvp;
			dst[i].alpha = p.color.w;
			dst[i].pad[0] = 0.0f; dst[i].pad[1] = 0.0f; dst[i].pad[2] = 0.0f;
			dst[i].color = p.color;
			++i;
		}
	}
}

void ParticleManager::CreatePipeline()
{
	RootSignature();
	Descriptor();
	CreateRootParameters();
	StaticSamplers();
	SignatureBlob();
	RootSignatureFromBlob();
	InputLayer();
	InitializeBlend();
	RasterizerState();
	ShaderCompile();
	InitializeGraphicPipeline();
}

void ParticleManager::RootSignature()
{
	//RootSignatureの作成

	descriptionRootSignature.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT; //入力アセンブラーでの使用を許可
}

void ParticleManager::Descriptor()
{
	// 2 つの SRV レンジを用意する
	// [0]: Vertex Shader 用のインスタンスバッファ (StructuredBuffer) -> t0
	descriptorRange[0] = {};
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].BaseShaderRegister = 0; // t0
	descriptorRange[0].RegisterSpace = 0;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// [1]: Pixel Shader 用のテクスチャ -> t3
	descriptorRange[1] = {};
	descriptorRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[1].NumDescriptors = 1;
	descriptorRange[1].BaseShaderRegister = 3; // t3
	descriptorRange[1].RegisterSpace = 0;
	descriptorRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
}

void ParticleManager::CreateRootParameters()
{
	//RootParemeter生成PuxelShaderのMaterialとVertexShaderのTransform

	rootParameters[0] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;//CBVを使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; //PixelShaderで使う
	rootParameters[0].Descriptor.ShaderRegister = 0; //レジスタ番号0とバインド。b0の0と一致

	//Sprite用
	rootParameters[1] = {};
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;//VertexShaderで使える
	rootParameters[1].Descriptor.ShaderRegister = 0;//レジスタ番号0を使用

	// [2]: PixelShader のテクスチャ SRV テーブル (t3)
	rootParameters[2] = {};
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;//DescriptorTableを使う
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//PixelShaderで使う
	rootParameters[2].DescriptorTable.pDescriptorRanges = &descriptorRange[1];// テクスチャSRV
	rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

	// [3]: DirectionalLight の CBV b1 (PS)
	rootParameters[3] = {};
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[3].Descriptor.ShaderRegister = 1;

	// [4]: VertexShader のインスタンス StructuredBuffer SRV テーブル (t0)
	rootParameters[4] = {};
	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[4].DescriptorTable.pDescriptorRanges = &descriptorRange[0];
	rootParameters[4].DescriptorTable.NumDescriptorRanges = 1;

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

	dxCommon_->SetHr(D3D12SerializeRootSignature(&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob));

	if (FAILED(dxCommon_->GetHr()))
	{
		Logger::Log(logStream, reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}
}

void ParticleManager::RootSignatureFromBlob()
{
	//バイナリをもとに生成

	dxCommon_->SetHr(dxCommon_->GetDevice()->CreateRootSignature(0,
		signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature)));
	assert(SUCCEEDED(dxCommon_->GetHr()));
}

void ParticleManager::InputLayer()
{
	//InputLayer

	inputElementDescs[0].SemanticName = "POSITION"; //セマンティック名
	inputElementDescs[0].SemanticIndex = 0; //セマンティックインデックス
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT; //頂点のフォーマット
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;


	inputLayoutDesc.pInputElementDescs = inputElementDescs; //入力要素の配列
	inputLayoutDesc.NumElements = _countof(inputElementDescs); //入力要素の数
}

void ParticleManager::InitializeBlend()
{
	//BlendStateの設定
	//すべての色要素を書き込む
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;

	//--ノーマルブレンド------------------------------
	//blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	//blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	//blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
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

	vertexShaderBlob = dxCommon_->CompileShader(L"Resources/shaders/Particle.VS.hlsl",
		L"vs_6_0", dxCommon_->GetDxcUtils().Get(), dxCommon_->GetDxcCompiler(), dxCommon_->GetIncludeHandler(), logStream);
	assert(vertexShaderBlob != nullptr);
	pixelShaderBlob = dxCommon_->CompileShader(L"Resources/shaders/Particle.PS.hlsl",
		L"ps_6_0", dxCommon_->GetDxcUtils().Get(), dxCommon_->GetDxcCompiler(), dxCommon_->GetIncludeHandler(), logStream);
	assert(pixelShaderBlob != nullptr);

	//Sprite用Shaderをコンパイルする
	//Shaderをコンパイルする
	//vertexShaderBlob = dxCommon->CompileShader(L"Resources/shaders/Object3D.VS.hlsl",
	//	L"vs_6_0", dxCommon->GetDxcUtils().Get(), dxCommon->GetDxcCompiler(), dxCommon->GetIncludeHandler(), logStream);
	//assert(vertexShaderBlob != nullptr);

	//pixelShaderBlob = dxCommon->CompileShader(L"Resources/shaders/Object3D.PS.hlsl",
	//	L"ps_6_0", dxCommon->GetDxcUtils().Get(), dxCommon->GetDxcCompiler(), dxCommon->GetIncludeHandler(), logStream);
	//assert(pixelShaderBlob != nullptr);
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

	// 必要なデフォルト設定（DepthStencilState 等）が未設定ならゼロ初期化回避のため明示設定
	graphicPipelineStateDesc.DepthStencilState.DepthEnable = FALSE;
	graphicPipelineStateDesc.DepthStencilState.StencilEnable = FALSE;

	// Create PSO
	HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicPipelineStateDesc, IID_PPV_ARGS(&pipelineState));
	assert(SUCCEEDED(hr));
}

void ParticleManager::InitializeVertexData()
{
	vertices_.clear();
	vertices_.reserve(4);
	vertices_.push_back({ -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f }); // 左下
	vertices_.push_back({ -0.5f,  0.5f, 0.0f, 1.0f, 0.0f, 0.0f }); // 左上
	vertices_.push_back({ 0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 1.0f }); // 右下
	vertices_.push_back({ 0.5f,  0.5f, 0.0f, 1.0f, 1.0f, 0.0f }); // 右上
}

void ParticleManager::CreateVertexResource()
{
	vertexBufferSize = sizeof(VertexData) * vertices_.size();
	vertexBuffer_ = dxCommon_->CreateBufferResource(dxCommon_->GetDevice(), vertexBufferSize);
	assert(vertexBuffer_ != nullptr);


}

void ParticleManager::CreateVertexBufferView()
{
	//頂点リソースに頂点データを書き込む
	void* mappedData = nullptr;
	HRESULT hr = vertexBuffer_->Map(0, nullptr, &mappedData);
	assert(SUCCEEDED(hr));
	std::memcpy(mappedData, vertices_.data(), vertexBufferSize);
	vertexBuffer_->Unmap(0, nullptr);


}

void ParticleManager::SetupDraw()
{
	CreatePipeline();
}

void ParticleManager::Draw()
{
	auto cmdList = dxCommon_->GetCommandList();

	
	cmdList->SetPipelineState(pipelineState.Get());
	cmdList->SetGraphicsRootSignature(rootSignature.Get());

	
	if (directionalLightResource_)
	{
		cmdList->SetGraphicsRootConstantBufferView(3, directionalLightResource_->GetGPUVirtualAddress());
	}

	srvManager_->PreDraw();

	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	cmdList->IASetVertexBuffers(0, 1, &vbv_);

	for (auto& kv : particleGroups_)
	{
		const ParticleGroup& group = kv.second;

		if (group.textureSRVIndex_ != 0)
		{
			cmdList->SetGraphicsRootDescriptorTable(2, srvManager_->GetGPUDescriptorHandle(group.textureSRVIndex_));
		}
		if (group.instanceSRVIndex_ != 0)
		{
			cmdList->SetGraphicsRootDescriptorTable(4, srvManager_->GetGPUDescriptorHandle(group.instanceSRVIndex_));
		}

		UINT instanceCount = static_cast<UINT>(group.particles.size());
		if (instanceCount == 0) continue;
		cmdList->DrawInstanced(4, instanceCount, 0, 0);
	}
}

void ParticleManager::CreateParticleGroup(const std::string& name, const std::string& textureFilePath)
{
	//同じ名前のパーティクルグループが存在する場合は作成しない
	if (particleGroups_.find(name) != particleGroups_.end())
	{
		Logger::Log(logStream, "Particle group with name " + name + " already exists.");
		return;
	}

	ParticleGroup newGroup;
	newGroup.textureFilePath_ = textureFilePath;

	// テクスチャを読み込む
	DirectX::ScratchImage image = dxCommon_->LoadTexture(textureFilePath);
	const DirectX::TexMetadata& md = image.GetMetadata();
	// テクスチャ用 GPU リソース作成
	Microsoft::WRL::ComPtr<ID3D12Resource> texResource = dxCommon_->CreateTextureResource(md);
	newGroup.textureResource_ = texResource;

	// Upload once and keep intermediate alive until GPU finishes
	newGroup.uploadIntermediate_ = dxCommon_->UploadTextureData(newGroup.textureResource_, image, dxCommon_->GetDevice(), dxCommon_->GetCommandList());

	uint32_t texSrvIndex = srvManager_->Allocate();
	srvManager_->CreateSRVforTexture2D(texSrvIndex, newGroup.textureResource_.Get(), md.format, static_cast<UINT>(md.mipLevels));
	newGroup.textureSRVIndex_ = texSrvIndex;

	// Instance buffer sized for InstanceData, not Particle
	const UINT initialInstanceCount = 1;
	size_t instanceBufferSize = sizeof(InstanceData) * initialInstanceCount;
	newGroup.instancingResource_ = dxCommon_->CreateBufferResource(dxCommon_->GetDevice(), instanceBufferSize);
	assert(newGroup.instancingResource_ != nullptr);
	newGroup.instanceCount_ = initialInstanceCount;

	uint32_t instSrvIndex = srvManager_->Allocate();
	srvManager_->CreateSRVforStructuredBuffer(instSrvIndex, newGroup.instancingResource_.Get(), initialInstanceCount, static_cast<UINT>(sizeof(InstanceData)));
	newGroup.instanceSRVIndex_ = instSrvIndex;

	void* mapped = nullptr;
	HRESULT hr = newGroup.instancingResource_->Map(0, nullptr, &mapped);
	if (SUCCEEDED(hr))
	{
		newGroup.instanceMapped_ = mapped;
	}
	else
	{
		newGroup.instanceMapped_ = nullptr;
	}

	particleGroups_.emplace(name, std::move(newGroup));
}

void ParticleManager::Emit(const std::string name, const Vector3& position, uint32_t count)
{
	
}