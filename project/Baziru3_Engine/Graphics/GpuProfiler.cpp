#include "GpuProfiler.h"
#include <cassert>
#include <d3dx12.h>

/// <summary>
/// シングルトンのインスタンスを取得します。
/// </summary>
GpuProfiler* GpuProfiler::GetInstance()
{
    static GpuProfiler instance;
    return &instance;
}

/// <summary>
/// プロファイラの初期化処理。
/// タイムスタンプ測定に必要なクエリヒープと、CPUへのデータ転送用リードバックバッファを生成します。
/// </summary>
void GpuProfiler::Initialize(ID3D12Device* device, ID3D12CommandQueue* commandQueue, uint32_t maxProfiles)
{
    std::lock_guard<std::mutex> lock(mutex_);
    maxProfiles_ = maxProfiles;
    // 1つの計測パスにつき、開始と終了で2つのタイムスタンプを使用します
    queryCount_ = maxProfiles_ * 2;

    // 1秒あたりのGPU内クロックカウンタの増加周波数を取得します
    // これにより、クエリの差分値をミリ秒（ms）に正確に換算できます
    HRESULT hr = commandQueue->GetTimestampFrequency(&gpuFrequency_);
    assert(SUCCEEDED(hr));

    // --- 1. クエリヒープの作成 ---
    D3D12_QUERY_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP; // タイムスタンプ測定用ヒープ
    heapDesc.Count = queryCount_;                    // 格納できるタイムスタンプの最大数
    heapDesc.NodeMask = 0;                           // シングルアダプタの場合は0を設定

    hr = device->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(&queryHeap_));
    assert(SUCCEEDED(hr));

    // --- 2. リードバックバッファの作成 ---
    // GPUで測定されたタイムスタンプ（64bit整数値）をCPUへ転送するためのバッファサイズを算出
    uint64_t bufferSize = queryCount_ * sizeof(uint64_t);
    
    D3D12_HEAP_PROPERTIES readbackHeapProps = {};
    readbackHeapProps.Type = D3D12_HEAP_TYPE_READBACK; // CPU読み取り専用のヒープ（READBACK）

    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; // 一次元バッファ
    bufferDesc.Width = bufferSize;                          // バッファ長
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    // バッファリソースを作成（GPUによるコピー先となるため、初期状態は COPY_DEST）
    hr = device->CreateCommittedResource(
        &readbackHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&readbackBuffer_)
    );
    assert(SUCCEEDED(hr));
}

/// <summary>
/// 終了処理。リソースをクリアしてメモリリークを防ぎます。
/// </summary>
void GpuProfiler::Finalize()
{
    std::lock_guard<std::mutex> lock(mutex_);
    readbackBuffer_.Reset();
    queryHeap_.Reset();
    profiles_.clear();
    profileOrder_.clear();
    results_.clear();
}

/// <summary>
/// フレームの開始処理。
/// 計測パスのアクティブフラグをリセットし、新しいフレームのコマンドを記録する準備を行います。
/// </summary>
void GpuProfiler::BeginFrame(ID3D12GraphicsCommandList* commandList)
{
    std::lock_guard<std::mutex> lock(mutex_);
    assert(!isFrameActive_);
    isFrameActive_ = true;

    // 前フレームで登録された計測パスのアクティブ状態を一旦クリア
    for (auto& pair : profiles_)
    {
        pair.second.active = false;
    }
}

/// <summary>
/// フレームの終了処理。
/// 計測したタイムスタンプデータを、リードバックバッファへコピー（Resolve）するコマンドを記録します。
/// </summary>
void GpuProfiler::EndFrame(ID3D12GraphicsCommandList* commandList)
{
    std::lock_guard<std::mutex> lock(mutex_);
    assert(isFrameActive_);
    isFrameActive_ = false;

    if (profileOrder_.empty()) return;

    // GPU上のクエリヒープに保存されているバイナリタイムスタンプデータを、
    // CPUからアクセス可能なリードバックバッファ（readbackBuffer_）へ書き出すコマンドを積みます。
    commandList->ResolveQueryData(
        queryHeap_.Get(),
        D3D12_QUERY_TYPE_TIMESTAMP,
        0,
        static_cast<UINT>(profileOrder_.size() * 2), // 今回のフレームで測定した総スロット数
        readbackBuffer_.Get(),
        0
    );
}

/// <summary>
/// 指定された名前の描画パスの開始地点にタイムスタンプクエリを記録します。
/// </summary>
void GpuProfiler::BeginProfile(ID3D12GraphicsCommandList* commandList, const std::string& name)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isFrameActive_) return;

    // マップから計測パス情報を検索し、未登録の場合は新規登録します
    auto it = profiles_.find(name);
    if (it == profiles_.end())
    {
        // 計測上限に達している場合は何もしない
        if (profileOrder_.size() >= maxProfiles_) return;

        uint32_t index = static_cast<uint32_t>(profileOrder_.size());
        ProfileData data;
        data.name = name;
        data.queryIndexStart = index * 2;     // 開始タイムスタンプ用のインデックス
        data.queryIndexEnd = index * 2 + 1;   // 終了タイムスタンプ用のインデックス
        data.active = true;

        profiles_[name] = data;
        profileOrder_.push_back(name);
        it = profiles_.find(name);
    }
    else
    {
        it->second.active = true;
    }

    // パスの開始位置に現在のGPUタイムスタンプを書き込むコマンドを記録
    commandList->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, it->second.queryIndexStart);
}

/// <summary>
/// 指定された名前の描画パスの終了地点にタイムスタンプクエリを記録します。
/// </summary>
void GpuProfiler::EndProfile(ID3D12GraphicsCommandList* commandList, const std::string& name)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isFrameActive_) return;

    auto it = profiles_.find(name);
    if (it != profiles_.end() && it->second.active)
    {
        // パスの終了位置に現在のGPUタイムスタンプを書き込むコマンドを記録
        commandList->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, it->second.queryIndexEnd);
    }
}

/// <summary>
/// 前フレームのタイムスタンプ結果をリードバックバッファからCPU側にマップ（読み出し）し、
/// GPU周波数に基づいて処理時間（ミリ秒）へ計算・換算します。
/// </summary>
void GpuProfiler::ResolveResults()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (profileOrder_.empty()) return;

    // CPUからリードバックバッファをマップしてアクセス可能にします
    D3D12_RANGE readRange = { 0, profileOrder_.size() * 2 * sizeof(uint64_t) };
    uint64_t* queryData = nullptr;
    HRESULT hr = readbackBuffer_->Map(0, &readRange, reinterpret_cast<void**>(&queryData));
    if (FAILED(hr)) return;

    results_.clear();
    results_.reserve(profileOrder_.size());

    // 各登録パスのタイムスタンプ差分から処理時間（ms）を算出します
    for (size_t i = 0; i < profileOrder_.size(); ++i)
    {
        const std::string& name = profileOrder_[i];
        const auto& data = profiles_[name];

        // このフレームで測定されていないパスはスキップ
        if (!data.active) continue;

        uint64_t startTime = queryData[data.queryIndexStart];
        uint64_t endTime = queryData[data.queryIndexEnd];

        // 計測が正常に行われたことを確認
        if (endTime >= startTime)
        {
            uint64_t duration = endTime - startTime;
            // 処理時間（ミリ秒） = (経過クロック数 * 1000) / 1秒あたりの周波数
            float timeMs = (static_cast<float>(duration) * 1000.0f) / static_cast<float>(gpuFrequency_);
            results_.push_back({ name, timeMs });
        }
        else
        {
            results_.push_back({ name, 0.0f });
        }
    }

    // マップを解除（CPU側での読み込み終了を通知、書き込みを行わないため writeRange は 0 を設定）
    D3D12_RANGE writeRange = { 0, 0 };
    readbackBuffer_->Unmap(0, &writeRange);
}
