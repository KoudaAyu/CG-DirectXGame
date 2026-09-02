#define NOMINMAX
#include "CollisionDebugDraw.h"
#include "Collider.h"
#include "SphereCollider.h"
#include "BoxCollider.h"
#include "CapsuleCollider.h"
#include "MeshCollider.h"
#include "SkeletonCollider.h"
#include "SoftBodyDeformer.h"
#include "Baziru3_Engine/Core/Camera/Camera.h"
#include "Baziru3_Engine/Graphics/Shapes/Sphere/Sphere.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3d.h"
#include "Matrix4x4.h"
#include <imgui.h>
#include <cmath>
#include <algorithm>
#include <vector>

namespace CollisionDebugDraw
{
    static inline Vector3 Cross(const Vector3& a, const Vector3& b)
    {
        return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
    }

    static inline float Length(const Vector3& v)
    {
        return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    }

    static inline Vector3 Normalize(const Vector3& v)
    {
        float len = Length(v);
        if (len > 1e-5f) return v * (1.0f / len);
        return { 0.0f, 0.0f, 0.0f };
    }

    void Draw(const std::vector<Collider*>& colliders, Camera* camera)
    {
#ifdef _DEBUG
        if (!camera) return;

        ImGuiIO& io = ImGui::GetIO();
        float width = io.DisplaySize.x;
        float height = io.DisplaySize.y;
        if (width <= 0.0f || height <= 0.0f) return;

        const Matrix4x4& vp = camera->GetViewProjectionMatrix();
        ImDrawList* drawList = ImGui::GetForegroundDrawList();

        auto project3DTo2D = [&](const Vector3& pos3D, ImVec2& outPos) -> bool {
            float w = pos3D.x * vp.m[0][3] + pos3D.y * vp.m[1][3] + pos3D.z * vp.m[2][3] + vp.m[3][3];
            if (w <= 0.0f) return false;
            float x = (pos3D.x * vp.m[0][0] + pos3D.y * vp.m[1][0] + pos3D.z * vp.m[2][0] + vp.m[3][0]) / w;
            float y = (pos3D.x * vp.m[0][1] + pos3D.y * vp.m[1][1] + pos3D.z * vp.m[2][1] + vp.m[3][1]) / w;
            outPos.x = (x + 1.0f) * 0.5f * width;
            outPos.y = (1.0f - y) * 0.5f * height;
            return true;
        };

        for (Collider* col : colliders)
        {
            if (!col || !col->IsEnabled()) continue;

            Vector3 worldPos = col->GetWorldPosition();

            // 距離カリング (40m以上離れたオブジェクトのデバッグ描画をスキップ)
            if (camera && col->GetAttribute() != CollisionAttribute::Player)
            {
                Vector3 camPos = camera->GetTranslate();
                float dx = worldPos.x - camPos.x;
                float dy = worldPos.y - camPos.y;
                float dz = worldPos.z - camPos.z;
                if (dx * dx + dy * dy + dz * dz > 40.0f * 40.0f)
                {
                    continue;
                }
            }

            ImU32 colColor = ImGui::ColorConvertFloat4ToU32({ 1.0f, 1.0f, 1.0f, 0.85f });
            if (col->GetAttribute() == CollisionAttribute::Minion || (col->GetAttribute() == CollisionAttribute::Player && col->GetType() == ColliderType::Sphere && static_cast<SphereCollider*>(col)->GetRadius() <= 0.45f))
            {
                colColor = ImGui::ColorConvertFloat4ToU32({ 1.0f, 0.5f, 0.1f, 0.95f }); // Minions
            }
            else if (col->GetAttribute() == CollisionAttribute::Player)
            {
                colColor = ImGui::ColorConvertFloat4ToU32({ 0.1f, 1.0f, 0.4f, 0.95f }); // Player
            }
            else if (col->GetAttribute() == CollisionAttribute::Enemy)
            {
                colColor = ImGui::ColorConvertFloat4ToU32({ 1.0f, 0.2f, 0.2f, 0.95f }); // Red
            }
            else if (col->GetAttribute() == CollisionAttribute::Obstacle)
            {
                colColor = ImGui::ColorConvertFloat4ToU32({ 0.2f, 0.75f, 1.0f, 0.85f }); // Light blue
            }
            else if (col->GetAttribute() == CollisionAttribute::Bullet)
            {
                colColor = ImGui::ColorConvertFloat4ToU32({ 1.0f, 0.85f, 0.0f, 0.95f }); // Yellow
            }

            if (col->GetType() == ColliderType::Sphere)
            {
                SphereCollider* sphere = static_cast<SphereCollider*>(col);
                float radius = sphere->GetRadius();
                Vector3 scale = sphere->GetScale();
                Vector3 rot = sphere->GetRotation();
                const int numSegments = 48;

                float cosP = std::cos(rot.x), sinP = std::sin(rot.x);
                float cosY = std::cos(rot.y), sinY = std::sin(rot.y);
                float cosR = std::cos(rot.z), sinR = std::sin(rot.z);

                bool isSoftBody = (col->GetAttribute() == CollisionAttribute::Player || col->GetAttribute() == CollisionAttribute::Minion);

                auto transformLocal = [&](const Vector3& local) -> Vector3 {
                    Vector3 deformed = local;
                    if (isSoftBody && radius > 0.001f)
                    {
                        Vector3 normLocal = local * (1.0f / radius);
                        Vector3 normal = Normalize(local);
                        deformed = SoftBodyDeformer::CalculateDeformedPosition(normLocal, normal, SoftBodyDeformer::GetGlobalTime()) * radius;
                    }

                    float scaleMultiplierX = isSoftBody ? 1.28f : scale.x;
                    float scaleMultiplierY = isSoftBody ? 1.12f : scale.y;
                    float scaleMultiplierZ = isSoftBody ? 1.28f : scale.z;

                    Vector3 s = { deformed.x * scaleMultiplierX, deformed.y * scaleMultiplierY, deformed.z * scaleMultiplierZ };
                    Vector3 ry = { s.x * cosY + s.z * sinY, s.y, -s.x * sinY + s.z * cosY };
                    Vector3 rx = { ry.x, ry.y * cosP - ry.z * sinP, ry.y * sinP + ry.z * cosP };
                    Vector3 rz = { rx.x * cosR - rx.y * sinR, rx.x * sinR + rx.y * cosR, rx.z };
                    return { worldPos.x + rz.x, worldPos.y + rz.y, worldPos.z + rz.z };
                };

                // 48分割用 sin/cos 事前計算テーブル (初回のみ初期化)
                static bool s_tablesInit = false;
                static float s_sinTable[49];
                static float s_cosTable[49];
                static float s_meridianCos[8];
                static float s_meridianSin[8];
                static float s_latCos[7];
                static float s_latSin[7];

                if (!s_tablesInit)
                {
                    for (int i = 0; i <= numSegments; ++i)
                    {
                        float phi = i * (6.2831853f / numSegments);
                        s_sinTable[i] = std::sin(phi);
                        s_cosTable[i] = std::cos(phi);
                    }
                    for (int m = 0; m < 8; ++m)
                    {
                        float mAngle = m * (3.14159265f / 8.0f);
                        s_meridianCos[m] = std::cos(mAngle);
                        s_meridianSin[m] = std::sin(mAngle);
                    }
                    float latAngles[7] = { 0.0f, 0.45f, -0.45f, 0.90f, -0.90f, 1.25f, -1.25f };
                    for (int lat = 0; lat < 7; ++lat)
                    {
                        s_latCos[lat] = std::cos(latAngles[lat]);
                        s_latSin[lat] = std::sin(latAngles[lat]);
                    }
                    s_tablesInit = true;
                }

                // 経線の描画（8本）
                for (int m = 0; m < 8; ++m)
                {
                    float cosM = s_meridianCos[m], sinM = s_meridianSin[m];

                    std::vector<ImVec2> pts2D;
                    pts2D.reserve(numSegments + 1);
                    for (int i = 0; i <= numSegments; ++i)
                    {
                        Vector3 local = { s_sinTable[i] * cosM * radius, s_cosTable[i] * radius, s_sinTable[i] * sinM * radius };
                        ImVec2 p2D;
                        if (project3DTo2D(transformLocal(local), p2D)) pts2D.push_back(p2D);
                    }
                    if (pts2D.size() > 1) drawList->AddPolyline(pts2D.data(), (int)pts2D.size(), colColor, false, (m == 0 || m == 4) ? 2.0f : 1.2f);
                }

                // 緯線の描画（7本: 赤道 + 上下各3本）
                for (int lat = 0; lat < 7; ++lat)
                {
                    float latR = radius * s_latCos[lat];
                    float latY = radius * s_latSin[lat];

                    std::vector<ImVec2> pts2D;
                    pts2D.reserve(numSegments + 1);
                    for (int i = 0; i <= numSegments; ++i)
                    {
                        Vector3 local = { s_cosTable[i] * latR, latY, s_sinTable[i] * latR };
                        ImVec2 p2D;
                        if (project3DTo2D(transformLocal(local), p2D)) pts2D.push_back(p2D);
                    }
                    if (pts2D.size() > 1) drawList->AddPolyline(pts2D.data(), (int)pts2D.size(), colColor, false, (lat == 0) ? 2.5f : 1.2f);
                }
            }
            else if (col->GetType() == ColliderType::Box)
            {
                BoxCollider* box = static_cast<BoxCollider*>(col);
                Vector3 ext = box->GetExtents();
                Vector3 rot = box->GetWorldRotation();

                Vector3 localCorners[8] = {
                    { -ext.x, -ext.y, -ext.z },
                    {  ext.x, -ext.y, -ext.z },
                    {  ext.x, -ext.y,  ext.z },
                    { -ext.x, -ext.y,  ext.z },
                    { -ext.x,  ext.y, -ext.z },
                    {  ext.x,  ext.y, -ext.z },
                    {  ext.x,  ext.y,  ext.z },
                    { -ext.x,  ext.y,  ext.z }
                };

                Vector3 worldCorners[8];
                for (int i = 0; i < 8; ++i)
                {
                    float cosX = std::cos(rot.x), sinX = std::sin(rot.x);
                    Vector3 pt1 = { localCorners[i].x, localCorners[i].y * cosX - localCorners[i].z * sinX, localCorners[i].y * sinX + localCorners[i].z * cosX };
                    float cosY = std::cos(rot.y), sinY = std::sin(rot.y);
                    Vector3 pt2 = { pt1.x * cosY + pt1.z * sinY, pt1.y, -pt1.x * sinY + pt1.z * cosY };
                    float cosZ = std::cos(rot.z), sinZ = std::sin(rot.z);
                    Vector3 pt3 = { pt2.x * cosZ - pt2.y * sinZ, pt2.x * sinZ + pt2.y * cosZ, pt2.z };
                    worldCorners[i] = pt3 + worldPos;
                }

                ImVec2 screenCorners[8];
                bool projected[8];
                for (int i = 0; i < 8; ++i)
                {
                    projected[i] = project3DTo2D(worldCorners[i], screenCorners[i]);
                }

                if (projected[0] && projected[1] && projected[2] && projected[3])
                {
                    ImVec2 pts[5] = { screenCorners[0], screenCorners[1], screenCorners[2], screenCorners[3], screenCorners[0] };
                    drawList->AddPolyline(pts, 5, colColor, false, 2.0f);
                }
                if (projected[4] && projected[5] && projected[6] && projected[7])
                {
                    ImVec2 pts[5] = { screenCorners[4], screenCorners[5], screenCorners[6], screenCorners[7], screenCorners[4] };
                    drawList->AddPolyline(pts, 5, colColor, false, 2.0f);
                }
                for (int i = 0; i < 4; ++i)
                {
                    if (projected[i] && projected[i + 4])
                    {
                        drawList->AddLine(screenCorners[i], screenCorners[i + 4], colColor, 2.0f);
                    }
                }
            }
            else if (col->GetType() == ColliderType::Capsule)
            {
                CapsuleCollider* capsule = static_cast<CapsuleCollider*>(col);
                float radius = capsule->GetRadius();
                float halfH = capsule->GetHeight() * 0.5f;

                Vector3 bottomCenter = worldPos - Vector3{ 0.0f, halfH, 0.0f };
                Vector3 topCenter = worldPos + Vector3{ 0.0f, halfH, 0.0f };

                const int numSegments = 16;
                std::vector<ImVec2> ptsBottom;
                std::vector<ImVec2> ptsTop;
                for (int i = 0; i <= numSegments; ++i)
                {
                    float angle = i * (6.2831853f / numSegments);
                    Vector3 pBottom = { bottomCenter.x + std::cos(angle) * radius, bottomCenter.y, bottomCenter.z + std::sin(angle) * radius };
                    Vector3 pTop = { topCenter.x + std::cos(angle) * radius, topCenter.y, topCenter.z + std::sin(angle) * radius };

                    ImVec2 pB2D, pT2D;
                    if (project3DTo2D(pBottom, pB2D)) ptsBottom.push_back(pB2D);
                    if (project3DTo2D(pTop, pT2D)) ptsTop.push_back(pT2D);
                }
                if (ptsBottom.size() > 1) drawList->AddPolyline(ptsBottom.data(), (int)ptsBottom.size(), colColor, false, 2.0f);
                if (ptsTop.size() > 1) drawList->AddPolyline(ptsTop.data(), (int)ptsTop.size(), colColor, false, 2.0f);

                Vector3 sides[4] = { { radius, 0.0f, 0.0f }, { -radius, 0.0f, 0.0f }, { 0.0f, 0.0f, radius }, { 0.0f, 0.0f, -radius } };
                for (int i = 0; i < 4; ++i)
                {
                    ImVec2 pB2D, pT2D;
                    if (project3DTo2D(bottomCenter + sides[i], pB2D) && project3DTo2D(topCenter + sides[i], pT2D))
                    {
                        drawList->AddLine(pB2D, pT2D, colColor, 2.0f);
                    }
                }
            }
            else if (col->GetType() == ColliderType::Mesh)
            {
                MeshCollider* meshCollider = static_cast<MeshCollider*>(col);
                if (meshCollider && meshCollider->GetObject3d())
                {
                    meshCollider->Update();
                    Object3d* obj = meshCollider->GetObject3d();
                    Matrix4x4 world = obj->GetWorldMatrix();

                    const auto& modelData = obj->GetModelData();
                    ImU32 meshWireColor = ImGui::ColorConvertFloat4ToU32({ 0.0f, 0.9f, 1.0f, 0.75f });
                    const auto& verts = modelData.vertices;
                    const auto& indices = modelData.indices;

                    size_t numTris = indices.empty() ? (verts.size() / 3) : (indices.size() / 3);
                    size_t maxTris = (std::min)(numTris, size_t(384));

                    for (size_t t = 0; t < maxTris; ++t)
                    {
                        Vector4 p0_4 = indices.empty() ? verts[t * 3 + 0].position : verts[indices[t * 3 + 0]].position;
                        Vector4 p1_4 = indices.empty() ? verts[t * 3 + 1].position : verts[indices[t * 3 + 1]].position;
                        Vector4 p2_4 = indices.empty() ? verts[t * 3 + 2].position : verts[indices[t * 3 + 2]].position;

                        Vector3 p0 = { p0_4.x * world.m[0][0] + p0_4.y * world.m[1][0] + p0_4.z * world.m[2][0] + world.m[3][0],
                                       p0_4.x * world.m[0][1] + p0_4.y * world.m[1][1] + p0_4.z * world.m[2][1] + world.m[3][1],
                                       p0_4.x * world.m[0][2] + p0_4.y * world.m[1][2] + p0_4.z * world.m[2][2] + world.m[3][2] };
                        Vector3 p1 = { p1_4.x * world.m[0][0] + p1_4.y * world.m[1][0] + p1_4.z * world.m[2][0] + world.m[3][0],
                                       p1_4.x * world.m[0][1] + p1_4.y * world.m[1][1] + p1_4.z * world.m[2][1] + world.m[3][1],
                                       p1_4.x * world.m[0][2] + p1_4.y * world.m[1][2] + p1_4.z * world.m[2][2] + world.m[3][2] };
                        Vector3 p2 = { p2_4.x * world.m[0][0] + p2_4.y * world.m[1][0] + p2_4.z * world.m[2][0] + world.m[3][0],
                                       p2_4.x * world.m[0][1] + p2_4.y * world.m[1][1] + p2_4.z * world.m[2][1] + world.m[3][1],
                                       p2_4.x * world.m[0][2] + p2_4.y * world.m[1][2] + p2_4.z * world.m[2][2] + world.m[3][2] };

                        ImVec2 s0, s1, s2;
                        if (project3DTo2D(p0, s0) && project3DTo2D(p1, s1) && project3DTo2D(p2, s2))
                        {
                            ImVec2 triPts[4] = { s0, s1, s2, s0 };
                            drawList->AddPolyline(triPts, 4, meshWireColor, false, 1.2f);
                        }
                    }
                }
            }
            else if (col->GetType() == ColliderType::Skeleton)
            {
                SkeletonCollider* skelCollider = static_cast<SkeletonCollider*>(col);
                if (skelCollider && skelCollider->GetObject3d())
                {
                    Object3d* obj = skelCollider->GetObject3d();
                    const auto& skeleton = obj->GetSkeleton();
                    if (!skeleton.joints.empty())
                    {
                        Matrix4x4 modelWorldMatrix = MakeAffineMatrix(obj->GetScale(), obj->GetRotate(), obj->GetTranslate());
                        for (size_t i = 0; i < skeleton.joints.size(); ++i)
                        {
                            Vector3 jointPos = skeleton.GetJointWorldPosition(i, modelWorldMatrix);
                            float radius = skelCollider->GetJointRadius(skeleton.joints[i].name);

                            const int numSegments = 8;
                            std::vector<ImVec2> pts2D;
                            for (int j = 0; j <= numSegments; ++j)
                            {
                                float angle = j * (6.2831853f / numSegments);
                                Vector3 p3D = { jointPos.x + std::cos(angle) * radius, jointPos.y, jointPos.z + std::sin(angle) * radius };
                                ImVec2 p2D;
                                if (project3DTo2D(p3D, p2D)) pts2D.push_back(p2D);
                            }
                            if (pts2D.size() > 1) drawList->AddPolyline(pts2D.data(), (int)pts2D.size(), colColor, false, 1.5f);

                            pts2D.clear();
                            for (int j = 0; j <= numSegments; ++j)
                            {
                                float angle = j * (6.2831853f / numSegments);
                                Vector3 p3D = { jointPos.x + std::cos(angle) * radius, jointPos.y + std::sin(angle) * radius, jointPos.z };
                                ImVec2 p2D;
                                if (project3DTo2D(p3D, p2D)) pts2D.push_back(p2D);
                            }
                            if (pts2D.size() > 1) drawList->AddPolyline(pts2D.data(), (int)pts2D.size(), colColor, false, 1.5f);
                        }
                    }
                }
            }
        }
#endif
    }
}
