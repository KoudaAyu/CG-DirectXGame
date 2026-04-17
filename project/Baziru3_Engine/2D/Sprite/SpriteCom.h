#pragma once

#include <d3d12.h>
#include <ostream>
#include"DirectXCom.h"
#include"Log.h"

enum BlendMode
{
	//!< ブレンドなし
	kBlendMode_None,

	//!< αブレンド
	kBlendMode_Normal,

	//!< 加算ブレンド
	kBlendMode_Add,

	//!< 減算ブレンド
	kBlendMode_Sub,

	//!< 乗算ブレンド
	kBlendMode_Mul,

	//!< スクリーンブレンド
	kBlendMode_Screen,

	//利用禁止
	kCountOfBlendMode,
};

class SpriteCom
{
public:

	SpriteCom(std::ostream& logStream, DirectXCom* dxCommon);
	~SpriteCom();

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
	void DepthStencilDesc();
	
	void SetupDraw(ID3D12GraphicsCommandList* commandList);
    
    // Runtime blend mode control
    void SetBlendMode(BlendMode mode);
    BlendMode GetBlendMode() const;


public:
	const D3D12_ROOT_SIGNATURE_DESC& GetDescriptionRootSignature() const
	{
		return descriptionRootSignature;
	}
	void SetRootSignatureParameters(D3D12_ROOT_PARAMETER* parameters, UINT numParameters)
	{
		descriptionRootSignature.pParameters = parameters;
		descriptionRootSignature.NumParameters = numParameters;
	}
	void SetStaticSamplers(const D3D12_STATIC_SAMPLER_DESC* samplers, UINT numSamplers)
	{
		descriptionRootSignature.pStaticSamplers = samplers;
		descriptionRootSignature.NumStaticSamplers = numSamplers;
	}
	D3D12_ROOT_PARAMETER* GetRootParameters() { return rootParameters; }
	const D3D12_ROOT_PARAMETER* GetRootParameters() const { return rootParameters; }
	const D3D12_DESCRIPTOR_RANGE* GetDescriptorRange() const { return descriptorRange; }
	const D3D12_STATIC_SAMPLER_DESC* GetStaticSamplers() const { return staticSamplers; }
	void SetDescriptionRootSignature(const D3D12_ROOT_SIGNATURE_DESC& desc)
	{
		descriptionRootSignature = desc;
	}
	const D3D12_INPUT_LAYOUT_DESC& GetInputLayoutDesc() const
	{
		return inputLayoutDesc;
	}
	const Microsoft::WRL::ComPtr<ID3D12RootSignature>& GetRootSignature() const
	{
		return rootSignature;
	}
	const 	Microsoft::WRL::ComPtr<ID3DBlob>& GetSignatureBlob() const
	{
		return signatureBlob;
	}
	const D3D12_BLEND_DESC& GetBlendDesc() const { return blendDesc; }
	const D3D12_RASTERIZER_DESC& GetRasterizerDesc() const { return rasterizerDesc; }
	const Microsoft::WRL::ComPtr<IDxcBlob>& GetVertexShaderBlob() const
	{
		return vertexShaderBlob;
	}
	const Microsoft::WRL::ComPtr<IDxcBlob>& GetPixelShaderBlob() const
	{
		return pixelShaderBlob;
	}
	const D3D12_GRAPHICS_PIPELINE_STATE_DESC& GetGraphicPipelineStateDesc() const { return graphicPipelineStateDesc; }
	D3D12_GRAPHICS_PIPELINE_STATE_DESC& GetGraphicPipelineStateDesc() { return graphicPipelineStateDesc; }

	DirectXCom* GetDxCommon() { return dxCommon; }
	const Microsoft::WRL::ComPtr<ID3D12PipelineState>& GetPipelineState() const { return pipelineState; }


private:
	DirectXCom* dxCommon = nullptr;

  
    BlendMode currentBlendMode = kBlendMode_Normal;
    void ApplyBlendMode(BlendMode mode);

	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
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
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState = nullptr;

	std::ostream& logStream;
};