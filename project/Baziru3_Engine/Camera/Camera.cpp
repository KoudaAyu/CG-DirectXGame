#include"Camera.h"
#include"DirectXCom.h"
#include"WindowsAPI.h"

Camera::Camera()
	: transform_({ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} }),
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

	// --- カメラ用のリソース作成を追加 ---
	cameraResource = directXCom_->CreateBufferResource(directXCom_->GetDevice().Get(), sizeof(CameraForGPU));

	
	cameraResource->Map(0, nullptr, reinterpret_cast<void**>(&cameraData));
	// 初期値を設定
	if (cameraData)
	{
		cameraData->worldPosition = { 0.0f, 0.0f, -10.0f };
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
	worldMatrix_ = MakeAffineMatrix(transform_.GetScale(), transform_.GetRotate(), transform_.GetTranslate());
	viewMatrix_ = Inverse(worldMatrix_);

	projectionMatrix_ =
		MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearZ_, farZ_);

	viewProjectionMatrix_ = Multiply(viewMatrix_, projectionMatrix_);

	assert(cameraData);
	
	if (cameraData)
	{
		cameraData->worldPosition = transform_.GetTranslate();
	}
}