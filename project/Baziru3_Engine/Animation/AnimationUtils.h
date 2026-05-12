#pragma once

#include <vector>
#include "Keyframe/Keyframe.h"
#include "../Base/Quaternion.h"
#include "../Base/Vector.h"

// Calculate interpolated value at given time for Vector3 keyframes
Vector3 CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time);

// Calculate interpolated value at given time for Quaternion keyframes
Quaternion CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time);
