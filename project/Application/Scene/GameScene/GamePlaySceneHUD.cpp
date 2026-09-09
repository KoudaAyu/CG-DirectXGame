#define NOMINMAX
#include "Application/Scene/GameScene/GamePlaySceneHUD.h"

#include "Camera.h"
#include "Application/GameObject/SlimeManager.h"
#include "Application/Enemy/EnemyManager.h"

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace
{
    constexpr float kScreenWidth = 1280.0f;  // WindowAPI::kClientWidth
    constexpr float kScreenHeight = 720.0f;  // WindowAPI::kClientHeight

    // ラベル画像。SCORE / TIME / COIN は ClearScene と共用
    constexpr const char* kLabelScoreTexture = "Resources/UI/Clear/label_score.png";
    constexpr const char* kLabelTimeTexture = "Resources/UI/Clear/label_time.png";
    constexpr const char* kLabelCoinTexture = "Resources/UI/Clear/label_coin.png";

    // ゲームシーンだけで使うもの（未作成でも白い四角で動く）
    constexpr const char* kLabelLifeTexture = "Resources/UI/Game/label_life.png";
    constexpr const char* kLifeSlimeTexture = "Resources/UI/Game/life_slime.png";

    constexpr int kScoreDigits = 6;
    constexpr int kTimeCells = 5;   // MM:SS
    constexpr int kCoinDigits = 3;

    constexpr int kHeadNumberPoolSize = 48; //!< 頭の上に同時に出せる数字の数
    constexpr int kHeadNumberCells = 2;     //!< 強さは2桁まで
    constexpr int kScorePopupPoolSize = 8;
    constexpr int kScorePopupCells = 6;     //!< "+" + 5桁

    /// @brief 行き過ぎて戻るイージング（にゅっと出る感じ）
    float EaseOutBack(float x)
    {
        constexpr float c1 = 1.70158f;
        constexpr float c3 = c1 + 1.0f;
        const float t = x - 1.0f;
        return 1.0f + c3 * t * t * t + c1 * t * t;
    }

    /// @brief 一度沈み込んでから消える（にゅっと引っ込む感じ）
    float EaseInBack(float x)
    {
        constexpr float c1 = 1.70158f;
        constexpr float c3 = c1 + 1.0f;
        return c3 * x * x * x - c1 * x * x;
    }

    float EaseOutCubic(float x)
    {
        const float t = 1.0f - x;
        return 1.0f - t * t * t;
    }
}

GamePlaySceneHud::~GamePlaySceneHud()
{
    Finalize();
}

GamePlaySceneHud::LabelSprite GamePlaySceneHud::MakeLabel(const char* texturePath, const Vector2& size,
                                                          const UiTextShadowStyle& shadowStyle)
{
    LabelSprite label;
    label.base = MakeSprite(texturePath, size);
    label.shadow = UiShadow::Create(texturePath, size, { 0.5f, 0.5f });
    // 色は**ここで1回だけ**決める。毎フレーム振り直すとチカチカする
    label.shadowColor = UiShadow::MakeColor(shadowStyle);
    return label;
}

void GamePlaySceneHud::ApplyLabel(LabelSprite& label, const Vector2& center, const Vector2& size,
                                  float alpha)
{
    ApplySprite(label.base.get(), center, size, alpha);
    UiShadow::Sync(label.shadow.get(), label.base.get(), labelShadow_, label.shadowColor);
}

std::unique_ptr<Sprite> GamePlaySceneHud::MakeSprite(const char* texturePath, const Vector2& size)
{
    // 画像が無くても engine が 4x4 の白ダミーに差し替えるので落ちない
    std::unique_ptr<Sprite> sprite = Sprite::Create(texturePath, { 0.0f, 0.0f });
    if (!sprite) return nullptr;

    sprite->SetAnchorPoint({ 0.5f, 0.5f });
    sprite->SetSize(size);
    sprite->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
    return sprite;
}

void GamePlaySceneHud::ApplySprite(Sprite* sprite, const Vector2& center, const Vector2& size,
                                   float alpha)
{
    if (!sprite) return;

    sprite->SetPosition(center);
    sprite->SetSize(size);

    Vector4 color = sprite->GetColor();
    color.w = std::clamp(alpha, 0.0f, 1.0f);
    sprite->SetColor(color);

    // 頂点・行列の書き込みはメインスレッド側（この Update）で済ませておく
    sprite->Update();
}

void GamePlaySceneHud::Initialize()
{
    Finalize();

    // --- ラベル ---
    labelScore_ = MakeLabel(kLabelScoreTexture, labelSize_, labelShadow_);
    labelTime_ = MakeLabel(kLabelTimeTexture, labelSize_, labelShadow_);
    labelCoin_ = MakeLabel(kLabelCoinTexture, labelSize_, labelShadow_);
    labelLife_ = MakeLabel(kLabelLifeTexture, labelLifeSize_, labelShadow_);

    // --- 数値 ---
    NumberDisplayStyle style;
    style.digitSize = digitSize_;
    style.spacing = digitSpacing_;
    style.shadow = numberShadow_;

    scoreNumber_.Initialize(kScoreDigits, style);
    scoreNumber_.SetMode(NumberDisplay::Mode::Integer);
    scoreNumber_.SetZeroPadding(true);
    scoreNumber_.SetRollSpeed(scoreRollCatchUp_, scoreRollMinStep_);

    timeNumber_.Initialize(kTimeCells, style);
    timeNumber_.SetMode(NumberDisplay::Mode::TimeMMSS);
    // 時間はパラパラさせない。経過秒をそのまま出す
    timeNumber_.SetRollSpeed(1000.0f, 1000.0f);

    coinNumber_.Initialize(kCoinDigits, style);
    coinNumber_.SetMode(NumberDisplay::Mode::Integer);
    coinNumber_.SetZeroPadding(true);
    coinNumber_.SetRollSpeed(coinRollCatchUp_, coinRollMinStep_);

    // --- 残機アイコン ---
    lifeIcons_.clear();
    lifeIcons_.resize(static_cast<size_t>((std::max)(1, lifeMaxIcons_)));
    for (size_t i = 0; i < lifeIcons_.size(); ++i)
    {
        LifeIcon& icon = lifeIcons_[i];
        icon.sprite = MakeSprite(kLifeSlimeTexture, lifeIconSize_);
        icon.anim = 0.0f;
        icon.isAlive = false;
        // 位相を個体ごとにずらす。そろって上下すると機械っぽくなる
        icon.phase = static_cast<float>(i) * 0.7f;
    }
    shownLifeCount_ = 0;

    // --- 頭の上の強さ（プール）---
    NumberDisplayStyle headStyle;
    headStyle.digitSize = headDigitSize_;
    headStyle.spacing = headDigitSpacing_;
    headStyle.punchAmount = 0.45f; // 強さが変わったときにしっかり弾ませる
    headStyle.shadow = numberShadow_;
    // 頭の数字は小さいので、ずらし量も控えめにする
    headStyle.shadow.offset = { numberShadow_.offset.x * 0.6f, numberShadow_.offset.y * 0.6f };

    headNumbers_.clear();
    headNumbers_.resize(kHeadNumberPoolSize);
    for (HeadNumber& head : headNumbers_)
    {
        head.number = std::make_unique<NumberDisplay>();
        head.number->Initialize(kHeadNumberCells, headStyle);
        head.number->SetMode(NumberDisplay::Mode::Integer);
        head.number->SetZeroPadding(false); // 1桁なら1桁だけ
        head.number->SetCenterAlign(true);
        head.number->SetRollSpeed(1000.0f, 1000.0f); // 頭の数字は即座に切り替える
        head.used = false;
    }
    headCursor_ = 0;

    // --- スコア加算のポップアップ（プール）---
    NumberDisplayStyle popupStyle;
    popupStyle.digitSize = popupDigitSize_;
    popupStyle.spacing = 1.0f;
    popupStyle.shadow = numberShadow_;
    popupStyle.shadow.offset = { numberShadow_.offset.x * 0.7f, numberShadow_.offset.y * 0.7f };

    scorePopups_.clear();
    scorePopups_.resize(kScorePopupPoolSize);
    for (ScorePopup& popup : scorePopups_)
    {
        popup.number = std::make_unique<NumberDisplay>();
        popup.number->Initialize(kScorePopupCells, popupStyle);
        popup.number->SetMode(NumberDisplay::Mode::SignedPopup);
        popup.number->SetZeroPadding(false);
        popup.number->SetCenterAlign(true);
        popup.number->SetRollSpeed(1000.0f, 1000.0f);
        popup.isActive = false;
    }

    lifeLostEvent_ = false;
    counterTickEvent_ = false;
}

void GamePlaySceneHud::Finalize()
{
    auto finalizeSprite = [](std::unique_ptr<Sprite>& sprite) {
        if (sprite)
        {
            sprite->Finalize();
            sprite.reset();
        }
    };

    auto finalizeLabel = [&finalizeSprite](LabelSprite& label) {
        finalizeSprite(label.base);
        finalizeSprite(label.shadow);
    };

    finalizeLabel(labelScore_);
    finalizeLabel(labelTime_);
    finalizeLabel(labelCoin_);
    finalizeLabel(labelLife_);

    scoreNumber_.Finalize();
    timeNumber_.Finalize();
    coinNumber_.Finalize();

    for (LifeIcon& icon : lifeIcons_) finalizeSprite(icon.sprite);
    lifeIcons_.clear();

    headNumbers_.clear();
    scorePopups_.clear();
}

bool GamePlaySceneHud::TakeLifeLostEvent()
{
    const bool fired = lifeLostEvent_;
    lifeLostEvent_ = false;
    return fired;
}

bool GamePlaySceneHud::TakeCounterTickEvent()
{
    const bool fired = counterTickEvent_;
    counterTickEvent_ = false;
    return fired;
}

// ===================================================================
// ワールド座標 -> 画面座標
// ===================================================================

bool GamePlaySceneHud::WorldToScreen(const Camera& camera, const Vector3& world, Vector2& outScreen)
{
    const Matrix4x4& vp = camera.GetViewProjectionMatrix();

    // 行ベクトル規約（v * M）。engine 全体がこの並び
    const float x = world.x * vp.m[0][0] + world.y * vp.m[1][0] + world.z * vp.m[2][0] + vp.m[3][0];
    const float y = world.x * vp.m[0][1] + world.y * vp.m[1][1] + world.z * vp.m[2][1] + vp.m[3][1];
    const float w = world.x * vp.m[0][3] + world.y * vp.m[1][3] + world.z * vp.m[2][3] + vp.m[3][3];

    if (w <= 0.0001f) return false; // カメラの後ろ

    const float ndcX = x / w;
    const float ndcY = y / w;

    outScreen = { (ndcX * 0.5f + 0.5f) * kScreenWidth,
                  (0.5f - ndcY * 0.5f) * kScreenHeight };
    return true;
}

// ===================================================================
// 更新
// ===================================================================

void GamePlaySceneHud::Update(float deltaTime, const FrameInput& input)
{
    if (!showHud_)
    {
        // 消しているあいだは全部透明にしておく（描画側でも弾いている）
        return;
    }

    // --- 1段目のラベル ---
    ApplyLabel(labelScore_, labelScorePos_, labelSize_, 1.0f);
    ApplyLabel(labelTime_, labelTimePos_, labelSize_, 1.0f);
    ApplyLabel(labelCoin_, labelCoinPos_, labelSize_, 1.0f);
    ApplyLabel(labelLife_, labelLifePos_, labelLifeSize_, 1.0f);

    // --- 数値 ---
    scoreNumber_.SetTarget(input.score);
    coinNumber_.SetTarget(input.coin);
    timeNumber_.SetTarget(static_cast<int>(input.elapsedSeconds));
    UpdateCounters(deltaTime);

    // --- 残機 ---
    UpdateLife(deltaTime, input.life);

    // --- 頭の上の強さ ---
    UpdateHeadNumbers(input);

    // --- スコア加算のポップアップ ---
    UpdateScorePopups(deltaTime, input);
}

void GamePlaySceneHud::UpdateCounters(float deltaTime)
{
    scoreNumber_.GetStyle().digitSize = digitSize_;
    scoreNumber_.GetStyle().spacing = digitSpacing_;
    timeNumber_.GetStyle().digitSize = digitSize_;
    timeNumber_.GetStyle().spacing = digitSpacing_;
    coinNumber_.GetStyle().digitSize = digitSize_;
    coinNumber_.GetStyle().spacing = digitSpacing_;

    // 重ね文字の設定は毎フレーム流し込む（ImGui でその場で見比べられるように）。
    // **桁ごとの色は cells_ 側に持っているので、ここを書き換えても色は振り直されない**
    scoreNumber_.GetStyle().shadow = numberShadow_;
    timeNumber_.GetStyle().shadow = numberShadow_;
    coinNumber_.GetStyle().shadow = numberShadow_;

    scoreNumber_.SetRollSpeed(scoreRollCatchUp_, scoreRollMinStep_);
    coinNumber_.SetRollSpeed(coinRollCatchUp_, coinRollMinStep_);

    // 桁の絵が変わったフレームを拾ってカウンタ音のトリガにする。
    // 時間表示は毎秒必ず変わるので、ここには混ぜない
    const bool scoreTicked = scoreNumber_.Update(deltaTime, scoreValuePos_, 1.0f);
    timeNumber_.Update(deltaTime, timeValuePos_, 1.0f);
    const bool coinTicked = coinNumber_.Update(deltaTime, coinValuePos_, 1.0f);

    if (scoreTicked || coinTicked)
    {
        counterTickEvent_ = true;
    }
}

void GamePlaySceneHud::UpdateLife(float deltaTime, int life)
{
    if (lifeIcons_.empty()) return;

    const int capacity = static_cast<int>(lifeIcons_.size());
    const int clamped = std::clamp(life, 0, capacity);

    if (clamped < shownLifeCount_)
    {
        lifeLostEvent_ = true;
    }
    shownLifeCount_ = clamped;

    for (int i = 0; i < capacity; ++i)
    {
        LifeIcon& icon = lifeIcons_[static_cast<size_t>(i)];
        icon.isAlive = (i < clamped);

        // 増えたら 0 -> 1、減ったら 1 -> 0 へ。時間で進める
        const float speed = icon.isAlive ? (1.0f / (std::max)(0.01f, lifePopSeconds_))
                                         : (1.0f / (std::max)(0.01f, lifeHideSeconds_));
        icon.anim += (icon.isAlive ? speed : -speed) * deltaTime;
        icon.anim = std::clamp(icon.anim, 0.0f, 1.0f);

        if (icon.anim <= 0.0f)
        {
            ApplySprite(icon.sprite.get(), lifeOrigin_, { 0.0f, 0.0f }, 0.0f);
            continue;
        }

        icon.phase += deltaTime * lifeBobSpeed_;

        // 出るときは行き過ぎて戻る、消えるときは沈み込んでから消える
        const float pop = icon.isAlive ? EaseOutBack(icon.anim) : (1.0f - EaseInBack(1.0f - icon.anim));

        const float x = lifeOrigin_.x + lifeSpacing_ * static_cast<float>(i);
        const float y = lifeOrigin_.y + std::sin(icon.phase) * lifeBobAmplitude_;

        // スライムらしく、膨らむときは横に、縮むときは縦につぶす
        const float squash = 1.0f + (1.0f - icon.anim) * 0.35f;
        const Vector2 size = { lifeIconSize_.x * pop * squash,
                               lifeIconSize_.y * pop / (std::max)(0.2f, squash) };

        ApplySprite(icon.sprite.get(), { x, y }, size, icon.anim);
    }
}

void GamePlaySceneHud::PlaceHeadNumber(const Camera& camera, const Vector3& worldPosition,
                                       float headOffsetY, int value, const Vector4& tint)
{
    if (headCursor_ >= static_cast<int>(headNumbers_.size())) return;

    Vector2 screen{};
    const Vector3 headWorld = { worldPosition.x, worldPosition.y + headOffsetY, worldPosition.z };
    if (!WorldToScreen(camera, headWorld, screen)) return;

    // 画面の外に出たぶんは描かない（少しはみ出すぶんは許す）
    if (screen.x < -80.0f || screen.x > kScreenWidth + 80.0f) return;
    if (screen.y < -80.0f || screen.y > kScreenHeight + 80.0f) return;

    HeadNumber& slot = headNumbers_[static_cast<size_t>(headCursor_++)];
    if (!slot.number) return;

    slot.used = true;
    slot.number->GetStyle().digitSize = headDigitSize_;
    slot.number->GetStyle().spacing = headDigitSpacing_;
    slot.number->GetStyle().shadow = numberShadow_;
    slot.number->GetStyle().shadow.offset = { numberShadow_.offset.x * 0.6f,
                                              numberShadow_.offset.y * 0.6f };
    slot.number->SetTarget(value);
    slot.number->Update(1.0f / 60.0f, screen, headAlpha_ * tint.w);
}

void GamePlaySceneHud::ReleaseUnusedHeadNumbers()
{
    for (int i = headCursor_; i < static_cast<int>(headNumbers_.size()); ++i)
    {
        HeadNumber& head = headNumbers_[static_cast<size_t>(i)];
        if (!head.used) continue;

        head.used = false;
        if (head.number)
        {
            // 画面外へ逃がして透明にする
            head.number->Update(1.0f / 60.0f, { -1000.0f, -1000.0f }, 0.0f);
        }
    }
}

void GamePlaySceneHud::UpdateHeadNumbers(const FrameInput& input)
{
    headCursor_ = 0;

    if (!showHeadNumbers_ || !input.showHeadNumbers || !input.camera)
    {
        ReleaseUnusedHeadNumbers();
        return;
    }

    const Camera& camera = *input.camera;

    // --- 代表スライム（一番大きい個体）---
    if (input.player)
    {
        const float radius = input.player->GetCurrentScale() * 0.78f;
        PlaceHeadNumber(camera, input.player->GetPosition(), radius * headOffsetPlayer_,
                        input.player->GetSize(), { 1.0f, 1.0f, 1.0f, 1.0f });
    }

    // --- 代表以外のスライム ---
    if (input.slimeManager)
    {
        const Slime* leader = input.player;

        for (const auto& slimePtr : input.slimeManager->GetSlimes())
        {
            const Slime* slime = slimePtr.get();
            if (!slime || !slime->IsActive()) continue;
            if (slime == leader) continue; // 代表はすぐ上で出しているので二重に出さない
            if (slime->GetState() == SlimeState::Merging) continue;

            PlaceHeadNumber(camera, slime->GetPosition(),
                            slime->GetRadius() * headOffsetMinion_,
                            slime->GetSize(), { 1.0f, 1.0f, 1.0f, 1.0f });
        }
    }

    // --- 敵 ---
    if (input.enemyManager)
    {
        for (const auto& enemyPtr : input.enemyManager->GetEnemies())
        {
            MobEnemy* enemy = enemyPtr.get();
            if (!enemy || enemy->IsDead()) continue;

            // ヒットボックスの上端あたりから、さらに少し上に出す
            const Vector3 center = enemy->GetHitCenter();
            const Vector3 full = enemy->GetHitBoxFullSize();
            PlaceHeadNumber(camera, center, full.y * 0.5f * headOffsetEnemy_ + 0.3f,
                            enemy->GetStrength(), { 1.0f, 1.0f, 1.0f, 1.0f });
        }
    }

    ReleaseUnusedHeadNumbers();
}

void GamePlaySceneHud::PushScorePopup(int amount, const Vector3& worldPosition)
{
    if (amount <= 0) return;

    for (ScorePopup& popup : scorePopups_)
    {
        if (popup.isActive || !popup.number) continue;

        popup.isActive = true;
        popup.age = 0.0f;
        popup.worldPosition = worldPosition;
        popup.screenPosition = { -1000.0f, -1000.0f };
        popup.number->SetTarget(amount);
        popup.number->SnapToTarget();
        return;
    }

    // 空きが無ければ一番古いものを潰す（同時に何発も倒したときだけ起きる）
    ScorePopup* oldest = nullptr;
    for (ScorePopup& popup : scorePopups_)
    {
        if (!popup.number) continue;
        if (!oldest || popup.age > oldest->age) oldest = &popup;
    }
    if (oldest)
    {
        oldest->isActive = true;
        oldest->age = 0.0f;
        oldest->worldPosition = worldPosition;
        oldest->screenPosition = { -1000.0f, -1000.0f };
        oldest->number->SetTarget(amount);
        oldest->number->SnapToTarget();
    }
}

void GamePlaySceneHud::UpdateScorePopups(float deltaTime, const FrameInput& input)
{
    for (ScorePopup& popup : scorePopups_)
    {
        if (!popup.number) continue;

        if (!popup.isActive)
        {
            popup.number->Update(deltaTime, { -1000.0f, -1000.0f }, 0.0f);
            continue;
        }

        popup.age += deltaTime;
        if (popup.age >= popupLife_)
        {
            popup.isActive = false;
            popup.number->Update(deltaTime, { -1000.0f, -1000.0f }, 0.0f);
            continue;
        }

        // 出た瞬間だけワールド座標から画面座標を取り、以降は画面上を上っていく。
        // カメラが動いても文字が飛び回らないようにするため
        if (popup.screenPosition.x < -900.0f && input.camera)
        {
            Vector2 screen{};
            if (WorldToScreen(*input.camera, popup.worldPosition, screen))
            {
                popup.screenPosition = screen;
            }
            else
            {
                popup.isActive = false;
                popup.number->Update(deltaTime, { -1000.0f, -1000.0f }, 0.0f);
                continue;
            }
        }

        const float t = popup.age / (std::max)(0.01f, popupLife_);

        // 上りは最初だけ速く、最後はゆっくり止まる
        popup.screenPosition.y -= popupRiseSpeed_ * (1.0f - EaseOutCubic(t)) * deltaTime * 2.0f;

        // 出た瞬間にぷにっと大きくなってから落ち着く
        const float popScale = (t < 0.22f) ? EaseOutBack(t / 0.22f) : 1.0f;
        const float alpha = (t < 0.7f) ? 1.0f : (1.0f - (t - 0.7f) / 0.3f);

        popup.number->GetStyle().digitSize = popupDigitSize_;
        popup.number->Update(deltaTime, popup.screenPosition, alpha,
                             { popScale, popScale });
    }
}

// ===================================================================
// 描画
// ===================================================================

void GamePlaySceneHud::Draw(ID3D12GraphicsCommandList* commandList)
{
    if (!showHud_ || !commandList) return;

    // Sprite の PSO はデプス無効なので、後に描いたものが手前に来る
    auto drawLabel = [&](const LabelSprite& label) {
        if (labelShadow_.enabled && !labelShadow_.inFront && label.shadow) label.shadow->Draw(commandList);
        if (label.base) label.base->Draw(commandList);
        if (labelShadow_.enabled && labelShadow_.inFront && label.shadow) label.shadow->Draw(commandList);
    };

    drawLabel(labelScore_);
    drawLabel(labelTime_);
    drawLabel(labelCoin_);
    drawLabel(labelLife_);

    scoreNumber_.Draw(commandList);
    timeNumber_.Draw(commandList);
    coinNumber_.Draw(commandList);

    for (const LifeIcon& icon : lifeIcons_)
    {
        if (icon.anim > 0.0f && icon.sprite) icon.sprite->Draw(commandList);
    }

    for (const HeadNumber& head : headNumbers_)
    {
        if (head.used && head.number) head.number->Draw(commandList);
    }

    for (const ScorePopup& popup : scorePopups_)
    {
        if (popup.isActive && popup.number) popup.number->Draw(commandList);
    }
}

void GamePlaySceneHud::DrawImGui()
{
#ifdef USE_IMGUI
    if (!ImGui::CollapsingHeader("Game HUD")) return;

    ImGui::Checkbox("Show HUD", &showHud_);
    ImGui::SameLine();
    ImGui::Checkbox("Head Numbers", &showHeadNumbers_);

    ImGui::SeparatorText("Text Shadow (白文字が見づらい対策)");
    ImGui::TextWrapped("同じ文字を少しずらして濃紺で重ねる。In Front を切ると普通の影になる");
    ImGui::Checkbox("Label Shadow##on", &labelShadow_.enabled);
    ImGui::SameLine();
    ImGui::Checkbox("Label In Front", &labelShadow_.inFront);
    ImGui::DragFloat2("Label Offset", &labelShadow_.offset.x, 0.2f, -30.0f, 30.0f);
    ImGui::ColorEdit3("Label Color", &labelShadow_.color.x);
    ImGui::DragFloat("Label Alpha Scale", &labelShadow_.alphaScale, 0.01f, 0.0f, 1.0f);

    ImGui::Checkbox("Number Shadow##on", &numberShadow_.enabled);
    ImGui::SameLine();
    ImGui::Checkbox("Number In Front", &numberShadow_.inFront);
    ImGui::DragFloat2("Number Offset", &numberShadow_.offset.x, 0.2f, -30.0f, 30.0f);
    ImGui::ColorEdit3("Number Color", &numberShadow_.color.x);
    ImGui::DragFloat("Number Alpha Scale", &numberShadow_.alphaScale, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Color Jitter", &numberShadow_.colorJitter, 0.005f, 0.0f, 0.5f);
    ImGui::DragFloat("Blue Jitter", &numberShadow_.blueJitter, 0.005f, 0.0f, 0.5f);
    ImGui::TextDisabled("※ Jitter は Initialize() のときだけ効く（色の振り直しは Re-roll）");
    if (ImGui::Button("Re-roll shadow colors"))
    {
        // 色は生成時に1回だけ決めているので、振り直すには作り直すのが一番早い
        labelShadow_.colorJitter = numberShadow_.colorJitter;
        labelShadow_.blueJitter = numberShadow_.blueJitter;
        Initialize();
    }

    ImGui::SeparatorText("Row 1 : SCORE / TIME / COIN");
    ImGui::DragFloat2("Label Score", &labelScorePos_.x, 1.0f);
    ImGui::DragFloat2("Label Time", &labelTimePos_.x, 1.0f);
    ImGui::DragFloat2("Label Coin", &labelCoinPos_.x, 1.0f);
    ImGui::DragFloat2("Label Size", &labelSize_.x, 1.0f);
    ImGui::DragFloat2("Value Score", &scoreValuePos_.x, 1.0f);
    ImGui::DragFloat2("Value Time", &timeValuePos_.x, 1.0f);
    ImGui::DragFloat2("Value Coin", &coinValuePos_.x, 1.0f);
    ImGui::DragFloat2("Digit Size", &digitSize_.x, 0.5f);
    ImGui::DragFloat("Digit Spacing", &digitSpacing_, 0.2f, -20.0f, 40.0f);

    ImGui::SeparatorText("Row 2 : LIFE");
    ImGui::DragFloat2("Label Life", &labelLifePos_.x, 1.0f);
    ImGui::DragFloat2("Label Life Size", &labelLifeSize_.x, 1.0f);
    ImGui::DragFloat2("Life Origin", &lifeOrigin_.x, 1.0f);
    ImGui::DragFloat("Life Spacing", &lifeSpacing_, 0.5f, 4.0f, 120.0f);
    ImGui::DragFloat2("Life Icon Size", &lifeIconSize_.x, 0.5f);
    ImGui::DragFloat("Pop Seconds", &lifePopSeconds_, 0.01f, 0.02f, 1.5f);
    ImGui::DragFloat("Hide Seconds", &lifeHideSeconds_, 0.01f, 0.02f, 1.5f);
    ImGui::DragFloat("Bob Amplitude", &lifeBobAmplitude_, 0.1f, 0.0f, 20.0f);
    ImGui::DragFloat("Bob Speed", &lifeBobSpeed_, 0.05f, 0.0f, 12.0f);
    ImGui::Text("Shown life: %d / icons %d", shownLifeCount_, static_cast<int>(lifeIcons_.size()));

    ImGui::SeparatorText("Head Numbers");
    ImGui::DragFloat2("Head Digit Size", &headDigitSize_.x, 0.5f);
    ImGui::DragFloat("Head Spacing", &headDigitSpacing_, 0.2f, -20.0f, 30.0f);
    ImGui::DragFloat("Offset Player", &headOffsetPlayer_, 0.02f, 0.0f, 8.0f);
    ImGui::DragFloat("Offset Small Slime", &headOffsetMinion_, 0.02f, 0.0f, 8.0f);
    ImGui::DragFloat("Offset Enemy", &headOffsetEnemy_, 0.02f, 0.0f, 8.0f);
    ImGui::DragFloat("Head Alpha", &headAlpha_, 0.01f, 0.0f, 1.0f);
    ImGui::Text("Used this frame: %d / %d", headCursor_, static_cast<int>(headNumbers_.size()));

    ImGui::SeparatorText("Score Popup");
    ImGui::DragFloat("Rise Speed", &popupRiseSpeed_, 1.0f, 0.0f, 400.0f);
    ImGui::DragFloat("Popup Life", &popupLife_, 0.02f, 0.1f, 5.0f);
    ImGui::DragFloat2("Popup Digit Size", &popupDigitSize_.x, 0.5f);
    ImGui::DragFloat("Popup Offset Y", &popupOffsetY_, 0.02f, 0.0f, 8.0f);

    ImGui::SeparatorText("Counter Roll");
    ImGui::DragFloat("Score CatchUp", &scoreRollCatchUp_, 0.1f, 0.5f, 40.0f);
    ImGui::DragFloat("Score MinStep", &scoreRollMinStep_, 1.0f, 1.0f, 5000.0f);
    ImGui::DragFloat("Coin CatchUp", &coinRollCatchUp_, 0.1f, 0.5f, 40.0f);
    ImGui::DragFloat("Coin MinStep", &coinRollMinStep_, 0.5f, 1.0f, 500.0f);
#endif
}
