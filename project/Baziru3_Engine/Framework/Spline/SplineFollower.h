#pragma once
#include "SplinePath.h"
#include <functional>

namespace BaziruEngine
{
    /// <summary>
    /// パス追従の再生モード
    /// </summary>
    enum class SplinePlayMode
    {
        Once,      // 終点に到達したら停止
        Loop,      // 始点に戻って無限ループ
        PingPong   // 終点に到達したら折り返して往復
    };

    /// <summary>
    /// スプライン曲線（レール）追従移動コントローラー
    /// 指定した SplinePath に沿って等速/非等速で移動し、座標や回転（オイラー角）を更新します。
    /// </summary>
    class SplineFollower
    {
    public:
        SplineFollower();
        explicit SplineFollower(const SplinePath* path, float speed = 5.0f, SplinePlayMode mode = SplinePlayMode::Loop);
        ~SplineFollower() = default;

        /// <summary>
        /// 追従対象のスプラインパスを設定
        /// </summary>
        void SetPath(const SplinePath* path);
        const SplinePath* GetPath() const { return path_; }

        /// <summary>
        /// 移動速度（units / 秒）を設定
        /// </summary>
        void SetSpeed(float speed) { speed_ = speed; }
        float GetSpeed() const { return speed_; }

        /// <summary>
        /// 再生モード（Once / Loop / PingPong）を設定
        /// </summary>
        void SetPlayMode(SplinePlayMode mode) { playMode_ = mode; }
        SplinePlayMode GetPlayMode() const { return playMode_; }

        /// <summary>
        /// 進行方向に向かって回転（オイラー角）を自動計算するかどうか
        /// </summary>
        void SetAlignRotation(bool enable) { alignRotation_ = enable; }
        bool IsAlignRotation() const { return alignRotation_; }

        /// <summary>
        /// 毎フレームの移動更新処理
        /// </summary>
        /// <param name="deltaTime">経過時間（秒）</param>
        void Update(float deltaTime);

        // --- 再生制御 ---

        void Play() { isPlaying_ = true; isPaused_ = false; }
        void Pause() { isPaused_ = true; }
        void Resume() { isPaused_ = false; }
        void Stop();
        void Restart();

        bool IsPlaying() const { return isPlaying_ && !isPaused_; }
        bool IsFinished() const { return isFinished_; }

        /// <summary>
        /// 進行度（0.0 ~ 1.0）を直接設定
        /// </summary>
        void SetProgress(float normalizedT);

        /// <summary>
        /// 移動距離（0.0 ~ TotalLength）を直接設定
        /// </summary>
        void SetDistance(float distance);

        // --- 現在値の取得 ---

        const Vector3& GetPosition() const { return currentSample_.position; }
        const Vector3& GetTangent() const { return currentSample_.tangent; }
        const Vector3& GetNormal() const { return currentSample_.normal; }
        const Vector3& GetRotation() const { return currentRotationEuler_; }
        float GetCurrentDistance() const { return currentDistance_; }
        float GetProgress() const;
        uint32_t GetLapCount() const { return lapCount_; }

        // --- イベントコールバック ---

        /// <summary>
        /// 移動終了（Once時のみ）に呼ばれるコールバック
        /// </summary>
        void SetOnFinishedCallback(std::function<void()> callback) { onFinished_ = callback; }

        /// <summary>
        /// 周回時（Loop時またはPingPong往復時）に呼ばれるコールバック
        /// </summary>
        void SetOnLapCallback(std::function<void(uint32_t)> callback) { onLap_ = callback; }

    private:
        void UpdateTransform();
        Vector3 CalculateRotationEuler(const Vector3& forward, const Vector3& up) const;

    private:
        const SplinePath* path_ = nullptr;
        float speed_ = 5.0f;
        SplinePlayMode playMode_ = SplinePlayMode::Loop;
        bool alignRotation_ = true;

        float currentDistance_ = 0.0f;
        int moveDirection_ = 1; // 1: 前進, -1: 後退 (PingPong用)
        uint32_t lapCount_ = 0;

        bool isPlaying_ = true;
        bool isPaused_ = false;
        bool isFinished_ = false;

        SplineSample currentSample_;
        Vector3 currentRotationEuler_{ 0.0f, 0.0f, 0.0f }; // Yaw, Pitch, Roll (radian)

        std::function<void()> onFinished_;
        std::function<void(uint32_t)> onLap_;
    };
}
