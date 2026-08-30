#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <vector>

namespace BaziruEngine::Core {

/**
 * @brief 定数バッファアロケーション結果構造体
 */
struct ConstantBufferAllocation {
    D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = 0; // GPU側仮想アドレス (RootCBVに直接バインド可能)
    void* cpuAddress = nullptr;               // CPU側書き込みアドレス
    size_t size = 0;                          // アロケートサイズ (256Bアライメント済み)
    uint32_t offset = 0;                      // リングバッファ先頭からのバイトオフセット
};

/**
 * @brief 定数バッファ (CBV) 用リングバッファアロケータ
 *
 * 毎フレームの committed resource 生成を廃止し、大きな1枚の Upload Heap バッファを
 * 256バイト境界でリング状に切り出して超高速 ($O(1)$) に定数データを供給します。
 */
class ConstantBufferRingAllocator {
public:
    ConstantBufferRingAllocator() = default;
    ~ConstantBufferRingAllocator();

    /**
     * @brief リングバッファアロケータの初期化
     * @param device D3D12デバイスへのポインタ
     * @param totalCapacity 全容量 (デフォルト 2MB = 2,097,152 Bytes)
     */
    void Initialize(ID3D12Device* device, size_t totalCapacity = 2 * 1024 * 1024);

    /**
     * @brief 終了処理
     */
    void Finalize();

    /**
     * @brief 新しいフレームの開始時に呼び出し、アロケートカウンタをリセット
     */
    void NewFrame();

    /**
     * @brief 定数バッファ領域をアロケート (256バイト境界アライメント処理を含む)
     * @param sizeInBytes 必要なデータサイズ
     * @param initialData 初期コピーするデータのポインタ (任意)
     * @return アロケート結果
     */
    ConstantBufferAllocation Allocate(size_t sizeInBytes, const void* initialData = nullptr);

    /**
     * @brief リングバッファリソースを取得
     */
    ID3D12Resource* GetResource() const { return ringBuffer_.Get(); }

    /**
     * @brief 現在のフレームでの使用率 (%) を取得 (プロファイラUI表示用)
     */
    float GetCurrentFrameUsageRatio() const;

    /**
     * @brief アロケータの統計情報を取得
     */
    size_t GetTotalCapacity() const { return totalCapacity_; }
    size_t GetAllocatedThisFrame() const { return allocatedThisFrame_; }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> ringBuffer_;
    uint8_t* mappedCpuAddress_ = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS gpuBaseAddress_ = 0;

    size_t totalCapacity_ = 0;
    size_t currentOffset_ = 0;
    size_t allocatedThisFrame_ = 0;

    std::mutex allocMutex_;
};

} // namespace BaziruEngine::Core
