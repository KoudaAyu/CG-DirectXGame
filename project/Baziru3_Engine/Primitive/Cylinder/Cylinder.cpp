#include "Cylinder.h"
#include "Cylinder.h"

#include "Camera.h"
#include "Light.h"
#include "MaterialManager.h"
#include "Object3dCom.h"

#include <cmath>
#include <cstring>
#include <numbers>

Cylinder::~Cylinder()
{
    Finalize();
}

void Cylinder::Initialize(DirectXCom* dxCommon, uint32_t divide, float topRadius, float bottomRadius, float height)
{
    Finalize();

    dxCommon_ = dxCommon;
    if (!dxCommon_)
    {
        return;
    }

    if (divide < 3)
    {
        divide = 3;
    }

    auto verts = CreateMesh(divide, topRadius, bottomRadius, height);
    CreateVertexBuffer(verts);
    vertexCount_ = static_cast<uint32_t>(verts.size());
}

void Cylinder::Initialize(DirectXCom* dxCommon, Object3dCom* object3dCom, MaterialManager* materialManager, Light* light, Camera* camera, uint32_t divide, float topRadius, float bottomRadius, float height)
{
    Initialize(dxCommon, divide, topRadius, bottomRadius, height);

    object3dCom_ = object3dCom;
    materialManager_ = materialManager;
    light_ = light;
    camera_ = camera;

    if (!dxCommon_)
    {
        return;
    }

    transformationMatrixResource_ = dxCommon_->CreateBufferResource(dxCommon_->GetDevice().Get(), sizeof(TransformationMatrix));
    transformationMatrixResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_));
    transformationMatrixData_->WVP = MakeIdentity4x4();
    transformationMatrixData_->World = MakeIdentity4x4();
    transformationMatrixData_->WorldInverseTranspose = MakeIdentity4x4();
}

void Cylinder::Finalize()
{
    if (transformationMatrixResource_ && transformationMatrixData_)
    {
        transformationMatrixResource_->Unmap(0, nullptr);
        transformationMatrixData_ = nullptr;
    }

    vertexBuffer_.Reset();
    transformationMatrixResource_.Reset();
    vertexBufferView_ = {};
    vertexCount_ = 0;
    object3dCom_ = nullptr;
    materialManager_ = nullptr;
    light_ = nullptr;
    camera_ = nullptr;
    dxCommon_ = nullptr;
}

void Cylinder::Update()
{
    if (!camera_ || !transformationMatrixData_)
    {
        return;
    }

    if (!useCustomWorldMatrix_)
    {
        worldMatrix_ = MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    }
    viewMatrix_ = Inverse(camera_->GetWorldMatrix());
    wvpMatrix_ = Multiply(worldMatrix_, Multiply(viewMatrix_, camera_->GetProjectionMatrix()));
    transformationMatrixData_->WVP = wvpMatrix_;
    transformationMatrixData_->World = worldMatrix_;
    transformationMatrixData_->WorldInverseTranspose = Transpose(Inverse(worldMatrix_));
}

void Cylinder::Draw(D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle)
{
    if (!dxCommon_ || !object3dCom_ || !materialManager_ || !light_ || !camera_)
    {
        return;
    }

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList().Get();
    if (!commandList || vertexCount_ == 0 || textureSrvHandle.ptr == 0 || !transformationMatrixResource_)
    {
        return;
    }

    commandList->RSSetViewports(1, &dxCommon_->GetViewport());
    commandList->RSSetScissorRects(1, &dxCommon_->GetScissorRect());

    commandList->SetGraphicsRootSignature(object3dCom_->GetRootSignature().Get());
    if (overlayDraw_ && object3dCom_->GetOverlayPipelineState())
    {
        commandList->SetPipelineState(object3dCom_->GetOverlayPipelineState().Get());
    }
    else if (object3dCom_->GetEffectPipelineState())
    {
        commandList->SetPipelineState(object3dCom_->GetEffectPipelineState().Get());
    }
    else
    {
        commandList->SetPipelineState(object3dCom_->GetPipelineState().Get());
    }
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    commandList->SetGraphicsRootConstantBufferView(0, materialManager_->GetMaterialResource()->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandle);
    commandList->SetGraphicsRootConstantBufferView(3, light_->GetDirectionalLightResource()->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(4, camera_->GetCameraResource()->GetGPUVirtualAddress());

    commandList->DrawInstanced(vertexCount_, 1, 0, 0);
}

std::vector<Cylinder::Vertex> Cylinder::CreateMesh(uint32_t divide, float topRadius, float bottomRadius, float height) const
{
    std::vector<Vertex> verts;
    verts.reserve(divide * 6);

    const float twoPi = std::numbers::pi_v<float> * 2.0f;
    for (uint32_t i = 0; i < divide; ++i)
    {
        float a = float(i) * twoPi / float(divide);
        float b = float(i + 1) * twoPi / float(divide);
        float sinA = std::sin(a);
        float cosA = std::cos(a);
        float sinB = std::sin(b);
        float cosB = std::cos(b);
        float u = float(i) / float(divide);
        float uNext = float(i + 1) / float(divide);

        // positions: follow same convention as Ring (x = -sin, y = height, z = cos)
        Vector4 topA = { -sinA * topRadius, height, cosA * topRadius, 1.0f };
        Vector4 topB = { -sinB * topRadius, height, cosB * topRadius, 1.0f };
        Vector4 bottomA = { -sinA * bottomRadius, 0.0f, cosA * bottomRadius, 1.0f };
        Vector4 bottomB = { -sinB * bottomRadius, 0.0f, cosB * bottomRadius, 1.0f };

        Vector3 normalA = { -sinA, 0.0f, cosA };
        Vector3 normalB = { -sinB, 0.0f, cosB };

        // tri 1: topA, bottomA, topB
        verts.push_back({ topA, { u, 1.0f }, normalA });
        verts.push_back({ bottomA, { u, 0.0f }, normalA });
        verts.push_back({ topB, { uNext, 1.0f }, normalB });

        // tri 2: topB, bottomA, bottomB
        verts.push_back({ topB, { uNext, 1.0f }, normalB });
        verts.push_back({ bottomA, { u, 0.0f }, normalA });
        verts.push_back({ bottomB, { uNext, 0.0f }, normalB });
    }

    return verts;
}

void Cylinder::CreateVertexBuffer(const std::vector<Vertex>& verts)
{
    if (!dxCommon_ || verts.empty())
    {
        return;
    }

    size_t sizeInBytes = verts.size() * sizeof(Vertex);
    vertexBuffer_ = dxCommon_->CreateBufferResource(dxCommon_->GetDevice(), sizeInBytes);

    void* mapped = nullptr;
    vertexBuffer_->Map(0, nullptr, &mapped);
    std::memcpy(mapped, verts.data(), sizeInBytes);
    vertexBuffer_->Unmap(0, nullptr);

    vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = static_cast<UINT>(sizeInBytes);
    vertexBufferView_.StrideInBytes = static_cast<UINT>(sizeof(Vertex));
}
