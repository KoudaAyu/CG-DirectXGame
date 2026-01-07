#include"Camera.h"
#include"WindowsAPI.h"

Camera::Camera()
	: transform_({ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f,0.0f,-10.0f } }),
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

void Camera::Update()
{
	worldMatrix_ = MakeAffineMatrix(transform_.GetScale(), transform_.GetRotate(), transform_.GetTranslate());
	viewMatrix_ = Inverse(worldMatrix_);

	projectionMatrix_ =
		MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearZ_, farZ_);

	viewProjectionMatrix_ = Multiply(viewMatrix_, projectionMatrix_);
}