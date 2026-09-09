#pragma once

#include "Slime.h"
#include <vector>
#include <memory>
#include <d3d12.h>
#include <wrl.h>

class Object3dCom;
class Camera;
class KeyInput;
class RenderContext;

/**
 * @brief スライム群衆マネージャー（全スライムの一元管理）
 */
class SlimeManager {
public:
    SlimeManager() = default;
    ~SlimeManager() = default;

    void Initialize(Object3dCom* object3dCom, Camera* camera);
    void Update(float deltaTime, KeyInput* keyInput, const Vector2& stageTilt);
    void Draw(const RenderContext& ctx);
    void DrawDebug(Camera* camera);

    // スライム生成・消去
    Slime* SpawnSlime(const Vector3& pos, int size = 1);
    void SpawnSlimes(const Vector3& basePos, int count, int sizePerSlime = 1);
    void Clear();

    // 合体 / 分裂 / ジャンプ
    void RequestMerge() { mergeRequested_ = true; }
    void TriggerSplit();
    void TriggerJump();

    /// @brief 自爆（E キーによる全員分裂）が起きたことを外へ伝えるイベント
    /// @note 旧 PikminPlayer::SelfDestructEvent の移植。分裂は SlimeManager 側の
    ///       責務になったので、発火点もこちらに移してある
    struct SelfDestructEvent
    {
        bool fired = false;                    //!< このフレームに自爆したか
        Vector3 position{ 0.0f, 0.0f, 0.0f };  //!< 爆心（分裂した瞬間の最大スライムの位置）
        int sizeBefore = 1;                    //!< 分裂前の最大塊サイズ。爆風の広さに使う
    };

    /**
     * @brief 自爆イベントを取り出してクリアする
     * @param[out] out 取り出したイベント
     * @return 自爆していれば true
     * @note 1回の自爆につき1回だけ true を返す。拾い手は1箇所にすること（EnemyManager）
     */
    bool TakeSelfDestructEvent(SelfDestructEvent& out);

    /// @brief 演出・SE 用の1フレームイベント
    struct FxEvents
    {
        bool jumped = false;                        //!< SPACE キーでジャンプした
        bool split = false;                         //!< E キーで分裂（自爆）した
        Vector3 splitPosition{ 0.0f, 0.0f, 0.0f };  //!< 分裂した瞬間の位置
        int splitSizeBefore = 1;                    //!< 分裂前の最大塊サイズ
    };

    /**
     * @brief 演出・SE 用のイベントを取り出してクリアする
     * @param[out] out 取り出したイベント
     * @return 何か起きていれば true
     * @note TakeSelfDestructEvent() とは別枠。
     *       あちらは EnemyManager が1箇所で拾ってしまうので、
     *       演出・SE 側はこちらを使う（拾い手が競合しない）
     */
    bool TakeFxEvents(FxEvents& out);
    void TriggerStageBounce(const Vector2& stageTilt, const Vector2& pivot, float bouncePower = 13.5f);

    // カメラ用：全スライムの重心と広がり
    void GetGroupCenterAndSpread(Vector3& outCenter, float& outSpread) const;

    // スライム情報取得
    int GetActiveCount() const;
    int GetLivingCount() const;
    int GetTotalCount() const;
    int GetMaxSlimeSize() const;
    int GetTotalSize() const;

    const std::vector<std::unique_ptr<Slime>>& GetSlimes() const { return slimes_; }

    /**
     * @brief 群れの「代表」＝いま一番大きい生存スライムを返す
     * @return 生きているスライムが1体も居なければ nullptr
     * @note 旧設計の PikminPlayer（プレイヤー本体）に相当する。
     *       スライムが一本化されて「プレイヤー」という区別が無くなったので、
     *       敵の強さ比較・カメラの注視・HUD の残機表示などは
     *       「一番大きい個体」を代表として扱う。同サイズなら先に見つかったもの
     */
    Slime* GetLeader();
    const Slime* GetLeader() const;

    float GetMergeThreshold() const { return mergeThreshold_; }
    void SetMergeThreshold(float t) { mergeThreshold_ = t; }

    /**
     * @brief ミニオン（＝代表以外のスライム）の移動速度の倍率
     * @note 自爆で散らばったミニオンに追いつけるようにするためのもの。
     *       **代表（一番大きい個体＝プレイヤー本体）は常に 1.0 のまま。**
     *       掛かるのはステージ傾斜による転がりと斜面すべりだけで、
     *       分裂で弾け飛ぶ勢い（Launch）には影響しない
     */
    float GetMinionSpeedScale() const { return minionSpeedScale_; }
    void SetMinionSpeedScale(float s) { minionSpeedScale_ = (s < 0.0f) ? 0.0f : s; }

    float GetSplitPopPower() const { return splitPopPower_; }
    void SetSplitPopPower(float p) { splitPopPower_ = p; }
    float GetSplitUpPower() const { return splitUpPower_; }
    void SetSplitUpPower(float p) { splitUpPower_ = p; }

    static constexpr int kMaxSlimes = 30;

    /// @brief 残機（＝全スライムのサイズ合計）の既定上限
    static constexpr int kDefaultMaxTotalSize = 20;

    /// @brief 残機の上限。GrowthCube はこれを超える分を吐き出さない
    int GetMaxTotalSize() const { return maxTotalSize_; }
    void SetMaxTotalSize(int v) { maxTotalSize_ = (v < 1) ? 1 : v; }

    /// @brief これ以上大きくなれるか（残機が上限未満か）
    bool CanGrow() const { return GetTotalSize() < maxTotalSize_; }

    /**
     * @brief 弾に当たった1体を「分離」させる
     * @param victim 被弾した個体
     * @param knockDir 弾が飛んできた方向（水平成分だけ使う）
     * @param knockSpeed 本体のノックバック初速
     * @param ejectSpeed はじけ飛ぶサイズ1の初速（こちらのほうが速い）
     * @return はじけ飛んだサイズ1のスライム。死んだ・発生しなかったときは nullptr
     * @note サイズ N (>1) → N-1 になり、サイズ1が1体遠くへ飛ぶ。**残機合計は変わらない**。
     *       サイズ1の個体が当たったときだけその場で消滅する（＝残機 -1）。
     * @warning 中で slimes_ に push_back するので、**GetSlimes() を回しながら呼ばないこと**。
     *          Slime の実体は unique_ptr の先なので Slime* 自体は生き続けるが、
     *          vector のイテレータは無効化される。被弾した個体を先に集めてから呼ぶ
     */
    Slime* EjectOnBulletHit(Slime* victim, const Vector3& knockDir,
                            float knockSpeed, float ejectSpeed);

private:
    void ResolveSeparation(const Vector3& rotation, const Vector2& stageTilt, const Vector2& pivot);
    void CheckAndResolveMerge(const Vector2& stageTilt, const Vector2& pivot);
    void CreateXRayPipeline();

private:
    Object3dCom* object3dCom_ = nullptr;
    Camera* camera_ = nullptr;
    std::vector<std::unique_ptr<Slime>> slimes_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> xRayPSO_;

    bool mergeRequested_ = false;

    // 自爆（E キー分裂）イベント
    SelfDestructEvent selfDestruct_;

    // 演出・SE 用のイベント（自爆イベントとは別枠）
    FxEvents fxEvents_;
    float mergeThreshold_ = 2.5f;
    float splitPopPower_ = 8.0f;
    float splitUpPower_ = 7.0f;

    // 残機（サイズ合計）の上限
    int maxTotalSize_ = kDefaultMaxTotalSize;

    // ミニオン（代表以外）の移動速度の倍率。プレイヤー本体は常に 1.0
    float minionSpeedScale_ = 0.67f;

    // 全員落下時にカメラを初期位置に戻さないための直前有効位置キャッシュ
    Vector3 lastValidCenter_{ 0.0f, 0.5f, 0.0f };
    float lastValidSpread_ = 1.0f;
    float lastValidScale_ = 0.4f;
};
