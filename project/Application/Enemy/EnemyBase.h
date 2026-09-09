#pragma once

#include <functional>
#include <memory>
#include <string>

#include "Baziru3_Engine/Core/Base/Vector.h"
#include "Baziru3_Engine/Core/Base/RenderContext.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3d.h"
#include "Baziru3_Engine/Framework/Collision/BoxCollider.h"
#include "Application/Enemy/EnemyCollision.h"
#include "Application/Enemy/EnemyAnimation.h"

class Object3dCom;
class Camera;

/// @brief モブ敵の種類
enum class EnemyType
{
    Slime,          //!< 動きまわる。弾は撃たない
    FlowerClover,   //!< 地面に固定。弾は撃たない
    FlowerLotus,    //!< 動きまわる。弾を撃つ
    FlowerSunward,  //!< 地面に固定。弾を撃つ
    Boss,           //!< ボス。HP を持ち、その場で全方向弾を撃つ（Application/Enemy/Boss.h）

    Count
};

/// @brief 1フレーム分の外部状況（EnemyManager から流し込む）
struct EnemyUpdateContext
{
    float deltaTime = 1.0f / 60.0f;
    Vector2 stageTilt{ 0.0f, 0.0f }; //!< ステージ傾斜 (x: pitch, y: roll)
    Vector2 pivot{ 0.0f, 0.0f };     //!< 傾斜の回転中心（＝プレイヤーの XZ）
    Vector3 playerPos{ 0.0f, 0.0f, 0.0f };
    int playerStrength = 1;          //!< プレイヤーの塊サイズ
};

/**
 * @brief 敵キャラクター共通基底
 *
 * ここが持つのは「どの敵にも要る土台」だけ:
 *   - モデル読み込みと描画（スキニングアニメーション対応）
 *   - ステージ傾斜フレーム <-> ワールド座標の変換（傾けても地面から取り残されない）
 *   - 地面高さへの吸着と、地形法線に沿った姿勢
 *   - 強さ（strength）と、そこから見た目スケールを決める関数（差し替え可能）
 *   - 当たり判定の形状データ（押し出しは EnemyManager が自前でやる）
 *   - 撃破処理
 *
 * 個々の挙動（徘徊・追跡・射撃）は UpdateBehavior() を override して書く。
 * ボスもこれを継承すれば土台をそのまま使える想定。
 *
 * @note 座標は「ステージローカル（傾き0のときのワールド座標）」を正とし、
 *       毎フレーム StageLocalToWorld() で実際のワールド座標を導出する。
 *       挙動を書くときは anchorLocal_ を動かすこと。
 *
 * @note コライダーは **トリガー登録のみ**。エンジン側の押し出しは使わない。
 *       理由は EnemyManager::Initialize() のコメントを参照。
 */
class EnemyBase
{
public:
    /// @brief 強さ -> 見た目スケール倍率 を決める関数。差し替え可能
    using ScaleFromStrengthFunc = std::function<float(int)>;

    /// @brief 既定の変換（strength 1 で 0.70、以降ゆるやかに大きくなる）
    static float DefaultScaleFromStrength(int strength);

    /// @brief 全個体の既定変換を差し替える（個別設定がない敵に適用される）
    static void SetDefaultScaleFromStrength(ScaleFromStrengthFunc func);
    static const ScaleFromStrengthFunc& GetDefaultScaleFromStrengthFunc();

    /**
     * @brief ステージローカル（傾き0のときの座標）-> ワールド座標
     * @param local ステージローカル座標
     * @param stageTilt ステージ傾斜
     * @param pivot 傾斜の回転中心（プレイヤー XZ）
     * @note GamePlayScene が groundPlane_ に掛けている
     *       「ピボット中心の Rx(pitch) * Rz(-roll) 回転」と同じ変換
     */
    static Vector3 StageLocalToWorld(const Vector3& local, const Vector2& stageTilt, const Vector2& pivot);

    /// @brief ワールド座標 -> ステージローカル（StageLocalToWorld の逆変換）
    static Vector3 StageWorldToLocal(const Vector3& world, const Vector2& stageTilt, const Vector2& pivot);

public:
    EnemyBase() = default;
    virtual ~EnemyBase();

    /**
     * @brief 初期化
     * @param object3dCom 3D描画コンポーネント
     * @param camera カメラ
     * @param stageLocalPos 配置座標（ステージが水平なときのワールド座標と同じ）
     * @param strength 強さ。プレイヤーの塊サイズと比較される
     */
    void Initialize(Object3dCom* object3dCom, Camera* camera, const Vector3& stageLocalPos, int strength);

    void Update(const EnemyUpdateContext& ctx);
    void Draw(const RenderContext& ctx);
    void Finalize();

    // --- 強さ ---
    int GetStrength() const { return strength_; }
    void SetStrength(int strength);
    void SetScaleFromStrength(ScaleFromStrengthFunc func); //!< この個体だけ差し替え

    // --- 状態 ---
    bool IsDead() const { return isDead_; }

    /**
     * @brief 挙動を凍結する（配置エディタ用）
     * @note true の間は UpdateBehavior() を呼ばない。
     *       地面追従・姿勢・アニメーションは動いたままなので、
     *       エディタ上でも実物と同じ見た目で置き場所を確認できる
     */
    void SetFrozen(bool frozen) { isFrozen_ = frozen; }
    bool IsFrozen() const { return isFrozen_; }

    /// @brief 次のフレームに床へ即吸着させる（エディタでドラッグ移動したあと用）
    void RequestGroundSnap() { needsGroundSnap_ = true; }

    void Defeat();                                    //!< 撃破（即消滅）
    void ApplyPush(const Vector3& worldDelta, const Vector2& stageTilt, const Vector2& pivot);

    // --- 座標 ---
    const Vector3& GetPosition() const { return position_; }
    const Vector3& GetStageLocalPosition() const { return anchorLocal_; }
    void SetStageLocalPosition(const Vector3& p) { anchorLocal_ = p; }
    const Vector3& GetScale() const { return scale_; }
    float GetGroundOffset() const { return groundOffset_; }

    // --- 当たり判定 ---
    EnemyCollision::EnemyBody MakeHitBody() const;
    BoxCollider* GetCollider() const { return collider_.get(); }

    void SetHitShape(EnemyCollision::HitShape shape) { hitShape_ = shape; RefreshCollider(); }
    EnemyCollision::HitShape GetHitShape() const { return hitShape_; }

    /// @brief ヒットボックスの大きさ（モデルローカル単位。実寸は scale_ 倍される）
    void SetHitRadiusRatio(float r) { hitRadiusRatio_ = r; RefreshCollider(); }
    float GetHitRadiusRatio() const { return hitRadiusRatio_; }
    void SetHitHalfRatio(const Vector3& r) { hitHalfRatio_ = r; RefreshCollider(); }
    const Vector3& GetHitHalfRatio() const { return hitHalfRatio_; }
    void SetHitOffsetRatio(float y) { hitOffsetRatio_ = y; RefreshCollider(); }
    float GetHitOffsetRatio() const { return hitOffsetRatio_; }

    /// @brief ヒットボックス中心のワールド座標（モデル原点は足元にあるので少し持ち上げる）
    Vector3 GetHitCenter() const;

    /// @brief デバッグ表示用のヒットボックス全長
    Vector3 GetHitBoxFullSize() const;

    /**
     * @brief GetModelSpec() を読み直して、スケール・当たり判定の設定を反映し直す
     * @note モデルの再読み込みはしない。ImGui で設定テーブルをいじったあとに呼ぶ用
     */
    void RefreshFromSpec();

    // --- アニメーション ---
    bool IsAnimated() const { return isAnimated_; }
    EnemyAnimator& GetAnimator() { return animator_; }
    const EnemyAnimator& GetAnimator() const { return animator_; }

    virtual EnemyType GetType() const = 0;
    virtual const char* GetTypeName() const = 0;

protected:
    /// @brief 派生が指定するモデル・当たり判定の既定値
    struct ModelSpec
    {
        std::string directory;                      //!< 例 "Resources/Enemy/Slime"
        std::string fileName;                       //!< 例 "slime.gltf"
        float modelScale = 1.0f;                    //!< モデル固有の基準スケール
        float groundOffsetRatio = 0.0f;             //!< モデル原点を地面からどれだけ持ち上げるか（モデルローカル単位）
        float hitRadiusRatio = 0.7f;                //!< Sphere 半径（モデルローカル単位）
        Vector3 hitHalfRatio{ 0.6f, 1.0f, 0.6f };   //!< AABB 半径（モデルローカル単位）
        float hitOffsetRatio = 0.5f;                //!< ヒットボックス中心の高さ（モデルローカル単位）
        EnemyCollision::HitShape hitShape = EnemyCollision::HitShape::Sphere;
        bool isPushable = false;                    //!< プレイヤーに押されて動くか
        bool useAnimation = true;                   //!< スキニングアニメーションを使うか
        Vector4 tintColor{ 1.0f, 1.0f, 1.0f, 1.0f };
    };

    virtual ModelSpec GetModelSpec() const = 0;

    /// @brief 初期化の最後に呼ばれる（派生の初期値セット用）
    virtual void OnInitialized() {}

    /// @brief 挙動の更新。anchorLocal_ を動かすと敵が移動する
    virtual void UpdateBehavior(const EnemyUpdateContext& ctx) { (void)ctx; }

    /// @brief 描画スケール。ぷにぷに演出などを掛けたいとき用（当たり判定には影響しない）
    virtual Vector3 GetRenderScale() const { return scale_; }

    /// @brief 描画だけ上下させたいとき用（ホップ演出。当たり判定には影響しない）
    virtual float GetVisualOffsetY() const { return 0.0f; }

    void RefreshCollider();

private:
    /// @brief 強さとスケールの再計算。コライダー未生成の初期化中は refreshCollider = false で呼ぶ
    void SetStrengthInternal(int strength, bool refreshCollider);

protected:
    Object3dCom* object3dCom_ = nullptr;
    Camera* camera_ = nullptr;

    // アニメーション付きの敵はプール（EnemyAnimation.h）から借りるので所有しない。
    // 静的メッシュのときだけ ownedObject_ が実体を持つ
    Object3d* object3d_ = nullptr;
    std::unique_ptr<Object3d> ownedObject_;
    const EnemyAnimationAsset* animAsset_ = nullptr;
    Object3d::ModelData modelData_;

    Vector3 anchorLocal_{ 0.0f, 0.0f, 0.0f }; //!< ステージローカル座標（挙動はここを動かす）
    Vector3 position_{ 0.0f, 0.0f, 0.0f };    //!< 実際のワールド座標（毎フレーム導出）
    Vector3 rotation_{ 0.0f, 0.0f, 0.0f };
    Vector3 scale_{ 1.0f, 1.0f, 1.0f };
    Vector3 groundNormal_{ 0.0f, 1.0f, 0.0f };

    float yaw_ = 0.0f;             //!< 向き（プレイヤー方向へ補間）
    float modelScale_ = 1.0f;      //!< モデル固有の基準スケール
    float groundOffset_ = 0.0f;    //!< モデル原点の地面からの高さ（ワールド単位）
    float groundOffsetRatio_ = 0.0f;

    int strength_ = 1;
    bool isDead_ = false;
    bool isPushable_ = false;
    bool needsGroundSnap_ = true;  //!< 初回だけ最上段の床へ即吸着する
    bool isFrozen_ = false;        //!< true の間は挙動を止める（配置エディタ用）
    bool isAnimated_ = false;
    float lifeTime_ = 0.0f;        //!< 生存時間（演出の位相に使う）

    EnemyCollision::HitShape hitShape_ = EnemyCollision::HitShape::Sphere;
    float hitRadiusRatio_ = 0.7f;
    Vector3 hitHalfRatio_{ 0.6f, 1.0f, 0.6f };
    float hitOffsetRatio_ = 0.5f;

    ScaleFromStrengthFunc scaleFunc_; //!< 空なら既定関数が使われる
    EnemyAnimator animator_;
    std::unique_ptr<BoxCollider> collider_;
};
