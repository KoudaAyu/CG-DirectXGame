#pragma once
#include "Sprite.h"
#include "Object3d.h"
#include "../3D/Procedural/ProceduralGenerator.h"
#include "../3D/Procedural/ProceduralGPUGenerator.h"
#include <memory>
#include <vector>
#include <string>

class Camera;
class DebugCamera;
class MaterialManager;
class SpriteManager;
class OffScreenRendering;

class DebugUI
{
public:
    DebugUI(MaterialManager* materialManager, SpriteManager* spriteManager, Camera* camera,
            Sprite::Transform* transformObject, bool* useMonsterBall, bool* drawObject, bool* drawSprite,
            Object3d* object3d);

    void Initialize();
    void Update();
    void Finalize();

    void SetOffScreenRendering(OffScreenRendering* offScreenRendering) { offScreenRendering_ = offScreenRendering; }

private:
    Sprite::Transform* transformObject_ = nullptr;
    bool* useMonsterBall_ = nullptr;
    bool* drawSphere_ = nullptr;
    bool* drawObject_ = nullptr;
    bool* drawSprite_ = nullptr;
    Object3d* targetObject3d_ = nullptr;

    // --- プロシージャル生成用パラメータ ---
    int proceduralMode = 0; // 0: 通常モデル, 1: 岩石 (Rock), 2: 樹木 (Tree)
    ProceduralGenerator::RockParameters rockParams;
    ProceduralGenerator::TreeParameters treeParams;
    char exportFileName[64] = "ProceduralAsset";
    BioProcedural::ExportResult exportResult;
    bool hasExported = false; // エクスポートを行ったかどうかのフラグ
    
    // GPUプロシージャル岩石生成
    ProceduralGPUGenerator gpuGenerator;
    bool isGpuGeneratorInitialized = false;
    
    // 元のOBJモデルデータを退避するバッファ
    Object3d::ModelData originalModelData;
    bool isOriginalModelDataSaved = false;

private:
    Camera* camera_ = nullptr;
    DebugCamera* debugCamera_ = nullptr;
    SpriteManager* spriteManager_ = nullptr;
    MaterialManager* materialManager_ = nullptr;
    OffScreenRendering* offScreenRendering_ = nullptr;

    std::vector<std::unique_ptr<Sprite>> sprites;

};

