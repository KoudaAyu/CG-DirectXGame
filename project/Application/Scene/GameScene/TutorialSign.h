#pragma once
#include <memory>
#include <string>
#include "Object3d.h"
#include "Object3dCom.h"
#include "Camera.h"
#include "RenderContext.h"

class TutorialSign
{
public:
    void Initialize(Object3dCom* object3dCom, Camera* camera, const Vector3& position, const std::string& message, float triggerRadius = 3.0f);
    void Update(const Vector3& playerPosition);
    void Draw(const RenderContext& ctx);
    void Finalize();

    Vector3 GetPosition() const { return position_; }
    std::string GetMessage() const { return message_; }
    bool IsPlayerNear() const { return isPlayerNear_; }

private:
    std::unique_ptr<Object3d> object3d_;
    Vector3 position_;
    float triggerRadius_;
    std::string message_;
    bool isPlayerNear_ = false;
    Object3dCom* object3dCom_ = nullptr;
    uint32_t defaultTextureIndex_ = UINT32_MAX;
};
