#pragma once
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

	void Initialize(SpriteCom* spriteCom);
	void Update();
	void Draw();

	Microsoft::WRL::ComPtr<ID3D12Resource> GetVertexResourceSprite() const { return vertexResourceSprite; }
	const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferViewSprite() const { return vertexBufferViewSprite; }
	Microsoft::WRL::ComPtr<ID3D12Resource> GetIndexResourceSprite() const { return indexResourceSprite; }
	const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferViewSprite() const { return indexBufferViewSprite; }



private:
	SpriteCom* spriteCom = nullptr;
	DirectXCom* dxCommon = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceSprite = nullptr;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResourceSprite = nullptr;
	D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};

};
