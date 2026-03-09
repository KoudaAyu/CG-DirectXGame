#pragma once

#include <d3d12.h>
#include <ostream>
#include"DirectXCom.h"
#include"Matrix4x4.h"
#include"Transform.h"
#include"Log.h"
#include"Random.h"

class ParticleEmitter;

class ParticleManager
{
public:
	struct Particle
	{
		Transform transform;
		Vector3 velocity;
		Vector4 color;
		float lifeTime;
		float currentTime;
	};

	struct ParticleForGPU
	{
		Matrix4x4 WVP;
		Matrix4x4 World;
		Vector4 color;
	};


public:

	ParticleManager(std::ostream& logStream, DirectXCom* dxCommon);
	~ParticleManager();

	void Initialize();

	Particle MakeNewParticles(std::mt19937& randomEngine,const Vector3& translate);

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

	
	const Microsoft::WRL::ComPtr<ID3D12PipelineState>& GetPipelineState() const { return pipelineState; }

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
	const D3D12_DESCRIPTOR_RANGE* GetDescriptorRange() const { return descriptorRangeForInstancing; }
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

	std::mt19937& GetRandomEngine() { return randomEngine; }

	uint32_t GetNumMaxInstances() const { return kNumMaxInstances; }

	ParticleManager::ParticleForGPU* GetInstanceData() { return instanceData; }

	D3D12_GPU_DESCRIPTOR_HANDLE GetInstancingSrvHandleGPU() const { return instancingSrvHandleGPU; }

private:
	const uint32_t kNumMaxInstances = 10;


private:
	DirectXCom* dxCommon = nullptr;

	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	D3D12_DESCRIPTOR_RANGE descriptorRangeForInstancing[2] = {};
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
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicPipeline_stateDesc{};
	D3D12_GRAPHICS_PIPELINE_STATE_DESC& graphicPipelineStateDesc = graphicPipeline_stateDesc;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState = nullptr;
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU;
	Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource;

	std::mt19937 randomEngine{ std::random_device{}() };
	std::list<ParticleManager::Particle> particles;

	ParticleManager::ParticleForGPU* instanceData = nullptr;

	std::ostream& logStream;

	uint32_t instancingSrvIndex_ = 0;
};