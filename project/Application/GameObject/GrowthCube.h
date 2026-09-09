#pragma once

#include "Baziru3_Engine/Graphics/3D/Object/Object3d.h"
#include "Baziru3_Engine/Core/Camera/Camera.h"
#include "Baziru3_Engine/Core/Base/RenderContext.h"
#include "Baziru3_Engine/Core/Base/Vector.h"
#include "Application/GameObject/SlimePhysics.h"
#include <memory>

class Object3dCom;
class SlimeManager;
class Slime;

/**
 * @brief 食べると残機（＝スライムのサイズ）が1つ増えるアイテム
 *
 * 名前は「Cube」だが、見た目は**スライムのプロシージャル球**を
 * Slime シェーダーで描いたもの。プレイヤーと同じぷるぷる質感で、
 * ベースカラーだけが「ゲーミング（虹色が流れ続ける）」になっている。
 *
 * 演出:
 *   - 本体はゲーミング色でぷるぷる揺れながら浮遊・自転する
 *   - まわりに**大きめのパーティクル**を撒いて、自分で光っているように見せる
 *     （発生は GamePlaySceneFx 側。本体より少し金色に寄せた色）
 *   - 食べられると縮みながらスライムへ吸い込まれ、コインのような光芒が弾ける
 *
 * @note 座標は EnemyBase / Coin と同じく「ステージローカル（傾き0のときのワールド座標）」
 *       を正とし、毎フレーム傾斜ピボット回転でワールド座標を導出する。
 * @note コライダーは登録しない。取得判定はスライムとの距離だけ
 *       （敵・コインと同じ理由。エンジン側の押し出しに巻き込まれて勝手に動くのを避ける）
 */
class GrowthCube
{
public:
    enum class State
    {
        Active,      //!< 浮遊・自転中（取得可能）
        Collecting,  //!< 取得演出中（膨らんでから縮み、スライムへ吸い込まれる）
        Inactive     //!< 消滅済み（リスポーン待機）
    };

public:
    GrowthCube() = default;
    ~GrowthCube() = default;

    /**
     * @brief 初期化
     * @param object3dCom 描画コンポーネント
     * @param camera カメラ
     * @param stageLocalPos ステージローカル座標（傾き0のときのワールド座標）
     * @param size 見た目の直径めやす
     */
    void Initialize(Object3dCom* object3dCom, Camera* camera, const Vector3& stageLocalPos, float size = 0.85f);

    /**
     * @brief 毎フレーム更新
     * @param deltaTime デルタタイム
     * @param stageTilt ステージ傾斜
     * @param pivot 傾斜の回転中心（スライム重心の XZ）
     * @param slimeManager スライム群（nullptr なら取得判定をしない）
     * @return このフレームに食べられたら true（演出・SE のトリガ）
     */
    bool Update(float deltaTime, const Vector2& stageTilt, const Vector2& pivot, SlimeManager* slimeManager);

    /// @brief Slime シェーダー（PipelineStateManager の "Slime_Normal"）で描く
    void Draw(const RenderContext& ctx);

    /// @brief アイテムを再出現させる
    void Respawn();

    State GetState() const { return state_; }
    bool IsActive() const { return state_ == State::Active; }

    /// @brief 取得演出中（まだ画面に居る）
    bool IsVanishing() const { return state_ == State::Collecting; }

    /// @brief パーティクルを出す対象か（本体が画面に居るあいだ true）
    bool IsVisible() const { return state_ != State::Inactive; }

    const Vector3& GetStageLocalPosition() const { return anchorLocal_; }

    /// @brief 配置し直す（エディタのドラッグ移動用）。次のフレームで床へ再吸着する
    void SetStageLocalPosition(const Vector3& p) { anchorLocal_ = p; needsGroundSnap_ = true; }

    const Vector3& GetPosition() const { return position_; }

    float GetBaseSize() const { return baseSize_; }
    void SetBaseSize(float size);

    /// @brief 見た目の半径（パーティクルの湧く範囲に使う）
    float GetVisualRadius() const { return baseSize_ * 0.5f * currentScale_; }

    /// @brief いまのゲーミング色（パーティクル側が金色に寄せて使う）
    const Vector4& GetGamingColor() const { return slimeParams_.baseColor; }

    bool IsAutoRespawn() const { return autoRespawn_; }
    void SetAutoRespawn(bool enable) { autoRespawn_ = enable; }

    SlimeParamsCPU& GetSlimeParams() { return slimeParams_; }

    // ===============================================================
    // 見た目の共有パラメータ（ImGui から調整する用）
    // ===============================================================
    static float sHoverAmplitude;   //!< 上下ホバリングの振幅 (m)
    static float sHoverSpeed;       //!< 上下ホバリングの速さ
    static float sSpinSpeed;        //!< 自転速度 (rad/s)
    static float sHeightOffset;     //!< 床からの浮かせ量 (m)
    static float sCollectSeconds;   //!< 取得 -> 完全に消えるまでの時間
    static float sGamingTimeScale;  //!< 虹色が流れる速さ
    static float sGamingSpaceScale; //!< 位置による色ずれ
    static float sGamingGain;       //!< 色の強さ（1.0 で原色）
    static float sPickupRadius;     //!< 取得半径にこのぶんだけ余裕を足す (m)

    /// @brief ゲーミング（虹色）を1色引く。パーティクル側と式をそろえるために公開している
    static Vector4 SampleGamingColor(float time, const Vector3& position);

private:
    /// @brief スライムとの接触判定。食べられたら true
    bool CheckSlimeCollision(SlimeManager* slimeManager);

private:
    Object3dCom* object3dCom_ = nullptr;
    Camera* camera_ = nullptr;
    std::unique_ptr<Object3d> object3d_;
    Object3d::ModelData modelData_;
    uint32_t textureIndex_ = 0;

    Vector3 anchorLocal_{ 0.0f, 0.0f, 0.0f }; //!< ステージローカル座標（配置データの正）
    Vector3 position_{ 0.0f, 0.0f, 0.0f };    //!< 実際のワールド座標（毎フレーム導出）
    Vector3 rotation_{ 0.0f, 0.0f, 0.0f };
    Vector3 groundNormal_{ 0.0f, 1.0f, 0.0f };

    float baseSize_ = 0.85f;
    float currentScale_ = 1.0f; //!< 取得演出の拡大・収縮倍率
    float spin_ = 0.0f;
    float lifeTime_ = 0.0f;
    bool needsGroundSnap_ = true;

    State state_ = State::Active;
    float collectTimer_ = 0.0f;
    Vector3 collectStartPos_{ 0.0f, 0.0f, 0.0f };

    /// @brief 吸い込まれる先。取得した瞬間の位置を控えておく
    /// @note SlimeManager は合体すると実体を erase するので、
    ///       Slime* を持ち越して毎フレーム GetPosition() を読むとダングリングする
    Vector3 collectTarget_{ 0.0f, 0.0f, 0.0f };
    Slime* collectedBySlime_ = nullptr; //!< デバッグ表示用。参照外しはしないこと

    SlimeParamsCPU slimeParams_;

    bool autoRespawn_ = false;
    float respawnTimer_ = 0.0f;
    float respawnCooldown_ = 10.0f;
};
