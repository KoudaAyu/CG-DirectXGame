#include "SlimePhysics.h"

namespace SlimePhysics
{
    float CalculateGroundHeight(float x, float z, const Vector2& stageTilt)
    {
        float cosPitch = std::cos(stageTilt.x);
        float cosRoll = std::cos(stageTilt.y);
        float safeCosRoll = (std::max)(cosRoll, 0.01f);

        return -std::tan(stageTilt.y) * x - (std::tan(stageTilt.x) / safeCosRoll) * z;
    }

    float CalculateGroundedCenterY(float x, float z, const Vector2& stageTilt, float baseOffset)
    {
        float cosPitch = std::cos(stageTilt.x);
        float cosRoll = std::cos(stageTilt.y);
        float ny = (std::max)(cosPitch * cosRoll, 0.01f);
        float groundHeight = CalculateGroundHeight(x, z, stageTilt);

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

            // 1. 板の傾斜による下り坂方向への内容物移動ベクトル（質量・合体状態に応じて増加）
            float tiltFlowFactor = input.isMerged ? 2.2f : (1.6f * input.massScale);
            float targetFlowX = std::sin(input.stageTilt.y) * tiltFlowFactor + input.velocity.x * 0.02f;
            float targetFlowZ = std::sin(input.stageTilt.x) * tiltFlowFactor + input.velocity.z * 0.02f;

            // 2. 接地重力および傾斜による上下の強い平坦化（傾けるほど平たく潰れる）
            float sag = input.isMerged ? -0.22f : -0.16f;
            float targetSquashY = sag - (std::min)(tiltMag * 0.45f + speedMag * 0.02f, 0.35f);

            // 3. スムーズスプリング補間（流動と潰れの追従）
            params.squashStretch.x += (targetFlowX - params.squashStretch.x) * (std::min)(1.0f, dt * 12.0f);
            params.squashStretch.z += (targetFlowZ - params.squashStretch.z) * (std::min)(1.0f, dt * 12.0f);
            params.squashStretch.y += (targetSquashY - params.squashStretch.y) * (std::min)(1.0f, dt * 12.0f);
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
    }
}
