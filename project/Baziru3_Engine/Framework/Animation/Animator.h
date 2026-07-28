#pragma once

#include <map>
#include <string>
#include <vector>

#include "AnimationData.h"
#include "../Base/Matrix4x4.h"

struct Skeleton;

class Animator
{
public:
    Animator() = default;

    // Set animation data to play (pointer is not owned)
    void SetAnimation(const Animation* anim);

    // Play new animation with optional smooth transition/cross-fade duration
    void PlayAnimation(const Animation* newAnim, float transitionTime = 0.0f);

    // Advance animation by dt seconds
    void Update(float dt);

    void ApplyTo(Skeleton& skeleton) const;

    bool HasAnimation() const { return animation_ != nullptr || prevAnimation_ != nullptr; }

    float GetTime() const { return animationTime_; }
    float GetDuration() const { return animation_ ? animation_->duration : 0.0f; }
    const Animation* GetCurrentAnimation() const { return animation_; }

    void SetPlaybackSpeed(float speed) { playbackSpeed_ = speed; }
    float GetPlaybackSpeed() const { return playbackSpeed_; }

    void SetPaused(bool pause) { isPaused_ = pause; }
    bool IsPaused() const { return isPaused_; }
    void TogglePause() { isPaused_ = !isPaused_; }

    void SetTime(float time);
    void StepFrame(float frameTime = 1.0f / 60.0f);

    bool IsBlending() const { return isBlending_; }
    float GetBlendFactor() const { return isBlending_ ? (transitionTimer_ / transitionDuration_) : 0.0f; }

    // Returns local transform matrices for each animated node (keyed by node name)
    const std::map<std::string, Matrix4x4>& GetLocalMatrices() const { return localMatrices_; }

private:
    const Animation* animation_ = nullptr;
    float animationTime_ = 0.0f;
    float playbackSpeed_ = 1.0f;
    bool isPaused_ = false;

    // Cross-fade animation blending variables
    const Animation* prevAnimation_ = nullptr;
    float prevAnimationTime_ = 0.0f;
    float transitionDuration_ = 0.0f;
    float transitionTimer_ = 0.0f;
    bool isBlending_ = false;

    std::map<std::string, Matrix4x4> localMatrices_;
};
