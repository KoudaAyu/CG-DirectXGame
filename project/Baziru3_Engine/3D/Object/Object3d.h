#pragma once

#include <string>
#include <vector>
#include "Camera.h"
#include "NodeAnimation.h"
#include"TextureManager.h"
#include"MaterialManager.h"
#include "Transform.h"
#include "Sprite.h"

class Object3dCom;

class Object3d
{
public:

    // GPU用の定数バッファ(CB)レイアウトには MaterialManager.h のグローバルな `Material` を使用

	struct MaterialData
	{
		std::string textureFilePath; // テクスチャファイルのパス
		uint32_t textureIndex = 0;      // テクスチャのインデックス
	};

	//objファイル関係
	struct ModelData
	{
		std::vector<Sprite::VertexData> vertices; // 頂点データ
		std::vector<uint32_t> indices; // インデックスデータ
		MaterialData material; // マテリアルデータ
		NodeAnimation rootNode; // 階層構造のルートノード
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

	void Initialize(Object3dCom* object3dCom, const ModelData& modelData);

	void Update();

	void Draw(ID3D12GraphicsCommandList* commandList);

	/// <summary>
	/// .mtlファイルの読み込み
	/// </summary>
	/// <param name="directoryPath"></param>
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

	/// <summary>
	/// Assimp対応モデルファイルの読み込み(.gltf など)
	/// </summary>
	/// <param name="directoryPath">ファイルパス</param>
	/// <param name="filename">モデルファイル名</param>
	/// <returns></returns>
	static ModelData LoadModelFile(const std::string& directoryPath, const std::string& filename);



	void VertexResource();

	void MaterialResource();

	void TransformationMatrixResource();

	void DirectionalLightResource();

   
    ~Object3d();

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

	const Microsoft::WRL::ComPtr<ID3D12Resource>& GetTransformationMatrixResource() const { return transformationMatrixResource; }

	void SetRotate(const Vector3& r) { transform.SetRotate(r); }
	void SetTranslate(const Vector3& t) { transform.SetTranslate(t); }
	void SetScale(const Vector3& s) { transform.SetScale(s); }
	Vector3 GetRotate() const { return transform.GetRotate(); }
	Vector3 GetTranslate() const { return transform.GetTranslate(); }
	Vector3 GetScale() const { return transform.GetScale(); }
	const ModelData& GetModelData() const { return modelData_; }

private:
	Transform transform;
	Transform cameraTransform;

private:
	Camera* camera_ = nullptr;
	Object3dCom* object3dCom_ = nullptr;
	Transform transform_;

    ModelData modelData_; // モデルデータを保持

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

	// ディレクショナルライト用のバッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource = nullptr;
	// バッファリソース内のデータを指すポインタ
	DirectionalLight* directionalLightData_ = nullptr;
};