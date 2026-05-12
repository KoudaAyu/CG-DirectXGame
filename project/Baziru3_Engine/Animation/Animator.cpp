#include "Animator.h"

#include <cmath>
#include <algorithm>
#include "NodeAnimation/NodeAnimation.h"
#include "../Base/Vector.h"
#include "../Base/Quaternion.h"
#include "../Base/Matrix4x4.h"
#include "../Animation/Keyframe/Keyframe.h"
#include "AnimationUtils.h"

void Animator::SetAnimation(const Animation* anim)
{
    animation_ = anim;
    animationTime_ = 0.0f;
    localMatrices_.clear();
}

void Animator::Update(float dt)
{
    if (!animation_) return;

    animationTime_ += dt;
    if (animation_->duration > 0.0f)
    {
        // wrap time into [0, duration)
        animationTime_ = std::fmod(animationTime_, animation_->duration);
        if (animationTime_ < 0.0f) animationTime_ += animation_->duration;
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
