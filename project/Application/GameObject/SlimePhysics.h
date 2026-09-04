#pragma once

#include "Baziru3_Engine/Core/Base/Vector.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3d.h"
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
    /**
     * @brief 傾斜面（地面プレーン）上の厳密な高さを算出
     * @param x ワールドX座標
     * @param z ワールドZ座標
     * @param stageTilt ステージの傾斜角 (x: ピッチ, y: ロール)
     * @param pivot 傾斜の回転中心（自機位置など、デフォルト: 0, 0）
     * @return 傾斜面上の正確なY座標
     */
    float CalculateGroundHeight(float x, float z, const Vector2& stageTilt, const Vector2& pivot = { 0.0f, 0.0f });

    /**
     * @brief 傾斜面上に乗るスライムの厳密な接地中心Y座標を算出
     * @param x ワールドX座標
     * @param z ワールドZ座標
     * @param stageTilt ステージの傾斜角
     * @param baseOffset スライム底面から中心までの高さオフセット
     * @param pivot 傾斜の回転中心（自機位置など、デフォルト: 0, 0）
     * @return スライムの中心Y座標
     */
    float CalculateGroundedCenterY(float x, float z, const Vector2& stageTilt, float baseOffset, const Vector2& pivot = { 0.0f, 0.0f });

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
