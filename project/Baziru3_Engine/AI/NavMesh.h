#pragma once
#include "Vector.h"
#include <vector>
#include <memory>

namespace BaziruEngine::AI {

/// <summary>
/// A*経路探索用のグリッドノード構造体
/// </summary>
struct NavNode {
    int xIdx = 0;
    int zIdx = 0;
    Vector3 position = { 0.0f, 0.0f, 0.0f };
    bool walkable = true;

    // A*探索アルゴリズム用のコスト変数
    float gCost = 999999.0f;
    float hCost = 0.0f;
    float fCost() const { return gCost + hCost; }
    NavNode* parent = nullptr;
};

/// <summary>
/// グリッド型ナビゲーションメッシュ（NavMesh）生成 ＆ A*経路探索管理クラス
/// </summary>
class NavMesh {
public:
    /// <summary>
    /// シングルトンインスタンスを取得
    /// </summary>
    static NavMesh* GetInstance();

    /// <summary>
    /// 指定された範囲と解像度でナビゲーション用のグリッドを構築します
    /// </summary>
    void BuildGrid(float minX, float maxX, float minZ, float maxZ, float gridSize, float agentRadius);

    /// <summary>
    /// スタート地点からゴール地点への最短経路をA*アルゴリズムを用いて探索します
    /// </summary>
    std::vector<Vector3> FindPath(const Vector3& start, const Vector3& end);

    /// <summary>
    /// 指定された座標が通行可能（遮蔽物に衝突しない）かを判定します
    /// </summary>
    bool IsWalkable(const Vector3& pos, float agentRadius);

    // グリッド境界やパラメータへのゲッター
    float GetMinX() const { return minX_; }
    float GetMaxX() const { return maxX_; }
    float GetMinZ() const { return minZ_; }
    float GetMaxZ() const { return maxZ_; }
    float GetGridSize() const { return gridSize_; }

private:
    NavMesh() = default;
    ~NavMesh() = default;
    NavMesh(const NavMesh&) = delete;
    NavMesh& operator=(const NavMesh&) = delete;

    // インデックスからノードを取得するヘルパー
    NavNode* GetNode(int xIdx, int zIdx);
    // 指定した座標に最も近いノードを取得するヘルパー
    NavNode* GetClosestNode(const Vector3& pos);
    // 隣接ノードを取得するヘルパー
    std::vector<NavNode*> GetNeighbors(NavNode* node);

private:
    std::vector<std::vector<NavNode>> grid_;
    float minX_ = -20.0f;
    float maxX_ = 20.0f;
    float minZ_ = -5.0f;
    float maxZ_ = 45.0f;
    float gridSize_ = 0.5f;
    float agentRadius_ = 0.5f;
    int width_ = 0;
    int height_ = 0;
};

} // namespace BaziruEngine::AI
