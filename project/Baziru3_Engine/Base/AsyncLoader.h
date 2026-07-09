#pragma once
#include <string>
#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <memory>
#include "Model.h"
#include "Skeleton.h"
#include "AnimationData.h"

class ModelCom;

class AsyncLoader
{
public:
    enum class TaskType
    {
        Model,
        Skeleton,
        Animation
    };

    struct LoadTask
    {
        TaskType type;
        std::string directoryPath;
        std::string filename;
        std::string key; // 登録識別用のフルパス

        // 読み込み結果
        Model::ModelData modelData;
        Skeleton skeleton;
        Animation animation;

        bool success = false;
    };

public:
    static AsyncLoader* GetInstance();
    static void Destroy();

    void Initialize(ModelCom* modelCom);
    void Finalize();

    // 非同期ロードのリクエスト
    void RequestModel(const std::string& directoryPath, const std::string& filename);
    void RequestSkeleton(const std::string& directoryPath, const std::string& filename);
    void RequestAnimation(const std::string& directoryPath, const std::string& filename);

    // メインスレッドから毎フレーム呼び出す同期更新処理
    void Update();

    // ロード完了確認ヘルパー
    bool IsModelLoaded(const std::string& filepath) const;
    bool IsSkeletonLoaded(const std::string& filepath) const;
    bool IsAnimationLoaded(const std::string& filepath) const;

    // 読み込み完了データの取得（取得後は内部キャッシュから削除）
    Skeleton GetSkeleton(const std::string& filepath);
    Animation GetAnimation(const std::string& filepath);

public:
    ~AsyncLoader() = default;

private:
    AsyncLoader() = default;
    AsyncLoader(const AsyncLoader&) = delete;
    AsyncLoader& operator=(const AsyncLoader&) = delete;

    void WorkerThreadLoop();

private:
    ModelCom* modelCom_ = nullptr;
    std::thread workerThread_;
    std::mutex queueMutex_;
    std::condition_variable cv_;
    std::queue<std::shared_ptr<LoadTask>> taskQueue_;
    std::vector<std::shared_ptr<LoadTask>> completedTasks_;
    bool isRunning_ = false;

    // 完了データのキャッシュ
    std::map<std::string, Skeleton> loadedSkeletons_;
    std::map<std::string, Animation> loadedAnimations_;
};
