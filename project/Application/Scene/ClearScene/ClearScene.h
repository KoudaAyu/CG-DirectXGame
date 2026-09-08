#pragma once

#include "BaseScene.h"
#include "Sprite.h"
#include "Vector.h"

#include <cstdint>
#include <memory>
#include <random>
#include <vector>

class KeyInput;
class Camera;
class FireworkFx;
struct SceneRenderRequests;

/// <summary>
/// クリアシーン
/// UI は仮想解像度 1280x720 を基準に配置している（WindowAPI::kClientWidth/Height）
///
/// 構成:
///   - "STAGE CLEAR!!" ロゴ … 1文字＝1スプライト。ランダムな順で画面上から落ちてきて着地でつぶれる
///   - SCORE / TIME / COIN ラベル … タイトルのボタンと同じく EaseOutBack でぽぽぽっと登場
///   - 数値 … 1枚のアトラス画像から桁ごとに切り出し、0 からカウントアップする
///   - "PRESS SPACE TO CONTINUE" … α を往復させて点滅
///   - 背景 … FireworkFx（バッチ描画）で打ち上げ花火。色は color(time, position) の
///            ベクター場から粒ごとに毎フレーム引き直す。炸裂後も軌跡を撒き続ける
///
/// スコアなどの値は SceneContext（SceneManager の key-value）から読む。
/// キーは kResultScoreKey / kResultTimeKey / kResultCoinKey（.cpp 冒頭）。
/// まだ GamePlayScene 側が値を書いていないので、無ければ既定値で動き、
/// ImGui の "Clear Scene" > "Result" から手で入れて演出を確認できる。
/// </summary>
class ClearScene : public BaseScene
{
public:
    // FireworkFx を前方宣言のまま unique_ptr で持っているので、
    // コンストラクタとデストラクタは両方 .cpp 側で定義する。
    // （SceneRegistration.cpp の REGISTER_SCENE が make_unique<ClearScene>() を展開して、
    //   あの TU で暗黙のデフォルトコンストラクタが実体化されるため。詳細は TitleScene.h と同じ）
    ClearScene();
    ~ClearScene() override;

    void InitializeScene() override;
    void Finalize() override;
    void Update() override;
    void Draw(SceneRenderRequests& renderRequests) override;

    const char* GetSceneType() const { return "CLEAR"; }

private:
    /// <summary>ロゴの1文字分</summary>
    struct LogoLetter
    {
        std::unique_ptr<Sprite> sprite;
        float baseWidth = 0.0f;      // 元画像の基準幅（ピクセル）
        float extraSpacing = 0.0f;   // この文字の後ろに足す隙間（単語の区切り用）
        float offsetX = 0.0f;        // ロゴ中心からの相対X（レイアウト結果）
        int dropOrder = 0;           // 何番目に落ちてくるか（シャッフルで決まる）
        float dropDelay = 0.0f;      // 落ち始めるまでの時間（dropOrder * dropStagger_）
        float phase = 0.0f;          // 着地後のゆらぎの位相
        float speedScale = 1.0f;     // ゆらぎ速度の個体差
        float landTimer = 0.0f;      // 着地からの経過秒（つぶれの減衰に使う）
        bool isLanded = false;
    };

    /// <summary>SCORE / TIME / COIN のラベル1個分</summary>
    struct Label
    {
        std::unique_ptr<Sprite> sprite;
        Vector2 offset{};   // リザルトグループ原点からの相対座標
        Vector2 size{};
        float popDelay = 0.0f;
    };

    /// <summary>数値の1桁分。アトラスのどのセルを出すかだけを持つ</summary>
    struct Digit
    {
        std::unique_ptr<Sprite> sprite;
        int cell = 0;        // 今フレーム出すセル番号（0-9 = 数字 / 10 = ":"）
        int shownCell = -1;  // 前フレームのセル番号（変化を検出して弾ませる）
        float punch = 0.0f;  // 桁が変わった瞬間の弾み。1.0 から減衰する
    };

    /// <summary>数値表示1行（SCORE / TIME / COIN）</summary>
    struct NumberRow
    {
        std::vector<Digit> digits;
        Vector2 rightOffset{};      // 一番右の桁の中心（グループ原点からの相対座標）
        float popDelay = 0.0f;      // 出現ディレイ
        float countDelay = 0.0f;    // カウントアップ開始ディレイ
        float countSeconds = 1.0f;  // 0 から目標値まで上がりきる時間
        int target = 0;             // 目標値
        float shown = 0.0f;         // 現在の表示値
        bool isTime = false;        // true なら MM:SS 表記（":" のセルが入る）
        bool isCountFinished = false;
        float finishPunch = 0.0f;   // 上がりきった瞬間の行全体の弾み
    };

    /// <summary>打ち上げ中の花火（「シュー」の部分）。炸裂した時点で消える</summary>
    struct Rocket
    {
        bool isActive = false;
        Vector3 position{};
        Vector3 velocity{};
        float fuse = 0.0f;       // 残りの上昇時間。0 になったら炸裂
        float trailAccum = 0.0f; // 軌跡の発生間隔カウンタ
        float power = 1.0f;      // 炸裂の大きさ
    };

    // --- 生成 ---
    void CreateLogo();
    void CreateLabels();
    void CreateNumbers();
    void CreatePrompt();
    void CreateFx();

    // --- 更新 ---
    void UpdateLogo(float deltaTime);
    void UpdateLabels(float deltaTime);
    void UpdateNumbers(float deltaTime);
    void UpdatePrompt(float deltaTime);
    void UpdateFx(float deltaTime);

    void LayoutLogo();
    void LayoutNumbers();

    /// <summary>
    /// 登場の順番を組み直す。
    /// ロゴ → SCORE ラベル → SCORE 数値 → TIME ラベル → TIME 数値
    ///      → COIN ラベル → COIN 数値 → PRESS SPACE を等間隔に並べる
    /// </summary>
    void RebuildTimeline();

    /// <summary>リザルトグループ原点を足して、実際の画面座標にする</summary>
    Vector2 GroupToScreen(const Vector2& offset) const;

    /// <summary>SceneContext から結果の値を読む（無ければ既定値のまま）</summary>
    void LoadResultFromSceneContext();

    /// <summary>行の桁数・表示値からアトラスのセル番号を割り当てる</summary>
    static void AssignCells(NumberRow& row);

    /// <summary>花火を1発打ち上げる。position を渡さなければ画面下からランダムに上げる</summary>
    void LaunchRocket();
    void LaunchRocketAt(const Vector3& burstPoint, float power);

    /// <summary>この1発を「柳」にするか抽選する</summary>
    bool RollWillow();

    /// <summary>粒の色を決めるベクター場 color(time, position)。2つの場をブレンドしたもの</summary>
    Vector4 EvaluateColorField(float time, const Vector3& position) const;

    /// <summary>2つ目の場。金 → 赤 → 紫 を巡回するグラデーション</summary>
    Vector3 EvaluateEmberPalette(float time, const Vector3& position) const;

    /// <summary>仮想解像度上の座標を、カメラ前方 depth の平面上のワールド座標へ変換する</summary>
    Vector3 ScreenToWorld(const Vector2& screenPosition, float depth) const;

    void ResetTuningToDefault();

    float RandomRange(float minValue, float maxValue);

#ifdef USE_IMGUI
    void DrawDebugUI();
#endif

    /// <summary>スプライトに中心座標・サイズ・不透明度・回転をまとめて反映する</summary>
    static void ApplySprite(Sprite* sprite, const Vector2& center, const Vector2& size,
                            float alpha, float rotation = 0.0f);

    /// <summary>スプライトを生成する（失敗しても nullptr が返るだけで落ちない）</summary>
    static std::unique_ptr<Sprite> MakeSprite(const char* texturePath, const Vector2& size,
                                              const Vector2& anchorPoint);

private:
    KeyInput* input_ = nullptr;

    std::vector<LogoLetter> logoLetters_;
    std::vector<Label> labels_;
    std::vector<NumberRow> numberRows_;
    std::unique_ptr<Sprite> promptSprite_;

    std::mt19937 randomEngine_;

    float sceneTime_ = 0.0f;
    float fadeAlpha_ = 0.0f;

    bool isLogoBurstDone_ = false; // 全文字が着地した瞬間の花火を撃ったか

    // ===============================================================
    // 背景の花火
    //
    // engine のカメラを Object3dCom::GetDefaultCamera() で借りて、
    // 正面固定（yaw / pitch / roll = 0）にしてから使う。
    //
    // 描画は SlimeFx ではなく FireworkFx（バッチ描画）。
    // SlimeFx は粒1個 = Object3d 1個で、頂点バッファのコミット粒度 64KB と
    // 定数バッファの 1フレーム 2.67MB 枠に当たるため 1000粒あたりが天井。
    // FireworkFx は全粒を1本の頂点バッファに展開してドローコール2回で描くので、
    // 4096粒でも頂点 640KB / 定数バッファ1個で済む。
    // ===============================================================
    Camera* fxCamera_ = nullptr;
    Vector3 savedCameraTranslate_{};
    Vector3 savedCameraRotate_{};

    std::unique_ptr<FireworkFx> fx_;
    std::vector<Rocket> rockets_;
    float launchTimer_ = 0.0f;
    float ambientAccum_ = 0.0f;

    // --- 調整用パラメータ（ImGui からいじれる。既定値は .cpp の定数） ---

    // レイアウト
    Vector2 logoCenter_{};
    float logoScale_ = 1.0f;
    float logoCharHeight_ = 0.0f;
    float logoCharSpacing_ = 0.0f;

    // ロゴの演出
    float dropSeconds_ = 0.0f;     // 1文字が落ちきるまでの時間
    float dropStagger_ = 0.0f;     // 文字ごとの落下ディレイ
    float dropHeight_ = 0.0f;      // 落下開始位置（基準位置からどれだけ上か）
    float landSquash_ = 0.0f;      // 着地のつぶれ量
    float landDamping_ = 0.0f;     // つぶれの減衰速度
    float landFrequency_ = 0.0f;   // つぶれの振動数
    float idleBobAmplitude_ = 0.0f;
    float idleBobSpeed_ = 0.0f;
    float idleJiggleAmount_ = 0.0f;
    float idleJiggleSpeed_ = 0.0f;
    float idleWobbleDegrees_ = 0.0f;

    // 数値
    Vector2 digitSize_{};          // 画面上の1桁のサイズ
    float digitSpacing_ = 0.0f;    // 桁の間隔
    float digitPunchAmount_ = 0.0f;// 桁が変わった瞬間の弾み量
    float digitPunchDamping_ = 0.0f;

    // リザルトのグループ（SCORE / TIME / COIN / PRESS SPACE）。
    // ここを動かすと4つまとめてずれる。個々の相対位置は Label::offset /
    // NumberRow::rightOffset / promptOffset_ が持っている
    Vector2 resultGroupOrigin_{};

    // 登場の順番
    float sequenceBeginOffset_ = 0.0f; // ロゴが出そろってから最初のラベルが出るまで
    float sequenceStep_ = 0.0f;        // 次の要素が出るまでの間隔
    float promptExtraDelay_ = 0.0f;    // 数値が上がりきるのを待つための追加ディレイ

    // 点滅プロンプト
    Vector2 promptOffset_{};
    Vector2 promptSize_{};
    float promptDelay_ = 0.0f;
    float promptBlinkSpeed_ = 0.0f;
    float promptAlphaMin_ = 0.0f;
    float promptAlphaMax_ = 1.0f;

    // 花火
    bool showFx_ = true;
    bool fxAdditive_ = true;
    bool fxUseColorField_ = true;
    bool fxEnableAmbient_ = true;
    bool fxEnableTrail_ = true;   // 炸裂後の軌跡
    float fxTrailDensity_ = 1.0f; // 軌跡の濃さ（1.0 で既定）
    float willowRatio_ = 0.0f;    // 「柳」を引く確率 0..1
    float launchIntervalMin_ = 0.0f;
    float launchIntervalMax_ = 0.0f;
    float fireworkPowerMin_ = 0.0f;
    float fireworkPowerMax_ = 0.0f;
    float rocketGravity_ = 0.0f;

    // カラー場 color(time, position)
    // 1つ目は虹のコサインパレット、2つ目は金→赤→紫。emberBlend_ で混ぜる
    float fieldScale_ = 0.0f;      // 位置の効き
    float fieldTimeScale_ = 0.0f;  // 時間の流れる速さ
    float fieldSwirl_ = 0.0f;      // 縞をゆがませる量
    float fieldSaturation_ = 1.0f;
    float fieldGain_ = 1.0f;
    Vector3 fieldDirection_{};     // 場の勾配方向

    float emberBlend_ = 0.0f;      // 2つ目の場をどれだけ混ぜるか 0..1
    float emberScale_ = 0.0f;
    float emberTimeScale_ = 0.0f;
    float emberSwirl_ = 0.0f;
    Vector3 emberDirection_{};

    // カメラ
    Vector3 cameraPos_{};
};
