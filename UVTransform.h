#pragma once

#include<d3d12.h>
#include<wrl.h>

#include"Buffer.h"
#include"Matrix4x4.h"
#include"Struct.h"


class UVTransform
{
public:

	void Initialize(ID3D12Device* device);
	void Update();
	void Draw(ID3D12GraphicsCommandList* commandList);
	void DrawSprite(ID3D12GraphicsCommandList* commandList);

	Material* GetMaterialData() { return materialData; }
	Material* GetMaterialDataSprite() { return materialDataSprite; }

	Vector3& GetUVTranslateSprite() { return uvTransformSprite.translate; }
	Vector3& GetUVScaleSprite() { return uvTransformSprite.scale; }
	Vector3& GetUVRotateSprite() { return uvTransformSprite.rotate; }

private:

	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResourceSprite;
	//マテリアルにデータを書き込む
	Material* materialData = nullptr;
	Material* materialDataSprite = nullptr;
	// データを設定（赤色 RGBA: 1,0,0,1）
	Vector4 temp{};
	Transform uvTransformSprite;

	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
};