#pragma once

#include"Quaternion.h"
#include"Transform.h"

struct EulerTransform
{
	Vector3 scale;
	Vector3 rotate; //Eluerでの回転
	Vector3 translate;
};

struct QuaternionTransform
{
	Vector3 scale;
	Quaternion rotate;
	Vector3 translate;
};

