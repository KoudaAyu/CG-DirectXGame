#pragma once

#include<cstdint>
#include<wrl.h>
#include<d3d12.h>

#include"Transform.h"

class DirectXCom;

struct Material
{
	Vector4 color;
	int32_t enableLighting;
    int32_t specularModel; // 0: Blinn-Phong, 1: Phong
    float padding[2]; // パディングを追加して16バイト境界に揃える
	Matrix4x4 uvTransform; // UV変換行列
	float shininess;
	float padding2[3];
};

class MaterialManager
{
public:
	void Initialize(DirectXCom* directXCom);

	Vector4& GetMaterialDataColor() { return materialData->color; }
	void SetMaterialDataColor(const Vector4& color)
	{
		if (materialData)
		{
			materialData->color = color;
		}
	}

	int32_t& GetMaterialDataEnableLighting() { return materialData->enableLighting; }

	float& GetMaterialDataShininess() { return materialData->shininess; }

	int32_t& GetMaterialSpecularModel() { return materialData->specularModel; }

	Microsoft::WRL::ComPtr<ID3D12Resource> GetMaterialResource() { return materialResource; }

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;
	Material* materialData = nullptr;
	DirectXCom* directXCom = nullptr;
};

