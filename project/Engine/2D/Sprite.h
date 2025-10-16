#pragma once

#include"DirectXCom.h"
#include"Matrix4x4.h"
#include"Vector.h"

class SpriteCom;

class Sprite
{
public:

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

public:

	const Microsoft::WRL::ComPtr<ID3D12Resource>& GetVertexResourceSprite() const { return vertexResourceSprite; }
	const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferViewSprite() const { return vertexBufferViewSprite; }
	const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferViewSprite() const { return indexBufferViewSprite; }
	const Microsoft::WRL::ComPtr<ID3D12Resource>& GetMaterialResourceSprite() const { return materialResourceSprite; }
	void SetUVTransform(const Matrix4x4& uv)
	{
		if (materialDataSprite)
		{
			materialDataSprite->uvTransform = uv;
		}
	}
	Material* GetMaterialDataSprite() const { return materialDataSprite; }
private:
	DirectXCom* dxCommon = nullptr;
	SpriteCom* spriteCom = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceSprite = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResourceSprite = nullptr;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
	D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};
	uint32_t* indexDataSprite = nullptr;
	VertexData* vertexDataSprite = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResourceSprite = nullptr;
	Material* materialDataSprite = nullptr;
};