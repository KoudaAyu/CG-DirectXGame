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
class GrowthCube;
class GrowthCubeManager;
class Boss;
class EnemyBullet;

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

    /// @brief 成長キューブの「自分の七光り」。本体より金色に寄せた大きい粒
    void UpdateGrowthCubes(float deltaTime, GrowthCubeManager* growthCubeManager);

    /// @brief ボスの禍々しいオーラ。Boss::GetAuraIntensity() で激しさが変わる
    void UpdateBoss(float deltaTime, Boss* boss);

    /**
     * @brief 敵の弾に「光る芯」をまとわせる（弾が小さくて見えない対策）
     * @note 1発につき毎フレーム1粒。寿命が短いので、飛んだ跡が尾のように残る。
     *       弾は同時にせいぜい数十発なので、レート制御は要らない
     */
    void UpdateBullets(float deltaTime, EnemyManager* enemyManager);

    /// @brief 弾1発ぶんの芯の光。ボスの弾は BossFight がこれを直接呼ぶ
    void EmitBulletGlow(const Vector3& position, const Vector3& velocity,
                        float radius, const Vector4& color, bool isBoss);

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
                   EnemyManager* enemyManager, GrowthCubeManager* growthCubeManager = nullptr);

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

    /// @brief 成長キューブが食べられたときの、コインのような光芒が弾ける演出
    void EmitGrowthCubeCollect(const Vector3& position, float radius);

    /// @brief ボスが自爆でダメージを受けたときの一撃
    void EmitBossHit(const Vector3& position, float radius);

    /// @brief ボスの最期の大爆発（ゲーミング色＋敵オーラ色。プレイヤー分裂の2倍くらい）
    void EmitBossExplosion(const Vector3& position, float radius);

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

    /// @brief 成長キューブ1個ぶんの「自分の七光り」を出す
    void EmitGrowthCubeAura(const Vector3& center, float radius, const Vector4& bodyColor);

    /// @brief ボス1体ぶんの禍々しいオーラを出す
    void EmitBossAura(const Vector3& center, float radius, float intensity);

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
    float growthCubeAccum_ = 0.0f;
    float bossAuraAccum_ = 0.0f;
    float bossEmberAccum_ = 0.0f;

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

    // 成長キューブの「自分の七光り」
    bool enableGrowthCubeGlow_ = true;
    int growthCubeGlowCount_ = 28;      //!< キューブ1個あたりの同時数
    float growthCubeGlowLife_ = 1.05f;
    float growthCubeGlowScale_ = 1.55f; //!< **でかい粒**。ほかの演出の倍くらい
    float growthCubeGlowSpeed_ = 1.1f;
    float growthCubeGoldMix_ = 0.45f;   //!< 本体の色を金色へ寄せる量 (0..1)
    Vector4 growthCubeGold_{ 1.0f, 0.82f, 0.25f, 1.0f };
    float growthCubeGlowAlpha_ = 0.70f;
    float growthCubeVanishBoost_ = 2.4f; //!< 食べられている最中の発生倍率
    float growthCubeRange_ = 28.0f;
    int growthCubeMaxEmitters_ = 8;
    int growthCubeCollectCount_ = 56;   //!< 食べられた瞬間に弾けるコイン光芒の数
    float growthCubeCollectSpeed_ = 9.0f;

    // ボスの禍々しいオーラ
    bool enableBossAura_ = true;
    int bossAuraCount_ = 96;            //!< 同時数（intensity 1.0 のとき）
    float bossAuraLife_ = 1.5f;
    float bossAuraScale_ = 1.30f;
    float bossAuraRise_ = 1.6f;
    float bossAuraSwirl_ = 2.2f;        //!< 接線方向の流れ（渦を巻く）
    float bossAuraAlpha_ = 0.80f;
    int bossEmberCount_ = 32;           //!< 落ちてくる火の粉の同時数
    float bossEmberLife_ = 1.2f;
    float bossEmberScale_ = 0.85f;
    int bossHitCount_ = 40;             //!< 被弾1回ぶんの粒
    float bossHitSpeed_ = 9.0f;
    int bossExplosionCount_ = 128;      //!< 最期の大爆発（分裂バーストの2倍相当）
    float bossExplosionSpeed_ = 15.0f;

    // 敵・ボスの弾の芯の光
    bool enableBulletGlow_ = true;
    float bulletGlowScale_ = 2.6f;    //!< 弾の当たり判定半径の何倍の粒を出すか
    float bulletGlowLife_ = 0.22f;    //!< 短いほどキュッと締まった芯になる
    float bulletGlowAlpha_ = 0.85f;
    float bulletGlowWhiteness_ = 0.45f; //!< 弾の色を白へ寄せる量（芯を明るく見せる）
    float bossBulletGlowBoost_ = 1.35f; //!< ボスの弾は一回り大きく

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
