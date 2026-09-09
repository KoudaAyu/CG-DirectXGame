#define NOMINMAX
#include "Application/Scene/GameScene/BossHpBar.h"

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace
{
    // 画像は無くてよい。engine が読み込み失敗時に 4x4 の白テクスチャを返すので、
    // SetColor() だけで色が決まる。あとから画像を置けばそのまま使われる
    constexpr const char* kBarTexture = "Resources/UI/Game/bar_white.png";

    /// @brief 行き過ぎてから戻るイージング（ビヨンと伸びる感じ）
    float EaseOutBack(float t, float overshoot)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        const float c1 = overshoot;
        const float c3 = c1 + 1.0f;
        const float u = t - 1.0f;
        return 1.0f + c3 * u * u * u + c1 * u * u;
    }

    Vector4 LerpColor(const Vector4& a, const Vector4& b, float t)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        return { a.x + (b.x - a.x) * t,
                 a.y + (b.y - a.y) * t,
                 a.z + (b.z - a.z) * t,
                 a.w + (b.w - a.w) * t };
    }

    /// @brief 指数減衰で target へ近づける（フレームレート非依存）
    float Approach(float current, float target, float rate, float deltaTime)
    {
        const float t = 1.0f - std::exp(-(std::max)(0.0f, rate) * deltaTime);
        return current + (target - current) * t;
    }
}

BossHpBar::~BossHpBar()
{
    Finalize();
}

std::unique_ptr<Sprite> BossHpBar::MakeSprite(const char* texturePath)
{
    std::unique_ptr<Sprite> sprite = Sprite::Create(texturePath, { 0.0f, 0.0f });
    if (!sprite) return nullptr;

    // 左端基準。ここが「左から右へ伸びるゲージ」の肝
    sprite->SetAnchorPoint({ 0.0f, 0.5f });
    sprite->SetSize({ 1.0f, 1.0f });
    sprite->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
    return sprite;
}

void BossHpBar::Initialize()
{
    Finalize();

    frame_ = MakeSprite(kBarTexture);
    back_ = MakeSprite(kBarTexture);
    delayed_ = MakeSprite(kBarTexture);
    fill_ = MakeSprite(kBarTexture);

    isVisible_ = false;
    appear_ = 0.0f;
    targetRatio_ = 1.0f;
    shownRatio_ = 1.0f;
    delayedRatio_ = 1.0f;
    delayHold_ = 0.0f;
}

void BossHpBar::Finalize()
{
    frame_.reset();
    back_.reset();
    delayed_.reset();
    fill_.reset();
}

void BossHpBar::Show()
{
    if (isVisible_) return;

    isVisible_ = true;
    appear_ = 0.0f;
    shownRatio_ = targetRatio_;
    delayedRatio_ = targetRatio_;
    delayHold_ = 0.0f;
}

void BossHpBar::Hide()
{
    isVisible_ = false;
}

Vector4 BossHpBar::RatioToColor(float ratio)
{
    // 実体は下のメンバを使うので、この static は使っていない。
    // （インターフェースを残すために置いてあるだけ）
    ratio = std::clamp(ratio, 0.0f, 1.0f);
    const Vector4 full{ 0.25f, 0.95f, 0.35f, 1.0f };
    const Vector4 half{ 1.00f, 0.90f, 0.20f, 1.0f };
    const Vector4 empty{ 1.00f, 0.20f, 0.18f, 1.0f };

    if (ratio >= 0.5f) return LerpColor(half, full, (ratio - 0.5f) * 2.0f);
    return LerpColor(empty, half, ratio * 2.0f);
}

void BossHpBar::ApplyBar(Sprite* sprite, float leftX, float centerY, float width, float height,
                         const Vector4& color)
{
    if (!sprite) return;

    sprite->SetPosition({ leftX, centerY });
    sprite->SetSize({ (std::max)(0.0f, width), (std::max)(0.0f, height) });
    sprite->SetColor(color);
    sprite->Update();
}

void BossHpBar::Update(float deltaTime)
{
    // --- 出入りのアニメーション ---
    if (isVisible_)
    {
        const float step = deltaTime / (std::max)(0.01f, appearSeconds_);
        appear_ = (std::min)(1.0f, appear_ + step);
    }
    else
    {
        const float step = deltaTime / (std::max)(0.01f, hideSeconds_);
        appear_ = (std::max)(0.0f, appear_ - step);
    }

    if (appear_ <= 0.0f)
    {
        // 完全に隠れている。スプライトを透明にしておく
        if (frame_) { frame_->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f }); frame_->Update(); }
        if (back_) { back_->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f }); back_->Update(); }
        if (delayed_) { delayed_->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f }); delayed_->Update(); }
        if (fill_) { fill_->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f }); fill_->Update(); }
        return;
    }

    // --- ゲージの追従 ---
    const float target = std::clamp(targetRatio_, 0.0f, 1.0f);
    const float before = shownRatio_;
    shownRatio_ = Approach(shownRatio_, target, ratioCatchUp_, deltaTime);

    // 減った瞬間は白いバーをその場に止めて、少し経ってから追いつかせる
    if (shownRatio_ < before - 1e-5f)
    {
        delayHold_ = delayedHoldSeconds_;
    }

    if (delayHold_ > 0.0f)
    {
        delayHold_ -= deltaTime;
    }
    else
    {
        delayedRatio_ = Approach(delayedRatio_, shownRatio_, delayedCatchUp_, deltaTime);
    }
    delayedRatio_ = (std::max)(delayedRatio_, shownRatio_);

    // --- レイアウト ---
    // にゅーっと伸びるのは「バー全体の横幅」。中身の比率とは別物なので、
    // 登場中はゲージがちゃんと満タンに見える
    const float grow = EaseOutBack(appear_, appearOvershoot_);
    const float barWidth = width_ * grow;
    const float leftX = center_.x - barWidth * 0.5f;
    const float alpha = std::clamp(appear_ * 1.4f, 0.0f, 1.0f);

    Vector4 frameColor = frameColor_; frameColor.w *= alpha;
    Vector4 backColor = backColor_;   backColor.w *= alpha;
    Vector4 delayedColor = delayedColor_; delayedColor.w *= alpha;

    // ゲージの色: 満タン緑 -> 黄 -> 赤
    Vector4 fillColor;
    if (shownRatio_ >= 0.5f)
    {
        fillColor = LerpColor(colorHalf_, colorFull_, (shownRatio_ - 0.5f) * 2.0f);
    }
    else
    {
        fillColor = LerpColor(colorEmpty_, colorHalf_, shownRatio_ * 2.0f);
    }
    fillColor.w *= alpha;

    ApplyBar(frame_.get(), leftX - framePadding_, center_.y,
             barWidth + framePadding_ * 2.0f, height_ + framePadding_ * 2.0f, frameColor);
    ApplyBar(back_.get(), leftX, center_.y, barWidth, height_, backColor);
    ApplyBar(delayed_.get(), leftX, center_.y, barWidth * delayedRatio_, height_, delayedColor);
    ApplyBar(fill_.get(), leftX, center_.y, barWidth * shownRatio_, height_, fillColor);
}

void BossHpBar::Draw(ID3D12GraphicsCommandList* commandList)
{
    if (!commandList || appear_ <= 0.0f) return;

    if (frame_) frame_->Draw(commandList);
    if (back_) back_->Draw(commandList);
    if (delayed_) delayed_->Draw(commandList);
    if (fill_) fill_->Draw(commandList);
}

void BossHpBar::DrawImGui()
{
#ifdef USE_IMGUI
    // ImGui のフォントに日本語グリフが無いので、ラベルは全部 ASCII で書くこと
    if (!ImGui::CollapsingHeader("Boss HP Bar")) return;

    ImGui::Text("Visible: %s / Appear: %.2f", isVisible_ ? "yes" : "no", appear_);
    ImGui::Text("Ratio: %.3f (target %.3f)", shownRatio_, targetRatio_);

    ImGui::SeparatorText("Layout");
    ImGui::DragFloat2("Center", &center_.x, 1.0f, 0.0f, 1280.0f);
    ImGui::DragFloat("Width", &width_, 2.0f, 40.0f, 1260.0f);
    ImGui::DragFloat("Height", &height_, 1.0f, 4.0f, 120.0f);
    ImGui::DragFloat("Frame Padding", &framePadding_, 0.5f, 0.0f, 30.0f);

    ImGui::SeparatorText("Animation");
    ImGui::DragFloat("Appear Seconds", &appearSeconds_, 0.01f, 0.05f, 3.0f);
    ImGui::DragFloat("Hide Seconds", &hideSeconds_, 0.01f, 0.05f, 3.0f);
    ImGui::DragFloat("Overshoot", &appearOvershoot_, 0.05f, 0.0f, 5.0f);
    ImGui::DragFloat("Ratio Catch Up", &ratioCatchUp_, 0.1f, 0.5f, 40.0f);
    ImGui::DragFloat("Delayed Catch Up", &delayedCatchUp_, 0.1f, 0.1f, 40.0f);
    ImGui::DragFloat("Delayed Hold", &delayedHoldSeconds_, 0.01f, 0.0f, 2.0f);

    ImGui::SeparatorText("Colors");
    ImGui::ColorEdit4("Frame", &frameColor_.x);
    ImGui::ColorEdit4("Back", &backColor_.x);
    ImGui::ColorEdit4("Delayed", &delayedColor_.x);
    ImGui::ColorEdit4("Full (green)", &colorFull_.x);
    ImGui::ColorEdit4("Half (yellow)", &colorHalf_.x);
    ImGui::ColorEdit4("Empty (red)", &colorEmpty_.x);

    if (ImGui::Button("Show")) Show();
    ImGui::SameLine();
    if (ImGui::Button("Hide")) Hide();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140.0f);
    ImGui::SliderFloat("Ratio##debug", &targetRatio_, 0.0f, 1.0f);
#endif
}
