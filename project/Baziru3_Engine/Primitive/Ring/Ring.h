#pragma once

#include <cstdint>
#include <d3d12.h>
#include <vector>

#include "DirectXCom.h"
#include "Vector.h"

class Ring
{
public:
    struct Vertex
    {
        Vector4 pos;
        Vector2 uv;
        Vector3 normal;
    };

public:
    void Initialize(DirectXCom* dxCommon, uint32_t ringDivide = 64, float outerRadius = 1.0f, float innerRadius = 0.2f);
    void Finalize();

    const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const { return vertexBufferView_; }
    uint32_t GetVertexCount() const { return vertexCount_; }

private:
    std::vector<Vertex> CreateMesh(uint32_t ringDivide, float outerRadius, float innerRadius) const;

private:
    DirectXCom* dxCommon_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    uint32_t vertexCount_ = 0;
};
