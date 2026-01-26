#include "Transform.h"
#include"Matrix4x4.h"

void Transform::Initialize()
{
	scale_ = { 1.0f,1.0f,1.0f };
	rotation_ = { 0.0f,0.0f,0.0f };
	translation_ = { 0.0f,0.0f,0.0f };
}

void Transform::Initialize(const Vector3& scale, const Vector3 rotate, const Vector3 translate)
{
	scale_ = scale;
	rotation_ = rotate;
	translation_ = translate;
}

void Transform::SetScale(const Vector3& scale)
{
	scale_ = scale;
}

void Transform::SetRotate(const Vector3 rotate)
{
	rotation_ = rotate;
}

void Transform::SetTranslate(const Vector3 translate)
{
	translation_ = translate;
}

void Transform::SetTransform(const Vector3& scale, const Vector3 rotate, const Vector3 translate)
{
	scale_ = scale;
	rotation_ = rotate;
	translation_ = translate;
}



void Transform::TransferMatrix()
{
	matWorld_ = MakeAffineMatrix(scale_, rotation_, translation_);
}