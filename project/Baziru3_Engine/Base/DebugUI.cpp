#include "DebugUI.h"
#include "MaterialManager.h"
#include "SpriteManager.h"
#include "Camera.h"
#include "DebugCamera\DebugCamera.h"
#include <imgui.h>
#include <string>
#include <fstream>

DebugUI::DebugUI(MaterialManager* materialManager, SpriteManager* spriteManager, Camera* camera,
    Sprite::Transform* transformObject, bool* useMonsterBall, bool* drawObject, bool* drawSprite,
    DebugCamera* debugCamera)
    : materialManager_(materialManager), spriteManager_(spriteManager), camera_(camera), transformObject_(transformObject), useMonsterBall_(useMonsterBall), drawObject_(drawObject), drawSprite_(drawSprite), debugCamera_(debugCamera)
{
}

void DebugUI::Initialize()
{
    // Try to load saved debug camera on init
    if (debugCamera_)
    {
        const std::string path = "debug_camera_save.bin";
        std::ifstream ifs(path, std::ios::binary);
        if (ifs)
        {
            debugCamera_->LoadFromFile(path);
        }
    }
}

void DebugUI::Update()
{
#ifdef USE_IMGUI
    ImGui::ShowDemoWindow();
    // Debug: indicate which DebugUI implementation is running
    OutputDebugStringA("[Base/DebugUI.cpp] DebugUI::Update running\n");

    ImGui::Begin("Windows");

    if (materialManager_)
    {
        ImGui::ColorEdit4("Material Color", &materialManager_->GetMaterialDataColor().x);
    }

    // Sprite position window: size (500,100), sliders (x,y) with initial (100,100) and format integer 4 digits, decimal 1
    ImGui::SetNextWindowSize(ImVec2(500.0f, 100.0f), ImGuiCond_Once);
    ImGui::Begin("Sprite Position");
    if (spriteManager_)
    {
        auto& sprites = spriteManager_->GetSprites();
        for (size_t i = 0; i < sprites.size(); ++i)
        {
            Sprite* s = sprites[i].get();
            if (!s) continue;

            // Get current position
            const Vector2& p = s->GetPosition();
            float posArr[2] = { p.x, p.y };
            char label[64];
            // Label each slider so ImGui state is unique per sprite
            std::snprintf(label, sizeof(label), "Sprite %zu Position", i);
            // Slider range chosen to cover typical window coordinates; format "%4.1f" meets the integer 4 digits and 1 decimal requirement
            if (ImGui::SliderFloat2(label, posArr, 0.0f, 2000.0f, "%4.1f"))
            {
                s->SetPosition({ posArr[0], posArr[1] });
            }
        }
    }
    ImGui::End();

    if (useMonsterBall_)
        ImGui::Checkbox("useMonsterBall", useMonsterBall_);

    if (materialManager_)
        ImGui::Checkbox("LightSprite Flag", (bool*)&materialManager_->GetMaterialDataEnableLighting());

    if (spriteManager_)
    {
        for (auto& spritePtr : spriteManager_->GetSprites())
        {
            ImGui::Checkbox("LightObject Flag", (bool*)&spritePtr->GetMaterialDataSprite()->enableLighting);
        }
    }

    if (drawObject_)
        ImGui::Checkbox("DrawObject", drawObject_);
    if (drawSprite_)
        ImGui::Checkbox("DrawSprite", drawSprite_);

    if (transformObject_)
        ImGui::DragFloat3("Object Rotate", &transformObject_->rotate.x, 0.01f, -10.0f, 10.0f);

    // Material collapsing header
    if (ImGui::CollapsingHeader("Material"))
    {
        if (materialManager_)
        {
            bool enableLock = (materialManager_->GetMaterialDataEnableLighting() != 0);
            if (ImGui::Checkbox("Enable Lighting", &enableLock))
            {
                materialManager_->GetMaterialDataEnableLighting() = enableLock ? 1 : 0;
            }

            ImGui::SliderFloat("Shininess", &materialManager_->GetMaterialDataShininess(), 0.1f, 100.0f);

            ImGui::ColorEdit4("Material Color", &materialManager_->GetMaterialDataColor().x);
        }
    }

    if (ImGui::Button("Reset Camera") && camera_)
    {
        camera_->SetRotate({ 0.0f, 0.0f, 0.0f });
        camera_->SetTranslate({ 0.0f, 0.0f, -5.0f });
        camera_->Update();
    }

   
    if (camera_)
    {
        ImGui::Separator();
        if (ImGui::CollapsingHeader("Camera"))
        {
            // Position
            auto pos = camera_->GetTranslate();
            float posArr[3] = { pos.x, pos.y, pos.z };
            if (ImGui::DragFloat3("Camera Position", posArr, 0.1f))
            {
                camera_->SetTranslate({ posArr[0], posArr[1], posArr[2] });
                camera_->Update();
            }

            // Rotation
            auto rot = camera_->GetRotate();
            float rotArr[3] = { rot.x, rot.y, rot.z };
            if (ImGui::DragFloat3("Camera Rotate", rotArr, 0.01f))
            {
                camera_->SetRotate({ rotArr[0], rotArr[1], rotArr[2] });
                camera_->Update();
            }

            // FOV / near / far
            float fov = camera_->GetFovY();
            if (ImGui::SliderFloat("FOV (rad)", &fov, 0.1f, 1.5f))
            {
                camera_->SetFovY(fov);
                camera_->Update();
            }
            float nearZ = camera_->GetNearZ();
            if (ImGui::DragFloat("Near Z", &nearZ, 0.01f, 0.01f, 10.0f))
            {
                camera_->SetNearZ(nearZ);
                camera_->Update();
            }
            float farZ = camera_->GetFarZ();
            if (ImGui::DragFloat("Far Z", &farZ, 1.0f, 10.0f, 10000.0f))
            {
                camera_->SetFarZ(farZ);
                camera_->Update();
            }
        }
    }

    // DebugCamera editor: show as separate window so it's easy to find
    if (debugCamera_)
    {
        ImGui::Begin("Debug Camera");
        auto rot = debugCamera_->GetRotation();
        float rotArr[3] = { rot.x, rot.y, rot.z };
            if (ImGui::DragFloat3("DebugCam Rotate", rotArr, 0.01f))
            {
                // Always update debug camera rotation. Do not copy into main camera.
                debugCamera_->SetRotation({ rotArr[0], rotArr[1], rotArr[2] });
            }

        auto pos = debugCamera_->GetTranslation();
        float posArr[3] = { pos.x, pos.y, pos.z };
            if (ImGui::DragFloat3("DebugCam Position", posArr, 0.1f))
            {
                // Always update debug camera translation. Do not copy into main camera.
                debugCamera_->SetTranslation({ posArr[0], posArr[1], posArr[2] });
            }

        if (ImGui::Button("Save DebugCam"))
        {
            debugCamera_->SaveToFile("debug_camera_save.bin");
        }
        ImGui::SameLine();
        if (ImGui::Button("Load DebugCam"))
        {
            debugCamera_->LoadFromFile("debug_camera_save.bin");
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset DebugCam"))
        {
            debugCamera_->Reset();
        }

        bool camMode = cameraMode_;
        if (ImGui::Checkbox("Use DebugCamera for Rendering", &camMode))
        {
            // camMode = true -> render using DebugCamera
            cameraMode_ = camMode;
            if (camera_ && debugCamera_)
            {
                // DebugCamera receives input when rendering with it; main Camera otherwise
                camera_->SetControlEnabled(!cameraMode_);
                debugCamera_->SetControlEnabled(cameraMode_);

                // Configure Camera to use DebugCamera view/projection when enabled
                camera_->SetDebugCameraOverride(debugCamera_);
                camera_->EnableDebugCameraOverride(cameraMode_);
            }
        }
        ImGui::End();
    }

    ImGui::End();

    ImGui::Begin("Material Settings");

    if (materialManager_)
    {
        bool enableLighting = (materialManager_->GetMaterialDataEnableLighting() != 0);
        if (ImGui::Checkbox("Enable Lighting", &enableLighting))
        {
            materialManager_->GetMaterialDataEnableLighting() = enableLighting ? 1 : 0;
        }

        ImGui::DragFloat("Shininess", &materialManager_->GetMaterialDataShininess(), 0.5f, 0.1f, 100.0f);
        // Specular model selection
        int specModel = materialManager_->GetMaterialSpecularModel();
        const char* items = "Blinn-Phong\0Phong\0";
        if (ImGui::Combo("Specular Model", &specModel, items))
        {
            materialManager_->GetMaterialSpecularModel() = specModel;
        }
    }

    ImGui::End();

    ImGui::Begin("Settings");

    if (useMonsterBall_)
        ImGui::Checkbox("Use Monster Ball", useMonsterBall_);

    ImGui::Separator();

    if (ImGui::CollapsingHeader("Material"))
    {
        if (materialManager_)
        {
            bool enable = (materialManager_->GetMaterialDataEnableLighting() != 0);
            if (ImGui::Checkbox("Enable Lighting", &enable))
            {
                materialManager_->GetMaterialDataEnableLighting() = enable ? 1 : 0;
            }

            ImGui::SliderFloat("Shininess", &materialManager_->GetMaterialDataShininess(), 0.1f, 100.0f);

            ImGui::ColorEdit4("Color", &materialManager_->GetMaterialDataColor().x);
        }
    }

    ImGui::End();
#endif
}

void DebugUI::Finalize()
{
}