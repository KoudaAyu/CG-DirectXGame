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
            std::vector<Vector3> positions;
            positions.reserve(modelData.vertices.size());
            for (const auto& v : modelData.vertices)
            {
                positions.push_back({ v.position.x, v.position.y, v.position.z });
            }
            aabbTree_.Build(positions, modelData.indices);
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

private:
    Object3d* object3d_ = nullptr;
    BaziruEngine::Collision::AABBTree aabbTree_;
};
