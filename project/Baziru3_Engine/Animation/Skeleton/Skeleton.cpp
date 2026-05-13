#include "Skeleton.h"

Skeleton SkeletonLoader::CreateSkeleton(const AnimNode& rootNode)
{
	Skeleton skeleton;
	JointLoader loader;
	skeleton.root = loader.CreateJoint(rootNode, {}, skeleton.joints);

	// 名前とindexのマッピングを行いアクセスしやすくする
	for (const Joint& joint : skeleton.joints)
	{
		skeleton.jointMap.emplace(joint.name, joint.index);
	}

	return skeleton;
}

void Skeleton::Update()
{
	for (Joint& joint : joints)
	{
		joint.localMatrix = joint.transform.MakeLocalMatrix();
		if (joint.parent)
		{
			const Matrix4x4& parentSkeletonMatrix = joints[*joint.parent].skeletonMatrix;
           joint.skeletonMatrix = Multiply(joint.localMatrix, parentSkeletonMatrix);
		}
		else
		{
			joint.skeletonMatrix = joint.localMatrix;
		}
	}
}

