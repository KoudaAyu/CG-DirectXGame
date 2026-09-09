#pragma once

#include <memory>
#include <random>

#include <d3d12.h>

#include "Baziru3_Engine/Core/Base/Vector.h"

class DirectXCom;
class Camera;
class FireworkFx;
class Slime;
class SlimeManager;
class CoinManager;
class EnemyManager;

/**
 * @brief ゲームプレイシーンのパーティクル演出をまとめたもの
 *
 * 中身は FireworkFx（バッチ描画のパーティクル）1本。
 * SlimeFx は粒1個 = Object3d 1個なので 1000粒あたりが天井だが、
 * こちらは全粒を1本の動的頂点バッファに展開してドローコール2回で描くため、
 * 数千粒でも頂点 1MB 程度・定数バッファ1個で済む。
 * 詳細は FireworkFx.h の冒頭コメントを参照。
 *
 * 出しているもの:
 *   - 画面全体にゆらゆら舞い上がる光（淡い黄 → 緑 → 水色）
 *   - プレイヤー／ミニオンの体内から染み出す「ぽよぽよ光」
 *   - プレイヤーの移動軌跡（ぽよぽよ光より白い尾）
 *   - プレイヤー足元の、拡大しながら消える同心円
 *   - コインのゲーミング光芒（虹色が流れ続ける。細長い粒）
 *   - 取得されたコインが消えるまで撒き続ける光
 *   - 敵からのちょっと不気味な光（紫 → 赤）
 *   - 敵とプレイヤーの衝突の水しぶき
 *   - 敵の撃破の血しぶき
 *   - プレイヤー分裂のゲーミング光芒（コインより派手）
 *
 * @note 粒数の指定は「同時に生きている数」。
 *       発生レートは (同時数 / 寿命) から自動で決めている。
 * @note コイン・敵は数が読めないので、視点から一定距離のものだけが光る。
 *       距離と同時上限は ImGui から調整できる。
 */
class GamePlaySceneFx
{
public:
    GamePlaySceneFx();
    ~GamePlaySceneFx();

    GamePlaySceneFx(const GamePlaySceneFx&) = delete;
    GamePlaySceneFx& operator=(const GamePlaySceneFx&) = delete;

    void Initialize(DirectXCom* dxCommon, Camera* camera);
    void Finalize();

    void SetCamera(Camera* camera);
    bool IsReady() const;

    /// @brief フレームの先頭で呼ぶ。カリングの中心（＝プレイヤー位置）を渡す
    void BeginFrame(const Vector3& focusCenter);

    // --- 常時出ているもの（毎フレーム呼ぶ）---
    void UpdateAmbient(float deltaTime);
    /// @brief 群れの代表（一番大きい個体）の光と軌跡
    void UpdatePlayer(float deltaTime, Slime* player);

    /// @brief 代表以外の小さいスライムの光。代表は UpdatePlayer() が担当するので除外する
    void UpdateMinions(float deltaTime, SlimeManager* slimeManager);
    void UpdateCoins(float deltaTime, CoinManager* coinManager);
    void UpdateEnemies(float deltaTime, EnemyManager* enemyManager);

    /// @brief FireworkFx 本体の更新。上の Update 群を全部呼んだあと、最後に1回
    void Update(float deltaTime);

    /**
     * @brief 常時出ているものを1フレームぶんまとめて回す
     *
     * BeginFrame() → 各 UpdateXxx() → Update() をこの順で呼ぶだけ。
     * シーン側はこれ1本を呼べばよく、演出の中身はこのクラスに閉じている
     *
     * @param focusCenter カリングの中心（＝群れの代表の位置）
     * @param player 群れの代表（SlimeManager::GetLeader()。nullptr 可）
     */
    void UpdateAll(float deltaTime, const Vector3& focusCenter, Slime* player,
                   SlimeManager* slimeManager, CoinManager* coinManager,
                   EnemyManager* enemyManager);

    void Draw(ID3D12GraphicsCommandList* commandList);
    void DrawImGui();
    void Clear();

    // --- 単発（イベントで呼ぶ）---

    /// @brief 敵とスライムが衝突したときの水しぶき
    /// @param slimeColor ぶつかったスライムの色。敵の紫赤と混ぜた色になる
    void EmitEnemyHitSplash(const Vector3& position, const Vector4& slimeColor);

    /// @brief 敵が倒されたときの血しぶき
    void EmitEnemyDefeat(const Vector3& position, int strength);

    /// @brief プレイヤーが分裂したときのゲーミング光芒
    void EmitPlayerSplit(const Vector3& position, int sizeBefore);

    int GetActiveParticleCount() const;

private:
    /// @brief スライム1体ぶんの「ぽよぽよ光」を出す
    void EmitSlimeGlow(const Vector3& center, float radius, const Vector4& baseColor);

    /// @brief 拡大しながら消える同心円を1枚出す
    void EmitFootRing(const Vector3& center, float radius, const Vector4& color);

    /// @brief コイン1枚ぶんの光芒を出す
    void EmitCoinShine(const Vector3& center, float radius, bool isVanishing);

    /// @brief 敵1体ぶんの不気味な光を出す
    void EmitEnemyAura(const Vector3& center, float radius);

    /// @brief 淡い黄 → 緑 → 水色 のパレットから1色引く
    Vector4 SampleAmbientColor(float t) const;

    /// @brief 紫 → 赤 のパレットから1色引く
    Vector4 SampleEnemyColor(float t) const;

    /// @brief 粒ごとの色を決めるベクター場（ゲーミング＝虹色が流れ続ける）
    Vector4 EvaluateGamingField(float time, const Vector3& position) const;

    float RandomRange(float minValue, float maxValue);

    /// @brief 視点中心からの距離で足切りする
    bool IsInRange(const Vector3& position) const;

private:
    std::unique_ptr<FireworkFx> fx_;
    Camera* camera_ = nullptr;
    std::mt19937 rng_{ std::random_device{}() };

    Vector3 focusCenter_{ 0.0f, 0.0f, 0.0f };

    // --- 発生レートの端数を持ち越すためのアキュムレータ ---
    float ambientAccum_ = 0.0f;
    float playerGlowAccum_ = 0.0f;
    float playerTrailAccum_ = 0.0f;
    float footRingTimer_ = 0.0f;
    float minionGlowAccum_ = 0.0f;
    float coinShineAccum_ = 0.0f;
    float enemyAuraAccum_ = 0.0f;

    Vector3 prevPlayerPos_{ 0.0f, 0.0f, 0.0f };
    bool hasPrevPlayerPos_ = false;

public:
    // ===============================================================
    // 調整パラメータ（ImGui の "Game FX" から全部いじれる）
    // 「粒数」は同時に生きている数。発生レートは (粒数 / 寿命) で決まる
    // ===============================================================

    bool enableAmbient_ = true;
    bool enableSlimeGlow_ = true;
    bool enablePlayerTrail_ = true;
    bool enableFootRing_ = true;
    bool enableCoinShine_ = true;
    bool enableEnemyAura_ = true;
    bool additive_ = true;

    // 舞い上がる光
    int ambientCount_ = 128;          //!< 同時に生きている数
    float ambientLife_ = 4.6f;
    float ambientScaleMin_ = 0.90f;   //!< 舞い上がる光だけ、他の演出の 2/3 の大きさ
    float ambientScaleMax_ = 1.20f;   //!< （元の 0.45〜0.60 を 2 倍にしたもの）
    float ambientAreaRadius_ = 26.0f; //!< 湧く範囲の半径（視点中心から）
    float ambientRiseMin_ = 0.7f;
    float ambientRiseMax_ = 1.7f;
    float ambientSway_ = 0.5f;        //!< 横方向のばらつき（ゆらぎの代わり）
    float ambientStartBelow_ = 3.0f;  //!< 視点中心よりどれだけ下から湧かせるか
    float ambientAlpha_ = 0.55f;

    // ぽよぽよ光（プレイヤー / ミニオン共通）
    int slimeGlowCount_ = 32;         //!< スライム1体あたりの同時数
    float slimeGlowLife_ = 0.85f;
    float slimeGlowScale_ = 0.66f;
    float slimeGlowSpeed_ = 0.9f;
    float slimeGlowColorRange_ = 0.22f; //!< 自分の色を中心にどれだけ振るか
    float slimeGlowAlpha_ = 0.75f;

    // プレイヤーの移動軌跡
    int trailCount_ = 16;
    float trailLife_ = 0.5f;
    float trailScale_ = 0.90f;
    float trailWhiteness_ = 0.55f;    //!< ぽよぽよ光の色から白へ寄せる量
    float trailMinSpeed_ = 1.2f;      //!< この速さ未満では尾を出さない

    // 足元の同心円
    float footRingInterval_ = 0.55f;
    int footRingParticles_ = 22;
    float footRingRadiusScale_ = 1.2f; //!< プレイヤーの見た目半径の何倍から始めるか
    float footRingExpand_ = 2.4f;      //!< 広がる速さ (m/s)
    float footRingLife_ = 0.55f;
    float footRingScale_ = 0.90f;

    // コインの光芒（ゲーミング）
    int coinShineCount_ = 32;         //!< コイン1枚あたりの同時数
    float coinShineLife_ = 0.7f;
    float coinShineScale_ = 0.78f;
    float coinShineAspect_ = 0.42f;   //!< 横／縦。1未満で細長くなる
    float coinShineSpeed_ = 1.6f;
    float coinVanishBoost_ = 2.2f;    //!< 取得後、消えるまでの発生倍率
    float coinRange_ = 24.0f;         //!< この距離より遠いコインは光らない
    int coinMaxEmitters_ = 12;        //!< 同時に光るコインの上限

    // 敵の不気味な光
    int enemyAuraCount_ = 16;         //!< 敵1体あたりの同時数
    float enemyAuraLife_ = 1.25f;
    float enemyAuraScale_ = 0.675f;   //!< 他より控えめ（0.75 倍相当）
    float enemyAuraRise_ = 0.75f;
    float enemyRange_ = 26.0f;
    int enemyMaxEmitters_ = 16;

    // 単発
    int hitSplashCount_ = 32;
    float hitSplashSpeed_ = 5.5f;
    int defeatSplashCount_ = 48;
    float defeatSplashSpeed_ = 7.0f;
    int splitBurstCount_ = 64;
    float splitBurstSpeed_ = 11.0f;

    // ゲーミング（虹色）のベクター場
    float gamingTimeScale_ = 0.55f;
    float gamingSpaceScale_ = 0.035f;
    float gamingGain_ = 0.85f;
};
