#include "Camera.h"
#include "Baziru3_Engine/Core/Base/Allocator/ConstantBufferAllocator.h"
#include "DirectXCom.h"
#include "WindowsAPI.h"
#include "SceneManager.h"


Camera::Camera()
    : transform_(
          {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -10.0f}}),
      fovY_(0.45f), aspectRatio_(float(WindowAPI::GetClientWidth()) /
                                 float(WindowAPI::GetClientHeight())),
      nearZ_(0.1f), farZ_(1000.0f),
      worldMatrix_(MakeAffineMatrix(transform_.GetScale(),
                                    transform_.GetRotate(),
                                    transform_.GetTranslate())),
      viewMatrix_(Inverse(worldMatrix_)),
      projectionMatrix_(
          MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearZ_, farZ_)),
      viewProjectionMatrix_(Multiply(viewMatrix_, projectionMatrix_)) {}

void Camera::Initialize(DirectXCom *directXCom) {
  directXCom_ = directXCom;
  cameraData_ = CameraForGPU{};
  cameraData_.worldPosition = transform_.GetTranslate();
}

void Camera::Finalize() { directXCom_ = nullptr; }

void Camera::Update() {
  worldMatrix_ = MakeAffineMatrix(transform_.GetScale(), transform_.GetRotate(),
                                  transform_.GetTranslate());
  viewMatrix_ = Inverse(worldMatrix_);

  projectionMatrix_ =
      MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearZ_, farZ_);

  viewProjectionMatrix_ = Multiply(viewMatrix_, projectionMatrix_);

  cameraData_.worldPosition = transform_.GetTranslate();

  DirectXCom* dx = directXCom_;
  if (!dx) {
    auto* sm = SceneManager::GetInstance();
    if (sm) {
      dx = sm->GetDirectXCom();
    }
  }

  if (dx) {
    auto *cbAllocator = dx->GetCBAllocator();
    if (cbAllocator) {
      auto alloc = cbAllocator->Allocate(sizeof(CameraForGPU));
      std::memcpy(alloc.cpuAddress, &cameraData_, sizeof(CameraForGPU));
      cameraGpuAddress_ = alloc.gpuAddress;
    }
  }
}