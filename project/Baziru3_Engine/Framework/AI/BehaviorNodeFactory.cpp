#include "BehaviorNodeFactory.h"
#include "CompositeNodes.h"
#include "CoverNodes.h"
#include "PathfindingNodes.h"

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
    try {
        auto it = creators_.find(typeName);
        if (it != creators_.end()) {
            return it->second(nodeJson); // 登録されたラムダ式を呼び出す
        } else {
            std::string warn = "[BehaviorNodeFactory Warning] Node type '" + typeName + "' is not registered. Skipping node creation.\n";
            OutputDebugStringA(warn.c_str());
        }
    } catch (const std::exception& e) {
        std::string err = "[BehaviorNodeFactory Error] Exception while creating node '" + typeName + "': " + e.what() + "\n";
        OutputDebugStringA(err.c_str());
    } catch (...) {
        std::string err = "[BehaviorNodeFactory Error] Unknown exception while creating node '" + typeName + "'.\n";
        OutputDebugStringA(err.c_str());
    }
    return nullptr; // 未登録またはエラー時はnullptr
}

// 基本的な制御ノードの登録処理
void BehaviorNodeFactory::RegisterBuiltinNodes() {
    RegisterNode<SelectorNode>("SelectorNode");
    RegisterNode<SequenceNode>("SequenceNode");
    RegisterNode<DetectCoverNode>("DetectCoverNode");
    RegisterNode<MoveToCoverNode>("MoveToCoverNode");
    RegisterNode<FindPathNode>("FindPathNode");
    RegisterNode<FollowPathNode>("FollowPathNode");
}

} // namespace BaziruEngine::AI
