#include "StingerTransition.h"

#include "Sprite.h"
#include "SpriteCom.h"
#include "WindowsAPI.h"
#include "SceneManager.h"
#include "OffScreenRendering.h"

#include <algorithm>
#include <cmath>

namespace {
    constexpr float kPi = 3.14159265f;

    /// @brief 3次エルミート補間（始点・終点・両端の速度を厳密に一致させる）
    float Hermite(float p0, float m0, float p1, float m1, float s) {
        float s2 = s * s;
        float s3 = s2 * s;
        float h00 = 2.0f * s3 - 3.0f * s2 + 1.0f;
        float h10 = s3 - 2.0f * s2 + s;
        float h01 = -2.0f * s3 + 3.0f * s2;
        float h11 = s3 - s2;
        return h00 * p0 + h10 * m0 + h01 * p1 + h11 * m1;
    }

    /**
     * @brief 速度が全区間で滑らかに連続する極上スプライントランジションカーブ
     * 
     * 1. 突入 (0.00 〜 0.36): 画面外から心地よく加速し、中央手前へスムーズに減速進入
     * 2. 中央ドリフト (0.36 〜 0.64): 画面を100%覆ったまま、時速数ピクセルの優雅な微速で流れる（完全停止のショックをゼロ化）
     * 3. 脱出 (0.64 〜 1.00): 微速から自然に二次加速し、画面外へ美しく去っていく
     * 境界での速度が数学的に完全に一致するため、カクつきや急発進が物理的に一切起きません。
     */
    float EvaluateSmoothContinuousPosition(float t, float startX, float cx, float endX) {
        t = std::clamp(t, 0.0f, 1.0f);

        const float t1 = 0.36f; // 突入完了地点
        const float t2 = 0.64f; // 脱出開始地点

        // 中央微速ドリフト区間 (cx + 90px -> cx - 90px)
        const float mid1 = cx + 90.0f;
        const float mid2 = cx - 90.0f;
        const float driftDist = mid2 - mid1; // -180px
        const float driftDuration = t2 - t1; // 0.28
        const float vDrift = driftDist;       // 単位sあたりの速度

        if (t < t1) {
            // 区間1: 突入
            float s = t / t1;
            float m0 = 0.0f;
            float m1 = vDrift * (t1 / driftDuration);
            return Hermite(startX, m0, mid1, m1, s);
        } else if (t <= t2) {
            // 区間2: 中央ドリフト（画面を密閉したまま優雅に微速スライド）
            float s = (t - t1) / driftDuration;
            return mid1 + driftDist * s;
        } else {
            // 区間3: 脱出
            float s = (t - t2) / (1.0f - t2);
            float m0 = vDrift * ((1.0f - t2) / driftDuration);
            float m1 = 0.0f;
            return Hermite(mid2, m0, endX, m1, s);
        }
    }
}

StingerTransition* StingerTransition::GetInstance() {
    static StingerTransition instance;
    return &instance;
}

void StingerTransition::Initialize(SpriteCom* spriteCom, WindowAPI* windowAPI) {
    spriteCom_ = spriteCom;
    windowAPI_ = windowAPI;

    if (!spriteCom_ || !windowAPI_) {
        return;
    }

    screenWidth_ = static_cast<float>(WindowAPI::GetClientWidth());
    screenHeight_ = static_cast<float>(WindowAPI::GetClientHeight());
    if (screenWidth_ <= 0.0f) screenWidth_ = 1280.0f;
    if (screenHeight_ <= 0.0f) screenHeight_ = 720.0f;
    travelDistance_ = screenWidth_ * 2.2f;

    stripes_.clear();

    const std::string whiteTexPath = "Resources/CG4/human/white.png";
    Sprite::Transform defaultTransform = {
        {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}
    };

    // 8層の洗練された立体多重ストライプ
    // 0: 先導極細スパークライン (Lead Spark)
    // 1: ネオンポップアクセント帯 (Pop Accent)
    // 2: シャープエッジホワイト光線 (White Edge)
    // 3: ディープスレートシャドウ帯 (Deep Shadow)
    // 4: メインシアンブルー中帯 (Cyan Mid Stripe)
    // 5: ゴールドスパークライン (Gold Spark)
    // 6: メイン全画面密閉ウルトラワイドカーテン (Main Curtain)
    // 7: 後追いシャドウアクセント帯 (Trailing Accent)
    struct StripeDef {
        Vector2 size;
        float offsetX;
    };
    const StripeDef defs[8] = {
        { {   10.0f, 3200.0f }, -340.0f },
        { {   50.0f, 3200.0f }, -270.0f },
        { {   18.0f, 3200.0f }, -210.0f },
        { {   85.0f, 3200.0f }, -130.0f },
        { {  240.0f, 3200.0f },  -50.0f },
        { {   14.0f, 3200.0f },   15.0f },
        { { 4400.0f, 3200.0f },   60.0f },
        { {  120.0f, 3200.0f }, -410.0f },
    };

    for (int i = 0; i < 8; ++i) {
        StripeLayer layer;
        layer.size = defs[i].size;
        layer.offsetX = defs[i].offsetX;
        layer.sprite = Sprite::Create(spriteCom_, defaultTransform, whiteTexPath);
        if (layer.sprite) {
            layer.sprite->SetAnchorPoint({ 0.5f, 0.5f });
            layer.sprite->SetSize(layer.size);
            layer.sprite->SetRotation(stripeAngle_);
            layer.sprite->SetPosition({ -9999.0f, -9999.0f });
        }
        stripes_.emplace_back(std::move(layer));
    }

    // 全画面完全密閉セーフティバックドロップ（回転なし水平プレートで四隅のチラ見えを100%遮断）
    safetyBackdrop_ = Sprite::Create(spriteCom_, defaultTransform, whiteTexPath);
    if (safetyBackdrop_) {
        safetyBackdrop_->SetAnchorPoint({ 0.0f, 0.0f });
        safetyBackdrop_->SetSize({ screenWidth_ + 40.0f, screenHeight_ + 40.0f });
        safetyBackdrop_->SetPosition({ -20.0f, -20.0f });
        safetyBackdrop_->SetRotation(0.0f);
        safetyBackdrop_->SetColor({ 0.08f, 0.09f, 0.12f, 0.0f });
    }

    // シネマティック・レターボックス（画面上下の黒帯）
    topLetterbox_ = Sprite::Create(spriteCom_, defaultTransform, whiteTexPath);
    if (topLetterbox_) {
        topLetterbox_->SetAnchorPoint({ 0.0f, 0.0f });
        topLetterbox_->SetSize({ screenWidth_, 28.0f });
        topLetterbox_->SetPosition({ 0.0f, -40.0f });
        topLetterbox_->SetColor({ 0.04f, 0.05f, 0.07f, 1.0f });
    }

    bottomLetterbox_ = Sprite::Create(spriteCom_, defaultTransform, whiteTexPath);
    if (bottomLetterbox_) {
        bottomLetterbox_->SetAnchorPoint({ 0.0f, 1.0f });
        bottomLetterbox_->SetSize({ screenWidth_, 28.0f });
        bottomLetterbox_->SetPosition({ 0.0f, screenHeight_ + 40.0f });
        bottomLetterbox_->SetColor({ 0.04f, 0.05f, 0.07f, 1.0f });
    }

    // 中央アイキャッチ 1: 外枠回転ダイヤモンド
    centerFrameSprite_ = Sprite::Create(spriteCom_, defaultTransform, whiteTexPath);
    if (centerFrameSprite_) {
        centerFrameSprite_->SetAnchorPoint({ 0.5f, 0.5f });
        centerFrameSprite_->SetSize({ 130.0f, 130.0f });
        centerFrameSprite_->SetRotation(0.785398f);
        centerFrameSprite_->SetPosition({ -9999.0f, -9999.0f });
        centerFrameSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
    }

    // 中央アイキャッチ 2: 内側コアダイヤモンド
    centerCoreSprite_ = Sprite::Create(spriteCom_, defaultTransform, whiteTexPath);
    if (centerCoreSprite_) {
        centerCoreSprite_->SetAnchorPoint({ 0.5f, 0.5f });
        centerCoreSprite_->SetSize({ 65.0f, 65.0f });
        centerCoreSprite_->SetRotation(0.785398f);
        centerCoreSprite_->SetPosition({ -9999.0f, -9999.0f });
        centerCoreSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
    }

    // 中央アイキャッチ 3: 水平閃光スリットバー
    centerGlintBar_ = Sprite::Create(spriteCom_, defaultTransform, whiteTexPath);
    if (centerGlintBar_) {
        centerGlintBar_->SetAnchorPoint({ 0.5f, 0.5f });
        centerGlintBar_->SetSize({ 600.0f, 6.0f });
        centerGlintBar_->SetRotation(0.0f);
        centerGlintBar_->SetPosition({ -9999.0f, -9999.0f });
        centerGlintBar_->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
    }

    // 中央インパクト白フラッシュ
    centerFlashSprite_ = Sprite::Create(spriteCom_, defaultTransform, whiteTexPath);
    if (centerFlashSprite_) {
        centerFlashSprite_->SetAnchorPoint({ 0.0f, 0.0f });
        centerFlashSprite_->SetSize({ screenWidth_, screenHeight_ });
        centerFlashSprite_->SetPosition({ 0.0f, 0.0f });
        centerFlashSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
    }

    ApplyThemeColors();
    isActive_ = false;
    hasSwitched_ = false;
    timer_ = 0.0f;
    progress_ = 0.0f;
    postSwitchTimer_ = 0.0f;
}

void StingerTransition::Finalize() {
    for (auto& stripe : stripes_) {
        if (stripe.sprite) {
            stripe.sprite->Finalize();
            stripe.sprite.reset();
        }
    }
    stripes_.clear();

    if (safetyBackdrop_) { safetyBackdrop_->Finalize(); safetyBackdrop_.reset(); }
    if (topLetterbox_) { topLetterbox_->Finalize(); topLetterbox_.reset(); }
    if (bottomLetterbox_) { bottomLetterbox_->Finalize(); bottomLetterbox_.reset(); }
    if (centerFrameSprite_) { centerFrameSprite_->Finalize(); centerFrameSprite_.reset(); }
    if (centerCoreSprite_) { centerCoreSprite_->Finalize(); centerCoreSprite_.reset(); }
    if (centerGlintBar_) { centerGlintBar_->Finalize(); centerGlintBar_.reset(); }
    if (centerFlashSprite_) { centerFlashSprite_->Finalize(); centerFlashSprite_.reset(); }

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
    if (stripes_.size() < 8) return;

    switch (theme_) {
    case TransitionTheme::TitleTactical:
        // タイトル調和: ダックアンバーゴールド × シグナルイエロー × タクティカルブラック × クリーンホワイト
        // タイトル画面のロゴ（{1.0, 0.8, 0.35}）、主役アヒル（イエロー）、背景コンテナ（ダークスレート）、清流（クリアシアン）に完全調和
        stripes_[0].color = { 1.0f, 1.0f, 1.0f, 1.0f };       // スパーク: 純白ハイライト
        stripes_[1].color = { 1.0f, 0.48f, 0.12f, 1.0f };     // アクセント: タクティカルオレンジ
        stripes_[2].color = { 0.98f, 0.98f, 1.0f, 1.0f };     // エッジ: クリーンホワイト
        stripes_[3].color = { 0.14f, 0.16f, 0.20f, 1.0f };     // シャドウ: ミリタリーガンメタル
        stripes_[4].color = { 1.0f, 0.80f, 0.22f, 1.0f };     // 中帯: ダックシグナルイエローゴールド
        stripes_[5].color = { 0.35f, 0.85f, 1.0f, 1.0f };     // スパーク: 清流シアン
        stripes_[6].color = { 0.08f, 0.09f, 0.12f, 1.0f };     // メインカーテン: タイトルダークスレート
        stripes_[7].color = { 0.85f, 0.55f, 0.15f, 1.0f };     // 後追い: アンバーゴールド
        break;

    case TransitionTheme::TororoPastel:
        // 猫麦とろろ風: 爽快スカイブルー × ピュアホワイト × パステルピンク × ゴールド
        stripes_[0].color = { 1.0f, 1.0f, 1.0f, 1.0f };       // スパーク: 純白
        stripes_[1].color = { 1.0f, 0.38f, 0.65f, 1.0f };     // ポップアクセント: パステルピンク
        stripes_[2].color = { 0.96f, 0.98f, 1.0f, 1.0f };     // エッジ: ホワイト
        stripes_[3].color = { 0.12f, 0.22f, 0.38f, 1.0f };     // シャドウ: ディープスレートブルー
        stripes_[4].color = { 0.22f, 0.72f, 0.98f, 1.0f };     // 中帯: 爽快スカイブルー
        stripes_[5].color = { 1.0f, 0.90f, 0.35f, 1.0f };     // スパーク: パステルゴールド
        stripes_[6].color = { 0.14f, 0.52f, 0.86f, 1.0f };     // メインカーテン: トロロブルー
        stripes_[7].color = { 0.40f, 0.35f, 0.70f, 1.0f };     // 後追い: パステルパープル
        break;

    case TransitionTheme::DuckovTactical:
        // Duckov風: タクティカルブラック × シグナルイエロー × タクティカルオレンジ
        stripes_[0].color = { 1.0f, 0.95f, 0.85f, 1.0f };
        stripes_[1].color = { 0.95f, 0.40f, 0.10f, 1.0f };     // タクティカルオレンジ
        stripes_[2].color = { 1.0f, 0.85f, 0.15f, 1.0f };     // シグナルイエロー
        stripes_[3].color = { 0.18f, 0.20f, 0.24f, 1.0f };     // ガンメタル
        stripes_[4].color = { 0.32f, 0.35f, 0.40f, 1.0f };     // タクティカルグレー
        stripes_[5].color = { 1.0f, 0.75f, 0.20f, 1.0f };     // イエローハイライト
        stripes_[6].color = { 0.08f, 0.09f, 0.11f, 1.0f };     // ダークスレート
        stripes_[7].color = { 0.22f, 0.24f, 0.28f, 1.0f };
        break;

    case TransitionTheme::CyberNeon:
        // サイバーネオン: ネオンマゼンタ × サイバーシアン × エレクトリックイエロー
        stripes_[0].color = { 1.0f, 1.0f, 1.0f, 1.0f };
        stripes_[1].color = { 1.0f, 0.15f, 0.55f, 1.0f };     // ネオンマゼンタ
        stripes_[2].color = { 0.10f, 0.95f, 0.98f, 1.0f };     // サイバーシアン
        stripes_[3].color = { 0.18f, 0.05f, 0.30f, 1.0f };     // ディープバイオレット
        stripes_[4].color = { 0.05f, 0.65f, 0.85f, 1.0f };     // エレクトリックブルー
        stripes_[5].color = { 1.0f, 0.95f, 0.15f, 1.0f };     // ネオンイエロー
        stripes_[6].color = { 0.55f, 0.08f, 0.45f, 1.0f };     // ネオンパープル
        stripes_[7].color = { 0.80f, 0.10f, 0.40f, 1.0f };
        break;

    case TransitionTheme::Monochrome:
    default:
        // ミニマルシック: ディープブラック × シルバーホワイト
        stripes_[0].color = { 1.0f, 1.0f, 1.0f, 1.0f };
        stripes_[1].color = { 0.85f, 0.87f, 0.90f, 1.0f };
        stripes_[2].color = { 1.0f, 1.0f, 1.0f, 1.0f };
        stripes_[3].color = { 0.22f, 0.22f, 0.25f, 1.0f };
        stripes_[4].color = { 0.50f, 0.52f, 0.56f, 1.0f };
        stripes_[5].color = { 0.90f, 0.92f, 0.95f, 1.0f };
        stripes_[6].color = { 0.06f, 0.06f, 0.07f, 1.0f };
        stripes_[7].color = { 0.15f, 0.15f, 0.17f, 1.0f };
        break;
    }

    for (auto& stripe : stripes_) {
        if (stripe.sprite) {
            stripe.sprite->SetColor(stripe.color);
        }
    }

    if (safetyBackdrop_ && stripes_.size() >= 7) {
        Vector4 bgCol = stripes_[6].color;
        bgCol.w = 0.0f;
        safetyBackdrop_->SetColor(bgCol);
    }
}

void StingerTransition::StartTransition(const std::string& nextSceneName, float duration,
                                       TransitionTheme theme) {
    if (isActive_) {
        return; // 二重呼び出しガード
    }

    pendingSceneName_ = nextSceneName;
    duration_ = (std::max)(0.25f, duration);
    timer_ = 0.0f;
    progress_ = 0.0f;
    postSwitchTimer_ = 0.0f;
    isActive_ = true;
    hasSwitched_ = false;

    SetTheme(theme);

    // 画面解像度の最新化（リサイズ等に対応）
    screenWidth_ = static_cast<float>(WindowAPI::GetClientWidth());
    screenHeight_ = static_cast<float>(WindowAPI::GetClientHeight());
    if (screenWidth_ <= 0.0f) screenWidth_ = 1280.0f;
    if (screenHeight_ <= 0.0f) screenHeight_ = 720.0f;
    travelDistance_ = screenWidth_ * 2.2f;

    if (safetyBackdrop_) {
        safetyBackdrop_->SetSize({ screenWidth_ + 40.0f, screenHeight_ + 40.0f });
        safetyBackdrop_->SetPosition({ -20.0f, -20.0f });
        safetyBackdrop_->SetRotation(0.0f);
        Vector4 bgCol = (stripes_.size() >= 7) ? stripes_[6].color : Vector4{ 0.08f, 0.09f, 0.12f, 1.0f };
        bgCol.w = 0.0f;
        safetyBackdrop_->SetColor(bgCol);
    }

    if (topLetterbox_) {
        topLetterbox_->SetSize({ screenWidth_, 28.0f });
    }
    if (bottomLetterbox_) {
        bottomLetterbox_->SetSize({ screenWidth_, 28.0f });
    }
    if (centerFlashSprite_) {
        centerFlashSprite_->SetSize({ screenWidth_, screenHeight_ });
    }

    // トランジション中は深度ベースアウトラインを抑制し、ストライプの上に輪郭線が透けるのを完全に防止
    if (offscreen_) {
        savedPostEffect_ = static_cast<int>(offscreen_->GetPostEffect());
        offscreen_->SetPostEffect(OffScreenRendering::PostEffect::Normal);
    }
}

void StingerTransition::Update(float deltaTime) {
    // 開発・テスト用ショートカット（Releaseモードでもワンキーでシーン切り替えトランジションをテスト可能）
    if (!isActive_) {
        if (GetAsyncKeyState(VK_F1) & 0x0001) { StartTransition("TITLE", 1.80f); return; }
        if (GetAsyncKeyState(VK_F2) & 0x0001) { StartTransition("GAMEPLAY", 1.80f); return; }
        if (GetAsyncKeyState(VK_F3) & 0x0001) { StartTransition("GAMEOVER", 1.80f); return; }
        if (GetAsyncKeyState(VK_F4) & 0x0001) { StartTransition("CLEAR", 1.80f); return; }
        if (GetAsyncKeyState(VK_F6) & 0x0001) {
            int nextTheme = (static_cast<int>(theme_) + 1) % 4;
            SetTheme(static_cast<TransitionTheme>(nextTheme));
        }
        return;
    }

    // 【マイクロスタッター完全排除】
    // 60FPSベースの滑らかなタイムステップで、タイマー微小誤差によるジッターを完全防止
    float clampedDt = (std::min)(deltaTime, 1.0f / 60.0f);
    timer_ += clampedDt;
    progress_ = std::clamp(timer_ / duration_, 0.0f, 1.0f);

    const float cx = screenWidth_ * 0.5f;
    const float cy = screenHeight_ * 0.5f;
    const float startX = cx + travelDistance_;
    const float endX   = cx - travelDistance_;

    // =========================================================================
    // 【極上エルミート連続スプライン曲線】
    // 速度が全区間で滑らかに連続し、完全停止のショックをゼロ化。
    // 中央では画面を完全に密閉したまま、優雅に微速ドリフト（約0.5秒間）します。
    // =========================================================================
    float t = progress_;

    // 安全な中間地点（0.48f）でシーン切り替えを実行！
    // 画面中央は 4400px の超特大カーテンで 100% 密閉されているため、裏のロードヒッチは一切見えません。
    if (t >= 0.48f && !hasSwitched_) {
        hasSwitched_ = true;
        if (!pendingSceneName_.empty()) {
            SceneManager::GetInstance()->ChangeScene(pendingSceneName_);
        }
    }

    // -------------------------------------------------------------------------
    // 1. 各ストライプの座標・描画更新（波打つカスケードオフセット）
    // -------------------------------------------------------------------------
    // 各レイヤーがほんのわずかにずれて追従することで、単なる「板」ではなく波打つリッチな質感を実現
    const float cascadeOffsets[8] = {
        0.000f, // 0: 先導スパーク（最先鋒）
        0.008f, // 1: ピンク
        0.016f, // 2: ホワイト
        0.024f, // 3: シャドウ
        0.032f, // 4: シアン
        0.040f, // 5: ゴールド
        0.048f, // 6: メインカーテン
        0.060f  // 7: 後追いシャドウ
    };

    for (size_t i = 0; i < stripes_.size(); ++i) {
        auto& stripe = stripes_[i];
        if (!stripe.sprite) continue;

        float offsetT = std::clamp(t - cascadeOffsets[i], 0.0f, 1.0f);
        float layerBaseX = EvaluateSmoothContinuousPosition(offsetT, startX, cx, endX);
        float x = layerBaseX + stripe.offsetX;

        stripe.sprite->SetPosition({ x, cy });
        stripe.sprite->SetRotation(stripeAngle_);
        stripe.sprite->SetSize(stripe.size);
        stripe.sprite->SetColor(stripe.color);
        stripe.sprite->Update();
    }

    // -------------------------------------------------------------------------
    // 1.5. 全画面完全密閉セーフティバックドロップ（回転なし水平プレートでチラ見え・四隅漏れを100%遮断）
    // -------------------------------------------------------------------------
    if (safetyBackdrop_) {
        float bgAlpha = 0.0f;
        if (t >= 0.26f && t <= 0.74f) {
            if (t < 0.38f) {
                float u = (t - 0.26f) / 0.12f;
                bgAlpha = u * u * (3.0f - 2.0f * u);
            } else if (t <= 0.62f) {
                bgAlpha = 1.0f; // 中央密閉ホールド（t=0.48でのシーン切り替え時は完全に 1.0f）
            } else {
                float u = (t - 0.62f) / 0.12f;
                bgAlpha = 1.0f - u * u * (3.0f - 2.0f * u);
            }
        }

        if (bgAlpha > 0.001f) {
            Vector4 col = (stripes_.size() >= 7) ? stripes_[6].color : Vector4{ 0.08f, 0.09f, 0.12f, 1.0f };
            safetyBackdrop_->SetPosition({ -20.0f, -20.0f });
            safetyBackdrop_->SetSize({ screenWidth_ + 40.0f, screenHeight_ + 40.0f });
            safetyBackdrop_->SetColor(col);
            safetyBackdrop_->SetAlpha(bgAlpha);
        } else {
            safetyBackdrop_->SetPosition({ -9999.0f, -9999.0f });
            safetyBackdrop_->SetAlpha(0.0f);
        }
    }

    // -------------------------------------------------------------------------
    // 2. シネマティック・レターボックス（上下の黒帯の滑らかな開閉）
    // -------------------------------------------------------------------------
    float letterboxOffset = 0.0f;
    if (t < 0.22f) {
        float s = t / 0.22f;
        letterboxOffset = (1.0f - s * s * (3.0f - 2.0f * s)) * -32.0f;
    } else if (t > 0.78f) {
        float s = (t - 0.78f) / 0.22f;
        letterboxOffset = (s * s * (3.0f - 2.0f * s)) * -32.0f;
    }
    if (topLetterbox_) {
        topLetterbox_->SetPosition({ 0.0f, letterboxOffset });
    }
    if (bottomLetterbox_) {
        bottomLetterbox_->SetPosition({ 0.0f, screenHeight_ - letterboxOffset });
    }

    // -------------------------------------------------------------------------
    // 3. 中央アイキャッチ・エンブレムの優雅な演出（じっくり味わえるゆったり回転）
    // -------------------------------------------------------------------------
    const bool inHoldWindow = (t >= 0.22f && t <= 0.78f);

    if (inHoldWindow) {
        float u = (t - 0.22f) / 0.56f; // 0.0 -> 1.0
        float pulse = std::sin(u * kPi); // 滑らかな正弦波 0.0 -> 1.0 -> 0.0

        // 外枠ダイヤモンド: 一定の角速度で反時計回りに優雅に回転
        if (centerFrameSprite_) {
            float size = 140.0f * (0.75f + pulse * 0.35f);
            float rot = 0.785398f - u * 2.8f;
            centerFrameSprite_->SetPosition({ cx, cy });
            centerFrameSprite_->SetSize({ size, size });
            centerFrameSprite_->SetRotation(rot);
            Vector4 col = (stripes_.size() >= 5) ? stripes_[4].color : Vector4{ 1.0f, 0.8f, 0.2f, 1.0f };
            centerFrameSprite_->SetColor(col);
            centerFrameSprite_->SetAlpha(pulse * 0.95f);
        }

        // 内側コアダイヤモンド: 時計回りに優雅に高速回転
        if (centerCoreSprite_) {
            float size = 68.0f * (0.80f + pulse * 0.35f);
            float rot = 0.785398f + u * 4.5f;
            centerCoreSprite_->SetPosition({ cx, cy });
            centerCoreSprite_->SetSize({ size, size });
            centerCoreSprite_->SetRotation(rot);
            centerCoreSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
            centerCoreSprite_->SetAlpha(pulse);
        }

        // 水平閃光スリットバー: 中央を左右に貫く発光バーが滑らかに展開
        if (centerGlintBar_) {
            float barW = 560.0f * (0.60f + pulse * 0.55f);
            float barH = 5.0f * (0.80f + pulse * 0.50f);
            centerGlintBar_->SetPosition({ cx, cy });
            centerGlintBar_->SetSize({ barW, barH });
            Vector4 col = (stripes_.size() >= 2) ? stripes_[1].color : Vector4{ 1.0f, 0.5f, 0.15f, 1.0f };
            centerGlintBar_->SetColor(col);
            centerGlintBar_->SetAlpha(pulse * 0.90f);
        }
    } else {
        if (centerFrameSprite_) {
            centerFrameSprite_->SetAlpha(0.0f);
            centerFrameSprite_->SetPosition({ -9999.0f, -9999.0f });
        }
        if (centerCoreSprite_) {
            centerCoreSprite_->SetAlpha(0.0f);
            centerCoreSprite_->SetPosition({ -9999.0f, -9999.0f });
        }
        if (centerGlintBar_) {
            centerGlintBar_->SetAlpha(0.0f);
            centerGlintBar_->SetPosition({ -9999.0f, -9999.0f });
        }
    }

    // 4. シーン切り替え直後のインパクト白フラッシュ (0.48 〜 0.60)
    if (centerFlashSprite_) {
        if (t >= 0.48f && t <= 0.60f) {
            float flashU = (t - 0.48f) / 0.12f;
            float flashA = 0.35f * (1.0f - flashU * flashU);
            centerFlashSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
            centerFlashSprite_->SetAlpha(flashA);
            centerFlashSprite_->SetPosition({ 0.0f, 0.0f });
        } else {
            centerFlashSprite_->SetAlpha(0.0f);
        }
    }

    // -------------------------------------------------------------------------
    // 5. トランジション完了
    // -------------------------------------------------------------------------
    if (progress_ >= 1.0f) {
        isActive_ = false;

        // ポストプロセスエフェクトを復元
        if (offscreen_ && savedPostEffect_ >= 0) {
            offscreen_->SetPostEffect(static_cast<OffScreenRendering::PostEffect>(savedPostEffect_));
            savedPostEffect_ = -1;
        }

        // スプライトを画面外へ退避
        if (safetyBackdrop_) {
            safetyBackdrop_->SetPosition({ -9999.0f, -9999.0f });
            safetyBackdrop_->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
            safetyBackdrop_->Update();
        }
        for (auto& stripe : stripes_) {
            if (stripe.sprite) {
                stripe.sprite->SetPosition({ -9999.0f, -9999.0f });
                stripe.sprite->Update();
            }
        }
        if (topLetterbox_) {
            topLetterbox_->SetPosition({ 0.0f, -9999.0f });
            topLetterbox_->Update();
        }
        if (bottomLetterbox_) {
            bottomLetterbox_->SetPosition({ 0.0f, -9999.0f });
            bottomLetterbox_->Update();
        }
        if (centerFrameSprite_) {
            centerFrameSprite_->SetPosition({ -9999.0f, -9999.0f });
            centerFrameSprite_->Update();
        }
        if (centerCoreSprite_) {
            centerCoreSprite_->SetPosition({ -9999.0f, -9999.0f });
            centerCoreSprite_->Update();
        }
        if (centerGlintBar_) {
            centerGlintBar_->SetPosition({ -9999.0f, -9999.0f });
            centerGlintBar_->Update();
        }
        if (centerFlashSprite_) {
            centerFlashSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
            centerFlashSprite_->Update();
        }
    }
}

void StingerTransition::Draw(const RenderContext& ctx) {
    if (!isActive_ || !spriteCom_) {
        return;
    }

    auto* cmdList = ctx.commandList;
    if (!cmdList) {
        return;
    }

    spriteCom_->SetupDraw(cmdList);

    // 0. 全画面完全密閉セーフティバックドロップ（回転なし水平プレートで四隅のチラ見えを100%遮断）
    if (safetyBackdrop_ && safetyBackdrop_->GetColor().w > 0.001f) {
        safetyBackdrop_->Update();
        safetyBackdrop_->Draw(cmdList);
    }

    // 1. 奥（メインカーテン index 6 等）から手前（先導スパークライン index 0）へ順次描画
    const int drawOrder[8] = { 6, 7, 4, 3, 1, 5, 2, 0 };
    for (int idx : drawOrder) {
        if (idx < static_cast<int>(stripes_.size()) && stripes_[idx].sprite) {
            stripes_[idx].sprite->Update();
            stripes_[idx].sprite->Draw(cmdList);
        }
    }

    // 2. 中央アイキャッチ群の描画
    // 水平スリットバー
    if (centerGlintBar_ && centerGlintBar_->GetColor().w > 0.01f) {
        centerGlintBar_->Update();
        centerGlintBar_->Draw(cmdList);
    }
    // 外枠ダイヤモンドフレーム
    if (centerFrameSprite_ && centerFrameSprite_->GetColor().w > 0.01f) {
        centerFrameSprite_->Update();
        centerFrameSprite_->Draw(cmdList);
    }
    // 内側コアダイヤモンド
    if (centerCoreSprite_ && centerCoreSprite_->GetColor().w > 0.01f) {
        centerCoreSprite_->Update();
        centerCoreSprite_->Draw(cmdList);
    }

    // 3. インパクト白フラッシュ
    if (centerFlashSprite_ && centerFlashSprite_->GetColor().w > 0.01f) {
        centerFlashSprite_->Update();
        centerFlashSprite_->Draw(cmdList);
    }

    // 4. シネマティック・レターボックス（最前面に重ねて画面を引き締める）
    if (topLetterbox_) {
        topLetterbox_->Update();
        topLetterbox_->Draw(cmdList);
    }
    if (bottomLetterbox_) {
        bottomLetterbox_->Update();
        bottomLetterbox_->Draw(cmdList);
    }
}
