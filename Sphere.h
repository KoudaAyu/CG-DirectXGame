#pragma once
#include <cstdint>
#include <d3d12.h>
#include <DirectXMath.h>
#include<wrl.h>
#include"Buffer.h"
#include"Struct.h"
class Sphere
{
public:
	Sphere();
	~Sphere();
	void Initialize(const Microsoft::WRL::ComPtr<ID3D12Device>& device, Buffer buffer);

	void CreateMapping();

	const uint32_t GetSubdivision() const { return kSubdivision; }
	const float GetLonEvery() const { return kLonEvery; }
	const float GetLatEvery() const { return kLatEvery; }
	const uint32_t GetVertexCount() const { return kVertexCount; }
	const uint32_t GetIndexCount() const { return kIndexCount; }
	const VertexData* GetVertexData() const { return vertexData; }
	const uint32_t* GetIndexData() const { return indexData; }
	const Microsoft::WRL::ComPtr<ID3D12Resource>& GetIndexResourceSphere() const
	{
		return indexResourceSphere;
	}
	const Microsoft::WRL::ComPtr<ID3D12Resource>& GetVertexResourceSphere() const 
	{ 
		return vertexResourceSphere;
	}
	const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferViewSphere() const
	{
		return indexBufferViewSphere;
	}
	const Microsoft::WRL::ComPtr<ID3D12Resource>& GetTransformationMatrixResourceSphere() const
	{
		return transformationMatrixResourceSphere;
	}
	const TransformationMatrix* GetTransformationMatrixDataSphere() const
	{
		return transformationMatrixDataSphere;
	}
	TransformationMatrix* GetTransformationMatrixDataSphere()
	{
		return transformationMatrixDataSphere;
	}

private:
	// 球体
	const uint32_t kSubdivision = 16; // 16分割

	// 経度分割1つ分の角度
	const float kLonEvery = DirectX::XM_2PI / float(kSubdivision);
	// 緯度分割1つ分の角度
	const float kLatEvery = DirectX::XM_PI / float(kSubdivision);

	// 頂点数・インデックス数
	// 緯度方向と経度方向の両端に重複する頂点があるため、+1が必要
	const uint32_t kVertexCount = (kSubdivision + 1) * (kSubdivision + 1);
	const uint32_t kIndexCount = kSubdivision * kSubdivision * 6; // 各四角形に三角形2つ、各三角形に頂点3つで 2*3=6
	
	VertexData* vertexData = nullptr; // 頂点データ
	uint32_t* indexData = nullptr; // インデックスデータ
	uint32_t* mappedIndex = nullptr;
	VertexData* mappedVertex = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResourceSphere = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceSphere = nullptr;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSphere{};
	D3D12_INDEX_BUFFER_VIEW indexBufferViewSphere{};

	VertexData* mapped = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResourceSphere;
	// データを書き込むためのポインタを取得
	TransformationMatrix* transformationMatrixDataSphere = nullptr;
};
