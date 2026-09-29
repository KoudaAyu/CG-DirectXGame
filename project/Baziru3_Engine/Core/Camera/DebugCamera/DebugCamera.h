#pragma once
#include "Camera.h"
#include "KeyInput.h"
#include "Matrix4x4.h"
#include "Vector.h"

class WindowAPI;
class DirectXCom;

/// <summary>
/// デバッグ用自由移動カメラ。
/// 
/// 【オブジェクト指向：ポリモーフィズム（派生クラス）】
/// ・基底クラス Camera を継承し、Update() メソッドをオーバーライド。
/// ・キーボード操作（WASD、矢印キー）による自由な視点移動・回転制御を実装。
/// ・Camera* 型のポインタとして扱えるため、通常カメラとデバッグカメラを
///   呼び出し側のコードを変更することなくポリモーフィックに切り替え可能。
/// </summary>
class DebugCamera : public Camera
{
public: 
	DebugCamera() = default;
	~DebugCamera() override = default;

	/// <summary>
	/// デバッグカメラの初期化
	/// </summary>
	/// <param name="windowAPI">ウィンドウ管理ポインタ</param>
	/// <param name="dxCommon">DirectX管理ポインタ（GPU定数バッファ割り当て用、省略可）</param>
	void Initialize(WindowAPI* windowAPI, DirectXCom* dxCommon = nullptr);

	/// <summary>
	/// カメラ状態の更新（キー入力による移動・回転の反映）
	/// 【ポリモーフィズム】基底クラス Camera::Update() をオーバーライド
	/// </summary>
	void Update() override;

	// --- 既存コードとの互換性のためのエイリアス ---
	const Matrix4x4& GetViewMatrix() const { return viewMatrix_; }
	const Matrix4x4& GetProjectionMatrix() const { return projectionMatrix_; }

private:
	WindowAPI* windowAPI_ = nullptr;

	// デバッグ用累積回転行列
	Matrix4x4 matRot_ = {};

	// デバッグ専用キー入力
	KeyInput keyInput_;

	// カメラ移動速度
	const float speed_ = 0.1f;
};
