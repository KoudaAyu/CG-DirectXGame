#pragma once

#include <cstdint>

#include "Matrix4x4.h"
#include "Vector.h"

struct Material
{
	Vector4 color;
	int32_t enableLighting;
	float padding[3];
	Matrix4x4 uvTransform;
	float shininess;
	int32_t shadingModel;
	float alphaThreshold;
	float specularIntensity;
};

namespace ShadingModel
{
	static constexpr int32_t Phong = 0;
	static constexpr int32_t BlinnPhong = 1;
}
