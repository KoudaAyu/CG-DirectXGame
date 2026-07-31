#pragma once

#pragma once

#include"Joint.h"
#include"../NodeAnimation/NodeStruct.h"

#include <string>
#include<map>
#include<vector>

struct Animation;

struct Skeleton
{
	int32_t root;
	std::map<std::string, int32_t> jointMap;
	std::vector<Joint> joints;
    
  void ApplyAnimation(const Animation& animation, float animationTime);
	void Update();
  Matrix4x4 GetJointWorldMatrix(size_t jointIndex, const Matrix4x4& skeletonRootWorldMatrix) const;
	Vector3 GetJointWorldPosition(size_t jointIndex, const Matrix4x4& skeletonRootWorldMatrix) const;
};

class SkeletonLoader
{
public:
    Skeleton CreateSkeleton(const AnimNode& rootNode);
  Skeleton LoadSkeletonFile(const std::string& directoryPath, const std::string& filename);
};

