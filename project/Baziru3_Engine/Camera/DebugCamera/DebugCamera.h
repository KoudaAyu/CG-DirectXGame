#pragma once
#include"KeyInput.h"
#include"Matrix4x4.h"
#include "Vector.h"
#include <string>

class DebugCamera
{
public: 
	void Initialize(WindowAPI* windowAPI);

	void Update();

	// Accessors for editor
	const Vector3& GetRotation() const { return rotation_; }
  const Vector3& GetTranslation() const { return translation_; }
	void SetRotation(const Vector3& r) { rotation_ = r; \
		{ char buf[128]; std::snprintf(buf, sizeof(buf), "DebugCamera::SetRotation: %f, %f, %f\n", r.x, r.y, r.z); OutputDebugStringA(buf); } }
	void SetTranslation(const Vector3& t) { translation_ = t; \
		{ char buf[128]; std::snprintf(buf, sizeof(buf), "DebugCamera::SetTranslation: %f, %f, %f\n", t.x, t.y, t.z); OutputDebugStringA(buf); } }

	// Persistence
	bool LoadFromFile(const std::string& path);
	bool SaveToFile(const std::string& path) const;

	// Reset to default
	void Reset();

	// Enable/disable responding to WASD
	void SetControlEnabled(bool enabled) { controlEnabled_ = enabled; }
	bool IsControlEnabled() const { return controlEnabled_; }

	// Projection accessors
	float GetFovY() const { return fovY; }
	void SetFovY(float f) { fovY = f; }
	float GetNearZ() const { return nearZ; }
	void SetNearZ(float n) { nearZ = n; }
	float GetFarZ() const { return farZ; }
	void SetFarZ(float f) { farZ = f; }

	const Matrix4x4& GetViewMatrix() const { return view_matrix_; }
	const Matrix4x4& GetProjectionMatrix() const { return projection_matrix_; }

private:

	WindowAPI* windowAPI = nullptr;

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

    bool controlEnabled_ = false;

	float fovY = 0.45f;  // 資料通り
	float aspectRatio = static_cast<float>(1280) / static_cast<float>(720);
	float nearZ = 0.1f;
	float farZ = 100.0f;
};
