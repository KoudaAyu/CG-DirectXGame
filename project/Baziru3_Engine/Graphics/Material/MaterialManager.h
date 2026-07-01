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
    float reflectionFactor; // 環境マップ反射強度 (0.0 - 1.0)
    float fresnelF0;        // 基準反射率 (誘電体は通常0.04)
	Matrix4x4 uvTransform; // UV変換行列
	float shininess;
	float padding2[3];
};

class MaterialManager
{
public:
	void Initialize(DirectXCom* directXCom);

	void Finalize();

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

	float& GetMaterialReflectionFactor() { return materialData->reflectionFactor; }
	float& GetMaterialFresnelF0() { return materialData->fresnelF0; }

	Microsoft::WRL::ComPtr<ID3D12Resource> GetMaterialResource() { return materialResource; }

	void Update();

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;
    Material hostMaterial_{};
    Material* materialData = &hostMaterial_;
	DirectXCom* directXCom = nullptr;
};

