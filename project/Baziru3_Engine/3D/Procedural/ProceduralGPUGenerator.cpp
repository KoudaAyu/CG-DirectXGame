#include "ProceduralGPUGenerator.h"
#include "DirectXCom.h"
#include <d3dcompiler.h>
#include <iostream>
#include <random>
#include <cassert>

#pragma comment(lib, "d3dcompiler.lib")

ProceduralGPUGenerator::~ProceduralGPUGenerator()
{
}

bool ProceduralGPUGenerator::Initialize(DirectXCom* dxCom)
{
    assert(dxCom != nullptr);
    dxCom_ = dxCom;

    // 1. パイプライン (CS / RootSignature) の構築
    if (!CreatePipeline())
    {
        return false;
    }

    // 2. 最大頂点数を仮定してGPUバッファを生成 (Subdivisions 12で約5000頂点)
    // 今後の拡張を考慮して最大 16384 頂点に対応させる
    if (!CreateBuffers(16384))
    {
        return false;
    }

    return true;
}

bool ProceduralGPUGenerator::CreatePipeline()
{
    auto device = dxCom_->GetDevice();

    // 1. Descriptor Range の作成 (CBV, SRV, UAV 各1つ)
    D3D12_DESCRIPTOR_RANGE descRangeCBV{};
    descRangeCBV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    descRangeCBV.NumDescriptors = 1;
    descRangeCBV.BaseShaderRegister = 0; // b0
    descRangeCBV.RegisterSpace = 0;
    descRangeCBV.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE descRangeSRV{};
    descRangeSRV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descRangeSRV.NumDescriptors = 1;
    descRangeSRV.BaseShaderRegister = 0; // t0
    descRangeSRV.RegisterSpace = 0;
    descRangeSRV.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE descRangeUAV{};
    descRangeUAV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    descRangeUAV.NumDescriptors = 1;
    descRangeUAV.BaseShaderRegister = 0; // u0
    descRangeUAV.RegisterSpace = 0;
    descRangeUAV.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // 2. Root Parameter の定義 (3つのテーブル)
    D3D12_ROOT_PARAMETER rootParams[3] = {};

    rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[0].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[0].DescriptorTable.pDescriptorRanges = &descRangeCBV;
    rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[1].DescriptorTable.pDescriptorRanges = &descRangeSRV;
    rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[2].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[2].DescriptorTable.pDescriptorRanges = &descRangeUAV;
    rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // 3. ルートシグネチャのシリアライズと作成
    D3D12_ROOT_SIGNATURE_DESC rootSigDesc{};
    rootSigDesc.NumParameters = 3;
    rootSigDesc.pParameters = rootParams;
    rootSigDesc.NumStaticSamplers = 0;
    rootSigDesc.pStaticSamplers = nullptr;
    rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr))
    {
        if (errorBlob)
        {
            std::cout << "Root Signature Error: " << (char*)errorBlob->GetBufferPointer() << std::endl;
        }
        return false;
    }
    device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), __uuidof(ID3D12RootSignature), (void**)rootSignature_.GetAddressOf());

    // 4. Compute Shader のコンパイル
    // DirectXCom固有のDxcCompilerを使用してシェーダーをビルド
    Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob = dxCom_->CompileShader(
        L"Resources/shaders/ProceduralRock.CS.hlsl",
        L"cs_6_0",
        dxCom_->GetDxcUtils(),
        dxCom_->GetDxcCompiler(),
        dxCom_->GetIncludeHandler(),
        std::cout
    );

    if (!shaderBlob)
    {
        std::cout << "Failed to compile ProceduralRock.CS.hlsl!" << std::endl;
        return false;
    }

    // 5. Compute Pipeline State の作成
    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.CS = { shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize() };
    psoDesc.pRootSignature = rootSignature_.Get();

    hr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
    if (FAILED(hr))
    {
        return false;
    }

    return true;
}

bool ProceduralGPUGenerator::CreateBuffers(uint32_t maxVertices)
{
    auto device = dxCom_->GetDevice();
    currentMaxVertices_ = maxVertices;

    // 1. Descriptor Heap (SRV/UAV用) の作成
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.NumDescriptors = 3; // 0: CBV, 1: SRV, 2: UAV
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    HRESULT hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descHeap_));
    if (FAILED(hr)) return false;

    // ディスクリプタサイズとGPU/CPUハンドルの割り当て
    uint32_t incrementSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_CPU_DESCRIPTOR_HANDLE cpuStart = descHeap_->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE gpuStart = descHeap_->GetGPUDescriptorHandleForHeapStart();

    // CBV 用ハンドル (Index 0)
    D3D12_CPU_DESCRIPTOR_HANDLE cbvCpuHandle = cpuStart;
    D3D12_GPU_DESCRIPTOR_HANDLE cbvGpuHandle = gpuStart;

    // SRV 用ハンドル (Index 1)
    D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle = { cpuStart.ptr + incrementSize };
    srvGpuHandle_ = { gpuStart.ptr + incrementSize };

    // UAV 用ハンドル (Index 2)
    D3D12_CPU_DESCRIPTOR_HANDLE uavCpuHandle = { cpuStart.ptr + incrementSize * 2 };
    uavGpuHandle_ = { gpuStart.ptr + incrementSize * 2 };

    // 2. 定数バッファの作成 (アライメント256)
    // CBV構造体のサイズ
    struct CB_RockParameters {
        float scale;
        int subdivisions;
        float noiseStrength;
        float noiseFrequency;
        int octaves;
        float voronoiStrength;
        int voronoiCells;
        float crackStrength;
        float crackFrequency;
        unsigned int seed;
        float padding[2];
        float voronoiCenters[50][4]; // float4 アライメント用
    };
    uint32_t cbSize = (sizeof(CB_RockParameters) + 255) & ~255;
    constantBuffer_ = dxCom_->CreateBufferResource(device, cbSize);

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
    cbvDesc.BufferLocation = constantBuffer_->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = cbSize;
    device->CreateConstantBufferView(&cbvDesc, cbvCpuHandle);

    // 3. 出力頂点バッファ [UAV & VBV] の作成
    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT; // GPU専用
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC resDesc{};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = maxVertices * sizeof(Sprite::VertexData);
    resDesc.Height = 1;
    resDesc.DepthOrArraySize = 1;
    resDesc.MipLevels = 1;
    resDesc.Format = DXGI_FORMAT_UNKNOWN;
    resDesc.SampleDesc.Count = 1;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS; // UAVとして使用

    hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_COMMON, // D3D12バッファの初期状態はCOMMONにする必要がある
        nullptr,
        IID_PPV_ARGS(&outputBuffer_)
    );
    if (FAILED(hr)) return false;

    // VBV（VertexBufferView）のセットアップ
    vertexBufferView_.BufferLocation = outputBuffer_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = maxVertices * sizeof(Sprite::VertexData);
    vertexBufferView_.StrideInBytes = sizeof(Sprite::VertexData);

    // UAV の作成 (構造化バッファとして)
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = maxVertices;
    uavDesc.Buffer.StructureByteStride = sizeof(Sprite::VertexData);
    uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
    device->CreateUnorderedAccessView(outputBuffer_.Get(), nullptr, &uavDesc, uavCpuHandle);

    // 4. 入力: ベース球体頂点バッファ [SRV] の作成
    // (まだ中身はないため、デフォルト状態で確保のみ行う)
    resDesc.Flags = D3D12_RESOURCE_FLAG_NONE; // SRV専用のためUAV不要
    hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&baseMeshSRV_)
    );
    if (FAILED(hr)) return false;

    // SRV の作成 (構造化バッファとして)
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = maxVertices;
    srvDesc.Buffer.StructureByteStride = sizeof(Sprite::VertexData);
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    device->CreateShaderResourceView(baseMeshSRV_.Get(), &srvDesc, srvCpuHandle);

    return true;
}

bool ProceduralGPUGenerator::SetBaseMesh(const std::vector<Sprite::VertexData>& vertices)
{
    if (vertices.empty()) return false;
    assert(vertices.size() <= currentMaxVertices_);

    auto device = dxCom_->GetDevice();
    auto commandList = dxCom_->GetCommandList();

    // 1. アップロードヒープ（中間リソース）の作成
    Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer = dxCom_->CreateBufferResource(device, vertices.size() * sizeof(Sprite::VertexData));

    // 2. アップロードバッファへ頂点データを書き込む
    void* mapPtr = nullptr;
    HRESULT hr = uploadBuffer->Map(0, nullptr, &mapPtr);
    if (FAILED(hr)) return false;
    std::memcpy(mapPtr, vertices.data(), vertices.size() * sizeof(Sprite::VertexData));
    uploadBuffer->Unmap(0, nullptr);

    // 3. コマンドリストでデフォルトヒープへコピー
    // baseMeshSRV_ の状態を COPY_DEST に遷移
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = baseMeshSRV_.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    commandList->ResourceBarrier(1, &barrier);

    commandList->CopyBufferRegion(baseMeshSRV_.Get(), 0, uploadBuffer.Get(), 0, vertices.size() * sizeof(Sprite::VertexData));

    // 状態を NON_PIXEL_SHADER_RESOURCE (CS読み込み用) に遷移
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    commandList->ResourceBarrier(1, &barrier);

    // コマンド実行とGPU完了待ち
    dxCom_->ExecuteAndWaitForGPU();

    isBaseMeshSet_ = true;
    return true;
}

bool ProceduralGPUGenerator::Dispatch(const BioProcedural::RockParameters& params, uint32_t vertexCount)
{
    if (!isBaseMeshSet_) return false;
    assert(vertexCount <= currentMaxVertices_);

    auto commandList = dxCom_->GetCommandList();

    // 1. パラメータの定数バッファ転送
    struct CB_RockParameters {
        float scale;
        int subdivisions;
        float noiseStrength;
        float noiseFrequency;
        int octaves;
        float voronoiStrength;
        int voronoiCells;
        float crackStrength;
        float crackFrequency;
        unsigned int seed;
        float padding[2];
        float voronoiCenters[50][4]; // float4 アライメント用
    };

    CB_RockParameters cbData{};
    cbData.scale = params.scale;
    cbData.subdivisions = params.subdivisions;
    cbData.noiseStrength = params.noiseStrength;
    cbData.noiseFrequency = params.noiseFrequency;
    cbData.octaves = params.octaves;
    cbData.voronoiStrength = params.voronoiStrength;
    cbData.voronoiCells = params.voronoiCells;
    cbData.crackStrength = params.crackStrength;
    cbData.crackFrequency = params.crackFrequency;
    cbData.seed = params.seed;

    // CPU側での乱数生成によるボロノイ中心の計算
    std::mt19937 rand(params.seed);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    for (int i = 0; i < params.voronoiCells; ++i)
    {
        cbData.voronoiCenters[i][0] = dist(rand); // x
        cbData.voronoiCenters[i][1] = dist(rand); // y
        cbData.voronoiCenters[i][2] = dist(rand); // z
        cbData.voronoiCenters[i][3] = 1.0f;       // w (アライメント用)
    }

    void* mapPtr = nullptr;
    HRESULT hr = constantBuffer_->Map(0, nullptr, &mapPtr);
    if (FAILED(hr)) return false;
    std::memcpy(mapPtr, &cbData, sizeof(cbData));
    constantBuffer_->Unmap(0, nullptr);

    // 2. 出力バッファの状態を UAV (書き込み用) にバリア遷移
    static bool isFirstDispatch = true;
    D3D12_RESOURCE_STATES stateBefore = isFirstDispatch ? D3D12_RESOURCE_STATE_COMMON : D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    isFirstDispatch = false;

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = outputBuffer_.Get();
    barrier.Transition.StateBefore = stateBefore;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS; // UAVステート
    commandList->ResourceBarrier(1, &barrier);

    // 3. パイプラインと Descriptor Heap の設定
    commandList->SetComputeRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());

    ID3D12DescriptorHeap* heaps[] = { descHeap_.Get() };
    commandList->SetDescriptorHeaps(1, heaps);

    // 記述子テーブルのバインド (0: CBV, 1: SRV, 2: UAV)
    D3D12_GPU_DESCRIPTOR_HANDLE gpuStart = descHeap_->GetGPUDescriptorHandleForHeapStart();
    uint32_t incrementSize = dxCom_->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_GPU_DESCRIPTOR_HANDLE handleCBV = gpuStart;
    D3D12_GPU_DESCRIPTOR_HANDLE handleSRV = { gpuStart.ptr + incrementSize };
    D3D12_GPU_DESCRIPTOR_HANDLE handleUAV = { gpuStart.ptr + incrementSize * 2 };

    commandList->SetComputeRootDescriptorTable(0, handleCBV);
    commandList->SetComputeRootDescriptorTable(1, handleSRV);
    commandList->SetComputeRootDescriptorTable(2, handleUAV);

    // 4. Dispatch 実行 (スレッドグループ数は (N + 63) / 64)
    uint32_t threadGroups = (vertexCount + 63) / 64;
    commandList->Dispatch(threadGroups, 1, 1);

    // 5. 出力バッファの状態を VERTEX_BUFFER (グラフィックス描画用) にバリア戻し
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    commandList->ResourceBarrier(1, &barrier);

    return true;
}
