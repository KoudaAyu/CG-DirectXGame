#include "DebugCamera.h"
#include <fstream>
#include <string>

void DebugCamera::Initialize(WindowAPI* windowAPI)
{
	this->windowAPI = windowAPI;

	keyInput_.Initialize(windowAPI);

	matRot_ = MakeIdentity4x4();
}

bool DebugCamera::LoadFromFile(const std::string& path)
{
	std::ifstream ifs(path, std::ios::binary);
	if (!ifs) return false;
	ifs.read(reinterpret_cast<char*>(&translation_), sizeof(translation_));
	ifs.read(reinterpret_cast<char*>(&rotation_), sizeof(rotation_));
	return ifs.good();
}

bool DebugCamera::SaveToFile(const std::string& path) const
{
	std::ofstream ofs(path, std::ios::binary);
	if (!ofs) return false;
	ofs.write(reinterpret_cast<const char*>(&translation_), sizeof(translation_));
	ofs.write(reinterpret_cast<const char*>(&rotation_), sizeof(rotation_));
	return ofs.good();
}

void DebugCamera::Reset()
{
	rotation_ = { 0.0f, 0.0f, 0.0f };
	translation_ = { 0.0f, 0.0f, -50.0f };
	matRot_ = MakeIdentity4x4();
}

void DebugCamera::Update()
{
    // Only process input when control is enabled for the debug camera
	if (controlEnabled_)
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
        // Arrow keys control rotation: up/down -> pitch (x), left/right -> yaw (y)
		const float angleSpeed = 0.02f;
		if (keyInput_.IsKeyPressed(DIK_UP))
		{
			rotation_.x += angleSpeed;
		}
		else if (keyInput_.IsKeyPressed(DIK_DOWN))
		{
			rotation_.x -= angleSpeed;
		}
		if (keyInput_.IsKeyPressed(DIK_LEFT))
		{
			rotation_.y -= angleSpeed;
		}
		else if (keyInput_.IsKeyPressed(DIK_RIGHT))
		{
			rotation_.y += angleSpeed;
		}
	}

    // Build rotation matrix directly from Euler angles (rotation_ stores absolute angles)
	matRot_ = MakeIdentity4x4();
	matRot_ *= MakeRotateXMatrix(rotation_.x);
	matRot_ *= MakeRotateYMatrix(rotation_.y);
	matRot_ *= MakeRotateZMatrix(rotation_.z);

	// Compute world/view/projection
	Matrix4x4 worldMatrix = MakeAffineMatrix({ 1.0f,1.0f,1.0f }, matRot_, translation_);
	view_matrix_ = Inverse(worldMatrix);
	// store projection matrix to member so callers can retrieve it
	projection_matrix_ = MakePerspectiveFovMatrix(fovY, aspectRatio, nearZ, farZ);


}
