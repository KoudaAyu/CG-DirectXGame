#pragma once

#include <string>
#include <memory>
#include <vector>
#include "Vector.h"
#include "RenderContext.h"

class Sprite;
class SpriteCom;
class WindowAPI;
class OffScreenRendering;

/// @brief トランジションのビジュアルテーマ
enum class TransitionTheme {
    TitleTactical,    // タイトル調和: ダックアンバーゴールド × シグナルイエロー × タクティカルブラック × クリーンホワイト
    DuckovTactical,   // Duckov風: タクティカルブラック × シグナルイエロー × タクティカルオレンジ
    TororoPastel,     // 猫麦とろろ風: パステルスカイブルー × ピュアホワイト × パステルピンク
    CyberNeon,        // サイバーネオン: ネオンマゼンタ × サイバーシアン × エレクトリックイエロー
    Monochrome,       // ミニマルシック: ディープブラック × シルバーホワイト
};

/**
 * @brief アプリケーション層完結型スティンガートランジション（画面切り替え演出）
 * 
 * 猫麦とろろさんの配信で見られるような、シンプルでポップ・爽快な多重斜めストライプが
 * 画面を駆け抜けて瞬時にシーンを切り替えるハイクオリティなトランジションを提供します。
 * エンジン層（Baziru3_Engine）を一切変更せず、アプリケーション層だけで完結して動作します。
 */
class StingerTransition {
public:
    static StingerTransition* GetInstance();

    /// @brief トランジション初期化（スプライトリソースの生成）
    void Initialize(SpriteCom* spriteCom, WindowAPI* windowAPI);

    /// @brief 終了処理
    void Finalize();

    /// @brief 毎フレームの更新（アニメーション進行と中間地点でのSceneManagerシーン切り替え）
    void Update(float deltaTime);

    /// @brief 描画（スプライトパイプライン最前面に描画）
    void Draw(const RenderContext& ctx);

    /// @brief トランジションを開始して指定シーンへ切り替える
    /// @param nextSceneName 切り替え先シーン名（例: "GAMEPLAY", "TITLE" など）
    /// @param duration トランジション全体の秒数（デフォルト 1.8 秒）
    /// @param theme カラーテーマ（デフォルトはタイトル調和タクティカル）
    void StartTransition(const std::string& nextSceneName, float duration = 1.80f,
                         TransitionTheme theme = TransitionTheme::TitleTactical);

    /// @brief 現在トランジション中かどうか
    [[nodiscard]] bool IsTransitioning() const { return isActive_; }

    /// @brief トランジション進行度を取得（0.0f 〜 1.0f）
    [[nodiscard]] float GetProgress() const { return progress_; }

    /// @brief カラーテーマを変更
    void SetTheme(TransitionTheme theme);

    /// @brief オフスクリーンレンダリングの参照を設定（トランジション中のアウトラインポストプロセス抑制用）
    void SetOffScreenRendering(OffScreenRendering* offscreen) { offscreen_ = offscreen; }

private:
    StingerTransition() = default;
    ~StingerTransition() = default;
    StingerTransition(const StingerTransition&) = delete;
    StingerTransition& operator=(const StingerTransition&) = delete;

    struct StripeLayer {
        std::unique_ptr<Sprite> sprite;
        Vector4 color;
        Vector2 size;
        float offsetX = 0.0f; // メイン覆い帯からの相対Xオフセット
    };

    void ApplyThemeColors();
    static float StingerInCurve(float t);
    static float StingerOutCurve(float t);

private:
    SpriteCom* spriteCom_ = nullptr;
    WindowAPI* windowAPI_ = nullptr;
    OffScreenRendering* offscreen_ = nullptr;
    int savedPostEffect_ = -1;

    // 8層の多重ストライプ
    std::vector<StripeLayer> stripes_;

    // 全画面完全密閉セーフティバックドロップ（回転なし水平プレートで四隅のチラ見えを100%遮断）
    std::unique_ptr<Sprite> safetyBackdrop_;

    // シネマティック・レターボックス（画面上下のシネマ帯）
    std::unique_ptr<Sprite> topLetterbox_;
    std::unique_ptr<Sprite> bottomLetterbox_;

    // 中央アイキャッチ・エンブレム
    std::unique_ptr<Sprite> centerFrameSprite_;   // 外枠回転ダイヤモンド
    std::unique_ptr<Sprite> centerCoreSprite_;    // 内側高速回転コアダイヤモンド
    std::unique_ptr<Sprite> centerGlintBar_;      // 水平閃光ライトスリットバー
    std::unique_ptr<Sprite> centerFlashSprite_;   // シーン切り替えインパクト白フラッシュ

    bool isActive_ = false;
    bool hasSwitched_ = false;
    float timer_ = 0.0f;
    float duration_ = 1.80f;
    float progress_ = 0.0f;
    float postSwitchTimer_ = 0.0f; // 切り替え後の安全フレームタイマー

    std::string pendingSceneName_;
    TransitionTheme theme_ = TransitionTheme::TitleTactical;

    float screenWidth_ = 1280.0f;
    float screenHeight_ = 720.0f;
    float travelDistance_ = 3200.0f; // 画面外から中心までの移動距離
    float stripeAngle_ = -0.42f;     // 斜めストライプの傾き角度（約 -24度）
};

/// @brief アプリケーション全体から呼び出せる簡易シーン遷移ヘルパー
namespace SceneTransition {
    inline void Change(const std::string& sceneName, float duration = 1.80f,
                       TransitionTheme theme = TransitionTheme::TitleTactical) {
        StingerTransition::GetInstance()->StartTransition(sceneName, duration, theme);
    }
}
