#pragma once

#include <string>
#include <vector>
#include "Camera.h"
#include"Model.h"
#include"TextureManager.h"
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

	void Initialize(Object3dCom* object3dCom);

	void Update();

	void Draw();

	void VertexResource();

	void MaterialResource();

	void TransformationMatrixResource();

	void DirectionalLightResource();
	
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

	// Expose transformation matrix CBV so external code can bind the correct constant buffer
	const Microsoft::WRL::ComPtr<ID3D12Resource>& GetTransformationMatrixResource() const { return transformationMatrixResource; }

	// Transform setters/getters to control from outside (e.g., ImGui)
	void SetRotate(const Vector3& r) { transform.SetRotate(r); }
	void SetTranslate(const Vector3& t) { transform.SetTranslate(t); }
	void SetScale(const Vector3& s) { transform.SetScale(s); }
	Vector3 GetRotate() const { return transform.GetRotate(); }
	Vector3 GetTranslate() const { return transform.GetTranslate(); }
	Vector3 GetScale() const { return transform.GetScale(); }

	void SetModel(Model* model)
	{
		model_ = model;
	}
	// New getters to access model internals via Object3d
	Model* GetModel() const { return model_; }
	const Model::ModelData& GetModelData() const { return model_->GetModelData(); }
	const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const { return model_->GetVertexBufferView(); }
	ID3D12Resource* GetMaterialResource() const { return model_->GetMaterialResource(); }
	Model::Material* GetMaterialData() const { return model_->GetMaterialData(); }

private:
	Transform transform;
	Transform cameraTransform;

private:
	Camera* camera_ = nullptr;
	Object3dCom* object3dCom_ = nullptr;
	Transform transform_;

	Model* model_ = nullptr;

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