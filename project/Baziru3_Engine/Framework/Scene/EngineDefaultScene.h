#pragma once

#include "BaseScene.h"
#include "FoliageField.h"
#include "NatureMeshRenderer.h"
#include "WaterSurface.h"
#include "WindSystem.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

class EngineDefaultScene : public BaseScene
{
public:
    EngineDefaultScene() = default;
    ~EngineDefaultScene() override { Finalize(); }

    void InitializeScene() override;
    void Finalize() override;
    void Update() override;
    void Draw(SceneRenderRequests& renderRequests) override;

private:
    struct PlacedObject
    {
        uint32_t meshVariant = 0;
        Matrix4x4 worldMatrix{};
        float mossScale = 1.0f;
    };

    void RebuildProceduralAssets(uint32_t seed);
    void UpdateCameraControl(float dt);
    void UpdateSunAndAtmosphere();
    void ApplyCameraPreset(int presetIndex);
    void DrawLabImGui();

private:
    float timer_ = 0.0f;
    bool  enableNatureLab_ = true;
    bool  initializedLab_ = false;

    std::unique_ptr<BaziruEngine::Environment::WindSystem>         windSystem_;
    std::unique_ptr<BaziruEngine::Environment::NatureMeshRenderer> natureRenderer_;
    std::unique_ptr<BaziruEngine::Environment::FoliageField>       foliageField_;
    std::unique_ptr<BaziruEngine::Environment::WaterSurface>       waterSurface_;

    BaziruEngine::Environment::NatureMeshHandle terrainMesh_{};
    std::array<BaziruEngine::Environment::NatureMeshHandle, 4> rockMeshes_{};
    std::array<BaziruEngine::Environment::NatureMeshHandle, 3> treeMeshes_{};
    BaziruEngine::Environment::NatureMeshHandle orbMarkerMesh_{};

    std::vector<PlacedObject> placedRocks_;
    std::vector<PlacedObject> placedTrees_;

    // 環境・太陽パラメータ
    float waterLevel_ = 0.0f;
    float sunElevationDeg_ = 32.0f; // 太陽高度 (度)
    float sunAzimuthDeg_ = 215.0f;  // 太陽方位角 (度: 逆光気味の美しい角度)
    float sunIntensity_ = 1.35f;
    float mossIntensity_ = 1.10f;
    float treeSssIntensity_ = 1.15f;
    float fogDensity_ = 0.0042f;
    Vector3 currentFogColor_ = { 0.64f, 0.79f, 0.94f };
    uint32_t currentSeed_ = 20261117u;

    // インタラクティブ草押し分けオーブ
    bool    autoMoveRepulsor_ = true;
    bool    showRepulsorMarker_ = true;
    float   repulsorRadius_ = 2.1f;
    Vector3 repulsorPos_ = { -8.5f, 0.5f, -4.0f };

    // フリーフライカメラ制御
    Vector3 camPos_ = { -9.8f, 3.3f, -15.5f };
    float   camPitch_ = 0.16f;
    float   camYaw_ = 0.52f;
    float   camMoveSpeed_ = 9.0f;
    bool    rmbDragging_ = false;
    POINT   lastMousePt_{};
};
