#include "FadeApplication.h"

#include "SpriteCom.h"
#include "WindowsAPI.h"

#include <algorithm>

void FadeApplication::Initialize(SpriteCom* spriteCom, WindowAPI* windowAPI)
{
    spriteCom_ = spriteCom;
    windowAPI_ = windowAPI;

    if (!spriteCom_ || !windowAPI_)
    {
        return;
    }

    Sprite::Transform transform = {
        {1.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f}
    };

    fadeSprite_ = Sprite::Create(spriteCom_, transform, "Resources/CG4/human/white.png");
    if (!fadeSprite_)
    {
        return;
    }

    fadeSprite_->SetPosition({ 0.0f, 0.0f });
    fadeSprite_->SetAnchorPoint({ 0.0f, 0.0f });
    fadeSprite_->SetSize({
        static_cast<float>(WindowAPI::GetClientWidth()),
        static_cast<float>(WindowAPI::GetClientHeight())
    });

    ApplyColor();
}

void FadeApplication::Finalize()
{
    if (fadeSprite_)
    {
        fadeSprite_->Finalize();
        fadeSprite_.reset();
    }

    spriteCom_ = nullptr;
    windowAPI_ = nullptr;
    state_ = State::None;
    alpha_ = 0.0f;
    fadeSpeed_ = 0.0f;
}

void FadeApplication::Update()
{
    if (!fadeSprite_)
    {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(now - lastTime_).count();
    lastTime_ = now;

    if (deltaTime > 0.1f)
    {
        deltaTime = 1.0f / 60.0f;
    }

    float speedMultiplier = deltaTime * 60.0f;

    switch (state_)
    {
    case State::FadeOut:
        alpha_ = (std::min)(1.0f, alpha_ + fadeSpeed_ * speedMultiplier);
        break;

    case State::FadeIn:
        alpha_ = (std::max)(0.0f, alpha_ - fadeSpeed_ * speedMultiplier);
        if (alpha_ <= 0.0f)
        {
            state_ = State::None;
        }
        break;

    case State::None:
    default:
        break;
    }

    ApplyColor();
}

void FadeApplication::Draw()
{
    if (!fadeSprite_ || !spriteCom_ || !windowAPI_ || alpha_ <= 0.0f)
    {
        return;
    }

    auto* dxCommon = spriteCom_->GetDxCommon();
    if (!dxCommon || !dxCommon->GetCommandList())
    {
        return;
    }

    spriteCom_->SetupDraw(dxCommon->GetCommandList().Get());
    fadeSprite_->Update();
    fadeSprite_->Draw();
}

void FadeApplication::StartFadeOut(int frameCount)
{
    state_ = State::FadeOut;
    fadeSpeed_ = CalculateFadeSpeed(frameCount);
    lastTime_ = std::chrono::steady_clock::now();
}

void FadeApplication::StartFadeIn(int frameCount)
{
    state_ = State::FadeIn;
    fadeSpeed_ = CalculateFadeSpeed(frameCount);
    lastTime_ = std::chrono::steady_clock::now();
}

bool FadeApplication::IsAvailable() const
{
    return fadeSprite_ && spriteCom_ && windowAPI_;
}

bool FadeApplication::IsBusy() const
{
    return state_ != State::None;
}

bool FadeApplication::IsFadeOutFinished() const
{
    return state_ == State::FadeOut && alpha_ >= 1.0f;
}

void FadeApplication::ApplyColor()
{
    if (!fadeSprite_)
    {
        return;
    }

    fadeSprite_->SetColor({ 0.0f, 0.0f, 0.0f, alpha_ });
}

float FadeApplication::CalculateFadeSpeed(int frameCount)
{
  return 1.0f / static_cast<float>((std::max)(1, frameCount));
}
