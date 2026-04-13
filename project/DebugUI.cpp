#include "DebugUI.h"
#include "MaterialManager.h"
#include "SpriteManager.h"
#include "Camera.h"
#include <imgui.h>

DebugUI::DebugUI(MaterialManager* materialManager, SpriteManager* spriteManager, Camera* camera,
    Sprite::Transform* transformObject, bool* useMonsterBall, bool* drawObject, bool* drawSprite)
    : materialManager_(materialManager), spriteManager_(spriteManager), camera_(camera), transformObject_(transformObject), useMonsterBall_(useMonsterBall), drawObject_(drawObject), drawSprite_(drawSprite)
{
}

void DebugUI::Initialize()
{
}

void DebugUI::Update()
{
#ifdef USE_IMGUI
    ImGui::ShowDemoWindow();

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
    {
        ImGui::DragFloat3("Object Scale", &transformObject_->scale.x, 0.01f, 0.01f, 10.0f);
        ImGui::DragFloat3("Object Rotate", &transformObject_->rotate.x, 0.01f, -10.0f, 10.0f);
        ImGui::DragFloat3("Object Translate", &transformObject_->translate.x, 0.01f, -100.0f, 100.0f);
    }

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
    }

    if (camera_ && ImGui::CollapsingHeader("Camera"))
    {
        Vector3 cameraTranslate = camera_->GetWorldPosition();
        if (ImGui::DragFloat3("Camera Translate", &cameraTranslate.x, 0.05f, -100.0f, 100.0f))
        {
            camera_->SetTranslate(cameraTranslate);
        }

        Vector3 cameraRotate = camera_->GetRotate();
        if (ImGui::DragFloat3("Camera Rotate", &cameraRotate.x, 0.01f, -6.28318f, 6.28318f))
        {
            camera_->SetRotate(cameraRotate);
        }

        float fovY = camera_->GetFovY();
        if (ImGui::SliderFloat("Camera FovY", &fovY, 0.1f, 1.8f))
        {
            camera_->SetFovY(fovY);
        }
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