#include "PipelineStateManager.h"
#include "DirectXCom.h"
#include "Log.h"
#include "StringUtil.h"
#include <cassert>
#include <iostream>
#include <vector>
#include <fstream>

PipelineStateManager* PipelineStateManager::GetInstance()
{
	static PipelineStateManager instance;
	return &instance;
}

void PipelineStateManager::Initialize(DirectXCom* dxCommon)
{
	assert(dxCommon);
	
	// キャッシュのクリア
	pipelineStates_.clear();
	rootSignatures_.clear();
	watchedShaders_.clear();

	// PSOキャッシュのロード
	LoadPipelineLibrary(dxCommon->GetDevice().Get());

	// ファイル監視の登録
	auto registerShader = [this](const std::string& key, const std::wstring& path) {
		if (std::filesystem::exists(path)) {
			ShaderFileInfo info;
			info.filePath = path;
			info.lastWriteTime = std::filesystem::last_write_time(path);
			watchedShaders_[key] = info;
		}
	};
	
	registerShader("SpriteVS", L"Resources/shaders/Sprite.VS.hlsl");
	registerShader("SpritePS", L"Resources/shaders/Sprite.PS.hlsl");
	registerShader("Object3DVS", L"Resources/shaders/Object3D.VS.hlsl");
	registerShader("Object3DPS", L"Resources/shaders/Object3D.PS.hlsl");
	registerShader("DebugWireframePS", L"Resources/shaders/DebugWireframe.PS.hlsl");
	registerShader("SlimeVS", L"Resources/shaders/Slime.VS.hlsl");
	registerShader("SlimePS", L"Resources/shaders/Slime.PS.hlsl");

	// パイプラインステートおよびルートシグネチャの構築
	CreateSpritePipelines(dxCommon);
	CreateObject3dPipelines(dxCommon);
	CreateSlimePipelines(dxCommon);
}

void PipelineStateManager::Finalize()
{
	SavePipelineLibrary();
	pipelineLibrary_.Reset();

	pipelineStates_.clear();
	rootSignatures_.clear();
	watchedShaders_.clear();
}

void PipelineStateManager::Update(DirectXCom* dxCommon)
{
	bool spriteNeedsRebuild = false;
	bool objectNeedsRebuild = false;
	bool slimeNeedsRebuild = false;

	for (auto& [key, info] : watchedShaders_)
	{
		if (std::filesystem::exists(info.filePath))
		{
			try {
				auto writeTime = std::filesystem::last_write_time(info.filePath);
				if (writeTime != info.lastWriteTime)
				{
					info.lastWriteTime = writeTime;
					if (key.starts_with("Sprite"))
					{
						spriteNeedsRebuild = true;
					}
					else if (key.starts_with("Object3D"))
					{
						objectNeedsRebuild = true;
					}
					else if (key.starts_with("Slime"))
					{
						slimeNeedsRebuild = true;
					}
				}
			}
			catch (...) {
				// ファイルが書き込みロック中の場合などを考慮してキャッチ
			}
		}
	}

	if (spriteNeedsRebuild)
	{
		try {
			CreateSpritePipelines(dxCommon);
			OutputDebugStringA("Shader Hot-Reload: Rebuilt Sprite pipelines successfully!\n");
		}
		catch (...) {
			OutputDebugStringA("Shader Hot-Reload: Failed to rebuild Sprite pipelines.\n");
		}
	}
	if (objectNeedsRebuild)
	{
		try {
			CreateObject3dPipelines(dxCommon);
			OutputDebugStringA("Shader Hot-Reload: Rebuilt Object3D pipelines successfully!\n");
		}
		catch (...) {
			OutputDebugStringA("Shader Hot-Reload: Failed to rebuild Object3D pipelines.\n");
		}
	}
	if (slimeNeedsRebuild)
	{
		try {
			CreateSlimePipelines(dxCommon);
			OutputDebugStringA("Shader Hot-Reload: Rebuilt Slime pipelines successfully!\n");
		}
		catch (...) {
			OutputDebugStringA("Shader Hot-Reload: Failed to rebuild Slime pipelines.\n");
		}
	}
}

const Microsoft::WRL::ComPtr<ID3D12PipelineState>& PipelineStateManager::GetPipelineState(const std::string& name) const
{
	if (pipelineStates_.contains(name))
	{
		return pipelineStates_.at(name);
	}
	static Microsoft::WRL::ComPtr<ID3D12PipelineState> dummy = nullptr;
	return dummy;
}

const Microsoft::WRL::ComPtr<ID3D12RootSignature>& PipelineStateManager::GetRootSignature(const std::string& name) const
{
	if (rootSignatures_.contains(name))
	{
		return rootSignatures_.at(name);
	}
	static Microsoft::WRL::ComPtr<ID3D12RootSignature> dummy = nullptr;
	return dummy;
}

void PipelineStateManager::CreateSpritePipelines(DirectXCom* dxCommon)
{
	// 1. ルートシグネチャの作成
	D3D12_DESCRIPTOR_RANGE descriptorRange[1]{};
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].BaseShaderRegister = 3;
	descriptorRange[0].RegisterSpace = 0;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[5]{};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].Descriptor.ShaderRegister = 0;

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[3].Descriptor.ShaderRegister = 1;

	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[4].Descriptor.ShaderRegister = 2;

	D3D12_STATIC_SAMPLER_DESC staticSamplers[1]{};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr))
	{
		Logger::Log(std::cout, reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
	hr = dxCommon->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	assert(SUCCEEDED(hr));
	rootSignatures_["Sprite"] = rootSignature;

	// 2. シェーダーコンパイル (IDxcBlob を受け取るように修正)
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon->CompileShader(
		L"Resources/shaders/Sprite.VS.hlsl", L"vs_6_0",
		dxCommon->GetDxcUtils().Get(), dxCommon->GetDxcCompiler(), dxCommon->GetIncludeHandler(), std::cout);
	assert(vertexShaderBlob != nullptr);

	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon->CompileShader(
		L"Resources/shaders/Sprite.PS.hlsl", L"ps_6_0",
		dxCommon->GetDxcUtils().Get(), dxCommon->GetDxcCompiler(), dxCommon->GetIncludeHandler(), std::cout);
	assert(pixelShaderBlob != nullptr);

	// 3. インプットレイアウト
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3]{};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	// 4. 各種ブレンドモード用PSOの作成
	struct BlendSetup {
		std::string name;
		BOOL blendEnable;
		D3D12_BLEND srcBlend;
		D3D12_BLEND destBlend;
		D3D12_BLEND_OP blendOp;
	};

	std::vector<BlendSetup> blendSetups = {
		{ "Sprite_None", FALSE, D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD },
		{ "Sprite_Normal", TRUE, D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD },
		{ "Sprite_Add", TRUE, D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_ONE, D3D12_BLEND_OP_ADD },
		{ "Sprite_Subtract", TRUE, D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_ONE, D3D12_BLEND_OP_REV_SUBTRACT },
		{ "Sprite_Multiply", TRUE, D3D12_BLEND_ZERO, D3D12_BLEND_SRC_COLOR, D3D12_BLEND_OP_ADD },
		{ "Sprite_Screen", TRUE, D3D12_BLEND_INV_DEST_COLOR, D3D12_BLEND_ONE, D3D12_BLEND_OP_ADD }
	};

	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = false;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	for (const auto& setup : blendSetups)
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
		desc.pRootSignature = rootSignature.Get();
		desc.InputLayout = inputLayoutDesc;
		desc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
		desc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };

		desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		desc.BlendState.RenderTarget[0].BlendEnable = setup.blendEnable;
		if (setup.blendEnable)
		{
			// BlendEnable=TRUE の場合のみブレンド設定を適用
			desc.BlendState.RenderTarget[0].SrcBlend = setup.srcBlend;
			desc.BlendState.RenderTarget[0].BlendOp = setup.blendOp;
			desc.BlendState.RenderTarget[0].DestBlend = setup.destBlend;
			desc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
			desc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
			desc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
		}
		else
		{
			// BlendEnable=FALSE の場合はD3D12デフォルト値を使用（ドライバ互換性のため）
			desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
			desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
			desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
			desc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
			desc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
			desc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
		}

		desc.RasterizerState = rasterizerDesc;
		desc.NumRenderTargets = 1;
		desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		desc.SampleDesc.Count = 1;
		desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
		desc.DepthStencilState = depthStencilDesc;
		desc.DSVFormat = DXGI_FORMAT_UNKNOWN; // DepthEnable=false のためUNKNOWNを設定（NVIDIAドライバクラッシュ回避）

		Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
		std::wstring psoNameW = StringUtil::ConvertString(setup.name);
		HRESULT hrLoad = E_FAIL;
		if (pipelineLibrary_)
		{
			hrLoad = pipelineLibrary_->LoadGraphicsPipeline(psoNameW.c_str(), &desc, IID_PPV_ARGS(&pipelineState));
		}
		if (FAILED(hrLoad))
		{
			hr = dxCommon->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipelineState));
			if (FAILED(hr))
			{
				OutputDebugStringA(std::format("Error: CreateGraphicsPipelineState failed for {} (hr=0x{:08X})\n", setup.name, (uint32_t)hr).c_str());
			}
			assert(SUCCEEDED(hr));
			// PSO作成後に即座にデバイス削除理由を確認（NVIDIAドライバの遅延エラー検出のため）
			HRESULT deviceRemovedReason = dxCommon->GetDevice()->GetDeviceRemovedReason();
			if (FAILED(deviceRemovedReason))
			{
				OutputDebugStringA(std::format("Device removed after PSO creation: {} (reason=0x{:08X})\n", setup.name, (uint32_t)deviceRemovedReason).c_str());
				assert(false && "Device removed during Sprite PSO creation");
			}
			if (pipelineLibrary_ && SUCCEEDED(hr))
			{
				pipelineLibrary_->StorePipeline(psoNameW.c_str(), pipelineState.Get());
			}
		}
		pipelineStates_[setup.name] = pipelineState;
		OutputDebugStringA(std::format("OK: Created PSO: {}\n", setup.name).c_str());
	}
}

void PipelineStateManager::CreateObject3dPipelines(DirectXCom* dxCommon)
{
	// 1. ルートシグネチャの作成
	D3D12_DESCRIPTOR_RANGE descriptorRange[2]{};
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].BaseShaderRegister = 3;
	descriptorRange[0].RegisterSpace = 0;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	descriptorRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[1].NumDescriptors = 1;
	descriptorRange[1].BaseShaderRegister = 4;
	descriptorRange[1].RegisterSpace = 0;
	descriptorRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[6]{};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].Descriptor.ShaderRegister = 0;

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = &descriptorRange[0];
	rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[3].Descriptor.ShaderRegister = 1;

	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[4].Descriptor.ShaderRegister = 2;

	rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[5].DescriptorTable.pDescriptorRanges = &descriptorRange[1];
	rootParameters[5].DescriptorTable.NumDescriptorRanges = 1;

	D3D12_STATIC_SAMPLER_DESC staticSamplers[2]{};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	staticSamplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[1].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[1].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[1].ShaderRegister = 1;
	staticSamplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr))
	{
		Logger::Log(std::cout, reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
	hr = dxCommon->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	assert(SUCCEEDED(hr));
	rootSignatures_["Object3D"] = rootSignature;

	// 2. シェーダーコンパイル (IDxcBlob を受け取るように修正)
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon->CompileShader(
		L"Resources/shaders/Object3D.VS.hlsl", L"vs_6_0",
		dxCommon->GetDxcUtils().Get(), dxCommon->GetDxcCompiler(), dxCommon->GetIncludeHandler(), std::cout);
	assert(vertexShaderBlob != nullptr);

	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon->CompileShader(
		L"Resources/shaders/Object3D.PS.hlsl", L"ps_6_0",
		dxCommon->GetDxcUtils().Get(), dxCommon->GetDxcCompiler(), dxCommon->GetIncludeHandler(), std::cout);
	assert(pixelShaderBlob != nullptr);

	Microsoft::WRL::ComPtr<IDxcBlob> wireframePixelShaderBlob = dxCommon->CompileShader(
		L"Resources/shaders/DebugWireframe.PS.hlsl", L"ps_6_0",
		dxCommon->GetDxcUtils().Get(), dxCommon->GetDxcCompiler(), dxCommon->GetIncludeHandler(), std::cout);
	assert(wireframePixelShaderBlob != nullptr);

	// 3. インプットレイアウト
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3]{};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	// 4. PSO生成 (通常、エフェクト、オーバーレイ)
	// 通常
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
		desc.pRootSignature = rootSignature.Get();
		desc.InputLayout = inputLayoutDesc;
		desc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
		desc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };

		desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		desc.BlendState.RenderTarget[0].BlendEnable = TRUE;
		desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		desc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		desc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		desc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;

		desc.RasterizerState = rasterizerDesc;
		desc.NumRenderTargets = 1;
		desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		desc.SampleDesc.Count = 1;
		desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

		desc.DepthStencilState.DepthEnable = TRUE;
		desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

		Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
		HRESULT hrLoad = E_FAIL;
		if (pipelineLibrary_)
			hrLoad = pipelineLibrary_->LoadGraphicsPipeline(L"Object3D_Normal", &desc, IID_PPV_ARGS(&pipelineState));
		if (FAILED(hrLoad))
		{
			hr = dxCommon->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipelineState));
			if (FAILED(hr)) OutputDebugStringA(std::format("Error: CreateGraphicsPipelineState failed for Object3D_Normal (hr=0x{:08X})\n", (uint32_t)hr).c_str());
			assert(SUCCEEDED(hr));
			if (pipelineLibrary_ && SUCCEEDED(hr)) pipelineLibrary_->StorePipeline(L"Object3D_Normal", pipelineState.Get());
		}
		pipelineStates_["Object3D_Normal"] = pipelineState;

		// エフェクト (デプス書き込み無効)
		desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStateEffect;
		hrLoad = E_FAIL;
		if (pipelineLibrary_)
			hrLoad = pipelineLibrary_->LoadGraphicsPipeline(L"Object3D_Effect", &desc, IID_PPV_ARGS(&pipelineStateEffect));
		if (FAILED(hrLoad))
		{
			hr = dxCommon->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipelineStateEffect));
			if (FAILED(hr)) OutputDebugStringA(std::format("Error: CreateGraphicsPipelineState failed for Object3D_Effect (hr=0x{:08X})\n", (uint32_t)hr).c_str());
			assert(SUCCEEDED(hr));
			if (pipelineLibrary_ && SUCCEEDED(hr)) pipelineLibrary_->StorePipeline(L"Object3D_Effect", pipelineStateEffect.Get());
		}
		pipelineStates_["Object3D_Effect"] = pipelineStateEffect;

		// オーバーレイ (デプステスト無効)
		desc.DepthStencilState.DepthEnable = FALSE;
		desc.DSVFormat = DXGI_FORMAT_UNKNOWN; // DepthEnable=false のためUNKNOWNを設定（NVIDIAドライバクラッシュ回避）
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStateOverlay;
		hrLoad = E_FAIL;
		if (pipelineLibrary_)
			hrLoad = pipelineLibrary_->LoadGraphicsPipeline(L"Object3D_Overlay", &desc, IID_PPV_ARGS(&pipelineStateOverlay));
		if (FAILED(hrLoad))
		{
			hr = dxCommon->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipelineStateOverlay));
			if (FAILED(hr)) OutputDebugStringA(std::format("Error: CreateGraphicsPipelineState failed for Object3D_Overlay (hr=0x{:08X})\n", (uint32_t)hr).c_str());
			assert(SUCCEEDED(hr));
			if (pipelineLibrary_ && SUCCEEDED(hr)) pipelineLibrary_->StorePipeline(L"Object3D_Overlay", pipelineStateOverlay.Get());
		}
		pipelineStates_["Object3D_Overlay"] = pipelineStateOverlay;

		// ワイヤーフレーム (デプステスト有効、デプス書き込み無効、ワイヤーフレーム描画モード)
		desc.DepthStencilState.DepthEnable = TRUE;
		desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // Zファイティング防止
		desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		desc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
		desc.PS = { wireframePixelShaderBlob->GetBufferPointer(), wireframePixelShaderBlob->GetBufferSize() };

		Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStateWireframe;
		hrLoad = E_FAIL;
		if (pipelineLibrary_)
			hrLoad = pipelineLibrary_->LoadGraphicsPipeline(L"Object3D_Wireframe", &desc, IID_PPV_ARGS(&pipelineStateWireframe));
		if (FAILED(hrLoad))
		{
			hr = dxCommon->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipelineStateWireframe));
			if (FAILED(hr)) OutputDebugStringA(std::format("Error: CreateGraphicsPipelineState failed for Object3D_Wireframe (hr=0x{:08X})\n", (uint32_t)hr).c_str());
			assert(SUCCEEDED(hr));
			if (pipelineLibrary_ && SUCCEEDED(hr)) pipelineLibrary_->StorePipeline(L"Object3D_Wireframe", pipelineStateWireframe.Get());
		}
		pipelineStates_["Object3D_Wireframe"] = pipelineStateWireframe;
	}
}

void PipelineStateManager::CreateSlimePipelines(DirectXCom* dxCommon)
{
	// 1. スライム専用ルートシグネチャの作成
	D3D12_DESCRIPTOR_RANGE descriptorRange[2]{};
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].BaseShaderRegister = 3; // t3: Main Texture
	descriptorRange[0].RegisterSpace = 0;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	descriptorRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[1].NumDescriptors = 1;
	descriptorRange[1].BaseShaderRegister = 4; // t4: Cube Environment Map
	descriptorRange[1].RegisterSpace = 0;
	descriptorRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[7]{};
	// 0: Material (b0, Pixel)
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	// 1: TransformationMatrix (b0, Vertex)
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].Descriptor.ShaderRegister = 0;

	// 2: Texture2D (t3, All)
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = &descriptorRange[0];
	rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

	// 3: SlimeParams (b1, All: Vertex & Pixel)
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[3].Descriptor.ShaderRegister = 1;

	// 4: DirectionalLight (b2, Pixel)
	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[4].Descriptor.ShaderRegister = 2;

	// 5: Camera (b3, Pixel)
	rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[5].Descriptor.ShaderRegister = 3;

	// 6: Cube Environment Map (t4, Pixel)
	rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[6].DescriptorTable.pDescriptorRanges = &descriptorRange[1];
	rootParameters[6].DescriptorTable.NumDescriptorRanges = 1;

	D3D12_STATIC_SAMPLER_DESC staticSamplers[2]{};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	staticSamplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[1].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[1].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[1].ShaderRegister = 1;
	staticSamplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr))
	{
		Logger::Log(std::cout, reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
	hr = dxCommon->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	assert(SUCCEEDED(hr));
	rootSignatures_["Slime"] = rootSignature;

	// 2. シェーダーコンパイル
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon->CompileShader(
		L"Resources/shaders/Slime.VS.hlsl", L"vs_6_0",
		dxCommon->GetDxcUtils().Get(), dxCommon->GetDxcCompiler(), dxCommon->GetIncludeHandler(), std::cout);
	assert(vertexShaderBlob != nullptr);

	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon->CompileShader(
		L"Resources/shaders/Slime.PS.hlsl", L"ps_6_0",
		dxCommon->GetDxcUtils().Get(), dxCommon->GetDxcCompiler(), dxCommon->GetIncludeHandler(), std::cout);
	assert(pixelShaderBlob != nullptr);

	// 3. インプットレイアウト
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3]{};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE; // 両面描画（半透明ゼリー体）
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	// 4. Slime_Normal PSO（半透明αブレンド有効）
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
		desc.pRootSignature = rootSignature.Get();
		desc.InputLayout = inputLayoutDesc;
		desc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
		desc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };

		// 半透明αブレンド
		desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		desc.BlendState.RenderTarget[0].BlendEnable = TRUE;
		desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		desc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		desc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		desc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;

		desc.RasterizerState = rasterizerDesc;
		desc.NumRenderTargets = 1;
		desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		desc.SampleDesc.Count = 1;
		desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

		// デプスは読み取り可能・書き込み有効
		desc.DepthStencilState.DepthEnable = TRUE;
		desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

		Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
		HRESULT hrLoad = E_FAIL;
		if (pipelineLibrary_)
			hrLoad = pipelineLibrary_->LoadGraphicsPipeline(L"Slime_Normal", &desc, IID_PPV_ARGS(&pipelineState));
		if (FAILED(hrLoad))
		{
			hr = dxCommon->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipelineState));
			if (FAILED(hr)) OutputDebugStringA(std::format("Error: CreateGraphicsPipelineState failed for Slime_Normal (hr=0x{:08X})\n", (uint32_t)hr).c_str());
			assert(SUCCEEDED(hr));
			if (pipelineLibrary_ && SUCCEEDED(hr)) pipelineLibrary_->StorePipeline(L"Slime_Normal", pipelineState.Get());
		}
		pipelineStates_["Slime_Normal"] = pipelineState;
		OutputDebugStringA("OK: Created PSO: Slime_Normal\n");
	}
}

void PipelineStateManager::LoadPipelineLibrary(ID3D12Device* device)
{
	// 【診断モード】PSOキャッシュを一時的に無効化してデバイス削除の原因を切り分ける
	// pipelineLibrary_ を null のままにすることで、全PSO作成が CreateGraphicsPipelineState を直接使用する
	OutputDebugStringA("PipelineStateManager: PSO cache library DISABLED (diagnostic mode).\n");
	pipelineLibrary_.Reset();
	(void)device; // 未使用変数警告抑制
}

void PipelineStateManager::SavePipelineLibrary()
{
	if (!pipelineLibrary_) return;

	size_t serializedSize = pipelineLibrary_->GetSerializedSize();
	if (serializedSize == 0) return;

	std::vector<uint8_t> buffer(serializedSize);
	HRESULT hr = pipelineLibrary_->Serialize(buffer.data(), serializedSize);
	if (SUCCEEDED(hr))
	{
		std::wstring cachePath = L"Resources/shaders/PsoCache.bin";
		std::filesystem::create_directories(std::filesystem::path(cachePath).parent_path());
		std::ofstream file(cachePath, std::ios::binary);
		if (file.is_open())
		{
			file.write(reinterpret_cast<const char*>(buffer.data()), serializedSize);
			file.close();
		}
	}
}
