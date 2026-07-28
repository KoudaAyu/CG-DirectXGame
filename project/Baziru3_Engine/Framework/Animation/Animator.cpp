#include "Animator.h"

#include "Skeleton/Skeleton.h"

#include <cmath>
#include <algorithm>
#include "NodeAnimation/NodeAnimation.h"
#include "../Base/Vector.h"
#include "../Math/Quaternion.h"
#include "../Base/Matrix4x4.h"
#include "../Animation/Keyframe/Keyframe.h"
#include "AnimationUtils.h"

void Animator::SetAnimation(const Animation* anim)
{
    PlayAnimation(anim, 0.0f);
}

void Animator::PlayAnimation(const Animation* newAnim, float transitionTime)
{
    if (!newAnim)
    {
        return;
    }

    // すでに再生中または遷移ターゲットとして設定されている場合はブレンド処理をリセットせずに継続
    if (newAnim == animation_)
    {
        return;
    }

    if (transitionTime > 0.0f && animation_ != nullptr)
    {
        prevAnimation_ = animation_;
        prevAnimationTime_ = animationTime_;
        animation_ = newAnim;
        animationTime_ = 0.0f;
        transitionDuration_ = transitionTime;
        transitionTimer_ = 0.0f;
        isBlending_ = true;
    }
    else
    {
        animation_ = newAnim;
        animationTime_ = 0.0f;
        prevAnimation_ = nullptr;
        prevAnimationTime_ = 0.0f;
        isBlending_ = false;
    }
    localMatrices_.clear();
}

void Animator::SetTime(float time)
{
    if (!animation_) return;

    animationTime_ = time;
    if (animation_->duration > 0.0f)
    {
        animationTime_ = std::fmod(animationTime_, animation_->duration);
        if (animationTime_ < 0.0f) animationTime_ += animation_->duration;
    }

    for (const auto& kv : animation_->nodeAnimations)
    {
        const std::string& nodeName = kv.first;
        const NodeAnimation& nodeAnim = kv.second;

        Vector3 translate = CalculateValue(nodeAnim.translate.keyframes, animationTime_);
        Quaternion rotate = CalculateValue(nodeAnim.rotate.keyframes, animationTime_);
        Vector3 scale = CalculateValue(nodeAnim.scale.keyframes, animationTime_);

        Matrix4x4 local = MakeAffineMatrix(scale, rotate, translate);
        localMatrices_[nodeName] = local;
    }
}

void Animator::StepFrame(float frameTime)
{
    SetTime(animationTime_ + frameTime);
}

void Animator::Update(float dt)
{
    if (!animation_) return;

    if (!isPaused_)
    {
        animationTime_ += dt * playbackSpeed_;
        if (animation_->duration > 0.0f)
        {
            // wrap time into [0, duration)
            animationTime_ = std::fmod(animationTime_, animation_->duration);
            if (animationTime_ < 0.0f) animationTime_ += animation_->duration;
        }

        if (isBlending_ && prevAnimation_)
        {
            prevAnimationTime_ += dt * playbackSpeed_;
            if (prevAnimation_->duration > 0.0f)
            {
                prevAnimationTime_ = std::fmod(prevAnimationTime_, prevAnimation_->duration);
                if (prevAnimationTime_ < 0.0f) prevAnimationTime_ += prevAnimation_->duration;
            }

            transitionTimer_ += dt;
            if (transitionTimer_ >= transitionDuration_)
            {
                isBlending_ = false;
                prevAnimation_ = nullptr;
                prevAnimationTime_ = 0.0f;
            }
        }
    }

    // For each node, sample translate/rotate/scale and make local matrix
    for (const auto& kv : animation_->nodeAnimations)
    {
        const std::string& nodeName = kv.first;
        const NodeAnimation& nodeAnim = kv.second;

        Vector3 translate = CalculateValue(nodeAnim.translate.keyframes, animationTime_);
        Quaternion rotate = CalculateValue(nodeAnim.rotate.keyframes, animationTime_);
        Vector3 scale = CalculateValue(nodeAnim.scale.keyframes, animationTime_);

        Matrix4x4 local = MakeAffineMatrix(scale, rotate, translate);
        localMatrices_[nodeName] = local;
    }
}

void Animator::ApplyTo(Skeleton& skeleton) const
{
    if (!animation_)
    {
        return;
    }

    if (isBlending_ && prevAnimation_)
    {
        float factor = std::clamp(transitionTimer_ / transitionDuration_, 0.0f, 1.0f);
        skeleton.ApplyAnimationBlend(*prevAnimation_, prevAnimationTime_, *animation_, animationTime_, factor);
    }
    else
    {
        skeleton.ApplyAnimation(*animation_, animationTime_);
    }
}
