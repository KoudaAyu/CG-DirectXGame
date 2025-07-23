#pragma once
#include"KeyInput.h"
#include"Matrix4x4.h"
#include "Vector.h"

class DebugCamera
{
public: 
	void Initialize(HINSTANCE hInstance, HWND hwnd);

	void Update();

	const Matrix4x4& GetViewMatrix() const { return view_matrix_; }
	const Matrix4x4& GetProjectionMatrix() const { return projection_matrix_; }

private:
	Vector3 rotation_ = { 0.0f,0.0f,0.0f };
	Vector3 translation_ = { 0.0f,0.0f,-50.0f };

	//ビュー行列
	Matrix4x4 view_matrix_ = {};
	//射影行列
	Matrix4x4 projection_matrix_ = {};

	//累積回転行列
	Matrix4x4 matRot_;

	KeyInput keyInput_;

	const float speed = 0.1f;

	float fovY = 0.45f;  // 資料通り
	float aspectRatio = static_cast<float>(1280) / static_cast<float>(720);
	float nearZ = 0.1f;
	float farZ = 100.0f;
};
