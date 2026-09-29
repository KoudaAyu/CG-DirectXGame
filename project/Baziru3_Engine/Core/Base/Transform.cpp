#include "Transform.h"
#include "Matrix4x4.h"

void Transform::Initialize()
{
	scale_ = { 1.0f, 1.0f, 1.0f };
	rotation_ = { 0.0f, 0.0f, 0.0f };
	translation_ = { 0.0f, 0.0f, 0.0f };
	isDirty_ = true;
}

void Transform::Initialize(const Vector3& scale, const Vector3& rotate, const Vector3& translate)
{
	scale_ = scale;
	rotation_ = rotate;
	translation_ = translate;
	isDirty_ = true;
}

void Transform::SetScale(const Vector3& scale)
{
	if (scale_.x != scale.x || scale_.y != scale.y || scale_.z != scale.z)
	{
		scale_ = scale;
		isDirty_ = true;
	}
}

void Transform::SetRotate(const Vector3& rotate)
{
	if (rotation_.x != rotate.x || rotation_.y != rotate.y || rotation_.z != rotate.z)
	{
		rotation_ = rotate;
		isDirty_ = true;
	}
}

void Transform::SetTranslate(const Vector3& translate)
{
	if (translation_.x != translate.x || translation_.y != translate.y || translation_.z != translate.z)
	{
		translation_ = translate;
		isDirty_ = true;
	}
}

void Transform::SetTransform(const Vector3& scale, const Vector3& rotate, const Vector3& translate)
{
	SetScale(scale);
	SetRotate(rotate);
	SetTranslate(translate);
}

const Matrix4x4& Transform::GetWorldMatrix() const
{
	// 変更があった時だけ再計算してキャッシュ
	if (isDirty_)
	{
		matWorld_ = MakeAffineMatrix(scale_, rotation_, translation_);
		isDirty_ = false;
	}
	return matWorld_;
}

void Transform::TransferMatrix()
{
	// 強制的にワールド行列を再計算して最新状態に更新
	matWorld_ = MakeAffineMatrix(scale_, rotation_, translation_);
	isDirty_ = false;
}
