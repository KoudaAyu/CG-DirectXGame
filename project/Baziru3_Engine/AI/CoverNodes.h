#pragma once
#include "BehaviorNode.h"
#include "Vector.h"
#include <string>

namespace BaziruEngine::AI {

/// <summary>
/// 周囲にレイを飛ばし、隠れられる遮蔽物を検知するアクションノード
/// </summary>
class DetectCoverNode : public BehaviorNode {
public:
    DetectCoverNode() = default;
    virtual ~DetectCoverNode() = default;

    virtual void Deserialize(const nlohmann::json& nodeJson) override;

protected:
    virtual BehaviorStatus Update(std::shared_ptr<Blackboard> blackboard) override;

private:
    int rayCount_ = 8;             // レイの本数（周囲分割数）
    float maxDistance_ = 15.0f;     // 検知最大距離
    float minCoverDistance_ = 1.5f; // 壁から少し離れたカバーポジションへのオフセット
};

/// <summary>
/// Blackboardに保存されたCoverPositionへ向かって移動するアクションノード
/// </summary>
class MoveToCoverNode : public BehaviorNode {
public:
    MoveToCoverNode() = default;
    virtual ~MoveToCoverNode() = default;

    virtual void Deserialize(const nlohmann::json& nodeJson) override;

protected:
    virtual BehaviorStatus Update(std::shared_ptr<Blackboard> blackboard) override;

private:
    float speed_ = 3.0f;           // 移動速度
};

} // namespace BaziruEngine::AI
