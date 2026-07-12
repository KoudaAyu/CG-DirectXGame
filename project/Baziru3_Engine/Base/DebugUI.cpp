#include "DebugUI.h"
#include "MaterialManager.h"
#include "SpriteManager.h"
#include "Camera.h"
#include "SceneManager.h"
#include "OffScreenRendering.h"
#include "ParticleManager.h"
#include "Application/Scene/GameScene/GamePlayScene.h"
#include "BehaviorTreeEditor.h"
#include "Baziru3_Engine/Graphics/GpuProfiler.h"
#include "Baziru3_Engine/Collision/CollisionManager.h"
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

            BaseScene* currentScene = SceneManager::GetInstance()->GetCurrentScene();
            GamePlayScene* gps = dynamic_cast<GamePlayScene*>(currentScene);
            if (gps)
            {
                ImGui::Separator();
                ImGui::Text("Emitter Settings:");
                Emitter& emitter = gps->GetEmitter();

                int countVal = static_cast<int>(emitter.count);
                if (ImGui::SliderInt("Spawn Count", &countVal, 1, 1000))
                {
                    emitter.count = static_cast<uint32_t>(countVal);
                }

                ImGui::SliderFloat("Spawn Frequency (sec)", &emitter.frequency, 0.01f, 2.0f, "%.2fs");
            }
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

    ImGui::Begin("Performance Tracker");
    {
        float currentFps = ImGui::GetIO().Framerate;
        ImGui::Text("Current FPS: %.1f", currentFps);
        ImGui::Text("Frame Time: %.3f ms/frame", 1000.0f / currentFps);

        static float fpsHistory[120] = {};
        static int fpsHistoryOffset = 0;
        fpsHistory[fpsHistoryOffset] = currentFps;
        fpsHistoryOffset = (fpsHistoryOffset + 1) % 120;

        float minFps = fpsHistory[0];
        float maxFps = fpsHistory[0];
        for (int i = 1; i < 120; ++i)
        {
            if (fpsHistory[i] < minFps) minFps = fpsHistory[i];
            if (fpsHistory[i] > maxFps) maxFps = fpsHistory[i];
        }

        char label[64];
        std::snprintf(label, sizeof(label), "Min: %.1f | Max: %.1f", minFps, maxFps);
        ImGui::PlotLines("##FPSGraph", fpsHistory, 120, fpsHistoryOffset, label, 0.0f, 120.0f, ImVec2(0, 50.0f));

        ImGui::Separator();
        ImGui::Text("--- CPU/GPU Stage Profiler ---");

        // 1. 全ステージの active フラグをリセット
        for (int i = 0; i < kMaxStages; ++i)
        {
            stages_[i].active = false;
        }

        // Helper: 名前からステージのインデックスを取得、無ければ新規登録
        auto getStageIndex = [&](const std::string& name) -> int {
            for (int i = 0; i < kMaxStages; ++i)
            {
                if (stages_[i].name[0] == '\0') // 空きスロット
                {
                    strcpy_s(stages_[i].name, sizeof(stages_[i].name), name.c_str());
                    return i;
                }
                if (std::strcmp(stages_[i].name, name.c_str()) == 0)
                {
                    return i;
                }
            }
            return -1; // 満杯
        };

        // GPU プロファイル結果の回収と履歴の更新
        const auto& gpuResults = GpuProfiler::GetInstance()->GetResults();
        for (const auto& res : gpuResults)
        {
            int idx = getStageIndex(res.name);
            if (idx >= 0)
            {
                stages_[idx].active = true;
                stages_[idx].history[historyOffset_] = res.timeMs;
            }
        }

        // CPU 衝突判定（Collision）の所要時間を追加
        float collisionTime = CollisionManager::GetInstance()->GetLastUpdateDurationMs();
        int colIdx = getStageIndex("Collision (CPU)");
        if (colIdx >= 0)
        {
            stages_[colIdx].active = true;
            stages_[colIdx].history[historyOffset_] = collisionTime;
        }

        // オフセットを進める
        historyOffset_ = (historyOffset_ + 1) % kMaxHistoryFrames;

        // 各ステージの時間数値表示と個別の折れ線グラフ描画
        for (int i = 0; i < kMaxStages; ++i)
        {
            if (stages_[i].name[0] == '\0') continue; // 未使用

            float lastVal = stages_[i].history[historyOffset_ == 0 ? kMaxHistoryFrames - 1 : historyOffset_ - 1];

            ImGui::Text("%s: %.3f ms", stages_[i].name, lastVal);
            
            // グラフ用に最大値を動的計算してスケールを合わせる
            float maxVal = 0.1f;
            for (int j = 0; j < kMaxHistoryFrames; ++j)
            {
                if (stages_[i].history[j] > maxVal) maxVal = stages_[i].history[j];
            }
            maxVal *= 1.2f;

            char graphLabel[64];
            std::snprintf(graphLabel, sizeof(graphLabel), "Max: %.2f ms", maxVal);
            
            std::string imguiId = "##" + std::string(stages_[i].name);
            ImGui::PlotLines(imguiId.c_str(), stages_[i].history, kMaxHistoryFrames, historyOffset_, graphLabel, 0.0f, maxVal, ImVec2(0, 35.0f));
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