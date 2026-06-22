#include "Ring.h"
#include "Camera.h"
#include "Light.h"
#include "MaterialManager.h"
#include "Object3dCom.h"
#include "SceneManager.h"
#include "TextureManager.h"

#include <cmath>
#include <cstring>
#include <numbers>

Ring::~Ring()
{
    Finalize();
}

void Ring::Initialize(DirectXCom* dxCommon, uint32_t ringDivide, float outerRadius, float innerRadius)
{
    Finalize();

    dxCommon_ = dxCommon;
    if (!dxCommon_)
    {
        return;
    }

    auto verts = CreateMesh(ringDivide, outerRadius, innerRadius);
    CreateVertexBuffer(verts);
    vertexCount_ = static_cast<uint32_t>(verts.size());
}

void Ring::Initialize(DirectXCom* dxCommon, Object3dCom* object3dCom, MaterialManager* materialManager, Light* light, Camera* camera, uint32_t ringDivide, float outerRadius, float innerRadius)
{
    Initialize(dxCommon, ringDivide, outerRadius, innerRadius);

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

void Ring::Finalize()
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

void Ring::Update()
{
    if (!camera_ || !transformationMatrixData_)
    {
        return;
    }

    worldMatrix_ = MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    viewMatrix_ = camera_->GetViewMatrix();
    wvpMatrix_ = Multiply(worldMatrix_, Multiply(viewMatrix_, camera_->GetProjectionMatrix()));
    transformationMatrixData_->WVP = wvpMatrix_;
    transformationMatrixData_->World = worldMatrix_;
    transformationMatrixData_->WorldInverseTranspose = Transpose(Inverse(worldMatrix_));
}

void Ring::Draw(D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle)
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
    if (object3dCom_->GetEffectPipelineState())
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

    uint32_t skyboxIndex = SceneManager::GetInstance()->GetSkyboxTextureIndex();
    D3D12_GPU_DESCRIPTOR_HANDLE skyboxHandle{};
    if (skyboxIndex != TextureManager::kInvalidTextureIndex)
    {
        skyboxHandle = TextureManager::GetInstance()->GetSrvHandleGPU(skyboxIndex);
    }
    else
    {
        skyboxHandle = textureSrvHandle;
    }
    if (skyboxHandle.ptr != 0)
    {
        commandList->SetGraphicsRootDescriptorTable(5, skyboxHandle);
    }

    commandList->DrawInstanced(vertexCount_, 1, 0, 0);
}

std::vector<Ring::Vertex> Ring::CreateMesh(uint32_t ringDivide, float outerRadius, float innerRadius) const
{
    std::vector<Vertex> verts;
    verts.reserve(ringDivide * 6);

    const float twoPi = std::numbers::pi_v<float> * 2.0f;
    for (uint32_t i = 0; i < ringDivide; ++i)
    {
        float a = float(i) * twoPi / float(ringDivide);
        float b = float(i + 1) * twoPi / float(ringDivide);
        float sinA = std::sin(a);
        float cosA = std::cos(a);
        float sinB = std::sin(b);
        float cosB = std::cos(b);
        float u = float(i) / float(ringDivide);
        float uNext = float(i + 1) / float(ringDivide);

        Vector3 vOuterA3 = { -sinA * outerRadius, cosA * outerRadius, 0.0f };
        Vector3 vOuterB3 = { -sinB * outerRadius, cosB * outerRadius, 0.0f };
        Vector3 vInnerA3 = { -sinA * innerRadius, cosA * innerRadius, 0.0f };
        Vector3 vInnerB3 = { -sinB * innerRadius, cosB * innerRadius, 0.0f };
        Vector3 normal = { 0.0f, 0.0f, 1.0f };

        Vector4 vOuterA = { vOuterA3.x, vOuterA3.y, vOuterA3.z, 1.0f };
        Vector4 vOuterB = { vOuterB3.x, vOuterB3.y, vOuterB3.z, 1.0f };
        Vector4 vInnerA = { vInnerA3.x, vInnerA3.y, vInnerA3.z, 1.0f };
        Vector4 vInnerB = { vInnerB3.x, vInnerB3.y, vInnerB3.z, 1.0f };

        verts.push_back({ vOuterA, { u, 1.0f }, normal });
        verts.push_back({ vOuterB, { uNext, 1.0f }, normal });
        verts.push_back({ vInnerA, { u, 0.0f }, normal });

        verts.push_back({ vOuterB, { uNext, 1.0f }, normal });
        verts.push_back({ vInnerB, { uNext, 0.0f }, normal });
        verts.push_back({ vInnerA, { u, 0.0f }, normal });
    }

    return verts;
}

void Ring::CreateVertexBuffer(const std::vector<Vertex>& verts)
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
