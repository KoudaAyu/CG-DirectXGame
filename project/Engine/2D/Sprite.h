#pragma once
#include"DebugCamera.h"
#include"Matrix4x4.h"
#include"SpriteCom.h"
#include"Vector.h"
#include"WindowsAPI.h"


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
	void Update(DebugCamera* debugCamera, WindowAPI* windowAPI);
	void Draw();

	void VertexDataInitialize();
	void VertexDataCreate();
	void IndexDataCreate();

	Microsoft::WRL::ComPtr<ID3D12Resource> GetVertexResourceSprite() const { return vertexResourceSprite; }
	const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferViewSprite() const { return vertexBufferViewSprite; }
	Microsoft::WRL::ComPtr<ID3D12Resource> GetIndexResourceSprite() const { return indexResourceSprite; }
	const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferViewSprite() const { return indexBufferViewSprite; }
	Microsoft::WRL::ComPtr<ID3D12Resource> GetMaterialResourceSprite() const { return materialResourceSprite; }
	Material* GetMaterialDataSprite() const { return materialDataSprite; }
	Microsoft::WRL::ComPtr<ID3D12Resource> GetTransformationMatrixResourceSprite() const { return transformationMatrixResourceSprite; }
	TransformationMatrix* GetTransformationMatrixDataSprite() const { return transformationMatrixDataSprite; }


public:
	const Vector2& GetPosition() const { return position; }
	void SetPosition(const Vector2& pos) { position = pos; }

	float GetRotation() const { return rotation; }
	void SetRotation(float rot) { rotation = rot; }

	const Vector4& GetColor() const { return materialDataSprite->color; }
	void SetColor(const Vector4& col) { materialDataSprite->color = col; }

	const Vector2& GetSize() const { return size; }
	void SetSize(const Vector2& s) { size = s; }

private:
	//Sprite用
	Transform transformSprite;

	Vector2 position = { 0.0f,0.0f };
	float rotation = 0.0f;
	Vector2 size = { 100.0f,100.0f };

private:
	DebugCamera* debugCamera = nullptr;
	SpriteCom* spriteCom = nullptr;
	DirectXCom* dxCommon = nullptr;
	WindowAPI* windowAPI = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceSprite = nullptr;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResourceSprite = nullptr;
	D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};
	VertexData* vertexDataSprite = nullptr;
	uint32_t* indexDataSprite = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResourceSprite = nullptr;
	Material* materialDataSprite = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResourceSprite = nullptr;
	TransformationMatrix* transformationMatrixDataSprite = nullptr;
};