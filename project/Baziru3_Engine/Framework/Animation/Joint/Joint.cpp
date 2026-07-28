#include "Joint.h"

int32_t JointLoader::CreateJoint(const AnimNode& node, std::optional<int32_t> parent, std::vector<Joint>& joints)
{
	Joint joint;
	joint.name = node.name;
	joint.localMatrix = node.localMatrix;
	joint.skeletonMatrix = MakeIdentity4x4();
	joint.transform = node.transform;
	joint.index = int32_t(joints.size());
	joint.parent = parent;
	joints.push_back(joint);
	
    for (const AnimNode& child : node.children)
	{
		int32_t childIndex = CreateJoint(child, joint.index, joints);
		joints[joint.index].children.push_back(childIndex);
	}

	return joint.index;
}


