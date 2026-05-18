#pragma once

#pragma once

#include"../NodeAnimation/NodeStruct.h"
#include"Matrix4x4.h"
#include"Quaternion.h"
#include"Transform.h"

#include <optional>
#include <string>
#include <vector>

struct Skeleton;

struct Joint
{
 AnimNodeData::QuaternionTransform transform; // Transform情報 (scale, rotation(quaternion), translation)
	Matrix4x4 localMatrix; //localMatrix
	Matrix4x4 skeletonMatrix; //skeltonSpaceでの変換行列
	std::string name; //名前
	std::vector<int32_t> children; //子JointのIndexリスト。いらない場合は空
	int32_t index; //自身のIndex
	std::optional<int32_t> parent; //親JointのIndex。なければnull
};

class JointLoader
{
public:
    int32_t CreateJoint(const AnimNode& node, std::optional<int32_t> parentIndex, std::vector<Joint>& outJoints);

};
