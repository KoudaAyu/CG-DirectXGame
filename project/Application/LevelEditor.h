#pragma once
#include <vector>
#include <string>
#include <memory>
#include <map>
#include <filesystem>
#include "Object3d.h"
#include "Object3dCom.h"
#include "Vector.h"
#include "RenderContext.h"
#include "Cylinder.h"
#include "Baziru3_Engine/Collision/Collider.h"

struct LevelObjectData
{
    std::string name;
    std::string modelDirectory;
    std::string modelFilename;
    Vector3 position = { 0.0f, 0.0f, 0.0f };
    Vector3 rotation = { 0.0f, 0.0f, 0.0f };
    Vector3 scale = { 1.0f, 1.0f, 1.0f };
    bool isStatic = true;

    // --- プロシージャル用拡張 ---
    std::string type = ""; // "Tree" or "Rock"
    unsigned int seed = 0;
    int iterations = 3;
    float branchLength = 1.0f;
    float branchRadius = 0.1f;
    float taperRate = 0.8f;
    float angle = 25.0f;
    int subdivisions = 3;
    float noiseStrength = 0.3f;
    float voronoiStrength = 0.2f;
    float crackStrength = 0.4f;
};

class LevelEditor
{
public:
    void Initialize(DirectXCom* dxCommon, Object3dCom* object3dCom);
    ~LevelEditor();
    void Update(float deltaTime);
    
    // 通常の3Dオブジェクトとしてレベル上のオブジェクト群を描画する
    void Draw(const RenderContext& ctx);
    
    void DrawImGui();

    bool SaveToFile(const std::string& filepath);
    bool SaveToJson(const std::string& filepath);
    bool LoadFromFile(const std::string& filepath);
    bool LoadFromJson(const std::string& filepath);

    const std::vector<LevelObjectData>& GetObjects() const { return objectDatas_; }
    std::vector<LevelObjectData>& GetObjects() { return objectDatas_; }

    const std::vector<std::unique_ptr<Object3d>>& GetRuntimeObjects() const { return runtimeObjects_; }

private:
    void RefreshRuntimeObjects();
    void AddObject(const std::string& name, const std::string& dir, const std::string& file, bool isStatic);

private:
    DirectXCom* dxCommon_ = nullptr;
    Object3dCom* object3dCom_ = nullptr;

    std::vector<LevelObjectData> objectDatas_;
    std::vector<std::unique_ptr<Object3d>> runtimeObjects_;
    std::vector<std::unique_ptr<Collider>> colliders_;
    std::map<std::string, Object3d::ModelData> modelCache_;

    std::unique_ptr<Cylinder> axisCylinders_[3];
    Microsoft::WRL::ComPtr<ID3D12Resource> axisMaterialResources_[3];
    uint32_t axisTextureIndex_ = 0;

    int selectedIndex_ = -1;

    // 新規オブジェクト追加用バッファ入力
    char addName_[128] = "NewObject";
    char addDir_[256] = "Resources";
    char addFile_[256] = "plane.obj";
    bool addIsStatic_ = true;
    
    std::string currentFilepath_ = "Resources/stage_layout.json";
    std::filesystem::file_time_type lastLoadedTime_;
    bool autoReloadEnabled_ = true;
};
