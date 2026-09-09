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

    float GetSplitPopPower() const { return splitPopPower_; }
    void SetSplitPopPower(float p) { splitPopPower_ = p; }
    float GetSplitUpPower() const { return splitUpPower_; }
    void SetSplitUpPower(float p) { splitUpPower_ = p; }

    static constexpr int kMaxSlimes = 30;

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

    // 全員落下時にカメラを初期位置に戻さないための直前有効位置キャッシュ
    Vector3 lastValidCenter_{ 0.0f, 0.5f, 0.0f };
    float lastValidSpread_ = 1.0f;
    float lastValidScale_ = 0.4f;
};
