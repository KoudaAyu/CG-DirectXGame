#include "ClearScene.h"

#include "Camera.h"
#include "DirectXCom.h"
#include "KeyInput.h"
#include "RenderContext.h"
#include "SceneManager.h"
#include "WindowsAPI.h"

#include "Application/GameObject/SlimeFx.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3dCom.h"
#include "Baziru3_Engine/Graphics/Graphics/SceneRenderRequests.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <numeric>
#include <utility>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace {

// ===================================================================
// SceneContext のキー
//
// GamePlayScene 側はまだ何も書いていないので、無ければ既定値で動く。
// 向こうで用意ができたら、CLEAR へ遷移する直前に
//     SetSceneDataInt("result.score", score);
//     SetSceneDataFloat("result.time", elapsedSeconds);
//     SetSceneDataInt("result.coin", coin);
// を呼べばそのまま繋がる。
// ===================================================================
constexpr const char* kResultScoreKey = "result.score";
constexpr const char* kResultTimeKey = "result.time";
constexpr const char* kResultCoinKey = "result.coin";

// 値が渡ってこなかったときのダミー（演出の見た目を確認するためのもの）
constexpr int kResultScoreDefault = 8420;
constexpr float kResultTimeDefault = 95.0f; // 秒
constexpr int kResultCoinDefault = 23;

// ===================================================================
// ロゴ（1文字＝1スプライト）
//
// "STAGE CLEAR!!" を12枚に割っている。
// 文字数・並び・幅・単語の区切りは、この配列を書き換えるだけで変わる。
// 画像がまだ無くても engine が白いダミー（4x4）に差し替えるので、
// 白い四角が並んで動きの確認ができる（落ちない）。
// ===================================================================
struct LogoChar
{
    const char* texture;  // 画像パス
    float width;          // 基準幅（ピクセル）
    float extraSpacing;   // この文字の後ろに足す隙間（単語の区切り）
};

constexpr LogoChar kLogoChars[] = {
    {"Resources/UI/Clear/logo_char_01.png", 54.0f, 0.0f },  // S
    {"Resources/UI/Clear/logo_char_02.png", 54.0f, 0.0f },  // T
    {"Resources/UI/Clear/logo_char_03.png", 54.0f, 0.0f },  // A
    {"Resources/UI/Clear/logo_char_04.png", 54.0f, 0.0f },  // G
    {"Resources/UI/Clear/logo_char_05.png", 54.0f, 34.0f},  // E （ここで単語が切れる）
    {"Resources/UI/Clear/logo_char_06.png", 54.0f, 0.0f },  // C
    {"Resources/UI/Clear/logo_char_07.png", 54.0f, 0.0f },  // L
    {"Resources/UI/Clear/logo_char_08.png", 54.0f, 0.0f },  // E
    {"Resources/UI/Clear/logo_char_09.png", 54.0f, 0.0f },  // A
    {"Resources/UI/Clear/logo_char_10.png", 54.0f, 0.0f },  // R
    {"Resources/UI/Clear/logo_char_11.png", 30.0f, 0.0f },  // !
    {"Resources/UI/Clear/logo_char_12.png", 30.0f, 0.0f },  // !
};

constexpr int kLogoCharCount = static_cast<int>(std::size(kLogoChars));

// ===================================================================
// 数字アトラス
//
// 1枚の画像に 0-9 と ":" をまとめて、切り出し位置を毎フレーム変えて使う。
// Sprite::Update() が textureLeftTop_ / textureSize_ から UV を計算し直すので、
// スプライトを作り直さずに数字を差し替えられる。
//
//   セル 128 x 128 / 4列 x 3行 / 全体 512 x 384
//   index 0-9 = 数字 / 10 = ":" / 11 = 予備（空でよい）
//
// セルサイズと列数を変えたくなったら、下の3つを書き換えるだけでいい。
// ===================================================================
constexpr const char* kDigitAtlasTexture = "Resources/UI/Clear/digits.png";
constexpr Vector2 kDigitCellSize = {128.0f, 128.0f};
constexpr int kDigitAtlasColumns = 4;
constexpr int kColonCell = 10; // ":" のセル番号

// ":" は数字より細いので、画面上の横幅だけ詰める
constexpr float kColonWidthScale = 0.5f;

// ===================================================================
// ラベル画像
// ===================================================================
constexpr const char* kLabelTextures[] = {
    "Resources/UI/Clear/label_score.png",
    "Resources/UI/Clear/label_time.png",
    "Resources/UI/Clear/label_coin.png",
};

constexpr const char* kPromptTexture = "Resources/UI/Clear/press_space.png";

// 行の並び。kLabelTextures / kRowLayouts と対応している
enum RowIndex
{
    kRowScore = 0,
    kRowTime,
    kRowCoin,

    kRowCount,
};

// ===================================================================
// レイアウト初期値（1280x720 基準）
// 実行中は ImGui の "Clear Scene" ウィンドウから調整できる
// ===================================================================
constexpr float kScreenWidth = 1280.0f;
constexpr float kScreenHeight = 720.0f;

constexpr Vector2 kLogoCenterDefault = {380.0f, 128.0f};
constexpr float kLogoScaleDefault = 1.0f;
constexpr float kLogoCharHeightDefault = 94.0f;
constexpr float kLogoCharSpacingDefault = 5.0f;

// ===================================================================
// リザルトのグループ
//
// SCORE / TIME / COIN / PRESS SPACE は1つのまとまりとして扱う。
// 下の座標はすべて「グループ原点からの相対」で、
// resultGroupOrigin_ を動かすと4つまとめてずれる。
// ===================================================================
constexpr Vector2 kResultGroupOriginDefault = {1000.0f, 150.0f};

// 1行分のレイアウト（ラベル / ラベルのサイズ / 数値の右端の桁 / 桁数）。座標はグループ相対
struct RowLayout
{
    Vector2 labelOffset;
    Vector2 labelSize;
    Vector2 valueRightOffset;
    int digitCount;
    bool isTime;
};

constexpr RowLayout kRowLayouts[kRowCount] = {
    {{10.0f, 0.0f},   {190.0f, 56.0f}, {190.0f, 80.0f},  5, false}, // SCORE
    {{0.0f, 150.0f},  {160.0f, 56.0f}, {180.0f, 230.0f}, 5, true }, // TIME (MM:SS)
    {{5.0f, 300.0f},  {170.0f, 56.0f}, {170.0f, 380.0f}, 3, false}, // COIN
};

constexpr Vector2 kDigitSizeDefault = {56.0f, 72.0f};
constexpr float kDigitSpacingDefault = 4.0f;

constexpr Vector2 kPromptOffsetDefault = {-30.0f, 500.0f}; // グループ相対
constexpr Vector2 kPromptSizeDefault = {520.0f, 62.0f};

// ===================================================================
// 演出パラメータ初期値
// ===================================================================
constexpr float kDeltaTime = 1.0f / 60.0f; // SceneManager が固定タイムステップで回している
constexpr float kFadeInSeconds = 0.5f;

// ロゴの落下
constexpr float kDropSecondsDefault = 0.42f;  // 1文字が落ちきるまで
constexpr float kDropStaggerDefault = 0.11f;  // 文字ごとのディレイ（順番はシャッフル）
constexpr float kDropHeightDefault = 460.0f;  // 基準位置からどれだけ上から落とすか
constexpr float kLandSquashDefault = 0.42f;   // 着地の潰れ量
constexpr float kLandDampingDefault = 7.0f;   // 潰れの減衰（1/秒）
constexpr float kLandFrequencyDefault = 22.0f;// 潰れの振動数（rad/秒）

// 着地したあとの常時ゆらぎ（タイトルロゴと同じ味付け）
constexpr float kIdleBobAmplitudeDefault = 6.0f;
constexpr float kIdleBobSpeedDefault = 2.0f;
constexpr float kIdleJiggleAmountDefault = 0.05f;
constexpr float kIdleJiggleSpeedDefault = 3.2f;
constexpr float kIdleWobbleDegreesDefault = 2.5f;

// ラベル・数値の登場
constexpr float kPopSeconds = 0.36f;          // 1個が出きるまで
constexpr float kCountBeginOffset = 0.10f;    // 数値が出てからカウントが回り出すまで
constexpr float kCountSecondsDefault = 0.55f; // 0 から目標値まで上がりきる時間

// 登場の順番。ロゴが出そろったあと、下の7つが sequenceStep_ 秒おきに次々と出る
//   0: SCORE ラベル / 1: SCORE 数値 / 2: TIME ラベル / 3: TIME 数値
//   4: COIN ラベル  / 5: COIN 数値  / 6: PRESS SPACE
constexpr float kSequenceBeginOffsetDefault = 0.35f;
constexpr float kSequenceStepDefault = 0.63f;
constexpr float kPromptExtraDelayDefault = 0.35f; // 最後の数値が上がりきるのを待つ分

constexpr int kSequenceSlotPrompt = 6;

constexpr float kDigitPunchAmountDefault = 0.30f; // 桁が変わった瞬間の弾み
constexpr float kDigitPunchDampingDefault = 11.0f;

// 点滅プロンプト
constexpr float kPromptBlinkSpeedDefault = 3.4f;
constexpr float kPromptAlphaMinDefault = 0.25f;
constexpr float kPromptAlphaMaxDefault = 1.0f;

// ===================================================================
// 背景の花火
// ===================================================================
// 花火1発で 47粒（閃光1 + 内殻16 + 外殻22 + 尾8）使う。
// 同時に3〜4発（141）+ 上昇軌跡（約76）+ 環境の粒（約20）でピーク 250 前後。
// 粒1個につき Object3d が1個（＝頂点／インデックスバッファも1個）なので、
// シーン入場時の生成コストとの兼ね合いでこのくらいに抑えている
constexpr uint32_t kFxCapacity = 384;
constexpr int kMaxRockets = 4;

constexpr bool kFxAdditiveDefault = true;
constexpr bool kFxUseColorFieldDefault = true;

constexpr float kLaunchIntervalMinDefault = 0.55f;
constexpr float kLaunchIntervalMaxDefault = 1.30f;
constexpr float kFireworkPowerMinDefault = 1.05f;
constexpr float kFireworkPowerMaxDefault = 2.56f;
constexpr float kRocketGravityDefault = 9.5f;

constexpr float kRocketTrailInterval = 0.016f; // 「シュー」の軌跡の刻み
constexpr float kLaunchY = -6.8f;              // 画面下端の外から上げる
constexpr float kBurstXRange = 7.2f;
constexpr float kBurstYMin = -0.4f;
constexpr float kBurstYMax = 3.6f;
constexpr float kBurstZMin = -2.0f;
constexpr float kBurstZMax = 4.0f;

// カラー場を使うときは RGB が上書きされるので、ここは α（＝寿命に沿った濃さ）だけが効く
constexpr Vector4 kFireworkCoreColor = {1.0f, 1.0f, 0.92f, 1.0f};
constexpr Vector4 kFireworkShellColor = {0.75f, 0.9f, 1.0f, 1.0f};
constexpr Vector4 kRocketTrailColor = {1.0f, 0.95f, 0.8f, 0.9f};
constexpr Vector4 kAmbientColor = {0.8f, 0.9f, 1.0f, 0.30f};

constexpr float kAmbientInterval = 0.30f;
constexpr float kAmbientLifeTime = 6.0f;

// カラー場 color(time, position) の既定値
constexpr float kFieldScaleDefault = 0.05f;
constexpr float kFieldTimeScaleDefault = 0.04f;
constexpr float kFieldSwirlDefault = 0.1f;
constexpr float kFieldSaturationDefault = 0.8f;
constexpr float kFieldGainDefault = 0.95f;
constexpr Vector3 kFieldDirectionDefault = {1.0f, 0.75f, 0.45f};

// 2つ目の場（金 → 赤 → 紫）。1つ目と相関しないように向きも速さも変えてある
constexpr float kEmberBlendDefault = 0.55f;
constexpr float kEmberScaleDefault = 0.09f;
constexpr float kEmberTimeScaleDefault = 0.07f;
constexpr float kEmberSwirlDefault = 0.35f;
constexpr Vector3 kEmberDirectionDefault = {-0.6f, 1.0f, 0.3f};

// 巡回グラデーションの3ストップ。紫から金へ戻るので継ぎ目が出ない
constexpr Vector3 kEmberStops[3] = {
    {1.00f, 0.78f, 0.25f}, // 金
    {1.00f, 0.22f, 0.12f}, // 赤
    {0.62f, 0.16f, 0.85f}, // 紫
};

// カメラ。正面固定（yaw / pitch / roll = 0）で、z = 0 の平面が画面いっぱいになる位置
constexpr Vector3 kCameraPosDefault = {0.0f, 0.0f, -20.0f};
constexpr float kCameraFovY = 0.45f; // Camera のコンストラクタと同じ値

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = kPi * 2.0f;

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

/// <summary>だんだん速くなる（落下に使う）</summary>
float EaseInQuad(float x) { return x * x; }

/// <summary>最後にゆっくり止まる（カウントアップに使う）</summary>
float EaseOutCubic(float x)
{
    const float t = 1.0f - x;
    return 1.0f - t * t * t;
}

/// <summary>インデックスから 0..1 の疑似乱数を作る（毎回同じ結果になる）</summary>
float Hash01(uint32_t value)
{
    value = value * 747796405u + 2891336453u;
    uint32_t word = ((value >> ((value >> 28) + 4)) ^ value) * 277803737u;
    word = (word >> 22) ^ word;
    return static_cast<float>(word) / static_cast<float>(0xFFFFFFFFu);
}

/// <summary>アトラスのセル番号から切り出し左上座標を出す</summary>
Vector2 CellLeftTop(int cell)
{
    const int column = cell % kDigitAtlasColumns;
    const int row = cell / kDigitAtlasColumns;
    return {static_cast<float>(column) * kDigitCellSize.x,
            static_cast<float>(row) * kDigitCellSize.y};
}

} // namespace

// unique_ptr が持つ型（SlimeFx）の完全な定義が要るので、
// コンストラクタとデストラクタはヘッダではなくここで定義する
ClearScene::ClearScene() = default;
ClearScene::~ClearScene() = default;

// ===================================================================
// 初期化 / 終了
// ===================================================================

void ClearScene::InitializeScene()
{
    if (dxCommon_)
    {
        input_ = new KeyInput();
        input_->Initialize(dxCommon_->GetWindowAPI());
    }

    randomEngine_.seed(std::random_device{}());

    ResetTuningToDefault();

    CreateLogo();
    CreateLabels();
    CreateNumbers();
    CreatePrompt();
    CreateFx();

    LoadResultFromSceneContext();

    sceneTime_ = 0.0f;
    fadeAlpha_ = 0.0f;
    isLogoBurstDone_ = false;
    launchTimer_ = 0.6f; // 最初の1発は少し待ってから
    ambientAccum_ = 0.0f;
}

void ClearScene::Finalize()
{
    for (LogoLetter& letter : logoLetters_)
    {
        if (letter.sprite)
        {
            letter.sprite->Finalize();
            letter.sprite.reset();
        }
    }
    logoLetters_.clear();

    for (Label& label : labels_)
    {
        if (label.sprite)
        {
            label.sprite->Finalize();
            label.sprite.reset();
        }
    }
    labels_.clear();

    for (NumberRow& row : numberRows_)
    {
        for (Digit& digit : row.digits)
        {
            if (digit.sprite)
            {
                digit.sprite->Finalize();
                digit.sprite.reset();
            }
        }
        row.digits.clear();
    }
    numberRows_.clear();

    if (promptSprite_)
    {
        promptSprite_->Finalize();
        promptSprite_.reset();
    }

    fx_.reset();
    rockets_.clear();

    // 借りていた engine カメラを元の位置に戻す。
    // ここで Camera::Update() を呼んではいけない（終了時は CB アロケータが
    // 先に片付いている可能性があり、アクセス違反になる）。
    // 行列の再計算は次のフレームに Game::Update() がやってくれる
    if (fxCamera_)
    {
        fxCamera_->SetTranslate(savedCameraTranslate_);
        fxCamera_->SetRotate(savedCameraRotate_);
        fxCamera_ = nullptr;
    }

    delete input_;
    input_ = nullptr;
}

void ClearScene::ResetTuningToDefault()
{
    logoCenter_ = kLogoCenterDefault;
    logoScale_ = kLogoScaleDefault;
    logoCharHeight_ = kLogoCharHeightDefault;
    logoCharSpacing_ = kLogoCharSpacingDefault;

    dropSeconds_ = kDropSecondsDefault;
    dropStagger_ = kDropStaggerDefault;
    dropHeight_ = kDropHeightDefault;
    landSquash_ = kLandSquashDefault;
    landDamping_ = kLandDampingDefault;
    landFrequency_ = kLandFrequencyDefault;

    idleBobAmplitude_ = kIdleBobAmplitudeDefault;
    idleBobSpeed_ = kIdleBobSpeedDefault;
    idleJiggleAmount_ = kIdleJiggleAmountDefault;
    idleJiggleSpeed_ = kIdleJiggleSpeedDefault;
    idleWobbleDegrees_ = kIdleWobbleDegreesDefault;

    digitSize_ = kDigitSizeDefault;
    digitSpacing_ = kDigitSpacingDefault;
    digitPunchAmount_ = kDigitPunchAmountDefault;
    digitPunchDamping_ = kDigitPunchDampingDefault;

    resultGroupOrigin_ = kResultGroupOriginDefault;

    sequenceBeginOffset_ = kSequenceBeginOffsetDefault;
    sequenceStep_ = kSequenceStepDefault;
    promptExtraDelay_ = kPromptExtraDelayDefault;

    promptOffset_ = kPromptOffsetDefault;
    promptSize_ = kPromptSizeDefault;
    promptBlinkSpeed_ = kPromptBlinkSpeedDefault;
    promptAlphaMin_ = kPromptAlphaMinDefault;
    promptAlphaMax_ = kPromptAlphaMaxDefault;

    showFx_ = true;
    fxAdditive_ = kFxAdditiveDefault;
    fxUseColorField_ = kFxUseColorFieldDefault;
    fxEnableAmbient_ = true;
    launchIntervalMin_ = kLaunchIntervalMinDefault;
    launchIntervalMax_ = kLaunchIntervalMaxDefault;
    fireworkPowerMin_ = kFireworkPowerMinDefault;
    fireworkPowerMax_ = kFireworkPowerMaxDefault;
    rocketGravity_ = kRocketGravityDefault;

    fieldScale_ = kFieldScaleDefault;
    fieldTimeScale_ = kFieldTimeScaleDefault;
    fieldSwirl_ = kFieldSwirlDefault;
    fieldSaturation_ = kFieldSaturationDefault;
    fieldGain_ = kFieldGainDefault;
    fieldDirection_ = kFieldDirectionDefault;

    emberBlend_ = kEmberBlendDefault;
    emberScale_ = kEmberScaleDefault;
    emberTimeScale_ = kEmberTimeScaleDefault;
    emberSwirl_ = kEmberSwirlDefault;
    emberDirection_ = kEmberDirectionDefault;

    cameraPos_ = kCameraPosDefault;
}

float ClearScene::RandomRange(float minValue, float maxValue)
{
    std::uniform_real_distribution<float> distribution(minValue, maxValue);
    return distribution(randomEngine_);
}

// ===================================================================
// 生成
// ===================================================================

void ClearScene::CreateLogo()
{
    logoLetters_.clear();
    logoLetters_.reserve(kLogoCharCount);

    for (int i = 0; i < kLogoCharCount; ++i)
    {
        LogoLetter letter;
        letter.baseWidth = kLogoChars[i].width;
        letter.extraSpacing = kLogoChars[i].extraSpacing;

        // 着地後のゆらぎは文字ごとに位相と速度をバラす（毎回同じ結果になる）
        letter.phase = Hash01(static_cast<uint32_t>(i) * 2u + 1u) * kTwoPi;
        letter.speedScale = 0.8f + Hash01(static_cast<uint32_t>(i) * 2u + 7u) * 0.5f;
        letter.landTimer = 0.0f;
        letter.isLanded = false;

        // アンカーは下端中央。着地の潰れが「接地したまま」になってグミっぽい
        letter.sprite = MakeSprite(kLogoChars[i].texture,
                                   {letter.baseWidth, logoCharHeight_}, {0.5f, 1.0f});

        logoLetters_.push_back(std::move(letter));
    }

    // 落ちてくる順番をシャッフルする。
    // order[k] = k 番目に落ちてくる文字のインデックス
    std::vector<int> order(logoLetters_.size());
    std::iota(order.begin(), order.end(), 0);
    std::shuffle(order.begin(), order.end(), randomEngine_);

    for (size_t k = 0; k < order.size(); ++k)
    {
        // 実際のディレイは RebuildTimeline() が dropStagger_ から作る。
        // ここでは順番だけ覚えておく（ImGui で間隔をいじってもすぐ効くように）
        logoLetters_[static_cast<size_t>(order[k])].dropOrder = static_cast<int>(k);
    }

    LayoutLogo();
}

void ClearScene::CreateLabels()
{
    labels_.clear();
    labels_.resize(kRowCount);

    for (int i = 0; i < kRowCount; ++i)
    {
        Label& label = labels_[static_cast<size_t>(i)];
        label.offset = kRowLayouts[i].labelOffset;
        label.size = kRowLayouts[i].labelSize;
        label.sprite = MakeSprite(kLabelTextures[i], label.size, {0.5f, 0.5f});
    }
}

void ClearScene::CreateNumbers()
{
    numberRows_.clear();
    numberRows_.resize(kRowCount);

    for (int i = 0; i < kRowCount; ++i)
    {
        NumberRow& row = numberRows_[static_cast<size_t>(i)];
        row.isTime = kRowLayouts[i].isTime;
        row.rightOffset = kRowLayouts[i].valueRightOffset;
        row.countSeconds = kCountSecondsDefault;
        row.shown = 0.0f;
        row.target = 0;
        row.isCountFinished = false;
        row.finishPunch = 0.0f;

        row.digits.resize(static_cast<size_t>(kRowLayouts[i].digitCount));
        for (Digit& digit : row.digits)
        {
            digit.cell = 0;
            digit.shownCell = -1;
            digit.punch = 0.0f;
            digit.sprite = MakeSprite(kDigitAtlasTexture, digitSize_, {0.5f, 0.5f});
            if (digit.sprite)
            {
                // 切り出しサイズはここで一度決めれば十分。
                // 左上座標だけを毎フレーム差し替えて数字を切り替える
                digit.sprite->SetTextureSize(kDigitCellSize);
            }
        }

        AssignCells(row);
    }

    RebuildTimeline();
}

void ClearScene::CreatePrompt()
{
    promptSprite_ = MakeSprite(kPromptTexture, promptSize_, {0.5f, 0.5f});
}

void ClearScene::CreateFx()
{
    Object3dCom* object3dCom = GetObject3dCom();
    if (!dxCommon_ || !object3dCom)
    {
        return;
    }

    // engine のカメラを借りる。
    // Game がスカイボックスにもパーティクルにも同じカメラを渡しているので、
    // これを使っておけば視点がずれない。
    // SceneManager::GetCamera() は GAMEPLAY を経由すると解放済みを指すので使わない
    fxCamera_ = object3dCom->GetDefaultCamera();
    if (fxCamera_)
    {
        savedCameraTranslate_ = fxCamera_->GetTranslate();
        savedCameraRotate_ = fxCamera_->GetRotate();
    }

    fx_ = std::make_unique<SlimeFx>();
    fx_->Initialize(object3dCom, fxCamera_, kFxCapacity);
    fx_->SetAdditive(fxAdditive_);
    fx_->SetCameraPitch(0.0f); // カメラは正面固定なので板は常に正対する

    // 粒の色を決めるベクター場を登録する。
    // useColorField を立てた粒だけが、毎フレームここを通って塗り直される
    fx_->SetColorField([this](float time, const Vector3& position) {
        return EvaluateColorField(time, position);
    });

    rockets_.assign(kMaxRockets, Rocket{});
}

void ClearScene::LoadResultFromSceneContext()
{
    if (numberRows_.size() < static_cast<size_t>(kRowCount))
    {
        return;
    }

    numberRows_[kRowScore].target = GetSceneDataInt(kResultScoreKey, kResultScoreDefault);
    numberRows_[kRowCoin].target = GetSceneDataInt(kResultCoinKey, kResultCoinDefault);

    // タイムは秒（float）で受け取って、表示のときに MM:SS へ組み替える
    const float seconds = GetSceneDataFloat(kResultTimeKey, kResultTimeDefault);
    numberRows_[kRowTime].target = static_cast<int>(std::lround(seconds));
}

void ClearScene::LayoutLogo()
{
    if (logoLetters_.empty())
    {
        return;
    }

    // 全体幅を出してから中央揃えする
    float totalWidth = 0.0f;
    for (size_t i = 0; i < logoLetters_.size(); ++i)
    {
        totalWidth += logoLetters_[i].baseWidth * logoScale_;
        if (i + 1 < logoLetters_.size())
        {
            totalWidth += (logoCharSpacing_ + logoLetters_[i].extraSpacing) * logoScale_;
        }
    }

    float cursor = -totalWidth * 0.5f;
    for (size_t i = 0; i < logoLetters_.size(); ++i)
    {
        LogoLetter& letter = logoLetters_[i];
        const float width = letter.baseWidth * logoScale_;
        letter.offsetX = cursor + width * 0.5f;
        cursor += width + (logoCharSpacing_ + letter.extraSpacing) * logoScale_;
    }
}

void ClearScene::LayoutNumbers()
{
    for (int i = 0; i < kRowCount; ++i)
    {
        if (i < static_cast<int>(numberRows_.size()))
        {
            numberRows_[static_cast<size_t>(i)].rightOffset = kRowLayouts[i].valueRightOffset;
        }
        if (i < static_cast<int>(labels_.size()))
        {
            labels_[static_cast<size_t>(i)].offset = kRowLayouts[i].labelOffset;
            labels_[static_cast<size_t>(i)].size = kRowLayouts[i].labelSize;
        }
    }

    resultGroupOrigin_ = kResultGroupOriginDefault;
    promptOffset_ = kPromptOffsetDefault;
}

void ClearScene::RebuildTimeline()
{
    // 落下ディレイを順番から作り直しつつ、ロゴが出そろう時刻を求める
    float logoEnd = 0.0f;
    for (LogoLetter& letter : logoLetters_)
    {
        letter.dropDelay = dropStagger_ * static_cast<float>(letter.dropOrder);
        logoEnd = (std::max)(logoEnd, letter.dropDelay + dropSeconds_);
    }

    const float base = logoEnd + sequenceBeginOffset_;
    const auto slotTime = [&](int slot) {
        return base + sequenceStep_ * static_cast<float>(slot);
    };

    // ラベル → 数値 → ラベル → 数値 … と交互に並べる
    for (size_t i = 0; i < labels_.size(); ++i)
    {
        labels_[i].popDelay = slotTime(static_cast<int>(i) * 2);
    }
    for (size_t i = 0; i < numberRows_.size(); ++i)
    {
        NumberRow& row = numberRows_[i];
        row.popDelay = slotTime(static_cast<int>(i) * 2 + 1);
        row.countDelay = row.popDelay + kCountBeginOffset;
    }

    // 最後の数値が上がりきるのを少し待ってからプロンプトを出す
    promptDelay_ = slotTime(kSequenceSlotPrompt) + promptExtraDelay_;
}

Vector2 ClearScene::GroupToScreen(const Vector2& offset) const
{
    return {resultGroupOrigin_.x + offset.x, resultGroupOrigin_.y + offset.y};
}

void ClearScene::AssignCells(NumberRow& row)
{
    const int count = static_cast<int>(row.digits.size());
    if (count <= 0)
    {
        return;
    }

    int value = static_cast<int>(row.shown);
    if (value < 0)
    {
        value = 0;
    }

    if (row.isTime)
    {
        // MM:SS。桁の並びは固定（分の十/一 → ":" → 秒の十/一）
        int minutes = value / 60;
        int seconds = value % 60;
        if (minutes > 99)
        {
            minutes = 99;
            seconds = 59;
        }
        const int cells[5] = {minutes / 10, minutes % 10, kColonCell, seconds / 10, seconds % 10};
        for (int i = 0; i < count; ++i)
        {
            row.digits[static_cast<size_t>(i)].cell = cells[(std::min)(i, 4)];
        }
        return;
    }

    // 右詰めゼロ埋め（画面のラフに合わせて 00000 表記にする）
    for (int i = count - 1; i >= 0; --i)
    {
        row.digits[static_cast<size_t>(i)].cell = value % 10;
        value /= 10;
    }
}

// ===================================================================
// 更新
// ===================================================================

void ClearScene::Update()
{
    const float deltaTime = kDeltaTime;
    sceneTime_ += deltaTime;

    if (input_)
    {
        input_->Update();
    }

    if (fadeAlpha_ < 1.0f)
    {
        fadeAlpha_ = std::clamp(fadeAlpha_ + deltaTime / kFadeInSeconds, 0.0f, 1.0f);
    }

    // 調整パラメータが変わっても追従できるよう、毎フレーム組み直す（要素数が少ないので安い）
    RebuildTimeline();

    UpdateLogo(deltaTime);
    UpdateLabels(deltaTime);
    UpdateNumbers(deltaTime);
    UpdatePrompt(deltaTime);
    UpdateFx(deltaTime);

    // SPACE の扱い。
    // 演出の途中なら早送り（sceneTime_ を飛ばすだけで全部のイージングが終端に来る）、
    // 出そろっていればタイトルへ戻る。
    // 「押したら即タイトル」にしたければ、この if を消して ChangeScene だけ残せばいい
    if (input_ && input_->TriggerKey(DIK_SPACE) &&
        !SceneManager::GetInstance()->IsTransitioning())
    {
        if (sceneTime_ < promptDelay_)
        {
            sceneTime_ = promptDelay_;
            fadeAlpha_ = 1.0f;
        }
        else
        {
            SceneManager::GetInstance()->ChangeScene("TITLE");
        }
    }

#ifdef USE_IMGUI
    DrawDebugUI();
#endif
}

void ClearScene::UpdateLogo(float deltaTime)
{
    // 調整パラメータが変わっても追従できるよう毎フレーム並べ直す（十数文字なので安い）
    LayoutLogo();

    const float wobbleRadian = ToRadian(idleWobbleDegrees_);
    const float baselineY = logoCenter_.y + logoCharHeight_ * logoScale_ * 0.5f;

    int landedCount = 0;

    for (LogoLetter& letter : logoLetters_)
    {
        // --- 落下 ---
        // だんだん速くなるイージングで、上から基準位置まで一気に落とす
        const float dropRaw =
            std::clamp((sceneTime_ - letter.dropDelay) / (std::max)(0.01f, dropSeconds_), 0.0f,
                       1.0f);
        const float fall = EaseInQuad(dropRaw);
        const float dropOffsetY = -dropHeight_ * (1.0f - fall);

        if (dropRaw >= 1.0f)
        {
            if (!letter.isLanded)
            {
                letter.isLanded = true;
                letter.landTimer = 0.0f;
            }
            else
            {
                letter.landTimer += deltaTime;
            }
            ++landedCount;
        }

        // --- 着地のつぶれ ---
        // 減衰する振動。落ちた瞬間が一番つぶれていて、そこから跳ね返って収まる
        float squash = 0.0f;
        if (letter.isLanded)
        {
            squash = landSquash_ * std::exp(-landDamping_ * letter.landTimer) *
                     std::cos(landFrequency_ * letter.landTimer);
        }

        // --- 着地後の常時ゆらぎ（タイトルロゴと同じ味付け） ---
        // つぶれが収まってから効き始めるようにブレンドする
        const float idleRate =
            letter.isLanded ? std::clamp(letter.landTimer / 0.5f, 0.0f, 1.0f) : 0.0f;
        const float jiggleTime = sceneTime_ * idleJiggleSpeed_ * letter.speedScale + letter.phase;
        const float jiggle = std::sin(jiggleTime) * idleJiggleAmount_ * idleRate;
        const float bobY =
            std::sin(sceneTime_ * idleBobSpeed_ + letter.phase) * idleBobAmplitude_ * idleRate;
        const float rotation = std::sin(jiggleTime * 0.63f + 1.1f) * wobbleRadian * idleRate;

        // つぶれ（縦）と伸び（横）は体積保存っぽく逆向きに掛ける
        const float scaleX = 1.0f + squash * 0.8f + jiggle;
        const float scaleY = 1.0f - squash - jiggle * 0.85f;

        const Vector2 position = {logoCenter_.x + letter.offsetX,
                                  baselineY + dropOffsetY + bobY};
        const Vector2 size = {letter.baseWidth * logoScale_ * scaleX,
                              logoCharHeight_ * logoScale_ * scaleY};

        // 落ち始めるまでは透明（画面の外にいるので見えないが、念のため）
        const float alpha = (dropRaw > 0.0f) ? fadeAlpha_ : 0.0f;
        ApplySprite(letter.sprite.get(), position, size, alpha, rotation);
    }

    // 全文字が着地したら、お祝いに1発大きめのを上げる
    if (!isLogoBurstDone_ && !logoLetters_.empty() &&
        landedCount == static_cast<int>(logoLetters_.size()))
    {
        isLogoBurstDone_ = true;
        if (showFx_)
        {
            LaunchRocketAt(ScreenToWorld({logoCenter_.x, 210.0f}, -cameraPos_.z), 1.6f);
        }
    }
}

void ClearScene::UpdateLabels(float /*deltaTime*/)
{
    for (Label& label : labels_)
    {
        // タイトルのボタンと同じ、行き過ぎて戻る出方
        const float popRaw = std::clamp((sceneTime_ - label.popDelay) / kPopSeconds, 0.0f, 1.0f);
        const float popScale = (popRaw <= 0.0f) ? 0.0f : EaseOutBack(popRaw);

        const Vector2 size = {label.size.x * popScale, label.size.y * popScale};
        ApplySprite(label.sprite.get(), GroupToScreen(label.offset), size, fadeAlpha_ * popRaw);
    }
}

void ClearScene::UpdateNumbers(float deltaTime)
{
    for (size_t rowIndex = 0; rowIndex < numberRows_.size(); ++rowIndex)
    {
        NumberRow& row = numberRows_[rowIndex];

        // グループ原点を足した、実際の画面座標
        const Vector2 rowCenter = GroupToScreen(row.rightOffset);

        // --- 登場 ---
        const float popRaw = std::clamp((sceneTime_ - row.popDelay) / kPopSeconds, 0.0f, 1.0f);
        const float popScale = (popRaw <= 0.0f) ? 0.0f : EaseOutBack(popRaw);

        // --- カウントアップ ---
        // 最後にゆっくり止まるイージングにすると、桁の回りが落ち着いていくのが見える
        const float countRaw =
            std::clamp((sceneTime_ - row.countDelay) / (std::max)(0.01f, row.countSeconds), 0.0f,
                       1.0f);
        row.shown = static_cast<float>(row.target) * EaseOutCubic(countRaw);

        if (!row.isCountFinished && countRaw >= 1.0f)
        {
            row.isCountFinished = true;
            row.shown = static_cast<float>(row.target); // 端数で1つ下の値に落ちないよう固定
            row.finishPunch = 1.0f;

            // 上がりきった行の後ろで1発上げる
            if (showFx_)
            {
                LaunchRocketAt(ScreenToWorld(rowCenter, -cameraPos_.z),
                               RandomRange(fireworkPowerMin_, fireworkPowerMax_));
            }
        }

        row.finishPunch = Approach(row.finishPunch, 0.0f, digitPunchDamping_ * 0.7f, deltaTime);

        AssignCells(row);

        // --- 桁を右から並べる ---
        // ":" だけ横幅が狭いので、右端から幅を積み上げて中心を決める
        float cursorRight = rowCenter.x + digitSize_.x * 0.5f;

        for (int i = static_cast<int>(row.digits.size()) - 1; i >= 0; --i)
        {
            Digit& digit = row.digits[static_cast<size_t>(i)];

            const float width =
                (digit.cell == kColonCell) ? digitSize_.x * kColonWidthScale : digitSize_.x;
            const float centerX = cursorRight - width * 0.5f;
            cursorRight -= width + digitSpacing_;

            // 桁が変わった瞬間に弾ませる
            if (digit.cell != digit.shownCell)
            {
                if (digit.shownCell >= 0)
                {
                    digit.punch = 1.0f;
                }
                digit.shownCell = digit.cell;

                if (digit.sprite)
                {
                    digit.sprite->SetTextureLeftTop(CellLeftTop(digit.cell));
                }
            }
            digit.punch = Approach(digit.punch, 0.0f, digitPunchDamping_, deltaTime);

            // 縦に伸びて横が縮む＝跳ねた感じ。行が上がりきった瞬間は全桁まとめて弾む
            const float punch = digit.punch + row.finishPunch;
            const float scaleY = 1.0f + digitPunchAmount_ * punch;
            const float scaleX = 1.0f - digitPunchAmount_ * punch * 0.55f;

            const Vector2 size = {width * scaleX * popScale, digitSize_.y * scaleY * popScale};
            ApplySprite(digit.sprite.get(), {centerX, rowCenter.y}, size, fadeAlpha_ * popRaw);
        }
    }
}

void ClearScene::UpdatePrompt(float /*deltaTime*/)
{
    if (!promptSprite_)
    {
        return;
    }

    const Vector2 center = GroupToScreen(promptOffset_);

    if (sceneTime_ < promptDelay_)
    {
        ApplySprite(promptSprite_.get(), center, promptSize_, 0.0f);
        return;
    }

    // α を min..max で往復させる。min を 0 にしなければ、消えている間も文字が読める
    const float blink = (std::sin((sceneTime_ - promptDelay_) * promptBlinkSpeed_) * 0.5f) + 0.5f;
    const float alpha = promptAlphaMin_ + (promptAlphaMax_ - promptAlphaMin_) * blink;

    ApplySprite(promptSprite_.get(), center, promptSize_, fadeAlpha_ * alpha);
}

// ===================================================================
// 背景の花火
//
// 「シューーー」= 上昇中のロケットが軌跡の粒を置き続ける
// 「パン！」    = 頂点で SlimeFx::EmitFirework（閃光 + 2重の殻 + 尾を引く火花）
//
// 粒の色は SlimeFx に登録したカラー場から毎フレーム引き直される。
// 場は位置にも依存するので、同じ1発の中でも殻が開くにつれて色が散っていく。
// ===================================================================

void ClearScene::UpdateFx(float deltaTime)
{
    if (!fx_)
    {
        return;
    }

    // カメラは ImGui でいじれるので毎フレーム反映する。
    // Update() を呼ばないと GPU 側の定数バッファが確保されず、
    // ルートパラメータに張るカメラ CBV のアドレスが 0 のままになる
    if (fxCamera_)
    {
        fxCamera_->SetTranslate(cameraPos_);
        fxCamera_->SetRotate({0.0f, 0.0f, 0.0f});
        fxCamera_->Update();
    }

    fx_->SetAdditive(fxAdditive_);
    fx_->SetCameraPitch(0.0f); // 正面固定なので板は常に正対する

    if (!showFx_)
    {
        // 出ている粒は消えるまで面倒を見る
        fx_->Update(deltaTime);
        return;
    }

    // --- 打ち上げ ---
    launchTimer_ -= deltaTime;
    if (launchTimer_ <= 0.0f)
    {
        LaunchRocket();
        launchTimer_ = RandomRange(launchIntervalMin_, launchIntervalMax_);
    }

    // --- 上昇中のロケット ---
    for (Rocket& rocket : rockets_)
    {
        if (!rocket.isActive)
        {
            continue;
        }

        rocket.velocity.y -= rocketGravity_ * deltaTime;
        rocket.position = rocket.position + rocket.velocity * deltaTime;
        rocket.fuse -= deltaTime;

        // 「シュー」の尾。細かく置いて線に見せる
        rocket.trailAccum += deltaTime;
        while (rocket.trailAccum >= kRocketTrailInterval)
        {
            rocket.trailAccum -= kRocketTrailInterval;

            SlimeFxDesc desc;
            desc.position = rocket.position;
            desc.position.x += RandomRange(-0.05f, 0.05f);
            desc.position.z += RandomRange(-0.05f, 0.05f);
            desc.velocity = {RandomRange(-0.5f, 0.5f), RandomRange(-1.6f, -0.2f),
                             RandomRange(-0.5f, 0.5f)};
            desc.gravity = 2.0f;
            desc.drag = 3.0f;
            desc.colorBegin = kRocketTrailColor;
            desc.colorEnd = {kRocketTrailColor.x, kRocketTrailColor.y, kRocketTrailColor.z, 0.0f};
            desc.scaleBegin = 0.16f * rocket.power;
            desc.scaleEnd = 0.02f;
            desc.lifeTime = RandomRange(0.22f, 0.40f);
            desc.useSparkTexture = true;
            desc.useColorField = fxUseColorField_;
            fx_->Emit(desc);
        }

        // 頂点に着いたら炸裂
        if (rocket.fuse <= 0.0f)
        {
            fx_->EmitFirework(randomEngine_, rocket.position, kFireworkCoreColor,
                              kFireworkShellColor, rocket.power, fxUseColorField_);
            rocket.isActive = false;
        }
    }

    // --- 環境の粒。ゆっくり昇る蛍みたいなの ---
    if (fxEnableAmbient_)
    {
        ambientAccum_ += deltaTime;
        while (ambientAccum_ >= kAmbientInterval)
        {
            ambientAccum_ -= kAmbientInterval;

            SlimeFxDesc desc;
            desc.position = {RandomRange(-kBurstXRange, kBurstXRange), RandomRange(-5.5f, -3.0f),
                             RandomRange(kBurstZMin, kBurstZMax)};
            desc.velocity = {RandomRange(-0.15f, 0.15f), RandomRange(0.5f, 1.1f), 0.0f};
            desc.drag = 0.15f;
            desc.colorBegin = kAmbientColor;
            desc.colorEnd = {kAmbientColor.x, kAmbientColor.y, kAmbientColor.z, 0.0f};
            desc.scaleBegin = RandomRange(0.10f, 0.22f);
            desc.scaleEnd = 0.02f;
            desc.lifeTime = kAmbientLifeTime;
            desc.useSparkTexture = false;
            desc.useColorField = fxUseColorField_;
            fx_->Emit(desc);
        }
    }

    fx_->Update(deltaTime);
}

void ClearScene::LaunchRocket()
{
    const Vector3 burstPoint = {RandomRange(-kBurstXRange, kBurstXRange),
                                RandomRange(kBurstYMin, kBurstYMax),
                                RandomRange(kBurstZMin, kBurstZMax)};

    LaunchRocketAt(burstPoint, RandomRange(fireworkPowerMin_, fireworkPowerMax_));
}

void ClearScene::LaunchRocketAt(const Vector3& burstPoint, float power)
{
    if (!fx_ || rockets_.empty())
    {
        return;
    }

    Rocket* target = nullptr;
    for (Rocket& rocket : rockets_)
    {
        if (!rocket.isActive)
        {
            target = &rocket;
            break;
        }
    }
    if (!target)
    {
        return; // 上がりきっていない玉が詰まっているときは見送る
    }

    // 打ち上げ地点から burstPoint の高さまで、ちょうど上がりきる初速を逆算する。
    // v = sqrt(2 * g * h) で頂点に着き、そこまでの時間は t = v / g
    const float rise = (std::max)(0.5f, burstPoint.y - kLaunchY);
    const float gravity = (std::max)(0.5f, rocketGravity_);
    const float upSpeed = std::sqrt(2.0f * gravity * rise);
    const float flightTime = upSpeed / gravity;

    target->isActive = true;
    target->position = {burstPoint.x, kLaunchY, burstPoint.z};
    // 上がりながら少し流れると打ち上げっぽくなる。横のズレは着弾点から引いておく
    const float driftX = RandomRange(-0.8f, 0.8f);
    target->position.x -= driftX * flightTime * 0.5f;
    target->velocity = {driftX, upSpeed, 0.0f};
    target->fuse = flightTime;
    target->trailAccum = 0.0f;
    target->power = power;
}

Vector4 ClearScene::EvaluateColorField(float time, const Vector3& position) const
{
    // 位置と時間から1本のスカラー場 u を作って、それをコサインパレットに通す。
    //   u = (方向ベクトルとの内積) * スケール + 時間 + ゆがみ
    // 位置が効いているので、同じ1発の中でも殻が開くほど粒ごとに色がずれていく
    float u = (position.x * fieldDirection_.x + position.y * fieldDirection_.y +
               position.z * fieldDirection_.z) *
              fieldScale_;
    u += time * fieldTimeScale_;
    // まっすぐな縞になると人工的なので、ゆっくりうねらせる
    u += std::sin(position.y * 0.45f + time * 0.6f) * fieldSwirl_;

    // Inigo Quilez のコサインパレット: color = 0.5 + 0.5 * cos(2π(u + offset))
    // オフセットを 1/3 ずつずらすと RGB が滑らかな虹になる
    const auto channel = [u](float offset) {
        return 0.5f + 0.5f * std::cos(kTwoPi * (u + offset));
    };

    float red = channel(0.0f);
    float green = channel(1.0f / 3.0f);
    float blue = channel(2.0f / 3.0f);

    // 彩度。1 で虹、0 でモノクロ
    const float luma = (red + green + blue) / 3.0f;
    red = luma + (red - luma) * fieldSaturation_;
    green = luma + (green - luma) * fieldSaturation_;
    blue = luma + (blue - luma) * fieldSaturation_;

    // 2つ目の場（金 → 赤 → 紫）を重ねる。
    // 加算ではなく線形補間で混ぜているので、混ぜても明るさが暴れない
    if (emberBlend_ > 0.0f)
    {
        const Vector3 ember = EvaluateEmberPalette(time, position);
        red = red + (ember.x - red) * emberBlend_;
        green = green + (ember.y - green) * emberBlend_;
        blue = blue + (ember.z - blue) * emberBlend_;
    }

    // 加算合成では色が「明るさ」として足されるので、上限を切っておく
    red = std::clamp(red * fieldGain_, 0.0f, 1.0f);
    green = std::clamp(green * fieldGain_, 0.0f, 1.0f);
    blue = std::clamp(blue * fieldGain_, 0.0f, 1.0f);

    return {red, green, blue, 1.0f};
}

Vector3 ClearScene::EvaluateEmberPalette(float time, const Vector3& position) const
{
    // 1つ目の場と相関しないよう、向きも速さも swirl の軸も変えてある
    float v = (position.x * emberDirection_.x + position.y * emberDirection_.y +
               position.z * emberDirection_.z) *
              emberScale_;
    v += time * emberTimeScale_;
    v += std::sin(position.x * 0.38f - time * 0.45f) * emberSwirl_;

    // 0..1 に畳んでから 金 → 赤 → 紫 → 金 の巡回グラデにする。
    // 紫から金へ戻るので、どこにも継ぎ目が出ない
    const float phase = v - std::floor(v);
    const float scaled = phase * 3.0f;
    const int index = (std::min)(2, static_cast<int>(scaled));
    float t = scaled - static_cast<float>(index);
    t = t * t * (3.0f - 2.0f * t); // smoothstep。境目のにじみを自然にする

    const Vector3& from = kEmberStops[index];
    const Vector3& to = kEmberStops[(index + 1) % 3];

    return {from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t,
            from.z + (to.z - from.z) * t};
}

Vector3 ClearScene::ScreenToWorld(const Vector2& screenPosition, float depth) const
{
    // カメラの yaw / pitch / roll が 0 前提の逆算。
    // ndc → カメラ前方 depth の平面上のオフセット
    const float safeDepth = (std::max)(0.1f, depth);
    const float ndcX = (screenPosition.x - kScreenWidth * 0.5f) / (kScreenWidth * 0.5f);
    const float ndcY = (kScreenHeight * 0.5f - screenPosition.y) / (kScreenHeight * 0.5f);

    const float halfHeight = std::tan(kCameraFovY * 0.5f) * safeDepth;
    const float halfWidth = halfHeight * (kScreenWidth / kScreenHeight);

    return {cameraPos_.x + ndcX * halfWidth, cameraPos_.y + ndcY * halfHeight,
            cameraPos_.z + safeDepth};
}

// ===================================================================
// 描画
// ===================================================================

void ClearScene::Draw(SceneRenderRequests& renderRequests)
{
    if (!dxCommon_ || !dxCommon_->GetCommandList())
    {
        return;
    }

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList().Get();

    // 背景スカイボックスの描画
    SceneManager::GetInstance()->DrawSkybox(commandList);

    // --- 花火（3D） ---
    // UI より先に描いて背面に置く。ここはオフスクリーンパスの中なのでポストプロセスが乗る
    if (fx_ && fxCamera_)
    {
        // これを立てるとエンジン側のデバッグ用 plane が出なくなる
        renderRequests.sceneDrawn = true;

        RenderContext ctx;
        ctx.commandList = commandList;
        ctx.camera = fxCamera_;
        ctx.light = GetLight();

        fx_->Draw(ctx);
    }

    // --- UI ---
    // Sprite の PSO はデプス無効なので、あとから描いたものが必ず手前に来る
    for (const LogoLetter& letter : logoLetters_)
    {
        if (letter.sprite)
        {
            letter.sprite->Draw(commandList);
        }
    }

    for (const Label& label : labels_)
    {
        if (label.sprite)
        {
            label.sprite->Draw(commandList);
        }
    }

    for (const NumberRow& row : numberRows_)
    {
        for (const Digit& digit : row.digits)
        {
            if (digit.sprite)
            {
                digit.sprite->Draw(commandList);
            }
        }
    }

    if (promptSprite_)
    {
        promptSprite_->Draw(commandList);
    }
}

// ===================================================================
// デバッグUI（実行中に見た目を詰めるためのもの）
// ===================================================================

#ifdef USE_IMGUI
void ClearScene::DrawDebugUI()
{
    ImGui::Begin("Clear Scene");

    if (ImGui::CollapsingHeader("Result", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::TextWrapped("SceneContext のキー: \"%s\" / \"%s\" / \"%s\"", kResultScoreKey,
                           kResultTimeKey, kResultCoinKey);
        ImGui::TextWrapped("GamePlayScene 側がまだ書いていないので、既定値で動いています。");

        if (numberRows_.size() >= static_cast<size_t>(kRowCount))
        {
            ImGui::DragInt("Score", &numberRows_[kRowScore].target, 10.0f, 0, 99999);

            int timeSeconds = numberRows_[kRowTime].target;
            if (ImGui::DragInt("Time (sec)", &timeSeconds, 1.0f, 0, 5999))
            {
                numberRows_[kRowTime].target = timeSeconds;
            }
            ImGui::SameLine();
            ImGui::Text("= %02d:%02d", timeSeconds / 60, timeSeconds % 60);

            ImGui::DragInt("Coin", &numberRows_[kRowCoin].target, 1.0f, 0, 999);
        }

        if (ImGui::Button("Reload from SceneContext"))
        {
            LoadResultFromSceneContext();
        }
        ImGui::SameLine();
        if (ImGui::Button("Replay Animation"))
        {
            // 演出を最初から見直す。落ちる順番も引き直す
            sceneTime_ = 0.0f;
            fadeAlpha_ = 0.0f;
            isLogoBurstDone_ = false;

            std::vector<int> order(logoLetters_.size());
            std::iota(order.begin(), order.end(), 0);
            std::shuffle(order.begin(), order.end(), randomEngine_);
            for (size_t k = 0; k < order.size(); ++k)
            {
                LogoLetter& letter = logoLetters_[static_cast<size_t>(order[k])];
                letter.dropOrder = static_cast<int>(k);
                letter.isLanded = false;
                letter.landTimer = 0.0f;
            }

            for (NumberRow& row : numberRows_)
            {
                row.shown = 0.0f;
                row.isCountFinished = false;
                row.finishPunch = 0.0f;
                for (Digit& digit : row.digits)
                {
                    digit.punch = 0.0f;
                    digit.shownCell = -1;
                }
            }
        }
    }

    if (ImGui::CollapsingHeader("Result Group", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::TextWrapped("SCORE / TIME / COIN / PRESS SPACE はこの原点にぶら下がっています。"
                           "ここを動かすと4つまとめてずれます。");
        ImGui::DragFloat2("Group Origin", &resultGroupOrigin_.x, 1.0f);
        if (ImGui::Button("Reset Group Origin"))
        {
            resultGroupOrigin_ = kResultGroupOriginDefault;
        }
    }

    if (ImGui::CollapsingHeader("Sequence"))
    {
        ImGui::TextWrapped("ロゴ → SCORE ラベル → SCORE 数値 → TIME ラベル → TIME 数値 "
                           "→ COIN ラベル → COIN 数値 → PRESS SPACE の順に出ます。");
        ImGui::DragFloat("Begin Offset", &sequenceBeginOffset_, 0.01f, 0.0f, 2.0f);
        ImGui::DragFloat("Step", &sequenceStep_, 0.01f, 0.05f, 1.5f);
        ImGui::DragFloat("Prompt Extra Delay", &promptExtraDelay_, 0.01f, 0.0f, 2.0f);

        ImGui::Separator();
        ImGui::Text("Scene Time : %.2f s", sceneTime_);
        static const char* kSlotNames[] = {"SCORE label", "SCORE value", "TIME label",
                                           "TIME value",  "COIN label",  "COIN value"};
        for (size_t i = 0; i < labels_.size(); ++i)
        {
            ImGui::Text("%-12s %.2f s", kSlotNames[i * 2], labels_[i].popDelay);
            if (i < numberRows_.size())
            {
                ImGui::Text("%-12s %.2f s", kSlotNames[i * 2 + 1], numberRows_[i].popDelay);
            }
        }
        ImGui::Text("%-12s %.2f s", "PRESS SPACE", promptDelay_);
    }

    if (ImGui::CollapsingHeader("Logo"))
    {
        ImGui::SeparatorText("Layout");
        ImGui::DragFloat2("Logo Center", &logoCenter_.x, 1.0f);
        ImGui::DragFloat("Logo Scale", &logoScale_, 0.01f, 0.2f, 3.0f);
        ImGui::DragFloat("Char Height", &logoCharHeight_, 1.0f, 20.0f, 300.0f);
        ImGui::DragFloat("Char Spacing", &logoCharSpacing_, 0.5f, -20.0f, 60.0f);

        ImGui::SeparatorText("Drop");
        ImGui::DragFloat("Drop Seconds", &dropSeconds_, 0.01f, 0.05f, 2.0f);
        ImGui::DragFloat("Drop Stagger", &dropStagger_, 0.01f, 0.0f, 0.6f);
        ImGui::DragFloat("Drop Height", &dropHeight_, 5.0f, 100.0f, 900.0f);
        ImGui::DragFloat("Land Squash", &landSquash_, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Land Damping", &landDamping_, 0.1f, 1.0f, 30.0f);
        ImGui::DragFloat("Land Frequency", &landFrequency_, 0.5f, 4.0f, 60.0f);

        ImGui::SeparatorText("Idle");
        ImGui::DragFloat("Bob Amplitude", &idleBobAmplitude_, 0.2f, 0.0f, 40.0f);
        ImGui::DragFloat("Bob Speed", &idleBobSpeed_, 0.05f, 0.0f, 10.0f);
        ImGui::DragFloat("Jiggle Amount", &idleJiggleAmount_, 0.005f, 0.0f, 0.4f);
        ImGui::DragFloat("Jiggle Speed", &idleJiggleSpeed_, 0.05f, 0.0f, 12.0f);
        ImGui::DragFloat("Wobble Degrees", &idleWobbleDegrees_, 0.1f, 0.0f, 20.0f);
    }

    if (ImGui::CollapsingHeader("Values"))
    {
        ImGui::SeparatorText("Digits");
        ImGui::DragFloat2("Digit Size", &digitSize_.x, 0.5f, 8.0f, 200.0f);
        ImGui::DragFloat("Digit Spacing", &digitSpacing_, 0.5f, -20.0f, 40.0f);
        ImGui::DragFloat("Punch Amount", &digitPunchAmount_, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Punch Damping", &digitPunchDamping_, 0.2f, 1.0f, 40.0f);

        ImGui::SeparatorText("Rows (グループ原点からの相対座標)");
        static const char* kRowNames[kRowCount] = {"SCORE", "TIME", "COIN"};
        for (size_t i = 0; i < numberRows_.size(); ++i)
        {
            ImGui::PushID(static_cast<int>(i));
            ImGui::Text("%s", kRowNames[i]);
            ImGui::DragFloat2("Value Offset", &numberRows_[i].rightOffset.x, 1.0f);
            ImGui::DragFloat("Count Seconds", &numberRows_[i].countSeconds, 0.02f, 0.1f, 6.0f);
            if (i < labels_.size())
            {
                ImGui::DragFloat2("Label Offset", &labels_[i].offset.x, 1.0f);
                ImGui::DragFloat2("Label Size", &labels_[i].size.x, 1.0f, 10.0f, 600.0f);
            }
            ImGui::Separator();
            ImGui::PopID();
        }

        if (ImGui::Button("Reset Row Layout"))
        {
            LayoutNumbers();
        }
    }

    if (ImGui::CollapsingHeader("Prompt"))
    {
        ImGui::DragFloat2("Prompt Offset", &promptOffset_.x, 1.0f);
        ImGui::DragFloat2("Prompt Size", &promptSize_.x, 1.0f, 10.0f, 900.0f);
        ImGui::Text("Appear at %.2f s (Sequence で調整)", promptDelay_);
        ImGui::DragFloat("Blink Speed", &promptBlinkSpeed_, 0.05f, 0.2f, 15.0f);
        ImGui::DragFloatRange2("Alpha Range", &promptAlphaMin_, &promptAlphaMax_, 0.01f, 0.0f,
                               1.0f);
    }

    if (ImGui::CollapsingHeader("Fireworks", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Show", &showFx_);
        ImGui::SameLine();
        ImGui::Checkbox("Additive", &fxAdditive_);
        ImGui::SameLine();
        ImGui::Checkbox("Ambient", &fxEnableAmbient_);

        ImGui::Checkbox("Use Color Field", &fxUseColorField_);
        if (fx_ && !fx_->HasAdditivePipeline())
        {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f),
                               "加算合成 PSO の作成に失敗（アルファブレンドで描画中）");
        }

        if (fx_)
        {
            ImGui::Text("Particles : %d / %u", fx_->GetActiveCount(), fx_->GetCapacity());
        }

        int activeRockets = 0;
        for (const Rocket& rocket : rockets_)
        {
            if (rocket.isActive)
            {
                ++activeRockets;
            }
        }
        ImGui::Text("Rockets   : %d / %d", activeRockets, static_cast<int>(rockets_.size()));

        ImGui::DragFloatRange2("Launch Interval", &launchIntervalMin_, &launchIntervalMax_, 0.02f,
                               0.05f, 6.0f);
        ImGui::DragFloatRange2("Power", &fireworkPowerMin_, &fireworkPowerMax_, 0.02f, 0.2f, 3.0f);
        ImGui::DragFloat("Rocket Gravity", &rocketGravity_, 0.1f, 1.0f, 40.0f);

        if (ImGui::Button("Launch Now"))
        {
            LaunchRocket();
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Particles") && fx_)
        {
            fx_->Clear();
            for (Rocket& rocket : rockets_)
            {
                rocket.isActive = false;
            }
        }
    }

    if (ImGui::CollapsingHeader("Color Field"))
    {
        ImGui::TextWrapped("色は color(time, position) で毎フレーム引き直しています。");
        ImGui::DragFloat3("Direction", &fieldDirection_.x, 0.01f, -2.0f, 2.0f);
        ImGui::DragFloat("Position Scale", &fieldScale_, 0.005f, 0.0f, 1.0f);
        ImGui::DragFloat("Time Scale", &fieldTimeScale_, 0.005f, -1.0f, 1.0f);
        ImGui::DragFloat("Swirl", &fieldSwirl_, 0.01f, 0.0f, 2.0f);
        ImGui::DragFloat("Saturation", &fieldSaturation_, 0.01f, 0.0f, 1.5f);

        ImGui::SeparatorText("Field B (金 -> 赤 -> 紫)");
        ImGui::DragFloat("Blend", &emberBlend_, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat3("B Direction", &emberDirection_.x, 0.01f, -2.0f, 2.0f);
        ImGui::DragFloat("B Position Scale", &emberScale_, 0.005f, 0.0f, 1.0f);
        ImGui::DragFloat("B Time Scale", &emberTimeScale_, 0.005f, -1.0f, 1.0f);
        ImGui::DragFloat("B Swirl", &emberSwirl_, 0.01f, 0.0f, 2.0f);

        ImGui::SeparatorText("Output");
        ImGui::DragFloat("Gain", &fieldGain_, 0.01f, 0.0f, 2.0f);

        // 今この瞬間の場を、画面を横切る5点でサンプルして並べる
        const float now = fx_ ? fx_->GetElapsedTime() : sceneTime_;

        ImGui::Text("Blended (y = 1.5, z = 0)");
        for (int i = 0; i < 5; ++i)
        {
            const float x = -6.0f + 3.0f * static_cast<float>(i);
            const Vector4 color = EvaluateColorField(now, {x, 1.5f, 0.0f});
            ImGui::PushID(i);
            ImGui::ColorButton("##field", ImVec4(color.x, color.y, color.z, 1.0f),
                               ImGuiColorEditFlags_NoTooltip, ImVec2(48.0f, 24.0f));
            ImGui::PopID();
            if (i < 4)
            {
                ImGui::SameLine();
            }
        }

        ImGui::Text("Field B only");
        for (int i = 0; i < 5; ++i)
        {
            const float x = -6.0f + 3.0f * static_cast<float>(i);
            const Vector3 ember = EvaluateEmberPalette(now, {x, 1.5f, 0.0f});
            ImGui::PushID(100 + i);
            ImGui::ColorButton("##ember", ImVec4(ember.x, ember.y, ember.z, 1.0f),
                               ImGuiColorEditFlags_NoTooltip, ImVec2(48.0f, 24.0f));
            ImGui::PopID();
            if (i < 4)
            {
                ImGui::SameLine();
            }
        }
    }

    if (ImGui::CollapsingHeader("Camera"))
    {
        ImGui::TextWrapped("engine のカメラを借りています（正面固定）。"
                           "z を遠ざけるほど花火が小さく、画面に入る範囲が広くなります。");
        ImGui::DragFloat3("Camera Pos", &cameraPos_.x, 0.1f);
    }

    ImGui::Separator();
    if (ImGui::Button("Reset All Tuning"))
    {
        ResetTuningToDefault();
        LayoutNumbers();
    }
    ImGui::SameLine();
    if (ImGui::Button("Back to TITLE"))
    {
        SceneManager::GetInstance()->ChangeScene("TITLE");
    }

    ImGui::End();
}
#endif

// ===================================================================
// スプライトの共通処理
// ===================================================================

void ClearScene::ApplySprite(Sprite* sprite, const Vector2& center, const Vector2& size,
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

std::unique_ptr<Sprite> ClearScene::MakeSprite(const char* texturePath, const Vector2& size,
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
