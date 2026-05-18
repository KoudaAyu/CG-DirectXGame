#include "Animation.h"
#include "Animation.h"
#include "Keyframe.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include<cassert>

Animation LoadAnimationFile(const std::string& directoryPath, const std::string& filename)
{
	Animation animation; // 今回作るアニメーション
	Assimp::Importer importer;

	std::string fullPath = directoryPath + "/" + filename;
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

   return animation;
};
