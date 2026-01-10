#pragma once
#define NOMINMAX
#include <Windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <vector>
#include <random>
#include <list>
#include <string>
#include "Vector.h"
#include <unordered_map>
#include "Matrix4x4.h"
#include "Camera.h"
#include"TextureManager.h"
#include "DirectXCom.h"
#include"Object3dCom.h"
#include "Model.h"
#include "ModelCom.h"

class DirectXCom;
class SrvManager;

struct Particle
{
	Vector3 position;
	Vector3 velocity;
	float   lifeTime;
	float   current;
	Vector4 color;
	float   scale;
	bool IsAlive() const { return current < lifeTime; }
};

struct ParticleGroup
{
	// マテリアルデータ（テクスチャファイルパス と テクスチャ用SRVインデックス）
	std::string textureFilePath;
	uint32_t    textureSrvIndex = 0;

	// パーティクルのリスト（std::list<Particle> 型）
	std::list<Particle> particles;

	// インスタンシングデータ用SRVインデックス
	uint32_t instanceSrvIndex = 0;

	// インスタンシング用リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> instanceResource;

	// インスタンス数
	uint32_t instanceCount = 0;

	// インスタンシングデータを書き込むためのポインタ
	void* instanceMappedPtr = nullptr;

	bool useMesh = false;
	Model* model = nullptr;
};



class ParticleManager
{
public:
	// シングルトン
	static ParticleManager* GetInstance();

	// 初期化処理（スライドの指示通りの引数）
	void Initialize(DirectXCom* dx, SrvManager* srvMgr, Object3dCom* object3dCom);

	// 更新処理
	void Update(const Matrix4x4& view, const Matrix4x4& projection);

	// ParticleManager.h に既出の宣言が無ければ追加
	void Draw();

	// パーティクルの発生
	void Emit(const std::string name, const Vector3& position, uint32_t count);
	void Finalize();

	// パーティクルグループの生成
	void CreateParticleGroup(const std::string name, const std::string textureFilePath);
	void CreateParticleGroupFromModel(const std::string& name, const std::string& modelPath);

private:
	ParticleManager() = default;
	~ParticleManager() = default;
	ParticleManager(const ParticleManager&) = delete;
	ParticleManager& operator=(const ParticleManager&) = delete;
	static ParticleManager* instance;

	// 引数で受け取ったポインタをメンバに記録する
	DirectXCom* dx_ = nullptr;
	SrvManager* srvMgr_ = nullptr;

	// ランダムエンジンの初期化
	std::mt19937 rng_{};

	// パイプライン生成（この段階ではRootSignatureだけ作成）
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> pso_;

	// 頂点データの初期化（座標等）→ 頂点リソース生成 → VBV作成 → リソースに書き込む
	struct Vertex { float x, y, z, w; float u, v; };
	std::vector<Vertex> vertices_;

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_; // 頂点リソース
	D3D12_VERTEX_BUFFER_VIEW vbv_{};                      // VBV

	// ユーザが付けるグループ名をキーに、複数グループを保持
	std::unordered_map<std::string, ParticleGroup> particleGroups;

	struct TransformationMatrix { Matrix4x4 WVP; Matrix4x4 World; };

	Microsoft::WRL::ComPtr<ID3D12Resource> meshTransformCB_;
	TransformationMatrix* meshTransformPtr_ = nullptr;

	// （照明CBが未バインドだとデバッグ層が警告する場合があるのでダミーも用意）
	Microsoft::WRL::ComPtr<ID3D12Resource> meshLightCB_;
	struct DirectionalLight { Vector4 color; Vector3 direction; float intensity; };
	DirectionalLight* meshLightPtr_ = nullptr;

	// Object3dCom を借りるためのポインタ（Initialize時にもらう）
	Object3dCom* object3dCom_ = nullptr;

	Matrix4x4 viewProj_;

	// 1 draw = 1 slot
	static constexpr UINT kMaxMeshCB = 2048;
	static constexpr UINT kCBAlign = 256;
	static constexpr UINT AlignedCBSize = (sizeof(TransformationMatrix) + (kCBAlign - 1)) & ~(kCBAlign - 1);


	uint8_t* meshTransformCBBase_ = nullptr; // 先頭ポインタ（生ポインタでOK）
	uint32_t meshCBWriteIndex_ = 0;          // 今フレームの書き込み開始位置


private:
	void CreatePipeline_();          // パイプライン生成（RootSignatureのみ）
};
