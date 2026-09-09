#define NOMINMAX
#include "Application/GameObject/GrowthCubeManager.h"

#include "Application/GameObject/SlimeManager.h"

#include <algorithm>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

GrowthCubeManager::~GrowthCubeManager()
{
    Finalize();
}

void GrowthCubeManager::Initialize(Object3dCom* object3dCom, Camera* camera)
{
    object3dCom_ = object3dCom;
    camera_ = camera;
    cubes_.clear();
    collectEvents_.clear();
    collectedCount_ = 0;
    editorMode_ = false;
}

void GrowthCubeManager::Finalize()
{
    cubes_.clear();
    collectEvents_.clear();
    collectedCount_ = 0;
    object3dCom_ = nullptr;
    camera_ = nullptr;
}

GrowthCube* GrowthCubeManager::Spawn(const Vector3& stageLocalPos, float size)
{
    if (!object3dCom_) return nullptr;

    auto cube = std::make_unique<GrowthCube>();
    cube->Initialize(object3dCom_, camera_, stageLocalPos, size);

    GrowthCube* raw = cube.get();
    cubes_.push_back(std::move(cube));
    return raw;
}

void GrowthCubeManager::Remove(GrowthCube* cube)
{
    if (!cube) return;

    cubes_.erase(std::remove_if(cubes_.begin(), cubes_.end(),
                                [cube](const std::unique_ptr<GrowthCube>& c) { return c.get() == cube; }),
                 cubes_.end());
}

void GrowthCubeManager::ClearAll()
{
    cubes_.clear();
    collectEvents_.clear();
    collectedCount_ = 0;
}

void GrowthCubeManager::RespawnAll()
{
    for (auto& cube : cubes_)
    {
        if (cube) cube->Respawn();
    }
    collectedCount_ = 0;
}

void GrowthCubeManager::Update(float deltaTime, const Vector2& stageTilt, const Vector2& pivot, SlimeManager* slimeManager)
{
    collectEvents_.clear();

    // エディタ中は取得判定をしない。置いた瞬間に足元で食べられるのを防ぐ
    SlimeManager* target = editorMode_ ? nullptr : slimeManager;

    for (auto& cube : cubes_)
    {
        if (!cube) continue;

        if (cube->Update(deltaTime, stageTilt, pivot, target))
        {
            collectEvents_.push_back(cube->GetPosition());
            ++collectedCount_;
        }
    }
}

void GrowthCubeManager::Draw(const RenderContext& ctx)
{
    for (auto& cube : cubes_)
    {
        if (cube) cube->Draw(ctx);
    }
}

void GrowthCubeManager::DrawImGui()
{
#ifdef USE_IMGUI
    // ImGui のフォントに日本語グリフが無いので、ラベルは全部 ASCII で書くこと
    if (!ImGui::CollapsingHeader("Growth Cube")) return;

    ImGui::Text("Placed: %d / Eaten: %d", GetTotalCount(), collectedCount_);

    ImGui::SeparatorText("Look");
    ImGui::DragFloat("Hover Amplitude", &GrowthCube::sHoverAmplitude, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Hover Speed", &GrowthCube::sHoverSpeed, 0.05f, 0.0f, 12.0f);
    ImGui::DragFloat("Spin Speed", &GrowthCube::sSpinSpeed, 0.05f, 0.0f, 12.0f);
    ImGui::DragFloat("Height Offset", &GrowthCube::sHeightOffset, 0.01f, 0.0f, 4.0f);

    ImGui::SeparatorText("Gaming Color");
    ImGui::DragFloat("Time Scale", &GrowthCube::sGamingTimeScale, 0.01f, 0.0f, 4.0f);
    ImGui::DragFloat("Space Scale", &GrowthCube::sGamingSpaceScale, 0.005f, 0.0f, 0.5f);
    ImGui::DragFloat("Gain", &GrowthCube::sGamingGain, 0.01f, 0.1f, 2.0f);

    ImGui::SeparatorText("Pickup");
    ImGui::DragFloat("Pickup Margin (m)", &GrowthCube::sPickupRadius, 0.01f, 0.0f, 3.0f);
    ImGui::DragFloat("Collect Seconds", &GrowthCube::sCollectSeconds, 0.01f, 0.1f, 2.0f);

    if (ImGui::Button("Respawn All"))
    {
        RespawnAll();
    }
#endif
}
