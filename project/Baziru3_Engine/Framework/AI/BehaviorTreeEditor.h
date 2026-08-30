#pragma once
#include <string>
#include <vector>
#include <list>
#include <unordered_map>
#include <memory>
#include <nlohmann/json.hpp>
#include <imgui.h>
#include <imgui_node_editor.h>

namespace BaziruEngine::AI {

// =================================================================
// BehaviorTreeEditor (ビヘイビアツリー ビジュアルエディタ)
// -----------------------------------------------------------------
// ゲーム実行中にImGui上でノードベースのAIツリーを編集し、
// データ駆動用のJSONファイルをエクスポートするツールクラス。
// =================================================================

namespace ed = ax::NodeEditor;

// エディタ内で管理するピン（接続用端子）の情報
struct EditorPin {
    ed::PinId id;              // ユニークなピンID
    ed::PinKind kind;          // 入力(Input)か出力(Output)か
    struct EditorNode* node;   // 所属するノードへの逆引き参照
};

// エディタ内で管理するノード表示オブジェクト
struct EditorNode {
    ed::NodeId id;                    // ユニークなノードID
    std::string name;                 // ノード名（型名など）
    std::vector<EditorPin> inputs;    // 入力ピンのリスト
    std::vector<EditorPin> outputs;   // 出力ピンのリスト
    ImVec2 position;                  // 画面上のグリッド座標
    
    // ノード固有のパラメータ設定（例: MessageやWaitFrameなど）を保持するJSON
    nlohmann::json customProperties; 
};

// ノード同士の接続線の情報
struct EditorLink {
    ed::LinkId id;            // ユニークなリンクID
    ed::PinId startPinId;     // 開始ピン（親の出力ピン）
    ed::PinId endPinId;       // 終了ピン（子の入力ピン）
};

class BehaviorTreeEditor {
public:
    BehaviorTreeEditor();
    ~BehaviorTreeEditor();

    // エディタ画面の更新および描画処理（DebugUIなどのImGuiループ内から呼び出す）
    void Draw();

    // 指定されたJSONファイルからツリーの接続関係をロードしてエディタ上に復元します
    bool LoadTree(const std::string& filePath);

    // エディタ上のノードの接続状態を解析し、ツリー構造をJSONファイルへ保存します
    bool SaveTree(const std::string& filePath);

private:
    // シグナルやピン、ノード用の新規IDを生成するヘルパー
    uintptr_t GetNextId();

    // エディタの初期化状態をクリアする
    void ClearEditorState();

    // IDからエディタ用ノードを逆引きするヘルパー
    EditorNode* FindNode(ed::NodeId id);
    // IDからエディタ用ピンを逆引きするヘルパー
    EditorPin* FindPin(ed::PinId id);

    // ビジュアルエディタのノード配置関係から木構造を再帰的に解析し、JSONに変換します
    nlohmann::json BuildTreeJSON(EditorNode* currentNode);

    // ノード作成のポップアップメニューを処理する
    void HandleCreationPopup();

    // 新しいノードをエディタに配置する（型名を指定してピンを自動生成）
    EditorNode* CreateEditorNode(const std::string& typeName, ImVec2 pos);

    // JSON構造から再帰的にエディタ上のノードを構築する
    EditorNode* LoadNodeRecursive(const nlohmann::json& nodeJson, ImVec2 pos, EditorPin* parentOutputPin);

private:
    ed::EditorContext* editorContext_ = nullptr; // imgui-node-editorの実行コンテキスト
    
    std::list<EditorNode> nodes_; // 画面上に存在する全ノードのリスト
    std::vector<EditorLink> links_; // 画面上に存在する全リンク（接続線）のリスト

    uintptr_t nextId_ = 1; // ID自動生成用のカウンタ
    std::string currentLoadedPath_ = "test_tree.json"; // 現在編集・ロード中のファイルパス

    bool showEditor_ = true; // エディタ画面自体の表示ON/OFF
};

} // namespace BaziruEngine::AI
