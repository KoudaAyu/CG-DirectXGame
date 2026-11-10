#include "DeathTransition.h"

#include "Sprite.h"
#include "SpriteCom.h"
#include "WindowsAPI.h"
#include "SceneManager.h"
#include "OffScreenRendering.h"
#include "Application/Audio/TacticalAudio.h"
#include "Application/Config/GameConfig.h"
#include "Application/UI/TacticalUICanvas.h"

#include <algorithm>
#include <cmath>

namespace {
    constexpr float kPi = GameConfig::Math::kPi;

    // リソース＆オーディオ設定
    constexpr const char* kWhiteTexPath     = "Resources/white.png";
    constexpr const char* kSeShutterSlam    = "Resources/Audio/SE_Shutter_Slam.wav";
    constexpr const char* kSeShutterOpen    = "Resources/Audio/SE_Shutter_Open.wav";
    constexpr float       kSlamSeVolume     = 0.78f;
    constexpr float       kOpenSeVolume     = 0.68f;

    // 装甲シャッター形状・カラー設定
    constexpr float   kShutterOverlapPx     = 16.0f;
    constexpr float   kLipHeightPx          = 10.0f;
    constexpr float   kHazardHeightPx       = 3.0f;
    constexpr float   kSeamBarHeightPx      = 2.0f;

    constexpr Vector4 kShutterColor         = { 0.045f, 0.048f, 0.055f, 1.0f };
    constexpr Vector4 kLipColor             = { 0.16f,  0.17f,  0.20f,  1.0f };
    constexpr Vector4 kHazardStripeColor    = { 0.88f,  0.62f,  0.16f,  0.95f };
    constexpr Vector4 kSeamInitColor        = { 0.88f,  0.22f,  0.18f,  0.0f };
    constexpr Vector3 kSeamActiveRGB        = { 0.92f,  0.25f,  0.18f };
    constexpr Vector3 kBackdropRGB          = { 0.02f,  0.02f,  0.03f };

    // タイムライン位相設定
    constexpr float   kMinValidDuration     = 0.1f;
    constexpr float   kMaxFrameDeltaTime    = 1.0f / 60.0f;
    constexpr float   kClosePhaseEnd        = 0.38f;
    constexpr float   kHoldPhaseEnd         = 0.62f;
    constexpr float   kHoldPhaseDuration    = kHoldPhaseEnd - kClosePhaseEnd;
    constexpr float   kOpenPhaseDuration    = 1.0f - kHoldPhaseEnd;
    constexpr float   kSceneSwitchPoint     = 0.48f;
    constexpr float   kSlamSeTriggerPoint   = 0.30f;

    constexpr float   kBounceStartRatio     = 0.88f;
    constexpr float   kBounceSpanRatio      = 1.0f - kBounceStartRatio;
    constexpr float   kBounceAmplitudePx    = 2.5f;
    constexpr float   kSeamPeakAlpha        = 0.90f;
    constexpr float   kBackdropFadeOutRate  = 3.0f;
    constexpr float   kBackdropDrawMinAlpha = 0.001f;
    constexpr float   kSeamDrawMinAlpha     = 0.01f;

    // 心電図（ECG）フラットラインUI設定
    constexpr float   kUiVisibleStart       = 0.25f;
    constexpr float   kUiVisibleEnd         = 0.75f;
    constexpr float   kUiFadeSpan           = kClosePhaseEnd - kUiVisibleStart;
    constexpr float   kMaxByteAlpha         = 255.0f;
    constexpr float   kEcgWindowStart       = 0.30f;
    constexpr float   kEcgWindowSpan        = 0.40f;
    constexpr float   kEcgDampSpeed         = 2.2f;
    constexpr float   kEcgHalfWidthPx       = 260.0f;
    constexpr float   kEcgLineThickness     = 2.2f;
    constexpr int     kEcgColorR            = 235;
    constexpr int     kEcgColorG            = 75;
    constexpr int     kEcgColorB            = 65;

    struct EcgSamplePoint {
        float xRatio;
        float fixedOffsetY;
        float decayingOffsetY;
    };

    constexpr EcgSamplePoint kEcgWaveform[] = {
        { 0.00f,   0.0f,   0.0f },
        { 0.32f,   0.0f,   0.0f },
        { 0.36f, -14.0f,   0.0f },
        { 0.40f,  18.0f,   0.0f },
        { 0.44f,   0.0f, -32.0f },
        { 0.48f,   0.0f,  12.0f },
        { 0.52f,   0.0f,  -6.0f },
        { 0.56f,   0.0f,   0.0f },
        { 1.00f,   0.0f,   0.0f },
    };

    float EaseOutCubic(float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        return 1.0f - GameConfig::Math::Cube(1.0f - t);
    }

    float EaseInCubic(float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        return GameConfig::Math::Cube(t);
    }

    std::unique_ptr<Sprite> CreatePlateSprite(
        SpriteCom* spriteCom,
        const Vector2& anchor,
        const Vector2& size,
        const Vector4& color)
    {
        const Sprite::Transform transform = {
            { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }
        };
        auto sprite = Sprite::Create(spriteCom, transform, kWhiteTexPath);
        if (sprite) {
            sprite->SetAnchorPoint(anchor);
            sprite->SetSize(size);
            sprite->SetColor(color);
        }
        return sprite;
    }

    void DrawSpriteIfReady(ID3D12GraphicsCommandList* cmdList, Sprite* sprite, float minAlpha = 0.0f) {
        if (sprite && sprite->GetColor().w > minAlpha) {
            sprite->Update();
            sprite->Draw(cmdList);
        }
    }
}

DeathTransition* DeathTransition::GetInstance() {
    static DeathTransition instance;
    return &instance;
}

void DeathTransition::RefreshViewportMetrics() {
    screenWidth_ = static_cast<float>(WindowAPI::GetClientWidth());
    screenHeight_ = static_cast<float>(WindowAPI::GetClientHeight());
    if (screenWidth_ <= 0.0f)  screenWidth_ = kDefaultScreenWidth;
    if (screenHeight_ <= 0.0f) screenHeight_ = kDefaultScreenHeight;
}

void DeathTransition::ApplyResponsiveSpriteSizes() {
    const float halfH = screenHeight_ * 0.5f + kShutterOverlapPx;
    if (upperShutter_)      upperShutter_->SetSize({ screenWidth_, halfH });
    if (lowerShutter_)      lowerShutter_->SetSize({ screenWidth_, halfH });
    if (upperLip_)          upperLip_->SetSize({ screenWidth_, kLipHeightPx });
    if (lowerLip_)          lowerLip_->SetSize({ screenWidth_, kLipHeightPx });
    if (upperHazardStripe_) upperHazardStripe_->SetSize({ screenWidth_, kHazardHeightPx });
    if (lowerHazardStripe_) lowerHazardStripe_->SetSize({ screenWidth_, kHazardHeightPx });
    if (centerSeamBar_)     centerSeamBar_->SetSize({ screenWidth_, kSeamBarHeightPx });
    if (safetyBackdrop_)    safetyBackdrop_->SetSize({ screenWidth_, screenHeight_ });
}

void DeathTransition::Initialize(SpriteCom* spriteCom, WindowAPI* windowAPI) {
    spriteCom_ = spriteCom;
    windowAPI_ = windowAPI;
    if (!spriteCom_ || !windowAPI_) return;

    RefreshViewportMetrics();
    const float halfH = screenHeight_ * 0.5f + kShutterOverlapPx;
    constexpr Vector2 kTopLeftAnchor = { 0.0f, 0.0f };

    upperShutter_      = CreatePlateSprite(spriteCom_, kTopLeftAnchor, { screenWidth_, halfH },            kShutterColor);
    lowerShutter_      = CreatePlateSprite(spriteCom_, kTopLeftAnchor, { screenWidth_, halfH },            kShutterColor);
    upperLip_          = CreatePlateSprite(spriteCom_, kTopLeftAnchor, { screenWidth_, kLipHeightPx },     kLipColor);
    lowerLip_          = CreatePlateSprite(spriteCom_, kTopLeftAnchor, { screenWidth_, kLipHeightPx },     kLipColor);
    upperHazardStripe_ = CreatePlateSprite(spriteCom_, kTopLeftAnchor, { screenWidth_, kHazardHeightPx },  kHazardStripeColor);
    lowerHazardStripe_ = CreatePlateSprite(spriteCom_, kTopLeftAnchor, { screenWidth_, kHazardHeightPx },  kHazardStripeColor);
    centerSeamBar_     = CreatePlateSprite(spriteCom_, { 0.0f, 0.5f }, { screenWidth_, kSeamBarHeightPx }, kSeamInitColor);
    safetyBackdrop_    = CreatePlateSprite(spriteCom_, kTopLeftAnchor, { screenWidth_, screenHeight_ },    { kBackdropRGB.x, kBackdropRGB.y, kBackdropRGB.z, 0.0f });
}

void DeathTransition::Finalize() {
    upperShutter_.reset();
    lowerShutter_.reset();
    upperLip_.reset();
    lowerLip_.reset();
    upperHazardStripe_.reset();
    lowerHazardStripe_.reset();
    centerSeamBar_.reset();
    safetyBackdrop_.reset();
}

void DeathTransition::StartTransition(const std::string& nextSceneName, float duration) {
    if (isActive_) return;

    isActive_ = true;
    hasSwitched_ = false;
    slamSoundPlayed_ = false;
    openSoundPlayed_ = false;
    timer_ = 0.0f;
    duration_ = (duration > kMinValidDuration) ? duration : kDefaultDuration;
    progress_ = 0.0f;
    pendingSceneName_ = nextSceneName.empty() ? kDefaultTargetScene : nextSceneName;

    RefreshViewportMetrics();
    ApplyResponsiveSpriteSizes();

    // トランジション中はアウトラインポストプロセスを通常化してアーティファクトを抑制
    if (offscreen_) {
        savedPostEffect_ = static_cast<int>(offscreen_->GetPostEffect());
        offscreen_->SetPostEffect(OffScreenRendering::PostEffect::Normal);
    }
}

void DeathTransition::Update(float deltaTime) {
    if (!isActive_) return;

    timer_ += (std::min)(deltaTime, kMaxFrameDeltaTime);
    progress_ = std::clamp(timer_ / duration_, 0.0f, 1.0f);

    const float H = screenHeight_;
    const float halfH = H * 0.5f + kShutterOverlapPx;
    const float midY = H * 0.5f;

    float upperY = -halfH;
    float lowerY = H;
    float seamAlpha = 0.0f;
    float backdropAlpha = 0.0f;

    if (progress_ < kClosePhaseEnd) {
        // Phase 1: クランプ閉鎖
        const float p = progress_ / kClosePhaseEnd;
        const float ease = EaseOutCubic(p);
        float bounce = 0.0f;
        if (p > kBounceStartRatio) {
            const float bp = (p - kBounceStartRatio) / kBounceSpanRatio;
            bounce = std::sin(bp * kPi) * kBounceAmplitudePx;
        }

        upperY = -halfH * (1.0f - ease) - bounce;
        lowerY = H - halfH * ease + bounce;

        if (!slamSoundPlayed_ && progress_ >= kSlamSeTriggerPoint) {
            slamSoundPlayed_ = true;
            TacticalAudio::Play2D(kSeShutterSlam, kSlamSeVolume);
        }
    } else if (progress_ <= kHoldPhaseEnd) {
        // Phase 2: 完全密閉 ＆ 安全なシーン切り替え
        if (!slamSoundPlayed_) {
            slamSoundPlayed_ = true;
            TacticalAudio::Play2D(kSeShutterSlam, kSlamSeVolume);
        }
        upperY = -kShutterOverlapPx;
        lowerY = midY;
        backdropAlpha = 1.0f;

        const float p = (progress_ - kClosePhaseEnd) / kHoldPhaseDuration;
        seamAlpha = std::sin(p * kPi) * kSeamPeakAlpha;

        if (progress_ >= kSceneSwitchPoint && !hasSwitched_) {
            hasSwitched_ = true;
            if (!pendingSceneName_.empty()) {
                SceneManager::GetInstance()->ChangeScene(pendingSceneName_);
            }
        }
    } else {
        // Phase 3: カーゴベイ展開（GameOverSceneへ突入）
        if (!openSoundPlayed_) {
            openSoundPlayed_ = true;
            TacticalAudio::Play2D(kSeShutterOpen, kOpenSeVolume);
        }
        const float s = (progress_ - kHoldPhaseEnd) / kOpenPhaseDuration;
        const float ease = EaseInCubic(s);

        upperY = -kShutterOverlapPx - halfH * ease;
        lowerY = midY + halfH * ease;
        backdropAlpha = (std::max)(0.0f, 1.0f - s * kBackdropFadeOutRate);
    }

    if (upperShutter_)      upperShutter_->SetPosition({ 0.0f, upperY });
    if (upperLip_)          upperLip_->SetPosition({ 0.0f, upperY + halfH - kLipHeightPx });
    if (upperHazardStripe_) upperHazardStripe_->SetPosition({ 0.0f, upperY + halfH - kHazardHeightPx });

    if (lowerShutter_)      lowerShutter_->SetPosition({ 0.0f, lowerY });
    if (lowerLip_)          lowerLip_->SetPosition({ 0.0f, lowerY });
    if (lowerHazardStripe_) lowerHazardStripe_->SetPosition({ 0.0f, lowerY });

    if (centerSeamBar_) {
        centerSeamBar_->SetPosition({ 0.0f, midY });
        centerSeamBar_->SetColor({ kSeamActiveRGB.x, kSeamActiveRGB.y, kSeamActiveRGB.z, seamAlpha });
    }
    if (safetyBackdrop_) {
        safetyBackdrop_->SetColor({ kBackdropRGB.x, kBackdropRGB.y, kBackdropRGB.z, backdropAlpha });
    }

    if (progress_ >= 1.0f) {
        isActive_ = false;
        if (offscreen_ && savedPostEffect_ >= 0) {
            offscreen_->SetPostEffect(static_cast<OffScreenRendering::PostEffect>(savedPostEffect_));
            savedPostEffect_ = -1;
        }
    }
}

void DeathTransition::Draw(const RenderContext& ctx) {
    if (!isActive_ || !spriteCom_ || !ctx.commandList) return;

    auto* cmdList = ctx.commandList;
    spriteCom_->SetupDraw(cmdList);

    DrawSpriteIfReady(cmdList, safetyBackdrop_.get(), kBackdropDrawMinAlpha);
    DrawSpriteIfReady(cmdList, upperShutter_.get());
    DrawSpriteIfReady(cmdList, upperLip_.get());
    DrawSpriteIfReady(cmdList, upperHazardStripe_.get());
    DrawSpriteIfReady(cmdList, lowerShutter_.get());
    DrawSpriteIfReady(cmdList, lowerLip_.get());
    DrawSpriteIfReady(cmdList, lowerHazardStripe_.get());
    DrawSpriteIfReady(cmdList, centerSeamBar_.get(), kSeamDrawMinAlpha);
}

void DeathTransition::DrawUI() {
    if (!isActive_ || progress_ < kUiVisibleStart || progress_ > kUiVisibleEnd) return;

    TacticalUICanvas* ui = TacticalUICanvas::GetInstance();
    if (!ui) return;
    const UIVec2 disp = ui->GetDisplaySize();
    if (disp.x <= 0.0f || disp.y <= 0.0f) return;

    TacticalDrawList* dl = ui->GetPostTransitionDrawList();
    if (!dl) return;

    float alpha = 1.0f;
    if (progress_ < kClosePhaseEnd) {
        alpha = (progress_ - kUiVisibleStart) / kUiFadeSpan;
    } else if (progress_ > kHoldPhaseEnd) {
        alpha = (kUiVisibleEnd - progress_) / kUiFadeSpan;
    }
    const int a255 = static_cast<int>(std::clamp(alpha, 0.0f, 1.0f) * kMaxByteAlpha);
    if (a255 <= 0) return;

    const float midX = disp.x * 0.5f;
    const float midY = disp.y * 0.5f;
    const float ecgT = (progress_ - kEcgWindowStart) / kEcgWindowSpan;
    const float waveDamping = 1.0f - std::clamp(ecgT * kEcgDampSpeed, 0.0f, 1.0f);
    const float startX = midX - kEcgHalfWidthPx;
    const float ecgW = kEcgHalfWidthPx * 2.0f;

    constexpr size_t kPointCount = sizeof(kEcgWaveform) / sizeof(kEcgWaveform[0]);
    UIVec2 pts[kPointCount]{};
    for (size_t i = 0; i < kPointCount; ++i) {
        pts[i] = {
            startX + ecgW * kEcgWaveform[i].xRatio,
            midY + kEcgWaveform[i].fixedOffsetY + kEcgWaveform[i].decayingOffsetY * waveDamping
        };
    }

    dl->AddPolyline(pts, static_cast<int>(kPointCount), UI_COL32(kEcgColorR, kEcgColorG, kEcgColorB, a255), UIDrawFlags_None, kEcgLineThickness);
}
