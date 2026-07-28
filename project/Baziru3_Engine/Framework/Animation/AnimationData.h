#pragma once
#include <map>
#include <string>

#include "NodeAnimation.h"

struct Animation
{
	float duration; // アニメーションの全体の長さ（秒）
	std::map<std::string, NodeAnimation> nodeAnimations;
};
