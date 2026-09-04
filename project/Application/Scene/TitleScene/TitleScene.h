#pragma once

#include "BaseScene.h"
#include "Sprite.h"
#include "Vector.h"

#include <memory>

class KeyInput;
class MouseInput;
struct SceneRenderRequests;

/// <summary>
/// タイトルシーン
/// UI は仮想解像度 1280x720 を基準に配置している（WindowAPI::kClientWidth/Height）
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
    /// <summary>ボタン1個分のデータ</summary>
    struct Button
    {
        std::unique_ptr<Sprite> base;   // 通常時の画像
        std::unique_ptr<Sprite> light;  // 点灯時の画像（base の上に重ねて不透明度で入れ替える）
        Vector2 center{};               // 中心座標（ピクセル）
        Vector2 size{};                 // 基本サイズ（ピクセル）
        float scale = 1.0f;             // 現在の拡大率（ホバーでふくらむ）
        float lightRate = 0.0f;         // 点灯画像の不透明度 0..1
        bool isHovered = false;
    };

    void CreateSprites();
    void UpdateFadeIn(float deltaTime);
    void UpdateTitleLogo(float deltaTime);
    void UpdateButtons(float deltaTime);
    void DecideMenu(MenuItem item);

    /// <summary>仮想解像度(1280x720)上のマウス座標を取得する</summary>
    Vector2 GetMousePositionOnUI() const;

    /// <summary>矩形の内側に点があるか（中心・サイズ指定）</summary>
    static bool IsInside(const Vector2& point, const Vector2& center, const Vector2& size);

    /// <summary>スプライトに中心座標・サイズ・不透明度をまとめて反映する</summary>
    static void ApplySprite(Sprite* sprite, const Vector2& center, const Vector2& size, float alpha);

    /// <summary>スプライトを生成する（失敗しても nullptr が返るだけで落ちない）</summary>
    static std::unique_ptr<Sprite> MakeSprite(const char* texturePath, const Vector2& size);

private:
    KeyInput* input_ = nullptr;
    MouseInput* mouse_ = nullptr;

    std::unique_ptr<Sprite> titleLogo_;
    Button buttons_[static_cast<int>(MenuItem::Count)]{};

    float sceneTime_ = 0.0f;   // シーン開始からの経過秒
    float fadeAlpha_ = 0.0f;   // UI 全体のフェードイン係数 0..1
    float logoOffsetY_ = 0.0f; // タイトルロゴの上下ゆれ（ピクセル）

    bool isManualRequested_ = false;
    bool isExitRequested_ = false;
};
