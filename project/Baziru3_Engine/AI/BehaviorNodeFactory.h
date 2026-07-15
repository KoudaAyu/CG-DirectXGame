#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>

namespace BaziruEngine::AI {

class BehaviorNode;

// =================================================================
// BehaviorNodeFactory
// -----------------------------------------------------------------
// ノード名（文字列）から対応するC++クラスのインスタンスを動的生成するための
// シングルトンファクトリークラス。
// =================================================================
class BehaviorNodeFactory {
public:
    // JSONデータを元に、対応するノードをインスタンス化・デシリアライズする生成関数の型
    using CreatorFunc = std::function<std::shared_ptr<BehaviorNode>(const nlohmann::json&)>;

    // シングルトンインスタンスを取得
    static BehaviorNodeFactory& GetInstance();

    // 新しいノード型を登録します
    // T: 登録したいBehaviorNode派生クラスの型
    // typeName: JSONに記述するノード名（例: "MoveToCoverTask"）
    template<typename T>
    void RegisterNode(const std::string& typeName) {
        creators_[typeName] = [](const nlohmann::json& nodeJson) {
            auto node = std::make_shared<T>();
            node->Deserialize(nodeJson); // ロード時にノード特有のパラメータ解析を行う
            return node;
        };
    }

    // 指定したノード型名に基づいて、インスタンスを生成・デシリアライズして返します
    std::shared_ptr<BehaviorNode> Create(const std::string& typeName, const nlohmann::json& nodeJson);

private:
    // コンストラクタ等を隠蔽（シングルトン設計）
    BehaviorNodeFactory();
    ~BehaviorNodeFactory() = default;
    BehaviorNodeFactory(const BehaviorNodeFactory&) = delete;
    BehaviorNodeFactory& operator=(const BehaviorNodeFactory&) = delete;

    // エンジン組み込みの基本ノード（Selector, Sequence）を初期登録するメソッド
    void RegisterBuiltinNodes();

private:
    std::unordered_map<std::string, CreatorFunc> creators_; // 登録された生成関数のマップ
};

} // namespace BaziruEngine::AI
