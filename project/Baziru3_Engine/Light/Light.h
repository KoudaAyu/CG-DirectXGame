#pragma once
#include"Transform.h"

struct PointLight
{
    Vector4 color;
    Vector3 position;
    float intensity;
    float radius;
    float decay;
};

struct SpotLight
{
    Vector4 color;
    Vector3 position;
	float intensity;
    Vector3 direction;
    float distance;
    float decay;
    float cosAngle;
    float padding[2];
};