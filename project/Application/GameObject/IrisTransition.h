#pragma once

#include "Vector.h"
#include "Baziru3_Engine/Core/Base/DirectXCom.h"
#include <d3d12.h>
#include <wrl.h>

/**
 * @brief 円形アイリスアウト／イン（Iris Out / In）画面トランジション管理クラス
 * @note エンジンに依存せずアプリケーション層で完結して全画面円形マスクを描画
 */
class IrisTransition
{
public:
    enum class State
    {
        Idle,       // 非アクティブ（通常表示、画面全体が開いた状態）
        IrisOut,    // 円が小さくなって暗転していく（画面遷移前）
        Blackout,   // 完全に暗転した状態（シーン切り替え待機）
        IrisIn,     // 円が大きくなって画面が開いていく（画面遷移後）
    };

    struct IrisParamsCPU
    {
        Vector2 center{ 0.5f, 0.5f }; // 円の中心（UV空間: 0.0〜1.0）
        float radius = 1.5f;          // 円の半径（UV空間）
        float aspectRatio = 16.0f / 9.0f; // 画面アスペクト比
        float feather = 0.015f;       // 境界ぼかし幅
        float pad[3]{};
    };

    static IrisTransition* GetInstance();

    void Initialize(DirectXCom* dxCommon);
    void Finalize();

    /**
     * @brief アイリスアウト（円形暗転）を開始
     * @param duration 暗転にかかる時間（秒）
     * @param centerUV 円の中心となる画面UV座標 (0.0〜1.0)
     */
    void StartIrisOut(float duration = 1.0f, const Vector2& centerUV = { 0.5f, 0.5f });

    /**
     * @brief アイリスイン（円形明転）を開始
     * @param duration 明転にかかる時間（秒）
     * @param centerUV 円の中心となる画面UV座標 (0.0〜1.0)
     */
    void StartIrisIn(float duration = 1.0f, const Vector2& centerUV = { 0.5f, 0.5f });

    void Update(float deltaTime);
    void Draw(ID3D12GraphicsCommandList* commandList);

    State GetState() const { return state_; }
    bool IsActive() const { return state_ != State::Idle; }
    bool IsIrisOutComplete() const { return state_ == State::Blackout; }
    bool IsIrisInComplete() const { return state_ == State::Idle; }

    float GetProgress() const { return progress_; }

private:
    IrisTransition() = default;
    ~IrisTransition();
    IrisTransition(const IrisTransition&) = delete;
    IrisTransition& operator=(const IrisTransition&) = delete;

    void CreatePipelines(DirectXCom* dxCommon);

private:
    DirectXCom* dxCommon_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
    IrisParamsCPU* mappedBuffer_ = nullptr;

    State state_ = State::Idle;
    float timer_ = 0.0f;
    float duration_ = 1.0f;
    float progress_ = 0.0f; // 0.0 (全開) 〜 1.0 (全黒)
    Vector2 centerUV_{ 0.5f, 0.5f };
    float maxRadius_ = 1.5f; // 画面全体（四隅）を覆う十分な半径
};
