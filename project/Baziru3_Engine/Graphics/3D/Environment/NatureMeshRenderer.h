#pragma once

#include <d3d12.h>
#include <dxcapi.h>
#include <wrl.h>
#include <cstdint>
#include <ostream>
#include <vector>

#include "BioProceduralGenerator.h"
#include "Camera.h"
#include "DirectXCom.h"
#include "Light.h"
#include "Matrix4x4.h"
#include "Vector.h"
#include "WindSystem.h"

namespace BaziruEngine::Environment
{

struct NatureVertex
{
    Vector3 position; // POSITION0
    Vector3 normal;   // NORMAL0
    Vector2 uv;       // TEXCOORD0
    Vector4 color;    // COLOR0    (rgb: ベース色, a: 葉シルエット種別 1.0=不透明, <1.0=葉カード)
    Vector4 params;   // TEXCOORD1 (x: 風揺れウェイト, y: 透過SSS強度, z: 苔ウェイト, w: AO)
};

struct NatureMeshHandle
{
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer;
    D3D12_VERTEX_BUFFER_VIEW vbv{};
    D3D12_INDEX_BUFFER_VIEW ibv{};
    uint32_t indexCount = 0;
};

struct alignas(256) BotanicalDrawConstants
{
    Matrix4x4 world;
    Matrix4x4 viewProj;
    Matrix4x4 worldInvTranspose;
    Vector4   baseTint;         // rgb: 乗算ティント, a: 全体アルファ
    float     waterLevel;       // 水面高さ (濡れ表現境界)
    float     wetShoreHeight;   // 水際濡れ幅
    float     mossIntensity;    // 苔の濃さ (0..2)
    float     sssIntensity;     // 葉の透過光(SSS)強度
    Vector3   fogColor;         // 大気遠近フォグ色
    float     fogDensity;       // 大気フォグ濃度
};

/// <summary>
/// 地形・苔むした岩石・風揺れ＆透過光対応の樹木を描画するエンジン汎用ネイチャーメッシュレンダラ
/// </summary>
class NatureMeshRenderer
{
public:
    static constexpr uint32_t kMaxDrawsPerFrame = 128;

    NatureMeshRenderer() = default;
    ~NatureMeshRenderer() { Finalize(); }

    void Initialize(DirectXCom* dxCommon, std::ostream& logStream);
    void Finalize();

    /// フレーム先頭で描画スロット番号をリセット
    void BeginFrame() { currentDrawSlot_ = 0; }

    /// BioProcedural::MeshData (GenerateRock 等) から GPU メッシュを生成
    NatureMeshHandle CreateRockMeshFromProcedural(DirectXCom* dxCommon, const BioProcedural::MeshData& srcMesh, const Vector3& rockColor = { 0.44f, 0.43f, 0.41f });

    /// 生の NatureVertex 配列から GPU メッシュを生成
    NatureMeshHandle UploadMesh(DirectXCom* dxCommon, const std::vector<NatureVertex>& vertices, const std::vector<uint32_t>& indices);

    /// 川と岸辺・なだらかな丘を持つプロシージャル地形メッシュを生成
    static void BuildRiversideTerrainMesh(
        float sizeX, float sizeZ, int gridX, int gridZ, float waterLevel,
        std::vector<NatureVertex>& outVertices, std::vector<uint32_t>& outIndices);

    /// 任意の (x, z) における川辺地形の標高と法線を評価（草・岩・木の接地配置用）
    static float EvaluateRiversideHeight(float x, float z);
    static Vector3 EvaluateRiversideNormal(float x, float z);
    static float EvaluateRiverCenterX(float z);

    /// 自然な枝分かれと球面法線・透過葉クラスタを持つ広葉樹メッシュを生成
    static void BuildRealisticTreeMesh(
        uint32_t seed, float heightScale, bool autumnTint,
        std::vector<NatureVertex>& outVertices, std::vector<uint32_t>& outIndices);

    /// メッシュを1インスタンス描画
    void DrawMesh(
        ID3D12GraphicsCommandList* commandList,
        const NatureMeshHandle& mesh,
        const Matrix4x4& worldMatrix,
        const Camera* camera,
        const Light* light,
        const WindSystem* wind,
        D3D12_GPU_DESCRIPTOR_HANDLE envCubemapSrv,
        float waterLevel = 0.0f,
        float mossIntensity = 1.0f,
        float sssIntensity = 1.0f,
        const Vector3& fogColor = { 0.62f, 0.78f, 0.94f },
        float fogDensity = 0.0045f,
        const Vector4& tint = { 1.0f, 1.0f, 1.0f, 1.0f });

private:
    void CreatePipeline(std::ostream& logStream);

private:
    DirectXCom* dxCommon_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

    Microsoft::WRL::ComPtr<ID3D12Resource> drawConstantsBuffer_;
    uint8_t* mappedConstantsBase_ = nullptr;
    uint32_t currentDrawSlot_ = 0;
};

} // namespace BaziruEngine::Environment
