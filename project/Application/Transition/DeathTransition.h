#pragma once

#include <string>
#include <memory>
#include "Vector.h"
#include "RenderContext.h"

class Sprite;
class SpriteCom;
class WindowAPI;
class OffScreenRendering;

/// <summary>
/// 軍用メディバック・ヘビーシャッタートランジション (DeathTransition)
/// 
/// 死亡時（GamePlayScene -> GameOverScene）専用の超シネマティック軍用画面切り替え演出。
/// 上下から重厚な装甲ブラストシャッターがクランプ密閉し、中央で心電図（ECG）がフラットライン化。
/// シーン切り替え完了後、救難ヘリのカーゴドアのようにシャッターが上下に開き、
/// 夜間救難作戦の3Dジオラマ（GameOverScene）へとシームレスに直結します。
/// </summary>
class DeathTransition {
public:
    static constexpr const char* kDefaultTargetScene  = "GAMEOVER";
    static constexpr float       kDefaultDuration     = 1.70f;
    static constexpr float       kDefaultScreenWidth  = 1280.0f;
    static constexpr float       kDefaultScreenHeight = 720.0f;

    static DeathTransition* GetInstance();

    /// @brief 初期化（スプライトリソースの生成）
    void Initialize(SpriteCom* spriteCom, WindowAPI* windowAPI);

    /// @brief 終了処理
    void Finalize();

    /// @brief 毎フレームの更新
    void Update(float deltaTime);

    /// @brief 3D/スプライト描画
    void Draw(const RenderContext& ctx);

    /// @brief 2D DirectX 12 ネイティブHUD（心電図フラットライン）描画
    void DrawUI();

    /// <summary>
    /// 死亡専用トランジションを開始してGameOverSceneへ切り替える
    /// </summary>
    void StartTransition(const std::string& nextSceneName = kDefaultTargetScene, float duration = kDefaultDuration);

    [[nodiscard]] bool IsTransitioning() const { return isActive_; }
    [[nodiscard]] float GetProgress() const { return progress_; }

    void SetOffScreenRendering(OffScreenRendering* offscreen) { offscreen_ = offscreen; }

private:
    DeathTransition() = default;
    ~DeathTransition() = default;
    DeathTransition(const DeathTransition&) = delete;
    DeathTransition& operator=(const DeathTransition&) = delete;

    void RefreshViewportMetrics();
    void ApplyResponsiveSpriteSizes();

private:
    SpriteCom* spriteCom_ = nullptr;
    WindowAPI* windowAPI_ = nullptr;
    OffScreenRendering* offscreen_ = nullptr;
    int savedPostEffect_ = -1;

    // 上部・下部 重装甲シャッタープレート
    std::unique_ptr<Sprite> upperShutter_;
    std::unique_ptr<Sprite> lowerShutter_;

    // シャッター先端の装甲リップトリム（ガンメタルスチール）
    std::unique_ptr<Sprite> upperLip_;
    std::unique_ptr<Sprite> lowerLip_;

    // ハザード警告ライン（ミリタリーアンバーゴールド）
    std::unique_ptr<Sprite> upperHazardStripe_;
    std::unique_ptr<Sprite> lowerHazardStripe_;

    // 画面中央継ぎ目の密閉スリットライン
    std::unique_ptr<Sprite> centerSeamBar_;

    // 完全密閉時の暗黒セーフティバックドロップ
    std::unique_ptr<Sprite> safetyBackdrop_;

    bool isActive_ = false;
    bool hasSwitched_ = false;
    bool slamSoundPlayed_ = false;
    bool openSoundPlayed_ = false;
    float timer_ = 0.0f;
    float duration_ = kDefaultDuration;
    float progress_ = 0.0f;

    std::string pendingSceneName_ = kDefaultTargetScene;

    float screenWidth_ = kDefaultScreenWidth;
    float screenHeight_ = kDefaultScreenHeight;
};
