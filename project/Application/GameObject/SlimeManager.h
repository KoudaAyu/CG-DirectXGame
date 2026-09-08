#pragma once

#include "Slime.h"
#include <vector>
#include <memory>

class Object3dCom;
class Camera;
class KeyInput;
class RenderContext;

/**
 * @brief スライム群衆マネージャー（全スライムの一元管理）
 */
class SlimeManager {
public:
    SlimeManager() = default;
    ~SlimeManager() = default;

    void Initialize(Object3dCom* object3dCom, Camera* camera);
    void Update(float deltaTime, KeyInput* keyInput, const Vector2& stageTilt);
    void Draw(const RenderContext& ctx);
    void DrawDebug(Camera* camera);

    // スライム生成・消去
    Slime* SpawnSlime(const Vector3& pos, int size = 1);
    void SpawnSlimes(const Vector3& basePos, int count, int sizePerSlime = 1);
    void Clear();

    // 合体 / 分裂 / ジャンプ
    void RequestMerge() { mergeRequested_ = true; }
    void TriggerSplit();
    void TriggerJump();
    void TriggerStageBounce(const Vector2& stageTilt, const Vector2& pivot, float bouncePower = 13.5f);

    // カメラ用：全スライムの重心と広がり
    void GetGroupCenterAndSpread(Vector3& outCenter, float& outSpread) const;

    // スライム情報取得
    int GetActiveCount() const;
    int GetTotalCount() const;
    int GetMaxSlimeSize() const;
    int GetTotalSize() const;

    const std::vector<std::unique_ptr<Slime>>& GetSlimes() const { return slimes_; }

    float GetMergeThreshold() const { return mergeThreshold_; }
    void SetMergeThreshold(float t) { mergeThreshold_ = t; }

    float GetSplitPopPower() const { return splitPopPower_; }
    void SetSplitPopPower(float p) { splitPopPower_ = p; }
    float GetSplitUpPower() const { return splitUpPower_; }
    void SetSplitUpPower(float p) { splitUpPower_ = p; }

private:
    void ResolveSeparation(const Vector3& rotation, const Vector2& stageTilt, const Vector2& pivot);
    void CheckAndResolveMerge(const Vector2& stageTilt, const Vector2& pivot);

private:
    Object3dCom* object3dCom_ = nullptr;
    Camera* camera_ = nullptr;
    std::vector<std::unique_ptr<Slime>> slimes_;

    bool mergeRequested_ = false;
    float mergeThreshold_ = 2.5f;
    float splitPopPower_ = 8.0f;
    float splitUpPower_ = 7.0f;
};
