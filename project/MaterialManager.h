#pragma once

#include<cstdint>
#include<wrl.h>
#include<d3d12.h>

#include"Material.h"
#include"Transform.h"

class DirectXCom;

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

	int32_t& GetMaterialDataShadingModel() { return materialData->shadingModel; }

	float& GetMaterialDataAlphaThreshold() { return materialData->alphaThreshold; }

	float& GetMaterialDataSpecularIntensity() { return materialData->specularIntensity; }

	Microsoft::WRL::ComPtr<ID3D12Resource> GetMaterialResource() { return materialResource; }

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;
	Material* materialData = nullptr;
	DirectXCom* directXCom = nullptr;
};

