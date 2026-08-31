#pragma once
#include <memory>
#include "Object3d.h"
#include "Object3dCom.h"
#include "Camera.h"
#include "RenderContext.h"
#include "Baziru3_Engine/Framework/Collision/BoxCollider.h"
#include "Baziru3_Engine/Framework/Collision/MeshCollider.h"

class Obstacle
{
public:
    void Initialize(Object3dCom* object3dCom, Camera* camera, const Vector3& position, float radius, const std::string& modelFilename = "fence.obj", const Vector3& scale = { 1.0f, 1.0f, 1.0f }, const Vector3& rotation = { 0.0f, 0.0f, 0.0f });
    void Update();
    void Draw(const RenderContext& ctx);
    void Finalize();

    Vector3 GetPosition() const { return position_; }
    float GetRadius() const { return radius_; }
    BoxCollider* GetCollider() const { return collider_.get(); }
    BoxCollider* GetCollider2() const { return extraColliders_.empty() ? nullptr : extraColliders_[0].get(); }
    const std::vector<std::unique_ptr<BoxCollider>>& GetExtraColliders() const { return extraColliders_; }
    MeshCollider* GetMeshCollider() const { return meshCollider_.get(); }

private:
    std::unique_ptr<Object3d> object3d_;
    Vector3 position_;
    Vector3 rotation_;
    float radius_;
    std::unique_ptr<BoxCollider> collider_;
    std::vector<std::unique_ptr<BoxCollider>> extraColliders_;
    std::unique_ptr<MeshCollider> meshCollider_;
    Vector3 rot1_;
    Vector3 rot2_;
    std::vector<Vector3> extraRotations_;
    Object3dCom* object3dCom_ = nullptr;
    uint32_t defaultTextureIndex_ = UINT32_MAX;
};
