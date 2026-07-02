#include "DebugUI.h"
#include "MaterialManager.h"
#include "SpriteManager.h"
#include "Camera.h"
#include "../3D/Procedural/ProceduralGenerator.h"
#include "../3D/Object/Object3dCom.h"
#include "SceneManager.h"
#include "OffScreenRendering.h"
#include "ParticleManager.h"
#include "Application/Scene/GameScene/GamePlayScene.h"
#include <imgui.h>

DebugUI::DebugUI(MaterialManager* materialManager, SpriteManager* spriteManager, Camera* camera,
    Sprite::Transform* transformObject, bool* useMonsterBall, bool* drawObject, bool* drawSprite,
    Object3d* object3d)
    : materialManager_(materialManager), spriteManager_(spriteManager), camera_(camera), transformObject_(transformObject), useMonsterBall_(useMonsterBall), drawObject_(drawObject), drawSprite_(drawSprite), targetObject3d_(object3d)
{
}

void DebugUI::Initialize()
{
    // 樹木パラメータの木らしい初期値
    treeParams.iterations = 4;
    treeParams.branchLength = 1.0f;
    treeParams.branchRadius = 0.05f;
    treeParams.taperRate = 0.8f;
    treeParams.angle = 25.0f;
    treeParams.axiom = "X";
    treeParams.seed = 12345;

    // 岩石パラメータの初期値
    rockParams.scale = 1.0f;
    rockParams.subdivisions = 4;
    rockParams.noiseStrength = 0.3f;
    rockParams.noiseFrequency = 2.0f;
    rockParams.octaves = 3;
    rockParams.voronoiStrength = 0.2f;
    rockParams.voronoiCells = 10;
    rockParams.crackStrength = 0.1f;
    rockParams.crackFrequency = 4.0f;
    rockParams.seed = 42;

    // 初回に元のオブジェクトデータを保存して退避しておく
    if (targetObject3d_)
    {
        originalModelData = targetObject3d_->GetModelData();
        isOriginalModelDataSaved = true;
    }
}

void DebugUI::Update()
{
#ifdef USE_IMGUI
    // ImGui::ShowDemoWindow(); // 画面が散らかるためデフォルトでは非表示にします

    ImGui::Begin("Windows");

    if (materialManager_)
    {
        Vector4 col = materialManager_->GetMaterialDataColor();
        if (ImGui::ColorEdit4("Material Color", &col.x))
        {
            materialManager_->GetMaterialDataColor() = col;
            if (targetObject3d_)
            {
                targetObject3d_->SetColor(col);
            }
        }
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
        ImGui::PlotLines("##FPSGraph", fpsHistory, 120, fpsHistoryOffset, label, 0.0f, 120.0f, ImVec2(0, 80.0f));
    }
    ImGui::End();

    // --- プロシージャル生成エディタウィンドウ ---
    ImGui::Begin("Procedural Generator (BIO-AUTHORING STUDIO)");

    const char* modes[] = { "Normal (OBJ)", "Rock (Noise & Voronoi)", "Tree (L-System)" };
    int prevMode = proceduralMode;
    if (ImGui::Combo("Generation Mode", &proceduralMode, modes, IM_ARRAYSIZE(modes)))
    {
        if (proceduralMode == 0 && prevMode != 0 && isOriginalModelDataSaved && targetObject3d_)
        {
            // 通常モデル（元のデータ）に戻す
            targetObject3d_->UpdateModelData(originalModelData);
        }
        else if (proceduralMode != prevMode)
        {
            // モードが切り替わったら初期生成トリガーを引く
            prevMode = -1; 
            if (proceduralMode == 1) // Rock
            {
                if (materialManager_)
                {
                    materialManager_->GetMaterialDataColor() = { 0.6f, 0.6f, 0.6f, 1.0f };
                }
                if (targetObject3d_)
                {
                    targetObject3d_->SetColor({ 0.6f, 0.6f, 0.6f, 1.0f });
                }
            }
            else if (proceduralMode == 2) // Tree
            {
                if (materialManager_)
                {
                    materialManager_->GetMaterialDataColor() = { 0.55f, 0.35f, 0.17f, 1.0f };
                }
                if (targetObject3d_)
                {
                    targetObject3d_->SetColor({ 0.55f, 0.35f, 0.17f, 1.0f });
                }
                // 木らしい比率と分岐に自動リセット
                treeParams.iterations = 4;
                treeParams.branchLength = 1.0f;
                treeParams.branchRadius = 0.05f;
                treeParams.taperRate = 0.8f;
                treeParams.angle = 25.0f;
                treeParams.axiom = "X";
                treeParams.seed = 12345;
            }
        }
    }

    bool needRegen = false;

    if (proceduralMode == 1) // Rock
    {
        ImGui::Separator();
        ImGui::Text("Rock Parameters");
        if (ImGui::DragFloat("Scale", &rockParams.scale, 0.05f, 0.1f, 5.0f)) needRegen = true;
        if (ImGui::SliderInt("Subdivisions", &rockParams.subdivisions, 1, 12)) needRegen = true;
        if (ImGui::SliderFloat("Noise Strength", &rockParams.noiseStrength, 0.0f, 2.0f)) needRegen = true;
        if (ImGui::SliderFloat("Noise Frequency", &rockParams.noiseFrequency, 0.1f, 10.0f)) needRegen = true;
        if (ImGui::SliderInt("Noise Octaves", &rockParams.octaves, 1, 6)) needRegen = true;
        if (ImGui::SliderFloat("Voronoi Strength", &rockParams.voronoiStrength, 0.0f, 2.0f)) needRegen = true;
        if (ImGui::SliderInt("Voronoi Cells", &rockParams.voronoiCells, 2, 50)) needRegen = true;
        if (ImGui::SliderFloat("Crack Strength", &rockParams.crackStrength, 0.0f, 2.0f)) needRegen = true;
        if (ImGui::SliderFloat("Crack Frequency", &rockParams.crackFrequency, 0.5f, 10.0f)) needRegen = true;
        
        int seedVal = (int)rockParams.seed;
        if (ImGui::DragInt("Seed", &seedVal, 1, 0, 999999))
        {
            rockParams.seed = (unsigned int)seedVal;
            needRegen = true;
        }
    }
    else if (proceduralMode == 2) // Tree
    {
        ImGui::Separator();
        ImGui::Text("L-System Tree Parameters");
        
        // GPU化により、ドラッグ中もリアルタイムに超高速変形できるようになりました！
        if (ImGui::SliderInt("Iterations (Depth)", &treeParams.iterations, 1, 5)) needRegen = true;
        if (ImGui::SliderFloat("Branch Length", &treeParams.branchLength, 0.1f, 5.0f)) needRegen = true;
        if (ImGui::SliderFloat("Branch Radius", &treeParams.branchRadius, 0.01f, 0.5f)) needRegen = true;
        if (ImGui::SliderFloat("Taper Rate", &treeParams.taperRate, 0.5f, 0.95f)) needRegen = true;
        if (ImGui::SliderFloat("Branch Angle", &treeParams.angle, 5.0f, 90.0f)) needRegen = true;
        
        int seedVal = (int)treeParams.seed;
        if (ImGui::DragInt("Seed", &seedVal, 1, 0, 999999))
        {
            treeParams.seed = (unsigned int)seedVal;
            needRegen = true;
        }
    }

    if (proceduralMode != 0)
    {
        ImGui::Separator();
        ImGui::Text("Export Asset (Standalone OBJ)");
        ImGui::InputText("Export Name", exportFileName, IM_ARRAYSIZE(exportFileName));
        
        if (ImGui::Button("Export to OBJ"))
        {
            if (targetObject3d_)
            {
                exportResult = ProceduralGenerator::ExportToObj("Resources/Outputs", exportFileName, targetObject3d_->GetModelData());
                hasExported = true;
            }
            else
            {
                exportResult.success = false;
                exportResult.outputMessage = "Error: No target model data.";
                hasExported = true;
            }
        }

        if (hasExported)
        {
            if (exportResult.success)
            {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s", exportResult.outputMessage.c_str());
                
                // 統計情報 (就活ポートフォリオ用アピール) の描画
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "--- Geometry & Vertex Color Stats ---");
                ImGui::Indent(10.0f);
                ImGui::Text("Total Vertices: %u", exportResult.totalVertices);
                ImGui::Text("Total Polygons: %u", exportResult.totalIndices / 3);
                
                // 苔ウェイトの可視化統計
                ImGui::Text("Moss-affected Vertices (Red > 0.1): %u (%.1f%%)", 
                            exportResult.mossVertices, exportResult.mossRatio * 100.0f);
                ImGui::ProgressBar(exportResult.mossRatio, ImVec2(250.0f, 15.0f), "Moss Blending Area");
                
                // 風ウェイトの可視化統計
                ImGui::Text("Wind-sway Vertices (Green > 0.1): %u (%.1f%%)", 
                            exportResult.windVertices, exportResult.windRatio * 100.0f);
                ImGui::ProgressBar(exportResult.windRatio, ImVec2(250.0f, 15.0f), "Wind Animation Area");
                ImGui::Unindent(10.0f);
            }
            else
            {
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", exportResult.outputMessage.c_str());
            }
        }
    }

    // GPUジェネレーターの初期化
    if (!isGpuGeneratorInitialized && targetObject3d_ && targetObject3d_->GetObject3dCom())
    {
        DirectXCom* dxCom = targetObject3d_->GetObject3dCom()->GetDirectXCom();
        if (dxCom)
        {
            if (gpuGenerator.Initialize(dxCom))
            {
                isGpuGeneratorInitialized = true;
            }
        }
    }

    // GPU樹木ジェネレーターの初期化
    if (!isGpuTreeGeneratorInitialized && targetObject3d_ && targetObject3d_->GetObject3dCom())
    {
        DirectXCom* dxCom = targetObject3d_->GetObject3dCom()->GetDirectXCom();
        if (dxCom)
        {
            if (gpuTreeGenerator.Initialize(dxCom))
            {
                isGpuTreeGeneratorInitialized = true;
            }
        }
    }

    static int prevSubdivisions = -1;

    bool shouldDispatchTree = (proceduralMode == 2 && isGpuTreeGeneratorInitialized);
    if (needRegen || shouldDispatchTree || (proceduralMode != 0 && prevMode == -1))
    {
        if (targetObject3d_)
        {
            Object3d::ModelData newModelData;
            if (proceduralMode == 1) // Rock
            {
                if (isGpuGeneratorInitialized)
                {
                    // 分割数(Subdivisions)が変わった、または初回のみベースメッシュを再転送する
                    if (rockParams.subdivisions != prevSubdivisions || prevMode == -1)
                    {
                        // 綺麗な球体（変形パラメータ 0.0）を生成
                        ProceduralGenerator::RockParameters baseParams = rockParams;
                        baseParams.noiseStrength = 0.0f;
                        baseParams.voronoiStrength = 0.0f;
                        baseParams.crackStrength = 0.0f;
                        
                        Object3d::ModelData baseMesh = ProceduralGenerator::GenerateRock(baseParams);
                        
                        // BioProcedural::Vertex構造体に詰め替え
                        std::vector<BioProcedural::Vertex> bVertices(baseMesh.vertices.size());
                        for (size_t i = 0; i < baseMesh.vertices.size(); ++i)
                        {
                            bVertices[i].position = { baseMesh.vertices[i].position.x, baseMesh.vertices[i].position.y, baseMesh.vertices[i].position.z };
                            bVertices[i].normal = { baseMesh.vertices[i].normal.x, baseMesh.vertices[i].normal.y, baseMesh.vertices[i].normal.z };
                            bVertices[i].texcoord = { baseMesh.vertices[i].texcoord.x, baseMesh.vertices[i].texcoord.y };
                            bVertices[i].color = { 0.0f, 0.0f, 0.0f, 1.0f };
                        }
                        
                        gpuGenerator.SetBaseMesh(bVertices);
                        
                        // インデックスや基本情報を更新 (頂点バッファのアロケーションもここで行う)
                        baseMesh.material = originalModelData.material;
                        targetObject3d_->UpdateModelData(baseMesh);
                        
                        prevSubdivisions = rockParams.subdivisions;
                    }
                    
                    // GPU側で変形計算を実行
                    BioProcedural::RockParameters bParams;
                    bParams.scale = rockParams.scale;
                    bParams.subdivisions = rockParams.subdivisions;
                    bParams.noiseStrength = rockParams.noiseStrength;
                    bParams.noiseFrequency = rockParams.noiseFrequency;
                    bParams.octaves = rockParams.octaves;
                    bParams.voronoiStrength = rockParams.voronoiStrength;
                    bParams.voronoiCells = rockParams.voronoiCells;
                    bParams.crackStrength = rockParams.crackStrength;
                    bParams.crackFrequency = rockParams.crackFrequency;
                    bParams.seed = rockParams.seed;
                    
                    gpuGenerator.Dispatch(bParams, (uint32_t)targetObject3d_->GetModelData().vertices.size());
                    
                    // 描画用の頂点バッファビューをオーバーライド
                    targetObject3d_->OverrideVertexBufferView(gpuGenerator.GetVertexBufferView());
                }
                else
                {
                    // フォールバック (CPU版)
                    newModelData = ProceduralGenerator::GenerateRock(rockParams);
                    newModelData.material = originalModelData.material;
                    targetObject3d_->UpdateModelData(newModelData);
                }
            }
            else if (proceduralMode == 2) // Tree
            {
                if (isGpuTreeGeneratorInitialized)
                {
                    static int prevTreeIterations = -1;
                    static float prevTreeLength = -1.0f;
                    static float prevTreeRadius = -1.0f;
                    static float prevTreeTaper = -1.0f;
                    static float prevTreeAngle = -1.0f;
                    static unsigned int prevTreeSeed = 0;

                    bool skeletonChanged = (treeParams.iterations != prevTreeIterations ||
                                            treeParams.branchLength != prevTreeLength ||
                                            treeParams.branchRadius != prevTreeRadius ||
                                            treeParams.taperRate != prevTreeTaper ||
                                            treeParams.angle != prevTreeAngle ||
                                            treeParams.seed != prevTreeSeed ||
                                            prevMode == -1 ||
                                            needRegen);

                    static uint32_t currentSegments = 0;

                    if (skeletonChanged)
                    {
                        BioProcedural::TreeParameters bTreeParams;
                        bTreeParams.iterations = treeParams.iterations;
                        bTreeParams.branchLength = treeParams.branchLength;
                        bTreeParams.branchRadius = treeParams.branchRadius;
                        bTreeParams.taperRate = treeParams.taperRate;
                        bTreeParams.angle = treeParams.angle;
                        bTreeParams.axiom = treeParams.axiom;
                        bTreeParams.seed = treeParams.seed;

                        // 骨格のみ生成（極めて軽量）
                        std::vector<BioProcedural::GPUBranchesSegment> skeleton = BioProcedural::BioProceduralGenerator::GenerateTreeSkeleton(bTreeParams);
                        currentSegments = (uint32_t)skeleton.size();
                        
                        // GPUの構造化バッファへ転送
                        gpuTreeGenerator.SetSkeletonData(skeleton);

                        // 描画用のダミーメッシュ構造（最大サイズ）を設定する (初回のみ)
                        static bool isTreeDummyMeshCreated = false;
                        if (!isTreeDummyMeshCreated || prevMode == -1)
                        {
                            Object3d::ModelData dummyMesh;
                            
                            // 頂点領域を最大サイズでアロケート
                            dummyMesh.vertices.resize(ProceduralTreeGPUGenerator::kMaxVertices);
                            for (auto& v : dummyMesh.vertices)
                            {
                                v.position.w = 1.0f;
                            }
                            
                            // インデックスバッファの構築 (枝用と葉用)
                            uint32_t maxSegments = ProceduralTreeGPUGenerator::kMaxSegments;
                            dummyMesh.indices.reserve(maxSegments * 48 + maxSegments * 96);
                            
                            // 枝のインデックス
                            for (uint32_t segIdx = 0; segIdx < maxSegments; ++segIdx)
                            {
                                uint32_t baseV = segIdx * 18;
                                int radialSegments = 8;
                                for (int i = 0; i < radialSegments; ++i)
                                {
                                    uint32_t i0 = baseV + i;
                                    uint32_t i1 = baseV + 9 + i;
                                    uint32_t i2 = baseV + i + 1;
                                    uint32_t i3 = baseV + 9 + i + 1;

                                    dummyMesh.indices.push_back(i0);
                                    dummyMesh.indices.push_back(i1);
                                    dummyMesh.indices.push_back(i2);

                                    dummyMesh.indices.push_back(i2);
                                    dummyMesh.indices.push_back(i1);
                                    dummyMesh.indices.push_back(i3);
                                }
                            }
                            
                            // 葉のインデックス
                            uint32_t leafVertexStart = maxSegments * 18;
                            for (uint32_t segIdx = 0; segIdx < maxSegments; ++segIdx)
                            {
                                uint32_t baseV = leafVertexStart + segIdx * 32;
                                for (uint32_t leafIdx = 0; leafIdx < 4; ++leafIdx)
                                {
                                    uint32_t baseLeafV = baseV + leafIdx * 8;
                                    
                                    // Crossed Quad 1 (表裏)
                                    dummyMesh.indices.push_back(baseLeafV + 0);
                                    dummyMesh.indices.push_back(baseLeafV + 1);
                                    dummyMesh.indices.push_back(baseLeafV + 2);
                                    dummyMesh.indices.push_back(baseLeafV + 0);
                                    dummyMesh.indices.push_back(baseLeafV + 2);
                                    dummyMesh.indices.push_back(baseLeafV + 3);
                                    
                                    dummyMesh.indices.push_back(baseLeafV + 0);
                                    dummyMesh.indices.push_back(baseLeafV + 2);
                                    dummyMesh.indices.push_back(baseLeafV + 1);
                                    dummyMesh.indices.push_back(baseLeafV + 0);
                                    dummyMesh.indices.push_back(baseLeafV + 3);
                                    dummyMesh.indices.push_back(baseLeafV + 2);

                                    // Crossed Quad 2 (表裏)
                                    dummyMesh.indices.push_back(baseLeafV + 4);
                                    dummyMesh.indices.push_back(baseLeafV + 5);
                                    dummyMesh.indices.push_back(baseLeafV + 6);
                                    dummyMesh.indices.push_back(baseLeafV + 4);
                                    dummyMesh.indices.push_back(baseLeafV + 6);
                                    dummyMesh.indices.push_back(baseLeafV + 7);
                                    
                                    dummyMesh.indices.push_back(baseLeafV + 4);
                                    dummyMesh.indices.push_back(baseLeafV + 6);
                                    dummyMesh.indices.push_back(baseLeafV + 5);
                                    dummyMesh.indices.push_back(baseLeafV + 4);
                                    dummyMesh.indices.push_back(baseLeafV + 7);
                                    dummyMesh.indices.push_back(baseLeafV + 6);
                                }
                            }
                            
                            dummyMesh.material = originalModelData.material;
                            dummyMesh.material.textureFilePath = "Resources/checkerBoard.png";
                            targetObject3d_->UpdateModelData(dummyMesh);
                            targetObject3d_->SetColor({ 0.55f, 0.35f, 0.17f, 1.0f });
                            isTreeDummyMeshCreated = true;
                        }
                        
                        prevTreeIterations = treeParams.iterations;
                        prevTreeLength = treeParams.branchLength;
                        prevTreeRadius = treeParams.branchRadius;
                        prevTreeTaper = treeParams.taperRate;
                        prevTreeAngle = treeParams.angle;
                        prevTreeSeed = treeParams.seed;
                    }

                    // 2. GPUで円柱展開・葉ポリゴン並列構築と揺れアニメーションを実行
                    static float treeTime = 0.0f;
                    treeTime += 0.016f;
                    
                    Vector3 windDir = { 1.0f, 0.0f, 0.3f };
                    float windStrength = 0.6f;

                    gpuTreeGenerator.Dispatch(windDir, windStrength, treeTime, currentSegments);

                    // 3. 描画用バッファビューのオーバーライド
                    targetObject3d_->OverrideVertexBufferView(gpuTreeGenerator.GetVertexBufferView());
                }
                else
                {
                    // フォールバック (CPU版)
                    newModelData = ProceduralGenerator::GenerateTree(treeParams);
                    newModelData.material = originalModelData.material;
                    targetObject3d_->UpdateModelData(newModelData);
                }
            }
        }
    }

    ImGui::End();
#endif
}

void DebugUI::Finalize()
{
}