#include"Camera.h"
#include "Camera.h"
#include"DirectXCom.h"
#include"WindowsAPI.h"
#include "DebugCamera/DebugCamera.h"
#include <assert.h>

Camera::Camera()
	: transform_({ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -10.0f} }),
	fovY_(0.45f),
	aspectRatio_(float(WindowAPI::GetClientWidth()) / float(WindowAPI::GetClientHeight())),
	nearZ_(0.1f), farZ_(100.0f),
	worldMatrix_(MakeAffineMatrix(transform_.GetScale(), transform_.GetRotate(),
		transform_.GetTranslate())),
	viewMatrix_(Inverse(worldMatrix_)),
	projectionMatrix_(
		MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearZ_, farZ_)),
	viewProjectionMatrix_(Multiply(viewMatrix_, projectionMatrix_))
{
}

void Camera::Initialize(DirectXCom* directXCom)
{
	directXCom_ = directXCom;

	//カメラ用のGPU定義バッファサイズは256バイト境界に揃える
	const size_t cameraBufferSize = (sizeof(CameraForGPU) + 255) & ~size_t(255);

    // --- カメラ用のリソース作成を追加 ---
    // 256 バイト境界に揃えたサイズを使う
    cameraResource = directXCom_->CreateBufferResource(directXCom_->GetDevice().Get(), cameraBufferSize);
	
	cameraResource->Map(0, nullptr, reinterpret_cast<void**>(&cameraData));
	// 初期値を設定
	if (cameraData)
	{
		*cameraData = CameraForGPU{};
		cameraData->worldPosition = transform_.GetTranslate();
	}

	// Initialize input for optional camera control
	if (directXCom_ && directXCom_->GetWindowAPI())
	{
		keyInput_.Initialize(directXCom_->GetWindowAPI());
	}
}

void Camera::Finalize()
{
	if (cameraResource && cameraData)
	{
		cameraResource->Unmap(0, nullptr);
		cameraData = nullptr;
	}
	cameraResource.Reset();
	directXCom_ = nullptr;
}

void Camera::Update()
{
    keyInput_.Update();

	if (controlEnabled_)
	{
		// Simple WASD movement in world X/Z
		Vector3 pos = transform_.GetTranslate();
		const float moveSpeed = 0.1f;
		if (keyInput_.IsKeyPressed(DIK_D))
		{
			pos.x += moveSpeed;
		}
		if (keyInput_.IsKeyPressed(DIK_A))
		{
			pos.x -= moveSpeed;
		}
		if (keyInput_.IsKeyPressed(DIK_W))
		{
			pos.z += moveSpeed;
		}
		if (keyInput_.IsKeyPressed(DIK_S))
		{
			pos.z -= moveSpeed;
		}
		transform_.SetTranslate(pos);
	}

	worldMatrix_ = MakeAffineMatrix(transform_.GetScale(), transform_.GetRotate(), transform_.GetTranslate());
	viewMatrix_ = Inverse(worldMatrix_);

	projectionMatrix_ =
		MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearZ_, farZ_);

	viewProjectionMatrix_ = Multiply(viewMatrix_, projectionMatrix_);

    assert(cameraData);

    if (cameraData)
	{
		if (useDebugOverride_ && debugOverride_)
		{
			cameraData->worldPosition = debugOverride_->GetTranslation();
		}
		else
		{
			cameraData->worldPosition = transform_.GetTranslate();
		}
	}
}

const Matrix4x4& Camera::GetViewMatrix() const
{
	if (useDebugOverride_ && debugOverride_)
	{
		return debugOverride_->GetViewMatrix();
	}
	return viewMatrix_;
}

const Matrix4x4& Camera::GetProjectionMatrix() const
{
	if (useDebugOverride_ && debugOverride_)
	{
		return debugOverride_->GetProjectionMatrix();
	}
	return projectionMatrix_;
}

const Matrix4x4& Camera::GetViewProjectionMatrix() const
{
	static Matrix4x4 tmp = {};
	if (useDebugOverride_ && debugOverride_)
	{
		tmp = Multiply(debugOverride_->GetViewMatrix(), debugOverride_->GetProjectionMatrix());
		return tmp;
	}
	return viewProjectionMatrix_;
}
