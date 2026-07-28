#pragma once
#pragma once

#include <cstdint>
#include <d3d12.h>
#include <vector>

#include "Matrix4x4.h"
#include "DirectXCom.h"
#include "Sprite.h"
#include "Vector.h"

class Camera;
class Object3dCom;
class MaterialManager;
class Light;

class Cylinder
{
public:
    struct Vertex
    {
        Vector4 pos;
        Vector2 uv;
        Vector3 normal;
    };

public:
    Cylinder() = default;
    ~Cylinder();

    void Initialize(DirectXCom* dxCommon, uint32_t divide = 32, float topRadius = 1.0f, float bottomRadius = 1.0f, float height = 3.0f);
    void Initialize(DirectXCom* dxCommon, Object3dCom* object3dCom, MaterialManager* materialManager, Light* light, Camera* camera, uint32_t divide = 32, float topRadius = 1.0f, float bottomRadius = 1.0f, float height = 3.0f);
    void Finalize();
    void Update();
    void Draw(D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle);

    const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const { return vertexBufferView_; }
    uint32_t GetVertexCount() const { return vertexCount_; }
   void SetTransform(const Sprite::Transform& transform) { transform_ = transform; useCustomWorldMatrix_ = false; }
    void SetWorldMatrix(const Matrix4x4& worldMatrix) { worldMatrix_ = worldMatrix; useCustomWorldMatrix_ = true; }
    void SetOverlayDraw(bool enabled) { overlayDraw_ = enabled; }
    Sprite::Transform& GetTransform() { return transform_; }

private:
    std::vector<Vertex> CreateMesh(uint32_t divide, float topRadius, float bottomRadius, float height) const;
    void CreateVertexBuffer(const std::vector<Vertex>& verts);

private:
    DirectXCom* dxCommon_ = nullptr;
    Object3dCom* object3dCom_ = nullptr;
    MaterialManager* materialManager_ = nullptr;
    Light* light_ = nullptr;
    Camera* camera_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    uint32_t vertexCount_ = 0;
    TransformationMatrix* transformationMatrixData_ = nullptr;
    Sprite::Transform transform_ = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
    Matrix4x4 worldMatrix_{};
    Matrix4x4 viewMatrix_{};
    Matrix4x4 wvpMatrix_{};
  bool useCustomWorldMatrix_ = false;
    bool overlayDraw_ = false;
};
