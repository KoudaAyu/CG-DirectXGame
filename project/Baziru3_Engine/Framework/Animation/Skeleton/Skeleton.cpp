#include "Skeleton.h"

#include "Skeleton.h"
#include "BinaryAssetUtil.h"

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

#include <unordered_map>
#include <mutex>

static std::unordered_map<std::string, Skeleton> s_skeletonCache;
static std::mutex s_skeletonCacheMutex;

Skeleton SkeletonLoader::LoadSkeletonFile(const std::string& directoryPath, const std::string& filename)
{
	const std::string fullPath = directoryPath + "/" + filename;

	// メモリキャッシュ確認
	{
		std::lock_guard<std::mutex> lock(s_skeletonCacheMutex);
		auto it = s_skeletonCache.find(fullPath);
		if (it != s_skeletonCache.end())
		{
			return it->second;
		}
	}

	Skeleton skeleton;
	const std::string cachePath = BinaryAssetUtil::GetCachePath(fullPath, ".bskel");

	// キャッシュが有効な場合はバイナリからロード
	if (BinaryAssetUtil::IsCacheValid(fullPath, cachePath))
	{
		if (BinaryAssetUtil::LoadBSkel(cachePath, skeleton))
		{
			OutputDebugStringA(("[Binary Cache] Loaded skeleton from cache: " + cachePath + "\n").c_str());
			// メモリキャッシュに登録
			{
				std::lock_guard<std::mutex> lock(s_skeletonCacheMutex);
				s_skeletonCache[fullPath] = skeleton;
			}
			return skeleton;
		}
	}

	Assimp::Importer importer;
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

	skeleton = CreateSkeleton(rootNode);

	// キャッシュとして保存
	if (BinaryAssetUtil::SaveBSkel(cachePath, skeleton))
	{
		OutputDebugStringA(("[Binary Cache] Saved skeleton cache: " + cachePath + "\n").c_str());
	}

	// メモリキャッシュに登録
	{
		std::lock_guard<std::mutex> lock(s_skeletonCacheMutex);
		s_skeletonCache[fullPath] = skeleton;
	}

	return skeleton;
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

