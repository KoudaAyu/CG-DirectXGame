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
  Vector3 finalTranslate = transform_.GetTranslate();

  if (shakeTimer_ > 0.0f) {
    float dt = 1.0f / 60.0f;
    shakeTimer_ -= dt;
    float progress = (shakeDuration_ > 0.0f) ? (shakeTimer_ / shakeDuration_) : 0.0f;
    float currentPower = shakeIntensity_ * progress;

    float offsetX = (((std::rand() % 200) / 100.0f) - 1.0f) * currentPower;
    float offsetY = (((std::rand() % 200) / 100.0f) - 1.0f) * currentPower;
    float offsetZ = (((std::rand() % 200) / 100.0f) - 1.0f) * currentPower * 0.5f;

    finalTranslate.x += offsetX;
    finalTranslate.y += offsetY;
    finalTranslate.z += offsetZ;
  }

  worldMatrix_ = MakeAffineMatrix(transform_.GetScale(), transform_.GetRotate(),
                                  finalTranslate);
  viewMatrix_ = Inverse(worldMatrix_);

  projectionMatrix_ =
      MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearZ_, farZ_);

  viewProjectionMatrix_ = Multiply(viewMatrix_, projectionMatrix_);
  UpdateFrustum();

  cameraData_.worldPosition = finalTranslate;

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

void Camera::UpdateFrustum() {
  const Matrix4x4& m = viewProjectionMatrix_;

  // Left: row3 + row0
  frustum_.planes[0].normal = { m.m[0][3] + m.m[0][0], m.m[1][3] + m.m[1][0], m.m[2][3] + m.m[2][0] };
  frustum_.planes[0].distance = m.m[3][3] + m.m[3][0];

  // Right: row3 - row0
  frustum_.planes[1].normal = { m.m[0][3] - m.m[0][0], m.m[1][3] - m.m[1][0], m.m[2][3] - m.m[2][0] };
  frustum_.planes[1].distance = m.m[3][3] - m.m[3][0];

  // Bottom: row3 + row1
  frustum_.planes[2].normal = { m.m[0][3] + m.m[0][1], m.m[1][3] + m.m[1][1], m.m[2][3] + m.m[2][1] };
  frustum_.planes[2].distance = m.m[3][3] + m.m[3][1];

  // Top: row3 - row1
  frustum_.planes[3].normal = { m.m[0][3] - m.m[0][1], m.m[1][3] - m.m[1][1], m.m[2][3] - m.m[2][1] };
  frustum_.planes[3].distance = m.m[3][3] - m.m[3][1];

  // Near: row2
  frustum_.planes[4].normal = { m.m[0][2], m.m[1][2], m.m[2][2] };
  frustum_.planes[4].distance = m.m[3][2];

  // Far: row3 - row2
  frustum_.planes[5].normal = { m.m[0][3] - m.m[0][2], m.m[1][3] - m.m[1][2], m.m[2][3] - m.m[2][2] };
  frustum_.planes[5].distance = m.m[3][3] - m.m[3][2];

  // 各平面法線の正規化
  for (int i = 0; i < 6; ++i) {
    float len = std::sqrt(frustum_.planes[i].normal.x * frustum_.planes[i].normal.x +
                          frustum_.planes[i].normal.y * frustum_.planes[i].normal.y +
                          frustum_.planes[i].normal.z * frustum_.planes[i].normal.z);
    if (len > 1e-5f) {
      float invLen = 1.0f / len;
      frustum_.planes[i].normal.x *= invLen;
      frustum_.planes[i].normal.y *= invLen;
      frustum_.planes[i].normal.z *= invLen;
      frustum_.planes[i].distance *= invLen;
    }
  }
}