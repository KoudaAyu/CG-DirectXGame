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
    struct MergeResult {
        int newlyMergedCount = 0;
        bool playerPromoted = false;
        Vector3 promotedPos{ 0.0f, 0.0f, 0.0f };
        int promotedSize = 1;
    };

    MinionManager() = default;
    ~MinionManager() = default;

    void Initialize(Object3dCom* object3dCom, Camera* camera);
    MergeResult Update(float deltaTime, const Vector3& playerPos, bool isMerged, float playerScale = 0.8f, const Vector2& stageTilt = { 0.0f, 0.0f }, const Vector3& playerSquash = { 0.0f, 0.0f, 0.0f }, const Vector3& playerVelocity = { 0.0f, 0.0f, 0.0f }, int playerSize = 1);
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
    void SetAllAbsorbed(bool absorbed);
    void SetInitialAbsorbedCount(int absorbedCount);

    // 接触自動合体判定（プレイヤー接触・仲間同士接触の両方を解決）
    MergeResult CheckAndResolveMerge(const Vector3& playerPos, float playerScale, int playerSize = 1);

    // 全ロコロコの群れ重心および広がり半径を算出（カメラ追従用）
    void GetGroupCenterAndSpread(const Vector3& playerPos, Vector3& outCenter, float& outSpread) const;

    // ゲッター / セッター
    int GetActiveCount() const;
    int GetReadyCount(const Vector3& playerPos, float maxPickupRadius = 3.5f) const;
    int GetMaxMinionSize() const;

    int GetMergedCount() const { return absorbedCount_; }
    int GetAbsorbedCount() const { return absorbedCount_; }
    int GetTotalCount() const { return static_cast<int>(minions_.size()); }
    const std::vector<std::unique_ptr<Minion>>& GetMinions() const { return minions_; }
    bool IsAllMerged() const { return isAllMerged_; }

    float GetMergePickupRadius() const { return mergePickupRadius_; }
    void SetMergePickupRadius(float r) { mergePickupRadius_ = r; }

    float GetSplitPopPower() const { return splitPopPower_; }
    void SetSplitPopPower(float p) { splitPopPower_ = p; }
    float GetSplitUpPower() const { return splitUpPower_; }
    void SetSplitUpPower(float p) { splitUpPower_ = p; }

    // デバッグ多重球描画
    void DrawDebug(Camera* camera);

private:
    void ResolveSeparation(const Vector3& rotation, const Vector2& stageTilt, const Vector2& pivot = { 0.0f, 0.0f });
    void ResolvePlayerSeparation(const Vector3& playerPos, const Vector3& playerVelocity, const Vector3& playerScale, const Vector3& playerSquash, const Vector3& playerRotation, const Vector2& stageTilt, const Vector2& pivot = { 0.0f, 0.0f });

private:
    Object3dCom* object3dCom_ = nullptr;
    Camera* camera_ = nullptr;
    std::vector<std::unique_ptr<Minion>> minions_;

    int absorbedCount_ = 0; // プレイヤーに吸収されたロコロコ（サイズ）数
    bool isMergedState_ = false;
    bool isAllMerged_ = false;
    float mergePickupRadius_ = 4.5f;
    float splitPopPower_ = 8.0f;
    float splitUpPower_ = 7.0f;
    float separationRadius_ = 0.6f;
};

