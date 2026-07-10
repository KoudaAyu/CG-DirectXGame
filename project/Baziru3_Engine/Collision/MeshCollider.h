#pragma once
#include "Collider.h"
#include "Baziru3_Engine/3D/Object/Object3d.h"
#include "AABBTree.h"

class MeshCollider : public Collider
{
public:
    MeshCollider(Object3d* object3d, CollisionAttribute attribute);
    virtual ~MeshCollider() override = default;

    virtual Vector3 GetWorldPosition() const override;
    virtual void SetWorldPosition(const Vector3& pos) override;

    Object3d* GetObject3d() const { return object3d_; }
    const BaziruEngine::Collision::AABBTree& GetAABBTree() const { return aabbTree_; }
    const std::vector<Vector3>& GetSkinnedPositions() const { return skinnedPositions_; }

    void Update();

private:
    Object3d* object3d_ = nullptr;
    BaziruEngine::Collision::AABBTree aabbTree_;
    std::vector<Vector3> skinnedPositions_;
    uint32_t lastUpdatedFrame_ = 0xFFFFFFFF;
};
