#pragma once

#include <d3d12.h>
#include <ostream>
#include"DirectXCom.h"
#include"Log.h"

class SpriteCom
{
public:

	SpriteCom(std::ostream& logStream, DirectXCom* dxCommon);
	~SpriteCom();

	void Initialize();
	void Update();
	void SetupDraw();

	void CrateGraphicPipeline();

	void Descriptor();
	void CreateRootParameters();
	void RootSignature();
	void StaticSamplers();
	void InputLayer();
	void InitializeBlend();
	void RasterizerState();
	void ShaderCompile();
	void InitializeGraphicPipeline();

	void SignatureBlob();
	void RootSignatureFromBlob();


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

private:
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature;
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	D3D12_ROOT_PARAMETER rootParameters[4] = {};
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr;
	D3D12_BLEND_DESC blendDesc{};
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = nullptr;
	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = nullptr;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicPipelineStateDesc{};
	DirectXCom* dxCommon = nullptr;

	bool drawSphere = true;
	bool drawSprite = false;


	std::ostream& logStream;
};