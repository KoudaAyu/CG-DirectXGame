#include "SkeletonDebug.h"

#include "Camera.h"
#include "Cylinder.h"
#include "DirectXCom.h"
#include "Light.h"
#include "MaterialManager.h"
#include "Object3dCom.h"
#include "Sphere.h"
#include "Baziru3_Engine/Graphics/SceneRenderRequests.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

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
    jointWorldPositions_.clear();
    jointWorldMatrices_.clear();
    jointNames_.clear();
    jointParentIndices_.clear();

    jointSpheres_.reserve(skeleton.joints.size());
    jointCylinders_.reserve(skeleton.joints.size());
    jointCylinderVisible_.resize(skeleton.joints.size(), false);
    jointWorldPositions_.resize(skeleton.joints.size());
    jointWorldMatrices_.resize(skeleton.joints.size());
    jointNames_.reserve(skeleton.joints.size());
    jointParentIndices_.reserve(skeleton.joints.size());

    for (size_t jointIndex = 0; jointIndex < skeleton.joints.size(); ++jointIndex)
    {
        jointNames_.push_back(skeleton.joints[jointIndex].name);
        jointParentIndices_.push_back(skeleton.joints[jointIndex].parent ? *skeleton.joints[jointIndex].parent : -1);

        auto jointSphere = std::make_unique<Sphere>();
        jointSphere->Initialize(dxCommon, object3dCom, materialManager, light, camera);
        jointSphere->SetOverlayDraw(true);
        Sprite::Transform jointTransform = jointSphere->GetTransform();
        jointTransform.scale = { 0.015f, 0.015f, 0.015f }; // 巨大な球体から控えめで見やすい小さなドットに変更
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
    if (jointWorldPositions_.size() < debugSphereCount)
    {
        jointWorldPositions_.resize(debugSphereCount);
        jointWorldMatrices_.resize(debugSphereCount);
    }

    for (size_t jointIndex = 0; jointIndex < debugSphereCount; ++jointIndex)
    {
        const Matrix4x4 jointWorldMat = skeleton.GetJointWorldMatrix(jointIndex, skeletonRootWorldMatrix);
        const Vector3 jointPosition = { jointWorldMat.m[3][0], jointWorldMat.m[3][1], jointWorldMat.m[3][2] };

        jointWorldPositions_[jointIndex] = jointPosition;
        jointWorldMatrices_[jointIndex] = jointWorldMat;

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
                jointCylinders_[jointIndex]->SetWorldMatrix(MakeBoneSegmentMatrix(parentPosition, jointPosition, 0.012f));
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
    if (showBoneCylinders_ && textureHandle.ptr != 0)
    {
        for (size_t jointIndex = 0; jointIndex < jointCylinders_.size(); ++jointIndex)
        {
            if (jointCylinders_[jointIndex] && jointIndex < jointCylinderVisible_.size() && jointCylinderVisible_[jointIndex])
            {
                jointCylinders_[jointIndex]->Draw(textureHandle);
            }
        }
    }

    if (showJointSpheres_)
    {
        for (const auto& jointSphere : jointSpheres_)
        {
            renderRequests.spheres.Request(jointSphere.get());
        }
    }
}

void SkeletonDebug::DrawUI(const Camera* camera)
{
    if (!initialized_) return;

#ifdef USE_IMGUI
    if (!ImGui::GetCurrentContext()) return;

    ImGui::Begin("Skeleton Debug Controls");
    ImGui::Text("骨のデバッグ表示設定 (Bone Debug Visualizer)");
    ImGui::Separator();
    ImGui::Checkbox("ボーン骨格（線・関節）の表示", &showBoneCylinders_);
    showJointSpheres_ = showBoneCylinders_;
    ImGui::Checkbox("ローカル軸の表示 (Local Axes: Red=X, Green=Y, Blue=Z)", &showLocalAxes_);
    ImGui::Checkbox("主要ボーン名の表示 (Main Bone Names)", &showBoneNames_);
    if (showBoneNames_)
    {
        ImGui::SameLine();
        ImGui::Checkbox("指ボーン名も表示", &showFingerNames_);
    }
    if (showLocalAxes_)
    {
        ImGui::SliderFloat("軸の長さ (Axis Length)", &axisLength_, 0.05f, 0.4f);
    }
    ImGui::End();

    if (!camera) return;

    Matrix4x4 viewProj = camera->GetViewProjectionMatrix();
    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    auto projectPoint = [&](const Vector3& p) -> Vector2 {
        Vector4 c = {
            p.x * viewProj.m[0][0] + p.y * viewProj.m[1][0] + p.z * viewProj.m[2][0] + viewProj.m[3][0],
            p.x * viewProj.m[0][1] + p.y * viewProj.m[1][1] + p.z * viewProj.m[2][1] + viewProj.m[3][1],
            p.x * viewProj.m[0][2] + p.y * viewProj.m[1][2] + p.z * viewProj.m[2][2] + viewProj.m[3][2],
            p.x * viewProj.m[0][3] + p.y * viewProj.m[1][3] + p.z * viewProj.m[2][3] + viewProj.m[3][3]
        };
        if (c.w <= 0.001f) return { -10000.0f, -10000.0f };
        float iw = 1.0f / c.w;
        return { (c.x * iw * 0.5f + 0.5f) * 1280.0f, ((1.0f - c.y * iw) * 0.5f) * 720.0f };
    };

    // 1. 関節同士を繋ぐ鮮明なボーンライン描画 (Blender/Unity風アーマチュア)
    if (showBoneCylinders_)
    {
        for (size_t i = 0; i < jointWorldPositions_.size(); ++i)
        {
            if (i < jointParentIndices_.size() && jointParentIndices_[i] >= 0)
            {
                int32_t pIdx = jointParentIndices_[i];
                if (pIdx < (int32_t)jointWorldPositions_.size())
                {
                    Vector2 posChild = projectPoint(jointWorldPositions_[i]);
                    Vector2 posParent = projectPoint(jointWorldPositions_[pIdx]);

                    if (posChild.x > -1000.0f && posParent.x > -1000.0f)
                    {
                        // 鮮やかなグリーンで骨の接続線を描画
                        drawList->AddLine(ImVec2(posParent.x, posParent.y), ImVec2(posChild.x, posChild.y), IM_COL32(60, 240, 120, 255), 2.2f);
                    }
                }
            }

            // 関節ノードに小さな黄金色のドットを描画
            Vector2 screenPos = projectPoint(jointWorldPositions_[i]);
            if (screenPos.x > -1000.0f)
            {
                drawList->AddCircleFilled(ImVec2(screenPos.x, screenPos.y), 3.5f, IM_COL32(255, 220, 60, 255));
                drawList->AddCircle(ImVec2(screenPos.x, screenPos.y), 3.5f, IM_COL32(0, 0, 0, 220), 0, 1.2f);
            }
        }
    }

    // 2. 各関節のローカル座標軸 (X: 赤, Y: 緑, Z: 青)
    if (showLocalAxes_)
    {
        for (size_t i = 0; i < jointWorldPositions_.size(); ++i)
        {
            const Vector3& pos = jointWorldPositions_[i];
            Vector2 screenPos = projectPoint(pos);
            if (screenPos.x < -1000.0f) continue;

            if (i < jointWorldMatrices_.size())
            {
                const Matrix4x4& mat = jointWorldMatrices_[i];
                Vector3 xAxis = Normalize({ mat.m[0][0], mat.m[0][1], mat.m[0][2] });
                Vector3 yAxis = Normalize({ mat.m[1][0], mat.m[1][1], mat.m[1][2] });
                Vector3 zAxis = Normalize({ mat.m[2][0], mat.m[2][1], mat.m[2][2] });

                Vector2 pX = projectPoint({ pos.x + xAxis.x * axisLength_, pos.y + xAxis.y * axisLength_, pos.z + xAxis.z * axisLength_ });
                Vector2 pY = projectPoint({ pos.x + yAxis.x * axisLength_, pos.y + yAxis.y * axisLength_, pos.z + yAxis.z * axisLength_ });
                Vector2 pZ = projectPoint({ pos.x + zAxis.x * axisLength_, pos.y + zAxis.y * axisLength_, pos.z + zAxis.z * axisLength_ });

                if (pX.x > -1000.0f) drawList->AddLine(ImVec2(screenPos.x, screenPos.y), ImVec2(pX.x, pX.y), IM_COL32(255, 60, 60, 255), 2.0f);
                if (pY.x > -1000.0f) drawList->AddLine(ImVec2(screenPos.x, screenPos.y), ImVec2(pY.x, pY.y), IM_COL32(60, 255, 60, 255), 2.0f);
                if (pZ.x > -1000.0f) drawList->AddLine(ImVec2(screenPos.x, screenPos.y), ImVec2(pZ.x, pZ.y), IM_COL32(60, 140, 255, 255), 2.0f);
            }
        }
    }

    // 3. ボーン名の画面座標描画 (見やすく接頭辞除去＆主要ボーンのみ整理表示)
    if (showBoneNames_)
    {
        for (size_t i = 0; i < jointWorldPositions_.size(); ++i)
        {
            if (i >= jointNames_.size()) continue;

            Vector2 screenPos = projectPoint(jointWorldPositions_[i]);
            if (screenPos.x < -1000.0f) continue;

            std::string cleanName = jointNames_[i];
            size_t prefixPos = cleanName.find("mixamorig:");
            if (prefixPos != std::string::npos)
            {
                cleanName = cleanName.substr(prefixPos + 10);
            }

            bool isFingerOrToe = (cleanName.find("Thumb") != std::string::npos ||
                                  cleanName.find("Index") != std::string::npos ||
                                  cleanName.find("Middle") != std::string::npos ||
                                  cleanName.find("Ring") != std::string::npos ||
                                  cleanName.find("Pinky") != std::string::npos ||
                                  cleanName.find("Toe") != std::string::npos ||
                                  cleanName.find("InHand") != std::string::npos ||
                                  cleanName.find("End") != std::string::npos);

            if (!showFingerNames_ && isFingerOrToe)
            {
                continue;
            }

            drawList->AddText(ImVec2(screenPos.x + 6.0f, screenPos.y - 6.0f), IM_COL32(255, 255, 255, 240), cleanName.c_str());
        }
    }
#endif
}
