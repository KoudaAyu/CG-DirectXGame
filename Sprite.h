#pragma once
#include"Matrix4x4.h"
#include"SpriteCom.h"
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
	void Update();
	void Draw();

	Microsoft::WRL::ComPtr<ID3D12Resource> GetVertexResourceSprite() const { return vertexResourceSprite; }
	const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferViewSprite() const { return vertexBufferViewSprite; }
	Microsoft::WRL::ComPtr<ID3D12Resource> GetIndexResourceSprite() const { return indexResourceSprite; }
	const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferViewSprite() const { return indexBufferViewSprite; }
	Microsoft::WRL::ComPtr<ID3D12Resource> GetMaterialResourceSprite() const { return materialResourceSprite; }
	Material* GetMaterialDataSprite() const { return materialDataSprite; }

private:
	SpriteCom* spriteCom = nullptr;
	DirectXCom* dxCommon = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceSprite = nullptr;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResourceSprite = nullptr;
	D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};
	VertexData* vertexDataSprite = nullptr;
	uint32_t* indexDataSprite = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResourceSprite = nullptr;
	Material* materialDataSprite = nullptr;
};
