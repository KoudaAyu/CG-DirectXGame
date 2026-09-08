#pragma once

#include <memory>

#include "Baziru3_Engine/Core/Base/Vector.h"
#include "Baziru3_Engine/Core/Base/RenderContext.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3d.h"

class Object3dCom;
class Camera;

/**
 * @brief 収集アイテム（コイン）1枚
 *
 * 仕様はあえて最小限:
 *   - 地面には追従する（ステージを傾けても床に貼り付いたまま）
 *   - 落下・跳ね返り・押し出しといった物理は一切やらない。置かれた場所にただ在る
 *   - 当たり判定はプレイヤーとの距離だけ。コライダーは登録しない
 *     （敵と同じく、エンジン側の押し出しに巻き込まれて勝手に動くのを避けるため）
 *
 * @note 座標は EnemyBase と同じく「ステージローカル（傾き0のときのワールド座標）」を正とし、
 *       毎フレーム StageLocalToWorld 相当の変換でワールド座標を導出する。
 *
 * @note 取得されると、その場で消えるのではなく
 *       「上昇しながら小さくなって消える」演出に入る（IsVanishing() が true の間）。
 *       GamePlaySceneFx はそのあいだパーティクルを出し続ける。
 */
class Coin
{
public:
    Coin() = default;
    ~Coin();

    /**
     * @brief 初期化
     * @param object3dCom 3D描画コンポーネント
     * @param camera カメラ
     * @param modelData 共有のコインメッシュ（CoinManager が持っている実体）
     * @param stageLocalPos 配置座標（ステージが水平なときのワールド座標）
     */
    void Initialize(Object3dCom* object3dCom, Camera* camera,
                    const Object3d::ModelData& modelData, const Vector3& stageLocalPos);

    /**
     * @brief 更新
     * @param deltaTime デルタタイム
     * @param stageTilt ステージ傾斜
     * @param pivot 傾斜の回転中心（プレイヤー XZ）
     * @param spinSpeed 自転速度 (rad/s)
     * @param bobHeight 上下ふわふわの振幅 (m)
     * @param bobSpeed 上下ふわふわの速さ
     * @param heightOffset 床からの浮かせ量 (m)
     */
    void Update(float deltaTime, const Vector2& stageTilt, const Vector2& pivot,
                float spinSpeed, float bobHeight, float bobSpeed, float heightOffset);

    void Draw(const RenderContext& ctx);
    void Finalize();

    // --- 状態 ---
    bool IsCollected() const { return isCollected_; }

    /// @brief 取得された。上昇しながら小さくなって消える演出に入る
    void Collect();

    /// @brief 取得済みで、まだ消えきっていない（＝演出とパーティクルが続いている）
    bool IsVanishing() const { return isVanishing_; }

    /// @brief 演出なしで「取得済み」にする（設定を変えて作り直したときの状態復元用）
    void CollectSilently() { isCollected_ = true; isVanishing_ = false; vanishTimer_ = 0.0f; }

    void Revive();

    // --- 座標 ---
    const Vector3& GetPosition() const { return position_; }
    const Vector3& GetStageLocalPosition() const { return anchorLocal_; }

    /// @brief 配置し直す（エディタのドラッグ移動用）。次のフレームで床へ再吸着する
    void SetStageLocalPosition(const Vector3& p) { anchorLocal_ = p; needsGroundSnap_ = true; }

    void SetScale(float s) { scale_ = s; }
    float GetScale() const { return scale_; }

    /// @brief 消えるまでの時間 (秒)
    static float GetVanishSeconds() { return sVanishSeconds_; }
    static void SetVanishSeconds(float s) { sVanishSeconds_ = s; }

    /// @brief 消えながら上昇する速さ (m/s)
    static float GetVanishRiseSpeed() { return sVanishRiseSpeed_; }
    static void SetVanishRiseSpeed(float s) { sVanishRiseSpeed_ = s; }

    Object3d* GetObject3d() const { return object3d_.get(); }

private:
    static float sVanishSeconds_;   //!< 取得 -> 完全に消えるまでの時間
    static float sVanishRiseSpeed_; //!< 消えながら上る速さ

    std::unique_ptr<Object3d> object3d_;

    Vector3 anchorLocal_{ 0.0f, 0.0f, 0.0f }; //!< ステージローカル座標（配置データの正）
    Vector3 position_{ 0.0f, 0.0f, 0.0f };    //!< 実際のワールド座標（毎フレーム導出）
    Vector3 groundNormal_{ 0.0f, 1.0f, 0.0f };
    Vector3 rotation_{ 0.0f, 0.0f, 0.0f };

    float scale_ = 1.0f;
    float spin_ = 0.0f;      //!< 自転角
    float lifeTime_ = 0.0f;  //!< ふわふわの位相に使う
    bool isCollected_ = false;
    bool needsGroundSnap_ = true;

    // 取得演出
    bool isVanishing_ = false;
    float vanishTimer_ = 0.0f;
    float vanishBaseScale_ = 1.0f;
};
