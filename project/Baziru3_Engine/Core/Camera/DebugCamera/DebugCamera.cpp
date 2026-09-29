#include "DebugCamera.h"
#include "WindowsAPI.h"

void DebugCamera::Initialize(WindowAPI* windowAPI, DirectXCom* dxCommon)
{
	// 基底クラス Camera の初期化（DirectX12 CBアロケータの設定等）
	Camera::Initialize(dxCommon);

	windowAPI_ = windowAPI;
	keyInput_.Initialize(windowAPI);

	// デバッグカメラの初期位置・画角の設定
	translation_ = { 0.0f, 0.0f, -50.0f };
	transform_.SetTranslate(translation_);
	fovY_ = 0.45f;
	if (windowAPI) {
		aspectRatio_ = static_cast<float>(WindowAPI::GetClientWidth()) / static_cast<float>(WindowAPI::GetClientHeight());
	} else {
		aspectRatio_ = 1280.0f / 720.0f;
	}
	nearZ_ = 0.1f;
	farZ_ = 1000.0f;

	matRot_ = MakeIdentity4x4();
}

void DebugCamera::Update()
{
	keyInput_.Update();

	// --- キーボードによるカメラ平行移動（WASD） ---
	if (keyInput_.IsKeyPressed(DIK_D))
	{
		translation_.x += speed_;
	}
	else if (keyInput_.IsKeyPressed(DIK_A))
	{
		translation_.x -= speed_;
	}

	if (keyInput_.IsKeyPressed(DIK_W))
	{
		translation_.z += speed_;
	}
	else if (keyInput_.IsKeyPressed(DIK_S))
	{
		translation_.z -= speed_;
	}

	// --- キーボードによるカメラ回転（上下キー） ---
	if (keyInput_.IsKeyPressed(DIK_UP))
	{
		rotation_.x += speed_ * 0.2f;
	}
	else if (keyInput_.IsKeyPressed(DIK_DOWN))
	{
		rotation_.x -= speed_ * 0.2f;
	}

	// 追加回転分の回転行列
	Matrix4x4 matRotateDelta = MakeIdentity4x4();
	matRotateDelta *= MakeRotateXMatrix(rotation_.x);
	matRotateDelta *= MakeRotateYMatrix(rotation_.y);

	// 累積回転行列を合成
	matRot_ = Multiply(matRotateDelta, matRot_);
	
	// ワールド行列・ビュー行列・射影行列の更新
	worldMatrix_ = MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, matRot_, translation_);
	viewMatrix_ = Inverse(worldMatrix_);
	projectionMatrix_ = MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearZ_, farZ_);
	viewProjectionMatrix_ = Multiply(viewMatrix_, projectionMatrix_);

	transform_.SetTranslate(translation_);
	transform_.SetRotate(rotation_);

	// 視錐台とGPUバッファの更新（基底クラス機能の活用）
	UpdateFrustum();
	UpdateGPUBuffer(translation_);
}
