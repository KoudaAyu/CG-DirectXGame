#pragma once
#include <cstdint>
#include <DirectXMath.h>
#include"DirectXCom.h"
#include"Sprite.h"
#include <vector>

class Camera;
class Object3dCom;
class MaterialManager;
class Light;


class Sphere
{
public:
	void Initialize(DirectXCom* dxCommon, Object3dCom* object3dCom, MaterialManager* materialManager, Light* light, Camera* camera);
	void Update();
	void Draw(D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle);

public:
	
	Microsoft::WRL::ComPtr<ID3D12Resource> GetVertexResourceSphere() const { return vertexResourceSphere; }
	const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferViewSphere() const { return vertexBufferViewSphere; }
	const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferViewSphere() const { return indexBufferViewSphere; }
	const uint32_t GetIndexCount() const { return kIndexCount; }
	TransformationMatrix* GetTransformationMatrixDataSphere() const { return transformationMatrixDataSphere; }
	Microsoft::WRL::ComPtr<ID3D12Resource> GetTransformationMatrixResourceSphere() const { return transformationMatrixResourceSphere; }
	void SetTransform(const Sprite::Transform& transform) { this->transform = transform; }
	Sprite::Transform& GetTransform() { return transform; }
private:
	DirectXCom* directXCom_ = nullptr;
	Object3dCom* object3dCom_ = nullptr;
	MaterialManager* materialManager_ = nullptr;
	Light* light_ = nullptr;
	Camera* camera_ = nullptr;

	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle{};
private:
	// 球体
	static constexpr uint32_t kSubdivision = 16; // 16分割

	// 経度分割1つ分の角度
	static constexpr float kLonEvery = DirectX::XM_2PI / float(kSubdivision);
	// 緯度分割1つ分の角度
	static constexpr float kLatEvery = DirectX::XM_PI / float(kSubdivision);

	// 頂点数・インデックス数
	// 緯度方向と経度方向の両端に重複する頂点があるため、+1が必要
	static constexpr uint32_t kVertexCount = (kSubdivision + 1) * (kSubdivision + 1);
	static constexpr uint32_t kIndexCount = kSubdivision * kSubdivision * 6; // 各四角形に三角形2つ、各三角形に頂 vertex 3つで 2*3=6
	// 頂点配列を確保
	std::vector<Sprite::VertexData> vertexData = std::vector<Sprite::VertexData>(kVertexCount);

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceSphere;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResourceSphere;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSphere{};
	D3D12_INDEX_BUFFER_VIEW indexBufferViewSphere{};
	Sprite::VertexData* mapped = nullptr;

	TransformationMatrix* transformationMatrixDataSphere = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResourceSphere;

	Sprite::Transform transform = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
	Matrix4x4 worldMatrix;
	Matrix4x4 viewMatrix;
	Matrix4x4 WVPMatrix;
};

