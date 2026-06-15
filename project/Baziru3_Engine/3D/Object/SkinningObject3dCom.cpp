#include"SkinningObject3dCom.h"
#include "Light.h"

// 参照メンバー logStream を初期化するコンストラクタ定義
SkinningObject3dCom::SkinningObject3dCom(std::ostream& logStream)
    : logStream(logStream)
{
}

void SkinningObject3dCom::Initialize(DirectXCom* directXCom)
{
    dxCommon = directXCom;
    CreateGraphicsPipelineState();
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

    if (rootSignature)
    {
        ctx.commandList->SetGraphicsRootSignature(rootSignature.Get());
    }
    if (pipelineState)
    {
        ctx.commandList->SetPipelineState(pipelineState.Get());
    }

    // Material (CBV at b0, Pixel Shader) -> Index 0
    if (object->GetMaterialResource())
    {
        ctx.commandList->SetGraphicsRootConstantBufferView(0, object->GetMaterialResource()->GetGPUVirtualAddress());
    }

    // Transformation Matrix (CBV at b0, Vertex Shader) -> Index 1
    if (object->GetTransformationMatrixResource())
    {
        ctx.commandList->SetGraphicsRootConstantBufferView(1, object->GetTransformationMatrixResource()->GetGPUVirtualAddress());
    }

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

    // MatrixPalette SRV DescriptorTable (t0, Vertex Shader) -> Index 5
    const SkinCluster& skinCluster = object->GetSkinCluster();
    if (skinCluster.paletteSrvHandle.second.ptr != 0)
    {
        ctx.commandList->SetGraphicsRootDescriptorTable(5, skinCluster.paletteSrvHandle.second);
    }

    // VBV を複数設定: VertexData (slot 0) と VertexInfluence (slot 1)
    D3D12_VERTEX_BUFFER_VIEW vbvs[2] = {
        object->GetVertexBufferView(),             // slot 0: VertexData (Position, TexCoord, Normal)
        skinCluster.influenceBufferView            // slot 1: VertexInfluence (Weight, Index)
    };
    ctx.commandList->IASetVertexBuffers(0, 2, vbvs);
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
    inputLayoutDesc.NumElements = _countof(inputElementDescs);
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
    vertexShaderBlob = dxCommon->CompileShader(L"Resources/CG4/SkinningObject3d.VS.hlsl",
        L"vs_6_0", dxCommon->GetDxcUtils().Get(), dxCommon->GetDxcCompiler(), dxCommon->GetIncludeHandler(), logStream);
    assert(vertexShaderBlob != nullptr);

    pixelShaderBlob = dxCommon->CompileShader(L"Resources/shaders/Object3d.PS.hlsl",
        L"ps_6_0", dxCommon->GetDxcUtils().Get(), dxCommon->GetDxcCompiler(), dxCommon->GetIncludeHandler(), logStream);
    assert(pixelShaderBlob != nullptr);
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
