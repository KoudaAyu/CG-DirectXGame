#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

/// <summary>
/// GPUタイムスタンプクエリを用いたレンダリングパスの負荷計測プロファイラクラス。
/// DirectX 12のQuery機能を利用し、各描画ステージの所要時間（ミリ秒）を正確に計測します。
/// </summary>
class GpuProfiler
{
public:
    /// <summary>
    /// シングルトンインスタンスを取得します。
    /// </summary>
    /// <returns>GpuProfilerの唯一のインスタンスへのポインタ</returns>
    static GpuProfiler* GetInstance();

    /// <summary>
    /// プロファイラの初期化を行います。
    /// クエリヒープの生成、リードバック用のリソース確保、およびGPU周波数の取得を行います。
    /// </summary>
    /// <param name="device">DirectX12デバイスへのポインタ</param>
    /// <param name="commandQueue">計測のベースとなるコマンドキュー（周波数取得用）</param>
    /// <param name="maxProfiles">最大計測パス数（デフォルトは16）</param>
    void Initialize(ID3D12Device* device, ID3D12CommandQueue* commandQueue, uint32_t maxProfiles = 16);

    /// <summary>
    /// リソースの解放を行います。
    /// </summary>
    void Finalize();

    /// <summary>
    /// フレームの開始を通知します。
    /// 計測状態をリセットし、新規計測の準備を行います。
    /// </summary>
    /// <param name="commandList">コマンドを記録するコマンドリスト</param>
    void BeginFrame(ID3D12GraphicsCommandList* commandList);

    /// <summary>
    /// フレームの終了を通知します。
    /// このフレーム内に記録されたタイムスタンプデータをリードバックバッファへ解決（Resolve）するコマンドを記録します。
    /// </summary>
    /// <param name="commandList">コマンドを記録するコマンドリスト</param>
    void EndFrame(ID3D12GraphicsCommandList* commandList);

    /// <summary>
    /// 指定された名前の描画パスの計測を開始します。
    /// </summary>
    /// <param name="commandList">コマンドを記録するコマンドリスト</param>
    /// <param name="name">計測対象パスの識別名（例: "Scene Draw"）</param>
    void BeginProfile(ID3D12GraphicsCommandList* commandList, const std::string& name);

    /// <summary>
    /// 指定された名前の描画パスの計測を終了します。
    /// </summary>
    /// <param name="commandList">コマンドを記録するコマンドリスト</param>
    /// <param name="name">計測対象パスの識別名（例: "Scene Draw"）</param>
    void EndProfile(ID3D12GraphicsCommandList* commandList, const std::string& name);

    /// <summary>
    /// GPUの処理完了後に呼び出し、前フレームのタイムスタンプデータをCPU側へ回収・解析します。
    /// </summary>
    void ResolveResults();

    /// <summary>
    /// 計測結果を格納する構造体
    /// </summary>
    struct ProfileResult
    {
        std::string name; // パス名
        float timeMs;     // 処理時間（ミリ秒）
    };

    /// <summary>
    /// 最新の計測結果一覧を取得します。
    /// </summary>
    /// <returns>計測結果構造体のリストへの参照</returns>
    const std::vector<ProfileResult>& GetResults() const { return results_; }

private:
    GpuProfiler() = default;
    ~GpuProfiler() = default;
    GpuProfiler(const GpuProfiler&) = delete;
    GpuProfiler& operator=(const GpuProfiler&) = delete;

    /// <summary>
    /// 個別の計測パス情報を保持する構造体
    /// </summary>
    struct ProfileData
    {
        std::string name;             // 計測名
        uint32_t queryIndexStart = 0; // 開始タイムスタンプのクエリインデックス
        uint32_t queryIndexEnd = 0;   // 終了タイムスタンプのクエリインデックス
        bool active = false;          // このフレームで有効に計測されたかどうか
    };

    // DirectX 12 関連リソース
    Microsoft::WRL::ComPtr<ID3D12QueryHeap> queryHeap_ = nullptr;     // タイムスタンプクエリを格納するヒープ
    Microsoft::WRL::ComPtr<ID3D12Resource> readbackBuffer_ = nullptr; // GPUからデータをCPUへ引き渡すためのバッファ
    
    uint64_t gpuFrequency_ = 0; // GPUのタイムスタンプ周波数（1秒あたりのカウント数）
    uint32_t maxProfiles_ = 0;  // 計測可能な最大パス数
    uint32_t queryCount_ = 0;   // クエリヒープ内の総スロット数 (maxProfiles * 2)

    std::unordered_map<std::string, ProfileData> profiles_; // 計測パス情報を名前で引くマップ
    std::vector<std::string> profileOrder_;                 // クエリインデックス順のパス名リスト
    std::vector<ProfileResult> results_;                    // CPUで回収・変換済みの最終計測データ

    bool isFrameActive_ = false; // フレーム計測が現在進行中かどうかのフラグ
    std::mutex mutex_;           // 排他制御用ミューテックス
};
