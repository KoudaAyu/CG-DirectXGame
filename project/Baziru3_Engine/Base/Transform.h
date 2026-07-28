#pragma once
#include "Vector.h"
#include "Matrix4x4.h"


class Transform
{
public:
	// デフォルトコンストラクタ
	Transform() = default;
	// スケール・回転・平行移動をまとめて指定できるコンストラクタ
	Transform(const Vector3& scale, const Vector3& rotate, const Vector3& translate)
		: scale_(scale), rotation_(rotate), translation_(translate)
	{
	}

	void Initialize();
	void Initialize(const Vector3& scale, const Vector3 rotate, const Vector3 translate);


	void SetScale(const Vector3& scale);
	void SetRotate(const Vector3 rotate);
	void SetTranslate(const Vector3 translate);

	void SetTransform(const Vector3& scale, const Vector3 rotate, const Vector3 translate);

	const Vector3& GetScale() const { return scale_; }
	const Vector3& GetRotate() const { return rotation_; }
	const Vector3& GetTranslate() const { return translation_; }

	void TransferMatrix();

	// 直接アクセスされるケースが多いため公開

private:
	Matrix4x4 matWorld_{};

	Vector3 scale_{ 1.0f,1.0f,1.0f };
	Vector3 rotation_{ 0.0f,0.0f,0.0f };
	Vector3 translation_{ 0.0f,0.0f,0.0f };

};
