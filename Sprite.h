#pragma once
#include <d3d12.h>
#include <wrl.h>
#include "Buffer.h"
#include"Struct.h"
class Sprite
{
public:
	void Initialize(const Microsoft::WRL::ComPtr<ID3D12Device>& device, Buffer buffer);
	
	const Microsoft::WRL::ComPtr<ID3D12Resource>& GetVertexResourceSprite() const
	{
		return vertexResourceSprite;
	}
	const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferViewSprite() const
	{
		return vertexBufferViewSprite;
	}
	const Microsoft::WRL::ComPtr<ID3D12Resource>& GetIndexResourceSprite() const
	{
		return indexResourceSprite;
	}
	const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferViewSprite() const
	{
		return indexBufferViewSprite;
	}
	const uint32_t* GetIndexDataSprite() const
	{
		return indexDataSprite;
	}
	Microsoft::WRL::ComPtr<ID3D12Resource> GetTransformationMatrixResourceSprite()
	{
		return transformationMatrixResourceSprite;
	}
	TransformationMatrix* GetTransformationMatrixDataSprite()
	{
		return transformationMatrixDataSprite;
	}
private:
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceSprite = nullptr;
	//頂点バッファビューを生成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};

	Microsoft::WRL::ComPtr<ID3D12Resource> indexResourceSprite = nullptr;
	//頂点バッファービューを生成する
	D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};
	//インデックスリソースにデータを書き込む
	uint32_t* indexDataSprite = nullptr;

	VertexData* vertexDataSprite = nullptr;

	//Sprite用のTransformationMatrix用のリソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResourceSprite = nullptr;

	//データを書き込む
	TransformationMatrix* transformationMatrixDataSprite = nullptr;
};