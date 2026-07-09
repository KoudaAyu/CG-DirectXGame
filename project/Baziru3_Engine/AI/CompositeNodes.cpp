#include "CompositeNodes.h"
#include "BehaviorNodeFactory.h" // ノード動的生成用ファクトリーのインクルード

namespace BaziruEngine::AI {

// ==========================================
// CompositeNode 実装
// ==========================================

void CompositeNode::Deserialize(const nlohmann::json& nodeJson) {
    ClearChildren();

    // "Children"キーが存在し、かつ配列である場合、子ノードを再帰的に生成して登録します
    if (nodeJson.contains("Children") && nodeJson["Children"].is_array()) {
        for (const auto& childJson : nodeJson["Children"]) {
            if (childJson.contains("Type") && childJson["Type"].is_string()) {
                std::string typeName = childJson["Type"];
                // ファクトリーを使って子ノードのインスタンスを生成
                auto childNode = BehaviorNodeFactory::GetInstance().Create(typeName, childJson);
                if (childNode) {
                    AddChild(childNode);
                }
            }
        }
    }
}

// ==========================================
// SelectorNode 実装
// ==========================================
void SelectorNode::OnInitialize(std::shared_ptr<Blackboard> blackboard) {
    currentChildIndex_ = 0;
    // 実行開始時にすべての子供の状態を初期化（リセット）
    for (auto& child : children_) {
        child->Reset();
    }
}

BehaviorStatus SelectorNode::Update(std::shared_ptr<Blackboard> blackboard) {
    if (children_.empty()) {
        return BehaviorStatus::Success; // 子ノードが空の場合は成功として扱う
    }

    // 子ノードを順番に処理
    while (currentChildIndex_ < children_.size()) {
        auto status = children_[currentChildIndex_]->Tick(blackboard);

        // 実行中(Running)または成功(Success)を返したノードがあれば、その状態をそのまま返す
        if (status != BehaviorStatus::Failure) {
            return status;
        }

        // 失敗(Failure)した場合は、次の子ノードの評価に進む
        currentChildIndex_++;
    }

    // 全ての子ノードが失敗した場合は、自身も失敗を返す
    return BehaviorStatus::Failure;
}

// ==========================================
// SequenceNode 実装
// ==========================================
void SequenceNode::OnInitialize(std::shared_ptr<Blackboard> blackboard) {
    currentChildIndex_ = 0;
    // 実行開始時にすべての子供の状態を初期化（リセット）
    for (auto& child : children_) {
        child->Reset();
    }
}

BehaviorStatus SequenceNode::Update(std::shared_ptr<Blackboard> blackboard) {
    if (children_.empty()) {
        return BehaviorStatus::Success; // 子ノードが空の場合は成功として扱う
    }

    // 子ノードを順番に処理
    while (currentChildIndex_ < children_.size()) {
        auto status = children_[currentChildIndex_]->Tick(blackboard);

        // 実行中(Running)または失敗(Failure)を返したノードがあれば、その状態をそのまま返す
        if (status != BehaviorStatus::Success) {
            return status;
        }

        // 成功(Success)した場合は、次の子ノードの評価に進む
        currentChildIndex_++;
    }

    // 全ての子ノードが成功した場合は、自身も成功を返す
    return BehaviorStatus::Success;
}

} // namespace BaziruEngine::AI
