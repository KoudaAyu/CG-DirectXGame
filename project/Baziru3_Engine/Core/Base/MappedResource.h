#pragma once

#include <wrl.h>
#include <d3d12.h>

namespace Baziru3 {


template<typename T>
class PersistentMap {
public:
    PersistentMap() = default;
    explicit PersistentMap(Microsoft::WRL::ComPtr<ID3D12Resource> res) { reset(std::move(res)); }
    ~PersistentMap() { release(); }
    PersistentMap(const PersistentMap&) = delete;
    PersistentMap& operator=(const PersistentMap&) = delete;
    PersistentMap(PersistentMap&& other) noexcept { resource_ = std::move(other.resource_); ptr_ = other.ptr_; other.ptr_ = nullptr; }
    PersistentMap& operator=(PersistentMap&& other) noexcept { if (this != &other) { release(); resource_ = std::move(other.resource_); ptr_ = other.ptr_; other.ptr_ = nullptr; } return *this; }

    void reset(Microsoft::WRL::ComPtr<ID3D12Resource> res) {
        release();
        resource_ = std::move(res);
        if (resource_) resource_->Map(0, nullptr, reinterpret_cast<void**>(&ptr_));
    }

    void releaseWithWrittenRange(size_t bytes) {
        if (resource_) {
            D3D12_RANGE range{}; range.Begin = 0; range.End = static_cast<SIZE_T>(bytes);
            resource_->Unmap(0, &range);
            resource_.Reset(); ptr_ = nullptr;
        }
    }

    void release() {
        if (resource_) { resource_->Unmap(0, nullptr); resource_.Reset(); ptr_ = nullptr; }
    }

    T* get() const { return ptr_; }
    explicit operator bool() const { return ptr_ != nullptr; }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
    T* ptr_ = nullptr;
};


template<typename T>
class ScopedMap {
public:
    explicit ScopedMap(Microsoft::WRL::ComPtr<ID3D12Resource> res) : resource_(std::move(res)) { if (resource_) resource_->Map(0, nullptr, reinterpret_cast<void**>(&ptr_)); }
    ~ScopedMap() { if (resource_) resource_->Unmap(0, nullptr); }
    ScopedMap(const ScopedMap&) = delete; ScopedMap& operator=(const ScopedMap&) = delete;
    T* get() const { return ptr_; }
    explicit operator bool() const { return ptr_ != nullptr; }
private:
    Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
    T* ptr_ = nullptr;
};

} 
