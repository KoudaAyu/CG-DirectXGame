#pragma once
#include <memory>
#include <nlohmann/json.hpp> // JSONライブラリの読み込み

namespace BaziruEngine::AI {

class Blackboard;

// ノードの実行状態を表す列挙型
enum class BehaviorStatus {
    Invalid,  // 初期状態（まだ実行されていない）
    Running,  // 実行中（次のフレームも継続して処理する）
    Success,  // 成功終了
    Failure   // 失敗終了
};

// 全てのビヘイビアノードの抽象基底クラス
class BehaviorNode {
public:
    BehaviorNode() = default;
    virtual ~BehaviorNode() = default;

    // 毎フレーム呼び出されるメインの更新処理
    BehaviorStatus Tick(std::shared_ptr<Blackboard> blackboard);

    // 現在の実行状態を取得
    BehaviorStatus GetStatus() const { return status_; }

    // 状態をリセットし、初期状態（Invalid）に戻します
    void Reset() { status_ = BehaviorStatus::Invalid; }

    // JSONデータからノードパラメータをロードします（必要に応じて派生クラスでオーバーライド）
    virtual void Deserialize(const nlohmann::json& nodeJson) {}

protected:
    // ノードが開始されたときの初期化処理（初回Updateの直前に呼ばれる）
    virtual void OnInitialize(std::shared_ptr<Blackboard> blackboard) {}

    // 毎フレームの更新ロジック（派生クラスで実装する）
    virtual BehaviorStatus Update(std::shared_ptr<Blackboard> blackboard) = 0;

    // ノードが終了したときのクリーンアップ処理（Success/Failureが確定した直後に呼ばれる）
    virtual void OnTerminate(std::shared_ptr<Blackboard> blackboard, BehaviorStatus status) {}

private:
    BehaviorStatus status_ = BehaviorStatus::Invalid; // ノードの現在の実行状態
};

} // namespace BaziruEngine::AI
