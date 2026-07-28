#include "Skeleton.h"

#include "Skeleton.h"

#include "../AnimationData.h"
#include "../AnimationUtils.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <unordered_set>

namespace
{
	std::unordered_set<std::string> CollectBoneNames(const aiScene* scene)
	{
		std::unordered_set<std::string> boneNames;
		if (!scene)
		{
			return boneNames;
		}

		for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
		{
			const aiMesh* mesh = scene->mMeshes[meshIndex];
			if (!mesh)
			{
				continue;
			}

			for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex)
			{
				const aiBone* bone = mesh->mBones[boneIndex];
				if (bone)
				{
					boneNames.insert(bone->mName.C_Str());
				}
			}
		}

		return boneNames;
	}

	bool ConvertAssimpNodeToAnimNode(const aiNode* node, const std::unordered_set<std::string>& boneNames, AnimNode& outNode)
	{
		if (!node)
		{
			return false;
		}

		aiVector3D scale{};
		aiQuaternion rotate{};
		aiVector3D translate{};
		node->mTransformation.Decompose(scale, rotate, translate);

		AnimNode result{};
		result.name = node->mName.C_Str();
		result.transform.scale = { scale.x, scale.y, scale.z };
		result.transform.rotate = Quaternion(rotate.x, rotate.y, rotate.z, rotate.w);
		result.transform.translate = { translate.x, translate.y, translate.z };
		result.localMatrix = result.transform.MakeLocalMatrix();

		const bool isBoneNode = boneNames.contains(result.name);
		for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex)
		{
			AnimNode childNode{};
			if (ConvertAssimpNodeToAnimNode(node->mChildren[childIndex], boneNames, childNode))
			{
				result.children.push_back(std::move(childNode));
			}
		}

		if (!isBoneNode && result.children.empty())
		{
			return false;
		}

		outNode = std::move(result);
		return true;
	}
}

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

Skeleton SkeletonLoader::LoadSkeletonFile(const std::string& directoryPath, const std::string& filename)
{
	Assimp::Importer importer;
	const std::string fullPath = directoryPath + "/" + filename;
	const aiScene* scene = importer.ReadFile(fullPath, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals);
	if (!scene || !scene->mRootNode)
	{
		return {};
	}

	const std::unordered_set<std::string> boneNames = CollectBoneNames(scene);
	AnimNode rootNode{};
	if (!ConvertAssimpNodeToAnimNode(scene->mRootNode, boneNames, rootNode))
	{
		return {};
	}

	return CreateSkeleton(rootNode);
}

void Skeleton::ApplyAnimation(const Animation& animation, float animationTime)
{
	for (Joint& joint : joints)
	{
		if (auto it = animation.nodeAnimations.find(joint.name); it != animation.nodeAnimations.end())
		{
			const ::NodeAnimation& nodeAnimData = it->second;
			if (!nodeAnimData.translate.empty())
			{
				joint.transform.translate = CalculateValue(nodeAnimData.translate.keyframes, animationTime);
			}
			if (!nodeAnimData.rotate.empty())
			{
				joint.transform.rotate = CalculateValue(nodeAnimData.rotate.keyframes, animationTime);
			}
			if (!nodeAnimData.scale.empty())
			{
				joint.transform.scale = CalculateValue(nodeAnimData.scale.keyframes, animationTime);
			}
		}
	}
}

void Skeleton::ApplyAnimationBlend(const Animation& animA, float timeA, const Animation& animB, float timeB, float factor)
{
	factor = std::clamp(factor, 0.0f, 1.0f);

	for (Joint& joint : joints)
	{
		Vector3 transA = joint.transform.translate;
		Quaternion rotA = joint.transform.rotate;
		Vector3 scaleA = joint.transform.scale;

		Vector3 transB = joint.transform.translate;
		Quaternion rotB = joint.transform.rotate;
		Vector3 scaleB = joint.transform.scale;

		if (auto itA = animA.nodeAnimations.find(joint.name); itA != animA.nodeAnimations.end())
		{
			const ::NodeAnimation& nodeAnimA = itA->second;
			if (!nodeAnimA.translate.empty()) transA = CalculateValue(nodeAnimA.translate.keyframes, timeA);
			if (!nodeAnimA.rotate.empty()) rotA = CalculateValue(nodeAnimA.rotate.keyframes, timeA);
			if (!nodeAnimA.scale.empty()) scaleA = CalculateValue(nodeAnimA.scale.keyframes, timeA);
		}

		if (auto itB = animB.nodeAnimations.find(joint.name); itB != animB.nodeAnimations.end())
		{
			const ::NodeAnimation& nodeAnimB = itB->second;
			if (!nodeAnimB.translate.empty()) transB = CalculateValue(nodeAnimB.translate.keyframes, timeB);
			if (!nodeAnimB.rotate.empty()) rotB = CalculateValue(nodeAnimB.rotate.keyframes, timeB);
			if (!nodeAnimB.scale.empty()) scaleB = CalculateValue(nodeAnimB.scale.keyframes, timeB);
		}

		// 1. 位置・スケールの線形補間 (Lerp: (1 - t) A + t B)
		joint.transform.translate.x = (1.0f - factor) * transA.x + factor * transB.x;
		joint.transform.translate.y = (1.0f - factor) * transA.y + factor * transB.y;
		joint.transform.translate.z = (1.0f - factor) * transA.z + factor * transB.z;

		joint.transform.scale.x = (1.0f - factor) * scaleA.x + factor * scaleB.x;
		joint.transform.scale.y = (1.0f - factor) * scaleA.y + factor * scaleB.y;
		joint.transform.scale.z = (1.0f - factor) * scaleA.z + factor * scaleB.z;

		// 2. 回転の球面線形補間 (Slerp)
		joint.transform.rotate = Quaternion::Slerp(rotA, rotB, factor);
	}
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

Matrix4x4 Skeleton::GetJointWorldMatrix(size_t jointIndex, const Matrix4x4& skeletonRootWorldMatrix) const
{
	if (jointIndex >= joints.size())
	{
		return skeletonRootWorldMatrix;
	}

	return Multiply(joints[jointIndex].skeletonMatrix, skeletonRootWorldMatrix);
}

Vector3 Skeleton::GetJointWorldPosition(size_t jointIndex, const Matrix4x4& skeletonRootWorldMatrix) const
{
	const Matrix4x4 jointWorldMatrix = GetJointWorldMatrix(jointIndex, skeletonRootWorldMatrix);
	return {
		jointWorldMatrix.m[3][0],
		jointWorldMatrix.m[3][1],
		jointWorldMatrix.m[3][2]
	};
}

void Skeleton::ApplyHeadLookAt(const Vector3& targetWorldPos, const Matrix4x4& skeletonRootWorldMatrix, float weight)
{
	if (joints.empty() || weight <= 0.001f) return;

	// 頭ボーン（mixamorig:Head または Head）を探す
	int32_t headIndex = -1;
	if (auto it = jointMap.find("mixamorig:Head"); it != jointMap.end()) {
		headIndex = it->second;
	} else if (auto it2 = jointMap.find("Head"); it2 != jointMap.end()) {
		headIndex = it2->second;
	}

	if (headIndex < 0 || headIndex >= static_cast<int32_t>(joints.size())) return;

	// 現在の頭のワールド座標を取得
	Vector3 headPos = GetJointWorldPosition(static_cast<size_t>(headIndex), skeletonRootWorldMatrix);

	// 目標地点への方向
	Vector3 dirToTarget = {
		targetWorldPos.x - headPos.x,
		targetWorldPos.y - headPos.y,
		targetWorldPos.z - headPos.z
	};
	float distSq = dirToTarget.x * dirToTarget.x + dirToTarget.y * dirToTarget.y + dirToTarget.z * dirToTarget.z;
	if (distSq < 0.0001f) return;

	float invDist = 1.0f / std::sqrt(distSq);
	dirToTarget.x *= invDist;
	dirToTarget.y *= invDist;
	dirToTarget.z *= invDist;

	// キャラクターの前方ベクトル (ワールド空間における +Z)
	Vector3 modelForward = {
		skeletonRootWorldMatrix.m[2][0],
		skeletonRootWorldMatrix.m[2][1],
		skeletonRootWorldMatrix.m[2][2]
	};
	float fwdLen = std::sqrt(modelForward.x * modelForward.x + modelForward.y * modelForward.y + modelForward.z * modelForward.z);
	if (fwdLen > 0.0001f) {
		modelForward.x /= fwdLen; modelForward.y /= fwdLen; modelForward.z /= fwdLen;
	} else {
		modelForward = { 0, 0, 1 };
	}

	// キャラクター前方向から目標方向への回転差分
	Quaternion deltaRot = Quaternion::FromToRotation(modelForward, dirToTarget);

	// 頭のジョイントの現在の回転に部分的に適用 (Weight による補間)
	Joint& headJoint = joints[headIndex];
	Quaternion targetRot = deltaRot * headJoint.transform.rotate;
	headJoint.transform.rotate = Quaternion::Slerp(headJoint.transform.rotate, targetRot, std::clamp(weight, 0.0f, 1.0f));
}

