#pragma once

#include"DebugCamera.h"
#include"DirectXCom.h"
#include"MaterialManager.h"
#include"Matrix4x4.h"
#include"Vector.h"
#include <memory>
#include "MappedResource.h"

class SpriteCom;
class TextureManager;

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

	Sprite();
	~Sprite();

	void Initialize(SpriteCom* spriteCom,std::string textureFilePath);
	void Update(WindowAPI* windowAPI = nullptr, DebugCamera* debugCamera_ = nullptr);
    // テクスチャ/UV の重い処理を行わずに変換行列のみを更新する軽量メソッド
	void UpdateTransformOnly(WindowAPI* windowAPI);
	void Draw();

	void Finalize();

	void CreateIndexBufferView();
	void CreateVertexBufferView();
	void CreateVertexData();
	void CreateIndexData();

	void ReflectionProcessing();

	// インデックスから生成する簡易ヘルパー
	static std::unique_ptr<Sprite> Create(SpriteCom* spriteCom,
		uint32_t textureHandle,
		const Vector2& position);

	// オーバーロード: Transform とテクスチャパスで生成するヘルパー
	static std::unique_ptr<Sprite> Create(SpriteCom* spriteCom,
		const Sprite::Transform& transform,
		const std::string& texturePath);


public:


	Microsoft::WRL::ComPtr<ID3D12Resource> GetVertexResourceSprite() const { return vertexResourceSprite; }
	const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferViewSprite() const { return vertexBufferViewSprite; }
	const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferViewSprite() const { return indexBufferViewSprite; }

	Microsoft::WRL::ComPtr<ID3D12Resource> GetMaterialResourceSprite() const { return materialResourceSprite; }
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

	const Vector2& GetAnchorPoint() const { return anchorPoint; }
	void SetAnchorPoint(const Vector2& anchorPoint) { this->anchorPoint = anchorPoint; }

	void SetTextureLeftTop(const Vector2& leftTop) { textureLeftTop = leftTop; }

	void SetTextureSize(const Vector2& size) { textureSize = size; }

	void SetTextureHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle) { textureHandleGPU = handle; }
	
	void SetDirectionalLightResource(const Microsoft::WRL::ComPtr<ID3D12Resource>& light) { directionalLightResource = light; }

	
	void SetUVParams(const Vector3& scale, float rotZ, const Vector3& translate);

private:
	void AdjustTextureSize();
	void RecalculateUVMatrix();

private:
	Transform transform{ {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
	Vector2 position = { 0.0f,0.0f };
	float rotation = 0.0f;
	Vector2 size = { 640.0f,360.0f };
	Vector2 anchorPoint = { 0.0f,0.0f };
	bool isFlipX_ = false;
	bool isFlipY_ = false;
	//テクスチャ左上座標
	Vector2 textureLeftTop = { 0.0f,0.0f };
	Vector2 textureSize = { 100.0f,100.0f };

	
	Transform uvParams{ {1.0f,1.0f,1.0f}, {0.0f,0.0f,0.0f}, {0.0f,0.0f,0.0f} };
	bool uvDirty = true;

private:
	DirectXCom* dxCommon = nullptr;
	SpriteCom* spriteCom = nullptr;
    VertexData* vertexData = nullptr;
    Material* materialData = nullptr;
  
    Baziru3::PersistentMap<VertexData> vertexMap;
    Baziru3::PersistentMap<Material> materialMap;
    Baziru3::PersistentMap<TransformationMatrix> transformationMatrixMap;

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceSprite = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResourceSprite = nullptr;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
	D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};
	uint32_t* indexDataSprite = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResourceSprite = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResourceSprite = nullptr;
	TransformationMatrix* transformationMatrixDataSprite = nullptr;

	// 新規: 描画時に使うハンドル/リソース
	D3D12_GPU_DESCRIPTOR_HANDLE textureHandleGPU{};
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource = nullptr;

	uint32_t textureIndex = 0;
};