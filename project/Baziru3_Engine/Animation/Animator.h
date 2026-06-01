#pragma once

#include <map>
#include <string>
#include <vector>

#include "AnimationData.h"
#include "Matrix4x4.h"

struct Skeleton;

class Animator
{
public:
    Animator() = default;

    // Set animation data to play (pointer is not owned)
    void SetAnimation(const Animation* anim);

    // Advance animation by dt seconds
    void Update(float dt);

    void ApplyTo(Skeleton& skeleton) const;

    bool HasAnimation() const { return animation_ != nullptr; }

    float GetTime() const { return animationTime_; }

    // Returns local transform matrices for each animated node (keyed by node name)
    const std::map<std::string, Matrix4x4>& GetLocalMatrices() const { return localMatrices_; }

private:
    const Animation* animation_ = nullptr;
    float animationTime_ = 0.0f;
    std::map<std::string, Matrix4x4> localMatrices_;
};
