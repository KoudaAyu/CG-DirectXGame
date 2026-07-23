#include "LevelEditor.h"
#include <fstream>
#include <sstream>
#include <random>
#include <imgui.h>
#include <iostream>
#include "SceneManager.h"
#include "MaterialManager.h"
#include "Light.h"
#include <Windows.h>
#include "externals/nlohmann/json.hpp"
#include "Baziru3_Engine/3D/Procedural/BioProceduralGenerator.h"
#include "Baziru3_Engine/Collision/CollisionManager.h"
#include "Baziru3_Engine/Collision/BoxCollider.h"
#include "Baziru3_Engine/Collision/CapsuleCollider.h"

namespace {
    bool FileExists(const std::string& path) {
        std::ifstream f(path);
        return f.good();
    }

    std::vector<std::string> SplitCSVLine(const std::string& s, char delimiter) {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(s);
        while (std::getline(tokenStream, token, delimiter)) {
            tokens.push_back(token);
        }
        return tokens;
    }
} // namespace

LevelEditor::~LevelEditor()
{
    CollisionManager* colManager = CollisionManager::GetInstance();
    if (colManager)
    {
        for (auto& collider : colliders_)
        {
            if (collider)
            {
                colManager->UnregisterCollider(collider.get());
            }
        }
    }
    colliders_.clear();
}

void LevelEditor::Initialize(DirectXCom* dxCommon, Object3dCom* object3dCom)
{
    dxCommon_ = dxCommon;
    object3dCom_ = object3dCom;

    // 軸ヘルパー用シリンダーの作成 (X=赤, Y=緑, Z=青)
    MaterialManager* matManager = SceneManager::GetInstance()->GetMaterialManager();
    Light* light = SceneManager::GetInstance()->GetLight();
    Camera* camera = object3dCom_ ? object3dCom_->GetDefaultCamera() : nullptr;

    for (int i = 0; i < 3; ++i)
    {
        axisCylinders_[i] = std::make_unique<Cylinder>();
        // 細長いシリンダー: 半径0.015f, 高さ1.2f
        axisCylinders_[i]->Initialize(dxCommon_, object3dCom_, matManager, light, camera, 16, 0.015f, 0.015f, 1.2f);
        axisCylinders_[i]->SetOverlayDraw(true);
    }
    axisTextureIndex_ = TextureManager::GetInstance()->Load("Resources/uvChecker.png");

    // 軸用のマテリアルリソースを作成・初期化
    Vector4 axisColors[3] = {
        { 1.0f, 0.1f, 0.1f, 1.0f }, // X (Red)
        { 0.1f, 1.0f, 0.1f, 1.0f }, // Y (Green)
        { 0.1f, 0.1f, 1.0f, 1.0f }  // Z (Blue)
    };

    for (int i = 0; i < 3; ++i)
    {
        axisMaterialResources_[i] = dxCommon_->CreateBufferResource(dxCommon_->GetDevice().Get(), sizeof(Material));
        Material* data = nullptr;
        axisMaterialResources_[i]->Map(0, nullptr, reinterpret_cast<void**>(&data));
        if (data)
        {
            data->color = axisColors[i];
            data->enableLighting = 0; // ライティング無効にして鮮明に見せる
            data->specularModel = 0;
            data->shininess = 0.0f;
            data->uvTransform = MakeIdentity4x4();
            data->reflectionFactor = 0.0f;
            data->fresnelF0 = 0.00f;
            std::memset(data->padding2, 0, sizeof(data->padding2));
            axisMaterialResources_[i]->Unmap(0, nullptr);
        }
    }

    // 初期化時にサンプルデータを読み込み試行、なければデフォルト配置
    if (!LoadFromFile(currentFilepath_))
    {
        // ファイル自体が存在しない場合のみ、デフォルトの GroundPlane をメモリ上に追加（勝手に上書きセーブはしない）
        if (!std::filesystem::exists(currentFilepath_))
        {
            AddObject("GroundPlane", "Resources", "plane.obj", true);
        }
    }
}

void LevelEditor::Update(float deltaTime)
{
    // キーボード入力による選択オブジェクトの移動 (ImGuiで文字入力中以外)
    if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(objectDatas_.size()) && !ImGui::GetIO().WantTextInput)
    {
        float speed = (GetAsyncKeyState(VK_SHIFT) & 0x8000) ? 12.0f : 4.0f;
        float moveDist = speed * deltaTime;

        auto& obj = objectDatas_[selectedIndex_];
        if (GetAsyncKeyState(VK_LEFT) & 0x8000)  { obj.position.x -= moveDist; }
        if (GetAsyncKeyState(VK_RIGHT) & 0x8000) { obj.position.x += moveDist; }
        if (GetAsyncKeyState(VK_UP) & 0x8000)    { obj.position.z += moveDist; }
        if (GetAsyncKeyState(VK_DOWN) & 0x8000)  { obj.position.z -= moveDist; }
        if (GetAsyncKeyState(VK_PRIOR) & 0x8000) { obj.position.y += moveDist; } // PageUp
        if (GetAsyncKeyState(VK_NEXT) & 0x8000)  { obj.position.y -= moveDist; } // PageDown
    }

    // UI側のTransform変更を実行時の3Dオブジェクトに同期して更新
    size_t count = std::min(objectDatas_.size(), runtimeObjects_.size());
    for (size_t i = 0; i < count; ++i)
    {
        if (!runtimeObjects_[i]) continue;
        runtimeObjects_[i]->SetTranslate(objectDatas_[i].position);
        runtimeObjects_[i]->SetRotate(objectDatas_[i].rotation);
        runtimeObjects_[i]->SetScale(objectDatas_[i].scale);
        runtimeObjects_[i]->SetDeltaTime(deltaTime);
        runtimeObjects_[i]->Update();
    }

    // 川オブジェクトの更新
    for (auto& river : rivers_)
    {
        if (river) river->Update(deltaTime);
    }

    // 軸ヘルパーシリンダーの座標・回転・スケールを更新
    if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(objectDatas_.size()))
    {
        Vector3 center = objectDatas_[selectedIndex_].position;
        
        // X軸 (赤): ロールを-90度 (-1.5707963f)
        Sprite::Transform tX = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, -1.5707963f}, center };
        axisCylinders_[0]->SetTransform(tX);
        axisCylinders_[0]->Update();

        // Y軸 (緑): 無回転
        Sprite::Transform tY = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, center };
        axisCylinders_[1]->SetTransform(tY);
        axisCylinders_[1]->Update();

        // Z軸 (青): ピッチを90度 (1.5707963f)
        Sprite::Transform tZ = { {1.0f, 1.0f, 1.0f}, {1.5707963f, 0.0f, 0.0f}, center };
        axisCylinders_[2]->SetTransform(tZ);
        axisCylinders_[2]->Update();
    }

    // 自動リロード（ホットリロード）処理
    if (autoReloadEnabled_ && !currentFilepath_.empty())
    {
        static int reloadCheckCooldown = 0;
        // 毎フレームのIO負荷を下げるため、約30フレームに1回チェック
        if (++reloadCheckCooldown >= 30)
        {
            reloadCheckCooldown = 0;
            try
            {
                if (std::filesystem::exists(currentFilepath_))
                {
                    auto lastWrite = std::filesystem::last_write_time(currentFilepath_);
                    if (lastWrite > lastLoadedTime_)
                    {
                        if (currentFilepath_.rfind(".json") != std::string::npos)
                        {
                            LoadFromJson(currentFilepath_);
                        }
                        else
                        {
                            LoadFromFile(currentFilepath_);
                        }
                        lastLoadedTime_ = lastWrite;
                    }
                }
            }
            catch (...)
            {
                // ファイル書き込み中のロック競合などによるエラーの防止
            }
        }
    }
}

void LevelEditor::Draw(const RenderContext& ctx)
{
    if (!object3dCom_) return;

    size_t count = std::min(objectDatas_.size(), runtimeObjects_.size());
    for (size_t i = 0; i < count; ++i)
    {
        auto& obj = runtimeObjects_[i];
        if (!obj) continue;

        const auto& modelData = obj->GetModelData();
        RenderContext localCtx = ctx;

        // Tree の場合はプロシージャルカラー（鮮やかなグリーン）をセット
        MaterialManager* matMgr = SceneManager::GetInstance()->GetMaterialManager();
        if (matMgr)
        {
            if (objectDatas_[i].type == "Tree")
            {
                matMgr->GetMaterialDataColor() = { 0.25f, 0.80f, 0.25f, 1.0f }; // 鮮やかな森林グリーン
            }
            else
            {
                matMgr->GetMaterialDataColor() = { 1.0f, 1.0f, 1.0f, 1.0f }; // 通常カラー
            }
        }

        // モデル個別のテクスチャを指定
        if (modelData.material.textureIndex != TextureManager::kInvalidTextureIndex)
        {
            localCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(modelData.material.textureIndex);
        }
        else
        {
            // D3D12の未初期化デスクリプタアクセスエラーを防ぐため、デフォルトのテクスチャをバインド
            uint32_t dummyTex = TextureManager::GetInstance()->Load("Resources/uvChecker.png");
            localCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(dummyTex);
        }

        object3dCom_->Draw(obj.get(), localCtx, modelData, true);
    }

    // 川(River)の描画
    for (auto& river : rivers_)
    {
        if (river)
        {
            uint32_t texIndex = TextureManager::GetInstance()->Load("Resources/uvChecker.png");
            river->Draw(TextureManager::GetInstance()->GetSrvHandleGPU(texIndex));
        }
    }

    // 選択オブジェクトがあれば3D軸ヘルパー（赤、緑、青のシリンダー）を描画
    if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(objectDatas_.size()))
    {
        ID3D12GraphicsCommandList* cmdList = ctx.commandList;
        if (cmdList)
        {
            D3D12_GPU_DESCRIPTOR_HANDLE texHandle = TextureManager::GetInstance()->GetSrvHandleGPU(axisTextureIndex_);
            if (texHandle.ptr != 0)
            {
                // ルートシグネチャとパイプラインを設定
                cmdList->SetGraphicsRootSignature(object3dCom_->GetRootSignature().Get());
                if (object3dCom_->GetOverlayPipelineState())
                {
                    cmdList->SetPipelineState(object3dCom_->GetOverlayPipelineState().Get());
                }

                // 共通の定数バッファ引数をバインド
                if (ctx.light)
                {
                    cmdList->SetGraphicsRootConstantBufferView(3, ctx.light->GetDirectionalLightResource()->GetGPUVirtualAddress());
                }
                else
                {
                    cmdList->SetGraphicsRootConstantBufferView(3, 0);
                }

                if (ctx.camera && ctx.camera->GetCameraResource())
                {
                    cmdList->SetGraphicsRootConstantBufferView(4, ctx.camera->GetCameraResource()->GetGPUVirtualAddress());
                }
                else
                {
                    cmdList->SetGraphicsRootConstantBufferView(4, 0);
                }

                cmdList->SetGraphicsRootDescriptorTable(2, texHandle);

                for (int i = 0; i < 3; ++i)
                {
                    if (!axisCylinders_[i] || axisCylinders_[i]->GetVertexCount() == 0 || !axisCylinders_[i]->GetTransformationMatrixResource())
                        continue;

                    // 頂点バッファビュー設定
                    cmdList->IASetVertexBuffers(0, 1, &axisCylinders_[i]->GetVertexBufferView());
                    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

                    // マテリアル定数バッファ (ルート引数0) と Transform定数バッファ (ルート引数1) を設定
                    cmdList->SetGraphicsRootConstantBufferView(0, axisMaterialResources_[i]->GetGPUVirtualAddress());
                    cmdList->SetGraphicsRootConstantBufferView(1, axisCylinders_[i]->GetTransformationMatrixResource()->GetGPUVirtualAddress());

                    // 描画実行
                    cmdList->DrawInstanced(axisCylinders_[i]->GetVertexCount(), 1, 0, 0);
                }
            }
        }
    }
}

void LevelEditor::DrawImGui()
{
    // スタイル調整（モダンなダークカラー）
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.2f, 0.1f, 0.4f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.22f, 0.45f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.3f, 0.6f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.2f, 0.5f, 0.8f));

    // ==========================================
    // 1. Level Editor ウィンドウ (File I/O, Hierarchy, Add)
    // ==========================================
    if (ImGui::Begin("Level Editor"))
    {
        // ファイル操作
        ImGui::TextColored(ImVec4(0.7f, 0.6f, 0.9f, 1.0f), "[ File IO ]");
        ImGui::Text("Current File: %s", currentFilepath_.c_str());
        static char filepathBuf[256] = "Resources/stage_layout.json";
        ImGui::InputText("File Path", filepathBuf, sizeof(filepathBuf));
        if (ImGui::Button("Save Level"))
        {
            if (SaveToFile(filepathBuf))
            {
                currentFilepath_ = filepathBuf;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Level"))
        {
            if (LoadFromFile(filepathBuf))
            {
                currentFilepath_ = filepathBuf;
            }
        }
        ImGui::SameLine();
        ImGui::Checkbox("Auto Reload", &autoReloadEnabled_);

        ImGui::Separator();

        // オブジェクト一覧（ヒエラルキー）
        ImGui::TextColored(ImVec4(0.7f, 0.6f, 0.9f, 1.0f), "[ Hierarchy ]");
        if (ImGui::BeginChild("HierarchyList", ImVec2(0, 150), true))
        {
            for (int i = 0; i < static_cast<int>(objectDatas_.size()); ++i)
            {
                char label[128];
                // 一意のIDを確保するため ##%d を末尾に付与
                sprintf_s(label, "%s (%s)##%d", objectDatas_[i].name.c_str(), objectDatas_[i].isStatic ? "Static" : "Dynamic", i);
                if (ImGui::Selectable(label, selectedIndex_ == i))
                {
                    selectedIndex_ = i;
                }
            }
            ImGui::EndChild();
        }

        ImGui::Separator();

        // 新規オブジェクト追加パネル
        ImGui::TextColored(ImVec4(0.7f, 0.6f, 0.9f, 1.0f), "[ Add New Object ]");
        ImGui::InputText("New Name", addName_, sizeof(addName_));
        ImGui::InputText("New Model Dir", addDir_, sizeof(addDir_));
        ImGui::InputText("New Model File", addFile_, sizeof(addFile_));
        ImGui::Checkbox("New Is Static", &addIsStatic_);

        if (ImGui::Button("Add Object"))
        {
            AddObject(addName_, addDir_, addFile_, addIsStatic_);
        }
    }
    ImGui::End();

    // ==========================================
    // 2. Inspector (Parameters) ウィンドウ
    // ==========================================
    if (ImGui::Begin("Inspector"))
    {
        if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(objectDatas_.size()))
        {
            auto& obj = objectDatas_[selectedIndex_];
            ImGui::TextColored(ImVec4(0.7f, 0.6f, 0.9f, 1.0f), "[ Parameters: %s ]", obj.name.c_str());

            char nameBuf[128];
            strcpy_s(nameBuf, obj.name.c_str());
            if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf)))
            {
                obj.name = nameBuf;
            }

            // 既存オブジェクトのモデル情報編集
            char dirBuf[256];
            strcpy_s(dirBuf, obj.modelDirectory.c_str());
            if (ImGui::InputText("Model Dir", dirBuf, sizeof(dirBuf), ImGuiInputTextFlags_EnterReturnsTrue))
            {
                obj.modelDirectory = dirBuf;
                RefreshRuntimeObjects();
            }

            char fileBuf[256];
            strcpy_s(fileBuf, obj.modelFilename.c_str());
            if (ImGui::InputText("Model File", fileBuf, sizeof(fileBuf), ImGuiInputTextFlags_EnterReturnsTrue))
            {
                obj.modelFilename = fileBuf;
                RefreshRuntimeObjects();
            }
            ImGui::TextDisabled("(Press Enter to apply model path changes)");

            std::string fullPath = obj.modelDirectory + "/" + obj.modelFilename;
            if (!FileExists(fullPath))
            {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Warning: Specified file not found!");
            }

            // 操作説明
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Viewport Move Hotkeys:");
            ImGui::Text("- Arrow keys: Move on X/Z plane\n- PageUp/PageDown: Move on Y axis\n- Hold Shift: Move faster\n- Ctrl+Click Drag to type exact value");

            // Transformドラッグ入力 (微調整用)
            ImGui::DragFloat3("Position", &obj.position.x, 0.05f, -1000.0f, 1000.0f);

            // 角度表示をラジアンから度数法(Degrees)に変換してドラッグ操作
            Vector3 rotDegrees = {
                obj.rotation.x * 180.0f / 3.14159265f,
                obj.rotation.y * 180.0f / 3.14159265f,
                obj.rotation.z * 180.0f / 3.14159265f
            };
            if (ImGui::DragFloat3("Rotation", &rotDegrees.x, 0.5f, -360.0f, 360.0f))
            {
                obj.rotation.x = rotDegrees.x * 3.14159265f / 180.0f;
                obj.rotation.y = rotDegrees.y * 3.14159265f / 180.0f;
                obj.rotation.z = rotDegrees.z * 3.14159265f / 180.0f;
            }

            // スケールのドラッグ入力 (微調整用)
            ImGui::DragFloat3("Scale", &obj.scale.x, 0.01f, 0.001f, 1000.0f);

            ImGui::Checkbox("Is Static", &obj.isStatic);

            ImGui::Separator();

            if (ImGui::Button("Duplicate"))
            {
                LevelObjectData dup = obj;
                dup.name += "_Copy";
                objectDatas_.push_back(dup);
                RefreshRuntimeObjects();
                selectedIndex_ = static_cast<int>(objectDatas_.size()) - 1;
            }
            ImGui::SameLine();
            if (ImGui::Button("Delete"))
            {
                objectDatas_.erase(objectDatas_.begin() + selectedIndex_);
                RefreshRuntimeObjects();
                if (!objectDatas_.empty())
                {
                    selectedIndex_ = 0;
                }
                else
                {
                    selectedIndex_ = -1;
                }
            }
        }
        else
        {
            ImGui::TextDisabled("Select an object from Hierarchy to edit parameters.");
        }
    }
    ImGui::End();

    ImGui::PopStyleColor(4);
}

bool LevelEditor::SaveToFile(const std::string& filepath)
{
    if (filepath.rfind(".json") != std::string::npos)
    {
        return SaveToJson(filepath);
    }

    std::ofstream file(filepath);
    if (!file.is_open()) return false;

    // ヘッダー行
    file << "#Name,Directory,Filename,px,py,pz,rx,ry,rz,sx,sy,sz,isStatic\n";

    for (const auto& obj : objectDatas_)
    {
        file << obj.name << ","
             << obj.modelDirectory << ","
             << obj.modelFilename << ","
             << obj.position.x << "," << obj.position.y << "," << obj.position.z << ","
             << obj.rotation.x << "," << obj.rotation.y << "," << obj.rotation.z << ","
             << obj.scale.x << "," << obj.scale.y << "," << obj.scale.z << ","
             << (obj.isStatic ? 1 : 0) << "\n";
    }

    return true;
}

bool LevelEditor::SaveToJson(const std::string& filepath)
{
    nlohmann::json j = nlohmann::json::array();

    for (const auto& obj : objectDatas_)
    {
        nlohmann::json item;
        item["name"] = obj.name;
        item["type"] = obj.type;

        item["position"] = { {"x", obj.position.x}, {"y", obj.position.y}, {"z", obj.position.z} };
        item["rotation"] = { {"x", obj.rotation.x}, {"y", obj.rotation.y}, {"z", obj.rotation.z} };
        item["scale"] = { {"x", obj.scale.x}, {"y", obj.scale.y}, {"z", obj.scale.z} };
        item["isStatic"] = obj.isStatic;

        if (obj.type == "Tree" || obj.type == "Rock")
        {
            nlohmann::json params;
            params["seed"] = obj.seed;
            params["iterations"] = obj.iterations;
            params["branchLength"] = obj.branchLength;
            params["branchRadius"] = obj.branchRadius;
            params["taperRate"] = obj.taperRate;
            params["angle"] = obj.angle;
            params["subdivisions"] = obj.subdivisions;
            params["noiseStrength"] = obj.noiseStrength;
            params["voronoiStrength"] = obj.voronoiStrength;
            params["crackStrength"] = obj.crackStrength;
            item["parameters"] = params;
        }

        j.push_back(item);
    }

    std::ofstream file(filepath);
    if (!file.is_open()) return false;

    file << j.dump(4);
    return true;
}

bool LevelEditor::LoadFromFile(const std::string& filepath)
{
    if (filepath.rfind(".json") != std::string::npos)
    {
        return LoadFromJson(filepath);
    }

    std::ifstream file(filepath);
    if (!file.is_open()) return false;

    std::vector<LevelObjectData> tempDatas;
    std::string line;

    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == '#') continue; // 空行・コメント行スキップ

        auto tokens = SplitCSVLine(line, ',');
        if (tokens.size() < 13) continue; // 不完全な行はスキップ

        LevelObjectData obj;
        obj.name = tokens[0];
        obj.modelDirectory = tokens[1];
        obj.modelFilename = tokens[2];
        obj.position = { std::stof(tokens[3]), std::stof(tokens[4]), std::stof(tokens[5]) };
        obj.rotation = { std::stof(tokens[6]), std::stof(tokens[7]), std::stof(tokens[8]) };
        obj.scale = { std::stof(tokens[9]), std::stof(tokens[10]), std::stof(tokens[11]) };
        obj.isStatic = (std::stoi(tokens[12]) != 0);

        tempDatas.push_back(obj);
    }

    objectDatas_ = std::move(tempDatas);
    RefreshRuntimeObjects();
    if (!objectDatas_.empty())
    {
        selectedIndex_ = 0;
    }
    else
    {
        selectedIndex_ = -1;
    }

    if (std::filesystem::exists(filepath))
    {
        lastLoadedTime_ = std::filesystem::last_write_time(filepath);
    }
    return true;
}

bool LevelEditor::LoadFromJson(const std::string& filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open()) return false;

    nlohmann::json j;
    try
    {
        file >> j;
    }
    catch (...)
    {
        return false;
    }

    std::vector<LevelObjectData> tempDatas;
    std::vector<std::unique_ptr<River>> tempRivers;

    for (const auto& item : j)
    {
        LevelObjectData obj;
        obj.name = item.value("name", "Unnamed");
        obj.type = item.value("type", "");

        if (obj.type == "River" && item.contains("parameters") && item["parameters"].contains("points"))
        {
            // 旧カーブ形式の川オブジェクトのパースと生成
            std::vector<Vector3> points;
            float width = 2.0f;
            float flowSpeed = 1.0f;
            float waveScale = 1.0f;
            Vector4 color = { 0.1f, 0.4f, 0.9f, 0.8f };

            const auto& params = item["parameters"];
            width = params.value("river_width", 2.0f);
            flowSpeed = params.value("river_flow_speed", 1.0f);
            waveScale = params.value("river_wave_scale", 1.0f);

            for (const auto& pt : params["points"])
            {
                points.push_back({ pt.value("x", 0.0f), pt.value("y", 0.0f), pt.value("z", 0.0f) });
            }

            auto river = std::make_unique<River>();
            Camera* camera = object3dCom_ ? object3dCom_->GetDefaultCamera() : nullptr;
            MaterialManager* matManager = SceneManager::GetInstance()->GetMaterialManager();
            Light* light = SceneManager::GetInstance()->GetLight();

            river->Initialize(dxCommon_, object3dCom_, matManager, light, camera, points, width, flowSpeed, waveScale, color);
            tempRivers.push_back(std::move(river));

            // --- 🎉 川のカーブに沿って大岩・小石(ShoreRock)＆沿岸森林(Tree)を高密度プロシージャル構築 ---
            if (points.size() >= 2)
            {
                float halfW = width * 0.5f;
                static std::mt19937 rng(12345);
                static std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

                for (size_t pIdx = 0; pIdx < points.size(); ++pIdx)
                {
                    Vector3 pt = points[pIdx];
                    Vector3 dir = { 0.0f, 0.0f, 1.0f };
                    if (pIdx + 1 < points.size()) dir = { points[pIdx + 1].x - pt.x, 0.0f, points[pIdx + 1].z - pt.z };
                    else if (pIdx > 0) dir = { pt.x - points[pIdx - 1].x, 0.0f, pt.z - points[pIdx - 1].z };

                    float dLen = std::sqrt(dir.x * dir.x + dir.z * dir.z);
                    if (dLen > 0.0001f) { dir.x /= dLen; dir.z /= dLen; }
                    Vector3 side = { -dir.z, 0.0f, dir.x };

                    // 1. 水際の高密度小石・大岩 (Shore Rocks & Boulders: 川の外側陸地にのみ配置)
                    for (float sideSign : {-1.0f, 1.0f})
                    {
                        int rockCount = 1 + static_cast<int>(dist01(rng) * 2.0f);
                        for (int r = 0; r < rockCount; ++r)
                        {
                            float offset = halfW + 0.5f + dist01(rng) * 1.2f; // 川幅の外側+0.5m以上の安全オフセット
                            float randX = dist01(rng) * 0.4f - 0.2f;
                            float randZ = dist01(rng) * 0.4f - 0.2f;

                            LevelObjectData rockObj;
                            rockObj.name = "ShoreRock";
                            rockObj.type = "Rock";
                            rockObj.modelDirectory = "Resources";
                            rockObj.modelFilename = "sphere.obj";
                            rockObj.position = { pt.x + sideSign * side.x * offset + randX, pt.y, pt.z + sideSign * side.z * offset + randZ };
                            
                            bool isBigBoulder = (dist01(rng) < 0.2f);
                            float s = isBigBoulder ? (0.6f + dist01(rng) * 0.4f) : (0.25f + dist01(rng) * 0.3f);
                            rockObj.scale = { s, s * (0.5f + dist01(rng) * 0.5f), s };
                            rockObj.rotation = { dist01(rng) * 3.14f, dist01(rng) * 3.14f, dist01(rng) * 3.14f };
                            rockObj.seed = static_cast<int>(dist01(rng) * 99999);
                            rockObj.subdivisions = 3;
                            rockObj.noiseStrength = 0.3f;
                            rockObj.voronoiStrength = 0.2f;
                            rockObj.crackStrength = 0.25f;

                            tempDatas.push_back(rockObj);
                        }
                    }

                    // 2. 岸辺の自然な水辺の茂み (Shore Grass: 川の境界外側に配置)
                    for (float sideSign : {-1.0f, 1.0f})
                    {
                        if (dist01(rng) < 0.55f)
                        {
                            float offset = halfW + 0.6f + dist01(rng) * 0.8f; // 川の外側+0.6m以上の安全オフセット
                            float randX = dist01(rng) * 0.4f - 0.2f;
                            float randZ = dist01(rng) * 0.4f - 0.2f;

                            LevelObjectData grassObj;
                            grassObj.name = "ShoreReeds";
                            grassObj.type = "Tree";
                            grassObj.modelDirectory = "Resources";
                            grassObj.modelFilename = "tree.obj";
                            grassObj.position = { pt.x + sideSign * side.x * offset + randX, pt.y, pt.z + sideSign * side.z * offset + randZ };
                            
                            float gs = 0.3f + dist01(rng) * 0.25f;
                            grassObj.scale = { gs * 0.7f, gs * 0.65f, gs * 0.7f };
                            grassObj.rotation = { 0.0f, dist01(rng) * 6.28f, 0.0f };
                            grassObj.seed = static_cast<int>(dist01(rng) * 99999);
                            grassObj.iterations = 2;
                            grassObj.branchLength = 0.12f;
                            grassObj.branchRadius = 0.05f;
                            grassObj.taperRate = 0.8f;
                            grassObj.angle = 25.0f;

                            tempDatas.push_back(grassObj);
                        }
                    }

                    // 3. 沿岸の立体森林 (Dense Riverbank Forest: 川から十分離れた陸地)
                    for (float sideSign : {-1.0f, 1.0f})
                    {
                        if (dist01(rng) < 0.8f)
                        {
                            float offset = halfW + 2.5f + dist01(rng) * 4.5f; // 川から2.5m以上十分離す
                            float randX = dist01(rng) * 1.0f - 0.5f;
                            float randZ = dist01(rng) * 1.0f - 0.5f;

                            LevelObjectData treeObj;
                            treeObj.name = "RiverTree";
                            treeObj.type = "Tree";
                            treeObj.modelDirectory = "Resources";
                            treeObj.modelFilename = "tree.obj";
                            treeObj.position = { pt.x + sideSign * side.x * offset + randX, pt.y, pt.z + sideSign * side.z * offset + randZ };
                            
                            float ts = 0.8f + dist01(rng) * 0.7f;
                            treeObj.scale = { ts, ts * (0.9f + dist01(rng) * 0.3f), ts };
                            treeObj.rotation = { 0.0f, dist01(rng) * 6.28f, 0.0f };
                            treeObj.seed = static_cast<int>(dist01(rng) * 99999);
                            treeObj.iterations = 2;
                            treeObj.branchLength = 0.16f;
                            treeObj.branchRadius = 0.07f;
                            treeObj.taperRate = 0.8f;
                            treeObj.angle = 25.0f;

                            tempDatas.push_back(treeObj);
                        }
                    }
                }
            }

            continue;
        }

        // CSV互換のためにデフォルトを設定
        obj.modelDirectory = "Resources";
        if (obj.type == "Tree" || obj.type == "Rock")
        {
            obj.modelFilename = "Outputs/ProceduralAsset_LOD0.obj"; // ダミー
        }
        else
        {
            obj.modelFilename = "plane.obj";
        }

        // position
        if (item.contains("position"))
        {
            const auto& pos = item["position"];
            obj.position = { pos.value("x", 0.0f), pos.value("y", 0.0f), pos.value("z", 0.0f) };
        }
        // rotation
        if (item.contains("rotation"))
        {
            const auto& rot = item["rotation"];
            obj.rotation = { rot.value("x", 0.0f), rot.value("y", 0.0f), rot.value("z", 0.0f) };
        }
        // scale
        if (item.contains("scale"))
        {
            const auto& scl = item["scale"];
            obj.scale = { scl.value("x", 1.0f), scl.value("y", 1.0f), scl.value("z", 1.0f) };
        }

        obj.isStatic = item.value("isStatic", true);

        // parameters
        if (item.contains("parameters"))
        {
            const auto& params = item["parameters"];
            obj.seed = params.value("seed", 0);
            obj.iterations = params.value("iterations", 3);
            obj.branchLength = params.value("branchLength", 1.0f);
            obj.branchRadius = params.value("branchRadius", 0.1f);
            obj.taperRate = params.value("taperRate", 0.8f);
            obj.angle = params.value("angle", 25.0f);
            obj.subdivisions = params.value("subdivisions", 3);
            obj.noiseStrength = params.value("noiseStrength", 0.3f);
            obj.voronoiStrength = params.value("voronoiStrength", 0.2f);
            obj.crackStrength = params.value("crackStrength", 0.4f);
            obj.biomeZoneType = params.value("biome_zone_type", "");
        }

        tempDatas.push_back(obj);
    }

    objectDatas_ = std::move(tempDatas);
    rivers_ = std::move(tempRivers);
    RefreshRuntimeObjects();

    if (!objectDatas_.empty())
    {
        selectedIndex_ = 0;
    }
    else
    {
        selectedIndex_ = -1;
    }

    if (std::filesystem::exists(filepath))
    {
        lastLoadedTime_ = std::filesystem::last_write_time(filepath);
    }
    return true;
}

void LevelEditor::RefreshRuntimeObjects()
{
    // 古いコライダーの登録解除とクリア
    CollisionManager* colManager = CollisionManager::GetInstance();
    if (colManager)
    {
        for (auto& collider : colliders_)
        {
            if (collider)
            {
                colManager->UnregisterCollider(collider.get());
            }
        }
    }
    colliders_.clear();

    runtimeObjects_.clear();

    for (size_t idx = 0; idx < objectDatas_.size(); ++idx)
    {
        const auto& objData = objectDatas_[idx];
        std::string cacheKey;
        bool isProcedural = (objData.type == "Tree" || objData.type == "Rock");
        bool isBiome = (objData.type == "Biome");
        bool isSpawnOrLoot = (objData.type == "PlayerSpawn" || objData.type == "EnemySpawn" || objData.type == "LootBox");

        if (isProcedural)
        {
            if (objData.type == "Tree")
            {
                std::ostringstream oss;
                oss << "ProcTree_" << objData.seed << "_" << objData.iterations << "_"
                    << objData.branchLength << "_" << objData.branchRadius << "_"
                    << objData.taperRate << "_" << objData.angle;
                cacheKey = oss.str();
            }
            else
            {
                std::ostringstream oss;
                oss << "ProcRock_" << objData.seed << "_" << objData.subdivisions << "_"
                    << objData.noiseStrength << "_" << objData.voronoiStrength << "_"
                    << objData.crackStrength;
                cacheKey = oss.str();
            }
        }
        else if (isSpawnOrLoot)
        {
            cacheKey = "SpawnLoot_" + objData.type;
        }
        else if (isBiome)
        {
            cacheKey = "Biome_" + objData.biomeZoneType;
        }
        else
        {
            cacheKey = objData.modelDirectory + "/" + objData.modelFilename;
        }

        // キャッシュからロード、存在しなければ新規作成
        auto it = modelCache_.find(cacheKey);
        if (it == modelCache_.end())
        {
            Object3d::ModelData modelData;

            if (isProcedural)
            {
                BioProcedural::MeshData procMesh;
                if (objData.type == "Tree")
                {
                    BioProcedural::TreeParameters params;
                    params.seed = objData.seed;
                    params.iterations = objData.iterations;
                    params.branchLength = objData.branchLength;
                    params.branchRadius = objData.branchRadius;
                    params.taperRate = objData.taperRate;
                    params.angle = objData.angle;
                    params.radialSegments = 8;
                    
                    procMesh = BioProcedural::BioProceduralGenerator::GenerateTree(params);
                }
                else
                {
                    BioProcedural::RockParameters params;
                    params.seed = objData.seed;
                    params.subdivisions = objData.subdivisions;
                    params.noiseStrength = objData.noiseStrength;
                    params.voronoiStrength = objData.voronoiStrength;
                    params.crackStrength = objData.crackStrength;
                    
                    procMesh = BioProcedural::BioProceduralGenerator::GenerateRock(params);
                }

                modelData.vertices.resize(procMesh.vertices.size());
                for (size_t i = 0; i < procMesh.vertices.size(); ++i)
                {
                    modelData.vertices[i].position = { procMesh.vertices[i].position.x, procMesh.vertices[i].position.y, procMesh.vertices[i].position.z, 1.0f };
                    modelData.vertices[i].normal = { procMesh.vertices[i].normal.x, procMesh.vertices[i].normal.y, procMesh.vertices[i].normal.z };
                    modelData.vertices[i].texcoord = { procMesh.vertices[i].texcoord.u, procMesh.vertices[i].texcoord.v };
                }
                modelData.indices = procMesh.indices;

                // プロシージャル樹木は無地白テクスチャを割り当て
                if (objData.type == "Tree")
                {
                    std::string whiteTex = "Resources/CG4/human/white.png";
                    if (!FileExists(whiteTex))
                    {
                        whiteTex = "Resources/uvChecker.png";
                    }
                    modelData.material.textureFilePath = whiteTex;
                    modelData.material.textureIndex = TextureManager::GetInstance()->Load(whiteTex);
                }
                else
                {
                    std::string texturePath = "Resources/Outputs/ProceduralAsset_LOD0_atlas.tga";
                    if (!FileExists(texturePath))
                    {
                        texturePath = "Resources/uvChecker.png";
                    }
                    modelData.material.textureFilePath = texturePath;
                    modelData.material.textureIndex = TextureManager::GetInstance()->Load(texturePath);
                }
            }
            else if (isSpawnOrLoot)
            {
                BioProcedural::MeshData customMesh;
                if (objData.type == "LootBox")
                {
                    customMesh = BioProcedural::BioProceduralGenerator::GenerateBoxShape();
                }
                else
                {
                    customMesh = BioProcedural::BioProceduralGenerator::GenerateHumanShape();
                }

                modelData.vertices.resize(customMesh.vertices.size());
                for (size_t i = 0; i < customMesh.vertices.size(); ++i)
                {
                    modelData.vertices[i].position = { customMesh.vertices[i].position.x, customMesh.vertices[i].position.y, customMesh.vertices[i].position.z, 1.0f };
                    modelData.vertices[i].normal = { customMesh.vertices[i].normal.x, customMesh.vertices[i].normal.y, customMesh.vertices[i].normal.z };
                    modelData.vertices[i].texcoord = { customMesh.vertices[i].texcoord.u, customMesh.vertices[i].texcoord.v };
                }
                modelData.indices = customMesh.indices;

                std::string whiteTex = "Resources/CG4/human/white.png";
                if (!FileExists(whiteTex)) whiteTex = "Resources/uvChecker.png";
                modelData.material.textureFilePath = whiteTex;
                modelData.material.textureIndex = TextureManager::GetInstance()->Load(whiteTex);
            }
            else if (isBiome)
            {
                // バイオーム境界板
                std::string fullPath = "Resources/plane.obj";
                if (FileExists(fullPath))
                {
                    modelData = Object3d::LoadObjFile("Resources", "plane.obj");
                    modelData.material.textureIndex = TextureManager::kInvalidTextureIndex;
                    modelData.material.textureFilePath = "";
                }
            }
            else
            {
                std::string fullPath = objData.modelDirectory + "/" + objData.modelFilename;
                if (!FileExists(fullPath))
                {
                    runtimeObjects_.push_back(nullptr);
                    continue;
                }
                if (objData.modelFilename.rfind(".obj") != std::string::npos)
                {
                    modelData = Object3d::LoadObjFile(objData.modelDirectory, objData.modelFilename);
                }
                else
                {
                    modelData = Object3d::LoadModelFile(objData.modelDirectory, objData.modelFilename);
                }
            }

            modelCache_[cacheKey] = modelData;
            it = modelCache_.find(cacheKey);
        }

        auto runtimeObj = std::make_unique<Object3d>();
        runtimeObj->Initialize(object3dCom_, it->second);
        runtimeObj->SetTranslate(objData.position);

        if (isBiome)
        {
            // Blenderの水平面(Z-upのXY平面)をDirectXの水平面(Y-upのXZ平面)に合わせるため、X軸で90度(1.57079ラジアン)回転させる
            Vector3 rotated = objData.rotation;
            rotated.x += 1.57079f;
            runtimeObj->SetRotate(rotated);
        }
        else
        {
            runtimeObj->SetRotate(objData.rotation);
        }

        runtimeObj->SetScale(objData.scale);
        runtimeObj->SetEnableLighting(true);

        // タイプに応じた確定マテリアルカラーを設定（白化を100%防止）
        if (objData.type == "Tree")
        {
            runtimeObj->SetColor({ 0.25f, 0.85f, 0.30f, 1.0f }); // 鮮やかな森林グリーン
        }
        else if (objData.type == "Rock")
        {
            runtimeObj->SetColor({ 0.55f, 0.55f, 0.55f, 1.0f }); // ナチュラルな岩グレー
        }
        else if (objData.type == "PlayerSpawn")
        {
            runtimeObj->SetColor({ 0.20f, 0.60f, 1.00f, 1.0f }); // 青色
        }
        else if (objData.type == "EnemySpawn")
        {
            runtimeObj->SetColor({ 1.00f, 0.25f, 0.25f, 1.0f }); // 赤色
        }
        else if (objData.type == "LootBox")
        {
            runtimeObj->SetColor({ 1.00f, 0.80f, 0.20f, 1.0f }); // 金色
        }

        if (isBiome)
        {
            Vector4 color = { 0.5f, 0.5f, 0.5f, 0.4f }; // デフォルト (グレー半透明)
            if (objData.biomeZoneType == "Forest")          { color = { 0.1f, 0.7f, 0.1f, 0.4f }; }
            else if (objData.biomeZoneType == "Desert")     { color = { 0.7f, 0.6f, 0.2f, 0.4f }; }
            else if (objData.biomeZoneType == "River")      { color = { 0.1f, 0.4f, 0.8f, 0.4f }; }
            else if (objData.biomeZoneType == "Grassland")  { color = { 0.5f, 0.8f, 0.1f, 0.4f }; }
            runtimeObj->SetColor(color);
            runtimeObj->SetEnableLighting(false); // ライトを無効にしてゾーンカラーをクリアに描画
        }

        runtimeObj->Update();

        runtimeObjects_.push_back(std::move(runtimeObj));

        // プロシージャルオブジェクトならコライダーも生成して登録する
        if (isProcedural && colManager)
        {
            std::unique_ptr<Collider> collider;
            if (objData.type == "Tree")
            {
                float radius = objData.branchRadius * objData.scale.x;
                float height = objData.branchLength * objData.scale.y * 3.0f; // 樹木の高さに比例
                // vectorがリサイズされたり再配置されても安全なよう、&objData.position のアドレスは、RefreshRuntimeObjects が終わるまでは有効
                collider = std::make_unique<CapsuleCollider>(radius, height, &const_cast<Vector3&>(objData.position), CollisionAttribute::Obstacle);
            }
            else // Rock
            {
                Vector3 size = { objData.scale.x * 2.0f, objData.scale.y * 2.0f, objData.scale.z * 2.0f };
                collider = std::make_unique<BoxCollider>(size, &const_cast<Vector3&>(objData.position), nullptr, CollisionAttribute::Obstacle);
            }

            if (collider)
            {
                colManager->RegisterCollider(collider.get());
                colliders_.push_back(std::move(collider));
            }
        }
    }
}

void LevelEditor::AddObject(const std::string& name, const std::string& dir, const std::string& file, bool isStatic)
{
    LevelObjectData obj;
    obj.name = name;
    obj.modelDirectory = dir;
    obj.modelFilename = file;
    // 重なりやZ-fightingを防ぐため、初期位置を少し上に浮かせます
    obj.position = { 0.0f, 1.5f, 0.0f };
    obj.rotation = { 0.0f, 0.0f, 0.0f };
    obj.scale = { 1.0f, 1.0f, 1.0f };
    obj.isStatic = isStatic;

    objectDatas_.push_back(obj);
    RefreshRuntimeObjects();
    selectedIndex_ = static_cast<int>(objectDatas_.size()) - 1;
}
