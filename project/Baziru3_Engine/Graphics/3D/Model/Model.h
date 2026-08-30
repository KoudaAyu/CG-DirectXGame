#pragma once

#include "DirectXCom.h"
#include "ModelCom.h"
#include "NodeAnimation.h"
#include "Matrix4x4.h"
#include "Sprite.h"
#include "Transform.h"

#include <wrl.h>
#include <d3d12.h>
#include <vector>
#include <string>
#include <map>
#include <cstdint>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

/**
 * @brief 3Dモデルデータの管理・描画制御を行うクラス
 */
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

	// SkinCluster 用データ
	struct VertexWeightData {
		float weight;
		std::uint32_t vertexIndex;
	};

	struct JointWeightData {
		Matrix4x4 inverseBindPoseMatrix; // 関節の逆バインド行列
		std::vector<VertexWeightData> vertexWeights; // このジョイントが影響する頂点とウェイト
	};

	//objファイル関係
	struct ModelData
	{
		std::vector<Sprite::VertexData> vertices; // 頂点データ
		std::vector<uint32_t> indices; // インデックスデータ
		MaterialData material; // マテリアルデータ
		NodeAnimation rootNode; // 階層構造のルートノード
		// ジョイント名 -> ウェイトデータ
		std::map<std::string, JointWeightData> skinClusterData;
		float boundingRadius = 2.0f; // バウンディングスフィア半径（視錐台カリング用）
	};


	

public:

	/**
	 * @brief 3Dモデルの初期化処理を行います
	 * @param modelCom 3Dモデル共通設定オブジェクトのポインタ
	 * @param directorypath モデルファイルが格納されているディレクトリパス
	 * @param filename モデルファイルのファイル名
	 */
	void Initialize(ModelCom* modelCom, const std::string& directorypath, const std::string& filename);
	void Initialize(ModelCom* modelCom, const std::string& directorypath, const std::string& filename, const ModelData& modelData);
	
	/**
	 * @brief モデルの更新処理を行います
	 */
	void Update();

	/**
	 * @brief 各種描画バッファやテクスチャをコマンドリストにバインドします
	 * @param commandList コマンドリスト
	 */
	void Bind(ID3D12GraphicsCommandList* commandList);

	/**
	 * @brief モデルの描画コマンドを発行します
	 */
	void Draw();

	void SetModelCom(ModelCom* modelCom) { modelCom_ = modelCom; }
	void SetDirectoryPath(const std::string& directory) { directoryPath_ = directory; }
	void SetFilename(const std::string& filename) { filename_ = filename; }
	void SetPath(const std::string& directory, const std::string& filename) { directoryPath_ = directory; filename_ = filename; }




	const ModelData& GetModelData() const { return modelData_; }
	const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const { return vertexBufferView_; }
	const Material* GetMaterialData() const { return &materialData_; }
	Material* GetMaterialData() { return &materialData_; }



	/// <summary>
	/// .mtlファイルの読み込み
	/// </summary>
	/// <param name="directoryPath"></param>
	/// <param name="filename"></param>
	/// <returns></returns>
	static MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);

	/// <summary>
	/// .objファイルの読み込み
	/// </summary>
	/// <param name="directoryPath">ファイルパス</param>
	/// <param name="filename">.objパス</param>
	/// <returns></returns>
	static ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);

	/**
	 * @brief GLTFやFBXなどのモデルファイルをAssimp経由でロードします（スキンウェイト対応）
	 * @param directoryPath モデルファイルが格納されているディレクトリパス
	 * @param filename モデルのファイル名
	 * @return ロードされたModelDataオブジェクト
	 */
	static ModelData LoadModelFile(const std::string& directoryPath, const std::string& filename);

	void VertexResource();


private:



	ModelCom* modelCom_ = nullptr;

	ModelData modelData_;

	// 保存用パス（Setter用）
	std::string directoryPath_{};
	std::string filename_{};

	// GPU リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource = nullptr;
	// バッファリソース内のデータを指すポインタ (nullptr when unmapped)
	Sprite::VertexData* vertexData_ = nullptr;
	// バッファリソースの使い道を補足するバッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

	// マテリアル用実データ
	Material materialData_{};

	// インデックスバッファ用リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource = nullptr;
	// インデックスバッファービュー
	D3D12_INDEX_BUFFER_VIEW indexBufferView{};

};
