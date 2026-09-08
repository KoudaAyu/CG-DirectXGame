#pragma once

#include "Baziru3_Engine/Core/Base/Vector.h"

/**
 * @brief プレイヤーの塊（スライム）と敵のヒットボックスの判定・解決
 *
 * Application/GameObject/SlimeCollision.h を下敷きにした敵専用版。
 * SlimeCollision がスライム同士（対等な押し合い）を扱うのに対し、
 * こちらは「片方が敵の固定ヒットボックス」で「強さ比較で結果が変わる」点が違う。
 *
 * 押し出しは床面法線に射影した平面内で行うので、
 * ステージが傾いていても上下方向にガタつかない（SlimeCollision と同じ方針）。
 */
namespace EnemyCollision
{
    /// @brief 敵のヒットボックス形状
    enum class HitShape
    {
        Sphere, //!< 球（デフォルト）
        AABB    //!< 軸平行ボックス（背の高い花などはこちらが素直）
    };

    /// @brief 衝突の結末
    enum class HitOutcome
    {
        None,          //!< 接触していない
        EnemyDefeated, //!< プレイヤーの塊のほうが強い → 敵が倒される
        PlayerBounced, //!< 敵のほうが強い → プレイヤーが跳ね飛ばされる
        Standoff       //!< 同じ強さ → 押し合うだけ
    };

    /// @brief 判定に使うプレイヤー側スライムの情報
    struct SlimeBody
    {
        Vector3 position{ 0.0f, 0.0f, 0.0f };      //!< 中心ワールド座標（解決時に更新される）
        Vector3 scale{ 1.0f, 1.0f, 1.0f };         //!< 3Dスケール
        Vector3 squashStretch{ 0.0f, 0.0f, 0.0f }; //!< 変形（横幅の微増減に反映）
        float baseRadius = 1.0f;                   //!< 基本幾何半径
        int strength = 1;                          //!< 強さ（＝含まれる最小単位スライムの数）
    };

    /// @brief 判定に使う敵側ヒットボックスの情報
    struct EnemyBody
    {
        Vector3 position{ 0.0f, 0.0f, 0.0f };      //!< ヒットボックス中心のワールド座標
        HitShape shape = HitShape::Sphere;         //!< 形状
        float radius = 0.5f;                       //!< Sphere 用の半径
        Vector3 halfExtents{ 0.5f, 0.5f, 0.5f };   //!< AABB 用の半径（各軸の半分の長さ）
        int strength = 1;                          //!< 強さ
        bool isPushable = false;                   //!< true なら押し出しを敵側にも分配する
    };

    /// @brief 衝突結果
    struct HitResult
    {
        bool hit = false;                        //!< 接触したか
        HitOutcome outcome = HitOutcome::None;   //!< 結末
        Vector3 pushDir{ 0.0f, 0.0f, 0.0f };     //!< 敵 → プレイヤー方向（正規化・床面内）
        float depth = 0.0f;                      //!< めり込み深さ
        float impulse = 0.0f;                    //!< ぷるぷる波紋の強さ（0.05〜1.0）
        Vector3 enemyPush{ 0.0f, 0.0f, 0.0f };   //!< 敵側に適用すべき押し出し量（isPushable時のみ）
    };

    /**
     * @brief スライムの見た目の外形にほぼ一致する実効半径を求める
     * @param scale 3Dスケール
     * @param squashStretch 変形パラメータ（y が潰れ量）
     * @param baseRadius 基本幾何半径
     * @return 実効半径
     * @note SlimeCollision::CalculateEffectiveRadius と同じ式にそろえてある
     */
    float CalcSlimeRadius(const Vector3& scale, const Vector3& squashStretch, float baseRadius = 1.0f);

    /**
     * @brief 球 vs 球（床面内で判定）
     * @param centerA 球Aの中心
     * @param radiusA 球Aの半径
     * @param centerB 球Bの中心
     * @param radiusB 球Bの半径
     * @param planeNormal 床面法線（この方向の差分は無視される）
     * @param outPushDir B → A 方向の正規化ベクトル（出力）
     * @param outDepth めり込み深さ（出力）
     * @return 接触していれば true
     */
    bool CheckSphereSphere(const Vector3& centerA, float radiusA,
                           const Vector3& centerB, float radiusB,
                           const Vector3& planeNormal,
                           Vector3& outPushDir, float& outDepth);

    /**
     * @brief 球 vs AABB（最近点法。押し出し方向は床面内に射影する）
     * @param sphereCenter 球の中心
     * @param sphereRadius 球の半径
     * @param boxCenter ボックス中心
     * @param halfExtents ボックスの各軸半径
     * @param planeNormal 床面法線
     * @param outPushDir ボックス → 球 方向の正規化ベクトル（出力）
     * @param outDepth めり込み深さ（出力）
     * @return 接触していれば true
     */
    bool CheckSphereAABB(const Vector3& sphereCenter, float sphereRadius,
                         const Vector3& boxCenter, const Vector3& halfExtents,
                         const Vector3& planeNormal,
                         Vector3& outPushDir, float& outDepth);

    /**
     * @brief スライムと敵ヒットボックスの接触判定（形状に応じて上の2つに振り分ける）
     * @param slime プレイヤー側スライム
     * @param enemy 敵側ヒットボックス
     * @param planeNormal 床面法線
     * @param outPushDir 敵 → プレイヤー方向（出力）
     * @param outDepth めり込み深さ（出力）
     * @return 接触していれば true
     */
    bool Check(const SlimeBody& slime, const EnemyBody& enemy,
               const Vector3& planeNormal,
               Vector3& outPushDir, float& outDepth);

    /**
     * @brief プレイヤーの塊と敵の衝突を解決する
     *
     * - プレイヤーのほうが強い → 敵を倒す判定を返す（押し出しはしない。塊はそのまま突き進む）
     * - 敵のほうが強い        → プレイヤーを押し出して bounceSpeed で弾き飛ばす
     * - 同じ強さ              → 押し出すだけ
     *
     * @param[in,out] slime プレイヤー側スライム（position が押し出し補正される）
     * @param[in,out] playerVelocity プレイヤーの速度（跳ね返り時に上書きされる）
     * @param[in] enemy 敵側ヒットボックス
     * @param[in] bounceSpeed 跳ね飛ばし初速（m/s）
     * @param[in] planeNormal 床面法線
     * @return 衝突結果
     */
    HitResult ResolvePlayerVsEnemy(SlimeBody& slime, Vector3& playerVelocity,
                                   const EnemyBody& enemy,
                                   float bounceSpeed = 12.0f,
                                   const Vector3& planeNormal = { 0.0f, 1.0f, 0.0f });

    /**
     * @brief 弾（小さな球）とスライムの接触判定
     * @param bulletPos 弾の中心
     * @param bulletRadius 弾の半径
     * @param slime プレイヤー側スライム
     * @param outPushDir 弾 → プレイヤー方向（出力・水平化済み）
     * @return 接触していれば true
     */
    bool CheckBulletVsSlime(const Vector3& bulletPos, float bulletRadius,
                            const SlimeBody& slime, Vector3& outPushDir);
}
