#include "ProceduralTreeGPUGenerator.h"
#include "DirectXCom.h"
#include "Sprite.h"
#include <d3dcompiler.h>
#include <iostream>
#include <cassert>

#pragma comment(lib, "d3dcompiler.lib")

ProceduralTreeGPUGenerator::~ProceduralTreeGPUGenerator()
{
}

bool ProceduralTreeGPUGenerator::Initialize(DirectXCom* dxCom)
{
    assert(dxCom != nullptr);
    dxCom_ = dxCom;

    // 1. パイプラインの構築
    if (!CreatePipeline())
    {
        return false;
    }

    // 2. 固定サイズでGPUバッファを生成 (最大2000セグメント対応)
    if (!CreateBuffers())
    {
        return false;
    }

    return true;
}

bool ProceduralTreeGPUGenerator::CreatePipeline()
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

    // 2. Root Parameter の定義
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

    // 3. ルートシグネチャの作成
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
            std::cout << "Tree Root Signature Error: " << (char*)errorBlob->GetBufferPointer() << std::endl;
        }
        return false;
    }
    device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), __uuidof(ID3D12RootSignature), (void**)rootSignature_.GetAddressOf());

    // 4. Compute Shader のコンパイル (DirectXComのコンパイラを使用)
    Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob = dxCom_->CompileShader(
        L"Resources/shaders/ProceduralTree.CS.hlsl",
        L"cs_6_0",
        dxCom_->GetDxcUtils(),
        dxCom_->GetDxcCompiler(),
        dxCom_->GetIncludeHandler(),
        std::cout
    );

    if (!shaderBlob)
    {
        std::cout << "Failed to compile ProceduralTree.CS.hlsl!" << std::endl;
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

bool ProceduralTreeGPUGenerator::CreateBuffers()
{
    auto device = dxCom_->GetDevice();

    // 1. Descriptor Heap (SRV/UAV用) の作成 (0: CBV, 1: SRV, 2: UAV)
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.NumDescriptors = 3;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    HRESULT hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descHeap_));
    if (FAILED(hr)) return false;

    uint32_t incrementSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_CPU_DESCRIPTOR_HANDLE cpuStart = descHeap_->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE gpuStart = descHeap_->GetGPUDescriptorHandleForHeapStart();

    D3D12_CPU_DESCRIPTOR_HANDLE cbvCpuHandle = cpuStart;
    D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle = { cpuStart.ptr + incrementSize };
    D3D12_CPU_DESCRIPTOR_HANDLE uavCpuHandle = { cpuStart.ptr + incrementSize * 2 };

    srvGpuHandle_ = { gpuStart.ptr + incrementSize };
    uavGpuHandle_ = { gpuStart.ptr + incrementSize * 2 };

    // 2. 定数バッファの作成 (アライメント256)
    uint32_t cbSize = (sizeof(TreeConstantBuffer) + 255) & ~255;
    constantBuffer_ = dxCom_->CreateBufferResource(device, cbSize);

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
    cbvDesc.BufferLocation = constantBuffer_->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = cbSize;
    device->CreateConstantBufferView(&cbvDesc, cbvCpuHandle);

    // 3. 出力頂点バッファ [UAV & VBV] の作成 (最大 100,000頂点)
    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT; // GPU専用デポ
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC resDesc{};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = kMaxVertices * sizeof(Sprite::VertexData);
    resDesc.Height = 1;
    resDesc.DepthOrArraySize = 1;
    resDesc.MipLevels = 1;
    resDesc.Format = DXGI_FORMAT_UNKNOWN;
    resDesc.SampleDesc.Count = 1;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&outputBuffer_)
    );
    if (FAILED(hr)) return false;

    // Vertex Buffer Viewのセットアップ
    vertexBufferView_.BufferLocation = outputBuffer_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = kMaxVertices * sizeof(Sprite::VertexData);
    vertexBufferView_.StrideInBytes = sizeof(Sprite::VertexData);

    // UAVの作成
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = kMaxVertices;
    uavDesc.Buffer.StructureByteStride = sizeof(Sprite::VertexData);
    uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
    device->CreateUnorderedAccessView(outputBuffer_.Get(), nullptr, &uavDesc, uavCpuHandle);

    // 4. 入力: 骨格セグメントバッファ [SRV] の作成
    resDesc.Width = kMaxSegments * sizeof(BioProcedural::GPUBranchesSegment);
    resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&skeletonSRV_)
    );
    if (FAILED(hr)) return false;

    // SRVの作成
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = kMaxSegments;
    srvDesc.Buffer.StructureByteStride = sizeof(BioProcedural::GPUBranchesSegment);
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    device->CreateShaderResourceView(skeletonSRV_.Get(), &srvDesc, srvCpuHandle);

    return true;
}

bool ProceduralTreeGPUGenerator::SetSkeletonData(const std::vector<BioProcedural::GPUBranchesSegment>& segments)
{
    if (segments.empty()) return false;
    
    // 安全クリッピング（最大セグメント数を超えた場合は切り捨ててクラッシュを防ぐ）
    uint32_t transferSize = (uint32_t)segments.size();
    if (transferSize > kMaxSegments)
    {
        transferSize = kMaxSegments;
    }

    auto device = dxCom_->GetDevice();
    auto commandList = dxCom_->GetCommandList();

    // 1. アップロードヒープ（一時バッファ）の作成
    Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer = dxCom_->CreateBufferResource(device, transferSize * sizeof(BioProcedural::GPUBranchesSegment));

    // 2. アップロードヒープへのデータ転送
    void* mapPtr = nullptr;
    HRESULT hr = uploadBuffer->Map(0, nullptr, &mapPtr);
    if (FAILED(hr)) return false;
    std::memcpy(mapPtr, segments.data(), transferSize * sizeof(BioProcedural::GPUBranchesSegment));
    uploadBuffer->Unmap(0, nullptr);

    // 3. コマンドリストを用いたコピー
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = skeletonSRV_.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    commandList->ResourceBarrier(1, &barrier);

    commandList->CopyBufferRegion(skeletonSRV_.Get(), 0, uploadBuffer.Get(), 0, transferSize * sizeof(BioProcedural::GPUBranchesSegment));

    // CS読み込み用に状態を遷移
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    commandList->ResourceBarrier(1, &barrier);

    // 実行と完了待機
    dxCom_->ExecuteAndWaitForGPU();

    isSkeletonDataTransferred_ = true;
    return true;
}

bool ProceduralTreeGPUGenerator::Dispatch(const Vector3& windDirection, float windStrength, float timeValue, uint32_t currentSegments)
{
    if (!isSkeletonDataTransferred_) return false;
    
    // 安全クリップ
    uint32_t activeSegments = (currentSegments > kMaxSegments) ? kMaxSegments : currentSegments;

    auto commandList = dxCom_->GetCommandList();

    // 1. 定数バッファの更新
    TreeConstantBuffer cbData{};
    cbData.windDirection = windDirection;
    cbData.windStrength = windStrength;
    cbData.time = timeValue;
    cbData.maxSegments = kMaxSegments;
    cbData.currentSegments = activeSegments;

    void* mapPtr = nullptr;
    HRESULT hr = constantBuffer_->Map(0, nullptr, &mapPtr);
    if (FAILED(hr)) return false;
    std::memcpy(mapPtr, &cbData, sizeof(cbData));
    constantBuffer_->Unmap(0, nullptr);

    // 2. 出力バッファを UAV にバリア遷移
    static bool isFirstDispatch = true;
    D3D12_RESOURCE_STATES stateBefore = isFirstDispatch ? D3D12_RESOURCE_STATE_COMMON : D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    isFirstDispatch = false;

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = outputBuffer_.Get();
    barrier.Transition.StateBefore = stateBefore;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    commandList->ResourceBarrier(1, &barrier);

    // 3. ルートシグネチャとパイプライン、 Descriptor Heap の設定
    commandList->SetComputeRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());

    ID3D12DescriptorHeap* heaps[] = { descHeap_.Get() };
    commandList->SetDescriptorHeaps(1, heaps);

    // 記述子テーブルのバインド
    D3D12_GPU_DESCRIPTOR_HANDLE gpuStart = descHeap_->GetGPUDescriptorHandleForHeapStart();
    uint32_t incrementSize = dxCom_->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_GPU_DESCRIPTOR_HANDLE handleCBV = gpuStart;
    D3D12_GPU_DESCRIPTOR_HANDLE handleSRV = { gpuStart.ptr + incrementSize };
    D3D12_GPU_DESCRIPTOR_HANDLE handleUAV = { gpuStart.ptr + incrementSize * 2 };

    commandList->SetComputeRootDescriptorTable(0, handleCBV);
    commandList->SetComputeRootDescriptorTable(1, handleSRV);
    commandList->SetComputeRootDescriptorTable(2, handleUAV);

    // 4. Dispatch 実行 (最大セグメント数をすべてクリア/展開するため、スレッドグループは最大数固定にする)
    uint32_t threadGroups = (kMaxSegments + 63) / 64;
    commandList->Dispatch(threadGroups, 1, 1);

    // 5. 描画用に VERTEX_BUFFER へステートを戻す
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    commandList->ResourceBarrier(1, &barrier);

    return true;
}
