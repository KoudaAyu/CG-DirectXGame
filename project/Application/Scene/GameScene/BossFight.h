#pragma once

#include <memory>
#include <string>
#include <vector>

#include <d3d12.h>

#include "Baziru3_Engine/Core/Base/Vector.h"
#include "Baziru3_Engine/Core/Base/RenderContext.h"
#include "Application/Enemy/Boss.h"
#include "Application/Enemy/EnemyBullet.h"
#include "Application/Editor/StageLayout.h"
#include "Application/Scene/GameScene/BossHpBar.h"

class Object3dCom;
class Camera;
class Slime;
class SlimeManager;
class StageTerrain;
class GamePlaySceneFx;

/**
 * @brief ボス戦フェーズ
 *
 * ボス本体（Boss）、ボスの弾、HPバー、カメラ演出、フェーズ進行をまとめて持つ。
 * GamePlayScene からは「毎フレーム Update して、返ってきた要求に従う」だけでいい。
 *
 * フェーズ:
 * @code
 *   Idle      トリガー待ち。ボスはその場に立っているが撃たない
 *     | プレイヤーがボス戦トリガー付きの地形メッシュに踏み入れた
 *   FocusIn   カメラがボスの正面へ寄って、また戻る。この間ボスもプレイヤーも動けない
 *     | カメラが戻りきった
 *   Battle    通常の戦闘。ボスが全方向弾を撃つ
 *     | HP <= 0 になった / プレイヤーのほうが強くなって体当たりされた
 *   Dying     カメラがボスの正面へ。数秒かけて震え・オーラ・カメラシェイクが激しくなる
 *     | 溜めきった
 *   Explode   プレイヤーの自爆のような大爆発。ボスは消える
 *     | 余韻
 *   Finished  クリアシーンへ切り替えてほしい（requestClear が立つ）
 * @endcode
 *
 * @note ボスは EnemyManager には入れていない。あちらは MobEnemy 専用で、
 *       強さ＝HP・専用の弾・専用の演出が全部特殊なため。
 *       衝突解決だけ EnemyCollision を共用している
 */
class BossFight
{
public:
    enum class Phase
    {
        Idle,     //!< トリガー待ち
        FocusIn,  //!< 登場フォーカス（ボスもプレイヤーも動けない）
        Battle,   //!< 戦闘中
        Dying,    //!< 死亡演出（震え＋オーラ＋カメラシェイク）
        Explode,  //!< 爆発してボスが消えたあとの余韻
        Finished, //!< 終了。クリアシーンへ
    };

    /// @brief ボス戦が触るシーン側のオブジェクト
    struct SceneRefs
    {
        Object3dCom* object3dCom = nullptr;
        Camera* camera = nullptr;
        SlimeManager* slimeManager = nullptr;
        StageTerrain* terrain = nullptr;   //!< ボス戦トリガーの判定に使う
        GamePlaySceneFx* fx = nullptr;     //!< オーラ・爆発の発生先
    };

    /// @brief 毎フレーム渡す状態
    struct FrameInput
    {
        float deltaTime = 1.0f / 60.0f;
        Vector2 stageTilt{ 0.0f, 0.0f };
        Vector2 pivot{ 0.0f, 0.0f };   //!< 傾斜の回転中心（＝スライム重心の XZ）
        int playerLife = 1;            //!< 残機（＝全スライムのサイズ合計）。スコアに使う
        bool editorMode = false;       //!< 配置エディタ中は全部止める
    };

    /// @brief 1フレームぶんの「シーンにやってほしいこと」
    struct FrameResult
    {
        bool freezeSlimes = false;  //!< true の間、プレイヤーの入力と速度を殺す
        bool requestClear = false;  //!< クリアシーンへ切り替えてほしい（1回だけ立つ）
        int scoreGain = 0;          //!< 加算するスコア（ボス撃破時のみ）
        float cameraShake = 0.0f;   //!< このフレームに足すカメラシェイクの trauma
        Vector3 scorePopupAt{ 0.0f, 0.0f, 0.0f }; //!< スコア加算を浮かせる位置

        // --- BGM ---
        // BossFight は音を鳴らさない。「切り替えて」と言うだけにして、
        // 実際に Stop / Play するのは GamePlayScene の1箇所にまとめてある
        // （BGM は同時に1本だけ、という決まりをそこで守れる）
        bool requestBossBgm = false;   //!< ボス戦BGMへ切り替えてほしい（1回だけ立つ）
        bool requestNormalBgm = false; //!< 通常BGMへ戻してほしい（1回だけ立つ）
    };

public:
    BossFight() = default;
    ~BossFight();

    BossFight(const BossFight&) = delete;
    BossFight& operator=(const BossFight&) = delete;

    void Initialize(const SceneRefs& refs);
    void Finalize();

    /// @brief 配置データからボスを置き直す（シーン初期化・エディタからの反映）
    void SetLayout(const StageBossEntry& entry);

    /// @brief 現在の配置を書き出す（エディタの保存用）
    void WriteLayout(StageBossEntry& out) const;

    /// @brief 最初の状態へ戻す（R キーのリスタート用）
    void Restart();

    /**
     * @brief プレイヤーが自爆した（E キーの全員分裂）
     * @note SlimeManager::TakeSelfDestructEvent() は EnemyManager が1箇所で拾ってしまうので、
     *       GamePlayScene が TakeFxEvents() で拾ったものをこちらへ回してもらう。
     *       次の Update() でまとめて HP を減らす
     */
    void NotifySelfDestruct(const Vector3& position);

    FrameResult Update(const FrameInput& input);

    void Draw(const RenderContext& ctx);

    /// @brief HPバー（2D）。3D の後、ほかの HUD と一緒に描く
    void DrawHud(ID3D12GraphicsCommandList* commandList);

    void DrawImGui();

    // --- 状態 ---
    Phase GetPhase() const { return phase_; }
    bool IsFighting() const { return phase_ == Phase::Battle; }
    bool IsEnabled() const { return enabled_; }

    Boss* GetBoss() { return boss_.get(); }
    const Boss* GetBoss() const { return boss_.get(); }

    /// @brief ボスが画面に居るか（オーラを出す対象か）
    bool HasVisibleBoss() const;

    // --- カメラ ---
    /**
     * @brief カメラの乗っ取り具合 (0..1)
     * @note 0 なら通常のカメラのまま。1 なら完全にボスの正面。
     *       シーン側は自分の計算した位置・回転と GetCameraTarget() を
     *       この重みで補間して使う
     */
    float GetCameraBlend() const { return cameraBlend_; }

    /**
     * @brief ボスの正面から見たときのカメラ位置と回転
     * @return ボットが居ない等で計算できなければ false
     */
    bool GetCameraTarget(Vector3& outPosition, Vector3& outRotation) const;

    // --- 配置エディタ用 ---
    /// @brief ボスの配置座標（ステージローカル）
    Vector3 GetStageLocalPosition() const { return placedPosition_; }
    void SetStageLocalPosition(const Vector3& p);
    void SetEnabled(bool enabled);
    int GetMaxHp() const { return placedHp_; }
    void SetMaxHp(int hp);

    /// @brief ボスのワールド座標（マーカー描画・ピッキング用）
    Vector3 GetWorldPosition() const;

    /// @brief ピッキング半径
    float GetPickRadius() const;

private:
    void SpawnBoss();
    void DestroyBoss();

    /// @brief トリガー判定。プレイヤーがボス戦トリガー付きのメッシュに乗ったか
    bool CheckTrigger(const Vector2& pivot) const;

    /// @brief ボスの弾を1発撃つ（プールから使い回す）
    void FireBullet(const Vector3& origin, const Vector3& direction);

    /// @brief ボスの弾 vs 全スライム
    /// @note 当たり方は雑魚の弾と同じ（EnemyManager::ResolveBulletCollisions と対）
    void ResolveBulletCollisions(SlimeManager* slimeManager);

    /// @brief 全スライム vs ボス。プレイヤーのほうが強ければボスが死ぬ
    /// @return このフレームにボスが倒されたら true
    bool ResolveSlimeCollisions(SlimeManager* slimeManager, const Vector2& stageTilt,
                                const Vector2& pivot, FrameResult& result);

    void EnterPhase(Phase next);
    void BeginDeath();

    /// @brief 現在のステージ傾斜での床面法線
    static Vector3 CalcStageNormal(const Vector2& stageTilt);

private:
    SceneRefs refs_{};

    std::unique_ptr<Boss> boss_;
    std::vector<std::unique_ptr<EnemyBullet>> bullets_;
    Object3d::ModelData bulletModel_;
    std::string bulletModelKey_;
    bool bulletModelReady_ = false;

    BossHpBar hpBar_;

    Phase phase_ = Phase::Idle;
    float phaseTimer_ = 0.0f;

    bool enabled_ = false;              //!< 配置データでボスが有効か
    Vector3 placedPosition_{ 0.0f, 0.0f, 0.0f };
    int placedHp_ = 100;

    int pendingSelfDestructs_ = 0;      //!< 次の Update でまとめて処理する自爆の回数
    Vector3 lastSelfDestructPos_{ 0.0f, 0.0f, 0.0f };

    bool clearRequested_ = false;

    // --- カメラ ---
    float cameraBlend_ = 0.0f;
    Vector3 cameraTargetPos_{ 0.0f, 0.0f, 0.0f };
    Vector3 cameraTargetRot_{ 0.0f, 0.0f, 0.0f };
    bool cameraTargetValid_ = false;

public:
    // ===============================================================
    // 調整パラメータ（ImGui の "Boss Fight" から全部いじれる）
    // ===============================================================

    // --- 登場フォーカス ---
    float focusInSeconds_ = 3.0f;      //!< フォーカスに入って戻ってくるまでの合計時間
    float focusInRatio_ = 0.32f;       //!< そのうち「寄っていく」割合
    float focusHoldRatio_ = 0.38f;     //!< 「ボスを映しっぱなし」の割合
    float focusDistance_ = 11.0f;      //!< ボスからカメラまでの距離 (m)
    float focusHeight_ = 3.2f;         //!< 注視点からのカメラの高さ (m)
    float focusLookHeightRatio_ = 0.75f; //!< ボスのどのあたりを見るか（0=足元 / 1=頭）

    // --- 死亡演出 ---
    float deathSeconds_ = 3.0f;        //!< 震えとオーラを溜める時間
    float deathBlendInRatio_ = 0.2f;   //!< カメラが寄りきるまでの割合
    float deathShakeMax_ = 0.95f;      //!< 最大のカメラシェイク trauma
    float deathAuraMax_ = 6.0f;        //!< 最大のオーラ倍率
    float deathBodyShakeMax_ = 0.14f;  //!< ボス本体の震え幅（スケール比）
    float explodeSeconds_ = 2.0f;      //!< 爆発してからクリアへ行くまでの余韻
    float explodeShake_ = 1.0f;        //!< 爆発の瞬間のカメラシェイク

    // --- 戦闘 ---
    float bulletKnockback_ = 9.0f;     //!< 被弾時のノックバック初速（本体）
    float bulletEjectSpeed_ = 16.0f;   //!< 被弾ではじけ飛ぶ強さ1のスライムの初速
    float bounceSpeed_ = 15.0f;        //!< ボスに弾かれるときの初速（代表）
    float minionBounceSpeed_ = 11.0f;  //!< 同（代表以外）
    bool enableCollision_ = true;

    // --- デバッグ ---
    bool debugForceTrigger_ = false;
};
