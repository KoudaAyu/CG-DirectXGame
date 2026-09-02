#include "DebugUI.h"
#include "ParticleManager.h"
#include "ParticleEmitter.h"
#include "MaterialManager.h"
#include "SpriteManager.h"
#include "Camera.h"
#include "SceneManager.h"
#include "OffScreenRendering.h"
#include "ParticleManager.h"
#include "Application/Scene/GameScene/GamePlayScene.h"
#include "BehaviorTreeEditor.h"
#include "Baziru3_Engine/Graphics/Graphics/GpuProfiler.h"
#include "Baziru3_Engine/Framework/Collision/CollisionManager.h"
#include "Baziru3_Engine/Core/Base/Allocator/ConstantBufferAllocator.h"
#include "Baziru3_Engine/Core/Base/Allocator/StackAllocator.h"
#include "DirectXCom.h"
#include "Light.h"
#include <imgui.h>


DebugUI::DebugUI(MaterialManager* materialManager, SpriteManager* spriteManager, Camera* camera,
    Sprite::Transform* transformObject, bool* useMonsterBall, bool* drawObject, bool* drawSprite)
    : materialManager_(materialManager), spriteManager_(spriteManager), camera_(camera), transformObject_(transformObject), useMonsterBall_(useMonsterBall), drawObject_(drawObject), drawSprite_(drawSprite)
{
    std::memset(stages_, 0, sizeof(stages_));
}

DebugUI::~DebugUI()
{
}

void DebugUI::Initialize()
{
#ifdef USE_IMGUI
    btEditor_ = std::make_unique<BaziruEngine::AI::BehaviorTreeEditor>();
#endif
}

void DebugUI::Update()
{
#ifdef USE_IMGUI
    // GPUプロファイル結果を前フレームから回収する
    GpuProfiler::GetInstance()->ResolveResults();

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

            ImGui::SliderFloat("Reflection Factor", &materialManager_->GetMaterialReflectionFactor(), 0.0f, 1.0f);
            ImGui::SliderFloat("Fresnel F0", &materialManager_->GetMaterialFresnelF0(), 0.0f, 1.0f);

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

        ImGui::SliderFloat("Reflection Factor", &materialManager_->GetMaterialReflectionFactor(), 0.0f, 1.0f);
        ImGui::SliderFloat("Fresnel F0", &materialManager_->GetMaterialFresnelF0(), 0.0f, 1.0f);

        // Specular model selection
        int specModel = materialManager_->GetMaterialSpecularModel();
        const char* items = "Blinn-Phong\0Phong\0";
        if (ImGui::Combo("Specular Model", &specModel, items))
        {
            materialManager_->GetMaterialSpecularModel() = specModel;
        }
    }

    ImGui::End();

    // --- Lighting Settings (Directional Light & Point Light) ---
    ImGui::Begin("Lighting Settings");

    if (light_)
    {
        if (ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen))
        {
            auto dirData = light_->GetDirectionalLightData();
            float dirColor[4] = { dirData.color.x, dirData.color.y, dirData.color.z, dirData.color.w };
            if (ImGui::ColorEdit4("Dir Color", dirColor))
            {
                light_->SetDirectionalLightColor({ dirColor[0], dirColor[1], dirColor[2], dirColor[3] });
            }

            float dirVec[3] = { dirData.direction.x, dirData.direction.y, dirData.direction.z };
            if (ImGui::DragFloat3("Direction", dirVec, 0.02f, -1.0f, 1.0f))
            {
                light_->SetDirectionalLightDirection({ dirVec[0], dirVec[1], dirVec[2] });
            }

            float dirIntensity = dirData.intensity;
            if (ImGui::DragFloat("Dir Intensity", &dirIntensity, 0.05f, 0.0f, 10.0f))
            {
                light_->SetDirectionalLightIntensity(dirIntensity);
            }
        }

        if (ImGui::CollapsingHeader("Point Light", ImGuiTreeNodeFlags_DefaultOpen))
        {
            auto pointData = light_->GetPointLightData();
            float ptColor[4] = { pointData.color.x, pointData.color.y, pointData.color.z, pointData.color.w };
            if (ImGui::ColorEdit4("Point Color", ptColor))
            {
                light_->SetPointLightColor({ ptColor[0], ptColor[1], ptColor[2], ptColor[3] });
            }

            float ptPos[3] = { pointData.position.x, pointData.position.y, pointData.position.z };
            if (ImGui::DragFloat3("Point Pos", ptPos, 0.1f, -100.0f, 100.0f))
            {
                light_->SetPointLightPosition({ ptPos[0], ptPos[1], ptPos[2] });
            }

            float ptIntensity = pointData.intensity;
            if (ImGui::DragFloat("Point Intensity", &ptIntensity, 0.05f, 0.0f, 20.0f))
            {
                light_->SetPointLightIntensity(ptIntensity);
            }

            float ptRadius = pointData.radius;
            if (ImGui::DragFloat("Point Radius", &ptRadius, 0.2f, 0.1f, 100.0f))
            {
                light_->SetPointLightRadius(ptRadius);
            }

            float ptDecay = pointData.decay;
            if (ImGui::DragFloat("Point Decay", &ptDecay, 0.1f, 0.5f, 5.0f))
            {
                light_->SetPointLightDecay(ptDecay);
            }
        }
    }

    ImGui::End();

    ImGui::Begin("Settings");

    if (offScreenRendering_)
    {
        if (ImGui::CollapsingHeader("Post Effect"))
        {
            int currentEffect = static_cast<int>(offScreenRendering_->GetPostEffect());
            const char* effectNames[] = {
                "Normal",
                "DepthBasedOutline",
                "LuminanceBaseOutline",
                "RadialBlur",
                "GaussianFilter",
                "BoxFilter"
            };
            if (ImGui::Combo("Effect Type", &currentEffect, effectNames, IM_ARRAYSIZE(effectNames)))
            {
                offScreenRendering_->SetPostEffect(static_cast<OffScreenRendering::PostEffect>(currentEffect));
            }

            if (offScreenRendering_->GetPostEffect() == OffScreenRendering::PostEffect::RadialBlur)
            {
                Vector2 center = offScreenRendering_->GetRadialBlurCenter();
                float centerArr[2] = { center.x, center.y };
                if (ImGui::SliderFloat2("Blur Center", centerArr, 0.0f, 1.0f))
                {
                    offScreenRendering_->SetRadialBlurCenter({ centerArr[0], centerArr[1] });
                }

                float blurWidth = offScreenRendering_->GetRadialBlurWidth();
                if (ImGui::SliderFloat("Blur Width", &blurWidth, 0.0f, 0.1f, "%.4f"))
                {
                    offScreenRendering_->SetRadialBlurWidth(blurWidth);
                }
            }
        }
    }

    if (useMonsterBall_)
        ImGui::Checkbox("Use Monster Ball", useMonsterBall_);

    ImGui::Checkbox("Draw Skybox", SceneManager::GetInstance()->GetShowSkyboxPtr());

    if (ImGui::CollapsingHeader("GPU Particle System"))
    {
        ParticleManager* pm = ParticleManager::GetInstance();
        if (pm)
        {
            ImGui::Text("Shader Layer: GPU Compute Shader");
            ImGui::Text("Max Capacity: %u particles", 10240);
            uint32_t activeCount = pm->GetNumInstance();
            ImGui::Text("Active Count: %u particles", activeCount);
            ImGui::ProgressBar(static_cast<float>(activeCount) / 10240.0f, ImVec2(0.0f, 0.0f), "Spawn Load");

            // エミッター設定は GamePlayScene 内の ImGui パネルで管理
        }
        else
        {
            ImGui::Text("GPU Particle System not initialized.");
        }
    }

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

            ImGui::SliderFloat("Reflection Factor", &materialManager_->GetMaterialReflectionFactor(), 0.0f, 1.0f);
            ImGui::SliderFloat("Fresnel F0", &materialManager_->GetMaterialFresnelF0(), 0.0f, 1.0f);

            ImGui::ColorEdit4("Color", &materialManager_->GetMaterialDataColor().x);
        }
    }

    if (materialManager_)
    {
        materialManager_->Update();
    }


    ImGui::End();

    ImGui::SetNextWindowSize(ImVec2(440.0f, 540.0f), ImGuiCond_Once);
    ImGui::Begin("Performance Tracker");
    {
        float currentFps = ImGui::GetIO().Framerate;
        float frameTimeMs = 1000.0f / (currentFps > 0.1f ? currentFps : 60.0f);

        // --- 1. FPS & Frame Time Badge Header ---
        ImVec4 fpsColor = (currentFps >= 55.0f) ? ImVec4(0.2f, 0.9f, 0.3f, 1.0f)
                       : (currentFps >= 30.0f) ? ImVec4(0.9f, 0.8f, 0.2f, 1.0f)
                       :                         ImVec4(0.9f, 0.2f, 0.2f, 1.0f);

        ImGui::TextColored(fpsColor, "FPS: %.1f", currentFps);
        ImGui::SameLine(140.0f);
        ImGui::Text("Frame Time: %.2f ms", frameTimeMs);
        ImGui::SameLine(310.0f);
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Target: 16.67 ms");

        static float fpsHistory[120] = {};
        static int fpsHistoryOffset = 0;
        fpsHistory[fpsHistoryOffset] = currentFps;
        fpsHistoryOffset = (fpsHistoryOffset + 1) % 120;

        float minFps = fpsHistory[0], maxFps = fpsHistory[0], avgFps = 0.0f;
        for (int i = 0; i < 120; ++i)
        {
            if (fpsHistory[i] < minFps) minFps = fpsHistory[i];
            if (fpsHistory[i] > maxFps) maxFps = fpsHistory[i];
            avgFps += fpsHistory[i];
        }
        avgFps /= 120.0f;

        char fpsLabel[64];
        std::snprintf(fpsLabel, sizeof(fpsLabel), "Min: %.1f | Avg: %.1f | Max: %.1f", minFps, avgFps, maxFps);
        ImGui::PlotLines("##FPSGraph", fpsHistory, 120, fpsHistoryOffset, fpsLabel, 0.0f, 120.0f, ImVec2(0, 45.0f));

        ImGui::Separator();

        // --- 2. GPU Particle Load Metric ---
        ParticleManager* pm = ParticleManager::GetInstance();
        if (pm)
        {
            uint32_t activeParticles = pm->GetNumInstance();
            uint32_t maxParticles = 10240; // GPU Particle Capacity
            float particleLoadRatio = static_cast<float>(activeParticles) / static_cast<float>(maxParticles);

            ImGui::Text("GPU Particles:");
            ImGui::SameLine(140.0f);
            ImGui::Text("%u / %u", activeParticles, maxParticles);

            char loadLabel[32];
            std::snprintf(loadLabel, sizeof(loadLabel), "%.1f%% Load", particleLoadRatio * 100.0f);
            ImGui::ProgressBar(particleLoadRatio, ImVec2(-1.0f, 0.0f), loadLabel);
            ImGui::Separator();
        }

        // --- 3. Custom Memory Allocators (CB Ring Allocator & Stack Allocator) ---
        DirectXCom* dxCom = (pm ? pm->GetDxCommon() : nullptr);
        if (dxCom)
        {
            ConstantBufferAllocator* cbAlloc = dxCom->GetCBAllocator();
            StackAllocator* stackAlloc = dxCom->GetStackAllocator();

            if (cbAlloc)
            {
                size_t allocated = cbAlloc->GetAllocatedThisFrame();
                size_t frameCapacity = cbAlloc->GetFrameSize();
                float ratio = cbAlloc->GetUsageRatio();

                ImGui::Text("CB Ring Allocator:");
                ImGui::SameLine(180.0f);
                ImGui::Text("%.1f KB / %.1f MB", allocated / 1024.0f, frameCapacity / (1024.0f * 1024.0f));

                char cbLabel[32];
                std::snprintf(cbLabel, sizeof(cbLabel), "%.2f%% Frame Usage", ratio * 100.0f);
                ImGui::ProgressBar(ratio, ImVec2(-1.0f, 0.0f), cbLabel);
            }

            if (stackAlloc)
            {
                size_t used = stackAlloc->GetUsedBytes();
                size_t total = stackAlloc->GetTotalBytes();
                float ratio = total > 0 ? static_cast<float>(used) / static_cast<float>(total) : 0.0f;

                ImGui::Text("Stack Allocator:");
                ImGui::SameLine(180.0f);
                ImGui::Text("%.1f KB / %.1f MB", used / 1024.0f, total / (1024.0f * 1024.0f));

                char stackLabel[32];
                std::snprintf(stackLabel, sizeof(stackLabel), "%.2f%% Usage", ratio * 100.0f);
                ImGui::ProgressBar(ratio, ImVec2(-1.0f, 0.0f), stackLabel);
            }
            ImGui::Separator();
        }

        // --- 4. GPU / CPU Stage Profiler ---
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "--- GPU / CPU Stage Profiler ---");


        // 全ステージの active フラグをリセット
        for (int i = 0; i < kMaxStages; ++i)
        {
            stages_[i].active = false;
        }

        auto getStageIndex = [&](const std::string& name) -> int {
            for (int i = 0; i < kMaxStages; ++i)
            {
                if (stages_[i].name[0] == '\0')
                {
                    strcpy_s(stages_[i].name, sizeof(stages_[i].name), name.c_str());
                    return i;
                }
                if (std::strcmp(stages_[i].name, name.c_str()) == 0)
                {
                    return i;
                }
            }
            return -1;
        };

        // GPU プロファイル結果の回収
        const auto& gpuResults = GpuProfiler::GetInstance()->GetResults();
        float totalGpuTimeMs = 0.0f;
        for (const auto& res : gpuResults)
        {
            int idx = getStageIndex(res.name);
            if (idx >= 0)
            {
                stages_[idx].active = true;
                stages_[idx].history[historyOffset_] = res.timeMs;
                totalGpuTimeMs += res.timeMs;
            }
        }

        // CPU 衝突判定の時間
        float collisionTime = CollisionManager::GetInstance()->GetLastUpdateDurationMs();
        int colIdx = getStageIndex("Collision (CPU)");
        if (colIdx >= 0)
        {
            stages_[colIdx].active = true;
            stages_[colIdx].history[historyOffset_] = collisionTime;
        }

        historyOffset_ = (historyOffset_ + 1) % kMaxHistoryFrames;

        // --- 4. GPU Pass Share Visual Breakdown Bar ---
        ImGui::Text("Total GPU Render Time: %.3f ms", totalGpuTimeMs);

        // 各パスの色定義
        struct PassColor { const char* prefix; ImVec4 color; const char* label; };
        PassColor passColors[] = {
            { "Scene",       ImVec4(0.2f, 0.6f, 1.0f, 1.0f), "Scene (3D)" },
            { "Sprite",      ImVec4(0.2f, 0.9f, 0.4f, 1.0f), "Sprite" },
            { "Particle",    ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "Particle" },
            { "PostProcess", ImVec4(0.8f, 0.3f, 0.9f, 1.0f), "PostProcess" },
            { "Collision",   ImVec4(0.9f, 0.9f, 0.2f, 1.0f), "Collision" }
        };

        // 突破バーを描画（ImDrawList）
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 barPos = ImGui::GetCursorScreenPos();
        float barWidth = ImGui::GetContentRegionAvail().x;
        float barHeight = 16.0f;

        // 背景
        drawList->AddRectFilled(barPos, ImVec2(barPos.x + barWidth, barPos.y + barHeight), IM_COL32(40, 40, 45, 255), 3.0f);

        if (totalGpuTimeMs > 0.001f)
        {
            float currentX = barPos.x;
            for (const auto& pc : passColors)
            {
                int idx = getStageIndex(pc.prefix);
                if (idx < 0) continue;
                int prevOffset = (historyOffset_ == 0) ? kMaxHistoryFrames - 1 : historyOffset_ - 1;
                float passMs = stages_[idx].history[prevOffset];
                float segWidth = (passMs / totalGpuTimeMs) * barWidth;

                if (segWidth > 1.0f)
                {
                    ImU32 col = ImGui::ColorConvertFloat4ToU32(pc.color);
                    drawList->AddRectFilled(ImVec2(currentX, barPos.y), ImVec2(currentX + segWidth - 1.0f, barPos.y + barHeight), col, 2.0f);
                    currentX += segWidth;
                }
            }
        }
        ImGui::Dummy(ImVec2(barWidth, barHeight + 4.0f));

        // 色凡例
        for (const auto& pc : passColors)
        {
            ImGui::ColorButton("##col", pc.color, ImGuiColorEditFlags_NoTooltip, ImVec2(10, 10));
            ImGui::SameLine();
            ImGui::TextColored(pc.color, "%s", pc.label);
            ImGui::SameLine(0, 12);
        }
        ImGui::NewLine();

        ImGui::Separator();

        // --- 5. Detailed Pass Table ---
        if (ImGui::BeginTable("ProfilerTable", 4, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Pass Name", ImGuiTableColumnFlags_WidthStretch, 150.0f);
            ImGui::TableSetupColumn("Last (ms)", ImGuiTableColumnFlags_WidthFixed, 75.0f);
            ImGui::TableSetupColumn("Share (%)", ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ImGui::TableSetupColumn("Max (ms)",  ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ImGui::TableHeadersRow();

            for (int i = 0; i < kMaxStages; ++i)
            {
                if (stages_[i].name[0] == '\0') continue;

                int prevOffset = (historyOffset_ == 0) ? kMaxHistoryFrames - 1 : historyOffset_ - 1;
                float lastVal = stages_[i].history[prevOffset];

                float maxVal = 0.001f;
                for (int j = 0; j < kMaxHistoryFrames; ++j)
                {
                    if (stages_[i].history[j] > maxVal) maxVal = stages_[i].history[j];
                }

                float share = (totalGpuTimeMs > 0.001f) ? (lastVal / totalGpuTimeMs) * 100.0f : 0.0f;

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", stages_[i].name);

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.3f", lastVal);

                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%.1f%%", share);

                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%.2f", maxVal);
            }
            ImGui::EndTable();
        }

        // --- 6. Individual Pass Sparklines ---
        if (ImGui::CollapsingHeader("Individual Pass Graphs"))
        {
            for (int i = 0; i < kMaxStages; ++i)
            {
                if (stages_[i].name[0] == '\0') continue;

                int prevOffset = (historyOffset_ == 0) ? kMaxHistoryFrames - 1 : historyOffset_ - 1;
                float lastVal = stages_[i].history[prevOffset];

                float maxVal = 0.1f;
                for (int j = 0; j < kMaxHistoryFrames; ++j)
                {
                    if (stages_[i].history[j] > maxVal) maxVal = stages_[i].history[j];
                }
                maxVal *= 1.2f;

                char graphLabel[64];
                std::snprintf(graphLabel, sizeof(graphLabel), "%.3f ms (Max: %.2f)", lastVal, maxVal);

                std::string imguiId = "##" + std::string(stages_[i].name);
                ImGui::Text("%s", stages_[i].name);
                ImGui::PlotLines(imguiId.c_str(), stages_[i].history, kMaxHistoryFrames, historyOffset_, graphLabel, 0.0f, maxVal, ImVec2(0, 30.0f));
            }
        }
    }
    ImGui::End();

    if (btEditor_)
    {
        btEditor_->Draw();
    }
#endif
}


void DebugUI::Finalize()
{
#ifdef USE_IMGUI
    if (btEditor_) {
        btEditor_.reset();
    }
#endif
}