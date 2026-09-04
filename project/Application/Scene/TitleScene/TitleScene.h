#pragma once

#include "BaseScene.h"
#include "Sprite.h"
#include "Vector.h"

#include <cstdint>
#include <memory>
#include <random>
#include <vector>

class KeyInput;
class MouseInput;
class Camera;
class PikminPlayer;
class MinionManager;
class SlimeFx;
struct SceneRenderRequests;

/// <summary>
/// タイトルシーン
/// UI は仮想解像度 1280x720 を基準に配置している（WindowAPI::kClientWidth/Height）
///
/// タイトルロゴは「1文字＝1スプライト」で構成していて、
/// 文字数・並びは TitleScene.cpp 冒頭の kTitleChars 配列を書き換えるだけで変わる。
/// （配列を1要素にすれば、従来どおり1枚絵のロゴとしても使える）
/// </summary>
class TitleScene : public BaseScene
{
public:
    /// <summary>メニュー項目</summary>
    enum class MenuItem
    {
        Start,
        Manual,
        End,

        Count, // 番兵（項目数）
    };

    // PikminPlayer / MinionManager / SlimeFx を前方宣言のまま unique_ptr で持っているので、
    // コンストラクタとデストラクタは両方 .cpp 側で定義する。
    //
    // デストラクタだけでは足りない: SceneRegistration.cpp の
    // REGISTER_SCENE(TitleScene, "TITLE") が std::make_unique<TitleScene>() を展開するため、
    // あの TU で暗黙のデフォルトコンストラクタが実体化される。
    // コンストラクタは「途中のメンバ構築が例外を投げたら構築済みメンバを破棄する」経路を持つので、
    // そこで ~unique_ptr<SlimeFx> が実体化され、不完全型の static_assert に引っかかる。
    TitleScene();
    ~TitleScene() override;

    void InitializeScene() override;
    void Finalize() override;
    void Update() override;
    void Draw(SceneRenderRequests& renderRequests) override;

    const char* GetSceneType() const { return "TITLE"; }

    // --- 遷移フラグ（中身は後で実装する用の仮置き） ---

    /// <summary>MANUAL が押されたか</summary>
    bool IsManualRequested() const { return isManualRequested_; }
    /// <summary>END が押されたか</summary>
    bool IsExitRequested() const { return isExitRequested_; }
    /// <summary>押された記録をクリアする</summary>
    void ClearRequests()
    {
        isManualRequested_ = false;
        isExitRequested_ = false;
    }

private:
    /// <summary>タイトルロゴの1文字分</summary>
    struct TitleLetter
    {
        std::unique_ptr<Sprite> sprite;
        float baseWidth = 0.0f;   // 元画像の基準幅（ピクセル）
        float offsetX = 0.0f;     // ロゴ中心からの相対X（レイアウト結果）
        float phase = 0.0f;       // ゆらぎの位相（文字ごとにバラす）
        float speedScale = 1.0f;  // ゆらぎ速度の個体差
        float popDelay = 0.0f;    // 登場のディレイ（秒）
    };

    /// <summary>ボタン1個分のデータ</summary>
    struct Button
    {
        std::unique_ptr<Sprite> base;   // 通常時の画像
        std::unique_ptr<Sprite> light;  // 点灯時の画像（base の上に重ねて不透明度で入れ替える）
        Vector2 center{};               // 中心座標（ピクセル）
        Vector2 size{};                 // 基本サイズ（ピクセル）
        float popDelay = 0.0f;          // 登場のディレイ（秒）
        float hoverRate = 0.0f;         // ホバーの進行度 0..1
        float scale = 1.0f;             // 現在の拡大率（表示用）
        float lightRate = 0.0f;         // 点灯画像の不透明度 0..1
        bool isHovered = false;
    };

    void CreateTitleLetters();
    void CreateButtons();
    void LayoutTitleLetters();

    void UpdateTitleLetters(float deltaTime);
    void UpdateButtons(float deltaTime);
    void UpdateFadeIn(float deltaTime);
    void DecideMenu(MenuItem item);

    void ResetTuningToDefault();

    // --- タイトルスライム（3D） ---
    void CreateSlime();
    void UpdateSlime(float deltaTime);
    void ResetSlimeTuningToDefault();

    /// <summary>マウスカーソルとスライムの地面平面との交点をワールド座標で返す</summary>
    /// <param name="outValid">交点が求まったら true</param>
    Vector3 GetMouseGroundPoint(bool& outValid) const;

    // --- 自動デモ ---
    void UpdateDemo(float deltaTime);
    void EnterMerge();
    void EnterSplit();
    void DoThrow();

    // --- パーティクル演出 ---
    void UpdateFx(float deltaTime);

    float RandomRange(float minValue, float maxValue);

#ifdef USE_IMGUI
    void DrawDebugUI();
#endif

    /// <summary>仮想解像度(1280x720)上のマウス座標を取得する</summary>
    Vector2 GetMousePositionOnUI() const;

    /// <summary>矩形の内側に点があるか（中心・サイズ指定）</summary>
    static bool IsInside(const Vector2& point, const Vector2& center, const Vector2& size);

    /// <summary>スプライトに中心座標・サイズ・不透明度・回転をまとめて反映する</summary>
    static void ApplySprite(Sprite* sprite, const Vector2& center, const Vector2& size,
                            float alpha, float rotation = 0.0f);

    /// <summary>スプライトを生成する（失敗しても nullptr が返るだけで落ちない）</summary>
    static std::unique_ptr<Sprite> MakeSprite(const char* texturePath, const Vector2& size,
                                              const Vector2& anchorPoint);

private:
    KeyInput* input_ = nullptr;
    MouseInput* mouse_ = nullptr;

    std::vector<TitleLetter> titleLetters_;
    Button buttons_[static_cast<int>(MenuItem::Count)]{};

    float sceneTime_ = 0.0f; // シーン開始からの経過秒
    float fadeAlpha_ = 0.0f; // UI 全体のフェードイン係数 0..1

    // --- 調整用パラメータ（ImGui からいじれる。既定値は .cpp の定数） ---
    Vector2 logoCenter_{};      // ロゴ全体の中心座標
    float logoScale_ = 1.0f;    // ロゴ全体の拡大率
    float charHeight_ = 0.0f;   // 1文字の基準の高さ
    float charSpacing_ = 0.0f;  // 文字間隔
    float bobAmplitude_ = 0.0f; // 上下ゆれの幅
    float bobSpeed_ = 0.0f;     // 上下ゆれの速さ
    float jiggleAmount_ = 0.0f; // グミ変形の強さ
    float jiggleSpeed_ = 0.0f;  // グミ変形の速さ
    float wobbleDegrees_ = 0.0f;// 傾きゆれの角度（度）

    bool isManualRequested_ = false;
    bool isExitRequested_ = false;

    // ===============================================================
    // タイトルスライム（3D）
    //
    // ゲーム側の PikminPlayer をそのまま流用している。
    // Update() の引数は全部ポインタなので、入力系を nullptr で渡すと
    // 「ステージの傾き（stageTilt）だけで動くスライム」になる。
    // タイトルではその傾きを「目標地点へ向ける P 制御」で自動生成していて、
    // マウスカーソルを追いかけてぷるぷる転がってくる。
    // ===============================================================

    /// <summary>
    /// engine 側のカメラを借りて使う（所有しない）。
    /// Object3dCom::GetDefaultCamera() から取る。これは Game が
    /// ParticleManager と SkyBox にも渡しているのと同じカメラなので、
    /// スライム・スカイボックス・パーティクルの視点が自動的に揃う。
    /// SceneManager::GetCamera() は GAMEPLAY を経由すると解放済みの
    /// playCamera_ を指しているので、そちらは使わない。
    /// </summary>
    Camera* slimeCamera_ = nullptr;
    Vector3 savedCameraTranslate_{}; // シーンを抜けるときに戻すための退避
    Vector3 savedCameraRotate_{};

    std::unique_ptr<PikminPlayer> slime_;
    std::unique_ptr<MinionManager> minions_;
    std::unique_ptr<SlimeFx> fx_;

    std::mt19937 randomEngine_;

    Vector2 slimeTilt_{};            // 現在のステージ傾き（.x = ピッチ→Z加速 / .y = ロール→X加速）
    bool isSlimeIntroPulseDone_ = false; // ロゴ登場に合わせた波紋を撃ったか
    bool wasAnyButtonHovered_ = false;   // ホバーに入った瞬間を取るための前フレーム値

    // --- 調整用パラメータ（ImGui からいじれる） ---
    bool showSlime_ = true;
    Vector3 cameraPos_{};        // タイトルカメラの位置
    float cameraPitch_ = 0.0f;   // タイトルカメラの見下ろし角（ラジアン）
    Vector3 slimeHome_{};        // スライムの定位置（ここを中心にうろつく）
    float slimeRoamRadius_ = 0.0f; // 定位置から離れられる最大距離
    float slimeTiltGain_ = 0.0f;   // 目標地点までの距離 → 傾きの変換係数
    float slimeMaxTilt_ = 0.0f;    // 傾きの上限（ラジアン）
    float slimeFollowRate_ = 0.0f; // マウス追従の強さ 0..1
    bool slimeOverrideColor_ = false; // PikminPlayer が毎フレーム塗る色を上書きするか
    Vector4 slimeColor_{};

    // ===============================================================
    // 自動デモ
    //
    // ゲーム中にプレイヤーがやること（転がる / 投げる / 合体 / 分裂）を
    // 気ままに繰り返す。マウスを動かすとスライムがそっちに寄ってくるので、
    // 「勝手に遊んでいるところに手を出せる」感じになる。
    // ===============================================================

    /// <summary>デモの状態。Merge / Split は瞬間のイベントなので状態は2つだけ</summary>
    enum class DemoState
    {
        Roam,    // 分裂状態。うろつきながらミニオンを投げる
        Rolling, // 合体状態。黄金の巨大スライムでゆったり転がる
    };

    DemoState demoState_ = DemoState::Roam;
    float demoTimer_ = 0.0f;
    float demoDuration_ = 0.0f;
    float throwTimer_ = 0.0f;
    int prevMergedCount_ = 0;
    int strandDelayFrames_ = 0;    // 分裂の数フレーム後に「粘りの糸」を出すためのカウンタ
    float slimeFlashTimer_ = 0.0f; // 分裂の瞬間の白フラッシュ

    // --- 調整用 ---
    bool isDemoEnabled_ = true;
    int minionSpawnCount_ = 0;
    float demoRoamSecondsMin_ = 0.0f;
    float demoRoamSecondsMax_ = 0.0f;
    float demoRollSecondsMin_ = 0.0f;
    float demoRollSecondsMax_ = 0.0f;
    float demoThrowIntervalMin_ = 0.0f;
    float demoThrowIntervalMax_ = 0.0f;

    // ===============================================================
    // パーティクル演出（SlimeFx）
    // ===============================================================

    float fxSparkleAccum_ = 0.0f;
    float fxVortexAccum_ = 0.0f; // マージ中の吸い込み渦
    float fxBackgroundAccum_ = 0.0f;
    float fxTrailDistance_ = 0.0f;
    Vector3 fxPrevSlimePos_{};

    // --- 調整用 ---
    bool showFx_ = true;
    bool fxEnableSparkle_ = true;
    bool fxEnableTrail_ = true;
    bool fxEnableGroundMark_ = true;
    bool fxEnableBulletTrail_ = true;
    bool fxEnableBackground_ = true;
    bool fxAdditive_ = true; // 加算合成（false ならアルファブレンド）
    float fxSparkleInterval_ = 0.0f;
    float fxTrailStep_ = 0.0f;
    float fxBackgroundInterval_ = 0.0f;
};
