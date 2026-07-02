#pragma once
#include"Transform.h"

template <typename tValue>

struct Keyframe
{
	float time; //キーフレームの時刻
	tValue value; //キーフレームの値
};

using KeyframeVector3 = Keyframe<Vector3>;
using KeyframeQuaternion = Keyframe<Quaternion>;
