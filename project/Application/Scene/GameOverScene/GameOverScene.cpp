#include "GameOverScene.h"

#include "Camera.h"
#include "DirectXCom.h"
#include "KeyInput.h"
#include "SceneManager.h"
#include "WindowsAPI.h"

#include "Application/GameObject/FireworkFx.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3dCom.h"
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
// ロゴ（1文字＝1スプライト）
//
// "Game Over..." を9枚に割っている。
//   G / a / m / e /（ここで単語が切れる）/ O / v / e / r / …
// 三点リーダー "…" は1文字扱いなので1枚。
//
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
    {"Resources/UI/GameOver/logo_char_01.png", 54.0f, 0.0f },  // G
    {"Resources/UI/GameOver/logo_char_02.png", 46.0f, 0.0f },  // a
    {"Resources/UI/GameOver/logo_char_03.png", 66.0f, 0.0f },  // m
    {"Resources/UI/GameOver/logo_char_04.png", 46.0f, 34.0f},  // e （ここで単語が切れる）
    {"Resources/UI/GameOver/logo_char_05.png", 58.0f, 0.0f },  // O
    {"Resources/UI/GameOver/logo_char_06.png", 48.0f, 0.0f },  // v
    {"Resources/UI/GameOver/logo_char_07.png", 46.0f, 0.0f },  // e
    {"Resources/UI/GameOver/logo_char_08.png", 40.0f, 0.0f },  // r
    {"Resources/UI/GameOver/logo_char_09.png", 60.0f, 0.0f },  // …（三点リーダー1枚）
};

constexpr int kLogoCharCount = static_cast<int>(std::size(kLogoChars));

// 背景の板。画像を用意しなくてよい。
// engine の LoadTexture() は失敗すると 4x4 の白ダミーを返すので、
// それを SetColor で染めればそのまま「一面の単色スプライト」になる。
// あとから本物のテクスチャを置けば、そのまま背景画像として使える
constexpr const char* kBackgroundTexture = "Resources/UI/GameOver/bg_panel.png";

// プロンプトはクリアシーンの画像をそのまま使い回す（演出も同じ）
constexpr const char* kPromptTexture = "Resources/UI/Clear/press_space.png";

// ===================================================================
// レイアウト初期値（1280x720 基準）
// 実行中は ImGui の "GameOver Scene" ウィンドウから調整できる
// ===================================================================
constexpr float kScreenWidth = 1280.0f;
constexpr float kScreenHeight = 720.0f;
constexpr Vector2 kScreenCenter = {kScreenWidth * 0.5f, kScreenHeight * 0.5f};

// ラフどおり「中央揃え、ロゴは真ん中よりやや上」
constexpr Vector2 kLogoCenterDefault = {kScreenCenter.x, kScreenCenter.y - 30.0f};
constexpr float kLogoScaleDefault = 1.0f;
constexpr float kLogoCharHeightDefault = 94.0f;
constexpr float kLogoCharSpacingDefault = 5.0f;

// プロンプトはロゴの下辺から kPromptGapDefault だけ空けた位置に「上辺」が来る。
// ロゴを動かすと自動で付いてくるので、ImGui でロゴ位置を詰めても間隔が崩れない
constexpr Vector2 kPromptSizeDefault = {520.0f, 62.0f};
constexpr float kPromptGapDefault = 30.0f;
constexpr Vector2 kPromptOffsetDefault = {0.0f, 0.0f}; // 上の自動配置への微調整

// ===================================================================
// 演出パラメータ初期値
// ===================================================================
constexpr float kDeltaTime = 1.0f / 60.0f; // SceneManager が固定タイムステップで回している
constexpr float kFadeInSeconds = 0.5f;

// ロゴの落下。
// クリアシーンと違って【全文字が同時に】落ちる（kDropStaggerDefault = 0）。
// そのぶん「圧倒された感じ」を、傾きのばらつきと重い着地で出す
constexpr float kDropSecondsDefault = 0.46f;
constexpr float kDropStaggerDefault = 0.0f;   // 0 = 全員同時。増やすとバラバラに落ちる
constexpr float kDropHeightDefault = 520.0f;  // 基準位置からどれだけ上から落とすか
constexpr float kDropTiltDegreesDefault = 17.0f; // 落下中の傾きのばらつき（±）
constexpr float kRestTiltDegreesDefault = 3.0f;  // 収まったあとに残す傾きのばらつき（±）
constexpr float kLandSquashDefault = 0.50f;   // 着地の潰れ量（クリアより深め）
constexpr float kLandFrequencyDefault = 19.0f;// 潰れの振動数（rad/秒）

// 【ここが今回の肝】
// クリアシーンは exp(-damping * t) で減衰させていたが、
// こちらは「あらゆる振幅に掛かる energy を、毎フレーム n 倍する」方式にした。
//   energy *= RandomRange(kEnergyDecayMin, kEnergyDecayMax)   （0.9 < n < 1）
// n が毎フレーム乱数なので減り方が不規則になり、
// 「ぷにょぷにょの勢いが不規則に、じわじわ失われていく」ように見える。
//   平均 0.97 → 60フレーム（1秒）で 0.97^60 ≒ 0.16 まで落ちる
constexpr float kEnergyDecayMinDefault = 0.950f;
constexpr float kEnergyDecayMaxDefault = 0.990f;
// 0 にすると完全に静止する。ほんの少し残すと「まだ生きている」感じになる
constexpr float kEnergyResidualDefault = 0.08f;

// 着地後の常時ゆらぎ。振幅はすべて energy が掛かる
constexpr float kIdleBobAmplitudeDefault = 7.0f;
constexpr float kIdleBobSpeedDefault = 2.0f;
constexpr float kIdleJiggleAmountDefault = 0.06f;
constexpr float kIdleJiggleSpeedDefault = 3.2f;
constexpr float kIdleWobbleDegreesDefault = 3.0f;

// プロンプト（クリアシーンと同じ値）
constexpr float kPromptExtraDelayDefault = 0.55f; // ロゴが落ちきってから出るまで
constexpr float kPromptBlinkSpeedDefault = 3.4f;
constexpr float kPromptAlphaMinDefault = 0.25f;
constexpr float kPromptAlphaMaxDefault = 1.0f;

// ===================================================================
// 背景（一面の単色スプライト）
//
// 暗い緑系 ⇔ 暗い青系をゆっくり往復する。α も別の周期で往復させるので、
// 2つの周期が噛み合わずに「微妙に変わり続ける」ように見える
// ===================================================================
constexpr Vector3 kBgColorADefault = {0.045f, 0.130f, 0.085f}; // 暗い緑
constexpr Vector3 kBgColorBDefault = {0.035f, 0.075f, 0.150f}; // 暗い青
constexpr float kBgColorSpeedDefault = 0.17f;  // 色の往復の速さ（rad/秒）
constexpr float kBgAlphaMinDefault = 0.55f;    // まずは振幅を狭くしておく
constexpr float kBgAlphaMaxDefault = 0.65f;
constexpr float kBgAlphaSpeedDefault = 0.29f;  // 色とは違う速さにして周期をずらす

// ===================================================================
// 水滴パーティクル
//
// 「窓に張り付いて流れ落ちる水滴」。
// FireworkFx の粒は等加速度運動しかしないので、
//   gravity（下向き加速度）と drag（速度の減衰）で終端速度 = gravity / drag に落ち着かせ、
//   「一定の速さでツーッと垂れる」動きを作っている。
//     既定 2.2 / 0.9 → 終端 2.44 [単位/秒]（画面の高さが約 9.2 単位なので 4秒弱で縦断）
//
// alignToVelocity + scaleAspect > 1 で、板が進行方向（＝下）に伸びて水滴らしくなる。
// trailInterval を入れると、垂れた跡が薄く残って「ガラスを伝った筋」に見える。
//
// 【合成モードを2本に分けている理由】
// FireworkFx の加算 / アルファは Draw() 単位でしか切り替えられない。
// 「一部アルファ・一部加算」にしたいので、インスタンスを2本用意して振り分けている
// ===================================================================
constexpr uint32_t kDropAlphaCapacity = 1536; // アルファ合成側（本体 + 垂れた跡）
constexpr uint32_t kDropAddCapacity = 768;    // 加算合成側

constexpr float kDropletIntervalDefault = 0.085f; // 1粒あたりの発生間隔
constexpr float kAdditiveRatioDefault = 0.35f;    // 加算合成に回す割合
constexpr float kClingRatioDefault = 0.22f;       // 張り付いたまま垂れない割合

constexpr Vector2 kDropletScaleRangeDefault = {0.55f, 1.35f}; // スケールはデカめで
constexpr Vector2 kDropletLifeRangeDefault = {1.1f, 5.2f};    // 短いものが途中で消える
constexpr Vector2 kDropletAlphaRangeDefault = {0.25f, 0.40f}; // アルファ合成側
constexpr Vector2 kDropletAddAlphaRangeDefault = {0.10f, 0.20f}; // 加算合成側は低め

constexpr float kDropletGravityDefault = 2.2f;
constexpr float kDropletDragDefault = 0.9f;
constexpr float kDropletAspectDefault = 1.7f; // 進行方向（縦）に伸ばす倍率

// 張り付いて動かない水滴。終端速度がほぼ 0 になる組み合わせ
constexpr float kClingGravity = 0.25f;
constexpr float kClingDrag = 4.0f;

// 垂れた跡
constexpr float kDropletTrailInterval = 0.055f;
constexpr float kDropletTrailLifeScale = 0.45f; // 本体の寿命に対する割合
constexpr float kDropletTrailScaleRatio = 0.32f;

// 水滴の色（青系）。カラー場がこの2色のあいだを行き来する
constexpr Vector3 kDropletColorADefault = {0.30f, 0.58f, 0.95f}; // 明るめの青
constexpr Vector3 kDropletColorBDefault = {0.16f, 0.34f, 0.72f}; // 深い青
constexpr float kDropletFieldScaleDefault = 0.06f;
constexpr float kDropletFieldTimeScaleDefault = 0.35f;

// シーンに入った瞬間に画面が空だと寂しいので、あらかじめ撒いておく数
constexpr int kDropletPrewarmCount = 48;

// ===================================================================
// カメラ。正面固定（yaw / pitch / roll = 0）で、z = 0 の平面が画面いっぱいになる位置。
// ClearScene の花火と同じ配置なので、見える範囲は x ±8.14 / y ±4.58
// ===================================================================
constexpr Vector3 kCameraPosDefault = {0.0f, 0.0f, -20.0f};
constexpr float kCameraFovY = 0.45f; // Camera のコンストラクタと同じ値

// 水滴を撒く範囲（ワールド）。画面の外側に少しはみ出させている
constexpr float kDropSpawnXRange = 8.6f;
constexpr float kDropSpawnTopMin = 3.7f; // 流れ落ちる水滴は上のほうから
constexpr float kDropSpawnTopMax = 5.4f;
constexpr float kDropSpawnAnyMin = -4.3f; // 張り付く水滴・初期配置はどこでも
constexpr float kDropSpawnAnyMax = 4.6f;
constexpr float kDropSpawnZMin = -1.0f;
constexpr float kDropSpawnZMax = 1.5f;

constexpr float kPi = 3.14159265358979323846f;

/// <summary>度をラジアンに</summary>
constexpr float ToRadian(float degree) { return degree * (kPi / 180.0f); }

/// <summary>だんだん速くなる（落下に使う）</summary>
float EaseInQuad(float x) { return x * x; }

/// <summary>インデックスから 0..1 の疑似乱数を作る（毎回同じ結果になる）</summary>
float Hash01(uint32_t value)
{
    value = value * 747796405u + 2891336453u;
    uint32_t word = ((value >> ((value >> 28) + 4)) ^ value) * 277803737u;
    word = (word >> 22) ^ word;
    return static_cast<float>(word) / static_cast<float>(0xFFFFFFFFu);
}

} // namespace

// unique_ptr が持つ型（FireworkFx）の完全な定義が要るので、
// コンストラクタとデストラクタはヘッダではなくここで定義する
GameOverScene::GameOverScene() = default;
GameOverScene::~GameOverScene() = default;

// ===================================================================
// 初期化 / 終了
// ===================================================================

void GameOverScene::InitializeScene()
{
    if (dxCommon_)
    {
        input_ = new KeyInput();
        input_->Initialize(dxCommon_->GetWindowAPI());
    }

    randomEngine_.seed(std::random_device{}());

    ResetTuningToDefault();

    CreateBackground();
    CreateLogo();
    CreatePrompt();
    CreateFx();

    sceneTime_ = 0.0f;
    fadeAlpha_ = 0.0f;
    isLogoLandDone_ = false;
    dropletAccum_ = 0.0f;

    RebuildTimeline();

    // 入った瞬間から窓が濡れているように、あらかじめ撒いておく
    for (int i = 0; i < kDropletPrewarmCount; ++i)
    {
        DropletDesc kind;
        kind.isAdditive = RandomRange(0.0f, 1.0f) < additiveRatio_;
        kind.isCling = true; // 初期配置は画面のどこにでも置きたいので張り付き扱いにする
        EmitDroplet(kind);
    }
}

void GameOverScene::Finalize()
{
    if (backgroundSprite_)
    {
        backgroundSprite_->Finalize();
        backgroundSprite_.reset();
    }

    for (LogoLetter& letter : logoLetters_)
    {
        if (letter.sprite)
        {
            letter.sprite->Finalize();
            letter.sprite.reset();
        }
    }
    logoLetters_.clear();

    if (promptSprite_)
    {
        promptSprite_->Finalize();
        promptSprite_.reset();
    }

    dropAlphaFx_.reset();
    dropAddFx_.reset();

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

void GameOverScene::ResetTuningToDefault()
{
    logoCenter_ = kLogoCenterDefault;
    logoScale_ = kLogoScaleDefault;
    logoCharHeight_ = kLogoCharHeightDefault;
    logoCharSpacing_ = kLogoCharSpacingDefault;

    dropSeconds_ = kDropSecondsDefault;
    dropStagger_ = kDropStaggerDefault;
    dropHeight_ = kDropHeightDefault;
    dropTiltDegrees_ = kDropTiltDegreesDefault;
    restTiltDegrees_ = kRestTiltDegreesDefault;
    landSquash_ = kLandSquashDefault;
    landFrequency_ = kLandFrequencyDefault;

    energyDecayMin_ = kEnergyDecayMinDefault;
    energyDecayMax_ = kEnergyDecayMaxDefault;
    energyResidual_ = kEnergyResidualDefault;

    idleBobAmplitude_ = kIdleBobAmplitudeDefault;
    idleBobSpeed_ = kIdleBobSpeedDefault;
    idleJiggleAmount_ = kIdleJiggleAmountDefault;
    idleJiggleSpeed_ = kIdleJiggleSpeedDefault;
    idleWobbleDegrees_ = kIdleWobbleDegreesDefault;

    showBackground_ = true;
    bgColorA_ = kBgColorADefault;
    bgColorB_ = kBgColorBDefault;
    bgColorSpeed_ = kBgColorSpeedDefault;
    bgAlphaMin_ = kBgAlphaMinDefault;
    bgAlphaMax_ = kBgAlphaMaxDefault;
    bgAlphaSpeed_ = kBgAlphaSpeedDefault;

    promptSize_ = kPromptSizeDefault;
    promptGap_ = kPromptGapDefault;
    promptOffset_ = kPromptOffsetDefault;
    promptExtraDelay_ = kPromptExtraDelayDefault;
    promptBlinkSpeed_ = kPromptBlinkSpeedDefault;
    promptAlphaMin_ = kPromptAlphaMinDefault;
    promptAlphaMax_ = kPromptAlphaMaxDefault;

    showDroplets_ = true;
    dropletUseColorField_ = true;
    dropletTrail_ = true;
    dropletInterval_ = kDropletIntervalDefault;
    additiveRatio_ = kAdditiveRatioDefault;
    clingRatio_ = kClingRatioDefault;
    dropletScaleRange_ = kDropletScaleRangeDefault;
    dropletLifeRange_ = kDropletLifeRangeDefault;
    dropletAlphaRange_ = kDropletAlphaRangeDefault;
    dropletAddAlphaRange_ = kDropletAddAlphaRangeDefault;
    dropletGravity_ = kDropletGravityDefault;
    dropletDrag_ = kDropletDragDefault;
    dropletAspect_ = kDropletAspectDefault;

    dropletColorA_ = kDropletColorADefault;
    dropletColorB_ = kDropletColorBDefault;
    dropletFieldScale_ = kDropletFieldScaleDefault;
    dropletFieldTimeScale_ = kDropletFieldTimeScaleDefault;

    cameraPos_ = kCameraPosDefault;
}

float GameOverScene::RandomRange(float minValue, float maxValue)
{
    std::uniform_real_distribution<float> distribution(minValue, maxValue);
    return distribution(randomEngine_);
}

// ===================================================================
// 生成
// ===================================================================

void GameOverScene::CreateBackground()
{
    // アンカーは中心。画面いっぱいに引き伸ばす
    backgroundSprite_ = MakeSprite(kBackgroundTexture, {kScreenWidth, kScreenHeight}, {0.5f, 0.5f});
}

void GameOverScene::CreateLogo()
{
    logoLetters_.clear();
    logoLetters_.reserve(kLogoCharCount);

    for (int i = 0; i < kLogoCharCount; ++i)
    {
        LogoLetter letter;
        letter.baseWidth = kLogoChars[i].width;
        letter.extraSpacing = kLogoChars[i].extraSpacing;

        // 着地後のゆらぎは文字ごとに位相と速度をバラす（毎回同じ結果になる）
        letter.phase = Hash01(static_cast<uint32_t>(i) * 2u + 1u) * (kPi * 2.0f);
        letter.speedScale = 0.8f + Hash01(static_cast<uint32_t>(i) * 2u + 7u) * 0.5f;
        letter.dropOrder = i; // 既定では dropStagger_ = 0 なので順番は効かない
        letter.landTimer = 0.0f;
        letter.energy = 1.0f;
        letter.isLanded = false;

        // アンカーは下端中央。着地の潰れが「接地したまま」になってグミっぽい
        letter.sprite = MakeSprite(kLogoChars[i].texture,
                                   {letter.baseWidth, logoCharHeight_}, {0.5f, 1.0f});

        logoLetters_.push_back(std::move(letter));
    }

    // 落下中の傾きと、収まったあとに残る傾きを引く。
    // 全文字が同時に落ちるので、ばらつきはこの傾きで作る
    for (LogoLetter& letter : logoLetters_)
    {
        letter.dropTilt = ToRadian(RandomRange(-dropTiltDegrees_, dropTiltDegrees_));
        letter.restTilt = ToRadian(RandomRange(-restTiltDegrees_, restTiltDegrees_));
    }

    LayoutLogo();
}

void GameOverScene::CreatePrompt()
{
    promptSprite_ = MakeSprite(kPromptTexture, promptSize_, {0.5f, 0.5f});
}

void GameOverScene::CreateFx()
{
    Object3dCom* object3dCom = GetObject3dCom();
    if (!dxCommon_ || !object3dCom)
    {
        return;
    }

    // engine のカメラを借りる。
    // Game がスカイボックスにもパーティクルにも同じカメラを渡しているので、
    // これを使っておけば視点がずれない。
    // 【バグ修正メモ】ここが nullptr になると Draw() の if を通らず水滴が1粒も出ない。
    // 原因になりうるのは GamePlayScene::Finalize() の SetDefaultCamera(nullptr) で、
    // 現在は GamePlayScene 側が「入る前のカメラ」を控えて戻すようにしてある。
    // SceneManager::GetCamera() は GAMEPLAY を抜けたあと解放済みの playCamera_ を
    // 指しうるので、フォールバックには使わないこと
    fxCamera_ = object3dCom->GetDefaultCamera();
    if (fxCamera_)
    {
        savedCameraTranslate_ = fxCamera_->GetTranslate();
        savedCameraRotate_ = fxCamera_->GetRotate();
    }

    // 合成モードは Draw() 単位でしか切り替えられないので、2本に分ける
    dropAlphaFx_ = std::make_unique<FireworkFx>();
    dropAlphaFx_->Initialize(dxCommon_, fxCamera_, kDropAlphaCapacity);
    dropAlphaFx_->SetAdditive(false); // アルファ合成

    dropAddFx_ = std::make_unique<FireworkFx>();
    dropAddFx_->Initialize(dxCommon_, fxCamera_, kDropAddCapacity);
    dropAddFx_->SetAdditive(true); // 加算合成

    // 粒の色を決めるベクター場。useColorField を立てた粒だけが毎フレーム塗り直される
    const FireworkFx::ColorField field = [this](float time, const Vector3& position) {
        return EvaluateDropletColorField(time, position);
    };
    dropAlphaFx_->SetColorField(field);
    dropAddFx_->SetColorField(field);
}

// ===================================================================
// レイアウト
// ===================================================================

void GameOverScene::LayoutLogo()
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

Vector2 GameOverScene::CalcPromptCenter() const
{
    // ロゴのアンカーは下端中央なので、下辺 = logoCenter_.y + 文字高さの半分
    const float logoBottom = logoCenter_.y + logoCharHeight_ * logoScale_ * 0.5f;

    // ラフどおり、プロンプトの【上辺】がロゴの下辺から promptGap_ だけ下に来るようにする
    const float centerY = logoBottom + promptGap_ + promptSize_.y * 0.5f;

    return {logoCenter_.x + promptOffset_.x, centerY + promptOffset_.y};
}

void GameOverScene::RebuildTimeline()
{
    // 落下ディレイを組み直しつつ、ロゴが出そろう時刻を求める。
    // 既定では dropStagger_ = 0 なので、全員が同時に落ちて同時に着地する
    float logoEnd = 0.0f;
    for (LogoLetter& letter : logoLetters_)
    {
        letter.dropDelay = dropStagger_ * static_cast<float>(letter.dropOrder);
        logoEnd = (std::max)(logoEnd, letter.dropDelay + dropSeconds_);
    }

    promptDelay_ = logoEnd + promptExtraDelay_;
}

void GameOverScene::ReplayAnimation()
{
    sceneTime_ = 0.0f;
    fadeAlpha_ = 0.0f;
    isLogoLandDone_ = false;

    for (LogoLetter& letter : logoLetters_)
    {
        letter.isLanded = false;
        letter.landTimer = 0.0f;
        letter.energy = 1.0f;
        letter.dropTilt = ToRadian(RandomRange(-dropTiltDegrees_, dropTiltDegrees_));
        letter.restTilt = ToRadian(RandomRange(-restTiltDegrees_, restTiltDegrees_));
    }
}

// ===================================================================
// 更新
// ===================================================================

void GameOverScene::Update()
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

    UpdateBackground(deltaTime);
    UpdateLogo(deltaTime);
    UpdatePrompt(deltaTime);
    UpdateFx(deltaTime);

    // SPACE の扱い（クリアシーンと同じ2段構え）。
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

void GameOverScene::UpdateBackground(float /*deltaTime*/)
{
    if (!backgroundSprite_)
    {
        return;
    }

    const Vector4 color = EvaluateBackgroundColor(sceneTime_);

    backgroundSprite_->SetColor({color.x, color.y, color.z,
                                 showBackground_ ? color.w * fadeAlpha_ : 0.0f});
    backgroundSprite_->SetPosition(kScreenCenter);
    backgroundSprite_->SetSize({kScreenWidth, kScreenHeight});
    backgroundSprite_->SetRotation(0.0f);
    backgroundSprite_->Update();
}

Vector4 GameOverScene::EvaluateBackgroundColor(float time) const
{
    // 色と α で速さを変えているので、2つの周期が噛み合わずに
    // 「同じところに戻ってこない」ゆらぎになる
    const float colorPhase = std::sin(time * bgColorSpeed_) * 0.5f + 0.5f;
    const float alphaPhase = std::sin(time * bgAlphaSpeed_ + 1.7f) * 0.5f + 0.5f;

    const Vector3 rgb = {bgColorA_.x + (bgColorB_.x - bgColorA_.x) * colorPhase,
                         bgColorA_.y + (bgColorB_.y - bgColorA_.y) * colorPhase,
                         bgColorA_.z + (bgColorB_.z - bgColorA_.z) * colorPhase};

    const float alpha = bgAlphaMin_ + (bgAlphaMax_ - bgAlphaMin_) * alphaPhase;

    return {rgb.x, rgb.y, rgb.z, alpha};
}

void GameOverScene::UpdateLogo(float deltaTime)
{
    // 調整パラメータが変わっても追従できるよう毎フレーム並べ直す（十数文字なので安い）
    LayoutLogo();

    const float wobbleRadian = ToRadian(idleWobbleDegrees_);
    const float baselineY = logoCenter_.y + logoCharHeight_ * logoScale_ * 0.5f;

    int landedCount = 0;

    for (LogoLetter& letter : logoLetters_)
    {
        // --- 落下 ---
        // だんだん速くなるイージングで、上から基準位置まで一気に落とす。
        // 既定では全文字の dropDelay が 0 なので、9枚が揃って落ちてくる
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
                letter.energy = 1.0f; // 着地の瞬間に満タン。ここから毎フレーム減っていく
            }
            else
            {
                letter.landTimer += deltaTime;

                // 【減衰の本体】
                // exp() で綺麗に減らすのではなく、毎フレーム n（0.9 < n < 1）倍する。
                // n が毎フレーム乱数なので、勢いの落ち方が不規則になって
                // 「じわじわ力尽きていく」ように見える
                letter.energy *= RandomRange(energyDecayMin_, energyDecayMax_);
                letter.energy = (std::max)(letter.energy, energyResidual_);
            }
            ++landedCount;
        }

        const float energy = letter.isLanded ? letter.energy : 0.0f;

        // --- 着地のつぶれ ---
        // 振幅は energy がそのまま掛かる。周波数は一定なので、
        // 「同じテンポで揺れながら、振れ幅だけが小さくなっていく」
        const float squash =
            letter.isLanded
                ? landSquash_ * energy * std::cos(landFrequency_ * letter.landTimer)
                : 0.0f;

        // --- 常時ゆらぎ。これも振幅に energy が掛かる ---
        const float jiggleTime = sceneTime_ * idleJiggleSpeed_ * letter.speedScale + letter.phase;
        const float jiggle = std::sin(jiggleTime) * idleJiggleAmount_ * energy;
        const float bobY = std::sin(sceneTime_ * idleBobSpeed_ + letter.phase) *
                           idleBobAmplitude_ * energy;

        // --- 傾き ---
        // 落下中はランダムな傾きのまま。着地後は、その傾きを起点に
        // energy で減衰しながら振れて、最終的に restTilt へ収まる
        float rotation = letter.dropTilt;
        if (letter.isLanded)
        {
            const float swing = (letter.dropTilt - letter.restTilt) * energy *
                                std::cos(landFrequency_ * 0.55f * letter.landTimer);
            rotation = letter.restTilt + swing +
                       std::sin(jiggleTime * 0.63f + 1.1f) * wobbleRadian * energy;
        }

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

    // --- 基準位置に落ちきった瞬間 ---
    if (!isLogoLandDone_ && !logoLetters_.empty() &&
        landedCount == static_cast<int>(logoLetters_.size()))
    {
        isLogoLandDone_ = true;

        // TODO(SE): ロゴが基準位置に落下した瞬間の音をここで鳴らす。
        //           全文字が同時に着地するので、鳴るのは1回だけ。
        //           「ドスン」系の重い音を想定（クリアシーンは文字ごとに鳴る軽い音）
    }
}

void GameOverScene::UpdatePrompt(float /*deltaTime*/)
{
    if (!promptSprite_)
    {
        return;
    }

    // クリアシーンの "PRESS SPACE TO CONTINUE" をそのまま流用している。
    // α を min..max で往復させる。min を 0 にしなければ、消えている間も文字が読める
    const Vector2 center = CalcPromptCenter();

    if (sceneTime_ < promptDelay_)
    {
        ApplySprite(promptSprite_.get(), center, promptSize_, 0.0f);
        return;
    }

    const float blink = (std::sin((sceneTime_ - promptDelay_) * promptBlinkSpeed_) * 0.5f) + 0.5f;
    const float alpha = promptAlphaMin_ + (promptAlphaMax_ - promptAlphaMin_) * blink;

    ApplySprite(promptSprite_.get(), center, promptSize_, fadeAlpha_ * alpha);
}

// ===================================================================
// 水滴
// ===================================================================

void GameOverScene::UpdateFx(float deltaTime)
{
    if (!dropAlphaFx_ || !dropAddFx_)
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

    dropAlphaFx_->SetTrailEnabled(dropletTrail_);
    dropAddFx_->SetTrailEnabled(dropletTrail_);

    if (showDroplets_)
    {
        dropletAccum_ += deltaTime;
        const float interval = (std::max)(0.005f, dropletInterval_);
        while (dropletAccum_ >= interval)
        {
            dropletAccum_ -= interval;

            DropletDesc kind;
            kind.isAdditive = RandomRange(0.0f, 1.0f) < additiveRatio_;
            kind.isCling = RandomRange(0.0f, 1.0f) < clingRatio_;
            EmitDroplet(kind);
        }
    }

    // showDroplets_ が false でも、出ている粒は消えるまで面倒を見る
    dropAlphaFx_->Update(deltaTime);
    dropAddFx_->Update(deltaTime);
}

void GameOverScene::EmitDroplet(const DropletDesc& kind)
{
    FireworkFx* fx = kind.isAdditive ? dropAddFx_.get() : dropAlphaFx_.get();
    if (!fx)
    {
        return;
    }

    FireworkFxDesc desc;

    // --- 位置 ---
    // 流れ落ちるものは画面の上のほうから、張り付くもの（と初期配置）はどこにでも
    const float spawnYMin = kind.isCling ? kDropSpawnAnyMin : kDropSpawnTopMin;
    const float spawnYMax = kind.isCling ? kDropSpawnAnyMax : kDropSpawnTopMax;

    desc.position = {RandomRange(-kDropSpawnXRange, kDropSpawnXRange),
                     RandomRange(spawnYMin, spawnYMax),
                     RandomRange(kDropSpawnZMin, kDropSpawnZMax)};

    // --- 動き ---
    // 終端速度 = gravity / drag に落ち着くので、「一定の速さでツーッと垂れる」になる。
    // 張り付くほうは drag を大きくして終端速度をほぼ 0 にしている
    desc.velocity = {RandomRange(-0.06f, 0.06f), RandomRange(-0.25f, 0.0f), 0.0f};
    desc.gravity = kind.isCling ? kClingGravity : dropletGravity_;
    desc.drag = kind.isCling ? kClingDrag : dropletDrag_;

    // --- 大きさ ---
    // スケールはデカめで。alignToVelocity + aspect > 1 で進行方向（下）に伸びる
    const float scale = RandomRange(dropletScaleRange_.x, dropletScaleRange_.y);
    desc.scaleBegin = scale;
    desc.scaleEnd = scale * 0.55f;
    // 張り付いた水滴は丸いまま。垂れるものだけ縦に伸ばす
    desc.scaleAspect = kind.isCling ? 1.0f : dropletAspect_;
    desc.alignToVelocity = !kind.isCling;
    desc.useSparkTexture = false; // ぼんやり丸のほうが水滴らしい

    // --- 色と α ---
    // α は colorBegin.w → 0 へ落ちていく（＝減り続ける）。
    // 寿命の幅を広く取ってあるので、短いものは画面の途中で消える
    const Vector2& alphaRange = kind.isAdditive ? dropletAddAlphaRange_ : dropletAlphaRange_;
    const float alpha = RandomRange(alphaRange.x, alphaRange.y);

    const float mix = RandomRange(0.0f, 1.0f);
    const Vector3 rgb = {dropletColorA_.x + (dropletColorB_.x - dropletColorA_.x) * mix,
                         dropletColorA_.y + (dropletColorB_.y - dropletColorA_.y) * mix,
                         dropletColorA_.z + (dropletColorB_.z - dropletColorA_.z) * mix};

    desc.colorBegin = {rgb.x, rgb.y, rgb.z, alpha};
    desc.colorEnd = {rgb.x, rgb.y, rgb.z, 0.0f};
    desc.useColorField = dropletUseColorField_;

    desc.lifeTime = RandomRange(dropletLifeRange_.x, dropletLifeRange_.y);

    // --- 垂れた跡 ---
    // 流れ落ちる水滴だけ、通った跡に薄い粒を置いていく（ガラスを伝った筋）
    if (!kind.isCling && dropletTrail_)
    {
        desc.trailInterval = kDropletTrailInterval;
        desc.trailLifeTime = desc.lifeTime * kDropletTrailLifeScale;
        desc.trailScale = scale * kDropletTrailScaleRatio;
    }

    fx->Emit(desc);
}

Vector4 GameOverScene::EvaluateDropletColorField(float time, const Vector3& position) const
{
    // 位置と時間から 0..1 のスカラーを作って、青系の2色を行き来させる。
    // 位置が効いているので、同じ時刻でも画面の場所によって少し色が違う
    const float u = (position.x + position.y * 0.6f) * dropletFieldScale_ +
                    time * dropletFieldTimeScale_;
    const float t = std::sin(u) * 0.5f + 0.5f;

    return {dropletColorA_.x + (dropletColorB_.x - dropletColorA_.x) * t,
            dropletColorA_.y + (dropletColorB_.y - dropletColorA_.y) * t,
            dropletColorA_.z + (dropletColorB_.z - dropletColorA_.z) * t,
            1.0f}; // w は α への係数。ここでは寿命フェードをそのまま残したいので 1.0
}

// ===================================================================
// 描画
// ===================================================================

void GameOverScene::Draw(SceneRenderRequests& renderRequests)
{
    if (!dxCommon_ || !dxCommon_->GetCommandList())
    {
        return;
    }

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList().Get();

    // これを立てるとエンジン側のデバッグ用 plane が出なくなる。
    // 水滴のカメラが取れていなくても背景と UI は描くので、無条件で立てる
    renderRequests.sceneDrawn = true;

    // 背景スカイボックスの描画
    SceneManager::GetInstance()->DrawSkybox(commandList);

    // --- 一面の単色スプライト ---
    // Sprite の PSO はデプス無効なので、スカイボックスの上に必ず乗る。
    // α を残してあるのでスカイボックスがうっすら透けて見える
    if (backgroundSprite_)
    {
        backgroundSprite_->Draw(commandList);
    }

    // --- 水滴（3D） ---
    // アルファ合成 → 加算合成 の順。加算のほうを後に描くと光の粒が上に乗る。
    // 単色板より後、UI より前なので「窓ガラスに付いた水滴」の層になる
    if (fxCamera_)
    {
        if (dropAlphaFx_)
        {
            dropAlphaFx_->Draw(commandList);
        }
        if (dropAddFx_)
        {
            dropAddFx_->Draw(commandList);
        }
    }

    // --- UI ---
    for (const LogoLetter& letter : logoLetters_)
    {
        if (letter.sprite)
        {
            letter.sprite->Draw(commandList);
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
void GameOverScene::DrawDebugUI()
{
    ImGui::Begin("GameOver Scene");

    ImGui::Text("scene time %.2f / prompt %.2f", sceneTime_, promptDelay_);
    if (ImGui::Button("Replay Animation"))
    {
        ReplayAnimation();
    }
    ImGui::SameLine();
    if (ImGui::Button("Back to TITLE"))
    {
        SceneManager::GetInstance()->ChangeScene("TITLE");
    }

    if (ImGui::CollapsingHeader("Logo", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::SeparatorText("Layout");
        ImGui::DragFloat2("Logo Center", &logoCenter_.x, 1.0f);
        ImGui::DragFloat("Logo Scale", &logoScale_, 0.01f, 0.2f, 3.0f);
        ImGui::DragFloat("Char Height", &logoCharHeight_, 1.0f, 20.0f, 300.0f);
        ImGui::DragFloat("Char Spacing", &logoCharSpacing_, 0.5f, -20.0f, 60.0f);

        ImGui::SeparatorText("Drop");
        ImGui::TextWrapped("Stagger を 0 にしておくと9枚が同時に落ちます。"
                           "ばらつきは Drop Tilt（落下中の傾き）で作っています。");
        ImGui::DragFloat("Drop Seconds", &dropSeconds_, 0.01f, 0.05f, 2.0f);
        ImGui::DragFloat("Drop Stagger", &dropStagger_, 0.005f, 0.0f, 0.5f);
        ImGui::DragFloat("Drop Height", &dropHeight_, 5.0f, 50.0f, 1200.0f);
        if (ImGui::DragFloat("Drop Tilt (deg)", &dropTiltDegrees_, 0.5f, 0.0f, 60.0f))
        {
            for (LogoLetter& letter : logoLetters_)
            {
                letter.dropTilt = ToRadian(RandomRange(-dropTiltDegrees_, dropTiltDegrees_));
            }
        }
        if (ImGui::DragFloat("Rest Tilt (deg)", &restTiltDegrees_, 0.2f, 0.0f, 20.0f))
        {
            for (LogoLetter& letter : logoLetters_)
            {
                letter.restTilt = ToRadian(RandomRange(-restTiltDegrees_, restTiltDegrees_));
            }
        }

        ImGui::SeparatorText("Landing / Energy");
        ImGui::TextWrapped("energy は着地の瞬間に 1.0。毎フレーム下の範囲の乱数を掛けて減らし、"
                           "つぶれ・ゆらぎ・傾きの振幅すべてに掛かります。");
        ImGui::DragFloat("Land Squash", &landSquash_, 0.01f, 0.0f, 1.2f);
        ImGui::DragFloat("Land Frequency", &landFrequency_, 0.2f, 1.0f, 60.0f);
        ImGui::DragFloatRange2("Energy Decay n", &energyDecayMin_, &energyDecayMax_, 0.001f,
                               0.85f, 1.0f, "min %.3f", "max %.3f");
        ImGui::DragFloat("Energy Residual", &energyResidual_, 0.005f, 0.0f, 0.5f);
        if (!logoLetters_.empty())
        {
            ImGui::Text("energy[0] = %.3f", logoLetters_[0].energy);
        }

        ImGui::SeparatorText("Idle Wobble");
        ImGui::DragFloat("Bob Amplitude", &idleBobAmplitude_, 0.1f, 0.0f, 40.0f);
        ImGui::DragFloat("Bob Speed", &idleBobSpeed_, 0.05f, 0.0f, 10.0f);
        ImGui::DragFloat("Jiggle Amount", &idleJiggleAmount_, 0.005f, 0.0f, 0.5f);
        ImGui::DragFloat("Jiggle Speed", &idleJiggleSpeed_, 0.05f, 0.0f, 12.0f);
        ImGui::DragFloat("Wobble (deg)", &idleWobbleDegrees_, 0.1f, 0.0f, 20.0f);
    }

    if (ImGui::CollapsingHeader("Background"))
    {
        ImGui::Checkbox("Show Background", &showBackground_);
        ImGui::ColorEdit3("Color A (green)", &bgColorA_.x);
        ImGui::ColorEdit3("Color B (blue)", &bgColorB_.x);
        ImGui::DragFloat("Color Speed", &bgColorSpeed_, 0.005f, 0.0f, 2.0f);
        ImGui::DragFloatRange2("Alpha", &bgAlphaMin_, &bgAlphaMax_, 0.005f, 0.0f, 1.0f,
                               "min %.2f", "max %.2f");
        ImGui::DragFloat("Alpha Speed", &bgAlphaSpeed_, 0.005f, 0.0f, 2.0f);

        const Vector4 current = EvaluateBackgroundColor(sceneTime_);
        ImGui::ColorButton("##bgnow", ImVec4(current.x, current.y, current.z, 1.0f),
                           ImGuiColorEditFlags_NoTooltip, ImVec2(60, 24));
        ImGui::SameLine();
        ImGui::Text("now  alpha %.2f", current.w);
    }

    if (ImGui::CollapsingHeader("Prompt"))
    {
        ImGui::TextWrapped("ロゴの下辺から Gap だけ空けた位置に、プロンプトの【上辺】が来ます。"
                           "ロゴを動かすと自動で付いてきます。");
        ImGui::DragFloat2("Prompt Size", &promptSize_.x, 1.0f);
        ImGui::DragFloat("Gap", &promptGap_, 1.0f, 0.0f, 300.0f);
        ImGui::DragFloat2("Offset", &promptOffset_.x, 1.0f);
        ImGui::DragFloat("Extra Delay", &promptExtraDelay_, 0.01f, 0.0f, 4.0f);
        ImGui::DragFloat("Blink Speed", &promptBlinkSpeed_, 0.05f, 0.0f, 12.0f);
        ImGui::DragFloatRange2("Blink Alpha", &promptAlphaMin_, &promptAlphaMax_, 0.01f, 0.0f,
                               1.0f, "min %.2f", "max %.2f");

        const Vector2 center = CalcPromptCenter();
        ImGui::Text("center = (%.0f, %.0f)", center.x, center.y);
    }

    if (ImGui::CollapsingHeader("Droplets"))
    {
        ImGui::Checkbox("Show Droplets", &showDroplets_);
        ImGui::SameLine();
        ImGui::Checkbox("Trail", &dropletTrail_);
        ImGui::SameLine();
        ImGui::Checkbox("Color Field", &dropletUseColorField_);

        if (dropAlphaFx_ && dropAddFx_)
        {
            ImGui::Text("alpha %d / %u    additive %d / %u", dropAlphaFx_->GetActiveCount(),
                        dropAlphaFx_->GetCapacity(), dropAddFx_->GetActiveCount(),
                        dropAddFx_->GetCapacity());

            if (!dropAlphaFx_->IsReady() || !dropAddFx_->IsReady())
            {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f),
                                   "FireworkFx の PSO 作成に失敗しています（シェーダを確認）");
            }
            if (ImGui::Button("Clear Droplets"))
            {
                dropAlphaFx_->Clear();
                dropAddFx_->Clear();
            }
        }

        ImGui::SeparatorText("Spawn");
        ImGui::DragFloat("Interval", &dropletInterval_, 0.002f, 0.005f, 0.6f);
        ImGui::SliderFloat("Additive Ratio", &additiveRatio_, 0.0f, 1.0f);
        ImGui::SliderFloat("Cling Ratio", &clingRatio_, 0.0f, 1.0f);

        ImGui::SeparatorText("Look");
        ImGui::DragFloatRange2("Scale", &dropletScaleRange_.x, &dropletScaleRange_.y, 0.01f,
                               0.05f, 4.0f, "min %.2f", "max %.2f");
        ImGui::DragFloatRange2("Life", &dropletLifeRange_.x, &dropletLifeRange_.y, 0.05f, 0.2f,
                               12.0f, "min %.2f", "max %.2f");
        ImGui::DragFloatRange2("Alpha (blend)", &dropletAlphaRange_.x, &dropletAlphaRange_.y,
                               0.005f, 0.0f, 1.0f, "min %.2f", "max %.2f");
        ImGui::DragFloatRange2("Alpha (additive)", &dropletAddAlphaRange_.x,
                               &dropletAddAlphaRange_.y, 0.005f, 0.0f, 1.0f, "min %.2f",
                               "max %.2f");
        ImGui::DragFloat("Aspect (stretch)", &dropletAspect_, 0.02f, 0.2f, 6.0f);

        ImGui::SeparatorText("Motion");
        ImGui::TextWrapped("終端速度 = Gravity / Drag。今は %.2f [単位/秒]（画面の高さは約 9.2）",
                           dropletGravity_ / (std::max)(0.01f, dropletDrag_));
        ImGui::DragFloat("Gravity", &dropletGravity_, 0.05f, 0.0f, 20.0f);
        ImGui::DragFloat("Drag", &dropletDrag_, 0.02f, 0.01f, 8.0f);

        ImGui::SeparatorText("Color");
        ImGui::ColorEdit3("Droplet A", &dropletColorA_.x);
        ImGui::ColorEdit3("Droplet B", &dropletColorB_.x);
        ImGui::DragFloat("Field Scale", &dropletFieldScale_, 0.005f, 0.0f, 1.0f);
        ImGui::DragFloat("Field Time Scale", &dropletFieldTimeScale_, 0.01f, 0.0f, 3.0f);
    }

    if (ImGui::CollapsingHeader("Camera"))
    {
        ImGui::TextWrapped("engine のカメラを借りています（正面固定）。"
                           "z を遠ざけるほど水滴が小さく、画面に入る範囲が広くなります。");
        ImGui::DragFloat3("Camera Pos", &cameraPos_.x, 0.1f);
    }

    ImGui::Separator();
    if (ImGui::Button("Reset All Tuning"))
    {
        ResetTuningToDefault();
    }

    ImGui::End();
}
#endif

// ===================================================================
// スプライトの共通処理（ClearScene と同じ）
// ===================================================================

void GameOverScene::ApplySprite(Sprite* sprite, const Vector2& center, const Vector2& size,
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

std::unique_ptr<Sprite> GameOverScene::MakeSprite(const char* texturePath, const Vector2& size,
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
