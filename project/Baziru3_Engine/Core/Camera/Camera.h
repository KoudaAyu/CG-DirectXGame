#pragma once

#include "Matrix4x4.h"
#include "Transform.h"


#include <d3d12.h>
#include <wrl.h>


struct CameraForGPU {
  Vector3 worldPosition;
};

// 視錐台カリング用平面
struct FrustumPlane {
  Vector3 normal;
  float distance;

  float GetSignedDistance(const Vector3& point) const {
    return normal.x * point.x + normal.y * point.y + normal.z * point.z + distance;
  }
};

// 視錐台 (6平面: Left, Right, Bottom, Top, Near, Far)
struct Frustum {
  FrustumPlane planes[6];

  bool IntersectsSphere(const Vector3& center, float radius) const {
    for (int i = 0; i < 6; ++i) {
      if (planes[i].GetSignedDistance(center) < -radius) {
        return false; // 完全に視錐台の外側
      }
    }
    return true;
  }
};

class DirectXCom;

class Camera {
public:
  Camera();
  void Update();

  void Initialize(DirectXCom *directXCom);

  void Finalize();

public:
  const Matrix4x4 &GetWorldMatrix() const { return worldMatrix_; }
  const Matrix4x4 &GetViewMatrix() const { return viewMatrix_; }
  const Matrix4x4 &GetProjectionMatrix() const { return projectionMatrix_; }
  const Matrix4x4 &GetViewProjectionMatrix() const {
    return viewProjectionMatrix_;
  }
  const Frustum &GetFrustum() const { return frustum_; }
  bool IsInFrustum(const Vector3& center, float radius) const {
    return frustum_.IntersectsSphere(center, radius);
  }
  const Vector3 &GetRotate() const { return transform_.GetRotate(); }
  const Vector3 &GetTranslate() const { return transform_.GetTranslate(); }

  const Vector3 &GetWorldPosition() const { return transform_.GetTranslate(); }

  void SetRotate(const Vector3 &rotate) { transform_.SetRotate(rotate); }
  void SetTranslate(const Vector3 &translate) {
    transform_.SetTranslate(translate);
  }
  float GetFovY() const { return fovY_; }
  void SetFovY(float fovY) { fovY_ = fovY; }
  float GetAspectRatio() const { return aspectRatio_; }
  void SetAspectRatio(float aspectRatio) { aspectRatio_ = aspectRatio; }
  float GetNearZ() const { return nearZ_; }
  void SetNearZ(float nearZ) { nearZ_ = nearZ; }
  float GetFarZ() const { return farZ_; }
  void SetFarZ(float farZ) { farZ_ = farZ; }
  // カメラシェイク演出
  void AddShake(float intensity, float duration) {
    shakeIntensity_ = intensity;
    shakeDuration_ = duration;
    shakeTimer_ = duration;
  }
  bool IsShaking() const { return shakeTimer_ > 0.0f; }

  // Access to GPU-side camera virtual address
  D3D12_GPU_VIRTUAL_ADDRESS GetCameraGpuAddress() const {
    return cameraGpuAddress_;
  }

private:
  void UpdateFrustum();

  Frustum frustum_;
  Transform transform_;
  // 回転
  Vector3 rotation_ = {0.0f, 0.0f, 0.0f};
  // 移動
  Vector3 translation_ = {0.0f, 0.0f, 0.0f};
  Matrix4x4 worldMatrix_{};
  Matrix4x4 viewMatrix_{};

  Matrix4x4 projectionMatrix_{};

  Matrix4x4 viewProjectionMatrix_{};

  // 水平方向視野角
  float fovY_ = 0.45f;
  // アスペクト比
  float aspectRatio_ = 1.0f;
  // ニアクリップ距離
  float nearZ_ = 0.1f;
  // ファークリップ距離
  float farZ_ = 1000.0f;

  // DirectX 関連
  DirectXCom *directXCom_ = nullptr;
  CameraForGPU cameraData_{};
  D3D12_GPU_VIRTUAL_ADDRESS cameraGpuAddress_ = 0;

  // カメラシェイク
  float shakeIntensity_ = 0.0f;
  float shakeDuration_ = 0.0f;
  float shakeTimer_ = 0.0f;
};