#pragma once

#include <vector>
#include "Keyframe/Keyframe.h"
#include "../Math/Quaternion.h"
#include "../Base/Vector.h"

// 指定時刻の Vector3 キーフレーム補間値を計算
Vector3 CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time);

// 指定時刻の Quaternion キーフレーム補間値を計算
Quaternion CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time);
