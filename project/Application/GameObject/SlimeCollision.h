#pragma once

#include "Baziru3_Engine/Core/Base/Vector.h"
#include <vector>
#include <array>
#include <cstdint>

class Camera;

/// @brief 多重球を構成する単一の部分球
struct SlimeSubSphere
{
    Vector3 worldPos{ 0.0f, 0.0f, 0.0f }; //!< 部分球のワールド中心座標
    float radius = 0.0f;                   //!< 部分球の実効半径
};

/// @brief スライムの多重球（Multi-Sphere）形状構造体
struct SlimeMultiSphereShape
{
    static constexpr size_t kSubSphereCount = 7;
    std::array<SlimeSubSphere, kSubSphereCount> spheres; //!< 前方舌先(0)・前方基部(1)・中央コア(2)・左脇腹(3)・右脇腹(4)・後方基底(5)・頭部ドーム(6)
    float maxBoundingRadius = 0.0f;                       //!< ブロードフェーズ用包絡球半径
    Vector3 center = { 0.0f, 0.0f, 0.0f };               //!< スライムの中心ワールド座標
    Vector3 rotation = { 0.0f, 0.0f, 0.0f };             //!< スライムの姿勢回転 (pitch, yaw, roll)
    Vector3 flowDirLocal = { 0.0f, 0.0f, 1.0f };         //!< 流動方向ベクトル（ローカル）
};

namespace SlimeCollision
{
    /**
     * @brief スライムの洋梨シルエットおよび傾斜流動（Fluid Mass Shift / Squash）に100%一致する7連多重球を構築
     * @param centerPos スライムの原点ワールド座標
     * @param scale 3Dスケール（合体・拡大追従用）
     * @param squashStretch 傾斜・速度による流動および潰れベクトル (x: flowX, y: squashY, z: flowZ)
     * @param rotation スライム（床）の回転角 (pitch, yaw, roll)
     * @param baseRadius 基本幾何半径（デフォルト 1.0f）
     * @return 構築された多重球データ
     */
    SlimeMultiSphereShape BuildMultiSphere(const Vector3& centerPos,
                                           const Vector3& scale,
                                           const Vector3& squashStretch = { 0.0f, 0.0f, 0.0f },
                                           const Vector3& rotation = { 0.0f, 0.0f, 0.0f },
                                           float baseRadius = 1.0f);

    /**
     * @brief 多重球同士の3D交差判定と押し出しベクトルの算出
     * @param shapeA スライムAの多重球
     * @param shapeB スライムBの多重球
     * @param outPushDir AからBを押し出す正規化方向（出力）
     * @param outPushLen めり込み深さ（押し出し距離）（出力）
     * @return 接触している場合 true
     */
    bool CheckCollision(const SlimeMultiSphereShape& shapeA, const SlimeMultiSphereShape& shapeB,
                        Vector3& outPushDir, float& outPushLen);

    /**
     * @brief 多重球同士の衝突判定と位置解決（質量比に基づく押し出し適用）
     * @param posA スライムAの位置（更新される）
     * @param scaleA スライムAのスケール
     * @param squashA スライムAの変形・流動パラメータ (squashStretch)
     * @param weightA Aが受ける押し出し割合（0.0: 不動、0.5: 対等、1.0: 100%押し出し）
     * @param posB スライムBの位置（更新される）
     * @param scaleB スライムBのスケール
     * @param squashB スライムBの変形・流動パラメータ (squashStretch)
     * @param weightB Bが受ける押し出し割合
     * @param outImpulse 衝突による衝撃強度（ぷるぷる波紋用、出力）
     * @param rotA Aの回転
     * @param rotB Bの回転
     * @param planeNormal 床面の法線ベクトル（法線方向の無駄な浮き沈みを排除し面内分離を保証）
     * @param baseRadiusA Aの基本半径
     * @param baseRadiusB Bの基本半径
     * @return 衝突が発生した場合 true
     */
    bool ResolveCollision(Vector3& posA, const Vector3& scaleA, const Vector3& squashA, float weightA,
                          Vector3& posB, const Vector3& scaleB, const Vector3& squashB, float weightB,
                          float& outImpulse,
                          const Vector3& rotA = { 0.0f, 0.0f, 0.0f },
                          const Vector3& rotB = { 0.0f, 0.0f, 0.0f },
                          const Vector3& planeNormal = { 0.0f, 1.0f, 0.0f },
                          float baseRadiusA = 1.0f, float baseRadiusB = 1.0f);

    /**
     * @brief デバッグ用3Dワイヤーフレーム描画（ImGui使用）
     * @param shape 描画する多重球
     * @param camera 描画用カメラ
     * @param color ARGB / ABGR 32ビット色
     */
    void DrawDebugMultiSphere(const SlimeMultiSphereShape& shape, Camera* camera, uint32_t color = 0xFF00FF7F);
}
