#pragma once
#include "Collider.h"
#include "Baziru3_Engine/3D/Object/Object3d.h"
#include "AABBTree.h"

class MeshCollider : public Collider
{
public:
    MeshCollider(Object3d* object3d, CollisionAttribute attribute)
        : Collider(ColliderType::Mesh, attribute)
        , object3d_(object3d)
    {
        if (object3d_)
        {
            // Build AABB Tree from model data vertices and indices
            const auto& modelData = object3d_->GetModelData();
            skinnedPositions_.reserve(modelData.vertices.size());
            for (const auto& v : modelData.vertices)
            {
                skinnedPositions_.push_back({ v.position.x, v.position.y, v.position.z });
            }
            aabbTree_.Build(skinnedPositions_, modelData.indices);
        }
    }

    virtual ~MeshCollider() override = default;

    virtual Vector3 GetWorldPosition() const override
    {
        return object3d_ ? object3d_->GetTranslate() : Vector3{ 0.0f, 0.0f, 0.0f };
    }

    virtual void SetWorldPosition(const Vector3& pos) override
    {
        if (object3d_)
        {
            object3d_->SetTranslate(pos);
        }
    }

    Object3d* GetObject3d() const { return object3d_; }
    const BaziruEngine::Collision::AABBTree& GetAABBTree() const { return aabbTree_; }
    const std::vector<Vector3>& GetSkinnedPositions() const { return skinnedPositions_; }

    void Update()
    {
        if (!object3d_ || !object3d_->HasAnimation()) return;

        const auto& modelData = object3d_->GetModelData();
        const auto& skinCluster = object3d_->GetSkinCluster();

        if (skinCluster.mappedInfluence.empty() || skinCluster.mappedPalette.empty()) return;

        skinnedPositions_.clear();
        skinnedPositions_.resize(modelData.vertices.size());

        for (size_t i = 0; i < modelData.vertices.size(); ++i)
        {
            const auto& v = modelData.vertices[i];
            const auto& influence = skinCluster.mappedInfluence[i];
            Vector3 skinnedPos = { 0.0f, 0.0f, 0.0f };
            bool processed = false;

            for (int j = 0; j < 4; ++j)
            {
                float w = influence.weights[j];
                if (w > 0.0f)
                {
                    int32_t jointIdx = influence.jointIndices[j];
                    if (jointIdx >= 0 && jointIdx < static_cast<int32_t>(skinCluster.mappedPalette.size()))
                    {
                        const Matrix4x4& m = skinCluster.mappedPalette[jointIdx].skeletonSpaceMatrix;
                        Vector3 vTransformed = {
                            v.position.x * m.m[0][0] + v.position.y * m.m[1][0] + v.position.z * m.m[2][0] + m.m[3][0],
                            v.position.x * m.m[0][1] + v.position.y * m.m[1][1] + v.position.z * m.m[2][1] + m.m[3][1],
                            v.position.x * m.m[0][2] + v.position.y * m.m[1][2] + v.position.z * m.m[2][2] + m.m[3][2]
                        };
                        skinnedPos.x += vTransformed.x * w;
                        skinnedPos.y += vTransformed.y * w;
                        skinnedPos.z += vTransformed.z * w;
                        processed = true;
                    }
                }
            }

            if (!processed)
            {
                skinnedPositions_[i] = { v.position.x, v.position.y, v.position.z };
            }
            else
            {
                skinnedPositions_[i] = skinnedPos;
            }
        }

        aabbTree_.Build(skinnedPositions_, modelData.indices);
    }

private:
    Object3d* object3d_ = nullptr;
    BaziruEngine::Collision::AABBTree aabbTree_;
    std::vector<Vector3> skinnedPositions_;
};
