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
    void Update(float deltaTime, const Vector3& playerPos, float playerYaw, bool isMerged);
    void Draw(const RenderContext& ctx);

    // --- 群衆の操作 ---
    void SpawnMinion(const Vector3& spawnPos, int count = 1, MinionType type = MinionType::Red);
    void ClearMinions();
    void SetMinionCount(int count, const Vector3& playerPos);

    // 投擲指示
    bool ThrowMinion(const Vector3& launchPos, const Vector3& forwardDir, float throwPower = 15.0f, float upPower = 8.0f);

    // ホイッスル呼び戻し
    void Whistle(const Vector3& whistlePos, float radius = 8.0f);

    // 合体 / 分裂トリガー
    void TriggerMerge(const Vector3& playerPos);
    void TriggerSplit(const Vector3& playerPos);

    // ゲッター
    int GetActiveCount() const;
    int GetTotalCount() const { return static_cast<int>(minions_.size()); }
    bool IsAllMerged() const { return isAllMerged_; }

    // パラメータ調整
    void SetFollowSpeed(float speed) { followSpeed_ = speed; }
    float GetFollowSpeed() const { return followSpeed_; }
    void SetSlotRadius(float radius) { slotBaseRadius_ = radius; }
    float GetSlotRadius() const { return slotBaseRadius_; }

private:
    void CalculateFormationSlots(const Vector3& playerPos, float playerYaw);
    void ResolveSeparation();

private:
    Object3dCom* object3dCom_ = nullptr;
    Camera* camera_ = nullptr;
    std::vector<std::unique_ptr<Minion>> minions_;

    bool isMergedState_ = false;
    bool isAllMerged_ = false;

    float followSpeed_ = 18.0f;
    float slotBaseRadius_ = 0.75f;
    float slotRowSpacing_ = 0.45f;
    float separationRadius_ = 0.4f;
};
