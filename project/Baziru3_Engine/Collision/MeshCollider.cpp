#include "MeshCollider.h"
#include "CollisionManager.h"

MeshCollider::MeshCollider(Object3d* object3d, CollisionAttribute attribute)
    : Collider(ColliderType::Mesh, attribute)
    , object3d_(object3d)
{
    if (object3d_)
    {
        const auto& modelData = object3d_->GetModelData();
        skinnedPositions_.reserve(modelData.vertices.size());
        for (const auto& v : modelData.vertices)
        {
            skinnedPositions_.push_back({ v.position.x, v.position.y, v.position.z });
        }
        aabbTree_.Build(skinnedPositions_, modelData.indices);
    }
}

Vector3 MeshCollider::GetWorldPosition() const
{
    return object3d_ ? object3d_->GetTranslate() : Vector3{ 0.0f, 0.0f, 0.0f };
}

void MeshCollider::SetWorldPosition(const Vector3& pos)
{
    if (object3d_)
    {
        object3d_->SetTranslate(pos);
    }
}

void MeshCollider::Update()
{
    if (!object3d_ || !object3d_->HasAnimation()) return;

    // Frame-level caching to prevent double-updates
    uint32_t currentFrame = CollisionManager::GetInstance()->GetFrameCount();
    if (lastUpdatedFrame_ == currentFrame) return;

    // Update rate throttling (Temporal sub-sampling):
    // Update animated collision geometry once every 2 frames (30Hz) to cut CPU load in half.
    // Stagger the updates based on the collider's unique pointer address to distribute the workload evenly across frames!
    // Since world-matrix translation/rotation is evaluated every frame, character movement remains 100% real-time.
    if ((currentFrame + (reinterpret_cast<uintptr_t>(this) >> 4)) % 2 != 0)
    {
        return;
    }
    lastUpdatedFrame_ = currentFrame;

    const auto& modelData = object3d_->GetModelData();
    const auto& skinCluster = object3d_->GetSkinCluster();

    if (skinCluster.mappedInfluence.empty() || skinCluster.mappedPalette.empty()) return;

    const size_t vertexCount = modelData.vertices.size();
    if (vertexCount == 0) return;

    skinnedPositions_.resize(vertexCount);

    // Bypass debug vector index bounds checks in Debug mode via raw pointers
    const Sprite::VertexData* vertices = modelData.vertices.data();
    const VertexInfluence* influences = skinCluster.mappedInfluence.data();
    const WellForGPU* palette = skinCluster.mappedPalette.data();
    const size_t paletteSize = skinCluster.mappedPalette.size();
    Vector3* skinnedOut = skinnedPositions_.data();

    for (size_t i = 0; i < vertexCount; ++i)
    {
        const Sprite::VertexData& v = vertices[i];
        const VertexInfluence& influence = influences[i];
        Vector3 skinnedPos = { 0.0f, 0.0f, 0.0f };
        bool processed = false;
        float weightSum = 0.0f;

        for (int j = 0; j < 4; ++j)
        {
            float w = influence.weights[j];
            if (w > 0.0f)
            {
                int32_t jointIdx = influence.jointIndices[j];
                if (jointIdx >= 0 && jointIdx < static_cast<int32_t>(paletteSize))
                {
                    const Matrix4x4& m = palette[jointIdx].skeletonSpaceMatrix;
                    Vector3 vTransformed = {
                        v.position.x * m.m[0][0] + v.position.y * m.m[1][0] + v.position.z * m.m[2][0] + m.m[3][0],
                        v.position.x * m.m[0][1] + v.position.y * m.m[1][1] + v.position.z * m.m[2][1] + m.m[3][1],
                        v.position.x * m.m[0][2] + v.position.y * m.m[1][2] + v.position.z * m.m[2][2] + m.m[3][2]
                    };
                    skinnedPos.x += vTransformed.x * w;
                    skinnedPos.y += vTransformed.y * w;
                    skinnedPos.z += vTransformed.z * w;
                    processed = true;

                    // Optimization: Break out early if weights have summed to 1.0 (avoids unnecessary iterations)
                    weightSum += w;
                    if (weightSum >= 0.999f)
                    {
                        break;
                    }
                }
            }
        }

        skinnedOut[i] = processed ? skinnedPos : Vector3{ v.position.x, v.position.y, v.position.z };
    }

    // Refit bounding boxes bottom-up instead of rebuilding the entire tree structure every frame (thousands of times faster!)
    aabbTree_.Update(skinnedPositions_);
}
