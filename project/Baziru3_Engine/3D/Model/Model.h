#pragma once


#include"Matrix4x4.h"
#include"ModelCom.h"
#include"Sprite.h"
#include"Transform.h"

class Model
{
public:
	struct Material
	{
		Vector4 color;
		int32_t enableLighting;
		float padding[3]; // パディングを追加して16バイト境界に揃える
		Matrix4x4 uvTransform; // UV変換行列
	};

	struct MaterialData
	{
		std::string textureFilePath; // テクスチャファイルのパス
		uint32_t textureIndex = 0;      // テクスチャのインデックス
	};

	//objファイル関係
	struct ModelData
	{
		std::vector<Sprite::VertexData> vertices; // 頂点データ
		MaterialData material; // マテリアルデータ
	};

	struct VertexData
	{
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};

	struct TransformationMatrixData
	{
		Matrix4x4 WVP;
		Matrix4x4 World;
	};

	struct DirectionalLight
	{
		Vector4 color;
		Vector3 direction;
		float intensity;
	};

	void Initialize(ModelCom* modelCom);
	void Initialize(ModelCom* modelCom, const std::string& objDirectory, const std::string& objFilename);

	// MTL/OBJ ローダ
	static MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);
	static ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);

	// モデル読み込みとGPUバッファ生成の高レベルAPI
	bool LoadFromObj(const std::string& directoryPath, const std::string& filename);

	// 取得系（必要に応じて使用）
	const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const { return vertexBufferView_; }
	const Microsoft::WRL::ComPtr<ID3D12Resource>& GetVertexResource() const { return vertexResource; }
	const Microsoft::WRL::ComPtr<ID3D12Resource>& GetMaterialResource() const { return materialResource; }
	const ModelData& GetModelData() const { return modelData_; }

private:
	// GPU リソース生成
	void CreateVertexBufferFromModel();
	void CreateMaterialBuffer();

private:
	ModelCom* modelCom_ = nullptr;

	//Objファイル関係
	ModelData modelData_;

	//バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource = nullptr;
	//バッファリソース内のデータを指すポインタ
	VertexData* vertexData_ = nullptr;
	//バッファリソースの使い道を補足するバッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

	//バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource = nullptr;
	//バッファリソース内のデータを指すポインタ
	Material* materialData_ = nullptr;
};