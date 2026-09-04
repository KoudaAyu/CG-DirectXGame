#include "SplinePath.h"
#include <algorithm>
#include <cassert>

namespace BaziruEngine
{
    namespace
    {
        inline float LengthSq(const Vector3& v)
        {
            return v.x * v.x + v.y * v.y + v.z * v.z;
        }

        inline float Length(const Vector3& v)
        {
            return std::sqrt(LengthSq(v));
        }

        inline Vector3 Normalize(const Vector3& v)
        {
            float len = Length(v);
            if (len > 1e-6f)
            {
                float inv = 1.0f / len;
                return { v.x * inv, v.y * inv, v.z * inv };
            }
            return { 0.0f, 0.0f, 1.0f };
        }

        inline Vector3 Cross(const Vector3& a, const Vector3& b)
        {
            return {
                a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x
            };
        }
    }

    void SplinePath::AddPoint(const Vector3& point)
    {
        controlPoints_.push_back(point);
        dirty_ = true;
    }

    void SplinePath::SetPoints(const std::vector<Vector3>& points)
    {
        controlPoints_ = points;
        dirty_ = true;
    }

    void SplinePath::SetPoint(size_t index, const Vector3& point)
    {
        if (index < controlPoints_.size())
        {
            controlPoints_[index] = point;
            dirty_ = true;
        }
    }

    void SplinePath::Clear()
    {
        controlPoints_.clear();
        distanceTable_.clear();
        totalLength_ = 0.0f;
        dirty_ = false;
    }

    void SplinePath::SetLoop(bool isLoop)
    {
        if (isLoop_ != isLoop)
        {
            isLoop_ = isLoop;
            dirty_ = true;
        }
    }

    void SplinePath::EnsureBuilt() const
    {
        if (dirty_)
        {
            const_cast<SplinePath*>(this)->BuildDistanceTable(samplesPerSegment_);
        }
    }

    float SplinePath::GetTotalLength() const
    {
        EnsureBuilt();
        return totalLength_;
    }

    void SplinePath::BuildDistanceTable(uint32_t samplesPerSegment)
    {
        samplesPerSegment_ = (samplesPerSegment < 2) ? 2 : samplesPerSegment;
        distanceTable_.clear();
        totalLength_ = 0.0f;
        dirty_ = false;

        size_t n = controlPoints_.size();
        if (n < 2)
        {
            if (n == 1)
            {
                distanceTable_.push_back({ 0.0f, 0.0f, controlPoints_[0] });
            }
            return;
        }

        size_t numSegments = isLoop_ ? n : (n - 1);
        size_t totalSamples = numSegments * samplesPerSegment_ + 1;
        distanceTable_.reserve(totalSamples);

        Vector3 prevPos = EvaluatePosition(0.0f);
        distanceTable_.push_back({ 0.0f, 0.0f, prevPos });

        float currentDist = 0.0f;
        float stepT = 1.0f / static_cast<float>(totalSamples - 1);

        for (size_t i = 1; i < totalSamples; ++i)
        {
            float t = static_cast<float>(i) * stepT;
            if (t > 1.0f) t = 1.0f;

            Vector3 currPos = EvaluatePosition(t);
            float d = Length(currPos - prevPos);
            currentDist += d;

            distanceTable_.push_back({ t, currentDist, currPos });
            prevPos = currPos;
        }

        totalLength_ = currentDist;
    }

    void SplinePath::GetSegmentControlPoints(float globalT, size_t& outSegment, float& outLocalT,
                                            Vector3& outP0, Vector3& outP1, Vector3& outP2, Vector3& outP3) const
    {
        size_t n = controlPoints_.size();
        if (n < 2)
        {
            Vector3 single = n == 1 ? controlPoints_[0] : Vector3{ 0, 0, 0 };
            outP0 = outP1 = outP2 = outP3 = single;
            outSegment = 0;
            outLocalT = 0.0f;
            return;
        }

        float clampedT = std::clamp(globalT, 0.0f, 1.0f);
        size_t numSegments = isLoop_ ? n : (n - 1);

        float scaledT = clampedT * static_cast<float>(numSegments);
        size_t seg = static_cast<size_t>(std::floor(scaledT));
        if (seg >= numSegments)
        {
            seg = numSegments - 1;
            outLocalT = 1.0f;
        }
        else
        {
            outLocalT = scaledT - static_cast<float>(seg);
        }

        outSegment = seg;

        if (isLoop_)
        {
            size_t i0 = (seg + n - 1) % n;
            size_t i1 = seg % n;
            size_t i2 = (seg + 1) % n;
            size_t i3 = (seg + 2) % n;

            outP0 = controlPoints_[i0];
            outP1 = controlPoints_[i1];
            outP2 = controlPoints_[i2];
            outP3 = controlPoints_[i3];
        }
        else
        {
            // Open spline: 外挿または境界クランプ
            size_t i1 = seg;
            size_t i2 = seg + 1;
            outP1 = controlPoints_[i1];
            outP2 = controlPoints_[i2];

            if (i1 == 0)
            {
                // P0 = P1 - (P2 - P1)
                outP0 = outP1 - (outP2 - outP1);
            }
            else
            {
                outP0 = controlPoints_[i1 - 1];
            }

            if (i2 + 1 >= n)
            {
                // P3 = P2 + (P2 - P1)
                outP3 = outP2 + (outP2 - outP1);
            }
            else
            {
                outP3 = controlPoints_[i2 + 1];
            }
        }
    }

    Vector3 SplinePath::EvaluatePosition(float t) const
    {
        size_t n = controlPoints_.size();
        if (n == 0) return { 0.0f, 0.0f, 0.0f };
        if (n == 1) return controlPoints_[0];

        size_t segment = 0;
        float localT = 0.0f;
        Vector3 p0, p1, p2, p3;
        GetSegmentControlPoints(t, segment, localT, p0, p1, p2, p3);

        switch (splineType_)
        {
        case SplineType::Linear:
            return Lerp(p1, p2, localT);
        case SplineType::BezierCubic:
            return BezierCubic(p0, p1, p2, p3, localT);
        case SplineType::CatmullRom:
        default:
            return CatmullRom(p0, p1, p2, p3, localT);
        }
    }

    Vector3 SplinePath::EvaluateTangent(float t) const
    {
        size_t n = controlPoints_.size();
        if (n < 2) return { 0.0f, 0.0f, 1.0f };

        size_t segment = 0;
        float localT = 0.0f;
        Vector3 p0, p1, p2, p3;
        GetSegmentControlPoints(t, segment, localT, p0, p1, p2, p3);

        Vector3 rawTangent;
        switch (splineType_)
        {
        case SplineType::Linear:
            rawTangent = p2 - p1;
            break;
        case SplineType::BezierCubic:
        {
            // 3次ベジェの微分
            float invT = 1.0f - localT;
            rawTangent = (p1 - p0) * (3.0f * invT * invT) +
                         (p2 - p1) * (6.0f * invT * localT) +
                         (p3 - p2) * (3.0f * localT * localT);
            break;
        }
        case SplineType::CatmullRom:
        default:
            rawTangent = CatmullRomDerivative(p0, p1, p2, p3, localT);
            break;
        }

        return Normalize(rawTangent);
    }

    SplineSample SplinePath::EvaluateByDistance(float distance) const
    {
        EnsureBuilt();

        SplineSample sample;
        if (distanceTable_.empty())
        {
            return sample;
        }

        if (distanceTable_.size() == 1 || totalLength_ <= 1e-6f)
        {
            sample.position = distanceTable_[0].position;
            sample.tangent = { 0.0f, 0.0f, 1.0f };
            sample.normal = { 0.0f, 1.0f, 0.0f };
            sample.distance = 0.0f;
            sample.t = 0.0f;
            return sample;
        }

        float targetDist = distance;
        if (isLoop_)
        {
            // ループ時は全長で剰余
            targetDist = std::fmod(targetDist, totalLength_);
            if (targetDist < 0.0f) targetDist += totalLength_;
        }
        else
        {
            targetDist = std::clamp(targetDist, 0.0f, totalLength_);
        }

        // バイナリサーチで対象区間を特定
        auto it = std::lower_bound(distanceTable_.begin(), distanceTable_.end(), targetDist,
            [](const DistanceSample& s, float val) {
                return s.distance < val;
            });

        if (it == distanceTable_.begin())
        {
            float t = distanceTable_.front().t;
            sample.position = distanceTable_.front().position;
            sample.distance = 0.0f;
            sample.t = t;
            sample.tangent = EvaluateTangent(t);
        }
        else if (it == distanceTable_.end())
        {
            float t = distanceTable_.back().t;
            sample.position = distanceTable_.back().position;
            sample.distance = totalLength_;
            sample.t = t;
            sample.tangent = EvaluateTangent(t);
        }
        else
        {
            const auto& s1 = *it;
            const auto& s0 = *(it - 1);

            float segmentDist = s1.distance - s0.distance;
            float ratio = (segmentDist > 1e-6f) ? ((targetDist - s0.distance) / segmentDist) : 0.0f;
            ratio = std::clamp(ratio, 0.0f, 1.0f);

            float t = s0.t + (s1.t - s0.t) * ratio;
            sample.position = EvaluatePosition(t);
            sample.distance = targetDist;
            sample.t = t;
            sample.tangent = EvaluateTangent(t);
        }

        // 接線から上方向法線ベクトル（Frenet-Serretライク / Up=(0,1,0)基準）を算出
        Vector3 worldUp = { 0.0f, 1.0f, 0.0f };
        if (std::abs(sample.tangent.y) > 0.99f)
        {
            worldUp = { 0.0f, 0.0f, 1.0f };
        }
        Vector3 right = Normalize(Cross(worldUp, sample.tangent));
        sample.normal = Normalize(Cross(sample.tangent, right));

        return sample;
    }

    SplineSample SplinePath::EvaluateNormalized(float normalizedT) const
    {
        EnsureBuilt();
        float dist = std::clamp(normalizedT, 0.0f, 1.0f) * totalLength_;
        return EvaluateByDistance(dist);
    }

    Vector3 SplinePath::CatmullRom(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t)
    {
        float t2 = t * t;
        float t3 = t2 * t;

        // Standard uniform Catmull-Rom formula:
        // P(t) = 0.5 * ( 2*p1 + (-p0 + p2)*t + (2*p0 - 5*p1 + 4*p2 - p3)*t^2 + (-p0 + 3*p1 - 3*p2 + p3)*t^3 )
        Vector3 result;
        result.x = 0.5f * (2.0f * p1.x +
            (-p0.x + p2.x) * t +
            (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 +
            (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3);

        result.y = 0.5f * (2.0f * p1.y +
            (-p0.y + p2.y) * t +
            (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2 +
            (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3);

        result.z = 0.5f * (2.0f * p1.z +
            (-p0.z + p2.z) * t +
            (2.0f * p0.z - 5.0f * p1.z + 4.0f * p2.z - p3.z) * t2 +
            (-p0.z + 3.0f * p1.z - 3.0f * p2.z + p3.z) * t3);

        return result;
    }

    Vector3 SplinePath::CatmullRomDerivative(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t)
    {
        float t2 = t * t;

        // Derivative of Catmull-Rom:
        // P'(t) = 0.5 * ( (-p0 + p2) + 2*(2*p0 - 5*p1 + 4*p2 - p3)*t + 3*(-p0 + 3*p1 - 3*p2 + p3)*t^2 )
        Vector3 result;
        result.x = 0.5f * ((-p0.x + p2.x) +
            2.0f * (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t +
            3.0f * (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t2);

        result.y = 0.5f * ((-p0.y + p2.y) +
            2.0f * (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t +
            3.0f * (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t2);

        result.z = 0.5f * ((-p0.z + p2.z) +
            2.0f * (2.0f * p0.z - 5.0f * p1.z + 4.0f * p2.z - p3.z) * t +
            3.0f * (-p0.z + 3.0f * p1.z - 3.0f * p2.z + p3.z) * t2);

        return result;
    }

    Vector3 SplinePath::BezierCubic(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t)
    {
        float invT = 1.0f - t;
        float invT2 = invT * invT;
        float invT3 = invT2 * invT;
        float t2 = t * t;
        float t3 = t2 * t;

        return p0 * invT3 +
               p1 * (3.0f * invT2 * t) +
               p2 * (3.0f * invT * t2) +
               p3 * t3;
    }

    Vector3 SplinePath::Lerp(const Vector3& a, const Vector3& b, float t)
    {
        return a + (b - a) * t;
    }
}
