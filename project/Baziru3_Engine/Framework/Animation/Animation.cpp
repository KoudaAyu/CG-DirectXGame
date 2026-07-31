#include "Animation.h"

#include "Keyframe.h"
#include "BinaryAssetUtil.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include<cassert>

#include <unordered_map>
#include <mutex>

static std::unordered_map<std::string, Animation> s_animationCache;
static std::mutex s_animationCacheMutex;

Animation LoadAnimationFile(const std::string& directoryPath, const std::string& filename)
{
	const std::string fullPath = directoryPath + "/" + filename;

	// メモリキャッシュ確認
	{
		std::lock_guard<std::mutex> lock(s_animationCacheMutex);
		auto it = s_animationCache.find(fullPath);
		if (it != s_animationCache.end())
		{
			return it->second;
		}
	}

	Animation animation;
	const std::string cachePath = BinaryAssetUtil::GetCachePath(fullPath, ".banim");

	// キャッシュが有効な場合はバイナリからロード
	if (BinaryAssetUtil::IsCacheValid(fullPath, cachePath))
	{
		if (BinaryAssetUtil::LoadBAnim(cachePath, animation))
		{
			OutputDebugStringA(("[Binary Cache] Loaded animation from cache: " + cachePath + "\n").c_str());
			// メモリキャッシュに登録
			{
				std::lock_guard<std::mutex> lock(s_animationCacheMutex);
				s_animationCache[fullPath] = animation;
			}
			return animation;
		}
	}

	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(fullPath, aiProcess_Triangulate | aiProcess_FlipUVs);
	assert(scene->mNumAnimations != 0); //アニメーションがない
	aiAnimation* animationAssimp = scene->mAnimations[0]; // 最初のアニメーションを使う
	const double ticksPerSecond = (animationAssimp->mTicksPerSecond != 0.0) ? animationAssimp->mTicksPerSecond : 1.0;
	animation.duration = static_cast<float>(animationAssimp->mDuration / ticksPerSecond); // アニメーションの全体の長さを秒で計算

	// アニメーションの各チャネルを処理
	for (uint32_t channelIndex = 0; channelIndex < animationAssimp->mNumChannels; ++channelIndex)
	{
		aiNodeAnim* nodeAnimationAssimp = animationAssimp->mChannels[channelIndex];
		::NodeAnimation& nodeAnimData = animation.nodeAnimations[nodeAnimationAssimp->mNodeName.C_Str()];

		for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumPositionKeys; ++keyIndex)
		{
			aiVectorKey& keyAssimp = nodeAnimationAssimp->mPositionKeys[keyIndex];
			Keyframe<Vector3> keyframe;
			keyframe.time = static_cast<float>(keyAssimp.mTime / ticksPerSecond); // キーフレームの時刻を秒で計算
			keyframe.value = { keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z };
			nodeAnimData.translate.push_back(keyframe);
		}

		for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumRotationKeys; ++keyIndex)
		{
			aiQuatKey& keyAssimp = nodeAnimationAssimp->mRotationKeys[keyIndex];
			Keyframe<Quaternion> keyframe;
			keyframe.time = static_cast<float>(keyAssimp.mTime / ticksPerSecond);
			keyframe.value = Quaternion(keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z, keyAssimp.mValue.w);
			nodeAnimData.rotate.push_back(keyframe);
		}

		for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumScalingKeys; ++keyIndex)
		{
			aiVectorKey& keyAssimp = nodeAnimationAssimp->mScalingKeys[keyIndex];
			Keyframe<Vector3> keyframe;
			keyframe.time = static_cast<float>(keyAssimp.mTime / ticksPerSecond);
			keyframe.value = { keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z };
			nodeAnimData.scale.push_back(keyframe);
		}
	}

	// キャッシュとして保存
	if (BinaryAssetUtil::SaveBAnim(cachePath, animation))
	{
		OutputDebugStringA(("[Binary Cache] Saved animation cache: " + cachePath + "\n").c_str());
	}

	// メモリキャッシュに登録
	{
		std::lock_guard<std::mutex> lock(s_animationCacheMutex);
		s_animationCache[fullPath] = animation;
	}

	return animation;
}
