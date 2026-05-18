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

    bool IsInitialized() const { return initialized_; }

private:
    std::vector<std::unique_ptr<Sphere>> jointSpheres_;
    std::vector<std::unique_ptr<Cylinder>> jointCylinders_;
    std::vector<bool> jointCylinderVisible_;
    bool initialized_ = false;
};
