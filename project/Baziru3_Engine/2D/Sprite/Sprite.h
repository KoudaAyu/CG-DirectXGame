#pragma once

#include"DebugCamera.h"
#include"DirectXCom.h"
#include"Matrix4x4.h"
#include"Vector.h"

class SpriteCom;

class Sprite
{
public:

	struct Transform
	{
		Vector3 scale;
		Vector3 rotate;
		Vector3 translate;
	};

	struct VertexData
	{
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};

	struct Material
	{
		Vector4 color;
		int32_t enableLighting;
		float padding[3]; // パディングを追加して16バイト境界に揃える
		Matrix4x4 uvTransform; // UV変換行列
	};

	void Initialize(SpriteCom* spriteCom);
	void Update(WindowAPI* windowAPI, DebugCamera* debugCamera_);
	void Draw();

	void CreateIndexBufferView();
	void CreateVertexBufferView();
	void CreateVertexData();
	void CreateIndexData();

	void ReflectionProcessing();

public:

	const Microsoft::WRL::ComPtr<ID3D12Resource>& GetVertexResourceSprite() const { return vertexResourceSprite; }
	const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferViewSprite() const { return vertexBufferViewSprite; }
	const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferViewSprite() const { return indexBufferViewSprite; }
	const Microsoft::WRL::ComPtr<ID3D12Resource>& GetMaterialResourceSprite() const { return materialResourceSprite; }
	void SetUVTransform(const Matrix4x4& uv)
	{
		if (materialData)
		{
			materialData->uvTransform = uv;
		}
	}
	Material* GetMaterialDataSprite() const { return materialData; }
	void SetTransformationMatrix(const Matrix4x4& wvp, const Matrix4x4& world)
	{
		if (transformationMatrixDataSprite)
		{
			transformationMatrixDataSprite->WVP = wvp;
			transformationMatrixDataSprite->World = world;
		}
	}
	const Microsoft::WRL::ComPtr<ID3D12Resource>& GetTransformationMatrixResourceSprite() const { return transformationMatrixResourceSprite; }


public:
	//Spriteの座標関係
	const Vector2& GetPosition() const { return position; }
	void SetPosition(const Vector2& position)  { this->position = position; }

	//Spriteの回転関係
	const float GetRotation() const { return rotation; }
	void SetRotation(const float rotation) { this->rotation = rotation; }

	//Spriteの色
	const Vector4& GetColor() const { return materialData->color; }
	void SetColor(const Vector4& color) { materialData->color = color; }

	//Spriteの大きさ関係
	const Vector2& GetSize() const { return size; }
	void SetSize(const Vector2& size) { this->size = size; }

private:
	Transform transform{ {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
	Vector2 position = { 0.0f,0.0f };
	float rotation = 0.0f;
	Vector2 size = { 640.0f,360.0f };

private:
	DirectXCom* dxCommon = nullptr;
	SpriteCom* spriteCom = nullptr;
	VertexData* vertexData = nullptr;
	Material* materialData = nullptr;

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceSprite = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResourceSprite = nullptr;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
	D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};
	uint32_t* indexDataSprite = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResourceSprite = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResourceSprite = nullptr;
	TransformationMatrix* transformationMatrixDataSprite = nullptr;
};