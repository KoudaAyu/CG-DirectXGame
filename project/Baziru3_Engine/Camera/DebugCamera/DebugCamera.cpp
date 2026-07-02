#include "DebugCamera.h"

void DebugCamera::Initialize(WindowAPI* windowAPI)
{
	this->windowAPI = windowAPI;

	keyInput_.Initialize(windowAPI);

	matRot_ = MakeIdentity4x4();
}

void DebugCamera::Update()
{
	keyInput_.Update();

	if (keyInput_.IsKeyPressed(DIK_D))
	{
		//カメラ移動ベクトル
		Vector3 move = { speed,0.0f,0.0f };
		translation_ += move;
	}
	else if (keyInput_.IsKeyPressed(DIK_A))
	{
		//カメラ移動ベクトル
		Vector3 move = { -speed,0.0f,0.0f };
		translation_ += move;
	}
	else if (keyInput_.IsKeyPressed(DIK_W))
	{
		//カメラ移動ベクトル
		Vector3 move = { 0.0f,0.0f,speed };
		translation_ += move;
	}
	else if (keyInput_.IsKeyPressed(DIK_S))
	{
		//カメラ移動ベクトル
		Vector3 move = { 0.0f,0.0f,-speed };
		translation_ += move;
	}
	if (keyInput_.IsKeyPressed(DIK_UP))
	{
		
		//カメラ移動ベクトル
		Vector3 move = { 0.0f,0.0f,speed };
		rotation_ += move;
	}
	else if (keyInput_.IsKeyPressed(DIK_DOWN))
	{
		
		//カメラ移動ベクトル
		Vector3 move = { 0.0f,0.0f,-speed };
		rotation_ += move;
	}

	//追加回転分の回転行列
	Matrix4x4 matRotateDelta = MakeIdentity4x4();
	matRotateDelta *= MakeRotateXMatrix(rotation_.x);
	matRotateDelta *= MakeRotateYMatrix(rotation_.y);

	//累積の回転行列を合成
	matRot_ = Multiply(matRotateDelta, matRot_);
	
	Matrix4x4 worldMatrix = MakeAffineMatrix({ 1.0f,1.0f,1.0f }, matRot_, translation_);
	view_matrix_ = Inverse(worldMatrix);
	Matrix4x4 projection_Matrix = MakePerspectiveFovMatrix(fovY, aspectRatio, nearZ, farZ);


}
