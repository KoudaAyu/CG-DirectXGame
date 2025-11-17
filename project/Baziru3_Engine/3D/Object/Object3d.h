#pragma once

#include <string>
#include <vector>
#include "Camera.h"
#include "Transform.h"
#include "Sprite.h"

class Object3dCom;

class Object3d
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

	void Initialize(Object3dCom* object3dCom);

	void Update();

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

	void CreateVertexResource();

	void MaterialResource();

	void TransformationMatrixResource();
	
public:
	void SetCamera(Camera* camera)
	{
		camera_ = camera;
	}
	Camera* GetCamera() const
	{
		return camera_;
	}
	
	void SetObject3dCom(Object3dCom* object3dCom)
	{
		object3dCom_ = object3dCom;
	}
private:
	Camera* camera_ = nullptr;
	Object3dCom* object3dCom_ = nullptr;
	Transform transform_;

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

	//バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource = nullptr;
	//バッファリソース内のデータを指すポインタ
	TransformationMatrix* transformationMatrixData_ = nullptr;
};