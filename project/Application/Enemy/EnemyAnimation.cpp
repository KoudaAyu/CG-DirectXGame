#define NOMINMAX
#include "EnemyAnimation.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <memory>
#include <unordered_map>
#include <vector>

namespace
{
    std::unordered_map<std::string, EnemyAnimationAsset> g_assets;

    std::string ToLower(const std::string& s)
    {
        std::string out = s;
        for (char& c : out)
        {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return out;
    }

    /// @brief gltf に入っている全アニメーションを読む
    /// @note engine の LoadAnimationFile() は mAnimations[0] しか読まないため、ここで自前実装している
    void LoadAllClips(const std::string& fullPath, std::vector<EnemyAnimationClip>& outClips)
    {
        Assimp::Importer importer;
        // engine 側の LoadAnimationFile と同じフラグにそろえる
        const aiScene* scene = importer.ReadFile(fullPath, aiProcess_Triangulate | aiProcess_FlipUVs);
        if (!scene || scene->mNumAnimations == 0)
        {
            return;
        }

        outClips.reserve(scene->mNumAnimations);

        for (uint32_t animIndex = 0; animIndex < scene->mNumAnimations; ++animIndex)
        {
            const aiAnimation* src = scene->mAnimations[animIndex];
            if (!src) continue;

            EnemyAnimationClip clip;
            clip.name = src->mName.C_Str();
            if (clip.name.empty())
            {
                clip.name = "clip_" + std::to_string(animIndex);
            }

            const double ticksPerSecond = (src->mTicksPerSecond != 0.0) ? src->mTicksPerSecond : 1.0;
            clip.animation.duration = static_cast<float>(src->mDuration / ticksPerSecond);

            for (uint32_t channelIndex = 0; channelIndex < src->mNumChannels; ++channelIndex)
            {
                const aiNodeAnim* channel = src->mChannels[channelIndex];
                if (!channel) continue;

                NodeAnimation& node = clip.animation.nodeAnimations[channel->mNodeName.C_Str()];

                for (uint32_t k = 0; k < channel->mNumPositionKeys; ++k)
                {
                    const aiVectorKey& key = channel->mPositionKeys[k];
                    Keyframe<Vector3> kf;
                    kf.time = static_cast<float>(key.mTime / ticksPerSecond);
                    kf.value = { key.mValue.x, key.mValue.y, key.mValue.z };
                    node.translate.push_back(kf);
                }

                for (uint32_t k = 0; k < channel->mNumRotationKeys; ++k)
                {
                    const aiQuatKey& key = channel->mRotationKeys[k];
                    Keyframe<Quaternion> kf;
                    kf.time = static_cast<float>(key.mTime / ticksPerSecond);
                    kf.value = Quaternion(key.mValue.x, key.mValue.y, key.mValue.z, key.mValue.w);
                    node.rotate.push_back(kf);
                }

                for (uint32_t k = 0; k < channel->mNumScalingKeys; ++k)
                {
                    const aiVectorKey& key = channel->mScalingKeys[k];
                    Keyframe<Vector3> kf;
                    kf.time = static_cast<float>(key.mTime / ticksPerSecond);
                    kf.value = { key.mValue.x, key.mValue.y, key.mValue.z };
                    node.scale.push_back(kf);
                }
            }

            outClips.push_back(std::move(clip));
        }
    }
}

int EnemyAnimationAsset::FindClip(const std::string& name) const
{
    if (name.empty()) return -1;

    // 完全一致を優先
    for (size_t i = 0; i < clips.size(); ++i)
    {
        if (clips[i].name == name) return static_cast<int>(i);
    }

    // 大文字小文字の揺れを吸収（"Idle" と "idle" が混在しているため）
    const std::string lower = ToLower(name);
    for (size_t i = 0; i < clips.size(); ++i)
    {
        if (ToLower(clips[i].name) == lower) return static_cast<int>(i);
    }

    return -1;
}

const EnemyAnimationAsset* LoadEnemyAnimationAsset(const std::string& directoryPath, const std::string& fileName)
{
    const std::string key = directoryPath + "/" + fileName;

    auto it = g_assets.find(key);
    if (it != g_assets.end())
    {
        return &it->second;
    }

    EnemyAnimationAsset asset;
    asset.key = key;

    // 1. スキンウェイト付きのモデルデータ（SetupAnimation が要求する Model::ModelData）
    //    Object3d::LoadModelFile と Model::LoadModelFile は同じノード走査・同じ import フラグなので
    //    頂点の並びが一致する。ここでは Model 側だけ読んで Object3d 用に詰め替える
    asset.modelData = Model::LoadModelFile(directoryPath, fileName);

    if (asset.modelData.vertices.empty() || asset.modelData.skinClusterData.empty())
    {
        // スキンが無いモデル。静的メッシュとして扱う
        auto inserted = g_assets.emplace(key, std::move(asset));
        return &inserted.first->second;
    }

    // 2. スケルトン
    SkeletonLoader skeletonLoader;
    asset.skeleton = skeletonLoader.LoadSkeletonFile(directoryPath, fileName);
    if (asset.skeleton.joints.empty())
    {
        auto inserted = g_assets.emplace(key, std::move(asset));
        return &inserted.first->second;
    }

    // 3. Object3d 用のモデルデータ（頂点順は Model::ModelData と同一）
    asset.object3dModel.vertices = asset.modelData.vertices;
    asset.object3dModel.indices = asset.modelData.indices;
    asset.object3dModel.material.textureFilePath = asset.modelData.material.textureFilePath;

    float maxLenSq = 0.0f;
    for (const auto& v : asset.object3dModel.vertices)
    {
        float lenSq = v.position.x * v.position.x + v.position.y * v.position.y + v.position.z * v.position.z;
        maxLenSq = (std::max)(maxLenSq, lenSq);
    }
    // アニメーションで手足が振れてバインドポーズの外へ出るぶんを少し盛る
    asset.object3dModel.boundingRadius = (maxLenSq > 0.0f) ? (std::sqrt(maxLenSq) * 1.3f) : 2.0f;

    // 4. 全クリップ
    LoadAllClips(key, asset.clips);

    asset.valid = !asset.clips.empty();

    auto inserted = g_assets.emplace(key, std::move(asset));
    return &inserted.first->second;
}

namespace
{
    using SkinnedPool = std::unordered_map<std::string, std::vector<std::unique_ptr<Object3d>>>;

    /// @brief モデルごとの空き Object3d（使い回し用）
    /// @note **わざと解放しない。** ここに寝ている Object3d は D3D リソースを握っており、
    ///       静的デストラクタで壊すと DirectXCom より後になってアクセス違反になりうる
    ///       （engine の終了時の解放順が読めない件。engine-notes.md 参照）
    SkinnedPool& Pool()
    {
        static SkinnedPool* pool = new SkinnedPool();
        return *pool;
    }

    int g_poolTotal = 0;

    /// @brief プールに寝かせるときの退避先。CollisionManager の座標同期に拾われない位置へ逃がす
    constexpr float kParkedY = -100000.0f;
}

Object3d* AcquireSkinnedObject3d(const EnemyAnimationAsset* asset, Object3dCom* object3dCom, Camera* camera)
{
    if (!asset || !asset->valid || !object3dCom) return nullptr;

    auto& freeList = Pool()[asset->key];

    if (!freeList.empty())
    {
        std::unique_ptr<Object3d> obj = std::move(freeList.back());
        freeList.pop_back();
        Object3d* raw = obj.release(); // プールが所有し続ける形にするため、生ポインタで持ち回す
        raw->SetCamera(camera);
        return raw;
    }

    auto obj = std::make_unique<Object3d>();
    obj->Initialize(object3dCom, asset->object3dModel);
    obj->SetCamera(camera);
    obj->SetupAnimation(nullptr, asset->skeleton, asset->modelData);
    ++g_poolTotal;
    return obj.release();
}

void ReleaseSkinnedObject3d(const EnemyAnimationAsset* asset, Object3d* object)
{
    if (!asset || !object) return;

    // 遠くへ逃がしておく。CollisionManager::Update() の冒頭にある
    // 「コライダーの近くの Object3d を探して同期する」処理に拾われないようにするため
    object->SetTranslate({ 0.0f, kParkedY, 0.0f });

    Pool()[asset->key].push_back(std::unique_ptr<Object3d>(object));
}

int GetSkinnedObject3dPoolTotal()
{
    return g_poolTotal;
}

void EnemyAnimator::Initialize(Object3d* object, const EnemyAnimationAsset* asset)
{
    object_ = object;
    asset_ = (asset && asset->valid) ? asset : nullptr;
    currentIndex_ = -1;
    returnIndex_ = -1;
    time_ = 0.0f;
    speed_ = 1.0f;
    loop_ = true;
    finished_ = false;

    if (asset_ && !asset_->clips.empty())
    {
        currentIndex_ = 0; // 何も指定されなければ先頭クリップ
    }
}

int EnemyAnimator::ResolveClip(const std::string& name) const
{
    if (!asset_) return -1;
    return asset_->FindClip(name);
}

float EnemyAnimator::CurrentDuration() const
{
    if (!asset_ || currentIndex_ < 0) return 0.0f;
    return asset_->clips[static_cast<size_t>(currentIndex_)].animation.duration;
}

const char* EnemyAnimator::GetCurrentClipName() const
{
    if (!asset_ || currentIndex_ < 0) return "";
    return asset_->clips[static_cast<size_t>(currentIndex_)].name.c_str();
}

void EnemyAnimator::Play(const std::string& clipName)
{
    if (!IsValid()) return;

    // ワンショット再生中は割り込まない。終わってから戻る先を差し替えるだけにする
    int index = ResolveClip(clipName);
    if (index < 0) return;

    if (IsOneShotPlaying())
    {
        returnIndex_ = index;
        return;
    }

    if (index == currentIndex_ && loop_) return;

    currentIndex_ = index;
    returnIndex_ = -1;
    time_ = 0.0f;
    loop_ = true;
    finished_ = false;
}

void EnemyAnimator::PlayOneShot(const std::string& clipName, const std::string& returnClipName)
{
    if (!IsValid()) return;

    int index = ResolveClip(clipName);
    if (index < 0)
    {
        // 攻撃クリップが無いモデルなら、戻り先だけ再生しておく
        Play(returnClipName);
        return;
    }

    currentIndex_ = index;
    returnIndex_ = ResolveClip(returnClipName);
    time_ = 0.0f;
    loop_ = false;
    finished_ = false;
}

void EnemyAnimator::Update(float deltaTime)
{
    if (!IsValid() || currentIndex_ < 0) return;

    const float duration = CurrentDuration();
    if (duration <= 0.0f)
    {
        return;
    }

    time_ += deltaTime * speed_;

    if (loop_)
    {
        time_ = std::fmod(time_, duration);
        if (time_ < 0.0f) time_ += duration;
        return;
    }

    if (time_ >= duration)
    {
        finished_ = true;
        if (returnIndex_ >= 0)
        {
            currentIndex_ = returnIndex_;
            returnIndex_ = -1;
            time_ = 0.0f;
            loop_ = true;
            finished_ = false;
        }
        else
        {
            time_ = duration;
        }
    }
}

void EnemyAnimator::ApplyToSkeleton()
{
    if (!IsValid() || currentIndex_ < 0 || !object_) return;

    object_->GetSkeleton().ApplyAnimation(
        asset_->clips[static_cast<size_t>(currentIndex_)].animation, time_);
}
