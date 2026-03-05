#include"ParticleManager.h"
#include"ParticleEmitter.h"

ParticleManager::ParticleManager(std::ostream& logStream, DirectXCom* dxCommon)
	: logStream(logStream), dxCommon(dxCommon)
{
}


ParticleManager::~ParticleManager()
{
}

void ParticleManager::Initialize()
{
	SetupDraw();

	Random::SeedEngine();

	for (uint32_t index = 0; index < kNumMaxInstances; ++index)
	{
		particles.push_back(MakeNewParticles(randomEngine, emitter.transform.GetTranslate()));
	}

	//TransformationMatrix gTransformationMatrices[10];

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
		}
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC instancingSrvDesc{};
	instancingSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	instancingSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	instancingSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	instancingSrvDesc.Buffer.NumElements = 0;
	instancingSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	instancingSrvDesc.Buffer.NumElements = kNumMaxInstances;
	instancingSrvDesc.Buffer.StructureByteStride = sizeof(ParticleManager::ParticleForGPU);
	D3D12_CPU_DESCRIPTOR_HANDLE instancingSrvHandleCPU = dxCommon->GetCPUDescroptirHandle(dxCommon->GetSrvDescriptorHeap(), dxCommon->GetDescriptorSizeSRV(), 3);
	instancingSrvHandleGPU = dxCommon->GetGPUDescriptorHandle(dxCommon->GetSrvDescriptorHeap(), dxCommon->GetDescriptorSizeSRV(), 3);
	dxCommon->GetDevice()->CreateShaderResourceView(instancingResource.Get(), &instancingSrvDesc, instancingSrvHandleCPU);


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



void ParticleManager::RootSignature()
{
	//RootSignatureの作成

	descriptionRootSignature.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT; //入力アセンブラーでの使用を許可
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
	//RootParemeter生成PuxelShaderのMaterialとVertexShaderのTransform

	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;//CBVを使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; //PixelShaderで使う
	rootParameters[0].Descriptor.ShaderRegister = 0;


	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].DescriptorTable.pDescriptorRanges = &descriptorRangeForInstancing[0];
	rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;


	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = &descriptorRangeForInstancing[1];
	rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[3].Descriptor.ShaderRegister = 1; 

	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[4].Descriptor.ShaderRegister = 2; // b2: 


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

void ParticleManager::SetupDraw()
{
	CreateGraphicsPipeline();
	assert(pipelineState != nullptr && "ParticleManager pipeline state creation failed");
}
