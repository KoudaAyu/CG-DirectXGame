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
/// ゲームオーバーシーン
/// UI は仮想解像度 1280x720 を基準に配置している（WindowAPI::kClientWidth/Height）
///
/// 構成:
///   - 背景 … 画面いっぱいの単色スプライト。暗い緑系 〜 青系をゆっくり往復し、
///            α も 0.55〜0.65 のあいだで往復する。奥のスカイボックスが透けて見える
///   - "Game Over..." ロゴ … 1文字＝1スプライト。ClearScene の "STAGE CLEAR!!" を流用したもの。
///            ただしこちらは【全文字が同時に】、【それぞれバラバラの傾きで】落ちてくる。
///            着地後の「ぷにょぷにょ」は、あらゆる振幅に掛かる energy を
///            毎フレーム n（0.9 < n < 1、範囲内の乱数）倍することで徐々に力尽きていく
///   - "PRESS SPACE TO CONTINUE" … ClearScene と同じ画像・同じ点滅演出をそのまま再利用。
///            SPACE でタイトルへ戻る
///   - パーティクル … 窓に張り付いて流れ落ちる水滴。青系・大きめ・α 低め。
///            途中で消えるものが混ざるよう寿命をばらしてある
///
/// 【FireworkFx を2本持っている理由】
/// FireworkFx の合成モード（加算 / アルファ）は Draw() 単位で1つしか選べない。
/// 「一部アルファ合成・一部加算合成」にしたいので、インスタンスを2本用意して
/// 粒を振り分けている。バッチ描画なので2本でもドローコールは合計4回、
/// 頂点バッファも 1024粒あたり 160KB x 3枚程度で済む。
/// </summary>
class GameOverScene : public BaseScene
{
public:
    // FireworkFx を前方宣言のまま unique_ptr で持っているので、
    // コンストラクタとデストラクタは両方 .cpp 側で定義する。
    // （SceneRegistration.cpp の REGISTER_SCENE が make_unique<GameOverScene>() を展開して、
    //   あの TU で暗黙のデフォルトコンストラクタが実体化されるため。詳細は TitleScene.h と同じ）
    GameOverScene();
    ~GameOverScene() override;

    void InitializeScene() override;
    void Finalize() override;
    void Update() override;
    void Draw(SceneRenderRequests& renderRequests) override;

    const char* GetSceneType() const { return "GAMEOVER"; }

private:
    /// <summary>ロゴの1文字分</summary>
    struct LogoLetter
    {
        std::unique_ptr<Sprite> sprite;
        float baseWidth = 0.0f;      // 元画像の基準幅（ピクセル）
        float extraSpacing = 0.0f;   // この文字の後ろに足す隙間（単語の区切り用）
        float offsetX = 0.0f;        // ロゴ中心からの相対X（レイアウト結果）
        int dropOrder = 0;           // 何番目に落ちてくるか（既定は全員 0 ＝同時）
        float dropDelay = 0.0f;      // 落ち始めるまでの時間（dropOrder * dropStagger_）
        float dropTilt = 0.0f;       // 落下中の傾き（rad）。文字ごとにバラバラ
        float restTilt = 0.0f;       // 収まったときの傾き（rad）。少しだけ歪ませておく
        float phase = 0.0f;          // 着地後のゆらぎの位相
        float speedScale = 1.0f;     // ゆらぎ速度の個体差
        float landTimer = 0.0f;      // 着地からの経過秒
        float energy = 1.0f;         // 【本命】あらゆる振幅に掛かる係数。毎フレーム n 倍で減る
        bool isLanded = false;
    };

    /// <summary>水滴1粒の見た目（発生時にサイコロを振って決める）</summary>
    struct DropletDesc
    {
        bool isAdditive = false; // true なら加算合成のインスタンスへ流す
        bool isCling = false;    // true なら「張り付いたまま垂れずに消える」水滴
    };

    // --- 生成 ---
    void CreateBackground();
    void CreateLogo();
    void CreatePrompt();
    void CreateFx();

    // --- 更新 ---
    void UpdateBackground(float deltaTime);
    void UpdateLogo(float deltaTime);
    void UpdatePrompt(float deltaTime);
    void UpdateFx(float deltaTime);

    void LayoutLogo();

    /// <summary>ロゴの下辺 ＋ promptGap_ からプロンプトの中心座標を出す</summary>
    Vector2 CalcPromptCenter() const;

    /// <summary>ロゴが出そろう時刻から promptDelay_ を組み直す</summary>
    void RebuildTimeline();

    /// <summary>演出を頭から流し直す（傾きも引き直す）</summary>
    void ReplayAnimation();

    /// <summary>背景の色。暗い緑系 ⇔ 青系をゆっくり往復する</summary>
    Vector4 EvaluateBackgroundColor(float time) const;

    /// <summary>水滴を1粒撒く</summary>
    void EmitDroplet(const DropletDesc& kind);

    /// <summary>水滴の色を決めるベクター場 color(time, position)。青系のなかで揺らす</summary>
    Vector4 EvaluateDropletColorField(float time, const Vector3& position) const;

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

    std::unique_ptr<Sprite> backgroundSprite_;
    std::vector<LogoLetter> logoLetters_;
    std::unique_ptr<Sprite> promptSprite_;

    std::mt19937 randomEngine_;

    float sceneTime_ = 0.0f;
    float fadeAlpha_ = 0.0f;
    float promptDelay_ = 0.0f;

    bool isLogoLandDone_ = false; // 着地の瞬間の処理（SE など）を済ませたか

    // ===============================================================
    // 水滴
    //
    // engine のカメラを Object3dCom::GetDefaultCamera() で借りて、
    // 正面固定（yaw / pitch / roll = 0）にしてから使う。
    //
    // 【バグ注意】ここが nullptr になると Draw() を通らず水滴が1粒も出ない。
    // 以前 GamePlayScene::Finalize() が SetDefaultCamera(nullptr) していて
    // TitleScene / ClearScene が同じ形で壊れていた（現在は修正済み）。
    // SceneManager::GetCamera() は GAMEPLAY を抜けたあと解放済みを指しうるので
    // フォールバックには使わないこと
    // ===============================================================
    Camera* fxCamera_ = nullptr;
    Vector3 savedCameraTranslate_{};
    Vector3 savedCameraRotate_{};

    // 合成モードは Draw() 単位でしか切り替えられないので、2本に分けている
    std::unique_ptr<FireworkFx> dropAlphaFx_; // アルファ合成のぶん
    std::unique_ptr<FireworkFx> dropAddFx_;   // 加算合成のぶん
    float dropletAccum_ = 0.0f;

    // --- 調整用パラメータ（ImGui からいじれる。既定値は .cpp の定数） ---

    // レイアウト
    Vector2 logoCenter_{};
    float logoScale_ = 1.0f;
    float logoCharHeight_ = 0.0f;
    float logoCharSpacing_ = 0.0f;

    // ロゴの演出
    float dropSeconds_ = 0.0f;       // 落ちきるまでの時間
    float dropStagger_ = 0.0f;       // 文字ごとの落下ディレイ（既定 0 ＝全員同時）
    float dropHeight_ = 0.0f;        // 落下開始位置（基準位置からどれだけ上か）
    float dropTiltDegrees_ = 0.0f;   // 落下中の傾きのばらつき（±）
    float restTiltDegrees_ = 0.0f;   // 収まったときに残す傾きのばらつき（±）
    float landSquash_ = 0.0f;        // 着地のつぶれ量
    float landFrequency_ = 0.0f;     // つぶれの振動数（rad/秒）
    float energyDecayMin_ = 0.0f;    // 毎フレーム掛ける n の下限（0.9 < n < 1）
    float energyDecayMax_ = 0.0f;    // 同 上限
    float energyResidual_ = 0.0f;    // 完全停止させないための下限。0 で完全に力尽きる
    float idleBobAmplitude_ = 0.0f;
    float idleBobSpeed_ = 0.0f;
    float idleJiggleAmount_ = 0.0f;
    float idleJiggleSpeed_ = 0.0f;
    float idleWobbleDegrees_ = 0.0f;

    // 背景
    bool showBackground_ = true;
    Vector3 bgColorA_{}; // 暗い緑系
    Vector3 bgColorB_{}; // 暗い青系
    float bgColorSpeed_ = 0.0f;
    float bgAlphaMin_ = 0.0f;
    float bgAlphaMax_ = 0.0f;
    float bgAlphaSpeed_ = 0.0f;

    // プロンプト
    Vector2 promptSize_{};
    float promptGap_ = 0.0f;      // ロゴの下辺からプロンプトの上辺までの距離
    Vector2 promptOffset_{};      // 上の自動配置に足す微調整
    float promptExtraDelay_ = 0.0f;
    float promptBlinkSpeed_ = 0.0f;
    float promptAlphaMin_ = 0.0f;
    float promptAlphaMax_ = 1.0f;

    // 水滴
    bool showDroplets_ = true;
    bool dropletUseColorField_ = true;
    bool dropletTrail_ = true;
    float dropletInterval_ = 0.0f;  // 1粒あたりの発生間隔（秒）
    float additiveRatio_ = 0.0f;    // 加算合成に回す割合 0..1
    float clingRatio_ = 0.0f;       // 「張り付いたまま垂れない」割合 0..1
    Vector2 dropletScaleRange_{};   // 大きさ（min, max）
    Vector2 dropletLifeRange_{};    // 寿命（min, max）。短いものが「途中で消える」水滴になる
    Vector2 dropletAlphaRange_{};   // アルファ合成側の α（min, max）
    Vector2 dropletAddAlphaRange_{};// 加算合成側の α（min, max）。こちらは低め
    float dropletGravity_ = 0.0f;
    float dropletDrag_ = 0.0f;
    float dropletAspect_ = 0.0f;    // 縦に伸ばす量（小さいほど細長い筋になる）

    // 水滴の色（青系）。カラー場はこの2色のあいだを行き来する
    Vector3 dropletColorA_{};
    Vector3 dropletColorB_{};
    float dropletFieldScale_ = 0.0f;
    float dropletFieldTimeScale_ = 0.0f;

    // カメラ
    Vector3 cameraPos_{};
};
