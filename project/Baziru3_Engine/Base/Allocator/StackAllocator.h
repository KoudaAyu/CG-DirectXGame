#pragma once
#include <cstdint>
#include <cstddef>
#include <memory>

/// <summary>
/// 短寿命オブジェクト用の高速スタックアロケーター
/// 毎フレーム開始時に一括リセットされ、フレーム内の一時アロケーションを O(1) で行います。
/// </summary>
class StackAllocator
{
public:
    StackAllocator() = default;
    ~StackAllocator() = default;

    /// <summary>
    /// メモリプールを指定サイズで初期化します。
    /// </summary>
    void Initialize(size_t size);

    /// <summary>
    /// プールからメモリをアロケートします。
    /// </summary>
    void* Allocate(size_t size, size_t alignment = alignof(std::max_align_t));

    /// <summary>
    /// アロケーターのオフセットをリセットします。データ消去コストは O(1) です。
    /// </summary>
    void Reset();

    /// <summary>
    /// 現在の使用率（バイト数）を取得します。
    /// </summary>
    size_t GetUsedBytes() const { return offset_; }

    /// <summary>
    /// 全体の容量（バイト数）を取得します。
    /// </summary>
    size_t GetTotalBytes() const { return totalSize_; }

private:
    std::unique_ptr<uint8_t[]> pool_;
    size_t totalSize_ = 0;
    size_t offset_ = 0;
};
