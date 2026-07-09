#pragma once
#include "BehaviorNode.h"
#include "Blackboard.h"
#include <memory>
#include <string>

namespace BaziruEngine::AI {

// =================================================================
// BehaviorTree
// -----------------------------------------------------------------
// ビヘイビアツリー全体を管理・実行し、個体用メモリ(Blackboard)を保持するクラス。
// =================================================================
class BehaviorTree {
public:
    BehaviorTree() = default;
    ~BehaviorTree() = default;

    // ルートノードを設定
    void SetRoot(std::shared_ptr<BehaviorNode> root) { root_ = root; }
    
    // ルートノードを取得
    std::shared_ptr<BehaviorNode> GetRoot() const { return root_; }

    // Blackboard（共有メモリ）への参照を取得
    std::shared_ptr<Blackboard> GetBlackboard() { return blackboard_; }

    // 毎フレーム呼び出し、ツリー全体の更新（意思決定）を評価します
    BehaviorStatus Update() {
        if (root_) {
            return root_->Tick(blackboard_);
        }
        return BehaviorStatus::Invalid;
    }

    // JSONアセットファイルからツリーの構造をロードし、再構成します
    bool LoadFromJSON(const std::string& filePath);

    // エンジン初期化時に自動でBTの動作をテストするための静的関数
    static void ExecuteTests();

private:
    std::shared_ptr<BehaviorNode> root_; // ツリーの最上位ルートノード
    std::shared_ptr<Blackboard> blackboard_ = std::make_shared<Blackboard>(); // 個体用Blackboardメモリ
};

} // namespace BaziruEngine::AI
