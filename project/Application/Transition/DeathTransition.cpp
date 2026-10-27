#include "DeathTransition.h"

#include "Sprite.h"
#include "SpriteCom.h"
#include "WindowsAPI.h"
#include "SceneManager.h"
#include "OffScreenRendering.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#ifdef USE_IMGUI
#include "imgui.h"
#endif

namespace {
    constexpr float kPi = 3.14159265358979323846f;

    float EaseOutCubic(float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        return 1.0f - std::pow(1.0f - t, 3.0f);
    }

    float EaseInCubic(float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        return t * t * t;
    }
}

DeathTransition* DeathTransition::GetInstance() {
    static DeathTransition instance;
    return &instance;
}

void DeathTransition::Initialize(SpriteCom* spriteCom, WindowAPI* windowAPI) {
    spriteCom_ = spriteCom;
    windowAPI_ = windowAPI;

    if (!spriteCom_ || !windowAPI_) {
        return;
    }

    screenWidth_ = static_cast<float>(WindowAPI::GetClientWidth());
    screenHeight_ = static_cast<float>(WindowAPI::GetClientHeight());
    if (screenWidth_ <= 0.0f) screenWidth_ = 1280.0f;
    if (screenHeight_ <= 0.0f) screenHeight_ = 720.0f;

    const std::string whiteTexPath = "Resources/white.png";
    Sprite::Transform defaultTransform = {
        { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }
    };

    // 1. 上部・下部 重装甲ブラストシャッター本体（深黒マットチャコールスチール）
    upperShutter_ = Sprite::Create(spriteCom_, defaultTransform, whiteTexPath);
    if (upperShutter_) {
        upperShutter_->SetAnchorPoint({ 0.0f, 0.0f });
        upperShutter_->SetSize({ screenWidth_, screenHeight_ * 0.50f + 16.0f });
        upperShutter_->SetColor({ 0.045f, 0.048f, 0.055f, 1.0f });
    }

    lowerShutter_ = Sprite::Create(spriteCom_, defaultTransform, whiteTexPath);
    if (lowerShutter_) {
        lowerShutter_->SetAnchorPoint({ 0.0f, 0.0f });
        lowerShutter_->SetSize({ screenWidth_, screenHeight_ * 0.50f + 16.0f });
        lowerShutter_->SetColor({ 0.045f, 0.048f, 0.055f, 1.0f });
    }

    // 2. 接合リップトリム（高剛性ガンメタルスチール縁）
    upperLip_ = Sprite::Create(spriteCom_, defaultTransform, whiteTexPath);
    if (upperLip_) {
        upperLip_->SetAnchorPoint({ 0.0f, 0.0f });
        upperLip_->SetSize({ screenWidth_, 10.0f });
        upperLip_->SetColor({ 0.16f, 0.17f, 0.20f, 1.0f });
    }

    lowerLip_ = Sprite::Create(spriteCom_, defaultTransform, whiteTexPath);
    if (lowerLip_) {
        lowerLip_->SetAnchorPoint({ 0.0f, 0.0f });
        lowerLip_->SetSize({ screenWidth_, 10.0f });
        lowerLip_->SetColor({ 0.16f, 0.17f, 0.20f, 1.0f });
    }

    // 3. ミリタリーハザード警告ライン（タクティカルアンバーゴールド）
    upperHazardStripe_ = Sprite::Create(spriteCom_, defaultTransform, whiteTexPath);
    if (upperHazardStripe_) {
        upperHazardStripe_->SetAnchorPoint({ 0.0f, 0.0f });
        upperHazardStripe_->SetSize({ screenWidth_, 3.0f });
        upperHazardStripe_->SetColor({ 0.88f, 0.62f, 0.16f, 0.95f });
    }

    lowerHazardStripe_ = Sprite::Create(spriteCom_, defaultTransform, whiteTexPath);
    if (lowerHazardStripe_) {
        lowerHazardStripe_->SetAnchorPoint({ 0.0f, 0.0f });
        lowerHazardStripe_->SetSize({ screenWidth_, 3.0f });
        lowerHazardStripe_->SetColor({ 0.88f, 0.62f, 0.16f, 0.95f });
    }

    // 4. 接合シームグローライン（中心密閉時の緊急赤色スリット）
    centerSeamBar_ = Sprite::Create(spriteCom_, defaultTransform, whiteTexPath);
    if (centerSeamBar_) {
        centerSeamBar_->SetAnchorPoint({ 0.0f, 0.5f });
        centerSeamBar_->SetSize({ screenWidth_, 2.0f });
        centerSeamBar_->SetColor({ 0.88f, 0.22f, 0.18f, 0.0f });
    }

    // 5. 完全密閉セーフティバックドロップ（光漏れ完全遮断プレート）
    safetyBackdrop_ = Sprite::Create(spriteCom_, defaultTransform, whiteTexPath);
    if (safetyBackdrop_) {
        safetyBackdrop_->SetAnchorPoint({ 0.0f, 0.0f });
        safetyBackdrop_->SetSize({ screenWidth_, screenHeight_ });
        safetyBackdrop_->SetColor({ 0.02f, 0.02f, 0.03f, 0.0f });
    }
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
    timer_ = 0.0f;
    duration_ = (duration > 0.1f) ? duration : 1.70f;
    progress_ = 0.0f;
    pendingSceneName_ = nextSceneName.empty() ? "GAMEOVER" : nextSceneName;

    screenWidth_ = static_cast<float>(WindowAPI::GetClientWidth());
    screenHeight_ = static_cast<float>(WindowAPI::GetClientHeight());
    if (screenWidth_ <= 0.0f) screenWidth_ = 1280.0f;
    if (screenHeight_ <= 0.0f) screenHeight_ = 720.0f;

    // スプライトサイズの解像度追従
    const float halfH = screenHeight_ * 0.50f + 16.0f;
    if (upperShutter_) upperShutter_->SetSize({ screenWidth_, halfH });
    if (lowerShutter_) lowerShutter_->SetSize({ screenWidth_, halfH });
    if (upperLip_) upperLip_->SetSize({ screenWidth_, 10.0f });
    if (lowerLip_) lowerLip_->SetSize({ screenWidth_, 10.0f });
    if (upperHazardStripe_) upperHazardStripe_->SetSize({ screenWidth_, 3.0f });
    if (lowerHazardStripe_) lowerHazardStripe_->SetSize({ screenWidth_, 3.0f });
    if (centerSeamBar_) centerSeamBar_->SetSize({ screenWidth_, 2.0f });
    if (safetyBackdrop_) safetyBackdrop_->SetSize({ screenWidth_, screenHeight_ });

    // トランジション中はアウトラインポストプロセスを通常化してアーティファクトを抑制
    if (offscreen_) {
        savedPostEffect_ = static_cast<int>(offscreen_->GetPostEffect());
        offscreen_->SetPostEffect(OffScreenRendering::PostEffect::Normal);
    }
}

void DeathTransition::Update(float deltaTime) {
    if (!isActive_) return;

    float clampedDt = (std::min)(deltaTime, 1.0f / 60.0f);
    timer_ += clampedDt;
    progress_ = std::clamp(timer_ / duration_, 0.0f, 1.0f);

    const float H = screenHeight_;
    const float halfH = H * 0.50f + 16.0f;
    const float midY = H * 0.50f;

    float upperY = -halfH;
    float lowerY = H;
    float seamAlpha = 0.0f;
    float backdropAlpha = 0.0f;

    // -------------------------------------------------------------------------
    // タイムライン力学:
    // [0.00 〜 0.38]: 上下シャッターが重厚に閉鎖（EaseOutCubic + 接合インパルス）
    // [0.38 〜 0.62]: 完全密閉 ＆ 心電図フラットライン ＆ シーン切り替え（0.48でChangeScene）
    // [0.62 〜 1.00]: ヘリのカーゴドアのように上下に展開（EaseInCubic）してGameOverSceneへ突入
    // -------------------------------------------------------------------------

    if (progress_ < 0.38f) {
        // Phase 1: クランプ閉鎖
        float p = progress_ / 0.38f;
        float ease = EaseOutCubic(p);

        // 接合直前の微小なメカニカル・インパルス跳ね
        float bounce = 0.0f;
        if (p > 0.88f) {
            float bp = (p - 0.88f) / 0.12f;
            bounce = std::sin(bp * kPi) * 2.5f;
        }

        upperY = -halfH * (1.0f - ease) - bounce;
        lowerY = H - (halfH) * ease + bounce;
    }
    else if (progress_ <= 0.62f) {
        // Phase 2: 完全密閉 ＆ 安全なシーン切り替え
        upperY = -16.0f; // 16px オーバーラップで隙間ゼロ
        lowerY = midY;
        backdropAlpha = 1.0f;

        // 接合シームのパルス
        float p = (progress_ - 0.38f) / 0.24f;
        seamAlpha = std::sin(p * kPi) * 0.90f;

        // 中間安全地点（0.48）で裏側で確実にシーン切り替え！
        if (progress_ >= 0.48f && !hasSwitched_) {
            hasSwitched_ = true;
            if (!pendingSceneName_.empty()) {
                SceneManager::GetInstance()->ChangeScene(pendingSceneName_);
            }
        }
    }
    else {
        // Phase 3: カーゴベイ展開（GameOverSceneへ突入）
        float s = (progress_ - 0.62f) / 0.38f;
        float ease = EaseInCubic(s);

        upperY = -16.0f - (halfH) * ease;
        lowerY = midY + (halfH) * ease;
        backdropAlpha = (std::max)(0.0f, 1.0f - s * 3.0f); // 開き始めに即座にフェードアウト
    }

    // スプライト座標の適用
    if (upperShutter_) upperShutter_->SetPosition({ 0.0f, upperY });
    if (upperLip_) upperLip_->SetPosition({ 0.0f, upperY + halfH - 10.0f });
    if (upperHazardStripe_) upperHazardStripe_->SetPosition({ 0.0f, upperY + halfH - 3.0f });

    if (lowerShutter_) lowerShutter_->SetPosition({ 0.0f, lowerY });
    if (lowerLip_) lowerLip_->SetPosition({ 0.0f, lowerY });
    if (lowerHazardStripe_) lowerHazardStripe_->SetPosition({ 0.0f, lowerY });

    if (centerSeamBar_) {
        centerSeamBar_->SetPosition({ 0.0f, midY });
        centerSeamBar_->SetColor({ 0.92f, 0.25f, 0.18f, seamAlpha });
    }

    if (safetyBackdrop_) {
        safetyBackdrop_->SetColor({ 0.02f, 0.02f, 0.03f, backdropAlpha });
    }

    // 終了判定
    if (progress_ >= 1.0f) {
        isActive_ = false;
        if (offscreen_ && savedPostEffect_ >= 0) {
            offscreen_->SetPostEffect(static_cast<OffScreenRendering::PostEffect>(savedPostEffect_));
            savedPostEffect_ = -1;
        }
    }
}

void DeathTransition::Draw(const RenderContext& ctx) {
    if (!isActive_ || !spriteCom_) return;

    auto* cmdList = ctx.commandList;
    if (!cmdList) return;

    spriteCom_->SetupDraw(cmdList);

    // 1. 完全密閉セーフティバックドロップ
    if (safetyBackdrop_ && safetyBackdrop_->GetColor().w > 0.001f) {
        safetyBackdrop_->Update();
        safetyBackdrop_->Draw(cmdList);
    }

    // 2. 上部装甲シャッター＆リップ＆ハザードストライプ
    if (upperShutter_) {
        upperShutter_->Update();
        upperShutter_->Draw(cmdList);
    }
    if (upperLip_) {
        upperLip_->Update();
        upperLip_->Draw(cmdList);
    }
    if (upperHazardStripe_) {
        upperHazardStripe_->Update();
        upperHazardStripe_->Draw(cmdList);
    }

    // 3. 下部装甲シャッター＆リップ＆ハザードストライプ
    if (lowerShutter_) {
        lowerShutter_->Update();
        lowerShutter_->Draw(cmdList);
    }
    if (lowerLip_) {
        lowerLip_->Update();
        lowerLip_->Draw(cmdList);
    }
    if (lowerHazardStripe_) {
        lowerHazardStripe_->Update();
        lowerHazardStripe_->Draw(cmdList);
    }

    // 4. 接合シームグローライン
    if (centerSeamBar_ && centerSeamBar_->GetColor().w > 0.01f) {
        centerSeamBar_->Update();
        centerSeamBar_->Draw(cmdList);
    }
}

void DeathTransition::DrawUI() {
#ifdef USE_IMGUI
    if (!isActive_) return;

    // 密閉区間（0.30 〜 0.70）にミリタリー生体テレメトリと心電図フラットラインを描画
    if (progress_ < 0.25f || progress_ > 0.75f) return;

    ImGuiIO& io = ImGui::GetIO();
    const float sw = io.DisplaySize.x;
    const float sh = io.DisplaySize.y;
    if (sw <= 0.0f || sh <= 0.0f) return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    if (!dl) return;

    float alpha = 1.0f;
    if (progress_ < 0.38f) {
        alpha = (progress_ - 0.25f) / 0.13f;
    } else if (progress_ > 0.62f) {
        alpha = (0.75f - progress_) / 0.13f;
    }
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    int a255 = static_cast<int>(alpha * 255.0f);
    if (a255 <= 0) return;

    const float midY = sh * 0.50f;
    const float midX = sw * 0.50f;

    // 1. 中央心電図フラットラインの描画（ミリタリーアンバー / 緊急レッド）
    float ecgT = (progress_ - 0.30f) / 0.40f;
    float waveProgress = std::clamp(ecgT * 2.2f, 0.0f, 1.0f);

    float startX = midX - 260.0f;
    float endX   = midX + 260.0f;
    float ecgW   = endX - startX;

    // 心電図の波形ポイント
    ImVec2 pts[9] = {
        { startX, midY },
        { startX + ecgW * 0.32f, midY },
        { startX + ecgW * 0.36f, midY - 14.0f },
        { startX + ecgW * 0.40f, midY + 18.0f },
        { startX + ecgW * 0.44f, midY - 32.0f * (1.0f - waveProgress) },
        { startX + ecgW * 0.48f, midY + 12.0f * (1.0f - waveProgress) },
        { startX + ecgW * 0.52f, midY - 6.0f  * (1.0f - waveProgress) },
        { startX + ecgW * 0.56f, midY },
        { endX, midY }
    };

    ImU32 ecgColor = IM_COL32(235, 75, 65, a255);
    for (int i = 0; i < 8; ++i) {
        dl->AddLine(pts[i], pts[i + 1], ecgColor, 2.2f);
    }

#endif
}
