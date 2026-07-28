#pragma once

#include <d3d12.h>
#include <ostream>
#include"DirectXCom.h"
#include"Matrix4x4.h"
#include"Transform.h"
#include"Log.h"
#include"Random.h"
#include"RenderContext.h"
#include <list>
#include <memory>
#include <vector>
#include <unordered_map>

class ParticleEmitter;

class Camera;
class Ring;

class ParticleManager
{
public:
	static ParticleManager* GetInstance() { return instance_; }

public:
	struct ParticleCS
	{
		Vector3 translate;
		Vector3 scale;
		float lifeTime;
		Vector3 velocity;
		float currentTime;
		Vector4 color;
	};

	struct PerView
	{
		Matrix4x4 viewProjection;
		Matrix4x4 billboardMatrix;
		float deltaTime;
		float time;
		uint32_t maxParticles;
		float padding; // 16バイトアライメントのためのパディング
	};

	struct PerFrame
	{
		float time;
		float deltaTime;
	};

	struct GPUFieldData
	{
		Vector3 translate = { 0.0f, 0.0f, 0.0f }; // 力場の中心座標
		float radius = 5.0f;                      // 影響領域の半径
		uint32_t fieldType = 0;                   // 0: None, 1: Attractor(引き寄せ), 2: Vortex(渦), 3: Wind(風), 4: Drag(抵抗)
		float strength = 2.0f;                    // 強度
		Vector3 direction = { 1.0f, 0.0f, 0.0f }; // Windの方向
		float padding[2] = { 0.0f, 0.0f };
	};

	struct Particle
	{
		Transform transform;
		Vector3 velocity;
		Vector4 color;
		float lifeTime;
		float currentTime;
		uint32_t textureIndex = 0;
	};

	struct ParticleForGPU
	{
		Matrix4x4 WVP;
		Matrix4x4 World;
		Vector4 color;
		uint32_t textureIndex;
		uint32_t padding[3]; // 16バイトアラインメントのためのパディング
	};


public:

	ParticleManager(std::ostream& logStream, DirectXCom* dxCommon);
	~ParticleManager();

	void Initialize(Camera* camera);
	void Finalize();
	void ClearParticles();
	void Update(float deltaTime);
	void Draw(ID3D12GraphicsCommandList* commandList, const RenderContext& ctx, UINT vertexCount);

	Particle MakeNewParticles(std::mt19937& randomEngine,const Vector3& translate);
	Particle MakeHieEffect(std::mt19937& randomEngine, const Vector3& translate);




	void AddParticles(std::list<Particle>& newParticles);
	void AddEffectParticles(std::list<Particle>& newParticles);

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
	void CreateComputePipelineState();

	void SetupDraw(ID3D12GraphicsCommandList* commandList);
	void BindResources(ID3D12GraphicsCommandList* commandList, D3D12_GPU_VIRTUAL_ADDRESS materialCBV);

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
	ParticleEmitter* GetGPUEmitter() const { return gpuEmitter_.get(); }
	GPUFieldData* GetGPUFieldData() { return fieldData_; }
	void DrawUI(const std::string& windowTitle = "GPU Particle Studio / Editor");

	uint32_t GetNumMaxInstances() const { return kNumMaxInstances; }

	ParticleManager::ParticleForGPU* GetInstanceData() { return instanceData; }

	D3D12_GPU_DESCRIPTOR_HANDLE GetInstancingSrvHandleGPU() const { return instancingSrvHandleGPU; }

	uint32_t GetNumInstance() const { return numInstance; }

    struct InstanceGroup
    {
        uint32_t textureIndex = 0;
        uint32_t start = 0;
        uint32_t count = 0;
        D3D12_GPU_DESCRIPTOR_HANDLE srvHandle{};
    };

    const std::vector<InstanceGroup>& GetInstanceGroups() const { return instanceGroups; }

private:
	const uint32_t kNumMaxInstances = 256;
	uint32_t numInstance = 0;
	uint32_t writeIndex = 0;
	uint32_t normalInstanceCount_ = 0;
	uint32_t effectInstanceCount_ = 0;

private:
	DirectXCom* dxCommon = nullptr;

	Camera* camera_ = nullptr;

	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	D3D12_DESCRIPTOR_RANGE descriptorRangeForInstancing[2] = {};
	D3D12_ROOT_PARAMETER rootParameters[7] = {};
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr;
	D3D12_BLEND_DESC blendDesc{};
	D3D12_INPUT_ELEMENT_DESC inputElementDiscs[3] = {};
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
	std::unique_ptr<Ring> ring_;

	// GPU Particle リソースとディスクリプタ用インデックス
	Microsoft::WRL::ComPtr<ID3D12Resource> gpuParticleResource_ = nullptr;
	uint32_t gpuParticleUavIndex_ = 0;
	uint32_t gpuParticleSrvIndex_ = 0;
	std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> gpuParticleUavHandle_;
	std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> gpuParticleSrvHandle_;

	// GPU Particle カウンタリソースとディスクリプタ用インデックス
	Microsoft::WRL::ComPtr<ID3D12Resource> freeCounterResource_ = nullptr;
	uint32_t freeCounterUavIndex_ = 0;
	std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> freeCounterUavHandle_;

	// GPU Particle FreeListリソースとディスクリプタ用インデックス
	Microsoft::WRL::ComPtr<ID3D12Resource> freeListResource_ = nullptr;
	uint32_t freeListUavIndex_ = 0;
	std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> freeListUavHandle_;

	// PerView 用のバッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> perViewResource_ = nullptr;
	PerView* perViewData_ = nullptr;

	// PerFrame 用のバッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> perFrameResource_ = nullptr;
	PerFrame* perFrameData_ = nullptr;

	// GPU Field 用のバッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> fieldResource_ = nullptr;
	GPUFieldData* fieldData_ = nullptr;

	// Compute Pipeline
	Microsoft::WRL::ComPtr<ID3D12RootSignature> computeRootSignature_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> computePipelineState_ = nullptr;
	Microsoft::WRL::ComPtr<IDxcBlob> computeShaderBlob_;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> updateRootSignature_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> updatePipelineState_ = nullptr;
	Microsoft::WRL::ComPtr<IDxcBlob> updateShaderBlob_;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> emitRootSignature_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> emitPipelineState_ = nullptr;
	Microsoft::WRL::ComPtr<IDxcBlob> emitShaderBlob_ = nullptr;
	std::unique_ptr<ParticleEmitter> gpuEmitter_;

	static const uint32_t kMaxGPUParticles = 1024;
	static ParticleManager* instance_;

	std::mt19937 randomEngine{ std::random_device{}() };
	std::list<ParticleManager::Particle> particles;
	std::list<ParticleManager::Particle> effectParticles;

	ParticleManager::ParticleForGPU* instanceData = nullptr;

    std::vector<InstanceGroup> instanceGroups;
	std::vector<InstanceGroup> normalInstanceGroups_;
	std::vector<InstanceGroup> effectInstanceGroups_;

	std::ostream& logStream;

	uint32_t instancingSrvIndex_ = 0;
    bool finalized_ = false;
    enum class DrawMode
	{
		None,
		Ring,
		External
	};
};