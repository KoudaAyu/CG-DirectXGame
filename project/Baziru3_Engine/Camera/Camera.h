#pragma once

#include"Transform.h"
#include <Windows.h>
#include <cstdio>
#include"Matrix4x4.h"
#include "../Base/KeyInput.h"

#include <wrl.h>
#include <d3d12.h>

struct CameraForGPU
{
	Vector3 worldPosition;
};

class DirectXCom; 

class Camera
{
public:

	Camera();
	void Update();

	void Initialize(DirectXCom* directXCom);
	
	void Finalize();

	// Enable WASD control for camera
	void SetControlEnabled(bool enabled) { controlEnabled_ = enabled; }
	bool IsControlEnabled() const { return controlEnabled_; }

public:

	const Matrix4x4& GetWorldMatrix() const
	{
		return worldMatrix_;
	}
    const Matrix4x4& GetViewMatrix() const;
	const Matrix4x4& GetProjectionMatrix() const;
	const Matrix4x4& GetViewProjectionMatrix() const;
	const Vector3& GetRotate() const { return transform_.GetRotate(); }
	const Vector3& GetTranslate() const { return transform_.GetTranslate(); }

	const Vector3& GetWorldPosition() const { return transform_.GetTranslate(); }

	void SetRotate(const Vector3& rotate)
	{
       transform_.SetRotate(rotate);
		// Debug log when camera rotation is changed
		{
			char buf[128];
			snprintf(buf, sizeof(buf), "Camera::SetRotate called: %f, %f, %f\n", rotate.x, rotate.y, rotate.z);
			OutputDebugStringA(buf);
		}
	}
	void SetTranslate(const Vector3& translate)
	{
     transform_.SetTranslate(translate);
		// Debug log when camera translation is changed
		{
			char buf[128];
			snprintf(buf, sizeof(buf), "Camera::SetTranslate called: %f, %f, %f\n", translate.x, translate.y, translate.z);
			OutputDebugStringA(buf);
		}
	}
	float GetFovY() const
	{
		return fovY_;
	}
	void SetFovY(float fovY)
	{
		fovY_ = fovY;
	}
	float GetAspectRatio() const
	{
		return aspectRatio_;
	}
	void SetAspectRatio(float aspectRatio)
	{
		aspectRatio_ = aspectRatio;
	}
	float GetNearZ() const
	{
		return nearZ_;
	}
	void SetNearZ(float nearZ)
	{
		nearZ_ = nearZ;
	}
	float GetFarZ() const
	{
		return farZ_;
	}
	void SetFarZ(float farZ)
	{
		farZ_ = farZ;
	}

	// Access to GPU-side camera data and resource
	CameraForGPU* GetCameraData() const { return cameraData; }
	Microsoft::WRL::ComPtr<ID3D12Resource> GetCameraResource() const { return cameraResource; }

	// Debug camera override: when enabled, Camera will use DebugCamera's view/projection for rendering
	void SetDebugCameraOverride(const class DebugCamera* dbg) { debugOverride_ = dbg; }
	void EnableDebugCameraOverride(bool enable) { useDebugOverride_ = enable; }
	bool IsDebugCameraOverrideEnabled() const { return useDebugOverride_; }

private:
	Transform transform_;
	// 回転
	Vector3 rotation_ = { 0.0f, 0.0f, 0.0f };
	// 移動
	Vector3 translation_ = { 0.0f, 0.0f, 0.0f };
	Matrix4x4 worldMatrix_{};
	Matrix4x4 viewMatrix_{};

	Matrix4x4 projectionMatrix_{};

	Matrix4x4 viewProjectionMatrix_{};

	//水平方向視野角
	float fovY_ = 0.45f;
	//アスペクト比
	float aspectRatio_ = 1.0f;
	//ニアクリップ距離
	float nearZ_ = 0.1f;
	//ファークリップ距離
	float farZ_ = 100.0f;

	// DirectX 関連
	DirectXCom* directXCom_ = nullptr;
    // input for camera control
	KeyInput keyInput_;
	bool controlEnabled_ = false;
    Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource;
	CameraForGPU* cameraData = nullptr;

	// Optional debug camera override pointer (not owning)
	const class DebugCamera* debugOverride_ = nullptr;
	bool useDebugOverride_ = false;

};