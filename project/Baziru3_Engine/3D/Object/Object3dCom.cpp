#include"Object3dCom.h"
#include "Light.h"


// 参照メンバー logStream を初期化するコンストラクタ定義
Object3dCom::Object3dCom(std::ostream& logStream)
	: logStream(logStream)
{
}

void Object3dCom::Initialize(DirectXCom* directXCom)
{
	dxCommon = directXCom;
	CreateGraphicsPipelineState();
}

void Object3dCom::Update()
{
}

void Object3dCom::RootSignature()
{
	descriptionRootSignature.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT; //入力アセンブラーでの使用を許可
}


void Object3dCom::CreateGraphicsPipelineState()
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

	// ここでPSOを生成（初回だけ）
	if (!pipelineState)
	{
		auto& desc = graphicPipelineStateDesc;
		desc.pRootSignature = rootSignature.Get();
		if (pipelineState == nullptr)
		{
			dxCommon->SetHr(dxCommon->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipelineState)));
			assert(SUCCEEDED(dxCommon->GetHr()));
		}
        // Create an alternative PSO for effect rendering where depth writes are disabled.
		// Make a copy of the descriptor and adjust depth write mask.
		if (pipelineStateEffect == nullptr)
		{
			D3D12_GRAPHICS_PIPELINE_STATE_DESC descEffect = desc;
			// Ensure depth stencil state exists
			descEffect.DepthStencilState.DepthEnable = TRUE;
			descEffect.DepthStencilState.DepthFunc = depthStencilDesc.DepthFunc;
			descEffect.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // disable depth writes for effects
			descEffect.pRootSignature = rootSignature.Get();
			dxCommon->SetHr(dxCommon->GetDevice()->CreateGraphicsPipelineState(&descEffect, IID_PPV_ARGS(&pipelineStateEffect)));
			assert(SUCCEEDED(dxCommon->GetHr()));
		}
       if (pipelineStateOverlay == nullptr)
		{
			D3D12_GRAPHICS_PIPELINE_STATE_DESC descOverlay = desc;
			descOverlay.DepthStencilState.DepthEnable = FALSE;
			descOverlay.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
			descOverlay.pRootSignature = rootSignature.Get();
			dxCommon->SetHr(dxCommon->GetDevice()->CreateGraphicsPipelineState(&descOverlay, IID_PPV_ARGS(&pipelineStateOverlay)));
			assert(SUCCEEDED(dxCommon->GetHr()));
		}
	}
}

void Object3dCom::PreDraw()
{
	// PSOとルートシグネチャをバインド
	if (!pipelineState)
	{
		CreateGraphicsPipelineState();
	}
	auto CommandList = dxCommon->GetCommandList();
	CommandList->SetGraphicsRootSignature(rootSignature.Get());
	CommandList->SetPipelineState(pipelineState.Get());
}



void Object3dCom::Draw(Object3d* object, const ::RenderContext& ctx, const Object3d::ModelData& modelData, bool drawObject)
{
    if (!ctx.commandList) return;
    if (!ctx.camera)
	{
		Logger::Log(logStream, "Warning: camera is null when drawing object. Skipping draw.\n");
		return;
	}
	// Ensure the correct root signature and PSO are bound before setting root parameters.
	// PreDraw() should normally set these, but re-bind here to avoid mismatches
	// if other components changed the root signature (e.g., skybox/sprite pipelines).
	if (rootSignature)
	{
		ctx.commandList->SetGraphicsRootSignature(rootSignature.Get());
	}
	if (pipelineState)
	{
		ctx.commandList->SetPipelineState(pipelineState.Get());
	}
   
	// Ensure the descriptor table root parameter is always initialized before Draw.
	// GPU-based validation requires a valid root argument even if no custom texture is bound.
	D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = ctx.textureHandle;
	if (srvHandle.ptr == 0 && dxCommon)
	{
		// Fallback to the texture index stored in the model's material.
		uint32_t texIdx = modelData.material.textureIndex;
		if (texIdx != 0 && texIdx != UINT32_MAX)
		{
			srvHandle = dxCommon->GetSRVHandleGPU(texIdx);
		}
	}
	if (srvHandle.ptr != 0)
	{
		ctx.commandList->SetGraphicsRootDescriptorTable(2, srvHandle);
	}

   
    if (ctx.light)
    {
        ctx.commandList->SetGraphicsRootConstantBufferView(3, ctx.light->GetDirectionalLightResource()->GetGPUVirtualAddress());
    }
    else
    {
        ctx.commandList->SetGraphicsRootConstantBufferView(3, 0);
    }

   
    if (ctx.camera->GetCameraResource())
    {
        ctx.commandList->SetGraphicsRootConstantBufferView(4, ctx.camera->GetCameraResource()->GetGPUVirtualAddress());
    }
    else
    {
        // Camera GPU resource not available: skip drawing to avoid uninitialized root argument on GPU
        Logger::Log(logStream, "Warning: camera GPU resource not available when drawing object. Skipping draw.\n");
        return;
    }

   
    if (object)
    {
        object->Draw(ctx.commandList);
    }

    if (drawObject)
    {
        ctx.commandList->DrawInstanced(UINT(modelData.vertices.size()), 1, 0, 0);
    }
}

void Object3dCom::Descriptor()
{

	// SRV: t3, t4
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].BaseShaderRegister = 3;
	descriptorRange[0].RegisterSpace = 0;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
}

void Object3dCom::CreateRootParameters()
{
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;//CBVを使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; //PixelShaderで使う
	rootParameters[0].Descriptor.ShaderRegister = 0; //レジスタ番号0とバインド。b0の0と一致

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;//VertexShaderで使える
	rootParameters[1].Descriptor.ShaderRegister = 0;//レジスタ番号0を使用

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;//DescriptorTableを使う
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;//PixelShaderで使う
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;//Tableの中身の配列を指定
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);//Tableで管理する数

	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[3].Descriptor.ShaderRegister = 1;

	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う 
	rootParameters[4].Descriptor.ShaderRegister = 2; // b2 とバインド

	descriptionRootSignature.pParameters = rootParameters; //ルートパラメーター配列へのポインタ
	descriptionRootSignature.NumParameters = _countof(rootParameters);//配列の長さ
}

void Object3dCom::StaticSamplers()
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

void Object3dCom::SignatureBlob()
{
	//シリアライズしてバイナリにする

	dxCommon->SetHr(D3D12SerializeRootSignature(&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob));

	if (FAILED(dxCommon->GetHr()))
	{
		Logger::Log(logStream, reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}
}

void Object3dCom::RootSignatureFromBlob()
{
	dxCommon->SetHr(dxCommon->GetDevice()->CreateRootSignature(0,
		signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature)));
	assert(SUCCEEDED(dxCommon->GetHr()));
}

void Object3dCom::InputLayer()
{
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

void Object3dCom::InitializeBlend()
{
	//BlendStateの設定
	//すべての色要素を書き込む
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;

	//--ノーマルブレンド------------------------------
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	//--------------------------------------------

	//--加算ブレンド------------------------------
	/*blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;*/
	//--------------------------------------------

	//--減算ブレンド------------------------------
	/*blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;*/
	//--------------------------------------------

	//--乗算ブレンド------------------------------
	/*blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_DEST_COLOR;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	/*blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_INV_DEST_COLOR;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;*/
	//--------------------------------------------


	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
}

void Object3dCom::RasterizerState()
{
	//RasterizerStateの設定
    // モデルの頂点順が想定と逆でも見えるように一旦カリングしない
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	//三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
}

void Object3dCom::ShaderCompile()
{
	//Shaderをコンパイルする
	vertexShaderBlob = dxCommon->CompileShader(L"Resources/shaders/Object3D.VS.hlsl",
		L"vs_6_0", dxCommon->GetDxcUtils().Get(), dxCommon->GetDxcCompiler(), dxCommon->GetIncludeHandler(), logStream);
	assert(vertexShaderBlob != nullptr);

	pixelShaderBlob = dxCommon->CompileShader(L"Resources/shaders/Object3D.PS.hlsl",
		L"ps_6_0", dxCommon->GetDxcUtils().Get(), dxCommon->GetDxcCompiler(), dxCommon->GetIncludeHandler(), logStream);
	assert(pixelShaderBlob != nullptr);

}

void Object3dCom::InitializeGraphicPipeline()
{
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

	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	graphicPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
}
