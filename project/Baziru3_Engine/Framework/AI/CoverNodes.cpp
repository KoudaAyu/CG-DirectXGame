#include "CoverNodes.h"
#include "Blackboard.h"
#include "../Collision/CollisionManager.h"
#include <cmath>

namespace {
    // ヘルパー: Vector3の長さを計算
    float Length(const Vector3& v) {
        return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    }

    // ヘルパー: Vector3を正規化
    Vector3 Normalize(const Vector3& v) {
        float len = Length(v);
        if (len > 0.0001f) {
            return { v.x / len, v.y / len, v.z / len };
        }
        return { 0.0f, 0.0f, 0.0f };
    }
}

namespace BaziruEngine::AI {

// =================================================================
// DetectCoverNode 実装
// =================================================================

void DetectCoverNode::Deserialize(const nlohmann::json& nodeJson) {
    if (nodeJson.contains("RayCount") && nodeJson["RayCount"].is_number_integer()) {
        rayCount_ = nodeJson["RayCount"];
    }
    if (nodeJson.contains("MaxDistance") && nodeJson["MaxDistance"].is_number()) {
        maxDistance_ = nodeJson["MaxDistance"];
    }
    if (nodeJson.contains("MinCoverDistance") && nodeJson["MinCoverDistance"].is_number()) {
        minCoverDistance_ = nodeJson["MinCoverDistance"];
    }
}

BehaviorStatus DetectCoverNode::Update(std::shared_ptr<Blackboard> blackboard) {
    Vector3 agentPos = blackboard->Get<Vector3>("AgentPosition", { 0.0f, 0.0f, 0.0f });
    
    Vector3 threatPos = { 0.0f, 0.0f, 0.0f };
    bool hasThreat = blackboard->Has("ThreatPosition");
    if (hasThreat) {
        threatPos = blackboard->Get<Vector3>("ThreatPosition");
    }

    float closestCoverDist = 999999.0f;
    Vector3 bestCoverPos = { 0.0f, 0.0f, 0.0f };
    bool foundCover = false;

    float angleStep = 2.0f * 3.14159265f / static_cast<float>(rayCount_);
    for (int i = 0; i < rayCount_; ++i) {
        float angle = static_cast<float>(i) * angleStep;
        Vector3 rayDir = { std::cos(angle), 0.0f, std::sin(angle) };

        Collider* hitCollider = nullptr;
        float hitDist = 0.0f;
        
        // レイキャストの開始位置を少し浮かせる（地面等への接触を避けるため）
        Vector3 rayStart = agentPos + Vector3{ 0.0f, 0.5f, 0.0f };

        if (CollisionManager::GetInstance()->Raycast(rayStart, rayDir, maxDistance_, hitCollider, hitDist)) {
            if (hitCollider && hitCollider->GetAttribute() == CollisionAttribute::Obstacle) {
                // レイの衝突地点から、レイの逆方向に minCoverDistance_ 分だけ手前に引いた位置を候補とする
                Vector3 hitPoint = rayStart + rayDir * hitDist;
                Vector3 coverCandidate = hitPoint - rayDir * minCoverDistance_;
                coverCandidate.y = agentPos.y; // 高さはエージェントの現在位置に合わせる

                bool isSafe = true;
                if (hasThreat) {
                    // カバー候補地点から脅威位置（プレイヤー）に向けてレイを飛ばす
                    Vector3 toThreat = threatPos - coverCandidate;
                    float distToThreat = Length(toThreat);
                    Vector3 dirToThreat = Normalize(toThreat);

                    Collider* threatHitCollider = nullptr;
                    float threatHitDist = 0.0f;
                    Vector3 testStart = coverCandidate + Vector3{ 0.0f, 0.5f, 0.0f };

                    if (CollisionManager::GetInstance()->Raycast(testStart, dirToThreat, distToThreat, threatHitCollider, threatHitDist)) {
                        // 脅威に直面する前に障害物に当たっていれば、そこは脅威から見えない安全なカバーポイントである
                        if (threatHitCollider && threatHitCollider->GetAttribute() == CollisionAttribute::Obstacle) {
                            isSafe = true;
                        } else {
                            isSafe = false;
                        }
                    } else {
                        // 何も遮らないということは丸見えである
                        isSafe = false;
                    }
                }

                if (isSafe) {
                    float distToAgent = Length(coverCandidate - agentPos);
                    if (distToAgent < closestCoverDist) {
                        closestCoverDist = distToAgent;
                        bestCoverPos = coverCandidate;
                        foundCover = true;
                    }
                }
            }
        }
    }

    if (foundCover) {
        blackboard->Set<Vector3>("CoverPosition", bestCoverPos);
        return BehaviorStatus::Success;
    }

    return BehaviorStatus::Failure;
}

// =================================================================
// MoveToCoverNode 実装
// =================================================================

void MoveToCoverNode::Deserialize(const nlohmann::json& nodeJson) {
    if (nodeJson.contains("Speed") && nodeJson["Speed"].is_number()) {
        speed_ = nodeJson["Speed"];
    }
}

BehaviorStatus MoveToCoverNode::Update(std::shared_ptr<Blackboard> blackboard) {
    if (!blackboard->Has("CoverPosition")) {
        return BehaviorStatus::Failure;
    }

    Vector3 agentPos = blackboard->Get<Vector3>("AgentPosition", { 0.0f, 0.0f, 0.0f });
    Vector3 coverPos = blackboard->Get<Vector3>("CoverPosition");

    Vector3 toCover = coverPos - agentPos;
    float dist = Length(toCover);

    // 既に十分に近づいている場合は即座に成功
    if (dist < 0.1f) {
        blackboard->Set<Vector3>("AgentPosition", coverPos);
        return BehaviorStatus::Success;
    }

    // 移動処理（簡易的な直線移動補間）
    float dt = 1.0f / 60.0f; // 固定60FPSを想定
    float moveDist = speed_ * dt;

    if (moveDist >= dist) {
        blackboard->Set<Vector3>("AgentPosition", coverPos);
        return BehaviorStatus::Success;
    } else {
        Vector3 dir = Normalize(toCover);
        Vector3 newPos = agentPos + dir * moveDist;
        blackboard->Set<Vector3>("AgentPosition", newPos);
        return BehaviorStatus::Running;
    }
}

} // namespace BaziruEngine::AI
