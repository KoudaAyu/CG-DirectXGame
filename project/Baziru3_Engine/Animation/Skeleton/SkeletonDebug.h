#pragma once

#include <memory>
#include <vector>

#include "Skeleton.h"

class Camera;
class Cylinder;
class DirectXCom;
class Light;
class MaterialManager;
class Object3dCom;
class Sphere;
struct SceneRenderRequests;
struct Matrix4x4;
struct D3D12_GPU_DESCRIPTOR_HANDLE;

class SkeletonDebug
{
public:
    void Initialize(DirectXCom* dxCommon, Object3dCom* object3dCom, MaterialManager* materialManager, Light* light, Camera* camera, const Skeleton& skeleton);
    void Sync(const Skeleton& skeleton, const Matrix4x4& skeletonRootWorldMatrix);
    void Draw(SceneRenderRequests& renderRequests, D3D12_GPU_DESCRIPTOR_HANDLE textureHandle) const;
    void DrawUI(const Camera* camera);

    bool IsInitialized() const { return initialized_; }

    void SetShowBoneCylinders(bool show) { showBoneCylinders_ = show; }
    void SetShowJointSpheres(bool show) { showJointSpheres_ = show; }
    void SetShowLocalAxes(bool show) { showLocalAxes_ = show; }
    void SetShowBoneNames(bool show) { showBoneNames_ = show; }

private:
    std::vector<std::unique_ptr<Sphere>> jointSpheres_;
    std::vector<std::unique_ptr<Cylinder>> jointCylinders_;
    std::vector<bool> jointCylinderVisible_;
    std::vector<Vector3> jointWorldPositions_;
    std::vector<Matrix4x4> jointWorldMatrices_;
    std::vector<std::string> jointNames_;
    std::vector<int32_t> jointParentIndices_;

    bool showBoneCylinders_ = true;
    bool showJointSpheres_ = true;
    bool showLocalAxes_ = false;
    bool showBoneNames_ = false;
    bool showFingerNames_ = false;
    float axisLength_ = 0.15f;

    bool initialized_ = false;
};
