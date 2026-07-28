#pragma once
#include "BehaviorNode.h"
#include <vector>

namespace BaziruEngine::AI {

// 複数の子ノードを保持し、その実行順序を制御する中間ノードの基底クラス
class CompositeNode : public BehaviorNode {
public:
    CompositeNode() = default;
    virtual ~CompositeNode() = default;

    // 子ノードを追加します
    void AddChild(std::shared_ptr<BehaviorNode> child) {
        children_.push_back(child);
    }

    // すべての子ノードをクリアします
    void ClearChildren() {
        children_.clear();
    }

    // JSONデータから子ノード群を動的に生成してロードします
    virtual void Deserialize(const nlohmann::json& nodeJson) override;

protected:
    std::vector<std::shared_ptr<BehaviorNode>> children_; // 子ノードのリスト
};

// SelectorNode: 子ノードを左から順に実行し、いずれか1つでも Success または Running を返したら処理を中断して同じステータスを返します。
// （すべての子ノードが Failure を返したときのみ Failure を返します。いわゆる OR 条件やフォールバック挙動です）
class SelectorNode : public CompositeNode {
public:
    SelectorNode() = default;
    virtual ~SelectorNode() = default;

protected:
    virtual void OnInitialize(std::shared_ptr<Blackboard> blackboard) override;
    virtual BehaviorStatus Update(std::shared_ptr<Blackboard> blackboard) override;

private:
    size_t currentChildIndex_ = 0; // 現在実行中の子ノードのインデックス
};

// SequenceNode: 子ノードを左から順に実行し、すべてが Success を返すまで順に処理します。
// （途中で 1つでも Failure または Running を返した場合は、そこで処理を中断してそのステータスを返します。いわゆる AND 条件や一連のアクションシーケンスです）
class SequenceNode : public CompositeNode {
public:
    SequenceNode() = default;
    virtual ~SequenceNode() = default;

protected:
    virtual void OnInitialize(std::shared_ptr<Blackboard> blackboard) override;
    virtual BehaviorStatus Update(std::shared_ptr<Blackboard> blackboard) override;

private:
    size_t currentChildIndex_ = 0; // 現在実行中の子ノードのインデックス
};

} // namespace BaziruEngine::AI
