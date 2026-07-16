#pragma once

#include <array>
#include <wrl.h>
#include <d3d12.h>
#include "Sprite.h"
#include "DirectXCom.h"
#include "Camera.h"
#include "Matrix4x4.h"

class SkyBox
{
public:
    SkyBox() = default;
    ~SkyBox();

    void Initialize(DirectXCom* directXCom, Camera* camera);
    void Update();
    void Draw(ID3D12GraphicsCommandList* commandList, D3D12_GPU_DESCRIPTOR_HANDLE textureHandle);

private:
    void CreateVertexData();
    void CreateIndexData();
    void CreateBuffers();
    void UpdateTransformationMatrix();

private:
    DirectXCom* directXCom_ = nullptr;
    Camera* camera_ = nullptr;

    std::array<Sprite::VertexData, 24> vertexData{};
    std::array<uint32_t, 36> indexData_{};

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_ = nullptr;

    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    D3D12_INDEX_BUFFER_VIEW indexBufferView_{};

    TransformationMatrix transformationMatrixData_{};
};

