#pragma once
#include "BehaviorNode.h"
#include <string>

namespace BaziruEngine::AI {

/// <summary>
/// 現在位置から対象座標（脅威、あるいは避難地点）への最短経路（A*）を計算してBlackboardに格納するアクションノード
/// </summary>
class FindPathNode : public BehaviorNode {
public:
    FindPathNode() = default;
    virtual ~FindPathNode() = default;

    virtual void Deserialize(const nlohmann::json& nodeJson) override;

protected:
    virtual BehaviorStatus Update(std::shared_ptr<Blackboard> blackboard) override;

private:
    std::string targetKey_ = "ThreatPosition"; // 目標座標のBlackboardキー
    std::string pathKey_ = "Path";             // 出力経路のBlackboardキー
    float agentRadius_ = 0.5f;                 // エージェントの半径サイズ
};

/// <summary>
/// Blackboardに保存された経路点（Path）に沿ってエージェントを移動させるアクションノード
/// </summary>
class FollowPathNode : public BehaviorNode {
public:
    FollowPathNode() = default;
    virtual ~FollowPathNode() = default;

    virtual void Deserialize(const nlohmann::json& nodeJson) override;

protected:
    virtual BehaviorStatus Update(std::shared_ptr<Blackboard> blackboard) override;

private:
    float speed_ = 3.0f;                       // 移動速度
    std::string pathKey_ = "Path";             // 入力経路のBlackboardキー
    float arrivalTolerance_ = 0.2f;            // 各中間ノードに到達したと判定する閾値
};

} // namespace BaziruEngine::AI
