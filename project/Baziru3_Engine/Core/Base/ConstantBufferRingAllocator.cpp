#include "ConstantBufferRingAllocator.h"
#include <cassert>
#include <algorithm>
#include <cmath>

namespace BaziruEngine::Core {

ConstantBufferRingAllocator::~ConstantBufferRingAllocator()
{
    Finalize();
}

void ConstantBufferRingAllocator::Initialize(ID3D12Device* device, size_t totalCapacity)
{
    assert(device);
    Finalize();

    totalCapacity_ = (totalCapacity + 255) & ~size_t(255);
    currentOffset_ = 0;
    allocatedThisFrame_ = 0;

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Alignment = 0;
    resourceDesc.Width = totalCapacity_;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    HRESULT hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&ringBuffer_)
    );
    assert(SUCCEEDED(hr));

    D3D12_RANGE readRange{ 0, 0 };
    hr = ringBuffer_->Map(0, &readRange, reinterpret_cast<void**>(&mappedCpuAddress_));
    assert(SUCCEEDED(hr));

    gpuBaseAddress_ = ringBuffer_->GetGPUVirtualAddress();
}

void ConstantBufferRingAllocator::Finalize()
{
    if (ringBuffer_)
    {
        D3D12_RANGE writeRange{ 0, totalCapacity_ };
        ringBuffer_->Unmap(0, &writeRange);
        mappedCpuAddress_ = nullptr;
        ringBuffer_.Reset();
    }
    gpuBaseAddress_ = 0;
    currentOffset_ = 0;
    allocatedThisFrame_ = 0;
}

void ConstantBufferRingAllocator::NewFrame()
{
    std::lock_guard<std::mutex> lock(allocMutex_);
    allocatedThisFrame_ = 0;
}

ConstantBufferAllocation ConstantBufferRingAllocator::Allocate(size_t sizeInBytes, const void* initialData)
{
    std::lock_guard<std::mutex> lock(allocMutex_);
    assert(ringBuffer_ && mappedCpuAddress_);

    // 256バイトアライメント計算
    size_t alignedSize = (sizeInBytes + 255) & ~size_t(255);

    // リング周回チェック
    if (currentOffset_ + alignedSize > totalCapacity_)
    {
        currentOffset_ = 0; // ループして先頭に戻る
    }

    size_t offset = currentOffset_;
    currentOffset_ += alignedSize;
    allocatedThisFrame_ += alignedSize;

    ConstantBufferAllocation alloc;
    alloc.offset = static_cast<uint32_t>(offset);
    alloc.size = alignedSize;
    alloc.cpuAddress = mappedCpuAddress_ + offset;
    alloc.gpuAddress = gpuBaseAddress_ + offset;

    if (initialData)
    {
        std::memcpy(alloc.cpuAddress, initialData, sizeInBytes);
    }

    return alloc;
}

float ConstantBufferRingAllocator::GetCurrentFrameUsageRatio() const
{
    if (totalCapacity_ == 0) return 0.0f;
    return static_cast<float>(allocatedThisFrame_) / static_cast<float>(totalCapacity_);
}

} // namespace BaziruEngine::Core
