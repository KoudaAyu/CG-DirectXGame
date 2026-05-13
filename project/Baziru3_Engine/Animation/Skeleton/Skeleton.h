#pragma once

#pragma once

#include"Joint.h"
#include"../NodeAnimation/NodeStruct.h"

#include<map>
#include<vector>

struct Skeleton
{
	int32_t root;
	std::map<std::string, int32_t> jointMap;
	std::vector<Joint> joints;
    
	void Update();
};

class SkeletonLoader
{
public:
    Skeleton CreateSkeleton(const AnimNode& rootNode);
};

