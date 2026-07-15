#include "BehaviorTree.h"
#include "CompositeNodes.h"
#include "BehaviorNodeFactory.h" // ノード生成用ファクトリーのインクルード
#include "../Base/Vector.h"
#include <fstream>
#include <Windows.h>
#include <string>

namespace BaziruEngine::AI {

// ==========================================
// テスト用モックノードの定義
// ==========================================

// テスト用：コンソール（デバッガ）へのログ出力タスクノード
class TestPrintNode : public BehaviorNode {
public:
    TestPrintNode() = default;
protected:
    virtual BehaviorStatus Update(std::shared_ptr<Blackboard> blackboard) override {
        std::string logMsg = "[BT Test] PrintNode: " + message_ + "\n";
        OutputDebugStringA(logMsg.c_str());
        return BehaviorStatus::Success;
    }
public:
    // JSONから固有パラメータを解析
    virtual void Deserialize(const nlohmann::json& nodeJson) override {
        if (nodeJson.contains("Message") && nodeJson["Message"].is_string()) {
            message_ = nodeJson["Message"];
        }
    }
private:
    std::string message_ = "";
};

// テスト用：指定フレーム数分だけRunningを返す待機タスクノード
class TestWaitNode : public BehaviorNode {
public:
    TestWaitNode() = default;
protected:
    virtual void OnInitialize(std::shared_ptr<Blackboard> blackboard) override {
        currentFrame_ = 0;
        OutputDebugStringA("[BT Test] WaitNode Started\n");
    }
    virtual BehaviorStatus Update(std::shared_ptr<Blackboard> blackboard) override {
        currentFrame_++;
        if (currentFrame_ >= targetFrames_) {
            OutputDebugStringA("[BT Test] WaitNode Finished\n");
            return BehaviorStatus::Success;
        }
        return BehaviorStatus::Running;
    }
public:
    // JSONから固有パラメータを解析
    virtual void Deserialize(const nlohmann::json& nodeJson) override {
        if (nodeJson.contains("Frames") && nodeJson["Frames"].is_number_integer()) {
            targetFrames_ = nodeJson["Frames"];
        }
    }
private:
    int targetFrames_ = 0;
    int currentFrame_ = 0;
};

// ==========================================
// BehaviorTree 実装
// ==========================================

// JSONファイルからツリーの動的読み込みと再構築
bool BehaviorTree::LoadFromJSON(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::string err = "[BT Error] Failed to open JSON file: " + filePath + "\n";
        OutputDebugStringA(err.c_str());
        return false;
    }

    nlohmann::json rootJson;
    try {
        file >> rootJson;
    } catch (const nlohmann::json::parse_error& e) {
        std::string err = "[BT Error] JSON parse error: " + std::string(e.what()) + "\n";
        OutputDebugStringA(err.c_str());
        return false;
    }

    // 最上位ノードのType文字列の存在チェック
    if (!rootJson.contains("Type") || !rootJson["Type"].is_string()) {
        OutputDebugStringA("[BT Error] Root node must have a valid \"Type\" string.\n");
        return false;
    }

    std::string typeName = rootJson["Type"];
    
    // ファクトリーを使用してルートノードおよび配下の子ノードを再帰生成
    auto rootNode = BehaviorNodeFactory::GetInstance().Create(typeName, rootJson);
    if (rootNode) {
        SetRoot(rootNode);
        return true;
    }

    OutputDebugStringA("[BT Error] Failed to create root node from JSON.\n");
    return false;
}

// 動作テストの実行メソッド
void BehaviorTree::ExecuteTests() {
    OutputDebugStringA("[BT Test] Starting BehaviorTree JSON Loader Test...\n");

    // 1. テスト用モックノードを動的ファクトリーに登録
    BehaviorNodeFactory::GetInstance().RegisterNode<TestPrintNode>("TestPrintNode");
    BehaviorNodeFactory::GetInstance().RegisterNode<TestWaitNode>("TestWaitNode");

    // 2. テスト用JSONアセットファイルをローカルディレクトリへ一時出力する
    std::string testJsonPath = "test_tree.json";
    std::ofstream outFile(testJsonPath);
    if (outFile.is_open()) {
        outFile << R"JSON({
  "Type": "SequenceNode",
  "Children": [
    {
      "Type": "TestPrintNode",
      "Message": "JSON Loaded Sequence Step 1"
    },
    {
      "Type": "TestWaitNode",
      "Frames": 3
    },
    {
      "Type": "TestPrintNode",
      "Message": "JSON Loaded Sequence Step 2 (Completed after 3 frames)"
    }
  ]
})JSON";
        outFile.close();
    }

    // 3. JSONからのツリー構築とシミュレーションの実行
    auto tree = std::make_unique<BehaviorTree>();
    if (tree->LoadFromJSON(testJsonPath)) {
        // 模擬的に6フレームのゲームアップデートを進める
        for (int i = 0; i < 6; ++i) {
            std::string frameLog = "[BT Test] Frame " + std::to_string(i + 1) + " Tick\n";
            OutputDebugStringA(frameLog.c_str());
            tree->Update();
        }
    }

    // 4. 新しいカバー検知と移動のシミュレーションテスト
    OutputDebugStringA("[BT Test] Starting CoverNode Simulation Test...\n");
    std::string coverJsonPath = "test_cover_tree.json";
    std::ofstream coverFile(coverJsonPath);
    if (coverFile.is_open()) {
        coverFile << R"JSON({
  "Type": "SequenceNode",
  "Children": [
    {
      "Type": "DetectCoverNode",
      "RayCount": 8,
      "MaxDistance": 15.0,
      "MinCoverDistance": 1.5
    },
    {
      "Type": "MoveToCoverNode",
      "Speed": 5.0
    }
  ]
})JSON";
        coverFile.close();
    }

    auto coverTree = std::make_unique<BehaviorTree>();
    if (coverTree->LoadFromJSON(coverJsonPath)) {
        auto bb = coverTree->GetBlackboard();
        bb->Set<Vector3>("AgentPosition", { 0.0f, 0.0f, 0.0f });
        bb->Set<Vector3>("ThreatPosition", { 10.0f, 0.0f, 0.0f });

        OutputDebugStringA("[BT Test] Running CoverTree Tick 1 (DetectCover without obstacles)...\n");
        // この時点ではシーンに障害物がないのでDetectCoverNodeは失敗するはず
        if (coverTree->GetRoot()) {
            coverTree->GetRoot()->Reset();
        }
        BehaviorStatus status = coverTree->Update();
        std::string statusStr = "[BT Test] Tick 1 status (Expected Failure = 3): " + std::to_string(static_cast<int>(status)) + "\n";
        OutputDebugStringA(statusStr.c_str());

        // 模擬的にレイが壁を検知した状況を作り出すため、手動で黒板にCoverPositionをセットしてMoveToCoverの実行を行う
        bb->Set<Vector3>("CoverPosition", { 0.0f, 0.0f, 5.0f });
        OutputDebugStringA("[BT Test] Manually set CoverPosition to {0, 0, 5} for testing MoveToCoverNode...\n");
        
        auto moveToCoverTask = BehaviorNodeFactory::GetInstance().Create("MoveToCoverNode", nlohmann::json::object());
        if (moveToCoverTask) {
            moveToCoverTask->Deserialize(nlohmann::json::parse(R"({"Speed": 5.0})"));
            
            for (int frame = 1; frame <= 10; ++frame) {
                BehaviorStatus moveStatus = moveToCoverTask->Tick(bb);
                Vector3 pos = bb->Get<Vector3>("AgentPosition");
                std::string logLine = "[BT Test] Move Frame " + std::to_string(frame) + 
                                      " -> AgentPosition: {" + std::to_string(pos.x) + 
                                      ", " + std::to_string(pos.y) + 
                                      ", " + std::to_string(pos.z) + 
                                      "} Status: " + std::to_string(static_cast<int>(moveStatus)) + "\n";
                OutputDebugStringA(logLine.c_str());
                if (moveStatus == BehaviorStatus::Success) {
                    OutputDebugStringA("[BT Test] MoveToCover completed successfully!\n");
                    break;
                }
            }
        }
    }

    OutputDebugStringA("[BT Test] BehaviorTree JSON Loader Test Finished.\n");
}

} // namespace BaziruEngine::AI
