#include "River.h"
#include "Camera.h"
#include "Light.h"
#include "MaterialManager.h"
#include <algorithm>
#include <cmath>
#include <cassert>
#include <iostream>

Microsoft::WRL::ComPtr<ID3D12RootSignature> River::rootSignature_ = nullptr;
Microsoft::WRL::ComPtr<ID3D12PipelineState> River::pipelineState_ = nullptr;
bool River::isPipelineCreated_ = false;

void River::Initialize(DirectXCom* dxCommon, Object3dCom* object3dCom, MaterialManager* materialManager, Light* light, Camera* camera,
                       const std::vector<Vector3>& points, float width, float flowSpeed, float waveScale, const Vector4& color)
{
    dxCommon_ = dxCommon;
    object3dCom_ = object3dCom;
    materialManager_ = materialManager;
    light_ = light;
    camera_ = camera;

    points_ = points;
    width_ = width;
    flowSpeed_ = flowSpeed;
    waveScale_ = waveScale;
    color_ = color;

    CreatePipeline();

    // 定数バッファ作成
    transformationMatrixResource_ = dxCommon_->CreateBufferResource(dxCommon_->GetDevice().Get(), (sizeof(TransformationMatrix) + 255) & ~255);
    transformationMatrixResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_));

    waterParamsResource_ = dxCommon_->CreateBufferResource(dxCommon_->GetDevice().Get(), (sizeof(WaterParamsForGPU) + 255) & ~255);
    waterParamsResource_->Map(0, nullptr, reinterpret_cast<void**>(&waterParamsData_));

    materialResource_ = dxCommon_->CreateBufferResource(dxCommon_->GetDevice().Get(), (sizeof(WaterMaterialForGPU) + 255) & ~255);
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));

    RebuildMesh();
}

void River::Finalize()
{
    if (transformationMatrixResource_ && transformationMatrixData_)
    {
        transformationMatrixResource_->Unmap(0, nullptr);
        transformationMatrixData_ = nullptr;
    }
    if (waterParamsResource_ && waterParamsData_)
    {
        waterParamsResource_->Unmap(0, nullptr);
        waterParamsData_ = nullptr;
    }
    if (materialResource_ && materialData_)
    {
        materialResource_->Unmap(0, nullptr);
        materialData_ = nullptr;
    }
    transformationMatrixResource_.Reset();
    waterParamsResource_.Reset();
    materialResource_.Reset();
    vertexBuffer_.Reset();
    indexBuffer_.Reset();
    riverBedVertexBuffer_.Reset();
    riverBedIndexBuffer_.Reset();
}

void River::CreatePipeline()
{
    if (isPipelineCreated_) return;

    // --- ルートシグネチャの作成 ---
    D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
    descriptorRange[0].BaseShaderRegister = 3; // t3: gTexture
    descriptorRange[0].NumDescriptors = 1;
    descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[6] = {};
    // b0 (VS): TransformationMatrix
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[0].Descriptor.ShaderRegister = 0;

    // b0 (PS): Material
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[1].Descriptor.ShaderRegister = 0;

    // b1 (PS): DirectionalLight
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[2].Descriptor.ShaderRegister = 1;

    // b2 (PS): Camera
    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[3].Descriptor.ShaderRegister = 2;

    // b3 (VS/PS): WaterParams
    rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[4].Descriptor.ShaderRegister = 3;

    // t3 (PS): gTexture
    rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[5].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[5].DescriptorTable.pDescriptorRanges = &descriptorRange[0];

    // サンプラー
    D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers[0].ShaderRegister = 0;
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.NumParameters = _countof(rootParameters);
    rootSignatureDesc.pStaticSamplers = staticSamplers;
    rootSignatureDesc.NumStaticSamplers = _countof(staticSamplers);

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    assert(SUCCEEDED(hr));

    hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));

    // --- シェーダーのコンパイル ---
    Microsoft::WRL::ComPtr<IDxcBlob> vsBlob = dxCommon_->CompileShader(
        L"Resources/shaders/Water.VS.hlsl", L"vs_6_0",
        dxCommon_->GetDxcUtils().Get(), dxCommon_->GetDxcCompiler(), dxCommon_->GetIncludeHandler(), std::cout);
    Microsoft::WRL::ComPtr<IDxcBlob> psBlob = dxCommon_->CompileShader(
        L"Resources/shaders/Water.PS.hlsl", L"ps_6_0",
        dxCommon_->GetDxcUtils().Get(), dxCommon_->GetDxcCompiler(), dxCommon_->GetIncludeHandler(), std::cout);
    assert(vsBlob && psBlob);

    // --- パイプラインステートの作成 ---
    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = rootSignature_.Get();
    psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // 両面描画
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // 水面透過のためデプス書き込みはOFF
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    // アルファブレンディング
    psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
    psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
    assert(SUCCEEDED(hr));

    isPipelineCreated_ = true;
}

void River::RebuildMesh()
{
    std::vector<Vector3> pts = points_;
    if (pts.size() < 2)
    {
        pts = { { -10.0f, 0.1f, 0.0f }, { 10.0f, 0.1f, 0.0f } };
    }

    // 1. 川底 (RiverBed) メッシュの作成
    std::vector<Vertex> bedVertices;
    std::vector<uint32_t> bedIndices;

    // 2. 水面 (WaterSurface) メッシュの作成
    std::vector<Vertex> waterVertices;
    std::vector<uint32_t> waterIndices;

    float halfW = width_ * 0.5f;
    float currentV = 0.0f;
    float depth = 1.2f; // 川底深さ 1.2m

    for (size_t i = 0; i < pts.size(); ++i)
    {
        Vector3 dir = { 0.0f, 0.0f, 1.0f };
        if (i + 1 < pts.size())
        {
            dir = { pts[i + 1].x - pts[i].x, pts[i + 1].y - pts[i].y, pts[i + 1].z - pts[i].z };
        }
        else if (i > 0)
        {
            dir = { pts[i].x - pts[i - 1].x, pts[i].y - pts[i - 1].y, pts[i].z - pts[i - 1].z };
        }

        float len = std::sqrt(dir.x * dir.x + dir.z * dir.z);
        if (len > 0.0001f)
        {
            dir.x /= len;
            dir.z /= len;
        }

        Vector3 side = { -dir.z, 0.0f, dir.x };

        if (i > 0)
        {
            Vector3 prev = pts[i - 1];
            float segLen = std::sqrt((pts[i].x - prev.x) * (pts[i].x - prev.x) + (pts[i].z - prev.z) * (pts[i].z - prev.z));
            currentV += segLen / width_;
        }

        float px = pts[i].x;
        float py = pts[i].y;
        float pz = pts[i].z;

        // --- A. 川底すり鉢メッシュ (5頂点/断面) ---
        Vertex bv0, bv1, bv2, bv3, bv4;
        bv0.pos = { px - side.x * halfW,        py + 0.01f,       pz - side.z * halfW,        1.0f };
        bv1.pos = { px - side.x * halfW * 0.6f, py - depth * 0.6f, pz - side.z * halfW * 0.6f, 1.0f };
        bv2.pos = { px,                         py - depth,       pz,                         1.0f };
        bv3.pos = { px + side.x * halfW * 0.6f, py - depth * 0.6f, pz + side.z * halfW * 0.6f, 1.0f };
        bv4.pos = { px + side.x * halfW,        py + 0.01f,       pz + side.z * halfW,        1.0f };

        bv0.uv = { 0.0f, currentV };  bv0.normal = { -side.x * 0.5f, 0.866f, -side.z * 0.5f };
        bv1.uv = { 0.25f, currentV }; bv1.normal = { -side.x * 0.707f, 0.707f, -side.z * 0.707f };
        bv2.uv = { 0.5f, currentV };  bv2.normal = { 0.0f, 1.0f, 0.0f };
        bv3.uv = { 0.75f, currentV }; bv3.normal = { side.x * 0.707f, 0.707f, side.z * 0.707f };
        bv4.uv = { 1.0f, currentV };  bv4.normal = { side.x * 0.5f, 0.866f, side.z * 0.5f };

        bedVertices.push_back(bv0); bedVertices.push_back(bv1);
        bedVertices.push_back(bv2); bedVertices.push_back(bv3); bedVertices.push_back(bv4);

        if (i + 1 < pts.size())
        {
            uint32_t b0 = static_cast<uint32_t>(i * 5);
            uint32_t b1 = static_cast<uint32_t>((i + 1) * 5);
            bedIndices.push_back(b0 + 0); bedIndices.push_back(b0 + 1); bedIndices.push_back(b1 + 1);
            bedIndices.push_back(b0 + 0); bedIndices.push_back(b1 + 1); bedIndices.push_back(b1 + 0);

            bedIndices.push_back(b0 + 1); bedIndices.push_back(b0 + 2); bedIndices.push_back(b1 + 2);
            bedIndices.push_back(b0 + 1); bedIndices.push_back(b1 + 2); bedIndices.push_back(b1 + 1);

            bedIndices.push_back(b0 + 2); bedIndices.push_back(b0 + 3); bedIndices.push_back(b1 + 3);
            bedIndices.push_back(b0 + 2); bedIndices.push_back(b1 + 3); bedIndices.push_back(b1 + 2);

            bedIndices.push_back(b0 + 3); bedIndices.push_back(b0 + 4); bedIndices.push_back(b1 + 4);
            bedIndices.push_back(b0 + 3); bedIndices.push_back(b1 + 4); bedIndices.push_back(b1 + 3);
        }

        // --- B. 水面メッシュ (2頂点/断面) ---
        Vertex wv0, wv1;
        wv0.pos = { px - side.x * halfW * 0.95f, py - 0.05f, pz - side.z * halfW * 0.95f, 1.0f };
        wv1.pos = { px + side.x * halfW * 0.95f, py - 0.05f, pz + side.z * halfW * 0.95f, 1.0f };

        wv0.uv = { 0.0f, currentV }; wv0.normal = { 0.0f, 1.0f, 0.0f };
        wv1.uv = { 1.0f, currentV }; wv1.normal = { 0.0f, 1.0f, 0.0f };

        waterVertices.push_back(wv0);
        waterVertices.push_back(wv1);

        if (i + 1 < pts.size())
        {
            uint32_t wb0 = static_cast<uint32_t>(i * 2);
            uint32_t wb1 = static_cast<uint32_t>((i + 1) * 2);
            waterIndices.push_back(wb0 + 0); waterIndices.push_back(wb0 + 1); waterIndices.push_back(wb1 + 1);
            waterIndices.push_back(wb0 + 0); waterIndices.push_back(wb1 + 1); waterIndices.push_back(wb1 + 0);
        }
    }

    riverBedIndexCount_ = static_cast<uint32_t>(bedIndices.size());
    indexCount_ = static_cast<uint32_t>(waterIndices.size());

    // 1. 川底バッファ生成
    size_t bedVSize = sizeof(Vertex) * bedVertices.size();
    riverBedVertexBuffer_ = dxCommon_->CreateBufferResource(dxCommon_->GetDevice().Get(), bedVSize);
    Vertex* bedVData = nullptr;
    riverBedVertexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&bedVData));
    std::memcpy(bedVData, bedVertices.data(), bedVSize);
    riverBedVertexBuffer_->Unmap(0, nullptr);

    riverBedVertexBufferView_.BufferLocation = riverBedVertexBuffer_->GetGPUVirtualAddress();
    riverBedVertexBufferView_.SizeInBytes = static_cast<UINT>(bedVSize);
    riverBedVertexBufferView_.StrideInBytes = sizeof(Vertex);

    size_t bedISize = sizeof(uint32_t) * bedIndices.size();
    riverBedIndexBuffer_ = dxCommon_->CreateBufferResource(dxCommon_->GetDevice().Get(), bedISize);
    uint32_t* bedIData = nullptr;
    riverBedIndexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&bedIData));
    std::memcpy(bedIData, bedIndices.data(), bedISize);
    riverBedIndexBuffer_->Unmap(0, nullptr);

    riverBedIndexBufferView_.BufferLocation = riverBedIndexBuffer_->GetGPUVirtualAddress();
    riverBedIndexBufferView_.SizeInBytes = static_cast<UINT>(bedISize);
    riverBedIndexBufferView_.Format = DXGI_FORMAT_R32_UINT;

    // 2. 水面バッファ生成
    size_t vSize = sizeof(Vertex) * waterVertices.size();
    vertexBuffer_ = dxCommon_->CreateBufferResource(dxCommon_->GetDevice().Get(), vSize);
    Vertex* vData = nullptr;
    vertexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&vData));
    std::memcpy(vData, waterVertices.data(), vSize);
    vertexBuffer_->Unmap(0, nullptr);

    vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = static_cast<UINT>(vSize);
    vertexBufferView_.StrideInBytes = sizeof(Vertex);

    size_t iSize = sizeof(uint32_t) * waterIndices.size();
    indexBuffer_ = dxCommon_->CreateBufferResource(dxCommon_->GetDevice().Get(), iSize);
    uint32_t* iData = nullptr;
    indexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&iData));
    std::memcpy(iData, waterIndices.data(), iSize);
    indexBuffer_->Unmap(0, nullptr);

    indexBufferView_.BufferLocation = indexBuffer_->GetGPUVirtualAddress();
    indexBufferView_.SizeInBytes = static_cast<UINT>(iSize);
    indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
}

void River::Update(float deltaTime)
{
    totalTime_ += deltaTime;

    if (waterParamsData_)
    {
        waterParamsData_->time = totalTime_;
        waterParamsData_->flowSpeed = flowSpeed_;
        waterParamsData_->waveScale = waveScale_;
        waterParamsData_->padding = 0.0f;
    }

    if (transformationMatrixData_ && camera_)
    {
        Matrix4x4 world = MakeIdentity4x4();
        Matrix4x4 view = camera_->GetViewMatrix();
        Matrix4x4 proj = camera_->GetProjectionMatrix();
        Matrix4x4 wvp = Multiply(world, Multiply(view, proj));

        transformationMatrixData_->WVP = wvp;
        transformationMatrixData_->World = world;
        transformationMatrixData_->WorldInverseTranspose = Inverse(world);
    }
}

void River::Draw(D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle)
{
    if (indexCount_ == 0 || !dxCommon_ || !camera_ || textureSrvHandle.ptr == 0) return;

    auto commandList = dxCommon_->GetCommandList().Get();

    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());

    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // b0 (VS): WVP
    if (transformationMatrixResource_)
    {
        commandList->SetGraphicsRootConstantBufferView(0, transformationMatrixResource_->GetGPUVirtualAddress());
    }

    // b0 (PS): Material
    if (materialResource_)
    {
        commandList->SetGraphicsRootConstantBufferView(1, materialResource_->GetGPUVirtualAddress());
    }

    // b1 (PS): Light
    if (light_)
    {
        commandList->SetGraphicsRootConstantBufferView(2, light_->GetDirectionalLightResource()->GetGPUVirtualAddress());
    }

    // b2 (PS): Camera
    if (camera_ && camera_->GetCameraGpuAddress() != 0)
    {
        commandList->SetGraphicsRootConstantBufferView(3, camera_->GetCameraGpuAddress());
    }

    // b3 (VS/PS): WaterParams
    if (waterParamsResource_)
    {
        commandList->SetGraphicsRootConstantBufferView(4, waterParamsResource_->GetGPUVirtualAddress());
    }

    // t3 (PS): Texture
    commandList->SetGraphicsRootDescriptorTable(5, textureSrvHandle);

    // --- A. 泥の川底 (RiverBed) の描画 ---
    if (riverBedIndexCount_ > 0 && riverBedVertexBuffer_)
    {
        if (materialData_)
        {
            // 暗い湿った泥の川底カラー (Dark Mud Brown)
            materialData_->color = { 0.18f, 0.12f, 0.08f, 1.0f };
            materialData_->enableLighting = 1;
            materialData_->specularModel = 0;
            materialData_->reflectionFactor = 0.0f;
            materialData_->fresnelF0 = 0.04f;
            materialData_->uvTransform = MakeIdentity4x4();
            materialData_->shininess = 16.0f;
        }
        commandList->SetGraphicsRootConstantBufferView(1, materialResource_->GetGPUVirtualAddress());
        commandList->IASetVertexBuffers(0, 1, &riverBedVertexBufferView_);
        commandList->IASetIndexBuffer(&riverBedIndexBufferView_);
        commandList->DrawIndexedInstanced(riverBedIndexCount_, 1, 0, 0, 0);
    }

    // --- B. 水面 (WaterSurface) の描画 ---
    if (indexCount_ > 0 && vertexBuffer_)
    {
        if (materialData_)
        {
            // 水色透明カラー
            materialData_->color = color_;
            materialData_->enableLighting = 1;
            materialData_->specularModel = 0;
            materialData_->reflectionFactor = 0.0f;
            materialData_->fresnelF0 = 0.04f;
            materialData_->uvTransform = MakeIdentity4x4();
            materialData_->shininess = 96.0f;
        }
        commandList->SetGraphicsRootConstantBufferView(1, materialResource_->GetGPUVirtualAddress());
        commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
        commandList->IASetIndexBuffer(&indexBufferView_);
        commandList->DrawIndexedInstanced(indexCount_, 1, 0, 0, 0);
    }
}
