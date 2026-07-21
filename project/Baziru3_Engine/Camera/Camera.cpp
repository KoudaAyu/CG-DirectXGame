#include"Camera.h"
#include"DirectXCom.h"
#include"WindowsAPI.h"
#include <fstream>
#include <filesystem>
#include <string>
#include <Windows.h>
#include "externals/nlohmann/json.hpp"

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
	// --- Blenderのカメラ同期処理 ---
	static std::filesystem::file_time_type lastCameraSyncTime;
	static bool isFirstCameraSync = true;
	std::string syncPath = "Resources/camera_sync.json";
	try
	{
		if (std::filesystem::exists(syncPath))
		{
			auto currentWriteTime = std::filesystem::last_write_time(syncPath);
			if (isFirstCameraSync || currentWriteTime > lastCameraSyncTime)
			{
				lastCameraSyncTime = currentWriteTime;
				isFirstCameraSync = false;

				std::ifstream file(syncPath);
				if (file.is_open())
				{
					nlohmann::json j;
					file >> j;
					if (j.contains("position") && j.contains("rotation"))
					{
						const auto& pos = j["position"];
						const auto& rot = j["rotation"];

						Vector3 position = { pos.value("x", 0.0f), pos.value("y", 0.0f), pos.value("z", 0.0f) };
						Vector3 rotation = { rot.value("x", 0.0f), rot.value("y", 0.0f), rot.value("z", 0.0f) };

						// デバッグログ出力
						std::string logMsg = "[CameraSync] Read pos: (" + std::to_string(position.x) + ", " + std::to_string(position.y) + ", " + std::to_string(position.z) + 
											 "), rot: (" + std::to_string(rotation.x) + ", " + std::to_string(rotation.y) + ", " + std::to_string(rotation.z) + ")\n";
						OutputDebugStringA(logMsg.c_str());

						transform_.SetTranslate(position);
						transform_.SetRotate(rotation);
					}
				}
			}
		}
	}
	catch (...)
	{
		// ファイルアクセス競合の防止
	}

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