#include "PathfindingNodes.h"
#include "Blackboard.h"
#include "NavMesh.h"
#include <cmath>

namespace BaziruEngine::AI {

// =================================================================
// FindPathNode 実装
// =================================================================

void FindPathNode::Deserialize(const nlohmann::json& nodeJson) {
    if (nodeJson.contains("TargetKey") && nodeJson["TargetKey"].is_string()) {
        targetKey_ = nodeJson["TargetKey"];
    }
    if (nodeJson.contains("PathKey") && nodeJson["PathKey"].is_string()) {
        pathKey_ = nodeJson["PathKey"];
    }
    if (nodeJson.contains("AgentRadius") && nodeJson["AgentRadius"].is_number()) {
        agentRadius_ = nodeJson["AgentRadius"];
    }
}

BehaviorStatus FindPathNode::Update(std::shared_ptr<Blackboard> blackboard) {
    if (!blackboard->Has("AgentPosition") || !blackboard->Has(targetKey_)) {
        return BehaviorStatus::Failure;
    }

    Vector3 start = blackboard->Get<Vector3>("AgentPosition");
    Vector3 target = blackboard->Get<Vector3>(targetKey_);

    // 侵入可能グリッドを更新した上でA*探索を実行
    NavMesh::GetInstance()->BuildGrid(-20.0f, 20.0f, -5.0f, 45.0f, 0.5f, agentRadius_);
    std::vector<Vector3> path = NavMesh::GetInstance()->FindPath(start, target);

    if (path.empty()) {
        return BehaviorStatus::Failure; // 経路が見つからなかった
    }

    blackboard->Set<std::vector<Vector3>>(pathKey_, path);
    return BehaviorStatus::Success;
}

// =================================================================
// FollowPathNode 実装
// =================================================================

void FollowPathNode::Deserialize(const nlohmann::json& nodeJson) {
    if (nodeJson.contains("Speed") && nodeJson["Speed"].is_number()) {
        speed_ = nodeJson["Speed"];
    }
    if (nodeJson.contains("PathKey") && nodeJson["PathKey"].is_string()) {
        pathKey_ = nodeJson["PathKey"];
    }
    if (nodeJson.contains("ArrivalTolerance") && nodeJson["ArrivalTolerance"].is_number()) {
        arrivalTolerance_ = nodeJson["ArrivalTolerance"];
    }
}

BehaviorStatus FollowPathNode::Update(std::shared_ptr<Blackboard> blackboard) {
    if (!blackboard->Has(pathKey_)) {
        return BehaviorStatus::Failure;
    }

    std::vector<Vector3> path = blackboard->Get<std::vector<Vector3>>(pathKey_);
    if (path.empty()) {
        return BehaviorStatus::Success; // 既に経路の終点に到達している
    }

    Vector3 agentPos = blackboard->Get<Vector3>("AgentPosition");

    // 経路の最初の目標点（チェックポイント）を設定
    Vector3 targetPt = path.front();
    Vector3 toTarget = targetPt - agentPos;
    toTarget.y = 0.0f; // 高さは無視して2D平面で評価

    float dist = std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z);

    // チェックポイントに到達したとみなせる場合、その点を除外して次の点へ
    while (dist < arrivalTolerance_) {
        path.erase(path.begin());
        blackboard->Set<std::vector<Vector3>>(pathKey_, path);

        if (path.empty()) {
            return BehaviorStatus::Success; // 全経路の走破完了
        }

        targetPt = path.front();
        toTarget = targetPt - agentPos;
        toTarget.y = 0.0f;
        dist = std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z);
    }

    // 次のチェックポイントへ向けて移動補間
    float dt = 1.0f / 60.0f; // 固定60FPSステップ
    float moveDist = speed_ * dt;

    if (moveDist >= dist) {
        // このステップで次のチェックポイントにピッタリ到達する
        blackboard->Set<Vector3>("AgentPosition", targetPt);
        path.erase(path.begin());
        blackboard->Set<std::vector<Vector3>>(pathKey_, path);

        if (path.empty()) {
            return BehaviorStatus::Success;
        }
    } else {
        Vector3 dir = { toTarget.x / dist, 0.0f, toTarget.z / dist };
        Vector3 newPos = agentPos + dir * moveDist;
        blackboard->Set<Vector3>("AgentPosition", newPos);
    }

    return BehaviorStatus::Running;
}

} // namespace BaziruEngine::AI
