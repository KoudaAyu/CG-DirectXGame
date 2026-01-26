#pragma once

#include"DirectXCom.h"
#include"ModelCom.h"
#include "Sprite.h"
#include "Transform.h"
#include <wrl.h>
#include <d3d12.h>
#include <vector>
#include <string>

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

public:

	void Initialize(ModelCom* modelCom, const std::string& directorypath, const std::string& filename);

	void Update();

	void Draw();

	void SetModelCom(ModelCom* modelCom) { modelCom_ = modelCom; }
	void SetDirectoryPath(const std::string& directory) { directoryPath_ = directory; }
	void SetFilename(const std::string& filename) { filename_ = filename; }
	void SetPath(const std::string& directory, const std::string& filename) { directoryPath_ = directory; filename_ = filename; }
	
	

	// Getters for main.cpp usage
	const ModelData& GetModelData() const { return modelData_; }
	const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const { return vertexBufferView_; }
	ID3D12Resource* GetMaterialResource() const { return materialResource.Get(); }
	Material* GetMaterialData() const { return materialData_; }

	/// <summary>
	/// .mtlファイルの読み込み
	/// </summary>
	/// <param name="direcrotyPath"></param>
	/// <param name="filename"></param>
	/// <returns></returns>
	static MaterialData LoadMaterialTemplateFile(const std::string& direcrotyPath, const std::string& filename);

	/// <summary>
	/// .objファイルの読み込み
	/// </summary>
	/// <param name="directoryPath">ファイルパス</param>
	/// <param name="filename">.objパス</param>
	/// <returns></returns>
	static ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);

	void VertexResource();

	void MaterialResource();


private: 

	

	ModelCom* modelCom_ = nullptr;

	ModelData modelData_;

	// 保存用パス（Setter用）
	std::string directoryPath_{};
	std::string filename_{};

	// GPU resources
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource = nullptr;
	// バッファリソース内のデータを指すポインタ (nullptr when unmapped)
	Sprite::VertexData* vertexData_ = nullptr;
	// バッファリソースの使い道を補足するバッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

	// マテリアル用リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource = nullptr;
	Material* materialData_ = nullptr;
};
