#include "BehaviorNodeFactory.h"
#include "CompositeNodes.h"
#include "CoverNodes.h"

namespace BaziruEngine::AI {

// コンストラクタ：初期化時に組み込み基本ノードを登録
BehaviorNodeFactory::BehaviorNodeFactory() {
    RegisterBuiltinNodes();
}

// シングルトンの実体取得
BehaviorNodeFactory& BehaviorNodeFactory::GetInstance() {
    static BehaviorNodeFactory instance;
    return instance;
}

// 文字列型名からノードインスタンスの生成
std::shared_ptr<BehaviorNode> BehaviorNodeFactory::Create(const std::string& typeName, const nlohmann::json& nodeJson) {
    auto it = creators_.find(typeName);
    if (it != creators_.end()) {
        return it->second(nodeJson); // 登録されたラムダ式を呼び出す
    }
    return nullptr; // 未登録の型名の場合はnullptr
}

// 基本的な制御ノードの登録処理
void BehaviorNodeFactory::RegisterBuiltinNodes() {
    RegisterNode<SelectorNode>("SelectorNode");
    RegisterNode<SequenceNode>("SequenceNode");
    RegisterNode<DetectCoverNode>("DetectCoverNode");
    RegisterNode<MoveToCoverNode>("MoveToCoverNode");
}

} // namespace BaziruEngine::AI
