#include "SlimePhysics.h"

namespace SlimePhysics
{
    static float sFriction = 1.3f; // スライム共通の地面摩擦係数（通常・合体・ミニオン共通）

    float GetFriction()
    {
        return sFriction;
    }

    void SetFriction(float friction)
    {
        sFriction = friction;
    }

    float CalculateGroundHeight(float x, float z, const Vector2& stageTilt, const Vector2& pivot)
    {
        float relX = x - pivot.x;
        float relZ = z - pivot.y;

        float cosRoll = std::cos(stageTilt.y);
        float safeCosRoll = (std::max)(cosRoll, 0.01f);

        return -std::tan(stageTilt.y) * relX - (std::tan(stageTilt.x) / safeCosRoll) * relZ;
    }

    float CalculateGroundedCenterY(float x, float z, const Vector2& stageTilt, float baseOffset, const Vector2& pivot)
    {
        float cosPitch = std::cos(stageTilt.x);
        float cosRoll = std::cos(stageTilt.y);
        float ny = (std::max)(cosPitch * cosRoll, 0.01f);
        float groundHeight = CalculateGroundHeight(x, z, stageTilt, pivot);

        return groundHeight + baseOffset / ny;
    }

    void UpdateDeformation(SlimeParamsCPU& params, const DeformInput& input)
    {
        float dt = (std::max)(input.deltaTime, 0.0001f);

        if (input.isGrounded)
        {
            // --- 接地中（板の上でのスライム挙動） ---
            Vector3 accel = {
                (input.velocity.x - input.prevVelocity.x) / dt,
                0.0f,
                (input.velocity.z - input.prevVelocity.z) / dt
            };

            float speedMag = std::sqrt(input.velocity.x * input.velocity.x + input.velocity.z * input.velocity.z);
            float tiltMag = std::sqrt(input.stageTilt.x * input.stageTilt.x + input.stageTilt.y * input.stageTilt.y);

            // 1. 板の傾斜による下り坂方向への内容物移動ベクトル（もっこり感を保つため適度に調整）
            float tiltFlowFactor = input.isMerged ? 0.9f : (0.65f * input.massScale);
            float targetFlowX = std::sin(input.stageTilt.y) * tiltFlowFactor + input.velocity.x * 0.012f;
            float targetFlowZ = std::sin(input.stageTilt.x) * tiltFlowFactor + input.velocity.z * 0.012f;

            // 2. 接地重力および傾斜による上下の潰れ（ほどよく平べったい弾力スクワッシュ）
            float sag = input.isMerged ? -0.12f : -0.09f;
            float targetSquashY = sag - (std::min)(tiltMag * 0.22f + speedMag * 0.015f, 0.18f);

            // 3. スムーズスプリング補間（流動と潰れの追従）
            params.squashStretch.x += (targetFlowX - params.squashStretch.x) * (std::min)(1.0f, dt * 10.0f);
            params.squashStretch.z += (targetFlowZ - params.squashStretch.z) * (std::min)(1.0f, dt * 10.0f);
            params.squashStretch.y += (targetSquashY - params.squashStretch.y) * (std::min)(1.0f, dt * 10.0f);
        }
        else
        {
            // --- 空中（投擲・自由飛翔中） ---
            // 板の傾斜影響を受けず、飛行速度と鉛直重力による進行方向への伸び・水滴変形
            float targetFlowX = input.velocity.x * 0.025f;
            float targetFlowZ = input.velocity.z * 0.025f;
            float targetSquashY = (std::min)((std::max)(input.velocity.y * 0.018f, -0.15f), 0.25f);

            params.squashStretch.x += (targetFlowX - params.squashStretch.x) * (std::min)(1.0f, dt * 12.0f);
            params.squashStretch.z += (targetFlowZ - params.squashStretch.z) * (std::min)(1.0f, dt * 12.0f);
            params.squashStretch.y += (targetSquashY - params.squashStretch.y) * (std::min)(1.0f, dt * 12.0f);
        }

        // 4. 移動速度および衝撃に連動した表面波打ち強度（静止時は完全にゼロにして不要な波打ちを停止）
        float totalSpeed = std::sqrt(input.velocity.x * input.velocity.x + input.velocity.y * input.velocity.y + input.velocity.z * input.velocity.z);
        float targetWobble = 0.0f;
        if (totalSpeed > 0.15f || params.impulseStrength > 0.02f)
        {
            targetWobble = (std::min)(0.20f, totalSpeed * 0.025f + params.impulseStrength * 0.5f);
        }
        params.wobbleStrength += (targetWobble - params.wobbleStrength) * (std::min)(1.0f, dt * 8.0f);
        if (params.wobbleStrength < 0.001f) params.wobbleStrength = 0.0f;
    }
}
