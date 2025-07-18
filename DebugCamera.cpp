#include "DebugCamera.h"

void DebugCamera::Initialize(HINSTANCE hInstance, HWND hwnd)
{
	keyInput_.Initialize(hInstance, hwnd);
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
	
	Matrix4x4 worldMatrix = MakeAffineMatrix({ 1.0f,1.0f,1.0f }, rotation_, translation_);
	view_matrix_ = Inverse(worldMatrix);
	Matrix4x4 projection_Matrix = MakePerspectiveFovMatrix(fovY, aspectRatio, nearZ, farZ);
}
