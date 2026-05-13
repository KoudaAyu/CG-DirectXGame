#pragma once

#include "AnimationData.h"
#include "Skeleton/Skeleton.h"

Animation LoadAnimationFile(const std::string& directoryPath, const std::string& filename);

void ApplyAnimation(Skeleton& skeleton, const Animation& animation, float animationTime);

