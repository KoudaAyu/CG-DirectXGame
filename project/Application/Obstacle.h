#pragma once
#include <memory>
#include "Object3d.h"
#include "Object3dCom.h"
#include "Camera.h"
#include "RenderContext.h"
#include "Baziru3_Engine/Collision/BoxCollider.h"

class Obstacle
{
public:
    void Initialize(Object3dCom* object3dCom, Camera* camera, const Vector3& position, float radius);
    void Update();
    void Draw(const RenderContext& ctx);
    void Finalize();

    Vector3 GetPosition() const { return position_; }
    float GetRadius() const { return radius_; }
    BoxCollider* GetCollider() const { return collider_.get(); }

private:
    std::unique_ptr<Object3d> object3d_;
    Vector3 position_;
    float radius_;
    std::unique_ptr<BoxCollider> collider_;
    Object3dCom* object3dCom_ = nullptr;
    uint32_t defaultTextureIndex_ = UINT32_MAX;
};

