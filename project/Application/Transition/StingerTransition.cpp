#include "StingerTransition.h"

#include "Sprite.h"
#include "SpriteCom.h"
#include "WindowsAPI.h"
#include "SceneManager.h"
#include "OffScreenRendering.h"
#include "Application/Audio/TacticalAudio.h"
#include "Application/Config/AppDebugConfig.h"
#include "Application/Config/GameConfig.h"
#include "Application/UI/TacticalUICanvas.h"

#include <algorithm>
#include <cmath>

namespace {
    constexpr float kPi        = GameConfig::Math::kPi;
    constexpr float kQuarterPi = GameConfig::Math::kQuarterPi;

    // 共通リソース＆オーディオ設定
    constexpr const char* kWhiteTexPath        = "Resources/CG4/human/white.png";
    constexpr const char* kSeWhooshPath        = "Resources/Audio/SE_Stinger_Whoosh.wav";
    constexpr float       kWhooshVolume        = 0.58f;
    constexpr uint32_t    kWhooshCooldownMs    = 80;

    // ストライプ構造・レイアウト定数
    constexpr size_t  kStripeCount             = 8;
    constexpr size_t  kMainCurtainIndex        = 6;
    constexpr size_t  kFrameColorStripeIndex   = 4;
    constexpr size_t  kGlintColorStripeIndex   = 1;
    constexpr float   kTravelDistScreenMul     = 2.2f;
    constexpr float   kOffscreenCoord          = -9999.0f;
    constexpr Vector2 kOffscreenPos            = { kOffscreenCoord, kOffscreenCoord };
    constexpr Vector2 kCenterAnchor            = { 0.5f, 0.5f };
    constexpr Vector2 kTopLeftAnchor           = { 0.0f, 0.0f };
    constexpr Vector2 kBottomLeftAnchor        = { 0.0f, 1.0f };

    constexpr float   kBackdropMarginTotal     = 40.0f;
    constexpr float   kBackdropMarginOffset    = -20.0f;
    constexpr Vector4 kDefaultBackdropColor    = { 0.08f, 0.09f, 0.12f, 0.0f };
    constexpr Vector4 kFallbackBackdropOpaque  = { 0.08f, 0.09f, 0.12f, 1.0f };

    constexpr float   kLetterboxHeightPx       = 28.0f;
    constexpr float   kLetterboxInitHideOffset = 40.0f;
    constexpr float   kLetterboxSlideOffset    = -32.0f;
    constexpr Vector4 kLetterboxColor          = { 0.04f, 0.05f, 0.07f, 1.0f };

    constexpr Vector2 kInitFrameDiamondSize    = { 130.0f, 130.0f };
    constexpr Vector2 kInitCoreDiamondSize     = { 65.0f,  65.0f  };
    constexpr Vector2 kInitGlintBarSize        = { 600.0f, 6.0f   };
    constexpr Vector4 kTransparentWhite        = { 1.0f, 1.0f, 1.0f, 0.0f };
    constexpr Vector4 kOpaqueWhite             = { 1.0f, 1.0f, 1.0f, 1.0f };
    constexpr Vector4 kTransparentBlack        = { 0.0f, 0.0f, 0.0f, 0.0f };
    constexpr Vector4 kFallbackFrameColor      = { 1.0f, 0.8f, 0.2f, 1.0f };
    constexpr Vector4 kFallbackGlintColor      = { 1.0f, 0.5f, 0.15f, 1.0f };

    // エルミート連続スプライン＆タイムライン定数
    constexpr float   kHermiteCoeff3           = 3.0f;
    constexpr float   kMinDuration             = 0.25f;
    constexpr float   kMaxFrameDeltaTime       = 1.0f / 60.0f;
    constexpr float   kSplineEnterEnd          = 0.36f;
    constexpr float   kSplineExitStart         = 0.64f;
    constexpr float   kCenterDriftHalfPx       = 90.0f;
    constexpr float   kSceneSwitchPoint        = 0.48f;

    constexpr float   kBackdropWindowStart     = 0.26f;
    constexpr float   kBackdropHoldStart       = 0.38f;
    constexpr float   kBackdropHoldEnd         = 0.62f;
    constexpr float   kBackdropWindowEnd       = 0.74f;
    constexpr float   kBackdropFadeSpan        = 0.12f;
    constexpr float   kBackdropDrawMinAlpha    = 0.001f;
    constexpr float   kEmblemDrawMinAlpha      = 0.01f;

    constexpr float   kHoldWindowStart         = 0.22f;
    constexpr float   kHoldWindowEnd           = 0.78f;
    constexpr float   kHoldWindowSpan          = kHoldWindowEnd - kHoldWindowStart;
    constexpr float   kLetterboxAnimSpan       = 0.22f;

    // 中央アイキャッチ・エンブレム演出定数
    constexpr float   kFrameBaseSize           = 140.0f;
    constexpr float   kFrameScaleMin           = 0.75f;
    constexpr float   kFrameScalePulse         = 0.35f;
    constexpr float   kFrameSpinRad            = 2.8f;
    constexpr float   kFrameMaxAlpha           = 0.95f;

    constexpr float   kCoreBaseSize            = 68.0f;
    constexpr float   kCoreScaleMin            = 0.80f;
    constexpr float   kCoreScalePulse          = 0.35f;
    constexpr float   kCoreSpinRad             = 4.5f;

    constexpr float   kGlintBaseWidth          = 560.0f;
    constexpr float   kGlintWidthMin           = 0.60f;
    constexpr float   kGlintWidthPulse         = 0.55f;
    constexpr float   kGlintBaseHeight         = 5.0f;
    constexpr float   kGlintHeightMin          = 0.80f;
    constexpr float   kGlintHeightPulse        = 0.50f;
    constexpr float   kGlintMaxAlpha           = 0.90f;

    constexpr float   kFlashEndPoint           = 0.60f;
    constexpr float   kFlashDuration           = kFlashEndPoint - kSceneSwitchPoint;
    constexpr float   kFlashPeakAlpha          = 0.35f;

#if APP_DEBUG_UI
    constexpr int     kCycleThemeCount         = 4;
#endif

    struct StripeSpec {
        Vector2 size;
        float   offsetX;
        float   cascadeDelay;
    };

    constexpr StripeSpec kStripeSpecs[kStripeCount] = {
        { {   10.0f, 3200.0f }, -340.0f, 0.000f }, // 0: 先導極細スパークライン
        { {   50.0f, 3200.0f }, -270.0f, 0.008f }, // 1: アクセント帯
        { {   18.0f, 3200.0f }, -210.0f, 0.016f }, // 2: ホワイトエッジ光線
        { {   85.0f, 3200.0f }, -130.0f, 0.024f }, // 3: ディープスレートシャドウ帯
        { {  240.0f, 3200.0f },  -50.0f, 0.032f }, // 4: メイン中帯
        { {   14.0f, 3200.0f },   15.0f, 0.040f }, // 5: スパークライン
        { { 4400.0f, 3200.0f },   60.0f, 0.048f }, // 6: 全画面密閉ウルトラワイドカーテン
        { {  120.0f, 3200.0f }, -410.0f, 0.060f }, // 7: 後追いシャドウ帯
    };

    constexpr int kStripeDrawOrder[kStripeCount] = { 6, 7, 4, 3, 1, 5, 2, 0 };

    // テーマ別8層カラーパレットテーブル（enum TransitionTheme 定義順に厳密対応）
    constexpr Vector4 kThemePalettes[5][kStripeCount] = {
        // TitleTactical
        {
            { 1.0f,  1.0f,  1.0f,  1.0f },
            { 1.0f,  0.48f, 0.12f, 1.0f },
            { 0.98f, 0.98f, 1.0f,  1.0f },
            { 0.14f, 0.16f, 0.20f, 1.0f },
            { 1.0f,  0.80f, 0.22f, 1.0f },
            { 0.35f, 0.85f, 1.0f,  1.0f },
            { 0.08f, 0.09f, 0.12f, 1.0f },
            { 0.85f, 0.55f, 0.15f, 1.0f },
        },
        // DuckovTactical
        {
            { 1.0f,  0.95f, 0.85f, 1.0f },
            { 0.95f, 0.40f, 0.10f, 1.0f },
            { 1.0f,  0.85f, 0.15f, 1.0f },
            { 0.18f, 0.20f, 0.24f, 1.0f },
            { 0.32f, 0.35f, 0.40f, 1.0f },
            { 1.0f,  0.75f, 0.20f, 1.0f },
            { 0.08f, 0.09f, 0.11f, 1.0f },
            { 0.22f, 0.24f, 0.28f, 1.0f },
        },
        // TororoPastel
        {
            { 1.0f,  1.0f,  1.0f,  1.0f },
            { 1.0f,  0.38f, 0.65f, 1.0f },
            { 0.96f, 0.98f, 1.0f,  1.0f },
            { 0.12f, 0.22f, 0.38f, 1.0f },
            { 0.22f, 0.72f, 0.98f, 1.0f },
            { 1.0f,  0.90f, 0.35f, 1.0f },
            { 0.14f, 0.52f, 0.86f, 1.0f },
            { 0.40f, 0.35f, 0.70f, 1.0f },
        },
        // CyberNeon
        {
            { 1.0f,  1.0f,  1.0f,  1.0f },
            { 1.0f,  0.15f, 0.55f, 1.0f },
            { 0.10f, 0.95f, 0.98f, 1.0f },
            { 0.18f, 0.05f, 0.30f, 1.0f },
            { 0.05f, 0.65f, 0.85f, 1.0f },
            { 1.0f,  0.95f, 0.15f, 1.0f },
            { 0.55f, 0.08f, 0.45f, 1.0f },
            { 0.80f, 0.10f, 0.40f, 1.0f },
        },
        // Monochrome
        {
            { 1.0f,  1.0f,  1.0f,  1.0f },
            { 0.85f, 0.87f, 0.90f, 1.0f },
            { 1.0f,  1.0f,  1.0f,  1.0f },
            { 0.22f, 0.22f, 0.25f, 1.0f },
            { 0.50f, 0.52f, 0.56f, 1.0f },
            { 0.90f, 0.92f, 0.95f, 1.0f },
            { 0.06f, 0.06f, 0.07f, 1.0f },
            { 0.15f, 0.15f, 0.17f, 1.0f },
        },
    };

    float SmoothStep01(float u) {
        u = std::clamp(u, 0.0f, 1.0f);
        return u * u * (kHermiteCoeff3 - 2.0f * u);
    }

    /// @brief 3次エルミート補間（始点・終点・両端の速度を厳密に一致させる）
    float Hermite(float p0, float m0, float p1, float m1, float s) {
        const float s2 = s * s;
        const float s3 = s2 * s;
        const float h00 = 2.0f * s3 - kHermiteCoeff3 * s2 + 1.0f;
        const float h10 = s3 - 2.0f * s2 + s;
        const float h01 = -2.0f * s3 + kHermiteCoeff3 * s2;
        const float h11 = s3 - s2;
        return h00 * p0 + h10 * m0 + h01 * p1 + h11 * m1;
    }

    /// @brief 速度が全区間で滑らかに連続するスプライントランジションカーブ
    float EvaluateSmoothContinuousPosition(float t, float startX, float cx, float endX) {
        t = std::clamp(t, 0.0f, 1.0f);

        const float mid1 = cx + kCenterDriftHalfPx;
        const float mid2 = cx - kCenterDriftHalfPx;
        const float driftDist = mid2 - mid1;
        constexpr float kDriftDuration = kSplineExitStart - kSplineEnterEnd;

        if (t < kSplineEnterEnd) {
            const float s = t / kSplineEnterEnd;
            const float m1 = driftDist * (kSplineEnterEnd / kDriftDuration);
            return Hermite(startX, 0.0f, mid1, m1, s);
        }
        if (t <= kSplineExitStart) {
            const float s = (t - kSplineEnterEnd) / kDriftDuration;
            return mid1 + driftDist * s;
        }
        const float exitSpan = 1.0f - kSplineExitStart;
        const float s = (t - kSplineExitStart) / exitSpan;
        const float m0 = driftDist * (exitSpan / kDriftDuration);
        return Hermite(mid2, m0, endX, 0.0f, s);
    }

    std::unique_ptr<Sprite> CreateTransitionSprite(
        SpriteCom* spriteCom,
        const Vector2& anchor,
        const Vector2& size,
        const Vector2& pos,
        float rotation,
        const Vector4& color)
    {
        const Sprite::Transform transform = {
            { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }
        };
        auto sp = Sprite::Create(spriteCom, transform, kWhiteTexPath);
        if (sp) {
            sp->SetAnchorPoint(anchor);
            sp->SetSize(size);
            sp->SetPosition(pos);
            sp->SetRotation(rotation);
            sp->SetColor(color);
        }
        return sp;
    }

    void FinalizeAndResetSprite(std::unique_ptr<Sprite>& sp) {
        if (sp) {
            sp->Finalize();
            sp.reset();
        }
    }

    void ParkSpriteOffscreen(Sprite* sp, const Vector2& pos = kOffscreenPos) {
        if (sp) {
            sp->SetPosition(pos);
            sp->SetAlpha(0.0f);
            sp->Update();
        }
    }

    void DrawSpriteIfVisible(ID3D12GraphicsCommandList* cmdList, Sprite* sp, float minAlpha = 0.0f) {
        if (sp && sp->GetColor().w > minAlpha) {
            sp->Update();
            sp->Draw(cmdList);
        }
    }
}

StingerTransition* StingerTransition::GetInstance() {
    static StingerTransition instance;
    return &instance;
}

void StingerTransition::RefreshViewportMetrics() {
    screenWidth_ = static_cast<float>(WindowAPI::GetClientWidth());
    screenHeight_ = static_cast<float>(WindowAPI::GetClientHeight());
    if (screenWidth_ <= 0.0f)  screenWidth_ = kDefaultScreenWidth;
    if (screenHeight_ <= 0.0f) screenHeight_ = kDefaultScreenHeight;
    travelDistance_ = screenWidth_ * kTravelDistScreenMul;
}

void StingerTransition::Initialize(SpriteCom* spriteCom, WindowAPI* windowAPI) {
    spriteCom_ = spriteCom;
    windowAPI_ = windowAPI;
    if (!spriteCom_ || !windowAPI_) return;

    RefreshViewportMetrics();
    stripes_.clear();
    stripes_.reserve(kStripeCount);

    for (size_t i = 0; i < kStripeCount; ++i) {
        StripeLayer layer;
        layer.size = kStripeSpecs[i].size;
        layer.offsetX = kStripeSpecs[i].offsetX;
        layer.sprite = CreateTransitionSprite(spriteCom_, kCenterAnchor, layer.size, kOffscreenPos, stripeAngle_, kOpaqueWhite);
        stripes_.emplace_back(std::move(layer));
    }

    safetyBackdrop_ = CreateTransitionSprite(
        spriteCom_, kTopLeftAnchor,
        { screenWidth_ + kBackdropMarginTotal, screenHeight_ + kBackdropMarginTotal },
        { kBackdropMarginOffset, kBackdropMarginOffset }, 0.0f, kDefaultBackdropColor);

    topLetterbox_ = CreateTransitionSprite(
        spriteCom_, kTopLeftAnchor, { screenWidth_, kLetterboxHeightPx },
        { 0.0f, -kLetterboxInitHideOffset }, 0.0f, kLetterboxColor);

    bottomLetterbox_ = CreateTransitionSprite(
        spriteCom_, kBottomLeftAnchor, { screenWidth_, kLetterboxHeightPx },
        { 0.0f, screenHeight_ + kLetterboxInitHideOffset }, 0.0f, kLetterboxColor);

    centerFrameSprite_ = CreateTransitionSprite(spriteCom_, kCenterAnchor, kInitFrameDiamondSize, kOffscreenPos, kQuarterPi, kTransparentWhite);
    centerCoreSprite_  = CreateTransitionSprite(spriteCom_, kCenterAnchor, kInitCoreDiamondSize,  kOffscreenPos, kQuarterPi, kTransparentWhite);
    centerGlintBar_    = CreateTransitionSprite(spriteCom_, kCenterAnchor, kInitGlintBarSize,     kOffscreenPos, 0.0f,       kTransparentWhite);
    centerFlashSprite_ = CreateTransitionSprite(spriteCom_, kTopLeftAnchor, { screenWidth_, screenHeight_ }, { 0.0f, 0.0f }, 0.0f, kTransparentWhite);

    ApplyThemeColors();
    isActive_ = false;
    hasSwitched_ = false;
    timer_ = 0.0f;
    progress_ = 0.0f;
    postSwitchTimer_ = 0.0f;
}

void StingerTransition::Finalize() {
    for (auto& stripe : stripes_) {
        FinalizeAndResetSprite(stripe.sprite);
    }
    stripes_.clear();

    FinalizeAndResetSprite(safetyBackdrop_);
    FinalizeAndResetSprite(topLetterbox_);
    FinalizeAndResetSprite(bottomLetterbox_);
    FinalizeAndResetSprite(centerFrameSprite_);
    FinalizeAndResetSprite(centerCoreSprite_);
    FinalizeAndResetSprite(centerGlintBar_);
    FinalizeAndResetSprite(centerFlashSprite_);

    if (offscreen_ && savedPostEffect_ >= 0) {
        offscreen_->SetPostEffect(static_cast<OffScreenRendering::PostEffect>(savedPostEffect_));
        savedPostEffect_ = -1;
    }

    spriteCom_ = nullptr;
    windowAPI_ = nullptr;
    offscreen_ = nullptr;
    isActive_ = false;
}

void StingerTransition::SetTheme(TransitionTheme theme) {
    theme_ = theme;
    ApplyThemeColors();
}

void StingerTransition::ApplyThemeColors() {
    if (stripes_.size() < kStripeCount) return;

    size_t themeIdx = static_cast<size_t>(theme_);
    if (themeIdx >= sizeof(kThemePalettes) / sizeof(kThemePalettes[0])) {
        themeIdx = static_cast<size_t>(TransitionTheme::Monochrome);
    }

    for (size_t i = 0; i < kStripeCount; ++i) {
        stripes_[i].color = kThemePalettes[themeIdx][i];
        if (stripes_[i].sprite) {
            stripes_[i].sprite->SetColor(stripes_[i].color);
        }
    }

    if (safetyBackdrop_) {
        Vector4 bgCol = stripes_[kMainCurtainIndex].color;
        bgCol.w = 0.0f;
        safetyBackdrop_->SetColor(bgCol);
    }
}

void StingerTransition::StartTransition(const std::string& nextSceneName, float duration,
                                       TransitionTheme theme) {
    if (isActive_) return;

    pendingSceneName_ = nextSceneName;
    duration_ = (std::max)(kMinDuration, duration);
    timer_ = 0.0f;
    progress_ = 0.0f;
    postSwitchTimer_ = 0.0f;
    isActive_ = true;
    hasSwitched_ = false;

    SetTheme(theme);
    RefreshViewportMetrics();

    if (safetyBackdrop_) {
        safetyBackdrop_->SetSize({ screenWidth_ + kBackdropMarginTotal, screenHeight_ + kBackdropMarginTotal });
        safetyBackdrop_->SetPosition({ kBackdropMarginOffset, kBackdropMarginOffset });
        safetyBackdrop_->SetRotation(0.0f);
        Vector4 bgCol = (stripes_.size() > kMainCurtainIndex) ? stripes_[kMainCurtainIndex].color : kFallbackBackdropOpaque;
        bgCol.w = 0.0f;
        safetyBackdrop_->SetColor(bgCol);
    }

    if (topLetterbox_)      topLetterbox_->SetSize({ screenWidth_, kLetterboxHeightPx });
    if (bottomLetterbox_)   bottomLetterbox_->SetSize({ screenWidth_, kLetterboxHeightPx });
    if (centerFlashSprite_) centerFlashSprite_->SetSize({ screenWidth_, screenHeight_ });

    if (offscreen_) {
        savedPostEffect_ = static_cast<int>(offscreen_->GetPostEffect());
        offscreen_->SetPostEffect(OffScreenRendering::PostEffect::Normal);
    }

    TacticalAudio::Play2D(kSeWhooshPath, kWhooshVolume, kWhooshCooldownMs);
}

void StingerTransition::Update(float deltaTime) {
    if (!isActive_) {
#if APP_DEBUG_UI
        const bool ctrlHeld = IsAppKeyDown(VK_CONTROL);
        const bool curF1 = ctrlHeld && IsAppKeyDown(VK_F1);
        const bool curF2 = ctrlHeld && IsAppKeyDown(VK_F2);
        const bool curF3 = ctrlHeld && IsAppKeyDown(VK_F3);
        const bool curF4 = ctrlHeld && IsAppKeyDown(VK_F4);
        const bool curF6 = ctrlHeld && IsAppKeyDown(VK_F6);

        static bool prevF1 = false, prevF2 = false, prevF3 = false, prevF4 = false, prevF6 = false;
        const bool trigF1 = curF1 && !prevF1; prevF1 = curF1;
        const bool trigF2 = curF2 && !prevF2; prevF2 = curF2;
        const bool trigF3 = curF3 && !prevF3; prevF3 = curF3;
        const bool trigF4 = curF4 && !prevF4; prevF4 = curF4;
        const bool trigF6 = curF6 && !prevF6; prevF6 = curF6;

        if (trigF1) { StartTransition("TITLE", kDefaultDuration); return; }
        if (trigF2) { StartTransition("GAMEPLAY", kDefaultDuration); return; }
        if (trigF3) { StartTransition("GAMEOVER", kDefaultDuration); return; }
        if (trigF4) { StartTransition("CLEAR", kDefaultDuration); return; }
        if (trigF6) {
            const int nextTheme = (static_cast<int>(theme_) + 1) % kCycleThemeCount;
            SetTheme(static_cast<TransitionTheme>(nextTheme));
        }
#endif
        return;
    }

    timer_ += (std::min)(deltaTime, kMaxFrameDeltaTime);
    progress_ = std::clamp(timer_ / duration_, 0.0f, 1.0f);

    const float cx = screenWidth_ * 0.5f;
    const float cy = screenHeight_ * 0.5f;
    const float startX = cx + travelDistance_;
    const float endX   = cx - travelDistance_;
    const float t = progress_;

    if (t >= kSceneSwitchPoint && !hasSwitched_) {
        hasSwitched_ = true;
        if (!pendingSceneName_.empty()) {
            SceneManager::GetInstance()->ChangeScene(pendingSceneName_);
        }
    }

    // 1. 多重ストライプのカスケード更新
    for (size_t i = 0; i < stripes_.size() && i < kStripeCount; ++i) {
        auto& stripe = stripes_[i];
        if (!stripe.sprite) continue;

        const float offsetT = std::clamp(t - kStripeSpecs[i].cascadeDelay, 0.0f, 1.0f);
        const float x = EvaluateSmoothContinuousPosition(offsetT, startX, cx, endX) + stripe.offsetX;

        stripe.sprite->SetPosition({ x, cy });
        stripe.sprite->SetRotation(stripeAngle_);
        stripe.sprite->SetSize(stripe.size);
        stripe.sprite->SetColor(stripe.color);
        stripe.sprite->Update();
    }

    // 1.5. 全画面密閉セーフティバックドロップ
    if (safetyBackdrop_) {
        float bgAlpha = 0.0f;
        if (t >= kBackdropWindowStart && t <= kBackdropWindowEnd) {
            if (t < kBackdropHoldStart) {
                bgAlpha = SmoothStep01((t - kBackdropWindowStart) / kBackdropFadeSpan);
            } else if (t <= kBackdropHoldEnd) {
                bgAlpha = 1.0f;
            } else {
                bgAlpha = 1.0f - SmoothStep01((t - kBackdropHoldEnd) / kBackdropFadeSpan);
            }
        }

        if (bgAlpha > kBackdropDrawMinAlpha) {
            const Vector4 col = (stripes_.size() > kMainCurtainIndex) ? stripes_[kMainCurtainIndex].color : kFallbackBackdropOpaque;
            safetyBackdrop_->SetPosition({ kBackdropMarginOffset, kBackdropMarginOffset });
            safetyBackdrop_->SetSize({ screenWidth_ + kBackdropMarginTotal, screenHeight_ + kBackdropMarginTotal });
            safetyBackdrop_->SetColor(col);
            safetyBackdrop_->SetAlpha(bgAlpha);
        } else {
            safetyBackdrop_->SetPosition(kOffscreenPos);
            safetyBackdrop_->SetAlpha(0.0f);
        }
    }

    // 2. シネマティック・レターボックス開閉
    float letterboxOffset = 0.0f;
    if (t < kLetterboxAnimSpan) {
        letterboxOffset = (1.0f - SmoothStep01(t / kLetterboxAnimSpan)) * kLetterboxSlideOffset;
    } else if (t > kHoldWindowEnd) {
        letterboxOffset = SmoothStep01((t - kHoldWindowEnd) / kLetterboxAnimSpan) * kLetterboxSlideOffset;
    }
    if (topLetterbox_)    topLetterbox_->SetPosition({ 0.0f, letterboxOffset });
    if (bottomLetterbox_) bottomLetterbox_->SetPosition({ 0.0f, screenHeight_ - letterboxOffset });

    // 3. 中央アイキャッチ・エンブレム演出
    if (t >= kHoldWindowStart && t <= kHoldWindowEnd) {
        const float u = (t - kHoldWindowStart) / kHoldWindowSpan;
        const float pulse = std::sin(u * kPi);

        if (centerFrameSprite_) {
            const float size = kFrameBaseSize * (kFrameScaleMin + pulse * kFrameScalePulse);
            centerFrameSprite_->SetPosition({ cx, cy });
            centerFrameSprite_->SetSize({ size, size });
            centerFrameSprite_->SetRotation(kQuarterPi - u * kFrameSpinRad);
            centerFrameSprite_->SetColor((stripes_.size() > kFrameColorStripeIndex) ? stripes_[kFrameColorStripeIndex].color : kFallbackFrameColor);
            centerFrameSprite_->SetAlpha(pulse * kFrameMaxAlpha);
        }
        if (centerCoreSprite_) {
            const float size = kCoreBaseSize * (kCoreScaleMin + pulse * kCoreScalePulse);
            centerCoreSprite_->SetPosition({ cx, cy });
            centerCoreSprite_->SetSize({ size, size });
            centerCoreSprite_->SetRotation(kQuarterPi + u * kCoreSpinRad);
            centerCoreSprite_->SetColor(kOpaqueWhite);
            centerCoreSprite_->SetAlpha(pulse);
        }
        if (centerGlintBar_) {
            const float barW = kGlintBaseWidth  * (kGlintWidthMin  + pulse * kGlintWidthPulse);
            const float barH = kGlintBaseHeight * (kGlintHeightMin + pulse * kGlintHeightPulse);
            centerGlintBar_->SetPosition({ cx, cy });
            centerGlintBar_->SetSize({ barW, barH });
            centerGlintBar_->SetColor((stripes_.size() > kGlintColorStripeIndex) ? stripes_[kGlintColorStripeIndex].color : kFallbackGlintColor);
            centerGlintBar_->SetAlpha(pulse * kGlintMaxAlpha);
        }
    } else {
        ParkSpriteOffscreen(centerFrameSprite_.get());
        ParkSpriteOffscreen(centerCoreSprite_.get());
        ParkSpriteOffscreen(centerGlintBar_.get());
    }

    // 4. シーン切り替え直後のインパクト白フラッシュ
    if (centerFlashSprite_) {
        if (t >= kSceneSwitchPoint && t <= kFlashEndPoint) {
            const float flashU = (t - kSceneSwitchPoint) / kFlashDuration;
            centerFlashSprite_->SetColor(kOpaqueWhite);
            centerFlashSprite_->SetAlpha(kFlashPeakAlpha * (1.0f - flashU * flashU));
            centerFlashSprite_->SetPosition({ 0.0f, 0.0f });
        } else {
            centerFlashSprite_->SetAlpha(0.0f);
        }
    }

    // 5. トランジション完了処理
    if (progress_ >= 1.0f) {
        isActive_ = false;
        if (offscreen_ && savedPostEffect_ >= 0) {
            offscreen_->SetPostEffect(static_cast<OffScreenRendering::PostEffect>(savedPostEffect_));
            savedPostEffect_ = -1;
        }

        if (safetyBackdrop_) {
            safetyBackdrop_->SetPosition(kOffscreenPos);
            safetyBackdrop_->SetColor(kTransparentBlack);
            safetyBackdrop_->Update();
        }
        for (auto& stripe : stripes_) {
            if (stripe.sprite) {
                stripe.sprite->SetPosition(kOffscreenPos);
                stripe.sprite->Update();
            }
        }
        ParkSpriteOffscreen(topLetterbox_.get(), { 0.0f, kOffscreenCoord });
        ParkSpriteOffscreen(bottomLetterbox_.get(), { 0.0f, kOffscreenCoord });
        ParkSpriteOffscreen(centerFrameSprite_.get());
        ParkSpriteOffscreen(centerCoreSprite_.get());
        ParkSpriteOffscreen(centerGlintBar_.get());
        if (centerFlashSprite_) {
            centerFlashSprite_->SetColor(kTransparentBlack);
            centerFlashSprite_->Update();
        }
    }
}

void StingerTransition::Draw(const RenderContext& ctx) {
    if (!isActive_ || !spriteCom_ || !ctx.commandList) return;

    auto* cmdList = ctx.commandList;
    spriteCom_->SetupDraw(cmdList);

    DrawSpriteIfVisible(cmdList, safetyBackdrop_.get(), kBackdropDrawMinAlpha);

    for (int idx : kStripeDrawOrder) {
        if (idx < static_cast<int>(stripes_.size()) && stripes_[idx].sprite) {
            stripes_[idx].sprite->Update();
            stripes_[idx].sprite->Draw(cmdList);
        }
    }

    DrawSpriteIfVisible(cmdList, centerGlintBar_.get(), kEmblemDrawMinAlpha);
    DrawSpriteIfVisible(cmdList, centerFrameSprite_.get(), kEmblemDrawMinAlpha);
    DrawSpriteIfVisible(cmdList, centerCoreSprite_.get(), kEmblemDrawMinAlpha);
    DrawSpriteIfVisible(cmdList, centerFlashSprite_.get(), kEmblemDrawMinAlpha);
    DrawSpriteIfVisible(cmdList, topLetterbox_.get());
    DrawSpriteIfVisible(cmdList, bottomLetterbox_.get());
}
