#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include <vector>

class DirectXCom;

class ConstantBufferAllocator {
public:
    struct Allocation {
        void* cpuAddress = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = 0;
    };

    ConstantBufferAllocator(DirectXCom* dxCommon);
    ~ConstantBufferAllocator() = default;

    void Initialize(size_t bufferSize = 8 * 1024 * 1024); // デフォルト 8MB
    void Finalize();

    // 毎フレームの開始時に呼び出し、現在のフレームの書き込み領域の同期・更新を行う
    void BeginFrame();

    // 定数バッファ用の領域を切り出す
    Allocation Allocate(size_t size);

private:
    // アドレスアラインメント関数 (D3D12 の定数バッファは 256 バイトアラインが必要)
    static size_t AlignUp(size_t size, size_t alignment) {
        return (size + alignment - 1) & ~(alignment - 1);
    }

private:
    DirectXCom* dxCommon_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> bufferResource_ = nullptr;
    uint8_t* cpuStart_ = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS gpuStart_ = 0;

    size_t bufferSize_ = 0;
    size_t currentOffset_ = 0;

    // トリプルバッファリング構成
    static constexpr size_t kNumFrames = 3;
    size_t frameSize_ = 0;
    uint32_t currentFrameIndex_ = 0;
};
