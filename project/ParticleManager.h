#pragma once

#include <d3d12.h>
#include <ostream>
#include"DirectXCom.h"
#include"Log.h"

class ParticleManager
{
public:
	ParticleManager(std::ostream& logStream, DirectXCom* dxCommon);
	~ParticleManager();

	void Initialize();

	void RootSignature();
	void CreateGraphicsPipeline();
	void Descriptor();
	void CreateRootParameters();
	void StaticSamplers();
	void SignatureBlob();
	void RootSignatureFromBlob();
	void InputLayer();
	void InitializeBlend();
	void RasterizerState();
	void ShaderCompile();
	void InitializeGraphicPipeline();

	void SetupDraw();

public:
	const Microsoft::WRL::ComPtr<ID3D12RootSignature>& GetRootSignature() const
	{
		return rootSignature;
	}

	// Expose pipeline state object (PSO) for use by the renderer
	const Microsoft::WRL::ComPtr<ID3D12PipelineState>& GetPipelineState() const
	{
		return pipelineState;
	}

private:
	DirectXCom* dxCommon = nullptr;

	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	// We need two ranges: [0] for VS StructuredBuffer SRV (t0), [1] for PS Texture SRV (t3)
	D3D12_DESCRIPTOR_RANGE descriptorRange[2] = {};
	D3D12_ROOT_PARAMETER rootParameters[5] = {};
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr;
	D3D12_BLEND_DESC blendDesc{};
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob;
	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicPipelineStateDesc{};

	// PSO object created when pipeline is initialized
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState = nullptr;

	std::ostream& logStream;
};
