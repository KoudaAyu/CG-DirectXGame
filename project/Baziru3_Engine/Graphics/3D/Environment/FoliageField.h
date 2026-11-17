#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include <functional>
#include <ostream>
#include <vector>

#include "Camera.h"
#include "DirectXCom.h"
#include "Light.h"
#include "Matrix4x4.h"
#include "Vector.h"
#include "WindSystem.h"

namespace BaziruEngine::Environment
{

struct GrassBladeVertex
{
    Vector3 localPos; // x: 幅[-0.5..0.5], y: 高さ比率[0..1], z: 自然カーブ
    Vector3 normal;   // キャンバー断面法線
    float   heightT;  // 根元0.0 〜 穂先1.0
};

struct GrassInstance
{
    Vector3 position;       // 12B
    float   height;         // 4B  -> 16B
    float   width;          // 4B
    float   yaw;            // 4B
    float   bend;           // 4B
    float   colorVariation; // 4B  -> 32B
};

struct alignas(256) GrassFieldConstants
{
    Matrix4x4 viewProj;
    Vector3   baseColorRoot;
    float     sssIntensity;
    Vector3   baseColorTip;
    float     maxDrawDistance;
    Vector3   dryTipColor;
    float     specularIntensity;
    Vector3   fogColor;
    float     fogDensity;
};

/// <summary>
/// 数万本の3D曲面草ブレードを1ドローコールでGPUインスタンシング描画する植生システム
/// </summary>
class FoliageField
{
public:
    FoliageField() = default;
    ~FoliageField() { Finalize(); }

    void Initialize(DirectXCom* dxCommon, std::ostream& logStream, uint32_t maxInstances = 36000);
    void Finalize();

    /// 地形高さコールバックに従って自然な株（クランプ）分布で草インスタンスを生成・転送
    void PopulateOnTerrain(
        float areaSizeX,
        float areaSizeZ,
        float waterLevel,
        uint32_t targetCount,
        uint32_t seed,
        const std::function<float(float, float)>& heightFunc,
        const std::function<Vector3(float, float)>& normalFunc);

    /// 直接インスタンス配列をアップロード
    void UploadInstances(const std::vector<GrassInstance>& instances);

    void Draw(
        ID3D12GraphicsCommandList* commandList,
        const Camera* camera,
        const Light* light,
        const WindSystem* wind);

    // 調整パラメータ
    void SetActiveInstanceCount(uint32_t count);
    uint32_t GetActiveInstanceCount() const { return activeInstanceCount_; }
    uint32_t GetTotalUploadedCount() const { return uploadedInstanceCount_; }
    uint32_t GetMaxCapacity() const { return maxInstances_; }

    void SetSssIntensity(float sss) { sssIntensity_ = sss; }
    float GetSssIntensity() const { return sssIntensity_; }

    void SetSpecularIntensity(float spec) { specularIntensity_ = spec; }
    float GetSpecularIntensity() const { return specularIntensity_; }

    void SetMaxDrawDistance(float dist) { maxDrawDistance_ = dist; }
    float GetMaxDrawDistance() const { return maxDrawDistance_; }

    void SetFog(const Vector3& color, float density)
    {
        fogColor_ = color;
        fogDensity_ = density;
    }

    void SetBladePalette(const Vector3& rootCol, const Vector3& tipCol, const Vector3& dryCol)
    {
        rootColor_ = rootCol;
        tipColor_ = tipCol;
        dryColor_ = dryCol;
    }

private:
    void CreateBladeMesh();
    void CreatePipeline(std::ostream& logStream);

private:
    DirectXCom* dxCommon_ = nullptr;
    uint32_t maxInstances_ = 0;
    uint32_t uploadedInstanceCount_ = 0;
    uint32_t activeInstanceCount_ = 0;
    uint32_t instanceSrvIndex_ = UINT32_MAX;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

    Microsoft::WRL::ComPtr<ID3D12Resource> bladeVertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> bladeIndexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW vbv_{};
    D3D12_INDEX_BUFFER_VIEW ibv_{};
    uint32_t bladeIndexCount_ = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> instanceBuffer_;
    GrassInstance* mappedInstances_ = nullptr;
    D3D12_GPU_DESCRIPTOR_HANDLE instanceSrvGpu_{};

    Microsoft::WRL::ComPtr<ID3D12Resource> constantsBuffer_;
    GrassFieldConstants* mappedConstants_ = nullptr;

    Vector3 rootColor_ = { 0.07f, 0.20f, 0.05f };
    Vector3 tipColor_  = { 0.30f, 0.58f, 0.14f };
    Vector3 dryColor_  = { 0.56f, 0.54f, 0.20f };
    Vector3 fogColor_  = { 0.62f, 0.78f, 0.94f };
    float   fogDensity_ = 0.0045f;
    float   sssIntensity_ = 1.15f;
    float   specularIntensity_ = 0.42f;
    float   maxDrawDistance_ = 68.0f;
};

} // namespace BaziruEngine::Environment
