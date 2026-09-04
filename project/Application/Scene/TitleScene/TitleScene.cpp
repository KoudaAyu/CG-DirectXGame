#include "TitleScene.h"

#include "DirectXCom.h"
#include "KeyInput.h"
#include "SceneManager.h"
#include "WindowsAPI.h"

#include "Baziru3_Engine/Core/IO/Mouse/MouseInput.h"
#include "Baziru3_Engine/Framework/Collision/CollisionManager.h"
#include "Baziru3_Engine/Graphics/Graphics/SceneRenderRequests.h"

#include "Application/GameObject/SlimeFx.h"
#include "Application/Minion/MinionManager.h"
#include "Application/Player/PikminPlayer.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3dCom.h"
#include "Camera.h"
#include "RenderContext.h"

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

// ===================================================================
// タイトルスライム（3D）
//
// ゲーム側の PikminPlayer をそのまま置いている。
// Slime 用の RootSignature / PSO は engine の PipelineStateManager が
// 起動時に登録しているので、GAMEPLAY 以外のシーンからでもそのまま使える。
// ===================================================================
constexpr bool kShowSlimeDefault = true;

// カメラの既定値。仮想解像度 1280x720 で、スライムがロゴの真下あたり
// （画面座標でだいたい 400, 470）に来るように逆算してある
constexpr Vector3 kCameraPosDefault = {1.53f, 3.0f, -10.0f};
constexpr float kCameraPitchDefault = 0.171f; // 見下ろし角（ラジアン）

constexpr Vector3 kSlimeHomeDefault = {0.0f, 0.0f, 0.0f};
constexpr float kSlimeRoamRadiusDefault = 2.4f;  // 定位置から離れられる距離（UI に被らせない）
constexpr float kSlimeTiltGainDefault = 0.16f;   // 目標までの距離 → 傾き
constexpr float kSlimeMaxTiltDefault = 0.30f;    // 傾きの上限（ラジアン）
constexpr float kSlimeFollowRateDefault = 0.85f; // マウス追従の強さ 0..1

constexpr float kSlimeTiltAccel = 16.0f; // タイトル用のゆるい加速（ゲーム側は 38）
constexpr float kSlimeFriction = 2.0f;   // 減衰。小さいほど行き過ぎて揺り戻す
constexpr float kSlimeTiltLerpRate = 6.0f; // 傾きの追従速度（1/秒）

constexpr float kSlimeIdleDriftRadius = 0.8f; // 何もしていないときのうろつき幅
constexpr float kSlimeIdleDriftSpeed = 0.35f; // うろつきの速さ（rad/秒）

constexpr float kSlimeIntroImpulse = 0.5f;  // ロゴが出そろった瞬間の波紋
constexpr float kSlimeHoverImpulse = 0.22f; // ボタンに乗った瞬間の波紋

constexpr Vector4 kSlimeColorDefault = {0.2f, 0.85f, 1.0f, 0.9f};

// ===================================================================
// 自動デモ
// ===================================================================
constexpr bool kDemoEnabledDefault = true;
constexpr int kMinionSpawnCountDefault = 8;

constexpr float kDemoRoamSecondsMinDefault = 4.0f; // 分裂状態でうろつく時間
constexpr float kDemoRoamSecondsMaxDefault = 7.0f;
constexpr float kDemoRollSecondsMinDefault = 3.0f; // 合体状態で転がる時間
constexpr float kDemoRollSecondsMaxDefault = 5.0f;
constexpr float kDemoThrowIntervalMinDefault = 1.1f; // 投擲の間隔
constexpr float kDemoThrowIntervalMaxDefault = 2.0f;

constexpr float kThrowFlightTime = 0.9f; // 放物線の飛行時間（これを決め打ちして初速を逆算する）
constexpr float kThrowGravity = -24.0f;  // Minion / AimGuide と同じ重力
constexpr float kThrowRangeX = 5.0f;     // 着弾点のばらつき（画面左寄りに寄せる）
constexpr float kThrowRangeZ = 4.0f;
constexpr float kThrowBiasX = -1.6f; // 右のボタン列に被らないよう左へオフセット

// ===================================================================
// パーティクル演出
// ===================================================================
// 粒1個につき Object3d を1個持つ（＝板ポリの頂点バッファも1個ずつ）。
// 増やしすぎるとリソースが無駄になるので、実際に同時に出る数から少し余裕を見た値にしてある
constexpr uint32_t kFxCapacity = 128;

constexpr float kFxSparkleIntervalDefault = 0.06f;    // キラキラの発生間隔（秒）
constexpr float kFxTrailStepDefault = 0.30f;          // 移動軌跡を1個置く距離
constexpr float kFxBackgroundIntervalDefault = 0.35f; // 背景パーティクルの発生間隔

constexpr float kFxTrailSpeedThreshold = 1.0f; // これ以上の速さで動いていたら軌跡を残す

constexpr Vector4 kFxSparkleColorNormal = {0.65f, 0.95f, 1.0f, 0.95f};
constexpr Vector4 kFxSparkleColorMerged = {1.0f, 0.9f, 0.45f, 1.0f};
constexpr Vector4 kFxTrailColorNormal = {0.35f, 0.85f, 1.0f, 0.5f};
constexpr Vector4 kFxTrailColorMerged = {1.0f, 0.82f, 0.3f, 0.5f};
constexpr Vector4 kFxBulletColor = {0.55f, 0.95f, 1.0f, 0.75f};
constexpr Vector4 kFxMergeColor = {1.0f, 0.88f, 0.4f, 0.95f};
constexpr Vector4 kFxSplitColor = {1.0f, 1.0f, 1.0f, 1.0f};
constexpr Vector4 kFxStrandColor = {0.7f, 0.95f, 1.0f, 0.8f};
constexpr Vector4 kFxBackgroundColor = {0.85f, 0.95f, 1.0f, 0.35f};

constexpr float kFxBackgroundLifeTime = 7.0f;

// ロゴの最後の文字が出きる時刻。ここに波紋を合わせる
constexpr float kSlimeIntroPulseTime =
    kPopStagger * static_cast<float>(kTitleCharCount - 1) + kPopSeconds;

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

// unique_ptr が持つ型（PikminPlayer / MinionManager / SlimeFx）の完全な定義が要るので、
// コンストラクタとデストラクタはヘッダではなくここで定義する。
// 詳しい理由は TitleScene.h のコメントを参照
TitleScene::TitleScene() = default;
TitleScene::~TitleScene() = default;

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
    CreateSlime();

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

    // PikminPlayer / Minion のデストラクタが CollisionManager から自分を外してくれる
    fx_.reset();
    minions_.reset();
    slime_.reset();

    // 借りていた engine カメラを元の位置に戻す。
    // 戻さないとゲーム中のスカイボックスやパーティクルの見え方が変わってしまう
    if (slimeCamera_)
    {
        slimeCamera_->SetTranslate(savedCameraTranslate_);
        slimeCamera_->SetRotate(savedCameraRotate_);
        // ゲーム終了時部品解放の順がよくわからないけど、ここでUpdate呼ぶとエラー起きる可能性があるので
        // コメントアウトしておく
        //slimeCamera_->Update();
        slimeCamera_ = nullptr;
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

    ResetSlimeTuningToDefault();
}

void TitleScene::ResetSlimeTuningToDefault()
{
    showSlime_ = kShowSlimeDefault;
    cameraPos_ = kCameraPosDefault;
    cameraPitch_ = kCameraPitchDefault;
    slimeHome_ = kSlimeHomeDefault;
    slimeRoamRadius_ = kSlimeRoamRadiusDefault;
    slimeTiltGain_ = kSlimeTiltGainDefault;
    slimeMaxTilt_ = kSlimeMaxTiltDefault;
    slimeFollowRate_ = kSlimeFollowRateDefault;
    slimeOverrideColor_ = false;
    slimeColor_ = kSlimeColorDefault;

    isDemoEnabled_ = kDemoEnabledDefault;
    minionSpawnCount_ = kMinionSpawnCountDefault;
    demoRoamSecondsMin_ = kDemoRoamSecondsMinDefault;
    demoRoamSecondsMax_ = kDemoRoamSecondsMaxDefault;
    demoRollSecondsMin_ = kDemoRollSecondsMinDefault;
    demoRollSecondsMax_ = kDemoRollSecondsMaxDefault;
    demoThrowIntervalMin_ = kDemoThrowIntervalMinDefault;
    demoThrowIntervalMax_ = kDemoThrowIntervalMaxDefault;

    showFx_ = true;
    fxEnableSparkle_ = true;
    fxEnableTrail_ = true;
    fxEnableGroundMark_ = true;
    fxEnableBulletTrail_ = true;
    fxEnableBackground_ = true;
    fxSparkleInterval_ = kFxSparkleIntervalDefault;
    fxTrailStep_ = kFxTrailStepDefault;
    fxBackgroundInterval_ = kFxBackgroundIntervalDefault;

    if (slime_)
    {
        slime_->SetTiltAccel(kSlimeTiltAccel);
        slime_->SetFriction(kSlimeFriction);
    }
}

float TitleScene::RandomRange(float minValue, float maxValue)
{
    std::uniform_real_distribution<float> distribution(minValue, maxValue);
    return distribution(randomEngine_);
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

void TitleScene::CreateSlime()
{
    Object3dCom* object3dCom = GetObject3dCom();
    if (!dxCommon_ || !object3dCom)
    {
        return;
    }

    randomEngine_.seed(std::random_device{}());

    // engine のカメラを借りる。
    // これは Game が ParticleManager と SkyBox にも渡している同じカメラなので、
    // ここを動かすとスライム・背景・エフェクトの視点がまとめて揃う。
    // SceneManager::GetCamera() は GAMEPLAY を経由すると解放済みの playCamera_ を
    // 指すことがあるので、そちらではなく Object3dCom 経由で取る。
    slimeCamera_ = object3dCom->GetDefaultCamera();
    if (!slimeCamera_)
    {
        return;
    }

    savedCameraTranslate_ = slimeCamera_->GetTranslate();
    savedCameraRotate_ = slimeCamera_->GetRotate();

    slimeCamera_->SetTranslate(cameraPos_);
    slimeCamera_->SetRotate({cameraPitch_, 0.0f, 0.0f});
    slimeCamera_->Update();

    // PikminPlayer / Minion の Initialize が SphereCollider を登録するので、先に器を初期化しておく
    CollisionManager::GetInstance()->Initialize();

    slime_ = std::make_unique<PikminPlayer>();
    slime_->Initialize(object3dCom, slimeCamera_, slimeHome_);

    // タイトル用にゆったりした挙動へ振り直す（ゲーム側の既定値には触らない）
    slime_->SetTiltAccel(kSlimeTiltAccel);
    slime_->SetFriction(kSlimeFriction);

    // 見た目もタイトル向けに少し盛る（ここは Update() に上書きされない）
    SlimeParamsCPU& params = slime_->GetSlimeParams();
    params.wobbleStrength = 0.24f;
    params.wobbleFrequency = 4.4f;
    params.fresnelPower = 2.2f;
    params.envReflection = 0.55f;
    params.innerGlow = 0.55f;

    // ミニオン。合体のたびに全員吸えるよう、吸引半径はタイトル用に広く取る。
    // こうしておくと Merge -> Split のループで毎回プレイヤーの足元に集め直されるので、
    // 群れが画面外へ散らばっていかない
    minions_ = std::make_unique<MinionManager>();
    minions_->Initialize(object3dCom, slimeCamera_);
    minions_->SetMergePickupRadius(40.0f);
    minions_->SetSplitPopPower(6.0f);
    minions_->SetSplitUpPower(6.0f);
    minions_->SpawnMinion(slimeHome_, minionSpawnCount_, MinionType::Red);

    fx_ = std::make_unique<SlimeFx>();
    fx_->Initialize(object3dCom, slimeCamera_, kFxCapacity);
    fx_->SetCameraPitch(cameraPitch_);

    slimeTilt_ = {0.0f, 0.0f};
    isSlimeIntroPulseDone_ = false;
    wasAnyButtonHovered_ = false;

    demoState_ = DemoState::Roam;
    demoTimer_ = 0.0f;
    demoDuration_ = RandomRange(demoRoamSecondsMin_, demoRoamSecondsMax_);
    throwTimer_ = 1.2f;
    prevMergedCount_ = 0;
    strandDelayFrames_ = 0;
    slimeFlashTimer_ = 0.0f;

    fxSparkleAccum_ = 0.0f;
    fxBackgroundAccum_ = 0.0f;
    fxTrailDistance_ = 0.0f;
    fxPrevSlimePos_ = slime_->GetPosition();
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

    // ボタンのホバー状態を見てから動かすので、UpdateButtons の後ろに置くこと
    UpdateDemo(deltaTime);
    UpdateSlime(deltaTime);
    UpdateFx(deltaTime);

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

void TitleScene::UpdateSlime(float deltaTime)
{
    if (!slime_ || !slimeCamera_)
    {
        return;
    }

    // カメラは ImGui でいじれるので毎フレーム反映する。
    // Update() を呼ばないと GPU 側の定数バッファが確保されず、
    // Slime の PS がカメラ位置を読めない（＝フレネルと環境反射が死ぬ）
    slimeCamera_->SetTranslate(cameraPos_);
    slimeCamera_->SetRotate({cameraPitch_, 0.0f, 0.0f});
    slimeCamera_->Update();

    // ビルボードの向きはカメラのピッチだけで決まる
    if (fx_)
    {
        fx_->SetCameraPitch(cameraPitch_);
    }

    // --- 目標地点を決める ---
    // ベースはゆっくりしたリサージュのうろつき
    Vector3 target = slimeHome_;
    target.x += std::sin(sceneTime_ * kSlimeIdleDriftSpeed) * kSlimeIdleDriftRadius;
    target.z += std::sin(sceneTime_ * kSlimeIdleDriftSpeed * 1.37f + 1.1f) *
                kSlimeIdleDriftRadius * 0.6f;

    // マウスが画面内にあれば、そっちへ寄っていく
    bool hasMouseTarget = false;
    const Vector3 mouseGround = GetMouseGroundPoint(hasMouseTarget);
    if (hasMouseTarget && slimeFollowRate_ > 0.0f)
    {
        target.x += (mouseGround.x - target.x) * slimeFollowRate_;
        target.z += (mouseGround.z - target.z) * slimeFollowRate_;
    }
    else if (minions_ && !slime_->IsMerged())
    {
        // マウスが画面外のときは、一番近い転がり中のミニオンへ寄っていく。
        // 見た目がピクミンっぽくなるのと、投擲は「手元 3.5m 以内のミニオン」しか
        // 掴めない実装なので、こうしておくとデモの投擲が枯れない
        const Vector3& position = slime_->GetPosition();
        const Minion* nearest = nullptr;
        float nearestDistanceSq = 1e9f;

        for (const auto& minion : minions_->GetMinions())
        {
            if (!minion || !minion->IsActive())
            {
                continue;
            }
            if (minion->GetState() != MinionState::Rolling)
            {
                continue;
            }
            const Vector3 diff = minion->GetPosition() - position;
            const float distanceSq = diff.x * diff.x + diff.z * diff.z;
            if (distanceSq < nearestDistanceSq)
            {
                nearestDistanceSq = distanceSq;
                nearest = minion.get();
            }
        }

        if (nearest)
        {
            const Vector3& minionPos = nearest->GetPosition();
            target.x += (minionPos.x - target.x) * 0.6f;
            target.z += (minionPos.z - target.z) * 0.6f;
        }
    }

    // 定位置から離れすぎないように制限する（ボタンの上まで行かせない）
    float offsetX = target.x - slimeHome_.x;
    float offsetZ = target.z - slimeHome_.z;
    const float distance = std::sqrt(offsetX * offsetX + offsetZ * offsetZ);
    if (distance > slimeRoamRadius_ && distance > 1e-4f)
    {
        const float shrink = slimeRoamRadius_ / distance;
        offsetX *= shrink;
        offsetZ *= shrink;
    }
    target = {slimeHome_.x + offsetX, slimeHome_.y, slimeHome_.z + offsetZ};

    // --- 目標地点へ向けてステージを傾ける（P 制御） ---
    // PikminPlayer::Update() の実装では
    //   stageTilt.y -> X 方向の加速度 / stageTilt.x -> Z 方向の加速度
    // 加速度と摩擦がバネダンパになるので、行き過ぎて揺り戻す動きが自然に出る
    const Vector3& position = slime_->GetPosition();
    const float targetTiltY =
        std::clamp((target.x - position.x) * slimeTiltGain_, -slimeMaxTilt_, slimeMaxTilt_);
    const float targetTiltX =
        std::clamp((target.z - position.z) * slimeTiltGain_, -slimeMaxTilt_, slimeMaxTilt_);

    slimeTilt_.x = Approach(slimeTilt_.x, targetTiltX, kSlimeTiltLerpRate, deltaTime);
    slimeTilt_.y = Approach(slimeTilt_.y, targetTiltY, kSlimeTiltLerpRate, deltaTime);

    // 入力系は全部 nullptr で渡す。
    // こうすると E キーの合体トグルも投擲も走らず、傾きだけで動く状態になる
    slime_->Update(deltaTime, nullptr, nullptr, nullptr, nullptr, slimeTilt_);

    // PikminPlayer::Update() は毎フレーム baseColor を塗り直すので、
    // 色を変えたいときは「後がけ」する必要がある
    SlimeParamsCPU& params = slime_->GetSlimeParams();
    if (slimeOverrideColor_)
    {
        params.baseColor = slimeColor_;
    }

    // 分裂した瞬間だけ白く飛ばす
    if (slimeFlashTimer_ > 0.0f)
    {
        slimeFlashTimer_ -= deltaTime;
        const float flash = std::clamp(slimeFlashTimer_ / 0.12f, 0.0f, 1.0f);
        params.baseColor.x += (1.0f - params.baseColor.x) * flash;
        params.baseColor.y += (1.0f - params.baseColor.y) * flash;
        params.baseColor.z += (1.0f - params.baseColor.z) * flash;
    }

    // ミニオンの更新。MinionManager は isMerged の変化を見て
    // 自分で TriggerMerge / TriggerSplit を呼ぶので、ここでは渡すだけでいい
    if (minions_)
    {
        minions_->Update(deltaTime, slime_->GetPosition(), slime_->IsMerged(),
                         slime_->GetCurrentScale(), slimeTilt_);
    }

    // --- ロゴが出そろった瞬間に波紋を1発 ---
    if (!isSlimeIntroPulseDone_ && sceneTime_ >= kSlimeIntroPulseTime)
    {
        params.impulseStrength = kSlimeIntroImpulse;
        isSlimeIntroPulseDone_ = true;
    }

    // --- ボタンに乗った瞬間にも小さく波紋 ---
    bool isAnyHovered = false;
    for (const Button& button : buttons_)
    {
        isAnyHovered = isAnyHovered || button.isHovered;
    }
    if (isAnyHovered && !wasAnyButtonHovered_)
    {
        params.impulseStrength = (std::max)(params.impulseStrength, kSlimeHoverImpulse);
    }
    wasAnyButtonHovered_ = isAnyHovered;
}

// ===================================================================
// 自動デモ
//
// ゲーム中にプレイヤーがやること（転がる / 投げる / 合体 / 分裂）を
// 気ままに繰り返す。状態は Roam（分裂）と Rolling（合体）の2つだけで、
// Merge / Split は状態の切り替わりに起きる瞬間のイベントとして扱う。
//
// 注意: MinionManager::TriggerSplit() は「非アクティブなミニオンが居ないと
// 何もしない」実装なので、必ず Merge を先に通すループにすること。
// ===================================================================

void TitleScene::UpdateDemo(float deltaTime)
{
    if (!isDemoEnabled_ || !slime_ || !minions_)
    {
        return;
    }

    // START を押したあとのフェードアウト中は触らない。
    // せっかく合体した状態が分裂に戻ってしまうため
    if (SceneManager::GetInstance()->IsTransitioning())
    {
        return;
    }

    demoTimer_ += deltaTime;

    switch (demoState_)
    {
    case DemoState::Roam:
        throwTimer_ -= deltaTime;
        if (throwTimer_ <= 0.0f)
        {
            DoThrow();
            throwTimer_ = RandomRange(demoThrowIntervalMin_, demoThrowIntervalMax_);
        }

        if (demoTimer_ >= demoDuration_)
        {
            EnterMerge();
            demoState_ = DemoState::Rolling;
            demoTimer_ = 0.0f;
            demoDuration_ = RandomRange(demoRollSecondsMin_, demoRollSecondsMax_);
        }
        break;

    case DemoState::Rolling:
        if (demoTimer_ >= demoDuration_)
        {
            EnterSplit();
            demoState_ = DemoState::Roam;
            demoTimer_ = 0.0f;
            demoDuration_ = RandomRange(demoRoamSecondsMin_, demoRoamSecondsMax_);
            throwTimer_ = 1.0f;
        }
        break;

    default:
        break;
    }
}

void TitleScene::DoThrow()
{
    if (!slime_ || !minions_ || slime_->IsMerged())
    {
        return;
    }

    Vector3 launchPos = slime_->GetPosition();
    launchPos.y += 0.5f;

    // 着弾点は定位置の左寄りにばらけさせる（右のボタン列に被らせない）
    const Vector3 target = {
        slimeHome_.x + kThrowBiasX + RandomRange(-kThrowRangeX * 0.5f, kThrowRangeX * 0.5f),
        slimeHome_.y,
        slimeHome_.z + RandomRange(-kThrowRangeZ * 0.5f, kThrowRangeZ * 0.5f)};

    // 飛行時間を決め打ちして初速を逆算する（AimGuide と同じやり方）
    const float flightTime = kThrowFlightTime;
    const Vector3 velocity = {
        (target.x - launchPos.x) / flightTime,
        ((target.y - launchPos.y) - 0.5f * kThrowGravity * flightTime * flightTime) / flightTime,
        (target.z - launchPos.z) / flightTime};

    if (!minions_->ThrowMinionWithVelocity(launchPos, velocity))
    {
        return; // 手元にミニオンが居なかった。次の機会に任せる
    }

    if (fx_)
    {
        fx_->EmitBurst(randomEngine_, launchPos, 6, 1.6f, 1.2f, kFxBulletColor, 0.16f, 0.32f, false);
    }

    // 投げた反動でぷるっと震える
    SlimeParamsCPU& params = slime_->GetSlimeParams();
    params.impulseStrength = (std::max)(params.impulseStrength, 0.18f);
}

void TitleScene::EnterMerge()
{
    if (!slime_ || slime_->IsMerged())
    {
        return;
    }

    const Vector3 playerPos = slime_->GetPosition();

    // 吸い寄せられる粒。実際の吸引は MinionManager が次の Update で始める
    if (fx_ && minions_)
    {
        for (const auto& minion : minions_->GetMinions())
        {
            if (!minion || !minion->IsActive())
            {
                continue;
            }
            fx_->EmitConverge(randomEngine_, minion->GetPosition(), playerPos, 3, kFxMergeColor,
                              0.15f, 0.45f);
        }
    }

    slime_->ToggleMerge();
    prevMergedCount_ = 0;
}

void TitleScene::EnterSplit()
{
    if (!slime_ || !slime_->IsMerged())
    {
        return;
    }

    const Vector3 playerPos = slime_->GetPosition();

    slime_->ToggleMerge();
    slimeFlashTimer_ = 0.12f;
    // 糸はミニオンが少し飛び出してから張る。
    // TriggerSplit はミニオンをプレイヤーの位置に置き直してから撃ち出すので、
    // 同じフレームに張ると長さ 0 になってしまう
    strandDelayFrames_ = 4;

    if (fx_)
    {
        // 鋭いキラッと、粘っこい飛沫を重ねる
        fx_->EmitBurst(randomEngine_, playerPos, 20, 5.0f, 3.2f, kFxSplitColor, 0.28f, 0.6f, true);
        fx_->EmitBurst(randomEngine_, playerPos, 12, 2.4f, 1.0f, kFxTrailColorNormal, 0.4f, 0.5f,
                       false);
    }

    prevMergedCount_ = 0;
}

// ===================================================================
// パーティクル演出
// ===================================================================

void TitleScene::UpdateFx(float deltaTime)
{
    if (!fx_)
    {
        return;
    }

    if (!showFx_ || !slime_)
    {
        fx_->Update(deltaTime);
        return;
    }

    const Vector3 slimePos = slime_->GetPosition();
    const bool isMerged = slime_->IsMerged();
    const float slimeScale = slime_->GetCurrentScale();
    const Vector4 bodyColor = isMerged ? kFxTrailColorMerged : kFxTrailColorNormal;
    const float groundY = slimeHome_.y + 0.02f;

    // --- 常時キラキラ ---
    if (fxEnableSparkle_)
    {
        fxSparkleAccum_ += deltaTime;
        // 合体中は密度を上げて「たくさん吸っている」感じを出す
        const float interval = (std::max)(0.01f, fxSparkleInterval_ * (isMerged ? 0.6f : 1.0f));
        while (fxSparkleAccum_ >= interval)
        {
            fxSparkleAccum_ -= interval;
            fx_->EmitSparkle(randomEngine_, {slimePos.x, slimePos.y + 0.1f, slimePos.z},
                             slimeScale * 1.1f, 1,
                             isMerged ? kFxSparkleColorMerged : kFxSparkleColorNormal, 0.14f, 0.85f);
        }
    }

    // --- 移動軌跡 ---
    const Vector3 movement = slimePos - fxPrevSlimePos_;
    const float movedDistance = std::sqrt(movement.x * movement.x + movement.z * movement.z);
    const float speed = movedDistance / (std::max)(deltaTime, 0.001f);

    if (fxEnableTrail_ && speed > kFxTrailSpeedThreshold)
    {
        fxTrailDistance_ += movedDistance;
        const float step = (std::max)(0.05f, fxTrailStep_);
        while (fxTrailDistance_ >= step)
        {
            fxTrailDistance_ -= step;

            // 地面に寝かせた「濡れた跡」
            if (fxEnableGroundMark_)
            {
                fx_->EmitGroundMark(randomEngine_, {slimePos.x, groundY, slimePos.z},
                                    slimeScale * 1.5f, bodyColor, 1.1f);
            }

            // 進行方向の逆にしずくを散らす
            const Vector3 dropletVelocity = {-movement.x * 2.0f, 0.6f, -movement.z * 2.0f};
            fx_->EmitDroplet(randomEngine_,
                             {slimePos.x, slimePos.y - slimeScale * 0.3f, slimePos.z},
                             dropletVelocity, bodyColor, 0.11f, 0.5f);
        }
    }
    else
    {
        fxTrailDistance_ = 0.0f;
    }
    fxPrevSlimePos_ = slimePos;

    // --- 弾軌跡 ---
    if (fxEnableBulletTrail_ && minions_)
    {
        for (const auto& minion : minions_->GetMinions())
        {
            if (!minion || !minion->IsActive())
            {
                continue;
            }
            if (minion->GetState() != MinionState::Thrown)
            {
                continue;
            }
            // 速度 0 で置いていくので、飛んだ跡がそのまま線として残る
            fx_->EmitDroplet(randomEngine_, minion->GetPosition(), {0.0f, 0.0f, 0.0f},
                             kFxBulletColor, 0.13f, 0.28f);
        }
    }

    // --- マージ: 1体吸収するたびに小さくバースト ---
    if (minions_ && isMerged)
    {
        const int mergedCount = minions_->GetMergedCount();
        if (mergedCount > prevMergedCount_)
        {
            const int absorbed = mergedCount - prevMergedCount_;
            for (int i = 0; i < absorbed; ++i)
            {
                fx_->EmitBurst(randomEngine_, slimePos, 6, 2.2f, 1.4f, kFxMergeColor, 0.16f, 0.4f,
                               true);
            }
            prevMergedCount_ = mergedCount;

            // 表面全体がぶるっと震える（波紋とは質の違う揺れ）
            SlimeParamsCPU& params = slime_->GetSlimeParams();
            params.wobbleStrength = (std::min)(0.45f, params.wobbleStrength + 0.06f);
        }
    }
    else if (!isMerged)
    {
        // 跳ね上げた wobble を少しずつ元へ戻す
        SlimeParamsCPU& params = slime_->GetSlimeParams();
        params.wobbleStrength += (0.24f - params.wobbleStrength) * (std::min)(1.0f, deltaTime * 2.0f);
    }

    // --- 分裂の少しあと: 親子を結ぶ「粘りの糸」 ---
    if (strandDelayFrames_ > 0)
    {
        --strandDelayFrames_;
        if (strandDelayFrames_ == 0 && minions_)
        {
            for (const auto& minion : minions_->GetMinions())
            {
                if (!minion || !minion->IsActive())
                {
                    continue;
                }
                fx_->EmitStrand(randomEngine_, slimePos, minion->GetPosition(), 4, kFxStrandColor,
                                0.12f, 0.22f);
            }
        }
    }

    // --- 背景 ---
    if (fxEnableBackground_)
    {
        fxBackgroundAccum_ += deltaTime;
        const float interval = (std::max)(0.05f, fxBackgroundInterval_);
        while (fxBackgroundAccum_ >= interval)
        {
            fxBackgroundAccum_ -= interval;

            // カメラの手前から奥までばら撒く。ビルボードなので遠近がそのまま出る
            SlimeFxDesc desc;
            desc.position = {cameraPos_.x + RandomRange(-9.0f, 9.0f),
                             slimeHome_.y + RandomRange(-1.5f, 5.0f),
                             cameraPos_.z + RandomRange(4.0f, 18.0f)};
            desc.velocity = {RandomRange(-0.15f, 0.15f), RandomRange(0.15f, 0.5f), 0.0f};
            desc.colorBegin = kFxBackgroundColor;
            desc.colorEnd = {kFxBackgroundColor.x, kFxBackgroundColor.y, kFxBackgroundColor.z, 0.0f};
            desc.scaleBegin = RandomRange(0.25f, 0.7f);
            desc.scaleEnd = RandomRange(0.3f, 0.9f);
            desc.lifeTime = kFxBackgroundLifeTime * RandomRange(0.7f, 1.3f);
            desc.useSparkTexture = false;
            fx_->Emit(desc);
        }
    }

    fx_->Update(deltaTime);
}

Vector3 TitleScene::GetMouseGroundPoint(bool& outValid) const
{
    outValid = false;
    Vector3 result = slimeHome_;

    if (!slimeCamera_)
    {
        return result;
    }

    const float screenWidth = static_cast<float>(WindowAPI::GetClientWidth());
    const float screenHeight = static_cast<float>(WindowAPI::GetClientHeight());

    const Vector2 mouse = GetMousePositionOnUI();
    if (mouse.x < 0.0f || mouse.y < 0.0f || mouse.x > screenWidth || mouse.y > screenHeight)
    {
        return result; // 画面の外。うろつきだけに任せる
    }

    const float ndcX = (mouse.x / screenWidth) * 2.0f - 1.0f;
    const float ndcY = 1.0f - (mouse.y / screenHeight) * 2.0f;

    // カメラ空間でのレイ方向
    const float tanHalfFov = std::tan(slimeCamera_->GetFovY() * 0.5f);
    const Vector3 rayInCamera = {ndcX * tanHalfFov * slimeCamera_->GetAspectRatio(),
                                 ndcY * tanHalfFov, 1.0f};

    // タイトルカメラはピッチ（X軸回転）しか使っていないので、その分だけ回してワールドに戻す
    const float cosPitch = std::cos(cameraPitch_);
    const float sinPitch = std::sin(cameraPitch_);
    const Vector3 rayInWorld = {rayInCamera.x,
                                rayInCamera.y * cosPitch - rayInCamera.z * sinPitch,
                                rayInCamera.y * sinPitch + rayInCamera.z * cosPitch};

    if (rayInWorld.y > -1e-4f)
    {
        return result; // 地平線より上を指している
    }

    const float rayLength = (slimeHome_.y - cameraPos_.y) / rayInWorld.y;
    if (rayLength <= 0.0f)
    {
        return result;
    }

    result = {cameraPos_.x + rayInWorld.x * rayLength, slimeHome_.y,
              cameraPos_.z + rayInWorld.z * rayLength};
    outValid = true;
    return result;
}

void TitleScene::DecideMenu(MenuItem item)
{
    switch (item)
    {
    case MenuItem::Start:
        // 合体させてからゲームへ。ChangeScene はフェードアウトの予約なので、
        // 暗転していく間に「ぷるんと膨らんで黄金色になる」ところが見える
        if (slime_ && !slime_->IsMerged())
        {
            slime_->ToggleMerge();
        }
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

void TitleScene::Draw(SceneRenderRequests& renderRequests)
{
    if (!dxCommon_ || !dxCommon_->GetCommandList())
    {
        return;
    }

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList().Get();

    // 背景スカイボックスの描画
    SceneManager::GetInstance()->DrawSkybox(commandList);

    // --- 3D パート ---
    // UI より先に描いて背面に置く。
    // ここはオフスクリーンパスの中なので、ポストプロセスが乗る
    if (showSlime_ && slimeCamera_)
    {
        // これを立てるとエンジン側のデバッグ用 plane が出なくなる
        renderRequests.sceneDrawn = true;

        RenderContext ctx;
        ctx.commandList = commandList;
        ctx.camera = slimeCamera_;
        ctx.light = GetLight();

        // デプスを書くもの（ミニオン → スライム）を先に、
        // デプスを書かないパーティクルを最後に描く。
        // こうするとパーティクルは前後関係だけ正しく効いて、粒同士は隠し合わない
        if (minions_)
        {
            minions_->Draw(ctx);
        }
        if (slime_)
        {
            slime_->Draw(ctx);
        }
        if (fx_ && showFx_)
        {
            fx_->Draw(ctx);
        }
    }

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
            isSlimeIntroPulseDone_ = false;
        }
    }

    if (ImGui::CollapsingHeader("Title Slime", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Show Slime", &showSlime_);

        if (slime_)
        {
            const Vector3& position = slime_->GetPosition();
            ImGui::Text("Pos  : %.2f, %.2f, %.2f", position.x, position.y, position.z);
            ImGui::Text("Tilt : pitch %.1f deg / roll %.1f deg", slimeTilt_.x * 57.2958f,
                        slimeTilt_.y * 57.2958f);
        }

        ImGui::SeparatorText("Camera");
        ImGui::DragFloat3("Camera Pos", &cameraPos_.x, 0.05f, -30.0f, 30.0f, "%.2f");
        ImGui::SliderFloat("Camera Pitch", &cameraPitch_, -0.6f, 1.2f, "%.3f rad");

        ImGui::SeparatorText("Movement");
        ImGui::DragFloat3("Home", &slimeHome_.x, 0.05f, -20.0f, 20.0f, "%.2f");
        ImGui::SliderFloat("Roam Radius", &slimeRoamRadius_, 0.0f, 8.0f, "%.2f");
        ImGui::SliderFloat("Follow Mouse", &slimeFollowRate_, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Tilt Gain", &slimeTiltGain_, 0.01f, 0.6f, "%.3f");
        ImGui::SliderFloat("Max Tilt", &slimeMaxTilt_, 0.02f, 0.8f, "%.3f rad");

        if (slime_)
        {
            float tiltAccel = slime_->GetTiltAccel();
            if (ImGui::SliderFloat("Tilt Accel", &tiltAccel, 2.0f, 60.0f, "%.1f"))
            {
                slime_->SetTiltAccel(tiltAccel);
            }
            float friction = slime_->GetFriction();
            if (ImGui::SliderFloat("Friction", &friction, 0.5f, 8.0f, "%.2f"))
            {
                slime_->SetFriction(friction);
            }

            ImGui::SeparatorText("Look");
            SlimeParamsCPU& params = slime_->GetSlimeParams();
            ImGui::SliderFloat("Wobble Strength", &params.wobbleStrength, 0.0f, 0.5f, "%.3f");
            ImGui::SliderFloat("Wobble Frequency", &params.wobbleFrequency, 1.0f, 15.0f, "%.1f");
            ImGui::SliderFloat("Fresnel Power", &params.fresnelPower, 0.5f, 6.0f, "%.1f");
            ImGui::SliderFloat("Env Reflection", &params.envReflection, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Inner Glow", &params.innerGlow, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Shininess", &params.specularShininess, 8.0f, 128.0f, "%.0f");

            // PikminPlayer::Update() が毎フレーム色を塗り直すので、
            // チェックを外している間はゲーム側と同じ色になる
            ImGui::Checkbox("Override Color", &slimeOverrideColor_);
            ImGui::ColorEdit4("Slime Color", &slimeColor_.x);

            ImGui::SeparatorText("Test");
            if (ImGui::Button("Impulse Ripple", ImVec2(140.0f, 0.0f)))
            {
                params.impulseStrength = kSlimeIntroImpulse;
            }
            ImGui::SameLine();
            if (ImGui::Button(slime_->IsMerged() ? "Split" : "Merge", ImVec2(140.0f, 0.0f)))
            {
                slime_->ToggleMerge();
            }
        }

        if (ImGui::Button("Reset Slime", ImVec2(140.0f, 0.0f)))
        {
            ResetSlimeTuningToDefault();
        }
    }

    if (ImGui::CollapsingHeader("Title Demo", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Auto Demo", &isDemoEnabled_);

        const char* stateName = (demoState_ == DemoState::Rolling) ? "Rolling (merged)" : "Roam (split)";
        ImGui::Text("State  : %s  %.1f / %.1f s", stateName, demoTimer_, demoDuration_);
        if (minions_)
        {
            ImGui::Text("Minions: %d active / %d merged / %d total", minions_->GetActiveCount(),
                        minions_->GetMergedCount(), minions_->GetTotalCount());
        }
        ImGui::Text("Throw in : %.2f s", throwTimer_);

        ImGui::SeparatorText("Timing");
        ImGui::DragFloatRange2("Roam Seconds", &demoRoamSecondsMin_, &demoRoamSecondsMax_, 0.1f,
                               0.5f, 20.0f, "%.1f", "%.1f");
        ImGui::DragFloatRange2("Roll Seconds", &demoRollSecondsMin_, &demoRollSecondsMax_, 0.1f,
                               0.5f, 20.0f, "%.1f", "%.1f");
        ImGui::DragFloatRange2("Throw Interval", &demoThrowIntervalMin_, &demoThrowIntervalMax_,
                               0.05f, 0.1f, 6.0f, "%.2f", "%.2f");

        ImGui::SeparatorText("Manual");
        if (ImGui::Button("Throw", ImVec2(90.0f, 0.0f)))
        {
            DoThrow();
        }
        ImGui::SameLine();
        if (ImGui::Button("Merge", ImVec2(90.0f, 0.0f)))
        {
            EnterMerge();
            demoState_ = DemoState::Rolling;
            demoTimer_ = 0.0f;
            demoDuration_ = RandomRange(demoRollSecondsMin_, demoRollSecondsMax_);
        }
        ImGui::SameLine();
        if (ImGui::Button("Split", ImVec2(90.0f, 0.0f)))
        {
            EnterSplit();
            demoState_ = DemoState::Roam;
            demoTimer_ = 0.0f;
            demoDuration_ = RandomRange(demoRoamSecondsMin_, demoRoamSecondsMax_);
        }
    }

    if (ImGui::CollapsingHeader("Title FX"))
    {
        ImGui::Checkbox("Show FX", &showFx_);
        if (fx_)
        {
            ImGui::Text("Particles: %d / %u", fx_->GetActiveCount(), fx_->GetCapacity());
        }

        ImGui::SeparatorText("Toggles");
        ImGui::Checkbox("Sparkle (player)", &fxEnableSparkle_);
        ImGui::Checkbox("Movement Trail", &fxEnableTrail_);
        ImGui::Checkbox("  - Ground Mark", &fxEnableGroundMark_);
        ImGui::Checkbox("Bullet Trail", &fxEnableBulletTrail_);
        ImGui::Checkbox("Background", &fxEnableBackground_);

        ImGui::SeparatorText("Density");
        ImGui::SliderFloat("Sparkle Interval", &fxSparkleInterval_, 0.01f, 0.4f, "%.3f s");
        ImGui::SliderFloat("Trail Step", &fxTrailStep_, 0.05f, 1.5f, "%.2f m");
        ImGui::SliderFloat("Background Interval", &fxBackgroundInterval_, 0.05f, 2.0f, "%.2f s");

        if (ImGui::Button("Clear Particles", ImVec2(140.0f, 0.0f)) && fx_)
        {
            fx_->Clear();
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
