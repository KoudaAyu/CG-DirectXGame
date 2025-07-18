#pragma once
#include"Vector.h"

struct Matrix3x3
{
	float m[3][3];
};

struct Matrix4x4
{
	float m[4][4];
};

struct TransformationMatrix
{
	Matrix4x4 WVP;
	Matrix4x4 World;

};

Matrix4x4 MakeScaleMatrix(const Vector3& scale);

Matrix4x4 MakeRotateXMatrix(float radian);

Matrix4x4 Multiply(Matrix4x4 m1, Matrix4x4 m2);

Matrix4x4 Inverse(Matrix4x4 m);

Matrix4x4 MakeIdentity4x4();

Matrix4x4 MakeRotateYMatrix(float radian);

Matrix4x4 MakeRotateZMatrix(float radian);

// 平行移動行列を作成
Matrix4x4 MakeTranslateMatrix(const Vector3& translate);

Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate);

Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearZ, float farZ);

Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearZ, float farZ);