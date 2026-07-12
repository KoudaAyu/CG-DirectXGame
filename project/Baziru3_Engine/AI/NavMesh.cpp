#include "NavMesh.h"
#include "../Collision/CollisionManager.h"
#include "../Collision/SphereCollider.h"
#include "../Collision/BoxCollider.h"
#include "../Collision/CapsuleCollider.h"
#include "../Scene/Manager/SceneManager.h"
#include <queue>
#include <unordered_set>
#include <algorithm>
#include <cmath>

namespace {
    // ヘルパー: 距離を計算
    float Distance(const Vector3& a, const Vector3& b) {
        float dx = a.x - b.x;
        float dy = a.y - b.y;
        float dz = a.z - b.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    // A*のノード比較用構造体（優先度キューで使用）
    struct NodeComparer {
        bool operator()(const BaziruEngine::AI::NavNode* lhs, const BaziruEngine::AI::NavNode* rhs) const {
            return lhs->fCost() > rhs->fCost(); // fCostが小さい方を優先
        }
    };
}

namespace BaziruEngine::AI {

NavMesh* NavMesh::GetInstance() {
    static NavMesh instance;
    return &instance;
}

void NavMesh::BuildGrid(float minX, float maxX, float minZ, float maxZ, float gridSize, float agentRadius) {
    minX_ = minX;
    maxX_ = maxX;
    minZ_ = minZ;
    maxZ_ = maxZ;
    gridSize_ = gridSize;
    agentRadius_ = agentRadius;

    width_ = static_cast<int>((maxX_ - minX_) / gridSize_) + 1;
    height_ = static_cast<int>((maxZ_ - minZ_) / gridSize_) + 1;

    grid_.clear();
    grid_.resize(width_, std::vector<NavNode>(height_));

    for (int x = 0; x < width_; ++x) {
        for (int z = 0; z < height_; ++z) {
            NavNode& node = grid_[x][z];
            node.xIdx = x;
            node.zIdx = z;
            node.position = {
                minX_ + static_cast<float>(x) * gridSize_,
                0.0f,
                minZ_ + static_cast<float>(z) * gridSize_
            };
            // 障害物との衝突判定を行って通行可能かをチェック
            node.walkable = IsWalkable(node.position, agentRadius_);
        }
    }
}

bool NavMesh::IsWalkable(const Vector3& pos, float agentRadius) {
    // 衝突判定データ（エージェント）の作成（Y軸を少し浮かせて障害物との当たり判定が正しく乗るようにする）
    CollisionData agentData;
    agentData.type = ColliderType::Sphere;
    agentData.attribute = CollisionAttribute::Enemy;
    agentData.worldPosition = pos;
    agentData.worldPosition.y = 0.5f;
    agentData.shape.radius = agentRadius;

    // マネージャー内の全ての障害物コライダーと判定を行う
    auto& colliders = CollisionManager::GetInstance()->GetColliders();
    for (Collider* col : colliders) {
        if (!col || !col->IsEnabled() || col->GetAttribute() != CollisionAttribute::Obstacle) {
            continue;
        }

        // コライダーからCollisionDataを再構築 (DODと同様)
        CollisionData colData;
        colData.originalCollider = col;
        colData.type = col->GetType();
        colData.attribute = col->GetAttribute();
        colData.worldPosition = col->GetWorldPosition();
        colData.isTrigger = col->IsTrigger();

        if (colData.type == ColliderType::Sphere) {
            SphereCollider* sphere = static_cast<SphereCollider*>(col);
            colData.shape.radius = sphere->GetRadius();
        }
        else if (colData.type == ColliderType::Box) {
            BoxCollider* box = static_cast<BoxCollider*>(col);
            colData.shape.size = box->GetSize();
            colData.shape.rotation = box->GetWorldRotation();
        }
        else if (colData.type == ColliderType::Capsule) {
            CapsuleCollider* capsule = static_cast<CapsuleCollider*>(col);
            colData.shape.radius = capsule->GetRadius();
            colData.shape.height = capsule->GetHeight();
        }

        Vector3 pushDir;
        float pushLen;
        bool intersects = false;

        if (colData.type == ColliderType::Sphere) {
            intersects = CollisionManager::CheckSphereSphere(agentData, colData, pushDir, pushLen);
        }
        else if (colData.type == ColliderType::Box) {
            intersects = CollisionManager::CheckSphereBox(agentData, colData, pushDir, pushLen);
        }
        else if (colData.type == ColliderType::Capsule) {
            intersects = CollisionManager::CheckSphereCapsule(agentData, colData, pushDir, pushLen);
        }

        if (intersects) {
            return false; // 障害物に接触しているため侵入不可
        }
    }

    return true; // どの障害物にも当たらなかったので通行可能
}

NavNode* NavMesh::GetNode(int xIdx, int zIdx) {
    if (xIdx >= 0 && xIdx < width_ && zIdx >= 0 && zIdx < height_) {
        return &grid_[xIdx][zIdx];
    }
    return nullptr;
}

NavNode* NavMesh::GetClosestNode(const Vector3& pos) {
    int x = static_cast<int>(std::round((pos.x - minX_) / gridSize_));
    int z = static_cast<int>(std::round((pos.z - minZ_) / gridSize_));
    
    // 範囲外の場合は範囲内にクランプする
    x = (std::max)(0, (std::min)(x, width_ - 1));
    z = (std::max)(0, (std::min)(z, height_ - 1));

    return &grid_[x][z];
}

std::vector<NavNode*> NavMesh::GetNeighbors(NavNode* node) {
    std::vector<NavNode*> neighbors;
    if (!node) return neighbors;

    // 周囲8方向のノードを走査
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dz = -1; dz <= 1; ++dz) {
            if (dx == 0 && dz == 0) continue;

            NavNode* neighbor = GetNode(node->xIdx + dx, node->zIdx + dz);
            if (neighbor && neighbor->walkable) {
                // 斜め移動時の「角抜け」を防ぐため、隣接する縦横が両方壁の場合は斜め移動不可にする
                if (dx != 0 && dz != 0) {
                    NavNode* side1 = GetNode(node->xIdx + dx, node->zIdx);
                    NavNode* side2 = GetNode(node->xIdx, node->zIdx + dz);
                    if ((side1 && !side1->walkable) || (side2 && !side2->walkable)) {
                        continue; // 角抜け防止でスキップ
                    }
                }
                neighbors.push_back(neighbor);
            }
        }
    }
    return neighbors;
}

std::vector<Vector3> NavMesh::FindPath(const Vector3& start, const Vector3& end) {
    // 念のため、実行前にグリッドが未初期化なら初期化する
    if (grid_.empty()) {
        BuildGrid(-20.0f, 20.0f, -5.0f, 45.0f, 0.5f, 0.5f);
    }

    NavNode* startNode = GetClosestNode(start);
    NavNode* endNode = GetClosestNode(end);

    std::vector<Vector3> path;
    if (!startNode || !endNode) return path;

    // もしスタート地点または終了地点が侵入不可の場合は、周囲の通行可能な最寄りのノードを探す
    if (!startNode->walkable) {
        float minDist = 999999.0f;
        NavNode* alternativeNode = nullptr;
        for (int radius = 1; radius <= 4; ++radius) {
            for (int dx = -radius; dx <= radius; ++dx) {
                for (int dz = -radius; dz <= radius; ++dz) {
                    NavNode* temp = GetNode(startNode->xIdx + dx, startNode->zIdx + dz);
                    if (temp && temp->walkable) {
                        float dist = Distance(start, temp->position);
                        if (dist < minDist) {
                            minDist = dist;
                            alternativeNode = temp;
                        }
                    }
                }
            }
            if (alternativeNode) break;
        }
        if (alternativeNode) startNode = alternativeNode;
    }

    if (!endNode->walkable) {
        float minDist = 999999.0f;
        NavNode* alternativeNode = nullptr;
        for (int radius = 1; radius <= 4; ++radius) {
            for (int dx = -radius; dx <= radius; ++dx) {
                for (int dz = -radius; dz <= radius; ++dz) {
                    NavNode* temp = GetNode(endNode->xIdx + dx, endNode->zIdx + dz);
                    if (temp && temp->walkable) {
                        float dist = Distance(end, temp->position);
                        if (dist < minDist) {
                            minDist = dist;
                            alternativeNode = temp;
                        }
                    }
                }
            }
            if (alternativeNode) break;
        }
        if (alternativeNode) endNode = alternativeNode;
    }

    // 全てのノードのコストを初期化
    for (int x = 0; x < width_; ++x) {
        for (int z = 0; z < height_; ++z) {
            grid_[x][z].gCost = 999999.0f;
            grid_[x][z].parent = nullptr;
        }
    }

    std::priority_queue<NavNode*, std::vector<NavNode*>, NodeComparer> openSet;
    std::unordered_set<NavNode*> closedSet;

    startNode->gCost = 0.0f;
    startNode->hCost = Distance(startNode->position, endNode->position);
    openSet.push(startNode);

    bool success = false;
    while (!openSet.empty()) {
        NavNode* current = openSet.top();
        openSet.pop();

        if (closedSet.count(current)) continue;
        closedSet.insert(current);

        if (current == endNode) {
            success = true;
            break;
        }

        for (NavNode* neighbor : GetNeighbors(current)) {
            if (closedSet.count(neighbor)) continue;

            float moveCost = Distance(current->position, neighbor->position);
            float tentativeGCost = current->gCost + moveCost;

            if (tentativeGCost < neighbor->gCost) {
                neighbor->parent = current;
                neighbor->gCost = tentativeGCost;
                neighbor->hCost = Distance(neighbor->position, endNode->position);
                openSet.push(neighbor);
            }
        }
    }

    if (success) {
        // 親を辿ってパスを復元する
        std::vector<Vector3> rawPath;
        NavNode* current = endNode;
        while (current != nullptr) {
            rawPath.push_back(current->position);
            current = current->parent;
        }
        std::reverse(rawPath.begin(), rawPath.end());

        // 経路最適化：直線で進める中間地点を間引く（経路の滑らかさ向上とノード数圧縮）
        if (rawPath.size() > 2) {
            path.push_back(rawPath.front());
            for (size_t i = 1; i < rawPath.size() - 1; ++i) {
                Vector3 last = path.back();
                Vector3 next = rawPath[i + 1];

                // last から next までの間に障害物がないかレイキャストで確認
                Vector3 toNext = next - last;
                float dist = Distance(last, next);
                Vector3 dir = { toNext.x / dist, 0.0f, toNext.z / dist };

                Collider* hitCollider = nullptr;
                float hitDist = 0.0f;
                Vector3 rayStart = last + Vector3{ 0.0f, 0.5f, 0.0f }; // 少し浮かせる

                if (CollisionManager::GetInstance()->Raycast(rayStart, dir, dist, hitCollider, hitDist)) {
                    if (hitCollider && hitCollider->GetAttribute() == CollisionAttribute::Obstacle) {
                        // 障害物に衝突するため、直前のノードをパスとして追加してカドを曲がらせる
                        path.push_back(rawPath[i]);
                    }
                }
            }
            path.push_back(rawPath.back());
        }
        else {
            path = rawPath;
        }
    }

    return path;
}

} // namespace BaziruEngine::AI
