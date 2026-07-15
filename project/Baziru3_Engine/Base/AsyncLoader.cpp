#include "AsyncLoader.h"
#include "ModelManager.h"
#include "Animation.h"
#include <Windows.h>
#include <cassert>

namespace {
    static std::unique_ptr<AsyncLoader>& AsyncLoaderStorage()
    {
        static std::unique_ptr<AsyncLoader> instance;
        return instance;
    }
}

AsyncLoader* AsyncLoader::GetInstance()
{
    auto& instance = AsyncLoaderStorage();
    if (!instance)
    {
        instance.reset(new AsyncLoader());
    }
    return instance.get();
}

void AsyncLoader::Destroy()
{
    AsyncLoaderStorage().reset();
}

void AsyncLoader::Initialize(ModelCom* modelCom)
{
    assert(modelCom);
    modelCom_ = modelCom;
    isRunning_ = true;

    workerThread_ = std::thread(&AsyncLoader::WorkerThreadLoop, this);
    OutputDebugStringA("[AsyncLoader] Initialized background worker thread.\n");
}

void AsyncLoader::Finalize()
{
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        isRunning_ = false;
        cv_.notify_all();
    }

    if (workerThread_.joinable())
    {
        workerThread_.join();
    }

    // キャッシュクリア
    loadedSkeletons_.clear();
    loadedAnimations_.clear();

    while (!taskQueue_.empty())
    {
        taskQueue_.pop();
    }
    completedTasks_.clear();

    OutputDebugStringA("[AsyncLoader] Finalized background worker thread.\n");
}

void AsyncLoader::RequestModel(const std::string& directoryPath, const std::string& filename)
{
    std::string filepath = directoryPath + "/" + filename;
    
    // すでにロード済みの場合はスキップ
    if (IsModelLoaded(filepath)) return;

    // すでにキューに入っているかチェック
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        // キュー内を簡易チェック（タスク数が多い場合は線形探索）
        // ※実際には多重リクエストを避けるための設計
    }

    auto task = std::make_shared<LoadTask>();
    task->type = TaskType::Model;
    task->directoryPath = directoryPath;
    task->filename = filename;
    task->key = filepath;

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        taskQueue_.push(task);
        cv_.notify_one();
    }
}

void AsyncLoader::RequestSkeleton(const std::string& directoryPath, const std::string& filename)
{
    std::string filepath = directoryPath + "/" + filename;
    if (IsSkeletonLoaded(filepath)) return;

    auto task = std::make_shared<LoadTask>();
    task->type = TaskType::Skeleton;
    task->directoryPath = directoryPath;
    task->filename = filename;
    task->key = filepath;

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        taskQueue_.push(task);
        cv_.notify_one();
    }
}

void AsyncLoader::RequestAnimation(const std::string& directoryPath, const std::string& filename)
{
    std::string filepath = directoryPath + "/" + filename;
    if (IsAnimationLoaded(filepath)) return;

    auto task = std::make_shared<LoadTask>();
    task->type = TaskType::Animation;
    task->directoryPath = directoryPath;
    task->filename = filename;
    task->key = filepath;

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        taskQueue_.push(task);
        cv_.notify_one();
    }
}

void AsyncLoader::Update()
{
    std::vector<std::shared_ptr<LoadTask>> localCompleted;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        if (completedTasks_.empty()) return;
        localCompleted = std::move(completedTasks_);
        completedTasks_.clear();
    }

    for (const auto& task : localCompleted)
    {
        if (!task->success)
        {
            OutputDebugStringA(("[AsyncLoader] Failed to load: " + task->key + "\n").c_str());
            continue;
        }

        if (task->type == TaskType::Model)
        {
            auto* mm = ModelManager::GetInstance();
            if (!mm->FindModel(task->key))
            {
                auto model = std::make_unique<Model>();
                model->Initialize(modelCom_, task->directoryPath, task->filename, task->modelData);
                mm->models_.insert(std::make_pair(task->key, std::move(model)));
                OutputDebugStringA(("[AsyncLoader] Registered model on main thread: " + task->key + "\n").c_str());
            }
        }
        else if (task->type == TaskType::Skeleton)
        {
            loadedSkeletons_[task->key] = std::move(task->skeleton);
            OutputDebugStringA(("[AsyncLoader] Cached skeleton: " + task->key + "\n").c_str());
        }
        else if (task->type == TaskType::Animation)
        {
            loadedAnimations_[task->key] = std::move(task->animation);
            OutputDebugStringA(("[AsyncLoader] Cached animation: " + task->key + "\n").c_str());
        }
    }
}

bool AsyncLoader::IsModelLoaded(const std::string& filepath) const
{
    return ModelManager::GetInstance()->FindModel(filepath) != nullptr;
}

bool AsyncLoader::IsSkeletonLoaded(const std::string& filepath) const
{
    return loadedSkeletons_.contains(filepath);
}

bool AsyncLoader::IsAnimationLoaded(const std::string& filepath) const
{
    return loadedAnimations_.contains(filepath);
}

Skeleton AsyncLoader::GetSkeleton(const std::string& filepath)
{
    auto it = loadedSkeletons_.find(filepath);
    if (it != loadedSkeletons_.end())
    {
        Skeleton s = std::move(it->second);
        loadedSkeletons_.erase(it);
        return s;
    }
    return {};
}

Animation AsyncLoader::GetAnimation(const std::string& filepath)
{
    auto it = loadedAnimations_.find(filepath);
    if (it != loadedAnimations_.end())
    {
        Animation a = std::move(it->second);
        loadedAnimations_.erase(it);
        return a;
    }
    return {};
}

void AsyncLoader::WorkerThreadLoop()
{
    while (isRunning_)
    {
        std::shared_ptr<LoadTask> task;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            cv_.wait(lock, [this] { return !isRunning_ || !taskQueue_.empty(); });
            if (!isRunning_ && taskQueue_.empty())
            {
                break;
            }
            task = taskQueue_.front();
            taskQueue_.pop();
        }

        // アセットロードの実行 (バイナリキャッシュのおかげで最速で読み込めます)
        if (task->type == TaskType::Model)
        {
            task->modelData = Model::LoadModelFile(task->directoryPath, task->filename);
            task->success = !task->modelData.vertices.empty();
        }
        else if (task->type == TaskType::Skeleton)
        {
            task->skeleton = SkeletonLoader{}.LoadSkeletonFile(task->directoryPath, task->filename);
            task->success = !task->skeleton.joints.empty();
        }
        else if (task->type == TaskType::Animation)
        {
            task->animation = LoadAnimationFile(task->directoryPath, task->filename);
            task->success = (task->animation.duration > 0.0f);
        }

        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            completedTasks_.push_back(task);
        }
    }
}
