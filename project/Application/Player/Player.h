#pragma once
#include <memory>
#include "Object3d.h"
#include "Object3dCom.h"
#include "Camera.h"
#include "RenderContext.h"

class Player
{
public:
    void Initialize(Object3dCom* object3dCom, Camera* camera);
    // Update now accepts optional MouseInput pointer so player can face cursor
    void Update(class MouseInput* mouseInput = nullptr);
    void Draw(const RenderContext& ctx);
    void Finalize();
    Vector3 GetPosition() const { return object3d_ ? object3d_->GetTranslate() : Vector3{ 0.0f, 0.0f, 0.0f }; }
    Vector3 GetRotation() const { return object3d_ ? object3d_->GetRotate() : Vector3{ 0.0f, 0.0f, 0.0f }; }

private:
    std::unique_ptr<Object3d> object3d_;
    Object3dCom* object3dCom_ = nullptr;
    Camera* camera_ = nullptr;
    uint32_t defaultTextureIndex_ = UINT32_MAX;
};

