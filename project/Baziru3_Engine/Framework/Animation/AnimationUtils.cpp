#include "AnimationUtils.h"
#include <cassert>

static Vector3 LerpVec3(const Vector3& a, const Vector3& b, float t)
{
    return a * (1.0f - t) + b * t;
}

Vector3 CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time)
{
    if (keyframes.empty()) return {0.0f,0.0f,0.0f};
    if (keyframes.size() == 1) return keyframes.front().value;

    // if before first key
    if (time <= keyframes.front().time) return keyframes.front().value;

    // search interval
    for (size_t i = 0; i + 1 < keyframes.size(); ++i)
    {
        const auto& k0 = keyframes[i];
        const auto& k1 = keyframes[i+1];
        if (time >= k0.time && time <= k1.time)
        {
            float dt = k1.time - k0.time;
            if (dt <= 1e-6f) return k1.value;
            float alpha = (time - k0.time) / dt;
            if (alpha < 0.0f) alpha = 0.0f;
            if (alpha > 1.0f) alpha = 1.0f;
            return LerpVec3(k0.value, k1.value, alpha);
        }
    }

    // fallback: after last key
    return keyframes.back().value;
}

Quaternion CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time)
{
    if (keyframes.empty()) return Quaternion();
    if (keyframes.size() == 1) return keyframes.front().value;

    if (time <= keyframes.front().time) return keyframes.front().value;

    for (size_t i = 0; i + 1 < keyframes.size(); ++i)
    {
        const auto& k0 = keyframes[i];
        const auto& k1 = keyframes[i+1];
        if (time >= k0.time && time <= k1.time)
        {
            float dt = k1.time - k0.time;
            if (dt <= 1e-6f) return k1.value;
            float alpha = (time - k0.time) / dt;
            if (alpha < 0.0f) alpha = 0.0f;
            if (alpha > 1.0f) alpha = 1.0f;
            return Quaternion::Slerp(k0.value, k1.value, alpha);
        }
    }

    return keyframes.back().value;
}
