#pragma once
#include <vector>
#include <memory>
#include <functional>
#include <cassert>

// 弾丸やエフェクトなどの頻繁な new/delete を防ぐオブジェクトプール
template <typename T>
class ObjectPool
{
public:
    ObjectPool() = default;
    ~ObjectPool() = default;

    // プールの初期化（事前に指定数だけメモリ確保）
    void Initialize(size_t initialCapacity, std::function<std::unique_ptr<T>()> factory = nullptr)
    {
        factory_ = factory ? factory : []() { return std::make_unique<T>(); };
        pool_.clear();
        pool_.reserve(initialCapacity);

        for (size_t i = 0; i < initialCapacity; ++i)
        {
            pool_.push_back(factory_());
        }
        totalCreated_ = initialCapacity;
    }

    // プールからオブジェクトを貸し出し
    T* Acquire()
    {
        if (pool_.empty())
        {
            auto newObj = factory_ ? factory_() : std::make_unique<T>();
            T* rawPtr = newObj.get();
            activeObjects_.push_back(std::move(newObj));
            totalCreated_++;
            return rawPtr;
        }

        auto obj = std::move(pool_.back());
        pool_.pop_back();
        T* rawPtr = obj.get();
        activeObjects_.push_back(std::move(obj));
        return rawPtr;
    }

    // 使用終了したオブジェクトをプールに返却
    void Release(T* object, std::function<void(T*)> resetFunc = nullptr)
    {
        if (!object) return;

        for (auto it = activeObjects_.begin(); it != activeObjects_.end(); ++it)
        {
            if (it->get() == object)
            {
                if (resetFunc)
                {
                    resetFunc(object);
                }
                pool_.push_back(std::move(*it));
                *it = std::move(activeObjects_.back());
                activeObjects_.pop_back();
                return;
            }
        }
    }

    // 全オブジェクトを一括返却
    void ReleaseAll(std::function<void(T*)> resetFunc = nullptr)
    {
        for (auto& obj : activeObjects_)
        {
            if (resetFunc && obj)
            {
                resetFunc(obj.get());
            }
            pool_.push_back(std::move(obj));
        }
        activeObjects_.clear();
    }

    size_t GetAvailableCount() const { return pool_.size(); }
    size_t GetActiveCount() const { return activeObjects_.size(); }
    size_t GetTotalCreated() const { return totalCreated_; }

    const std::vector<std::unique_ptr<T>>& GetActiveObjects() const { return activeObjects_; }

private:
    std::function<std::unique_ptr<T>()> factory_;
    std::vector<std::unique_ptr<T>> pool_;          // 待機中オブジェクト
    std::vector<std::unique_ptr<T>> activeObjects_; // 使用中オブジェクト
    size_t totalCreated_ = 0;
};
