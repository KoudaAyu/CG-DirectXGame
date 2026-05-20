#pragma once
#include "Sprite.h"
#include <memory>
#include <vector>

class Camera;
class DebugCamera;
class MaterialManager;
class SpriteManager;

class DebugUI
{
public:
    DebugUI(MaterialManager* materialManager, SpriteManager* spriteManager, Camera* camera,
            Sprite::Transform* transformObject, bool* useMonsterBall, bool* drawObject, bool* drawSprite,
            DebugCamera* debugCamera = nullptr);

    void Initialize();
    void Update();
    void Finalize();

private:
    Sprite::Transform* transformObject_ = nullptr;
    bool* useMonsterBall_ = nullptr;
    bool* drawSphere_ = nullptr;
    bool* drawObject_ = nullptr;
    bool* drawSprite_ = nullptr;

private:
    Camera* camera_ = nullptr;
    DebugCamera* debugCamera_ = nullptr;
    bool cameraMode_ = false;
    SpriteManager* spriteManager_ = nullptr;
    MaterialManager* materialManager_ = nullptr;

    std::vector<std::unique_ptr<Sprite>> sprites;

};

