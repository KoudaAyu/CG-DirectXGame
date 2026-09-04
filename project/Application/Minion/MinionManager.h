#pragma once

#include "Minion.h"
#include <vector>
#include <memory>

class Object3dCom;
class Camera;
class RenderContext;

/**
 * @brief ミニオン群衆マネージャー
 */
class MinionManager {
public:
    MinionManager() = default;
    ~MinionManager() = default;

    void Initialize(Object3dCom* object3dCom, Camera* camera);
    void Update(float deltaTime, const Vector3& playerPos, bool isMerged, float playerRadius = 0.8f, const Vector2& stageTilt = { 0.0f, 0.0f });
    void Draw(const RenderContext& ctx);

    // --- 群衆の操作 ---
    void SpawnMinion(const Vector3& spawnPos, int count = 1, MinionType type = MinionType::Red);
    void ClearMinions();
    void SetMinionCount(int count, const Vector3& playerPos);

    // 投擲指示
    bool ThrowMinionWithVelocity(const Vector3& launchPos, const Vector3& velocity);

    // 合体 / 分裂トリガー
    void TriggerMerge(const Vector3& playerPos, float mergeRadius = 4.5f);
    void TriggerSplit(const Vector3& playerPos);

    // ゲッター / セッター
    int GetActiveCount() const;
    int GetReadyCount(const Vector3& playerPos, float maxPickupRadius = 3.5f) const;
    int GetMergedCount() const;
    int GetTotalCount() const { return static_cast<int>(minions_.size()); }

    /// @brief ミニオンの実体を列挙する（軌跡エフェクトなど、外から個々の状態を見たいとき用）
    const std::vector<std::unique_ptr<Minion>>& GetMinions() const { return minions_; }
    bool IsAllMerged() const { return isAllMerged_; }

    float GetMergePickupRadius() const { return mergePickupRadius_; }
    void SetMergePickupRadius(float r) { mergePickupRadius_ = r; }

    float GetSplitPopPower() const { return splitPopPower_; }
    void SetSplitPopPower(float p) { splitPopPower_ = p; }
    float GetSplitUpPower() const { return splitUpPower_; }
    void SetSplitUpPower(float p) { splitUpPower_ = p; }

private:
    void ResolveSeparation();

private:
    Object3dCom* object3dCom_ = nullptr;
    Camera* camera_ = nullptr;
    std::vector<std::unique_ptr<Minion>> minions_;

    bool isMergedState_ = false;
    bool isAllMerged_ = false;
    float mergePickupRadius_ = 4.5f;
    float splitPopPower_ = 8.0f;
    float splitUpPower_ = 7.0f;
    float separationRadius_ = 0.6f;
};
