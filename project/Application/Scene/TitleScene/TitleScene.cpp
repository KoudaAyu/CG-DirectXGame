#include "TitleScene.h"

#include "DirectXCom.h"
#include "KeyInput.h"
#include "SceneManager.h"
#include "WindowsAPI.h"

#include "Baziru3_Engine/Core/IO/Mouse/MouseInput.h"
#include "Baziru3_Engine/Graphics/Graphics/SceneRenderRequests.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <utility>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace {

// ===================================================================
// タイトルロゴ（1文字＝1スプライト）
//
// タイトルが決まったら、この配列を書き換えるだけでOK。
// 要素数・並び・1文字ごとの幅は自動でレイアウトに反映される。
// 「やっぱり1枚絵にしたい」場合は、要素を1個だけにして幅を全体幅にすればいい。
//
// 画像がまだ無くてもエンジンが白いダミーテクスチャに差し替えるので、
// このままビルドしても白い四角が並んで動きの確認ができる（落ちない）。
// ===================================================================
struct TitleChar
{
    const char* texture; // 画像パス
    float width;         // 基準幅（ピクセル）。細い文字は小さく、太い文字は大きく
};

constexpr TitleChar kTitleChars[] = {
    {"Resources/UI/Title/logo_char_01.png", 120.0f},
    {"Resources/UI/Title/logo_char_02.png", 120.0f},
    {"Resources/UI/Title/logo_char_03.png", 120.0f},
    {"Resources/UI/Title/logo_char_04.png", 120.0f},
    {"Resources/UI/Title/logo_char_05.png", 120.0f},
};

constexpr int kTitleCharCount = static_cast<int>(std::size(kTitleChars));

// ===================================================================
// ボタン画像
// ===================================================================
struct ButtonAsset
{
    const char* base;  // 通常時
    const char* light; // 点灯時
};

constexpr ButtonAsset kButtonAssets[] = {
    {"Resources/UI/Title/btn_start.png",  "Resources/UI/Title/btn_start_light.png" },
    {"Resources/UI/Title/btn_manual.png", "Resources/UI/Title/btn_manual_light.png"},
    {"Resources/UI/Title/btn_end.png",    "Resources/UI/Title/btn_end_light.png"   },
};

constexpr const char* kMenuLabels[] = {"START", "MANUAL", "END"};

// ===================================================================
// レイアウト初期値（1280x720 基準）
// 実行中は ImGui の "Title Scene" ウィンドウから調整できる
// ===================================================================
constexpr Vector2 kLogoCenterDefault = {420.0f, 150.0f}; // ロゴ全体の中心
constexpr float kLogoScaleDefault = 1.0f;
constexpr float kCharHeightDefault = 160.0f; // 1文字の基準の高さ
constexpr float kCharSpacingDefault = 6.0f;  // 文字間隔

constexpr Vector2 kButtonSize = {320.0f, 90.0f};
constexpr float kButtonCenterX = 950.0f; // ボタン列の中心X
constexpr float kButtonFirstY = 420.0f;  // 一番上（START）の中心Y
constexpr float kButtonStepY = 110.0f;   // ボタン同士の間隔

// ===================================================================
// 演出パラメータ初期値
// ===================================================================
constexpr float kDeltaTime = 1.0f / 60.0f; // SceneManager が固定タイムステップで回している
constexpr float kFadeInSeconds = 0.6f;     // 画面全体フェードインの長さ（秒）

constexpr float kBobAmplitudeDefault = 9.0f;   // 文字の上下ゆれ幅（ピクセル）
constexpr float kBobSpeedDefault = 2.2f;       // 文字の上下ゆれ速度（rad/秒）
constexpr float kJiggleAmountDefault = 0.07f;  // グミ変形の強さ（0.07 = ±7%）
constexpr float kJiggleSpeedDefault = 3.4f;    // グミ変形の速さ（rad/秒）
constexpr float kWobbleDegreesDefault = 3.0f;  // 傾きゆれの角度（度）

constexpr float kPopSeconds = 0.45f; // 1文字が飛び出しきるまでの時間
constexpr float kPopStagger = 0.07f; // 文字ごとの登場ディレイ

constexpr float kButtonPopSeconds = 0.40f;     // ボタン1個が出きるまでの時間
constexpr float kButtonPopStagger = 0.09f;     // ボタンごとの登場ディレイ
constexpr float kButtonPopBeginOffset = 0.15f; // タイトルが出そろってからボタンが出るまでの間

constexpr float kHoverScale = 1.10f;         // ホバー時の拡大率
constexpr float kHoverLerpRate = 14.0f;      // ホバー追従の速さ（1/秒）
constexpr float kBlinkSpeed = 5.0f;          // ライト点滅の速さ（rad/秒）
constexpr float kButtonPulseAmount = 0.03f;  // ホバー中のぷにぷに変形
constexpr float kButtonWobbleSpeed = 4.0f;   // ホバー中の傾きゆれ速度
constexpr float kButtonWobbleDegrees = 2.0f; // ホバー中の傾きゆれ角度（度）

constexpr float kPi = 3.14159265358979323846f;

/// <summary>度をラジアンに</summary>
constexpr float ToRadian(float degree) { return degree * (kPi / 180.0f); }

/// <summary>指数補間でなめらかに目標値へ寄せる</summary>
float Approach(float current, float target, float rate, float deltaTime)
{
    const float t = 1.0f - std::exp(-rate * deltaTime);
    return current + (target - current) * t;
}

/// <summary>行き過ぎて戻るイージング（ぷにっと出る感じ）</summary>
float EaseOutBack(float x)
{
    constexpr float c1 = 1.70158f;
    constexpr float c3 = c1 + 1.0f;
    const float t = x - 1.0f;
    return 1.0f + c3 * t * t * t + c1 * t * t;
}

/// <summary>インデックスから 0..1 の疑似乱数を作る（毎回同じ結果になる）</summary>
float Hash01(uint32_t value)
{
    value = value * 747796405u + 2891336453u;
    uint32_t word = ((value >> ((value >> 28) + 4)) ^ value) * 277803737u;
    word = (word >> 22) ^ word;
    return static_cast<float>(word) / static_cast<float>(0xFFFFFFFFu);
}

} // namespace

// ===================================================================
// 初期化 / 終了
// ===================================================================

void TitleScene::InitializeScene()
{
    if (dxCommon_)
    {
        input_ = new KeyInput();
        input_->Initialize(dxCommon_->GetWindowAPI());

        mouse_ = new MouseInput();
        mouse_->Initialize(dxCommon_->GetWindowAPI());
    }

    ResetTuningToDefault();

    CreateTitleLetters();
    CreateButtons();

    sceneTime_ = 0.0f;
    fadeAlpha_ = 0.0f;
    ClearRequests();
}

void TitleScene::Finalize()
{
    for (TitleLetter& letter : titleLetters_)
    {
        if (letter.sprite)
        {
            letter.sprite->Finalize();
            letter.sprite.reset();
        }
    }
    titleLetters_.clear();

    for (Button& button : buttons_)
    {
        if (button.base)
        {
            button.base->Finalize();
            button.base.reset();
        }
        if (button.light)
        {
            button.light->Finalize();
            button.light.reset();
        }
    }

    delete input_;
    input_ = nullptr;

    delete mouse_;
    mouse_ = nullptr;
}

void TitleScene::ResetTuningToDefault()
{
    logoCenter_ = kLogoCenterDefault;
    logoScale_ = kLogoScaleDefault;
    charHeight_ = kCharHeightDefault;
    charSpacing_ = kCharSpacingDefault;
    bobAmplitude_ = kBobAmplitudeDefault;
    bobSpeed_ = kBobSpeedDefault;
    jiggleAmount_ = kJiggleAmountDefault;
    jiggleSpeed_ = kJiggleSpeedDefault;
    wobbleDegrees_ = kWobbleDegreesDefault;
}

void TitleScene::CreateTitleLetters()
{
    titleLetters_.clear();
    titleLetters_.reserve(kTitleCharCount);

    for (int i = 0; i < kTitleCharCount; ++i)
    {
        TitleLetter letter;
        letter.baseWidth = kTitleChars[i].width;

        // 文字ごとに位相と速度をバラして「気まま」に見せる（毎回同じ結果になる）
        letter.phase = Hash01(static_cast<uint32_t>(i) * 2u + 1u) * kPi * 2.0f;
        letter.speedScale = 0.8f + Hash01(static_cast<uint32_t>(i) * 2u + 7u) * 0.5f;
        letter.popDelay = kPopStagger * static_cast<float>(i);

        // アンカーは下端中央。こうすると縦の潰れが「接地したまま」になってグミっぽい
        letter.sprite = MakeSprite(kTitleChars[i].texture,
                                   {letter.baseWidth, charHeight_}, {0.5f, 1.0f});

        titleLetters_.push_back(std::move(letter));
    }

    LayoutTitleLetters();
}

void TitleScene::CreateButtons()
{
    for (int i = 0; i < static_cast<int>(MenuItem::Count); ++i)
    {
        Button& button = buttons_[i];

        button.size = kButtonSize;
        button.center = {kButtonCenterX, kButtonFirstY + kButtonStepY * static_cast<float>(i)};

        // タイトルの文字が出そろったあと、上から順にぽぽぽっと出す
        button.popDelay = kPopStagger * static_cast<float>(kTitleCharCount) +
                          kButtonPopBeginOffset + kButtonPopStagger * static_cast<float>(i);

        button.hoverRate = 0.0f;
        button.scale = 1.0f;
        button.lightRate = 0.0f;
        button.isHovered = false;

        button.base = MakeSprite(kButtonAssets[i].base, kButtonSize, {0.5f, 0.5f});
        button.light = MakeSprite(kButtonAssets[i].light, kButtonSize, {0.5f, 0.5f});
    }
}

void TitleScene::LayoutTitleLetters()
{
    if (titleLetters_.empty())
    {
        return;
    }

    // 全体幅を出してから中央揃えする
    float totalWidth = 0.0f;
    for (const TitleLetter& letter : titleLetters_)
    {
        totalWidth += letter.baseWidth * logoScale_;
    }
    totalWidth += charSpacing_ * logoScale_ * static_cast<float>(titleLetters_.size() - 1);

    float cursor = -totalWidth * 0.5f;
    for (TitleLetter& letter : titleLetters_)
    {
        const float width = letter.baseWidth * logoScale_;
        letter.offsetX = cursor + width * 0.5f;
        cursor += width + charSpacing_ * logoScale_;
    }
}

std::unique_ptr<Sprite> TitleScene::MakeSprite(const char* texturePath, const Vector2& size,
                                               const Vector2& anchorPoint)
{
    std::unique_ptr<Sprite> sprite = Sprite::Create(texturePath, {0.0f, 0.0f});
    if (!sprite)
    {
        return nullptr;
    }

    sprite->SetAnchorPoint(anchorPoint);
    sprite->SetSize(size);
    sprite->SetColor({1.0f, 1.0f, 1.0f, 0.0f}); // フェードインするので最初は透明
    return sprite;
}

// ===================================================================
// 更新
// ===================================================================

void TitleScene::Update()
{
    const float deltaTime = kDeltaTime;
    sceneTime_ += deltaTime;

    if (input_)
    {
        input_->Update();
    }
    if (mouse_)
    {
        mouse_->Update();
    }

    UpdateFadeIn(deltaTime);
    UpdateTitleLetters(deltaTime);
    UpdateButtons(deltaTime);

    // Space キーでもゲーム開始（従来のショートカットを残しておく）
    if (input_ && input_->TriggerKey(DIK_SPACE))
    {
        DecideMenu(MenuItem::Start);
    }

#ifdef USE_IMGUI
    DrawDebugUI();
#endif
}

void TitleScene::UpdateFadeIn(float deltaTime)
{
    if (fadeAlpha_ < 1.0f)
    {
        fadeAlpha_ = std::clamp(fadeAlpha_ + deltaTime / kFadeInSeconds, 0.0f, 1.0f);
    }
}

void TitleScene::UpdateTitleLetters(float /*deltaTime*/)
{
    // 調整パラメータが変わっても追従できるよう、毎フレーム並べ直す（数文字なので安い）
    LayoutTitleLetters();

    const float wobbleRadian = ToRadian(wobbleDegrees_);
    const float baselineY = logoCenter_.y + charHeight_ * logoScale_ * 0.5f;

    for (TitleLetter& letter : titleLetters_)
    {
        // --- 登場（下からぷにっと出る） ---
        const float popRaw = std::clamp((sceneTime_ - letter.popDelay) / kPopSeconds, 0.0f, 1.0f);
        const float popScale = (popRaw <= 0.0f) ? 0.0f : EaseOutBack(popRaw);

        // --- グミ変形（横に伸びたら縦は縮む） ---
        const float jiggleTime = sceneTime_ * jiggleSpeed_ * letter.speedScale + letter.phase;
        const float jiggle = std::sin(jiggleTime);
        const float scaleX = 1.0f + jiggleAmount_ * jiggle;
        const float scaleY = 1.0f - jiggleAmount_ * jiggle * 0.85f;

        // --- 上下のふわふわ ---
        const float bobY = std::sin(sceneTime_ * bobSpeed_ + letter.phase) * bobAmplitude_;

        // --- 傾きのゆれ（変形とは別の周期にしてバラつかせる） ---
        const float rotation = std::sin(jiggleTime * 0.63f + 1.1f) * wobbleRadian;

        const Vector2 position = {logoCenter_.x + letter.offsetX, baselineY + bobY};
        const Vector2 size = {letter.baseWidth * logoScale_ * scaleX * popScale,
                              charHeight_ * logoScale_ * scaleY * popScale};

        ApplySprite(letter.sprite.get(), position, size, fadeAlpha_ * popRaw, rotation);
    }
}

void TitleScene::UpdateButtons(float deltaTime)
{
    const Vector2 mousePos = GetMousePositionOnUI();

    // フェードイン中は誤爆防止で入力を受け付けない
    const bool acceptInput = (fadeAlpha_ >= 1.0f);
    const bool isClicked = acceptInput && mouse_ && mouse_->TriggerButton(0);

    for (int i = 0; i < static_cast<int>(MenuItem::Count); ++i)
    {
        Button& button = buttons_[i];

        // --- 登場（タイトルの文字と同じく、ぷにっと出る） ---
        const float popRaw =
            std::clamp((sceneTime_ - button.popDelay) / kButtonPopSeconds, 0.0f, 1.0f);
        const float popScale = (popRaw <= 0.0f) ? 0.0f : EaseOutBack(popRaw);
        const bool isPopFinished = (popRaw >= 1.0f);

        // 出きるまでは押せない・光らない
        button.isHovered =
            acceptInput && isPopFinished && IsInside(mousePos, button.center, button.size);

        // ホバー進行度。EaseOutBack を通すので、乗った瞬間に軽くオーバーシュートする
        const float target = button.isHovered ? 1.0f : 0.0f;
        button.hoverRate = Approach(button.hoverRate, target, kHoverLerpRate, deltaTime);
        const float hoverEase = EaseOutBack(button.hoverRate);
        button.scale = 1.0f + (kHoverScale - 1.0f) * hoverEase;

        // ライトの点滅：ホバー中は sin で 0..1 を往復、外れたらなめらかに消灯
        const float blink = (std::sin(sceneTime_ * kBlinkSpeed) * 0.5f) + 0.5f;
        button.lightRate = button.isHovered
                               ? blink
                               : Approach(button.lightRate, 0.0f, kHoverLerpRate, deltaTime);

        // ホバー中だけ、ぷにぷに変形と傾きゆれを足す
        const float pulse = std::sin(sceneTime_ * kBlinkSpeed) * kButtonPulseAmount * button.hoverRate;
        const float rotation = std::sin(sceneTime_ * kButtonWobbleSpeed + static_cast<float>(i)) *
                               ToRadian(kButtonWobbleDegrees) * button.hoverRate;

        const Vector2 drawSize = {button.size.x * button.scale * (1.0f + pulse) * popScale,
                                  button.size.y * button.scale * (1.0f - pulse) * popScale};

        // 下に通常画像、その上に点灯画像を重ねて不透明度で入れ替える。
        // 下の画像も一緒に薄くする本来のクロスフェードにしたい場合は、
        // 下の行の fadeAlpha_ を fadeAlpha_ * (1.0f - button.lightRate) に変える
        ApplySprite(button.base.get(), button.center, drawSize, fadeAlpha_ * popRaw, rotation);
        ApplySprite(button.light.get(), button.center, drawSize,
                    fadeAlpha_ * popRaw * button.lightRate, rotation);

        if (button.isHovered && isClicked)
        {
            DecideMenu(static_cast<MenuItem>(i));
        }
    }
}

void TitleScene::DecideMenu(MenuItem item)
{
    switch (item)
    {
    case MenuItem::Start:
        SceneManager::GetInstance()->ChangeScene("GAMEPLAY");
        break;

    case MenuItem::Manual:
        // TODO: マニュアル画面ができたらここで遷移させる
        isManualRequested_ = true;
        break;

    case MenuItem::End:
        // TODO: 終了処理（PostQuitMessage など）をここに入れる
        isExitRequested_ = true;
        break;

    default:
        break;
    }
}

// ===================================================================
// 描画
// ===================================================================

void TitleScene::Draw(SceneRenderRequests& /*renderRequests*/)
{
    if (!dxCommon_ || !dxCommon_->GetCommandList())
    {
        return;
    }

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList().Get();

    // 背景スカイボックスの描画
    SceneManager::GetInstance()->DrawSkybox(commandList);

    // UI の描画。Sprite::Draw() が内部で RootSignature / PSO を張り直すので、
    // ここで 2D を描いても後続の 3D 描画には影響しない
    for (const TitleLetter& letter : titleLetters_)
    {
        if (letter.sprite)
        {
            letter.sprite->Draw(commandList);
        }
    }

    for (const Button& button : buttons_)
    {
        if (button.base)
        {
            button.base->Draw(commandList);
        }
        // 完全に透明なときは描かない
        if (button.light && button.lightRate > 0.0f)
        {
            button.light->Draw(commandList);
        }
    }
}

// ===================================================================
// デバッグUI（実行中に見た目を詰めるためのもの）
// ===================================================================

#ifdef USE_IMGUI
void TitleScene::DrawDebugUI()
{
    ImGui::Begin("Title Scene");

    if (ImGui::CollapsingHeader("Title Logo", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Letters : %d", static_cast<int>(titleLetters_.size()));
        ImGui::DragFloat2("Center", &logoCenter_.x, 1.0f, 0.0f, 1280.0f, "%.0f");
        ImGui::SliderFloat("Scale", &logoScale_, 0.2f, 2.0f, "%.2f");
        ImGui::SliderFloat("Char Height", &charHeight_, 40.0f, 320.0f, "%.0f");
        ImGui::SliderFloat("Char Spacing", &charSpacing_, -40.0f, 80.0f, "%.0f");
        ImGui::Separator();
        ImGui::SliderFloat("Bob Amplitude", &bobAmplitude_, 0.0f, 40.0f, "%.1f");
        ImGui::SliderFloat("Bob Speed", &bobSpeed_, 0.0f, 8.0f, "%.2f");
        ImGui::SliderFloat("Jiggle Amount", &jiggleAmount_, 0.0f, 0.35f, "%.3f");
        ImGui::SliderFloat("Jiggle Speed", &jiggleSpeed_, 0.0f, 10.0f, "%.2f");
        ImGui::SliderFloat("Wobble Degrees", &wobbleDegrees_, 0.0f, 20.0f, "%.1f");

        if (ImGui::Button("Reset", ImVec2(120.0f, 0.0f)))
        {
            ResetTuningToDefault();
        }
        ImGui::SameLine();
        if (ImGui::Button("Replay Intro", ImVec2(120.0f, 0.0f)))
        {
            sceneTime_ = 0.0f;
            fadeAlpha_ = 0.0f;
        }
    }

    if (ImGui::CollapsingHeader("Menu"))
    {
        const Vector2 mousePos = GetMousePositionOnUI();
        ImGui::Text("Mouse  : %.0f, %.0f", mousePos.x, mousePos.y);
        ImGui::Text("FadeIn : %.2f", fadeAlpha_);

        for (int i = 0; i < static_cast<int>(MenuItem::Count); ++i)
        {
            const Button& button = buttons_[i];
            ImGui::Text("%-7s hover=%d scale=%.2f light=%.2f", kMenuLabels[i],
                        button.isHovered ? 1 : 0, button.scale, button.lightRate);
        }

        ImGui::Text("Manual requested : %d", isManualRequested_ ? 1 : 0);
        ImGui::Text("Exit requested   : %d", isExitRequested_ ? 1 : 0);
    }

    ImGui::End();
}
#endif

// ===================================================================
// ヘルパー
// ===================================================================

Vector2 TitleScene::GetMousePositionOnUI() const
{
    // 仮想解像度（1280x720）上のマウス座標を返す。
    // MouseInput::GetScaledPosition() は先に仮想解像度でクランプしてから
    // スケールする実装なので、ウィンドウが 1280x720 より大きいとズレる。
    // ここでは実クライアント座標から自前で換算している。
    Vector2 result = {-1.0f, -1.0f};

    WindowAPI* windowAPI = dxCommon_ ? dxCommon_->GetWindowAPI() : nullptr;
    if (!windowAPI)
    {
        return result;
    }

    const HWND hwnd = windowAPI->GetHwnd();
    POINT point{};
    RECT clientRect{};
    if (!GetCursorPos(&point) || !ScreenToClient(hwnd, &point) ||
        !GetClientRect(hwnd, &clientRect))
    {
        return result;
    }

    const float clientWidth = static_cast<float>(clientRect.right - clientRect.left);
    const float clientHeight = static_cast<float>(clientRect.bottom - clientRect.top);
    if (clientWidth <= 0.0f || clientHeight <= 0.0f)
    {
        return result;
    }

    result.x = static_cast<float>(point.x) *
               (static_cast<float>(WindowAPI::GetClientWidth()) / clientWidth);
    result.y = static_cast<float>(point.y) *
               (static_cast<float>(WindowAPI::GetClientHeight()) / clientHeight);
    return result;
}

bool TitleScene::IsInside(const Vector2& point, const Vector2& center, const Vector2& size)
{
    const float halfWidth = size.x * 0.5f;
    const float halfHeight = size.y * 0.5f;

    return (point.x >= center.x - halfWidth) && (point.x <= center.x + halfWidth) &&
           (point.y >= center.y - halfHeight) && (point.y <= center.y + halfHeight);
}

void TitleScene::ApplySprite(Sprite* sprite, const Vector2& center, const Vector2& size,
                             float alpha, float rotation)
{
    if (!sprite)
    {
        return;
    }

    sprite->SetPosition(center);
    sprite->SetSize(size);
    sprite->SetRotation(rotation);

    Vector4 color = sprite->GetColor();
    color.w = std::clamp(alpha, 0.0f, 1.0f);
    sprite->SetColor(color);

    // 頂点・行列の書き込みはメインスレッド側（この Update）で済ませておく
    sprite->Update();
}
