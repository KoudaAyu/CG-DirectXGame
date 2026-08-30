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
    std::string name;
    std::string type;
    std::string modelDirectory;
    std::string modelFilename;
    std::string biomeZoneType;
    bool isStatic = true;
    Vector3 position{ 0.0f, 0.0f, 0.0f };
    Vector3 rotation{ 0.0f, 0.0f, 0.0f };
    Vector3 scale{ 1.0f, 1.0f, 1.0f };

    int seed = 12345;
    int iterations = 2;
    float branchLength = 0.15f;
    float branchRadius = 0.06f;
    float taperRate = 0.8f;
    float angle = 25.0f;
    int subdivisions = 2;
    float noiseStrength = 0.25f;
    float voronoiStrength = 0.15f;
    float crackStrength = 0.1f;
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
    DirectXCom* dxCommon_ = nullptr;
    Object3dCom* object3dCom_ = nullptr;

    std::string currentFilepath_;
    std::filesystem::file_time_type lastLoadedTime_;
    bool autoReloadEnabled_ = true;

    int selectedIndex_ = -1;
    char addName_[128] = "";
    char addDir_[128] = "";
    char addFile_[128] = "";
    bool addIsStatic_ = true;

    uint32_t axisTextureIndex_ = 0;

    std::vector<LevelObjectData> objectDatas_;
    std::vector<std::unique_ptr<Object3d>> runtimeObjects_;
    std::unordered_map<std::string, Object3d::ModelData> modelCache_;

    std::vector<std::unique_ptr<Model>> models_;
    std::vector<std::unique_ptr<River>> rivers_;
    std::vector<std::unique_ptr<Collider>> colliders_;
    std::unique_ptr<Cylinder> axisCylinders_[3];
    ComPtr<ID3D12Resource> axisMaterialResources_[3];
};
