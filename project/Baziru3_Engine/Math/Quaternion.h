#pragma once
#include "Vector.h"
#include <cmath>

struct Quaternion
{
    float x, y, z, w; // メモリレイアウトは float4 (x,y,z,w)

    Quaternion() noexcept : x(0), y(0), z(0), w(1) {}
    Quaternion(float _x, float _y, float _z, float _w) noexcept : x(_x), y(_y), z(_z), w(_w) {}

    static Quaternion FromAxisAngle(const Vector3& axis, float angleRad)
    {
        float length = std::sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
        Vector3 a = axis;
        if (length > 0.0f)
        {
            float invLength = 1.0f / length;
            a.x *= invLength;
            a.y *= invLength;
            a.z *= invLength;
        }
        float s = std::sin(angleRad * 0.5f);
        return Quaternion(a.x * s, a.y * s, a.z * s, std::cos(angleRad * 0.5f));
    }

    void Normalize()
    {
        float n = std::sqrt(x * x + y * y + z * z + w * w);
        if (n > 0.0f) { float inv = 1.0f / n; x *= inv; y *= inv; z *= inv; w *= inv; }
    }

    Quaternion Conjugate() const { return Quaternion(-x, -y, -z, w); }

    static float Dot(const Quaternion& a, const Quaternion& b) noexcept
    {
        return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    }

    Quaternion operator*(const Quaternion& r) const noexcept
    {
        return Quaternion(
            w * r.x + x * r.w + y * r.z - z * r.y,
            w * r.y - x * r.z + y * r.w + z * r.x,
            w * r.z + x * r.y - y * r.x + z * r.w,
            w * r.w - x * r.x - y * r.y - z * r.z
        );
    }

    Vector3 Rotate(const Vector3& v) const noexcept
    {
        // q * (v as quaternion) * q^-1
        Quaternion qv(v.x, v.y, v.z, 0.0f);
        Quaternion res = (*this) * qv * this->Conjugate();
        return Vector3(res.x, res.y, res.z);
    }

    static Quaternion Slerp(Quaternion a, Quaternion b, float t)
    {
        float cosTheta = Dot(a, b);
        if (cosTheta < 0.0f)
        { // 最短経路
            b.x = -b.x; b.y = -b.y; b.z = -b.z; b.w = -b.w;
            cosTheta = -cosTheta;
        }
        const float EPS = 1e-6f;
        if (cosTheta > 1.0f - EPS)
        { // ほぼ同じ -> LERP
            Quaternion out(
                a.x + t * (b.x - a.x),
                a.y + t * (b.y - a.y),
                a.z + t * (b.z - a.z),
                a.w + t * (b.w - a.w)
            );
            out.Normalize();
            return out;
        }
        float theta = std::acos(cosTheta);
        float invSin = 1.0f / std::sin(theta);
        float s0 = std::sin((1.0f - t) * theta) * invSin;
        float s1 = std::sin(t * theta) * invSin;
        Quaternion out(
            a.x * s0 + b.x * s1,
            a.y * s0 + b.y * s1,
            a.z * s0 + b.z * s1,
            a.w * s0 + b.w * s1
        );
        return out;
    }
};