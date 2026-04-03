#pragma once

#include <string>
#include <vector>
#include "Camera.h"
#include "Material.h"
#include"TextureManager.h"
#include "Transform.h"
#include "Sprite.h"

class Object3dCom;

class Object3d
{
public:
	using VertexData = Sprite::VertexData;

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



	void VertexResource();

	void MaterialResource();

	void TransformationMatrixResource();

	void DirectionalLightResource();

	void UpdateWorldMatrixCache();

   
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
	Material* GetMaterialData() { return materialData_; }
	const Material* GetMaterialData() const { return materialData_; }
	void SetEnableLighting(bool enable)
	{
		if (materialData_)
		{
			materialData_->enableLighting = enable ? 1 : 0;
		}
	}
	void SetShininess(float shininess)
	{
		if (materialData_)
		{
			materialData_->shininess = shininess;
		}
	}
	void SetShadingModel(int32_t shadingModel)
	{
		if (materialData_)
		{
			materialData_->shadingModel = shadingModel;
		}
	}
	void SetAlphaThreshold(float alphaThreshold)
	{
		if (materialData_)
		{
			materialData_->alphaThreshold = alphaThreshold;
		}
	}
	void SetSpecularIntensity(float specularIntensity)
	{
		if (materialData_)
		{
			materialData_->specularIntensity = specularIntensity;
		}
	}

	void SetRotate(const Vector3& r) { transform.SetRotate(r); isWorldMatrixDirty_ = true; }
	void SetTranslate(const Vector3& t) { transform.SetTranslate(t); isWorldMatrixDirty_ = true; }
	void SetScale(const Vector3& s) { transform.SetScale(s); isWorldMatrixDirty_ = true; }
	Vector3 GetRotate() const { return transform.GetRotate(); }
	Vector3 GetTranslate() const { return transform.GetTranslate(); }
	Vector3 GetScale() const { return transform.GetScale(); }

private:
	Transform transform;
	Transform cameraTransform;

private:
	Camera* camera_ = nullptr;
	Object3dCom* object3dCom_ = nullptr;
	Transform transform_;

	ModelData modelData_; // store model data

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

	Matrix4x4 cachedWorldMatrix_ = MakeIdentity4x4();
	Matrix4x4 cachedWorldInverseTranspose_ = MakeIdentity4x4();
	bool isWorldMatrixDirty_ = true;
};