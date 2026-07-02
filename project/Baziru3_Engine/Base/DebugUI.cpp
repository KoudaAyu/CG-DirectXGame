#include "DebugUI.h"
#include "MaterialManager.h"
#include "SpriteManager.h"
#include "Camera.h"
#include "../3D/Procedural/ProceduralGenerator.h"
#include "../3D/Object/Object3dCom.h"
#include <imgui.h>

DebugUI::DebugUI(MaterialManager* materialManager, SpriteManager* spriteManager, Camera* camera,
    Sprite::Transform* transformObject, bool* useMonsterBall, bool* drawObject, bool* drawSprite,
    Object3d* object3d)
    : materialManager_(materialManager), spriteManager_(spriteManager), camera_(camera), transformObject_(transformObject), useMonsterBall_(useMonsterBall), drawObject_(drawObject), drawSprite_(drawSprite), targetObject3d_(object3d)
{
}

void DebugUI::Initialize()
{
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
        
        // 樹木はCPUでポリゴン構造ごと再構成されるため、操作の快適性を最優先し、ドラッグを終えて「手を離した瞬間」に再生成する設計にします
        ImGui::SliderInt("Iterations (Depth)", &treeParams.iterations, 1, 5);
        if (ImGui::IsItemDeactivatedAfterEdit()) needRegen = true;

        ImGui::SliderFloat("Branch Length", &treeParams.branchLength, 0.1f, 5.0f);
        if (ImGui::IsItemDeactivatedAfterEdit()) needRegen = true;

        ImGui::SliderFloat("Branch Radius", &treeParams.branchRadius, 0.01f, 0.5f);
        if (ImGui::IsItemDeactivatedAfterEdit()) needRegen = true;

        ImGui::SliderFloat("Taper Rate", &treeParams.taperRate, 0.5f, 0.95f);
        if (ImGui::IsItemDeactivatedAfterEdit()) needRegen = true;

        ImGui::SliderFloat("Branch Angle", &treeParams.angle, 5.0f, 90.0f);
        if (ImGui::IsItemDeactivatedAfterEdit()) needRegen = true;
        
        int seedVal = (int)treeParams.seed;
        ImGui::DragInt("Seed", &seedVal, 1, 0, 999999);
        if (ImGui::IsItemDeactivatedAfterEdit())
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

    static int prevSubdivisions = -1;

    if (needRegen || (proceduralMode != 0 && prevMode == -1))
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
                newModelData = ProceduralGenerator::GenerateTree(treeParams);
                newModelData.material = originalModelData.material;
                targetObject3d_->UpdateModelData(newModelData);
            }
        }
    }

    ImGui::End();
#endif
}

void DebugUI::Finalize()
{
}