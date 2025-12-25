#pragma once
#include"DirectXCom.h"
#include"SRVManager.h"
#include"Random.h"
#include"Transform.h"

#include<list>
#include<string>
#include<chrono>
#include<unordered_map>

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

	// HLSL の StructuredBuffer<InstanceData> に合わせたレイアウト
	struct InstanceData
	{
		Matrix4x4 WVP;
		Matrix4x4 World;
		float alpha;
		float pad[3];
		// Pixel shaderがcolorを使わない構成なら省略可能だが、将来拡張用に保持
		Vector4 color;
	};

	struct ParticleGroup
	{
		std::string textureFilePath_; // テクスチャファイルパス
		uint32_t textureSRVIndex_ = 0; // テクスチャ用SRVインデックス
		std::list<Particle> particles; // パーティクルリスト
		uint32_t instanceSRVIndex_ = 0;// パーティクルのインスタンスSRVインデックス
		Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource_ = nullptr;// パーティクルのインスタンス
		uint32_t instanceCount_ = 0;// パーティクルのインスタンス数
		void* instanceMapped_ = nullptr;// 韻スタン寝具データを書き込むためのアドレス
		// keep intermediate upload buffer alive until safe
		Microsoft::WRL::ComPtr<ID3D12Resource> uploadIntermediate_ = nullptr;
		// hold texture resource so SRV target is not destroyed
		Microsoft::WRL::ComPtr<ID3D12Resource> textureResource_ = nullptr;
	};

	struct VertexData
	{
		Vector4 position;
		Vector2 texcoord;
	};

public:

	ParticleManager(std::ostream& logStream, DirectXCom* dxCommon);
	~ParticleManager();

	void Initialize(DirectXCom* dxCommon, SRVManager* srvManager);
	void Update();
	void Draw();

	void SetupDraw();

	void CreateParticleGroup(const std::string& name, const std::string& textureFilePath);

	void SetGlobalOffset(const Vector3& offset) { globalOffset_ = offset; }

	static ParticleManager* GetInstance() { return instance_; }

private:

	DirectXCom* dxCommon_ = nullptr;
	SRVManager* srvManager_ = nullptr;

	//ランダムエンジン
	std::mt19937 random_{};

	void CreatePipeline();
	void RootSignature();
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

	void InitializeVertexData();
	void CreateVertexResource();
	void CreateVertexBufferView();

	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
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
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState = nullptr;

	std::ostream& logStream;

	std::vector<VertexData> vertices_;
	D3D12_VERTEX_BUFFER_VIEW vbv_{};


	size_t vertexBufferSize = 0;

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;


	std::unordered_map<std::string, ParticleGroup> particleGroups_;


	std::chrono::steady_clock::time_point lastUpdateTime_{};

	Vector3 globalOffset_{ 0.0f, 0.0f, 0.0f };

	static ParticleManager* instance_;
};