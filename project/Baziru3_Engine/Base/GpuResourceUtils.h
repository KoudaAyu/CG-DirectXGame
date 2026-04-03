#pragma once

#include <cassert>

#include "DirectXCom.h"
#include "Material.h"

class GpuResourceUtils
{
public:
	template <class T>
	static Microsoft::WRL::ComPtr<ID3D12Resource> CreateMappedBuffer(DirectXCom* dxCommon, T*& mappedData, size_t elementCount = 1)
	{
		assert(dxCommon);
		auto resource = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), sizeof(T) * elementCount);
		resource->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));
		return resource;
	}

	static void InitializeMaterial(Material* material, bool enableLighting = false)
	{
		assert(material);
		material->color = { 1.0f, 1.0f, 1.0f, 1.0f };
		material->enableLighting = enableLighting ? 1 : 0;
		material->uvTransform = MakeIdentity4x4();
		material->shininess = 32.0f;
		material->shadingModel = ShadingModel::BlinnPhong;
		material->alphaThreshold = 0.1f;
		material->specularIntensity = 1.0f;
	}

	static void InitializeTransformationMatrix(TransformationMatrix* transformation)
	{
		assert(transformation);
		transformation->WVP = MakeIdentity4x4();
		transformation->World = MakeIdentity4x4();
		transformation->WorldInverseTranspose = MakeIdentity4x4();
	}
};
