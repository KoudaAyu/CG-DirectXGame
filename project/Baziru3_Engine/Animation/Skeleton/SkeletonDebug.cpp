#include "SkeletonDebug.h"

#include "Camera.h"
#include "Cylinder.h"
#include "DirectXCom.h"
#include "Light.h"
#include "MaterialManager.h"
#include "Object3dCom.h"
#include "Sphere.h"
#include "Baziru3_Engine/Graphics/SceneRenderRequests.h"

#include <algorithm>
#include <cmath>

namespace
{
    Vector3 Subtract(const Vector3& a, const Vector3& b)
    {
        return { a.x - b.x, a.y - b.y, a.z - b.z };
    }

    float Length(const Vector3& v)
    {
        return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    }

    Vector3 Normalize(const Vector3& v)
    {
        const float length = Length(v);
        if (length <= 0.0001f)
        {
            return { 0.0f, 1.0f, 0.0f };
        }

        const float invLength = 1.0f / length;
        return { v.x * invLength, v.y * invLength, v.z * invLength };
    }

    Vector3 Cross(const Vector3& a, const Vector3& b)
    {
        return {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    }

    Matrix4x4 MakeBoneSegmentMatrix(const Vector3& start, const Vector3& end, float radius)
    {
        const Vector3 direction = Subtract(end, start);
        const float length = Length(direction);
        const Vector3 yAxis = Normalize(direction);

        Vector3 referenceAxis = { 0.0f, 0.0f, 1.0f };
        if (std::fabs(yAxis.z) > 0.99f)
        {
            referenceAxis = { 1.0f, 0.0f, 0.0f };
        }

        const Vector3 xAxis = Normalize(Cross(referenceAxis, yAxis));
        const Vector3 zAxis = Normalize(Cross(yAxis, xAxis));

        Matrix4x4 rotateMatrix = MakeIdentity4x4();
        rotateMatrix.m[0][0] = xAxis.x;
        rotateMatrix.m[0][1] = xAxis.y;
        rotateMatrix.m[0][2] = xAxis.z;
        rotateMatrix.m[1][0] = yAxis.x;
        rotateMatrix.m[1][1] = yAxis.y;
        rotateMatrix.m[1][2] = yAxis.z;
        rotateMatrix.m[2][0] = zAxis.x;
        rotateMatrix.m[2][1] = zAxis.y;
        rotateMatrix.m[2][2] = zAxis.z;

        return MakeAffineMatrix({ radius, length, radius }, rotateMatrix, start);
    }
}

void SkeletonDebug::Initialize(DirectXCom* dxCommon, Object3dCom* object3dCom, MaterialManager* materialManager, Light* light, Camera* camera, const Skeleton& skeleton)
{
    jointSpheres_.clear();
    jointCylinders_.clear();
    jointCylinderVisible_.clear();
    jointSpheres_.reserve(skeleton.joints.size());
    jointCylinders_.reserve(skeleton.joints.size());
    jointCylinderVisible_.resize(skeleton.joints.size(), false);

    for (size_t jointIndex = 0; jointIndex < skeleton.joints.size(); ++jointIndex)
    {
        auto jointSphere = std::make_unique<Sphere>();
        jointSphere->Initialize(dxCommon, object3dCom, materialManager, light, camera);
        jointSphere->SetOverlayDraw(true);
        Sprite::Transform jointTransform = jointSphere->GetTransform();
        jointTransform.scale = { 0.06f, 0.06f, 0.06f };
        jointSphere->SetTransform(jointTransform);
        jointSpheres_.push_back(std::move(jointSphere));

        auto jointCylinder = std::make_unique<Cylinder>();
        jointCylinder->Initialize(dxCommon, object3dCom, materialManager, light, camera, 12, 1.0f, 1.0f, 1.0f);
        jointCylinder->SetOverlayDraw(true);
        jointCylinders_.push_back(std::move(jointCylinder));
    }

    initialized_ = !jointSpheres_.empty();
}

void SkeletonDebug::Sync(const Skeleton& skeleton, const Matrix4x4& skeletonRootWorldMatrix)
{
    constexpr float kMinBoneSegmentLength = 0.0001f;
    constexpr float kMaxBoneSegmentLength = 2.0f;

    const size_t debugSphereCount = (std::min)(skeleton.joints.size(), jointSpheres_.size());
    for (size_t jointIndex = 0; jointIndex < debugSphereCount; ++jointIndex)
    {
        const Vector3 jointPosition = skeleton.GetJointWorldPosition(jointIndex, skeletonRootWorldMatrix);
        Sprite::Transform jointTransform = jointSpheres_[jointIndex]->GetTransform();
        jointTransform.translate = jointPosition;
        jointSpheres_[jointIndex]->SetTransform(jointTransform);
        jointSpheres_[jointIndex]->Update();

        if (jointIndex < jointCylinders_.size() && jointCylinders_[jointIndex] && skeleton.joints[jointIndex].parent)
        {
            const int32_t parentIndex = *skeleton.joints[jointIndex].parent;
            const Vector3 parentPosition = skeleton.GetJointWorldPosition(parentIndex, skeletonRootWorldMatrix);
            const float boneSegmentLength = Length(Subtract(jointPosition, parentPosition));
            const bool isValidBoneSegment = boneSegmentLength >= kMinBoneSegmentLength && boneSegmentLength <= kMaxBoneSegmentLength;

            jointCylinderVisible_[jointIndex] = isValidBoneSegment;
            if (isValidBoneSegment)
            {
                jointCylinders_[jointIndex]->SetWorldMatrix(MakeBoneSegmentMatrix(parentPosition, jointPosition, 0.03f));
                jointCylinders_[jointIndex]->Update();
            }
        }
        else if (jointIndex < jointCylinderVisible_.size())
        {
            jointCylinderVisible_[jointIndex] = false;
        }
    }
}

void SkeletonDebug::Draw(SceneRenderRequests& renderRequests, D3D12_GPU_DESCRIPTOR_HANDLE textureHandle) const
{
    if (textureHandle.ptr != 0)
    {
        for (size_t jointIndex = 0; jointIndex < jointCylinders_.size(); ++jointIndex)
        {
            if (jointCylinders_[jointIndex] && jointIndex < jointCylinderVisible_.size() && jointCylinderVisible_[jointIndex])
            {
                jointCylinders_[jointIndex]->Draw(textureHandle);
            }
        }
    }

    for (const auto& jointSphere : jointSpheres_)
    {
        renderRequests.spheres.Request(jointSphere.get());
    }
}
