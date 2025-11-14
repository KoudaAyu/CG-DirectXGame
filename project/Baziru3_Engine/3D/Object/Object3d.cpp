#include"Object3d.h"

#include"Matrix4x4.h"

void Object3d::Initialize()
{
	camera_ = object3dCom_->GetDefaultCamera();
}

void Object3d::Update()
{
	Matrix4x4 worldMatrix = MakeAffineMatrix(transform_.GetScale(), transform_.GetRotate(), transform_.GetTranslate());
	Matrix4x4 worldViewProjectionMatrix;
	if (camera_)
	{
		const Matrix4x4& viewProjectionMatrix = camera_->GetViewProjectionMatrix();
		worldViewProjectionMatrix = Multiply(worldMatrix, viewProjectionMatrix);
	}
	else
	{
		worldViewProjectionMatrix = worldMatrix;
	}

}