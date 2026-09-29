#pragma once
#include "Vector.h"
#include "Matrix4x4.h"

// 拡縮・回転・座標およびワールド行列を管理するクラス
class Transform
{
public:
	Transform() = default;
	Transform(const Vector3& scale, const Vector3& rotate, const Vector3& translate)
		: scale_(scale), rotation_(rotate), translation_(translate), isDirty_(true)
	{
	}

	void Initialize();
	void Initialize(const Vector3& scale, const Vector3& rotate, const Vector3& translate);

	// 各種パラメータの設定（値が変わったときのみ再計算フラグを立てる）
	void SetScale(const Vector3& scale);
	void SetRotate(const Vector3& rotate);
	void SetTranslate(const Vector3& translate);
	void SetTransform(const Vector3& scale, const Vector3& rotate, const Vector3& translate);

	const Vector3& GetScale() const { return scale_; }
	const Vector3& GetRotate() const { return rotation_; }
	const Vector3& GetTranslate() const { return translation_; }

	// ワールド行列の取得（値が変化した時のみ再計算してキャッシュを返す）
	const Matrix4x4& GetWorldMatrix() const;

	// 行列の強制再計算
	void TransferMatrix();

	bool IsDirty() const { return isDirty_; }
	void MarkDirty() { isDirty_ = true; }

private:
	// 値の変更を検知して不要な行列再計算をスキップするためのフラグ
	mutable bool isDirty_ = true;
	mutable Matrix4x4 matWorld_{};

	Vector3 scale_{ 1.0f, 1.0f, 1.0f };
	Vector3 rotation_{ 0.0f, 0.0f, 0.0f };
	Vector3 translation_{ 0.0f, 0.0f, 0.0f };
};
