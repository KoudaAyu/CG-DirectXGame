#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include <ostream>
#include <vector>

#include "Camera.h"
#include "DirectXCom.h"
#include "Light.h"
#include "Matrix4x4.h"
#include "OffScreenRendering.h"
#include "Vector.h"

namespace BaziruEngine::Environment
{

struct WaterVertex
{
    Vector3 position;
    Vector2 uv;
};

struct alignas(256) RealisticWaterConstants
{
    Matrix4x4 world;
    Matrix4x4 viewProj;
    Matrix4x4 invViewProj;
    Vector3   shallowColor;
    float     time;
    Vector3   deepColor;
    float     waveHeight;
    Vector3   absorptionCoeff;
    float     flowSpeed;
    Vector2   flowDirection;
    float     refractionScale;
    float     causticsIntensity;
    float     foamWidth;
    float     foamIntensity;
    float     nearZ;
    float     farZ;
    Vector3   fogColor;
    float     fogDensity;
};

/// <summary>
/// Gerstner波・屈折・Beer-Lambert水深減衰・水中コースティクス・障害物境界泡を備えたリアル水面システム
/// </summary>
class WaterSurface
{
public:
    WaterSurface() = default;
    ~WaterSurface() { Finalize(); }

    void InitializeGrid(
        DirectXCom* dxCommon,
        std::ostream& logStream,
        float sizeX,
        float sizeZ,
        int gridX = 160,
        int gridZ = 160,
        float waterLevel = 0.0f);

    void Finalize();
    void Update(float deltaTime);

    /// <summary>
    /// 描画直前に OffScreenRendering::CaptureSceneSnapshot を自動実行して屈折・水深を反映しつつ水面を描画
    /// </summary>
    void Draw(
        ID3D12GraphicsCommandList* commandList,
        OffScreenRendering* offScreen,
        const Camera* camera,
        const Light* light,
        D3D12_GPU_DESCRIPTOR_HANDLE envCubemapSrv);

    // パラメータ調整API
    void SetWaterLevel(float y) { waterLevel_ = y; }
    float GetWaterLevel() const { return waterLevel_; }

    void SetWaveHeight(float h) { waveHeight_ = h; }
    float GetWaveHeight() const { return waveHeight_; }

    void SetFlowSpeed(float s) { flowSpeed_ = s; }
    float GetFlowSpeed() const { return flowSpeed_; }

    void SetFlowDirection(const Vector2& dir) { flowDirection_ = dir; }
    const Vector2& GetFlowDirection() const { return flowDirection_; }

    void SetRefractionScale(float s) { refractionScale_ = s; }
    float GetRefractionScale() const { return refractionScale_; }

    void SetCausticsIntensity(float c) { causticsIntensity_ = c; }
    float GetCausticsIntensity() const { return causticsIntensity_; }

    void SetFoamWidth(float w) { foamWidth_ = w; }
    float GetFoamWidth() const { return foamWidth_; }

    void SetFoamIntensity(float i) { foamIntensity_ = i; }
    float GetFoamIntensity() const { return foamIntensity_; }

    void SetAbsorptionDensity(float densityScale) { absorptionScale_ = densityScale; }
    float GetAbsorptionDensity() const { return absorptionScale_; }

    void SetWaterColors(const Vector3& shallow, const Vector3& deep)
    {
        shallowColor_ = shallow;
        deepColor_ = deep;
    }
    const Vector3& GetShallowColor() const { return shallowColor_; }
    const Vector3& GetDeepColor() const { return deepColor_; }

    void SetFog(const Vector3& color, float density)
    {
        fogColor_ = color;
        fogDensity_ = density;
    }

private:
    void CreatePipeline(std::ostream& logStream);
    void BuildGridMesh(float sizeX, float sizeZ, int gridX, int gridZ);

private:
    DirectXCom* dxCommon_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW vbv_{};
    D3D12_INDEX_BUFFER_VIEW ibv_{};
    uint32_t indexCount_ = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> constantsBuffer_;
    RealisticWaterConstants* mappedConstants_ = nullptr;

    float   time_ = 0.0f;
    float   waterLevel_ = 0.0f;
    float   waveHeight_ = 0.065f;
    float   flowSpeed_ = 1.0f;
    Vector2 flowDirection_ = { 0.0f, 1.0f };
    float   refractionScale_ = 0.032f;
    float   causticsIntensity_ = 0.58f;
    float   foamWidth_ = 0.42f;
    float   foamIntensity_ = 0.90f;
    float   absorptionScale_ = 1.0f;

    Vector3 shallowColor_ = { 0.14f, 0.56f, 0.58f };
    Vector3 deepColor_    = { 0.02f, 0.13f, 0.26f };
    Vector3 baseAbsorption_ = { 1.35f, 0.48f, 0.24f }; // 赤が最も速く吸収され、青緑が深くまで届く物理特性
    Vector3 fogColor_     = { 0.62f, 0.78f, 0.94f };
    float   fogDensity_   = 0.0045f;
};

} // namespace BaziruEngine::Environment
