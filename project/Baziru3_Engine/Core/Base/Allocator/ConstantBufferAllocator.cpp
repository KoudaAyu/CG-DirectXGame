#include "ConstantBufferAllocator.h"
#include "../DirectXCom.h"
#include <cassert>
#include <format>

ConstantBufferAllocator::ConstantBufferAllocator(DirectXCom* dxCommon)
    : dxCommon_(dxCommon)
{
}

void ConstantBufferAllocator::Initialize(size_t bufferSize)
{
    assert(dxCommon_);
    bufferSize_ = bufferSize;

    // トリプルバッファリング用にフレームサイズを計算（256バイト境界アライン）
    frameSize_ = AlignUp(bufferSize_ / kNumFrames, 256);
    bufferSize_ = frameSize_ * kNumFrames; // バッファ全体のサイズをアライン済みのサイズに補正

    // コミットリソースとしてアップロードバッファ（定数バッファ）を一括確保
    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC resDesc{};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Alignment = 0;
    resDesc.Width = bufferSize_;
    resDesc.Height = 1;
    resDesc.DepthOrArraySize = 1;
    resDesc.MipLevels = 1;
    resDesc.Format = DXGI_FORMAT_UNKNOWN;
    resDesc.SampleDesc.Count = 1;
    resDesc.SampleDesc.Quality = 0;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&bufferResource_)
    );
    assert(SUCCEEDED(hr));

    // リソースを永続的に Map
    D3D12_RANGE readRange{0, 0}; // CPUからは読まない
    hr = bufferResource_->Map(0, &readRange, reinterpret_cast<void**>(&cpuStart_));
    assert(SUCCEEDED(hr));

    gpuStart_ = bufferResource_->GetGPUVirtualAddress();
    currentOffset_ = 0;
    currentFrameIndex_ = 0;
}

void ConstantBufferAllocator::Finalize()
{
    if (bufferResource_)
    {
        D3D12_RANGE readRange{0, 0};
        bufferResource_->Unmap(0, &readRange);
        cpuStart_ = nullptr;
        bufferResource_.Reset();
    }
}

void ConstantBufferAllocator::BeginFrame()
{
    // フレームインデックスを進めて、そのフレーム用の領域の先頭オフセットを設定
    currentFrameIndex_ = (currentFrameIndex_ + 1) % kNumFrames;
    currentOffset_ = currentFrameIndex_ * frameSize_;
}

ConstantBufferAllocator::Allocation ConstantBufferAllocator::Allocate(size_t size)
{
    size_t alignedSize = AlignUp(size, 256);
    
    // 今フレームの割り当て領域上限
    size_t frameEndOffset = (currentFrameIndex_ + 1) * frameSize_;

    if (currentOffset_ + alignedSize > frameEndOffset)
    {
        // もし領域が不足した場合はアサーション（通常はバッファサイズ設定に余裕を持たせる）
        assert(false && "ConstantBufferAllocator frame memory exhausted!");
        return Allocation{};
    }

    Allocation alloc;
    alloc.cpuAddress = cpuStart_ + currentOffset_;
    alloc.gpuAddress = gpuStart_ + currentOffset_;

    currentOffset_ += alignedSize;
    return alloc;
}
