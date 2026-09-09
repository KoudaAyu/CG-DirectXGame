#pragma once

#include <memory>

#include <d3d12.h>

#include "Sprite.h"
#include "Baziru3_Engine/Core/Base/Vector.h"

/**
 * @brief ボスのHPバー（画面下）
 *
 * バニラスプライトだけで作ってある。画像は用意しなくてよい:
 * `DirectXCom::LoadTexture()` は読み込みに失敗すると 4x4 の白テクスチャを返すので、
 * `SetColor()` だけで色が決まる。あとから画像を置けばそのまま使われる。
 *
 * 演出:
 *   - 出るときは横幅 0 から **にゅーーっと伸びる**（EaseOutBack で少し行き過ぎて戻る）
 *   - 消えるときは逆再生で縮む
 *   - ゲージの色は 満タン=緑 -> 黄 -> ゼロに近づくほど赤
 *   - 減った瞬間は、白い「遅れて減るバー」が少し遅れて追いつく（ダメージが見える）
 *
 * @note 仮想解像度 1280x720 基準。Sprite の PSO はデプス無効なので、
 *       3D の後に描けば必ず手前に来る。
 */
class BossHpBar
{
public:
    BossHpBar() = default;
    ~BossHpBar();

    BossHpBar(const BossHpBar&) = delete;
    BossHpBar& operator=(const BossHpBar&) = delete;

    void Initialize();
    void Finalize();

    /// @brief バーを出す（登場アニメーションが最初から走る）
    void Show();

    /// @brief バーを引っ込める
    void Hide();

    bool IsVisible() const { return isVisible_; }

    /// @brief 表示するHP比率 (0..1)
    void SetRatio(float ratio) { targetRatio_ = ratio; }

    /**
     * @brief 更新
     * @param deltaTime デルタタイム
     * @note Show() 直後は appear_ が 0 から 1 へ動き、それが横幅の倍率になる
     */
    void Update(float deltaTime);

    void Draw(ID3D12GraphicsCommandList* commandList);
    void DrawImGui();

private:
    /// @brief 満タン緑 -> 黄 -> 赤 のグラデーション
    static Vector4 RatioToColor(float ratio);

    static std::unique_ptr<Sprite> MakeSprite(const char* texturePath);
    static void ApplyBar(Sprite* sprite, float leftX, float centerY, float width, float height,
                         const Vector4& color);

private:
    std::unique_ptr<Sprite> frame_;   //!< 外枠（少しだけ大きい暗い板）
    std::unique_ptr<Sprite> back_;    //!< 空っぽの溝
    std::unique_ptr<Sprite> delayed_; //!< 遅れて減る白いバー
    std::unique_ptr<Sprite> fill_;    //!< 本体のゲージ

    bool isVisible_ = false;
    float appear_ = 0.0f;        //!< 0 = 隠れている / 1 = 出きっている
    float targetRatio_ = 1.0f;   //!< 目標のHP比率
    float shownRatio_ = 1.0f;    //!< 実際に描いている比率（少し遅れて追う）
    float delayedRatio_ = 1.0f;  //!< さらに遅れて減る白いバー
    float delayHold_ = 0.0f;     //!< 白いバーが減り始めるまでの待ち時間

public:
    // ===============================================================
    // レイアウト・見た目（1280x720 基準。ImGui の "Boss HP" から調整できる）
    // ===============================================================
    Vector2 center_{ 640.0f, 640.0f }; //!< バーの中心
    float width_ = 760.0f;             //!< 満タンのときの横幅
    float height_ = 26.0f;
    float framePadding_ = 5.0f;        //!< 外枠のはみ出し量

    float appearSeconds_ = 0.65f;      //!< にゅーっと伸びきるまでの時間
    float hideSeconds_ = 0.35f;
    float appearOvershoot_ = 1.7f;     //!< EaseOutBack の行き過ぎ量。大きいほどビヨンとする

    float ratioCatchUp_ = 12.0f;       //!< 本体ゲージが目標へ追いつく速さ
    float delayedCatchUp_ = 3.0f;      //!< 白いバーが追いつく速さ
    float delayedHoldSeconds_ = 0.35f; //!< 減った直後、白いバーが止まっている時間

    Vector4 frameColor_{ 0.06f, 0.05f, 0.10f, 0.85f };
    Vector4 backColor_{ 0.16f, 0.13f, 0.20f, 0.90f };
    Vector4 delayedColor_{ 1.00f, 1.00f, 1.00f, 0.75f };

    // ゲージの色。満タン -> 中間 -> 空
    Vector4 colorFull_{ 0.25f, 0.95f, 0.35f, 1.0f };
    Vector4 colorHalf_{ 1.00f, 0.90f, 0.20f, 1.0f };
    Vector4 colorEmpty_{ 1.00f, 0.20f, 0.18f, 1.0f };
};
