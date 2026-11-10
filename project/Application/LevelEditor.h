#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <filesystem>
#include <d3d12.h>
#include <wrl.h>

#include "Vector.h"
#include "Matrix4x4.h"
#include "RenderContext.h"
#include "Baziru3_Engine/Graphics/3D/Model/Model.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3d.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3dCom.h"
#include "Baziru3_Engine/3D/Water/River.h"
#include "Baziru3_Engine/Framework/Collision/Collider.h"
#include "Baziru3_Engine/Graphics/Primitive/Cylinder/Cylinder.h"

using Microsoft::WRL::ComPtr;

class DirectXCom;
class Object3dCom;
class Camera;
struct SceneRenderRequests;

struct LevelObjectData {
    static constexpr int   kDefaultSeed            = 12345;
    static constexpr int   kDefaultIterations      = 2;
    static constexpr float kDefaultBranchLength    = 0.15f;
    static constexpr float kDefaultBranchRadius    = 0.06f;
    static constexpr float kDefaultTaperRate       = 0.8f;
    static constexpr float kDefaultAngle           = 25.0f;
    static constexpr int   kDefaultSubdivisions    = 2;
    static constexpr float kDefaultNoiseStrength   = 0.25f;
    static constexpr float kDefaultVoronoiStrength = 0.15f;
    static constexpr float kDefaultCrackStrength   = 0.1f;

    std::string name;
    std::string type;
    std::string modelDirectory;
    std::string modelFilename;
    std::string biomeZoneType;
    bool isStatic = true;
    Vector3 position{ 0.0f, 0.0f, 0.0f };
    Vector3 rotation{ 0.0f, 0.0f, 0.0f };
    Vector3 scale{ 1.0f, 1.0f, 1.0f };

    int seed = kDefaultSeed;
    int iterations = kDefaultIterations;
    float branchLength = kDefaultBranchLength;
    float branchRadius = kDefaultBranchRadius;
    float taperRate = kDefaultTaperRate;
    float angle = kDefaultAngle;
    int subdivisions = kDefaultSubdivisions;
    float noiseStrength = kDefaultNoiseStrength;
    float voronoiStrength = kDefaultVoronoiStrength;
    float crackStrength = kDefaultCrackStrength;
};

class LevelEditor {
public:
    using ObjectData = LevelObjectData;

    LevelEditor() = default;
    ~LevelEditor();

    void Initialize(DirectXCom* dxCommon, Object3dCom* object3dCom);
    void Update(float deltaTime);
    void Draw(const RenderContext& ctx);
    void Draw(SceneRenderRequests& renderRequests);
    void DrawImGui();

    bool LoadFromFile(const std::string& filepath);
    bool LoadFromJson(const std::string& filepath);
    bool SaveToFile(const std::string& filepath);
    bool SaveToJson(const std::string& filepath);
    void RefreshRuntimeObjects();
    void AddObject(const std::string& name, const std::string& dir, const std::string& file, bool isStatic);

private:
    static constexpr size_t kInputBufferSize = 128;
    static constexpr size_t kAxisCount       = 3;

    DirectXCom* dxCommon_ = nullptr;
    Object3dCom* object3dCom_ = nullptr;

    std::string currentFilepath_;
    std::filesystem::file_time_type lastLoadedTime_;
    bool autoReloadEnabled_ = true;

    int selectedIndex_ = -1;
    char addName_[kInputBufferSize] = "";
    char addDir_[kInputBufferSize] = "";
    char addFile_[kInputBufferSize] = "";
    bool addIsStatic_ = true;

    uint32_t axisTextureIndex_ = 0;

    std::vector<LevelObjectData> objectDatas_;
    std::vector<std::unique_ptr<Object3d>> runtimeObjects_;
    std::unordered_map<std::string, Object3d::ModelData> modelCache_;

    std::vector<std::unique_ptr<Model>> models_;
    std::vector<std::unique_ptr<River>> rivers_;
    std::vector<std::unique_ptr<Collider>> colliders_;
    std::unique_ptr<Cylinder> axisCylinders_[kAxisCount];
    ComPtr<ID3D12Resource> axisMaterialResources_[kAxisCount];
};
