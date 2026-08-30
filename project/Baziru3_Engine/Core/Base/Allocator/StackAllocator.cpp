#include "StackAllocator.h"
#include <cassert>
#include <stdexcept>

void StackAllocator::Initialize(size_t size)
{
    pool_ = std::make_unique<uint8_t[]>(size);
    totalSize_ = size;
    offset_ = 0;
}

void* StackAllocator::Allocate(size_t size, size_t alignment)
{
    assert(pool_ != nullptr && "StackAllocator is not initialized.");

    // アライメントを考慮したアロケーションオフセットの計算
    uintptr_t currentAddress = reinterpret_cast<uintptr_t>(pool_.get()) + offset_;
    uintptr_t alignedAddress = (currentAddress + (alignment - 1)) & ~(alignment - 1);
    size_t newOffset = alignedAddress - reinterpret_cast<uintptr_t>(pool_.get()) + size;

    // バッファオーバーフローのチェック
    if (newOffset > totalSize_)
    {
        throw std::bad_alloc();
    }

    offset_ = newOffset;
    return reinterpret_cast<void*>(alignedAddress);
}

void StackAllocator::Reset()
{
    offset_ = 0;
}
