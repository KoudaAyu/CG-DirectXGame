#include "SplineFollower.h"
#include <algorithm>
#include <cmath>

namespace BaziruEngine
{
    SplineFollower::SplineFollower()
        : path_(nullptr)
        , speed_(5.0f)
        , playMode_(SplinePlayMode::Loop)
    {
    }

    SplineFollower::SplineFollower(const SplinePath* path, float speed, SplinePlayMode mode)
        : path_(path)
        , speed_(speed)
        , playMode_(mode)
    {
        if (path_)
        {
            UpdateTransform();
        }
    }

    void SplineFollower::SetPath(const SplinePath* path)
    {
        path_ = path;
        currentDistance_ = 0.0f;
        moveDirection_ = 1;
        isFinished_ = false;
        lapCount_ = 0;
        if (path_)
        {
            UpdateTransform();
        }
    }

    void SplineFollower::Stop()
    {
        isPlaying_ = false;
        isPaused_ = false;
        currentDistance_ = 0.0f;
        moveDirection_ = 1;
        isFinished_ = false;
        if (path_)
        {
            UpdateTransform();
        }
    }

    void SplineFollower::Restart()
    {
        currentDistance_ = 0.0f;
        moveDirection_ = 1;
        isFinished_ = false;
        isPlaying_ = true;
        isPaused_ = false;
        if (path_)
        {
            UpdateTransform();
        }
    }

    void SplineFollower::SetProgress(float normalizedT)
    {
        if (!path_) return;
        float totalLen = path_->GetTotalLength();
        SetDistance(std::clamp(normalizedT, 0.0f, 1.0f) * totalLen);
    }

    void SplineFollower::SetDistance(float distance)
    {
        if (!path_) return;
        float totalLen = path_->GetTotalLength();
        if (totalLen <= 1e-6f)
        {
            currentDistance_ = 0.0f;
        }
        else
        {
            currentDistance_ = std::clamp(distance, 0.0f, totalLen);
        }
        UpdateTransform();
    }

    float SplineFollower::GetProgress() const
    {
        if (!path_) return 0.0f;
        float totalLen = path_->GetTotalLength();
        if (totalLen <= 1e-6f) return 0.0f;
        return std::clamp(currentDistance_ / totalLen, 0.0f, 1.0f);
    }

    void SplineFollower::Update(float deltaTime)
    {
        if (!path_ || !isPlaying_ || isPaused_ || isFinished_)
        {
            return;
        }

        float totalLen = path_->GetTotalLength();
        if (totalLen <= 1e-6f)
        {
            return;
        }

        float deltaDist = speed_ * deltaTime * static_cast<float>(moveDirection_);
        currentDistance_ += deltaDist;

        if (playMode_ == SplinePlayMode::Once)
        {
            if (currentDistance_ >= totalLen)
            {
                currentDistance_ = totalLen;
                isFinished_ = true;
                isPlaying_ = false;
                if (onFinished_) onFinished_();
            }
            else if (currentDistance_ < 0.0f)
            {
                currentDistance_ = 0.0f;
                isFinished_ = true;
                isPlaying_ = false;
                if (onFinished_) onFinished_();
            }
        }
        else if (playMode_ == SplinePlayMode::Loop)
        {
            while (currentDistance_ >= totalLen)
            {
                currentDistance_ -= totalLen;
                lapCount_++;
                if (onLap_) onLap_(lapCount_);
            }
            while (currentDistance_ < 0.0f)
            {
                currentDistance_ += totalLen;
                lapCount_++;
                if (onLap_) onLap_(lapCount_);
            }
        }
        else if (playMode_ == SplinePlayMode::PingPong)
        {
            if (currentDistance_ >= totalLen)
            {
                currentDistance_ = totalLen - (currentDistance_ - totalLen);
                moveDirection_ = -1;
                lapCount_++;
                if (onLap_) onLap_(lapCount_);
            }
            else if (currentDistance_ <= 0.0f)
            {
                currentDistance_ = -currentDistance_;
                moveDirection_ = 1;
                lapCount_++;
                if (onLap_) onLap_(lapCount_);
            }
            currentDistance_ = std::clamp(currentDistance_, 0.0f, totalLen);
        }

        UpdateTransform();
    }

    void SplineFollower::UpdateTransform()
    {
        if (!path_) return;

        currentSample_ = path_->EvaluateByDistance(currentDistance_);

        if (alignRotation_)
        {
            Vector3 forward = currentSample_.tangent;
            if (moveDirection_ < 0)
            {
                forward = { -forward.x, -forward.y, -forward.z };
            }
            currentRotationEuler_ = CalculateRotationEuler(forward, currentSample_.normal);
        }
    }

    Vector3 SplineFollower::CalculateRotationEuler(const Vector3& forward, const Vector3& /*up*/) const
    {
        float forwardLenSq = forward.x * forward.x + forward.z * forward.z;
        if (forwardLenSq < 1e-6f)
        {
            // 真上または真下を向いている場合
            float pitch = (forward.y > 0.0f) ? -1.5707963f : 1.5707963f; // -PI/2 or PI/2
            return { pitch, 0.0f, 0.0f };
        }

        // Yaw (Y軸回転)
        float yaw = std::atan2(forward.x, forward.z);

        // Pitch (X軸回転)
        float horizontalLen = std::sqrt(forwardLenSq);
        float pitch = std::atan2(-forward.y, horizontalLen);

        // Roll (Z軸回転)
        float roll = 0.0f;

        return { pitch, yaw, roll };
    }
}
