#pragma once
#include <memory>
#include "Object3d.h"
#include "Object3dCom.h"
#include "Camera.h"
#include "RenderContext.h"

class Enemy
{
public:
    void Initialize(Object3dCom* object3dCom, Camera* camera);
    void Update();
    void Draw(const RenderContext& ctx);
    void Finalize();
    void OnHit();
    Vector3 GetPosition() const { return object3d_ ? object3d_->GetTranslate() : Vector3{ 0.0f, 0.0f, 0.0f }; }

private:
    std::unique_ptr<Object3d> object3d_;
    Object3dCom* object3dCom_ = nullptr;
    Camera* camera_ = nullptr;
    uint32_t defaultTextureIndex_ = UINT32_MAX;
    float hitFlashTimer_ = 0.0f;
    float hitFlashDuration_ = 0.12f;
};

