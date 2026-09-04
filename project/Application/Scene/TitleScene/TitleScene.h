#pragma once

#include "BaseScene.h"
#include "Sprite.h"
#include "Vector.h"

#include <cstdint>
#include <memory>
#include <vector>

class KeyInput;
class MouseInput;
struct SceneRenderRequests;

/// <summary>
/// タイトルシーン
/// UI は仮想解像度 1280x720 を基準に配置している（WindowAPI::kClientWidth/Height）
///
/// タイトルロゴは「1文字＝1スプライト」で構成していて、
/// 文字数・並びは TitleScene.cpp 冒頭の kTitleChars 配列を書き換えるだけで変わる。
/// （配列を1要素にすれば、従来どおり1枚絵のロゴとしても使える）
/// </summary>
class TitleScene : public BaseScene
{
public:
    /// <summary>メニュー項目</summary>
    enum class MenuItem
    {
        Start,
        Manual,
        End,

        Count, // 番兵（項目数）
    };

    void InitializeScene() override;
    void Finalize() override;
    void Update() override;
    void Draw(SceneRenderRequests& renderRequests) override;

    const char* GetSceneType() const { return "TITLE"; }

    // --- 遷移フラグ（中身は後で実装する用の仮置き） ---

    /// <summary>MANUAL が押されたか</summary>
    bool IsManualRequested() const { return isManualRequested_; }
    /// <summary>END が押されたか</summary>
    bool IsExitRequested() const { return isExitRequested_; }
    /// <summary>押された記録をクリアする</summary>
    void ClearRequests()
    {
        isManualRequested_ = false;
        isExitRequested_ = false;
    }

private:
    /// <summary>タイトルロゴの1文字分</summary>
    struct TitleLetter
    {
        std::unique_ptr<Sprite> sprite;
        float baseWidth = 0.0f;   // 元画像の基準幅（ピクセル）
        float offsetX = 0.0f;     // ロゴ中心からの相対X（レイアウト結果）
        float phase = 0.0f;       // ゆらぎの位相（文字ごとにバラす）
        float speedScale = 1.0f;  // ゆらぎ速度の個体差
        float popDelay = 0.0f;    // 登場のディレイ（秒）
    };

    /// <summary>ボタン1個分のデータ</summary>
    struct Button
    {
        std::unique_ptr<Sprite> base;   // 通常時の画像
        std::unique_ptr<Sprite> light;  // 点灯時の画像（base の上に重ねて不透明度で入れ替える）
        Vector2 center{};               // 中心座標（ピクセル）
        Vector2 size{};                 // 基本サイズ（ピクセル）
        float popDelay = 0.0f;          // 登場のディレイ（秒）
        float hoverRate = 0.0f;         // ホバーの進行度 0..1
        float scale = 1.0f;             // 現在の拡大率（表示用）
        float lightRate = 0.0f;         // 点灯画像の不透明度 0..1
        bool isHovered = false;
    };

    void CreateTitleLetters();
    void CreateButtons();
    void LayoutTitleLetters();

    void UpdateTitleLetters(float deltaTime);
    void UpdateButtons(float deltaTime);
    void UpdateFadeIn(float deltaTime);
    void DecideMenu(MenuItem item);

    void ResetTuningToDefault();

#ifdef USE_IMGUI
    void DrawDebugUI();
#endif

    /// <summary>仮想解像度(1280x720)上のマウス座標を取得する</summary>
    Vector2 GetMousePositionOnUI() const;

    /// <summary>矩形の内側に点があるか（中心・サイズ指定）</summary>
    static bool IsInside(const Vector2& point, const Vector2& center, const Vector2& size);

    /// <summary>スプライトに中心座標・サイズ・不透明度・回転をまとめて反映する</summary>
    static void ApplySprite(Sprite* sprite, const Vector2& center, const Vector2& size,
                            float alpha, float rotation = 0.0f);

    /// <summary>スプライトを生成する（失敗しても nullptr が返るだけで落ちない）</summary>
    static std::unique_ptr<Sprite> MakeSprite(const char* texturePath, const Vector2& size,
                                              const Vector2& anchorPoint);

private:
    KeyInput* input_ = nullptr;
    MouseInput* mouse_ = nullptr;

    std::vector<TitleLetter> titleLetters_;
    Button buttons_[static_cast<int>(MenuItem::Count)]{};

    float sceneTime_ = 0.0f; // シーン開始からの経過秒
    float fadeAlpha_ = 0.0f; // UI 全体のフェードイン係数 0..1

    // --- 調整用パラメータ（ImGui からいじれる。既定値は .cpp の定数） ---
    Vector2 logoCenter_{};      // ロゴ全体の中心座標
    float logoScale_ = 1.0f;    // ロゴ全体の拡大率
    float charHeight_ = 0.0f;   // 1文字の基準の高さ
    float charSpacing_ = 0.0f;  // 文字間隔
    float bobAmplitude_ = 0.0f; // 上下ゆれの幅
    float bobSpeed_ = 0.0f;     // 上下ゆれの速さ
    float jiggleAmount_ = 0.0f; // グミ変形の強さ
    float jiggleSpeed_ = 0.0f;  // グミ変形の速さ
    float wobbleDegrees_ = 0.0f;// 傾きゆれの角度（度）

    bool isManualRequested_ = false;
    bool isExitRequested_ = false;
};
