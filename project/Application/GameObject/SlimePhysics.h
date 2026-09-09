#pragma once

#include "Baziru3_Engine/Core/Base/Vector.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3d.h"
#include "Baziru3_Engine/Framework/Collision/MeshCollider.h"
#include <algorithm>
#include <cmath>

/// @brief スライム用GPU定数バッファのCPU側構造体（Slime.hlsli の SlimeParams と一致）
struct SlimeParamsCPU
{
    float time = 0.0f;
    float wobbleStrength = 0.12f;
    float wobbleFrequency = 4.0f;
    float impulseStrength = 0.0f;
    Vector3 squashStretch{ 0.0f, 0.0f, 0.0f };
    float padding1 = 0.0f;
    Vector4 baseColor{ 0.2f, 0.85f, 1.0f, 0.9f };
    float fresnelPower = 3.0f;
    float envReflection = 0.4f;
    float innerGlow = 0.4f;
    float specularShininess = 64.0f;
};

namespace SlimePhysics
{
    /// @brief 真下のレイキャストで見つかった「歩ける床」1枚分
    struct GroundLayer
    {
        float y = 0.0f;                        //!< 床のワールドY座標
        Vector3 normal{ 0.0f, 1.0f, 0.0f };    //!< 床のワールド法線
        int meshIndex = -1;                    //!< どの地形メッシュの床か（AddGroundMesh の登録順。-1 は不明）
    };

    /**
     * @brief 3D地面メッシュ（Object3d & MeshCollider）を1枚追加してポリゴン地形接地を有効化
     * @note 地形が複数メッシュに分割されている場合は、メッシュの数だけ呼ぶ。
     *       すべてのメッシュに同じピボット回転（ステージ傾斜）が掛かっている前提
     */
    void AddGroundMesh(Object3d* groundObject, MeshCollider* groundCollider);

    /**
     * @brief 登録済みの地面メッシュを全部捨ててから1枚だけ登録する（単一メッシュ用の従来API）
     */
    void SetGroundMesh(Object3d* groundObject, MeshCollider* groundCollider);

    /**
     * @brief 登録されたすべての地面メッシュを解除
     */
    void ClearGroundMeshes();

    /**
     * @brief 登録された地面メッシュを全て解除（ClearGroundMeshes のエイリアス）
     */
    void ClearGroundMesh();

    /// @brief 現在登録されている地面メッシュの枚数
    int GetGroundMeshCount();

    /**
     * @brief 登録済みの地面メッシュのうち、指定の Object3d が何番目かを返す
     * @return 登録されていなければ -1
     * @note GroundLayer::meshIndex と突き合わせて「いまどの地形パーツの上に居るか」を
     *       判定するために使う（ボス戦トリガー）。登録順は AddGroundMesh を呼んだ順
     */
    int FindGroundMeshIndex(const Object3d* groundObject);

    /**
     * @brief (x, z) の真下にある「歩ける床」を全部取得する（Y 降順）
     * @param x ワールドX座標
     * @param z ワールドZ座標
     * @param outLayers 結果の格納先（nullptr 可）
     * @param maxLayers outLayers の要素数
     * @return 見つかった床の総数。0 なら島の外、2以上ならそこは上下段が重なっている
     * @note 配置エディタの「遮蔽物のあるところは配置禁止」判定に使う。
     *       ステージ傾斜は考慮しない（傾き0の状態で問い合わせること）
     */
    int QueryGroundLayers(float x, float z, GroundLayer* outLayers, int maxLayers);

    /**
     * @brief 登録された地形メッシュ全体を包むワールドAABBを取得
     * @return 地形が1枚も登録されていなければ false
     * @note 地形の広さを表す定数がどこにも無いので、配置範囲やカメラの初期値はここから決める
     */
    bool GetGroundWorldBounds(Vector3& outMin, Vector3& outMax);

    /// @brief currentY を無視して最上面の地面高さを検索する際の特殊値
    constexpr float kIgnoreCurrentY = -99999.0f;

    /**
     * @brief 地面メッシュまたは傾斜面上の厳密な高さを算出
     * @param x ワールドX座標
     * @param z ワールドZ座標
     * @param stageTilt ステージの傾斜角 (x: ピッチ, y: ロール)
     * @param pivot 傾斜の回転中心（自機位置など、デフォルト: 0, 0）
     * @return 傾斜面・3D地形メッシュ上の正確なY座標
     */
    float CalculateGroundHeight(float x, float z, const Vector2& stageTilt, const Vector2& pivot = { 0.0f, 0.0f });

    /**
     * @brief 地面メッシュまたは傾斜面上の厳密な高さを算出（現在のY座標を基準にレイキャストを行い、有無および法線を判定）
     */
    float CalculateGroundHeightEx(float x, float z, float currentY, const Vector2& stageTilt, bool* outHasGround = nullptr, Vector3* outNormal = nullptr, const Vector2& pivot = { 0.0f, 0.0f }, bool isGrounded = true, float baseOffset = 0.0f);
    inline float CalculateGroundHeightEx(float x, float z, float currentY, const Vector2& stageTilt, bool* outHasGround, const Vector2& pivot)
    {
        return CalculateGroundHeightEx(x, z, currentY, stageTilt, outHasGround, nullptr, pivot, true, 0.0f);
    }

    /**
     * @brief 地面メッシュまたは傾斜面上に乗るスライムの厳密な接地中心Y座標を算出
     * @param x ワールドX座標
     * @param z ワールドZ座標
     * @param stageTilt ステージの傾斜角
     * @param baseOffset スライム底面から中心までの高さオフセット
     * @param pivot 傾斜の回転中心（自機位置など、デフォルト: 0, 0）
     * @return スライムの中心Y座標
     */
    float CalculateGroundedCenterY(float x, float z, const Vector2& stageTilt, float baseOffset, const Vector2& pivot = { 0.0f, 0.0f });

    /**
     * @brief 接地中心Y座標を算出（落下・空中判定および法線出力フラグ付き）
     */
    float CalculateGroundedCenterYEx(float x, float z, float currentY, const Vector2& stageTilt, float baseOffset, bool* outHasGround = nullptr, Vector3* outNormal = nullptr, const Vector2& pivot = { 0.0f, 0.0f }, bool isGrounded = true);
    inline float CalculateGroundedCenterYEx(float x, float z, float currentY, const Vector2& stageTilt, float baseOffset, bool* outHasGround, const Vector2& pivot, bool isGrounded = true)
    {
        return CalculateGroundedCenterYEx(x, z, currentY, stageTilt, baseOffset, outHasGround, nullptr, pivot, isGrounded);
    }

    /**
     * @brief 接地地点の法線ベクトルを取得
     */
    Vector3 GetGroundNormal(float x, float z, const Vector2& stageTilt, const Vector2& pivot = { 0.0f, 0.0f });

    /**
     * @brief 地形メッシュの壁・垂直面との水平押し出し衝突判定
     * @param[in,out] position オブジェクトのワールド座標（めり込み分が押し出される）
     * @param[in,out] velocity オブジェクトの移動速度（壁向き成分が相殺される）
     * @param radius オブジェクトの衝突半径
     * @param heightOffset 判定中心の高さオフセット（通常 0.0f）
     * @return 壁に衝突して押し出しが発生した場合は true
     */
    bool ResolveWallCollision(Vector3& position, Vector3& velocity, float radius, float heightOffset = 0.0f, const Vector3* prevPos = nullptr);

    /**
     * @brief 「ここより下に落ちたら奈落」とみなすワールドY座標
     * @param margin 地形の最下端からさらに何m下を奈落とみなすか
     * @return 地形が登録されていれば「地形AABBの最下端 - margin」、未登録なら -12.0f
     * @note ここを定数で決め打ちすると、上下2段の地形に差し替えた瞬間に
     *       「下段に乗っただけで奈落判定されて死ぬ」ようになる。
     *       実際 startLand は下段が y = -12.9 付近にあり、
     *       決め打ちの -12.0f だと乗った瞬間に全滅していた
     */
    float GetVoidY(float margin = 8.0f);

    /**
     * @brief 真上方向へのレイキャストにより天井のY座標を探索
     * @param x ワールドX座標
     * @param z ワールドZ座標
     * @param startY 探索開始のY座標（通常は床面またはスライム足元）
     * @param maxSearchDist 最大探索距離
     * @param[out] outCeilingY 見つかった天井のワールドY座標
     * @return 天井が見つかった場合 true
     */
    bool FindCeilingY(float x, float z, float startY, float maxSearchDist, float& outCeilingY);

    /**
     * @brief スライム変形計算用の入力パラメータ構造体
     */
    struct DeformInput
    {
        Vector3 velocity = { 0.0f, 0.0f, 0.0f };       //!< 現在の移動速度
        Vector3 prevVelocity = { 0.0f, 0.0f, 0.0f };   //!< 前フレームの移動速度（加速度算出用）
        Vector2 stageTilt = { 0.0f, 0.0f };          //!< ステージ傾斜 (x: pitch, y: roll)
        float deltaTime = 0.0166f;                    //!< デルタタイム
        bool isGrounded = true;                       //!< 板に接地しているか（空中ならfalse）
        bool isMerged = false;                        //!< 合体巨大化状態か
        float massScale = 1.0f;                       //!< 質量・スケール係数
    };

    /**
     * @brief 速度・加速度・板の傾斜・接地状態に基づいてスライムの動的変形（squashStretch）を更新
     * @param[in,out] params スライム定数バッファパラメータ（squashStretch が更新される）
     * @param[in] input 変形計算入力
     */
    void UpdateDeformation(SlimeParamsCPU& params, const DeformInput& input);

    /**
     * @brief 全スライム共通の地面摩擦係数を取得
     */
    float GetFriction();

    /**
     * @brief 全スライム共通の地面摩擦係数を設定
     */
    void SetFriction(float friction);

    /**
     * @brief ロコロコの大きさ（1-10）に応じたスライムカラーを取得
     * 小（1-2）: 青, 中（3-7）: 黄色, 大（8-10以上）: 赤
     */
    inline Vector4 GetColorBySize(int size)
    {
        if (size <= 2)
        {
            return { 0.2f, 0.55f, 1.0f, 0.90f }; // 小 (1-2): 青
        }
        else if (size <= 7)
        {
            return { 1.0f, 0.90f, 0.15f, 0.92f }; // 中 (3-7): 黄色
        }
        else
        {
            return { 1.0f, 0.25f, 0.20f, 0.92f }; // 大 (8-10以上): 赤
        }
    }
}
