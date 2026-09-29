#pragma once
#include "Camera.h"
#include "KeyInput.h"
#include "Matrix4x4.h"
#include "Vector.h"

class WindowAPI;
class DirectXCom;

// キーボードで自由移動・回転できるデバッグ用カメラ
class DebugCamera : public Camera
{
public: 
	DebugCamera() = default;
	~DebugCamera() override = default;

	// 初期化
	void Initialize(WindowAPI* windowAPI, DirectXCom* dxCommon = nullptr);

	// カメラの移動・回転更新
	void Update() override;

	// 互換用ゲッター
	const Matrix4x4& GetViewMatrix() const { return viewMatrix_; }
	const Matrix4x4& GetProjectionMatrix() const { return projectionMatrix_; }

private:
	WindowAPI* windowAPI_ = nullptr;
	Matrix4x4 matRot_ = {};
	KeyInput keyInput_;
	const float speed_ = 0.1f;
};
