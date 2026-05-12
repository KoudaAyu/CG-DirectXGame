#pragma once
#pragma once

#include <vector>
#include <utility>
#include <cstddef>

#include "Transform.h"
#include "Quaternion.h"
#include "Keyframe.h"

template <typename tValue>

struct AnimationCurve
{
	std::vector<Keyframe<tValue>> keyframes;

	
	void push_back(const Keyframe<tValue>& k) { keyframes.push_back(k); }

	template <class... Args>
	void emplace_back(Args&&... args) { keyframes.emplace_back(std::forward<Args>(args)...); }

	size_t size() const noexcept { return keyframes.size(); }

	bool empty() const noexcept { return keyframes.empty(); }

	void clear() noexcept { keyframes.clear(); }
};

struct NodeAnimation
{
	AnimationCurve<Vector3> translate;
	AnimationCurve<Quaternion> rotate;
	AnimationCurve<Vector3> scale;
};