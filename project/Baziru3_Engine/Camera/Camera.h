#pragma once

#include"Transform.h"
#include"Matrix4x4.h"

#include <wrl.h>
#include <d3d12.h>

struct CameraForGPU
{
	Vector3 worldPosition;
};

class DirectXCom; // forward

class Camera
{
public:

	Camera();
	void Update();

	void Initialize(DirectXCom* directXCom);
	
	void Finalize();

public:

	const Matrix4x4& GetWorldMatrix() const
	{
		return worldMatrix_;
	}
	const Matrix4x4& GetViewMatrix() const
	{
		return viewMatrix_;
	}
	const Matrix4x4& GetProjectionMatrix() const
	{
		return projectionMatrix_;
	}
	const Matrix4x4& GetViewProjectionMatrix() const
	{
		return viewProjectionMatrix_;
	}
	const Vector3& GetRotate() const { return rotation_; }
	const Vector3& GetTranslate() const { return translation_; }
	const Vector3& GetWorldPosition() const { return transform_.GetTranslate(); }

	void SetRotate(const Vector3& rotate)
	{
		transform_.SetRotate(rotate);
	}
	void SetTranslate(const Vector3& translate)
	{
		transform_.SetTranslate(translate);
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
	Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource;
	CameraForGPU* cameraData = nullptr;

};