#include "LevelEditor.h"
#include "Application/Config/AppDebugConfig.h"
#include "Application/Config/GameConfig.h"
#include "Application/UI/TacticalUICanvas.h"
#include "Baziru3_Engine/3D/Procedural/BioProceduralGenerator.h"
#include "Baziru3_Engine/Framework/Collision/BoxCollider.h"
#include "Baziru3_Engine/Framework/Collision/CapsuleCollider.h"
#include "Baziru3_Engine/Framework/Collision/CollisionManager.h"
#include "Baziru3_Engine/Framework/IO/JsonSafeLoader.h"
#include "Light.h"
#include "MaterialManager.h"
#include "SceneManager.h"
#include "externals/nlohmann/json.hpp"

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <iostream>
#include <random>
#include <sstream>

namespace {
    constexpr float kPi       = GameConfig::Math::kPi;
    constexpr float kTwoPi    = GameConfig::Math::kTwoPi;
    constexpr float kHalfPi   = GameConfig::Math::kHalfPi;
    constexpr float kDegToRad = GameConfig::Math::kDegToRad;
    constexpr float kRadToDeg = GameConfig::Math::kRadToDeg;

    // 3D軸ギズモ・移動・ホットリロード定数
    constexpr size_t   kAxisCount                 = 3;
    constexpr uint32_t kAxisCylinderSlices        = 16;
    constexpr float    kAxisCylinderRadius        = 0.015f;
    constexpr float    kAxisCylinderHeight        = 1.2f;
    constexpr float    kFastViewportMoveSpeed     = 12.0f;
    constexpr float    kNormalViewportMoveSpeed   = 4.0f;
    constexpr int      kReloadCheckIntervalFrames = 30;
    constexpr float    kAddedObjectDefaultPosY    = 1.5f;

    constexpr Vector4 kAxisColors[kAxisCount] = {
        { 1.0f, 0.1f, 0.1f, 1.0f }, // X (Red)
        { 0.1f, 1.0f, 0.1f, 1.0f }, // Y (Green)
        { 0.1f, 0.1f, 1.0f, 1.0f }, // Z (Blue)
    };

    constexpr Vector3 kAxisRotations[kAxisCount] = {
        { 0.0f,    0.0f, -kHalfPi }, // X軸
        { 0.0f,    0.0f,  0.0f    }, // Y軸
        { kHalfPi, 0.0f,  0.0f    }, // Z軸
    };

    // 描画マテリアルカラー・タイプ別カラーテーブル
    constexpr Vector4 kTreeDrawMaterialColor{ 0.25f, 0.80f, 0.25f, 1.0f };
    constexpr Vector4 kDefaultDrawMaterialColor{ 1.0f, 1.0f, 1.0f, 1.0f };

    struct TypeColorSpec {
        const char* typeName;
        Vector4     color;
    };

    constexpr TypeColorSpec kObjectTypeColors[] = {
        { "Tree",        { 0.25f, 0.85f, 0.30f, 1.0f } },
        { "Rock",        { 0.55f, 0.55f, 0.55f, 1.0f } },
        { "PlayerSpawn", { 0.20f, 0.60f, 1.00f, 1.0f } },
        { "EnemySpawn",  { 1.00f, 0.25f, 0.25f, 1.0f } },
        { "LootBox",     { 1.00f, 0.80f, 0.20f, 1.0f } },
    };

    constexpr Vector4 kDefaultBiomeZoneColor{ 0.5f, 0.5f, 0.5f, 0.4f };
    constexpr TypeColorSpec kBiomeZoneColors[] = {
        { "Forest",    { 0.1f, 0.7f, 0.1f, 0.4f } },
        { "Desert",    { 0.7f, 0.6f, 0.2f, 0.4f } },
        { "River",     { 0.1f, 0.4f, 0.8f, 0.4f } },
        { "Grassland", { 0.5f, 0.8f, 0.1f, 0.4f } },
    };

    // ImGui UI定数
    constexpr int   kPushedStyleColorCount = 4;
    constexpr float kHierarchyListHeight   = 150.0f;
    constexpr size_t kPathBufferSize       = 256;
    constexpr size_t kLabelBufferSize      = 128;
    constexpr size_t kPresetNameBufSize    = 64;
    constexpr float kDragPosSpeed          = 0.05f;
    constexpr float kDragPosLimit          = 1000.0f;
    constexpr float kDragRotSpeed          = 0.5f;
    constexpr float kDragRotLimit          = 360.0f;
    constexpr float kDragScaleSpeed        = 0.01f;
    constexpr float kDragScaleMin          = 0.001f;
    constexpr float kDragScaleMax          = 1000.0f;

    constexpr ImVec4 kUiColTitleBgActive{ 0.2f,  0.1f,  0.4f,  1.0f };
    constexpr ImVec4 kUiColButton{ 0.26f, 0.22f, 0.45f, 1.0f };
    constexpr ImVec4 kUiColButtonHovered{ 0.35f, 0.3f,  0.6f,  1.0f };
    constexpr ImVec4 kUiColHeader{ 0.3f,  0.2f,  0.5f,  0.8f };
    constexpr ImVec4 kUiColSectionHeader{ 0.7f,  0.6f,  0.9f,  1.0f };
    constexpr ImVec4 kUiColEnemySection{ 1.0f,  0.4f,  0.4f,  1.0f };
    constexpr ImVec4 kUiColWarningText{ 1.0f,  0.3f,  0.3f,  1.0f };
    constexpr ImVec4 kUiColHotkeyHeader{ 0.4f,  0.8f,  0.4f,  1.0f };

    // CSV / JSON 入出力定数
    constexpr int    kJsonIndentSpaces   = 4;
    constexpr size_t kCsvMinColumnCount  = 13;
    constexpr size_t kCsvColName         = 0;
    constexpr size_t kCsvColDir          = 1;
    constexpr size_t kCsvColFile         = 2;
    constexpr size_t kCsvColPosX         = 3;
    constexpr size_t kCsvColPosY         = 4;
    constexpr size_t kCsvColPosZ         = 5;
    constexpr size_t kCsvColRotX         = 6;
    constexpr size_t kCsvColRotY         = 7;
    constexpr size_t kCsvColRotZ         = 8;
    constexpr size_t kCsvColScaleX       = 9;
    constexpr size_t kCsvColScaleY       = 10;
    constexpr size_t kCsvColScaleZ       = 11;
    constexpr size_t kCsvColIsStatic     = 12;

    constexpr int   kJsonDefaultIterations   = 3;
    constexpr float kJsonDefaultBranchLength = 1.0f;
    constexpr float kJsonDefaultBranchRadius = 0.1f;
    constexpr float kJsonDefaultTaperRate    = 0.8f;
    constexpr float kJsonDefaultAngle        = 25.0f;
    constexpr int   kJsonDefaultSubdivisions = 3;
    constexpr float kJsonDefaultNoise        = 0.3f;
    constexpr float kJsonDefaultVoronoi      = 0.2f;
    constexpr float kJsonDefaultCrack        = 0.4f;

    // 河川・沿岸プロシージャル生成定数
    constexpr float    kDefaultRiverWidth     = 2.0f;
    constexpr float    kDefaultRiverFlowSpeed = 1.0f;
    constexpr float    kDefaultRiverWaveScale = 1.0f;
    constexpr Vector4  kDefaultRiverColor{ 0.1f, 0.4f, 0.9f, 0.8f };
    constexpr uint32_t kRiverProceduralSeed   = 12345u;
    constexpr float    kDirNormalizeEps       = 0.0001f;
    constexpr float    kMaxRandomSeedScale    = 99999.0f;

    constexpr float kShorePosJitterSpan    = 0.4f;
    constexpr float kShorePosJitterHalf    = 0.2f;
    constexpr float kRockBaseOffset        = 0.5f;
    constexpr float kRockOffsetSpan        = 1.2f;
    constexpr float kBigBoulderProbability = 0.2f;
    constexpr float kBigBoulderMinScale    = 0.6f;
    constexpr float kBigBoulderScaleSpan   = 0.4f;
    constexpr float kSmallRockMinScale     = 0.25f;
    constexpr float kSmallRockScaleSpan    = 0.3f;
    constexpr int   kShoreRockSubdivisions = 3;
    constexpr float kShoreRockNoise        = 0.3f;
    constexpr float kShoreRockVoronoi      = 0.2f;
    constexpr float kShoreRockCrack        = 0.25f;

    constexpr float kReedSpawnProbability = 0.55f;
    constexpr float kReedBaseOffset       = 0.6f;
    constexpr float kReedOffsetSpan       = 0.8f;
    constexpr float kReedMinScale         = 0.3f;
    constexpr float kReedScaleSpan        = 0.25f;
    constexpr float kReedScaleXZRatio     = 0.7f;
    constexpr float kReedScaleYRatio      = 0.65f;
    constexpr int   kReedIterations       = 2;
    constexpr float kReedBranchLength     = 0.12f;
    constexpr float kReedBranchRadius     = 0.05f;
    constexpr float kReedTaperRate        = 0.8f;
    constexpr float kReedAngle            = 25.0f;

    constexpr float kBankTreeSpawnProbability = 0.8f;
    constexpr float kBankTreeBaseOffset       = 2.5f;
    constexpr float kBankTreeOffsetSpan       = 4.5f;
    constexpr float kBankTreeMinScale         = 0.8f;
    constexpr float kBankTreeScaleSpan        = 0.7f;
    constexpr float kBankTreeScaleYBase       = 0.9f;
    constexpr float kBankTreeScaleYSpan       = 0.3f;
    constexpr int   kBankTreeIterations       = 2;
    constexpr float kBankTreeBranchLength     = 0.16f;
    constexpr float kBankTreeBranchRadius     = 0.07f;
    constexpr float kBankTreeTaperRate        = 0.8f;
    constexpr float kBankTreeAngle            = 25.0f;

    constexpr int   kTreeRadialSegments      = 8;
    constexpr float kTreeColliderHeightScale = 3.0f;

    constexpr const char* kCheckerTexPath     = "Resources/uvChecker.png";
    constexpr const char* kWhiteTexPath       = "Resources/CG4/human/white.png";
    constexpr const char* kRockAtlasTexPath   = "Resources/Outputs/ProceduralAsset_LOD0_atlas.tga";
    constexpr const char* kEnemySpawnObjFile  = "Enemy_Spawn_Point.obj";

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

    void UnregisterAndClearColliders(std::vector<std::unique_ptr<Collider>>& colliders) {
        if (CollisionManager* colManager = CollisionManager::GetInstance()) {
            for (auto& collider : colliders) {
                if (collider) {
                    colManager->UnregisterCollider(collider.get());
                }
            }
        }
        colliders.clear();
    }

    void CopyProceduralMeshToModelData(const BioProcedural::MeshData& src, Object3d::ModelData& dst) {
        dst.vertices.resize(src.vertices.size());
        for (size_t i = 0; i < src.vertices.size(); ++i) {
            const auto& sv = src.vertices[i];
            dst.vertices[i].position = { sv.position.x, sv.position.y, sv.position.z, 1.0f };
            dst.vertices[i].normal   = { sv.normal.x, sv.normal.y, sv.normal.z };
            dst.vertices[i].texcoord = { sv.texcoord.u, sv.texcoord.v };
        }
        dst.indices = src.indices;
    }

    void AssignTextureWithFallback(const char* preferredPath, Object3d::ModelData& modelData) {
        const std::string resolved = FileExists(preferredPath) ? preferredPath : kCheckerTexPath;
        modelData.material.textureFilePath = resolved;
        modelData.material.textureIndex = TextureManager::GetInstance()->Load(resolved);
    }

    int ResolveSelectedAfterLoad(size_t objectCount) {
        return (objectCount > 0) ? 0 : -1;
    }
} // namespace

LevelEditor::~LevelEditor()
{
    UnregisterAndClearColliders(colliders_);
}

void LevelEditor::Initialize(DirectXCom* dxCommon, Object3dCom* object3dCom)
{
    dxCommon_ = dxCommon;
    object3dCom_ = object3dCom;

    MaterialManager* matManager = SceneManager::GetInstance()->GetMaterialManager();
    Light* light = SceneManager::GetInstance()->GetLight();
    Camera* camera = object3dCom_ ? object3dCom_->GetDefaultCamera() : nullptr;

    for (size_t i = 0; i < kAxisCount; ++i)
    {
        axisCylinders_[i] = std::make_unique<Cylinder>();
        axisCylinders_[i]->Initialize(
            dxCommon_, object3dCom_, matManager, light, camera,
            kAxisCylinderSlices, kAxisCylinderRadius, kAxisCylinderRadius, kAxisCylinderHeight);
        axisCylinders_[i]->SetOverlayDraw(true);

        axisMaterialResources_[i] = dxCommon_->CreateBufferResource(dxCommon_->GetDevice().Get(), sizeof(Material));
        Material* data = nullptr;
        axisMaterialResources_[i]->Map(0, nullptr, reinterpret_cast<void**>(&data));
        if (data)
        {
            data->color = kAxisColors[i];
            data->enableLighting = 0;
            data->specularModel = 0;
            data->shininess = 0.0f;
            data->uvTransform = MakeIdentity4x4();
            data->reflectionFactor = 0.0f;
            data->fresnelF0 = 0.0f;
            std::memset(data->padding2, 0, sizeof(data->padding2));
            axisMaterialResources_[i]->Unmap(0, nullptr);
        }
    }
    axisTextureIndex_ = TextureManager::GetInstance()->Load(kCheckerTexPath);

    if (!LoadFromFile(currentFilepath_) && !std::filesystem::exists(currentFilepath_))
    {
        AddObject("GroundPlane", "Resources", "plane.obj", true);
    }
}

void LevelEditor::Update(float deltaTime)
{
    if (AppDebugState::IsDeveloperImGuiVisible() &&
        selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(objectDatas_.size()) &&
        !ImGui::GetIO().WantTextInput && IsAppWindowFocused())
    {
        const float speed = IsAppKeyDown(VK_SHIFT) ? kFastViewportMoveSpeed : kNormalViewportMoveSpeed;
        const float moveDist = speed * deltaTime;

        auto& obj = objectDatas_[selectedIndex_];
        if (IsAppKeyDown(VK_LEFT))  { obj.position.x -= moveDist; }
        if (IsAppKeyDown(VK_RIGHT)) { obj.position.x += moveDist; }
        if (IsAppKeyDown(VK_UP))    { obj.position.z += moveDist; }
        if (IsAppKeyDown(VK_DOWN))  { obj.position.z -= moveDist; }
        if (IsAppKeyDown(VK_PRIOR)) { obj.position.y += moveDist; }
        if (IsAppKeyDown(VK_NEXT))  { obj.position.y -= moveDist; }
    }

    const size_t count = (std::min)(objectDatas_.size(), runtimeObjects_.size());
    for (size_t i = 0; i < count; ++i)
    {
        if (!runtimeObjects_[i]) continue;
        runtimeObjects_[i]->SetTranslate(objectDatas_[i].position);
        runtimeObjects_[i]->SetRotate(objectDatas_[i].rotation);
        runtimeObjects_[i]->SetScale(objectDatas_[i].scale);
        runtimeObjects_[i]->SetDeltaTime(deltaTime);
        runtimeObjects_[i]->Update();
    }

    for (auto& river : rivers_)
    {
        if (river) river->Update(deltaTime);
    }

    if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(objectDatas_.size()))
    {
        const Vector3 center = objectDatas_[selectedIndex_].position;
        for (size_t i = 0; i < kAxisCount; ++i)
        {
            const Sprite::Transform axisTransform{ { 1.0f, 1.0f, 1.0f }, kAxisRotations[i], center };
            axisCylinders_[i]->SetTransform(axisTransform);
            axisCylinders_[i]->Update();
        }
    }

    if (autoReloadEnabled_ && !currentFilepath_.empty())
    {
        static int reloadCheckCooldown = 0;
        if (++reloadCheckCooldown >= kReloadCheckIntervalFrames)
        {
            reloadCheckCooldown = 0;
            try
            {
                if (std::filesystem::exists(currentFilepath_))
                {
                    const auto lastWrite = std::filesystem::last_write_time(currentFilepath_);
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
            }
        }
    }
}

void LevelEditor::Draw(const RenderContext& ctx)
{
    if (!object3dCom_) return;

    MaterialManager* matMgr = SceneManager::GetInstance()->GetMaterialManager();
    TextureManager* texMgr = TextureManager::GetInstance();
    const uint32_t dummyTex = texMgr->Load(kCheckerTexPath);
    const D3D12_GPU_DESCRIPTOR_HANDLE fallbackSrv = texMgr->GetSrvHandleGPU(dummyTex);

    const size_t count = (std::min)(objectDatas_.size(), runtimeObjects_.size());
    for (size_t i = 0; i < count; ++i)
    {
        auto& obj = runtimeObjects_[i];
        if (!obj) continue;

        const auto& modelData = obj->GetModelData();
        RenderContext localCtx = ctx;

        if (matMgr)
        {
            matMgr->GetMaterialDataColor() = (objectDatas_[i].type == "Tree")
                ? kTreeDrawMaterialColor
                : kDefaultDrawMaterialColor;
        }

        localCtx.textureHandle = (modelData.material.textureIndex != TextureManager::kInvalidTextureIndex)
            ? texMgr->GetSrvHandleGPU(modelData.material.textureIndex)
            : fallbackSrv;

        object3dCom_->Draw(obj.get(), localCtx, modelData, true);
    }

    for (auto& river : rivers_)
    {
        if (river)
        {
            river->Draw(fallbackSrv);
        }
    }
}

void LevelEditor::DrawImGui()
{
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, kUiColTitleBgActive);
    ImGui::PushStyleColor(ImGuiCol_Button,        kUiColButton);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kUiColButtonHovered);
    ImGui::PushStyleColor(ImGuiCol_Header,        kUiColHeader);

    if (ImGui::Begin("Level Editor"))
    {
        ImGui::TextColored(kUiColSectionHeader, "[ File IO ]");
        ImGui::Text("Current File: %s", currentFilepath_.c_str());
        static char filepathBuf[kPathBufferSize] = "Resources/stage_layout.json";
        ImGui::InputText("File Path", filepathBuf, sizeof(filepathBuf));
        if (ImGui::Button("Save Level") && SaveToFile(filepathBuf))
        {
            currentFilepath_ = filepathBuf;
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Level") && LoadFromFile(filepathBuf))
        {
            currentFilepath_ = filepathBuf;
        }
        ImGui::SameLine();
        ImGui::Checkbox("Auto Reload", &autoReloadEnabled_);

        ImGui::Separator();

        ImGui::TextColored(kUiColSectionHeader, "[ Hierarchy ]");
        if (ImGui::BeginChild("HierarchyList", ImVec2(0.0f, kHierarchyListHeight), true))
        {
            for (int i = 0; i < static_cast<int>(objectDatas_.size()); ++i)
            {
                char label[kLabelBufferSize];
                sprintf_s(label, "%s (%s)##%d",
                    objectDatas_[i].name.c_str(),
                    objectDatas_[i].isStatic ? "Static" : "Dynamic", i);
                if (ImGui::Selectable(label, selectedIndex_ == i))
                {
                    selectedIndex_ = i;
                }
            }
            ImGui::EndChild();
        }

        ImGui::Separator();

        ImGui::TextColored(kUiColSectionHeader, "[ Add New Object ]");
        ImGui::InputText("New Name", addName_, sizeof(addName_));
        ImGui::InputText("New Model Dir", addDir_, sizeof(addDir_));
        ImGui::InputText("New Model File", addFile_, sizeof(addFile_));
        ImGui::Checkbox("New Is Static", &addIsStatic_);
        if (ImGui::Button("Add Object"))
        {
            AddObject(addName_, addDir_, addFile_, addIsStatic_);
        }

        ImGui::Spacing();
        ImGui::TextColored(kUiColEnemySection, "[ Enemy Spawner Presets ]");

        struct EnemyPresetSpec {
            const char* buttonLabel;
            const char* nameFormat;
            const char* spawnType;
            bool        sameLineBefore;
        };
        static constexpr EnemyPresetSpec kEnemyPresets[] = {
            { "+ Sentry Enemy",    "Enemy_Sentry_%d",    "EnemySpawn",           false },
            { "+ Patrol Enemy",    "Enemy_Patrol_%d",    "EnemySpawn_Patrol",    true  },
            { "+ Reinforce Spawn", "Enemy_Reinforce_%d", "EnemySpawn_Reinforce", true  },
        };
        static int presetCounters[std::size(kEnemyPresets)] = { 1, 1, 1 };

        for (size_t p = 0; p < std::size(kEnemyPresets); ++p)
        {
            if (kEnemyPresets[p].sameLineBefore) ImGui::SameLine();
            if (ImGui::Button(kEnemyPresets[p].buttonLabel))
            {
                char spawnerName[kPresetNameBufSize];
                sprintf_s(spawnerName, kEnemyPresets[p].nameFormat, presetCounters[p]++);
                AddObject(spawnerName, "Resources", kEnemySpawnObjFile, false);
                if (!objectDatas_.empty())
                {
                    objectDatas_.back().type = kEnemyPresets[p].spawnType;
                }
            }
        }
    }
    ImGui::End();

    if (ImGui::Begin("Inspector"))
    {
        if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(objectDatas_.size()))
        {
            auto& obj = objectDatas_[selectedIndex_];
            ImGui::TextColored(kUiColSectionHeader, "[ Parameters: %s ]", obj.name.c_str());

            char nameBuf[kLabelBufferSize];
            strcpy_s(nameBuf, obj.name.c_str());
            if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) { obj.name = nameBuf; }

            char typeBuf[kLabelBufferSize];
            strcpy_s(typeBuf, obj.type.c_str());
            if (ImGui::InputText("Type", typeBuf, sizeof(typeBuf))) { obj.type = typeBuf; }

            char dirBuf[kPathBufferSize];
            strcpy_s(dirBuf, obj.modelDirectory.c_str());
            if (ImGui::InputText("Model Dir", dirBuf, sizeof(dirBuf), ImGuiInputTextFlags_EnterReturnsTrue))
            {
                obj.modelDirectory = dirBuf;
                RefreshRuntimeObjects();
            }

            char fileBuf[kPathBufferSize];
            strcpy_s(fileBuf, obj.modelFilename.c_str());
            if (ImGui::InputText("Model File", fileBuf, sizeof(fileBuf), ImGuiInputTextFlags_EnterReturnsTrue))
            {
                obj.modelFilename = fileBuf;
                RefreshRuntimeObjects();
            }
            ImGui::TextDisabled("(Press Enter to apply model path changes)");

            if (!FileExists(obj.modelDirectory + "/" + obj.modelFilename))
            {
                ImGui::TextColored(kUiColWarningText, "Warning: Specified file not found!");
            }

            ImGui::TextColored(kUiColHotkeyHeader, "Viewport Move Hotkeys:");
            ImGui::Text("- Arrow keys: Move on X/Z plane\n- PageUp/PageDown: Move on Y axis\n- Hold Shift: Move faster\n- Ctrl+Click Drag to type exact value");

            ImGui::DragFloat3("Position", &obj.position.x, kDragPosSpeed, -kDragPosLimit, kDragPosLimit);

            Vector3 rotDegrees = obj.rotation * kRadToDeg;
            if (ImGui::DragFloat3("Rotation", &rotDegrees.x, kDragRotSpeed, -kDragRotLimit, kDragRotLimit))
            {
                obj.rotation = rotDegrees * kDegToRad;
            }

            ImGui::DragFloat3("Scale", &obj.scale.x, kDragScaleSpeed, kDragScaleMin, kDragScaleMax);
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
                selectedIndex_ = ResolveSelectedAfterLoad(objectDatas_.size());
            }
        }
        else
        {
            ImGui::TextDisabled("Select an object from Hierarchy to edit parameters.");
        }
    }
    ImGui::End();

    ImGui::PopStyleColor(kPushedStyleColorCount);
}

bool LevelEditor::SaveToFile(const std::string& filepath)
{
    if (filepath.rfind(".json") != std::string::npos)
    {
        return SaveToJson(filepath);
    }

    std::ofstream file(filepath);
    if (!file.is_open()) return false;

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
        item["position"] = { { "x", obj.position.x }, { "y", obj.position.y }, { "z", obj.position.z } };
        item["rotation"] = { { "x", obj.rotation.x }, { "y", obj.rotation.y }, { "z", obj.rotation.z } };
        item["scale"]    = { { "x", obj.scale.x },    { "y", obj.scale.y },    { "z", obj.scale.z } };
        item["isStatic"] = obj.isStatic;

        if (obj.type == "Tree" || obj.type == "Rock")
        {
            item["parameters"] = {
                { "seed",            obj.seed },
                { "iterations",      obj.iterations },
                { "branchLength",    obj.branchLength },
                { "branchRadius",    obj.branchRadius },
                { "taperRate",       obj.taperRate },
                { "angle",           obj.angle },
                { "subdivisions",    obj.subdivisions },
                { "noiseStrength",   obj.noiseStrength },
                { "voronoiStrength", obj.voronoiStrength },
                { "crackStrength",   obj.crackStrength },
            };
        }
        j.push_back(item);
    }

    if (!BaziruEngine::IO::JsonSafeLoader::SaveSafe(filepath, j, kJsonIndentSpaces))
    {
        OutputDebugStringA(("[LevelEditor] Failed to save JSON: " + filepath + "\n").c_str());
        return false;
    }
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
        if (line.empty() || line[0] == '#') continue;

        const auto tokens = SplitCSVLine(line, ',');
        if (tokens.size() < kCsvMinColumnCount) continue;

        LevelObjectData obj;
        obj.name           = tokens[kCsvColName];
        obj.modelDirectory = tokens[kCsvColDir];
        obj.modelFilename  = tokens[kCsvColFile];
        obj.position       = { std::stof(tokens[kCsvColPosX]),   std::stof(tokens[kCsvColPosY]),   std::stof(tokens[kCsvColPosZ]) };
        obj.rotation       = { std::stof(tokens[kCsvColRotX]),   std::stof(tokens[kCsvColRotY]),   std::stof(tokens[kCsvColRotZ]) };
        obj.scale          = { std::stof(tokens[kCsvColScaleX]), std::stof(tokens[kCsvColScaleY]), std::stof(tokens[kCsvColScaleZ]) };
        obj.isStatic       = (std::stoi(tokens[kCsvColIsStatic]) != 0);
        tempDatas.push_back(obj);
    }

    objectDatas_ = std::move(tempDatas);
    RefreshRuntimeObjects();
    selectedIndex_ = ResolveSelectedAfterLoad(objectDatas_.size());

    if (std::filesystem::exists(filepath))
    {
        lastLoadedTime_ = std::filesystem::last_write_time(filepath);
    }
    return true;
}

bool LevelEditor::LoadFromJson(const std::string& filepath)
{
    nlohmann::json j;
    if (!BaziruEngine::IO::JsonSafeLoader::LoadSafe(filepath, j))
    {
        OutputDebugStringA(("[LevelEditor] Failed to load JSON (corrupted or missing): " + filepath + "\n").c_str());
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
            const auto& params = item["parameters"];
            const float width     = params.value("river_width", kDefaultRiverWidth);
            const float flowSpeed = params.value("river_flow_speed", kDefaultRiverFlowSpeed);
            const float waveScale = params.value("river_wave_scale", kDefaultRiverWaveScale);

            std::vector<Vector3> points;
            for (const auto& pt : params["points"])
            {
                points.push_back({ pt.value("x", 0.0f), pt.value("y", 0.0f), pt.value("z", 0.0f) });
            }

            auto river = std::make_unique<River>();
            Camera* camera = object3dCom_ ? object3dCom_->GetDefaultCamera() : nullptr;
            MaterialManager* matManager = SceneManager::GetInstance()->GetMaterialManager();
            Light* light = SceneManager::GetInstance()->GetLight();
            river->Initialize(dxCommon_, object3dCom_, matManager, light, camera, points, width, flowSpeed, waveScale, kDefaultRiverColor);
            tempRivers.push_back(std::move(river));

            if (points.size() >= 2)
            {
                const float halfW = width * 0.5f;
                static std::mt19937 rng(kRiverProceduralSeed);
                static std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

                for (size_t pIdx = 0; pIdx < points.size(); ++pIdx)
                {
                    const Vector3 pt = points[pIdx];
                    Vector3 dir = { 0.0f, 0.0f, 1.0f };
                    if (pIdx + 1 < points.size())
                    {
                        dir = { points[pIdx + 1].x - pt.x, 0.0f, points[pIdx + 1].z - pt.z };
                    }
                    else if (pIdx > 0)
                    {
                        dir = { pt.x - points[pIdx - 1].x, 0.0f, pt.z - points[pIdx - 1].z };
                    }

                    const float dLen = std::sqrt(dir.x * dir.x + dir.z * dir.z);
                    if (dLen > kDirNormalizeEps)
                    {
                        dir.x /= dLen;
                        dir.z /= dLen;
                    }
                    const Vector3 side = { -dir.z, 0.0f, dir.x };

                    // 1. 水際の小石・大岩
                    for (float sideSign : { -1.0f, 1.0f })
                    {
                        const int rockCount = 1 + static_cast<int>(dist01(rng) * 2.0f);
                        for (int r = 0; r < rockCount; ++r)
                        {
                            const float offset = halfW + kRockBaseOffset + dist01(rng) * kRockOffsetSpan;
                            const float randX  = dist01(rng) * kShorePosJitterSpan - kShorePosJitterHalf;
                            const float randZ  = dist01(rng) * kShorePosJitterSpan - kShorePosJitterHalf;

                            LevelObjectData rockObj;
                            rockObj.name           = "ShoreRock";
                            rockObj.type           = "Rock";
                            rockObj.modelDirectory = "Resources";
                            rockObj.modelFilename  = "sphere.obj";
                            rockObj.position       = { pt.x + sideSign * side.x * offset + randX, pt.y, pt.z + sideSign * side.z * offset + randZ };

                            const bool isBigBoulder = (dist01(rng) < kBigBoulderProbability);
                            const float s = isBigBoulder
                                ? (kBigBoulderMinScale + dist01(rng) * kBigBoulderScaleSpan)
                                : (kSmallRockMinScale + dist01(rng) * kSmallRockScaleSpan);
                            rockObj.scale           = { s, s * (0.5f + dist01(rng) * 0.5f), s };
                            rockObj.rotation        = { dist01(rng) * kPi, dist01(rng) * kPi, dist01(rng) * kPi };
                            rockObj.seed            = static_cast<int>(dist01(rng) * kMaxRandomSeedScale);
                            rockObj.subdivisions    = kShoreRockSubdivisions;
                            rockObj.noiseStrength   = kShoreRockNoise;
                            rockObj.voronoiStrength = kShoreRockVoronoi;
                            rockObj.crackStrength   = kShoreRockCrack;
                            tempDatas.push_back(rockObj);
                        }
                    }

                    // 2. 岸辺の水辺の茂み
                    for (float sideSign : { -1.0f, 1.0f })
                    {
                        if (dist01(rng) < kReedSpawnProbability)
                        {
                            const float offset = halfW + kReedBaseOffset + dist01(rng) * kReedOffsetSpan;
                            const float randX  = dist01(rng) * kShorePosJitterSpan - kShorePosJitterHalf;
                            const float randZ  = dist01(rng) * kShorePosJitterSpan - kShorePosJitterHalf;

                            LevelObjectData grassObj;
                            grassObj.name           = "ShoreReeds";
                            grassObj.type           = "Tree";
                            grassObj.modelDirectory = "Resources";
                            grassObj.modelFilename  = "tree.obj";
                            grassObj.position       = { pt.x + sideSign * side.x * offset + randX, pt.y, pt.z + sideSign * side.z * offset + randZ };

                            const float gs = kReedMinScale + dist01(rng) * kReedScaleSpan;
                            grassObj.scale        = { gs * kReedScaleXZRatio, gs * kReedScaleYRatio, gs * kReedScaleXZRatio };
                            grassObj.rotation     = { 0.0f, dist01(rng) * kTwoPi, 0.0f };
                            grassObj.seed         = static_cast<int>(dist01(rng) * kMaxRandomSeedScale);
                            grassObj.iterations   = kReedIterations;
                            grassObj.branchLength = kReedBranchLength;
                            grassObj.branchRadius = kReedBranchRadius;
                            grassObj.taperRate    = kReedTaperRate;
                            grassObj.angle        = kReedAngle;
                            tempDatas.push_back(grassObj);
                        }
                    }

                    // 3. 沿岸の立体森林
                    for (float sideSign : { -1.0f, 1.0f })
                    {
                        if (dist01(rng) < kBankTreeSpawnProbability)
                        {
                            const float offset = halfW + kBankTreeBaseOffset + dist01(rng) * kBankTreeOffsetSpan;
                            const float randX  = dist01(rng) - 0.5f;
                            const float randZ  = dist01(rng) - 0.5f;

                            LevelObjectData treeObj;
                            treeObj.name           = "RiverTree";
                            treeObj.type           = "Tree";
                            treeObj.modelDirectory = "Resources";
                            treeObj.modelFilename  = "tree.obj";
                            treeObj.position       = { pt.x + sideSign * side.x * offset + randX, pt.y, pt.z + sideSign * side.z * offset + randZ };

                            const float ts = kBankTreeMinScale + dist01(rng) * kBankTreeScaleSpan;
                            treeObj.scale        = { ts, ts * (kBankTreeScaleYBase + dist01(rng) * kBankTreeScaleYSpan), ts };
                            treeObj.rotation     = { 0.0f, dist01(rng) * kTwoPi, 0.0f };
                            treeObj.seed         = static_cast<int>(dist01(rng) * kMaxRandomSeedScale);
                            treeObj.iterations   = kBankTreeIterations;
                            treeObj.branchLength = kBankTreeBranchLength;
                            treeObj.branchRadius = kBankTreeBranchRadius;
                            treeObj.taperRate    = kBankTreeTaperRate;
                            treeObj.angle        = kBankTreeAngle;
                            tempDatas.push_back(treeObj);
                        }
                    }
                }
            }
            continue;
        }

        obj.modelDirectory = "Resources";
        obj.modelFilename  = (obj.type == "Tree" || obj.type == "Rock")
            ? "Outputs/ProceduralAsset_LOD0.obj"
            : "plane.obj";

        if (item.contains("modelDirectory")) { obj.modelDirectory = item["modelDirectory"].get<std::string>(); }
        if (item.contains("modelFilename"))  { obj.modelFilename  = item["modelFilename"].get<std::string>(); }

        if (item.contains("position"))
        {
            const auto& pos = item["position"];
            obj.position = { pos.value("x", 0.0f), pos.value("y", 0.0f), pos.value("z", 0.0f) };
        }
        if (item.contains("rotation"))
        {
            const auto& rot = item["rotation"];
            obj.rotation = { rot.value("x", 0.0f), rot.value("y", 0.0f), rot.value("z", 0.0f) };
        }
        if (item.contains("scale"))
        {
            const auto& scl = item["scale"];
            obj.scale = { scl.value("x", 1.0f), scl.value("y", 1.0f), scl.value("z", 1.0f) };
        }

        obj.isStatic = item.value("isStatic", true);

        if (item.contains("parameters"))
        {
            const auto& params = item["parameters"];
            obj.seed            = params.value("seed", 0);
            obj.iterations      = params.value("iterations", kJsonDefaultIterations);
            obj.branchLength    = params.value("branchLength", kJsonDefaultBranchLength);
            obj.branchRadius    = params.value("branchRadius", kJsonDefaultBranchRadius);
            obj.taperRate       = params.value("taperRate", kJsonDefaultTaperRate);
            obj.angle           = params.value("angle", kJsonDefaultAngle);
            obj.subdivisions    = params.value("subdivisions", kJsonDefaultSubdivisions);
            obj.noiseStrength   = params.value("noiseStrength", kJsonDefaultNoise);
            obj.voronoiStrength = params.value("voronoiStrength", kJsonDefaultVoronoi);
            obj.crackStrength   = params.value("crackStrength", kJsonDefaultCrack);
            obj.biomeZoneType   = params.value("biome_zone_type", "");
        }

        tempDatas.push_back(obj);
    }

    objectDatas_ = std::move(tempDatas);
    rivers_ = std::move(tempRivers);
    RefreshRuntimeObjects();
    selectedIndex_ = ResolveSelectedAfterLoad(objectDatas_.size());

    if (std::filesystem::exists(filepath))
    {
        lastLoadedTime_ = std::filesystem::last_write_time(filepath);
    }
    return true;
}

void LevelEditor::RefreshRuntimeObjects()
{
    UnregisterAndClearColliders(colliders_);
    runtimeObjects_.clear();

    CollisionManager* colManager = CollisionManager::GetInstance();

    for (size_t idx = 0; idx < objectDatas_.size(); ++idx)
    {
        const auto& objData = objectDatas_[idx];
        const bool isProcedural  = (objData.type == "Tree" || objData.type == "Rock");
        const bool isBiome       = (objData.type == "Biome");
        const bool isSpawnOrLoot = (objData.type == "PlayerSpawn" || objData.type == "EnemySpawn" || objData.type == "LootBox");

        std::string cacheKey;
        if (isProcedural)
        {
            std::ostringstream oss;
            if (objData.type == "Tree")
            {
                oss << "ProcTree_" << objData.seed << "_" << objData.iterations << "_"
                    << objData.branchLength << "_" << objData.branchRadius << "_"
                    << objData.taperRate << "_" << objData.angle;
            }
            else
            {
                oss << "ProcRock_" << objData.seed << "_" << objData.subdivisions << "_"
                    << objData.noiseStrength << "_" << objData.voronoiStrength << "_"
                    << objData.crackStrength;
            }
            cacheKey = oss.str();
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
                    params.seed           = objData.seed;
                    params.iterations     = objData.iterations;
                    params.branchLength   = objData.branchLength;
                    params.branchRadius   = objData.branchRadius;
                    params.taperRate      = objData.taperRate;
                    params.angle          = objData.angle;
                    params.radialSegments = kTreeRadialSegments;
                    procMesh = BioProcedural::BioProceduralGenerator::GenerateTree(params);
                }
                else
                {
                    BioProcedural::RockParameters params;
                    params.seed            = objData.seed;
                    params.subdivisions    = objData.subdivisions;
                    params.noiseStrength   = objData.noiseStrength;
                    params.voronoiStrength = objData.voronoiStrength;
                    params.crackStrength   = objData.crackStrength;
                    procMesh = BioProcedural::BioProceduralGenerator::GenerateRock(params);
                }

                CopyProceduralMeshToModelData(procMesh, modelData);
                AssignTextureWithFallback((objData.type == "Tree") ? kWhiteTexPath : kRockAtlasTexPath, modelData);
            }
            else if (isSpawnOrLoot)
            {
                const BioProcedural::MeshData customMesh = (objData.type == "LootBox")
                    ? BioProcedural::BioProceduralGenerator::GenerateBoxShape()
                    : BioProcedural::BioProceduralGenerator::GenerateHumanShape();
                CopyProceduralMeshToModelData(customMesh, modelData);
                AssignTextureWithFallback(kWhiteTexPath, modelData);
            }
            else if (isBiome)
            {
                if (FileExists("Resources/plane.obj"))
                {
                    modelData = Object3d::LoadObjFile("Resources", "plane.obj");
                    modelData.material.textureIndex = TextureManager::kInvalidTextureIndex;
                    modelData.material.textureFilePath.clear();
                }
            }
            else
            {
                const std::string fullPath = objData.modelDirectory + "/" + objData.modelFilename;
                if (!FileExists(fullPath))
                {
                    runtimeObjects_.push_back(nullptr);
                    continue;
                }
                modelData = (objData.modelFilename.rfind(".obj") != std::string::npos)
                    ? Object3d::LoadObjFile(objData.modelDirectory, objData.modelFilename)
                    : Object3d::LoadModelFile(objData.modelDirectory, objData.modelFilename);
            }

            it = modelCache_.emplace(cacheKey, std::move(modelData)).first;
        }

        auto runtimeObj = std::make_unique<Object3d>();
        runtimeObj->Initialize(object3dCom_, it->second);
        runtimeObj->SetTranslate(objData.position);

        Vector3 appliedRot = objData.rotation;
        if (isBiome) { appliedRot.x += kHalfPi; }
        runtimeObj->SetRotate(appliedRot);
        runtimeObj->SetScale(objData.scale);
        runtimeObj->SetEnableLighting(true);

        for (const auto& entry : kObjectTypeColors)
        {
            if (objData.type == entry.typeName)
            {
                runtimeObj->SetColor(entry.color);
                break;
            }
        }

        if (isBiome)
        {
            Vector4 zoneColor = kDefaultBiomeZoneColor;
            for (const auto& entry : kBiomeZoneColors)
            {
                if (objData.biomeZoneType == entry.typeName)
                {
                    zoneColor = entry.color;
                    break;
                }
            }
            runtimeObj->SetColor(zoneColor);
            runtimeObj->SetEnableLighting(false);
        }

        runtimeObj->Update();
        runtimeObjects_.push_back(std::move(runtimeObj));

        if (isProcedural && colManager)
        {
            std::unique_ptr<Collider> collider;
            if (objData.type == "Tree")
            {
                const float radius = objData.branchRadius * objData.scale.x;
                const float height = objData.branchLength * objData.scale.y * kTreeColliderHeightScale;
                collider = std::make_unique<CapsuleCollider>(
                    radius, height, &const_cast<Vector3&>(objData.position), CollisionAttribute::Obstacle);
            }
            else
            {
                const Vector3 size = objData.scale * 2.0f;
                collider = std::make_unique<BoxCollider>(
                    size, &const_cast<Vector3&>(objData.position), nullptr, CollisionAttribute::Obstacle);
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
    obj.name           = name;
    obj.modelDirectory = dir;
    obj.modelFilename  = file;
    obj.position       = { 0.0f, kAddedObjectDefaultPosY, 0.0f };
    obj.rotation       = { 0.0f, 0.0f, 0.0f };
    obj.scale          = { 1.0f, 1.0f, 1.0f };
    obj.isStatic       = isStatic;

    objectDatas_.push_back(obj);
    RefreshRuntimeObjects();
    selectedIndex_ = static_cast<int>(objectDatas_.size()) - 1;
}

void LevelEditor::Draw(SceneRenderRequests& renderRequests)
{
    (void)renderRequests;
    const uint32_t defaultTex = TextureManager::GetInstance()->Load(kCheckerTexPath);
    const D3D12_GPU_DESCRIPTOR_HANDLE handle = TextureManager::GetInstance()->GetSrvHandleGPU(defaultTex);

    for (auto& river : rivers_)
    {
        if (river && handle.ptr != 0)
        {
            river->Draw(handle);
        }
    }
}
