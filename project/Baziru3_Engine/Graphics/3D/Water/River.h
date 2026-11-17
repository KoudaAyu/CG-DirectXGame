#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <string>

#include "Vector.h"
#include "Matrix4x4.h"
#include "DirectXCom.h"

class Camera;
class Object3dCom;
class MaterialManager;
class Light;

struct WaterParamsForGPU
{
    float time;
    float flowSpeed;
    float waveScale;
    float padding;
};

class River
{
public:
    struct Vertex
    {
        Vector4 pos;
        Vector2 uv;
        Vector3 normal;
    };

public:
    River() = default;
    ~River() { Finalize(); }

    void Initialize(DirectXCom* dxCommon, Object3dCom* object3dCom, MaterialManager* materialManager, Light* light, Camera* camera,
                    const std::vector<Vector3>& points, float width = 2.0f, float flowSpeed = 1.0f, float waveScale = 1.0f, const Vector4& color = {0.1f, 0.4f, 0.9f, 0.8f});
    void Finalize();
    void Update(float deltaTime);
    void Draw(D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle);

    void SetPoints(const std::vector<Vector3>& points) { points_ = points; RebuildMesh(); }
    void SetWidth(float width) { width_ = width; RebuildMesh(); }
    void SetFlowSpeed(float speed) { flowSpeed_ = speed; }
    void SetWaveScale(float scale) { waveScale_ = scale; }
    void SetColor(const Vector4& color) { color_ = color; }

private:
    void RebuildMesh();
    void CreatePipeline();

private:
    DirectXCom* dxCommon_ = nullptr;
    Object3dCom* object3dCom_ = nullptr;
    MaterialManager* materialManager_ = nullptr;
    Light* light_ = nullptr;
    Camera* camera_ = nullptr;

    std::vector<Vector3> points_;
    float width_ = 2.0f;
    float flowSpeed_ = 1.0f;
    float waveScale_ = 1.0f;
    Vector4 color_ = { 0.1f, 0.4f, 0.9f, 0.8f };
    float totalTime_ = 0.0f;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> riverBedVertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> riverBedIndexBuffer_;

    Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> waterParamsResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;

    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
    uint32_t indexCount_ = 0;

    D3D12_VERTEX_BUFFER_VIEW riverBedVertexBufferView_{};
    D3D12_INDEX_BUFFER_VIEW riverBedIndexBufferView_{};
    uint32_t riverBedIndexCount_ = 0;

    TransformationMatrix* transformationMatrixData_ = nullptr;
    WaterParamsForGPU* waterParamsData_ = nullptr;
    struct WaterMaterialForGPU
    {
        Vector4 color;
        int32_t enableLighting;
        int32_t specularModel;
        float reflectionFactor;
        float fresnelF0;
        Matrix4x4 uvTransform;
        float shininess;
        Vector3 padding2;
    };
    WaterMaterialForGPU* materialData_ = nullptr;

    static Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    static Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    static bool isPipelineCreated_;
};
