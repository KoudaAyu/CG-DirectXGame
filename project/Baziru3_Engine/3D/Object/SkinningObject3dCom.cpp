#include"SkinningObject3dCom.h"
#include "Light.h"
#include "SceneManager.h"
#include "TextureManager.h"
#include "Baziru3_Engine/Collision/CollisionManager.h"

// 参照メンバー logStream を初期化するコンストラクタ定義
SkinningObject3dCom::SkinningObject3dCom(std::ostream& logStream)
    : logStream(logStream)
{
}

void SkinningObject3dCom::Initialize(DirectXCom* directXCom)
{
    dxCommon = directXCom;
    CreateGraphicsPipelineState();
    CreateComputePipelineState();
}

void SkinningObject3dCom::Update()
{
}

void SkinningObject3dCom::RootSignature()
{
    descriptionRootSignature.Flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
}

void SkinningObject3dCom::CreateGraphicsPipelineState()
{
    RootSignature();
    Descriptor();
    CreateRootParameters();
    StaticSamplers();
    SignatureBlob();
    RootSignatureFromBlob();
    InputLayer();
    InitializeBlend();
    RasterizerState();
    ShaderCompile();
    InitializeGraphicPipeline();

    if (!pipelineState)
    {
        auto& desc = graphicPipelineStateDesc;
        desc.pRootSignature = rootSignature.Get();
        if (pipelineState == nullptr)
        {
            dxCommon->SetHr(dxCommon->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipelineState)));
            assert(SUCCEEDED(dxCommon->GetHr()));
        }
        if (pipelineStateWireframe == nullptr)
        {
            auto descWire = desc;
            descWire.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
            descWire.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // 防止Z-fighting
            descWire.PS = { wireframePixelShaderBlob->GetBufferPointer(), wireframePixelShaderBlob->GetBufferSize() };
            dxCommon->SetHr(dxCommon->GetDevice()->CreateGraphicsPipelineState(&descWire, IID_PPV_ARGS(&pipelineStateWireframe)));
            assert(SUCCEEDED(dxCommon->GetHr()));
        }
    }
}

void SkinningObject3dCom::PreDraw()
{
    if (!pipelineState)
    {
        CreateGraphicsPipelineState();
    }
    auto CommandList = dxCommon->GetCommandList();
    CommandList->SetGraphicsRootSignature(rootSignature.Get());
    CommandList->SetPipelineState(pipelineState.Get());
}

void SkinningObject3dCom::Draw(Object3d* object, const ::RenderContext& ctx, const Object3d::ModelData& modelData, bool drawObject)
{
    if (!ctx.commandList) return;
    if (!object) return;
    if (!ctx.camera)
    {
        Logger::Log(logStream, "Warning: camera is null when drawing object. Skipping draw.\n");
        return;
    }

    const SkinCluster& skinCluster = object->GetSkinCluster();

    // 1. スキンニング計算用の UAV バリア (COMMON状態からUAV状態へ)
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = skinCluster.uavResource.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    if (!object->IsShared())
    {
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        ctx.commandList->ResourceBarrier(1, &barrier);

        // 2. スキンニング実行
        Skinning(object, ctx.commandList);

        // 3. 頂点バッファとして読み込むためのバリア
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        ctx.commandList->ResourceBarrier(1, &barrier);
    }

    if (rootSignature)
    {
        ctx.commandList->SetGraphicsRootSignature(rootSignature.Get());
    }
    if (pipelineState)
    {
        ctx.commandList->SetPipelineState(pipelineState.Get());
    }

    // 描画用の定数バッファをアロケート・転送
    object->PrepareConstantBuffers(dxCommon);

    // Material (CBV at b0, Pixel Shader) -> Index 0
    ctx.commandList->SetGraphicsRootConstantBufferView(0, object->GetMaterialGPUAddress());

    // Transformation Matrix (CBV at b0, Vertex Shader) -> Index 1
    ctx.commandList->SetGraphicsRootConstantBufferView(1, object->GetTransformationMatrixGPUAddress());

    // Texture Descriptor Table (t3, Pixel Shader) -> Index 2
    if (ctx.textureHandle.ptr != 0)
    {
        ctx.commandList->SetGraphicsRootDescriptorTable(2, ctx.textureHandle);
    }

    // Directional Light (CBV at b1, Pixel Shader) -> Index 3
    if (ctx.light)
    {
        ctx.commandList->SetGraphicsRootConstantBufferView(3, ctx.light->GetDirectionalLightResource()->GetGPUVirtualAddress());
    }
    else
    {
        ctx.commandList->SetGraphicsRootConstantBufferView(3, 0);
    }

    // Camera (CBV at b2, Pixel Shader) -> Index 4
    if (ctx.camera->GetCameraResource())
    {
        ctx.commandList->SetGraphicsRootConstantBufferView(4, ctx.camera->GetCameraResource()->GetGPUVirtualAddress());
    }
    else
    {
        Logger::Log(logStream, "Warning: camera GPU resource not available when drawing object. Skipping draw.\n");
        return;
    }

    // Environment map SRV DescriptorTable (t4, Pixel Shader) -> Index 6
    uint32_t skyboxIndex = SceneManager::GetInstance()->GetSkyboxTextureIndex();
    D3D12_GPU_DESCRIPTOR_HANDLE skyboxHandle{};
    if (skyboxIndex != TextureManager::kInvalidTextureIndex)
    {
        skyboxHandle = TextureManager::GetInstance()->GetSrvHandleGPU(skyboxIndex);
    }
    else
    {
        skyboxHandle = ctx.textureHandle;
    }
    if (skyboxHandle.ptr != 0)
    {
        ctx.commandList->SetGraphicsRootDescriptorTable(6, skyboxHandle);
    }

    // CSで計算し終えた変形後バッファ (uavResource) を頂点バッファとして設定する
    D3D12_VERTEX_BUFFER_VIEW vbv{};
    vbv.BufferLocation = skinCluster.uavResource->GetGPUVirtualAddress();
    vbv.StrideInBytes = sizeof(Object3d::VertexData);
    vbv.SizeInBytes = UINT(sizeof(Object3d::VertexData) * modelData.vertices.size());

    ctx.commandList->IASetVertexBuffers(0, 1, &vbv);
    ctx.commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // インデックスバッファ設定と描画
    if (object->HasIndexBuffer())
    {
        ctx.commandList->IASetIndexBuffer(&object->GetIndexBufferView());
        ctx.commandList->DrawIndexedInstanced(static_cast<UINT>(modelData.indices.size()), 1, 0, 0, 0);
    }
    else
    {
        ctx.commandList->DrawInstanced(static_cast<UINT>(modelData.vertices.size()), 1, 0, 0);
    }

    // GPU-accelerated wireframe overlay draw (if enabled in Collision Debug panel)
    if (CollisionManager::GetInstance()->IsShowDebugColliders() && CollisionManager::GetInstance()->IsShowMeshWireframe())
    {
        bool drawWireframe = true;
        if (ctx.camera)
        {
            Vector3 camPos = ctx.camera->GetTranslate();
            Vector3 objPos = object->GetTranslate();
            float dx = objPos.x - camPos.x;
            float dy = objPos.y - camPos.y;
            float dz = objPos.z - camPos.z;
            float distSq = dx * dx + dy * dy + dz * dz;
            if (distSq > 40.0f * 40.0f) // Skip wireframe if further than 40 units
            {
                drawWireframe = false;
            }
        }

        if (drawWireframe && pipelineStateWireframe)
        {
            ctx.commandList->SetPipelineState(pipelineStateWireframe.Get());
            if (object->HasIndexBuffer())
            {
                ctx.commandList->DrawIndexedInstanced(static_cast<UINT>(modelData.indices.size()), 1, 0, 0, 0);
            }
            else
            {
                ctx.commandList->DrawInstanced(static_cast<UINT>(modelData.vertices.size()), 1, 0, 0);
            }
        }
    }

    // 4. 描画終了後にCOMMON状態に戻す
    D3D12_RESOURCE_BARRIER postDrawBarrier{};
    postDrawBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    postDrawBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    postDrawBarrier.Transition.pResource = skinCluster.uavResource.Get();
    postDrawBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    postDrawBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
    postDrawBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    ctx.commandList->ResourceBarrier(1, &postDrawBarrier);
}

void SkinningObject3dCom::Descriptor()
{
    // SRV: t0 for MatrixPalette
    descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange[0].NumDescriptors = 1;
    descriptorRange[0].BaseShaderRegister = 0;
    descriptorRange[0].RegisterSpace = 0;
    descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // SRV: t3 for Texture
    descriptorRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange[1].NumDescriptors = 1;
    descriptorRange[1].BaseShaderRegister = 3;
    descriptorRange[1].RegisterSpace = 0;
    descriptorRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // SRV: t4 for Environment map
    descriptorRange[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange[2].NumDescriptors = 1;
    descriptorRange[2].BaseShaderRegister = 4;
    descriptorRange[2].RegisterSpace = 0;
    descriptorRange[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
}

void SkinningObject3dCom::CreateRootParameters()
{
    // Root Parameter 0: Material (CBV at b0, Pixel Shader)
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].Descriptor.ShaderRegister = 0;

    // Root Parameter 1: Transformation Matrix (CBV at b0, Vertex Shader)
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[1].Descriptor.ShaderRegister = 0;

    // Root Parameter 2: Descriptor Table for Texture (t3, Pixel Shader)
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[2].DescriptorTable.pDescriptorRanges = &descriptorRange[1];
    rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

    // Root Parameter 3: Directional Light (CBV at b1, Pixel Shader)
    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[3].Descriptor.ShaderRegister = 1;

    // Root Parameter 4: Camera (CBV at b2, Pixel Shader)
    rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[4].Descriptor.ShaderRegister = 2;

    // Root Parameter 5: Descriptor Table for MatrixPalette (t0, Vertex Shader)
    rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[5].DescriptorTable.pDescriptorRanges = &descriptorRange[0];
    rootParameters[5].DescriptorTable.NumDescriptorRanges = 1;

    // Root Parameter 6: Descriptor Table for Environment map (t4, Pixel Shader)
    rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[6].DescriptorTable.pDescriptorRanges = &descriptorRange[2];
    rootParameters[6].DescriptorTable.NumDescriptorRanges = 1;

    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);
}

void SkinningObject3dCom::StaticSamplers()
{
    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers[0].ShaderRegister = 0;
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    staticSamplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[1].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers[1].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers[1].ShaderRegister = 1;
    staticSamplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    descriptionRootSignature.pStaticSamplers = staticSamplers;
    descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);
}

void SkinningObject3dCom::SignatureBlob()
{
    dxCommon->SetHr(D3D12SerializeRootSignature(&descriptionRootSignature,
        D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob));

    if (FAILED(dxCommon->GetHr()))
    {
        Logger::Log(logStream, reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
        assert(false);
    }
}

void SkinningObject3dCom::RootSignatureFromBlob()
{
    dxCommon->SetHr(dxCommon->GetDevice()->CreateRootSignature(0,
        signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature)));
    assert(SUCCEEDED(dxCommon->GetHr()));
}

void SkinningObject3dCom::InputLayer()
{
    // Position (slot 0: VertexData)
    inputElementDescs[0].SemanticName = "POSITION";
    inputElementDescs[0].SemanticIndex = 0;
    inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[0].InputSlot = 0;
    inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElementDescs[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    inputElementDescs[0].InstanceDataStepRate = 0;

    // TexCoord (slot 0: VertexData)
    inputElementDescs[1].SemanticName = "TEXCOORD";
    inputElementDescs[1].SemanticIndex = 0;
    inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    inputElementDescs[1].InputSlot = 0;
    inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElementDescs[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    inputElementDescs[1].InstanceDataStepRate = 0;

    // Normal (slot 0: VertexData)
    inputElementDescs[2].SemanticName = "NORMAL";
    inputElementDescs[2].SemanticIndex = 0;
    inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    inputElementDescs[2].InputSlot = 0;
    inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElementDescs[2].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    inputElementDescs[2].InstanceDataStepRate = 0;

    // Weight (slot 1: VertexInfluence)
    inputElementDescs[3].SemanticName = "WEIGHT";
    inputElementDescs[3].SemanticIndex = 0;
    inputElementDescs[3].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[3].InputSlot = 1;
    inputElementDescs[3].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElementDescs[3].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    inputElementDescs[3].InstanceDataStepRate = 0;

    // Index (slot 1: VertexInfluence)
    inputElementDescs[4].SemanticName = "INDEX";
    inputElementDescs[4].SemanticIndex = 0;
    inputElementDescs[4].Format = DXGI_FORMAT_R32G32B32A32_SINT;
    inputElementDescs[4].InputSlot = 1;
    inputElementDescs[4].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElementDescs[4].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    inputElementDescs[4].InstanceDataStepRate = 0;

    inputLayoutDesc.pInputElementDescs = inputElementDescs;
    inputLayoutDesc.NumElements = 3;
}

void SkinningObject3dCom::InitializeBlend()
{
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
}

void SkinningObject3dCom::RasterizerState()
{
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
}

void SkinningObject3dCom::ShaderCompile()
{
    vertexShaderBlob = dxCommon->CompileShader(L"Resources/shaders/Object3D.VS.hlsl",
        L"vs_6_0", dxCommon->GetDxcUtils().Get(), dxCommon->GetDxcCompiler(), dxCommon->GetIncludeHandler(), logStream);
    assert(vertexShaderBlob != nullptr);

    pixelShaderBlob = dxCommon->CompileShader(L"Resources/shaders/Object3d.PS.hlsl",
        L"ps_6_0", dxCommon->GetDxcUtils().Get(), dxCommon->GetDxcCompiler(), dxCommon->GetIncludeHandler(), logStream);
    assert(pixelShaderBlob != nullptr);

    wireframePixelShaderBlob = dxCommon->CompileShader(L"Resources/shaders/DebugWireframe.PS.hlsl",
        L"ps_6_0", dxCommon->GetDxcUtils().Get(), dxCommon->GetDxcCompiler(), dxCommon->GetIncludeHandler(), logStream);
    assert(wireframePixelShaderBlob != nullptr);
}

void SkinningObject3dCom::InitializeGraphicPipeline()
{
    graphicPipelineStateDesc.InputLayout = inputLayoutDesc;
    graphicPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),
        vertexShaderBlob->GetBufferSize() };
    graphicPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(),
        pixelShaderBlob->GetBufferSize() };
    graphicPipelineStateDesc.BlendState = blendDesc;
    graphicPipelineStateDesc.RasterizerState = rasterizerDesc;
    graphicPipelineStateDesc.NumRenderTargets = 1;
    graphicPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    graphicPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    graphicPipelineStateDesc.SampleDesc.Count = 1;
    graphicPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    depthStencilDesc.DepthEnable = TRUE;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    graphicPipelineStateDesc.DepthStencilState = depthStencilDesc;
    graphicPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
}

void SkinningObject3dCom::CreateComputePipelineState()
{
    // Compute Shader のコンパイル
    computeShaderBlob = dxCommon->CompileShader(L"Resources/shaders/Skinning.CS.hlsl",
        L"cs_6_0", dxCommon->GetDxcUtils().Get(), dxCommon->GetDxcCompiler(), dxCommon->GetIncludeHandler(), logStream);
    assert(computeShaderBlob != nullptr);

    // Compute Shader 用の RootSignature の作成
    // ※今回は差し当たりGraphics用と似た構成や、もしくは適した形で別途作成します。ここではスライドに合わせた形にします。
    D3D12_ROOT_SIGNATURE_DESC computeRootSigDesc{};
    computeRootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    // ディスクリプタレンジの定義
    D3D12_DESCRIPTOR_RANGE paletteRange{};
    paletteRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    paletteRange.NumDescriptors = 1;
    paletteRange.BaseShaderRegister = 0; // t0
    paletteRange.RegisterSpace = 0;
    paletteRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE inputVertexRange{};
    inputVertexRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    inputVertexRange.NumDescriptors = 1;
    inputVertexRange.BaseShaderRegister = 1; // t1
    inputVertexRange.RegisterSpace = 0;
    inputVertexRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE influenceRange{};
    influenceRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    influenceRange.NumDescriptors = 1;
    influenceRange.BaseShaderRegister = 2; // t2
    influenceRange.RegisterSpace = 0;
    influenceRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE uavRange{};
    uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    uavRange.NumDescriptors = 1;
    uavRange.BaseShaderRegister = 0; // u0
    uavRange.RegisterSpace = 0;
    uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // パラメータ構成
    D3D12_ROOT_PARAMETER computeParams[5] = {};
    
    // t0: gMatrixPalette (SRV)
    computeParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    computeParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    computeParams[0].DescriptorTable.pDescriptorRanges = &paletteRange;
    computeParams[0].DescriptorTable.NumDescriptorRanges = 1;

    // t1: gInputVertices (SRV)
    computeParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    computeParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    computeParams[1].DescriptorTable.pDescriptorRanges = &inputVertexRange;
    computeParams[1].DescriptorTable.NumDescriptorRanges = 1;

    // t2: gVertexInfluences (SRV)
    computeParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    computeParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    computeParams[2].DescriptorTable.pDescriptorRanges = &influenceRange;
    computeParams[2].DescriptorTable.NumDescriptorRanges = 1;

    // u0: gOutputVertices (UAV)
    computeParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    computeParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    computeParams[3].DescriptorTable.pDescriptorRanges = &uavRange;
    computeParams[3].DescriptorTable.NumDescriptorRanges = 1;

    // b0: gSkinningInformation (CBV)
    computeParams[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    computeParams[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    computeParams[4].Descriptor.ShaderRegister = 0; // b0

    computeRootSigDesc.pParameters = computeParams;
    computeRootSigDesc.NumParameters = _countof(computeParams);

    Microsoft::WRL::ComPtr<ID3DBlob> computeSigBlob = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> computeErrorBlob = nullptr;
    dxCommon->SetHr(D3D12SerializeRootSignature(&computeRootSigDesc,
        D3D_ROOT_SIGNATURE_VERSION_1, &computeSigBlob, &computeErrorBlob));

    if (FAILED(dxCommon->GetHr()))
    {
        if (computeErrorBlob)
        {
            Logger::Log(logStream, reinterpret_cast<char*>(computeErrorBlob->GetBufferPointer()));
        }
        assert(false);
    }

    dxCommon->SetHr(dxCommon->GetDevice()->CreateRootSignature(0,
        computeSigBlob->GetBufferPointer(), computeSigBlob->GetBufferSize(),
        IID_PPV_ARGS(&computeRootSignature)));
    assert(SUCCEEDED(dxCommon->GetHr()));

    // Compute Pipeline State の作成
    D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineStateDesc{};
    computePipelineStateDesc.CS = {
        computeShaderBlob->GetBufferPointer(),
        computeShaderBlob->GetBufferSize()
    };
    computePipelineStateDesc.pRootSignature = computeRootSignature.Get();

    dxCommon->SetHr(dxCommon->GetDevice()->CreateComputePipelineState(
        &computePipelineStateDesc,
        IID_PPV_ARGS(&computePipelineState)
    ));
    assert(SUCCEEDED(dxCommon->GetHr()));
}

void SkinningObject3dCom::Skinning(Object3d* object, ID3D12GraphicsCommandList* commandList)
{
    if (!object || !commandList)
    {
        return;
    }

    const SkinCluster& skinCluster = object->GetSkinCluster();
    const Object3d::ModelData& modelData = object->GetModelData();
    size_t vertexCount = modelData.vertices.size();
    if (vertexCount == 0)
    {
        return;
    }

    // PipelineState と RootSignature をセット
    commandList->SetComputeRootSignature(computeRootSignature.Get());
    commandList->SetPipelineState(computePipelineState.Get());

    // 0: paletteSrvHandle (t0)
    commandList->SetComputeRootDescriptorTable(0, skinCluster.paletteSrvHandle.second);

    // 1: inputVertexSrvHandle (t1)
    // 入力頂点バッファのSRVを作成してバインド
    D3D12_SHADER_RESOURCE_VIEW_DESC inputVertexSrvDesc{};
    inputVertexSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
    inputVertexSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    inputVertexSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    inputVertexSrvDesc.Buffer.FirstElement = 0;
    inputVertexSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    inputVertexSrvDesc.Buffer.NumElements = UINT(vertexCount);
    inputVertexSrvDesc.Buffer.StructureByteStride = sizeof(Object3d::VertexData);
    
    dxCommon->GetDevice()->CreateShaderResourceView(
        object->GetVertexResource().Get(),
        &inputVertexSrvDesc,
        skinCluster.inputVertexSrvHandle.first
    );
    commandList->SetComputeRootDescriptorTable(1, skinCluster.inputVertexSrvHandle.second);

    // 2: influenceSrvHandle (t2)
    commandList->SetComputeRootDescriptorTable(2, skinCluster.influenceSrvHandle.second);

    // 3: uavDescriptorHandle (u0)
    commandList->SetComputeRootDescriptorTable(3, skinCluster.uavDescriptorHandle.second);

    // 4: skinningInfoResource (b0, CBV)
    commandList->SetComputeRootConstantBufferView(4, skinCluster.skinningInfoResource->GetGPUVirtualAddress());

    // 計算の実行 (Dispatch)
    // 1スレッドグループあたり1024スレッドで処理
    UINT groupX = (UINT(vertexCount) + 1023) / 1024;
    commandList->Dispatch(groupX, 1, 1);
}