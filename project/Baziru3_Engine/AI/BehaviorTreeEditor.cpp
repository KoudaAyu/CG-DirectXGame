#include "BehaviorTreeEditor.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <Windows.h>

namespace BaziruEngine::AI {

// ==========================================
// コンストラクタ / デストラクタ
// ==========================================

BehaviorTreeEditor::BehaviorTreeEditor() {
    // imgui-node-editorのコンテキスト生成
    ed::Config config;
    config.SettingsFile = "bt_editor_layout.json"; // ノード位置自動保存用ファイル
    editorContext_ = ed::CreateEditor(&config);

    // テスト用に起動時にデフォルトのツリーファイルを自動ロード
    LoadTree(currentLoadedPath_);
}

BehaviorTreeEditor::~BehaviorTreeEditor() {
    if (editorContext_) {
        ed::DestroyEditor(editorContext_);
        editorContext_ = nullptr;
    }
}

// ==========================================
// IDおよびノード管理ヘルパー
// ==========================================

uintptr_t BehaviorTreeEditor::GetNextId() {
    return nextId_++;
}

void BehaviorTreeEditor::ClearEditorState() {
    nodes_.clear();
    links_.clear();
    nextId_ = 1;
}

EditorNode* BehaviorTreeEditor::FindNode(ed::NodeId id) {
    for (auto& node : nodes_) {
        if (node.id == id) return &node;
    }
    return nullptr;
}

EditorPin* BehaviorTreeEditor::FindPin(ed::PinId id) {
    for (auto& node : nodes_) {
        for (auto& pin : node.inputs) {
            if (pin.id == id) return &pin;
        }
        for (auto& pin : node.outputs) {
            if (pin.id == id) return &pin;
        }
    }
    return nullptr;
}

// 接続状態からツリーの木構造を再帰的に巡回し、正規のBehaviorTree用JSONアセットに復元します
nlohmann::json BehaviorTreeEditor::BuildTreeJSON(EditorNode* currentNode) {
    if (!currentNode) return nullptr;

    nlohmann::json result;
    result["Type"] = currentNode->name;

    // ノード固有のパラメータ（カスタムプロパティ）をマージする
    for (auto& el : currentNode->customProperties.items()) {
        result[el.key()] = el.value();
    }

    // 出力ピンがノードにあり、子へのリンクがあるか確認
    if (!currentNode->outputs.empty()) {
        ed::PinId outputPinId = currentNode->outputs[0].id;
        std::vector<nlohmann::json> childrenJsonList;

        // この出力ピンに繋がっている全リンクを探索
        for (const auto& link : links_) {
            if (link.startPinId == outputPinId) {
                // リンク先（入力ピン）を特定
                EditorPin* inputPin = FindPin(link.endPinId);
                if (inputPin && inputPin->node) {
                    // 再帰的に子供のJSONを構築してリストに登録
                    nlohmann::json childJson = BuildTreeJSON(inputPin->node);
                    if (!childJson.is_null()) {
                        childrenJsonList.push_back(childJson);
                    }
                }
            }
        }

        // CompositeNode系であれば、子供リストをChildren配列としてJSONに格納
        if (!childrenJsonList.empty()) {
            result["Children"] = childrenJsonList;
        }
    }

    return result;
}

// 新規ノードの追加処理（型名に応じた入出力ピンの設定）
EditorNode* BehaviorTreeEditor::CreateEditorNode(const std::string& typeName, ImVec2 pos) {
    nodes_.emplace_back();
    EditorNode& node = nodes_.back();
    node.id = ed::NodeId(GetNextId());
    node.name = typeName;
    node.position = pos;

    // 基本設定: 全てのノードは親から繋がれるための「入力ピン」を1つ持つ（ルートノードも可視化用に持つ）
    node.inputs.push_back({ ed::PinId(GetNextId()), ed::PinKind::Input, &node });

    // 制御ノード (Sequence, Selector) の場合は、複数の子を繋ぐための「出力ピン」を1つ持つ
    if (typeName == "SequenceNode" || typeName == "SelectorNode") {
        node.outputs.push_back({ ed::PinId(GetNextId()), ed::PinKind::Output, &node });
    }

    // 初期化パラメータの設定 (テスト用のデフォルト値)
    if (typeName == "TestPrintNode") {
        node.customProperties["Message"] = "New Message";
    } else if (typeName == "TestWaitNode") {
        node.customProperties["Frames"] = 30;
    } else if (typeName == "DetectCoverNode") {
        node.customProperties["RayCount"] = 8;
        node.customProperties["MaxDistance"] = 15.0f;
        node.customProperties["MinCoverDistance"] = 1.5f;
    } else if (typeName == "MoveToCoverNode") {
        node.customProperties["Speed"] = 3.0f;
    }

    // ノードの配置座標をimgui-node-editorに反映
    ed::SetNodePosition(node.id, pos);

    return &node;
}

// ==========================================
// ファイルのセーブ ＆ ロード機能
// ==========================================

bool BehaviorTreeEditor::SaveTree(const std::string& filePath) {
    if (nodes_.empty()) return false;

    // 1. ルートノードの特定：親（入力ピン）に一切リンクが繋がっていない最上位ノードを探す
    EditorNode* rootNode = nullptr;
    for (auto& node : nodes_) {
        bool hasParentLink = false;
        if (!node.inputs.empty()) {
            ed::PinId inputPinId = node.inputs[0].id;
            for (const auto& link : links_) {
                if (link.endPinId == inputPinId) {
                    hasParentLink = true;
                    break;
                }
            }
        }
        if (!hasParentLink) {
            rootNode = &node;
            break; // 最初のルート候補を採用
        }
    }

    if (!rootNode) {
        OutputDebugStringA("[BT Editor] Error: Cannot find root node (unlinked input pin).\n");
        return false;
    }

    // 2. 木構造の解析とシリアライズ
    nlohmann::json rootJson = BuildTreeJSON(rootNode);

    // 3. JSONファイルへ書き出し
    std::ofstream file(filePath);
    if (!file.is_open()) return false;

    file << rootJson.dump(2); // インデント2文字で出力
    file.close();

    currentLoadedPath_ = filePath;
    OutputDebugStringA(("[BT Editor] Successfully saved tree to: " + filePath + "\n").c_str());
    return true;
}

// ロード時の再帰用ヘルパ（JSON構造からエディタノードとリンクを復元）
EditorNode* BehaviorTreeEditor::LoadNodeRecursive(const nlohmann::json& nodeJson, ImVec2 pos, EditorPin* parentOutputPin) {
    if (!nodeJson.contains("Type") || !nodeJson["Type"].is_string()) return nullptr;

    std::string typeName = nodeJson["Type"];
    
    // エディタノードを生成
    EditorNode* editorNode = CreateEditorNode(typeName, pos);
    if (!editorNode) return nullptr;

    // ノード固有プロパティの復元
    for (auto& el : nodeJson.items()) {
        if (el.key() != "Type" && el.key() != "Children") {
            editorNode->customProperties[el.key()] = el.value();
        }
    }

    // 親の出力ピンがある場合、リンク線で接続する
    if (parentOutputPin && !editorNode->inputs.empty()) {
        links_.push_back({
            ed::LinkId(GetNextId()),
            parentOutputPin->id,
            editorNode->inputs[0].id
        });
    }

    // 子供ノードがある場合、横並びで少し下に再帰展開して配置
    if (nodeJson.contains("Children") && nodeJson["Children"].is_array() && !editorNode->outputs.empty()) {
        float childXOffset = -150.0f * (nodeJson["Children"].size() - 1) / 2.0f;
        for (const auto& childJson : nodeJson["Children"]) {
            ImVec2 childPos(pos.x + childXOffset, pos.y + 150.0f);
            LoadNodeRecursive(childJson, childPos, &editorNode->outputs[0]);
            childXOffset += 150.0f; // 次の子ノードの位置を横にずらす
        }
    }

    return editorNode;
}

bool BehaviorTreeEditor::LoadTree(const std::string& filePath) {
    // コンテキストを設定してクラッシュを防止
    ed::SetCurrentEditor(editorContext_);

    std::ifstream file(filePath);
    if (!file.is_open()) {
        // ファイルがない場合は初期のダミールートノードを生成しておく
        ClearEditorState();
        CreateEditorNode("SequenceNode", ImVec2(400, 100));
        currentLoadedPath_ = filePath;
        ed::SetCurrentEditor(nullptr);
        return false;
    }

    nlohmann::json rootJson;
    try {
        file >> rootJson;
    } catch (...) {
        file.close();
        ed::SetCurrentEditor(nullptr);
        return false;
    }
    file.close();

    // エディタ状態のクリーンアップ
    ClearEditorState();

    // 再帰的にロードを実行して画面上に構築
    LoadNodeRecursive(rootJson, ImVec2(400, 100), nullptr);

    currentLoadedPath_ = filePath;
    OutputDebugStringA(("[BT Editor] Successfully loaded tree from: " + filePath + "\n").c_str());

    ed::SetCurrentEditor(nullptr);
    return true;
}

// ==========================================
// UI更新・描画処理
// ==========================================

void BehaviorTreeEditor::Draw() {
    if (!showEditor_) return;

    ImGui::SetNextWindowSize(ImVec2(1000, 600), ImGuiCond_FirstUseEver);
    ImGui::Begin("AI Behavior Tree Editor", &showEditor_, ImGuiWindowFlags_MenuBar);

    // メニューバーでの保存・読込処理
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Load test_tree.json")) {
                LoadTree("test_tree.json");
            }
            if (ImGui::MenuItem("Save test_tree.json")) {
                SaveTree("test_tree.json");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Load test_cover_tree.json")) {
                LoadTree("test_cover_tree.json");
            }
            if (ImGui::MenuItem("Save test_cover_tree.json")) {
                SaveTree("test_cover_tree.json");
            }
            ImGui::EndMenu();
        }
        ImGui::TextDisabled(" | Active File: %s", currentLoadedPath_.c_str());
        ImGui::EndMenuBar();
    }

    // 画面分割: 左半分（ノードキャンバス）、右半分（選択されたノードのインスペクター）
    ImGui::Columns(2, "EditorSplit", true);
    ImGui::SetColumnWidth(0, 750); // キャンバスを広めに取る

    // 🎨 [左側] ノードエディタキャンバスの描画
    ed::SetCurrentEditor(editorContext_);
    ed::Begin("MyNodeEditor", ImVec2(0.0f, 0.0f));

    // A. ノードの描画
    for (auto& node : nodes_) {
        ed::BeginNode(node.id);
        
        // ノードのヘッダータイトル表示
        ImGui::Text("%s", node.name.c_str());
        ImGui::Text("ID: %d", node.id.AsPointer());
        
        ImGui::Spacing();

        // 入力ピンの描画
        for (auto& pin : node.inputs) {
            ed::BeginPin(pin.id, ed::PinKind::Input);
            ImGui::Text("-> [In]");
            ed::EndPin();
        }

        // 出力ピンの描画
        for (auto& pin : node.outputs) {
            ImGui::Indent(80); // 右寄せっぽくインデント
            ed::BeginPin(pin.id, ed::PinKind::Output);
            ImGui::Text("[Out] ->");
            ed::EndPin();
            ImGui::Unindent(80);
        }

        ed::EndNode();
    }

    // B. リンク線（接続関係）の描画
    for (auto& link : links_) {
        ed::Link(link.id, link.startPinId, link.endPinId);
    }

    // C. ユーザーインタラクションの処理（接続の作成）
    if (ed::BeginCreate()) {
        ed::PinId inputPinId, outputPinId;
        // 新しいドラッグ接続が確定したか判定
        if (ed::QueryNewLink(&outputPinId, &inputPinId)) {
            // ピンタイプ（入力と出力）が正しく対になっているか、かつ自分自身への接続でないか確認
            EditorPin* outPin = FindPin(outputPinId);
            EditorPin* inPin = FindPin(inputPinId);
            
            if (outPin && inPin && outPin->kind == ed::PinKind::Output && inPin->kind == ed::PinKind::Input && outPin->node != inPin->node) {
                if (ed::AcceptNewItem()) {
                    // 同一の入力ピンに複数の親から接続させない（BTの親は常に1つのみ）
                    // 既存の入力ピンへのリンクがあれば削除する
                    links_.erase(std::remove_if(links_.begin(), links_.end(), [inputPinId](const EditorLink& l) {
                        return l.endPinId == inputPinId;
                    }), links_.end());

                    // 新しい接続情報を追加
                    links_.push_back({
                        ed::LinkId(GetNextId()),
                        outputPinId,
                        inputPinId
                    });
                }
            }
        }
    }
    ed::EndCreate();

    // D. 接続線の削除インタラクション
    if (ed::BeginDelete()) {
        ed::LinkId linkId;
        while (ed::QueryDeletedLink(&linkId)) {
            if (ed::AcceptDeletedItem()) {
                // 接続をリストから削除
                links_.erase(std::remove_if(links_.begin(), links_.end(), [linkId](const EditorLink& l) {
                    return l.id == linkId;
                }), links_.end());
            }
        }

        ed::NodeId nodeId;
        while (ed::QueryDeletedNode(&nodeId)) {
            if (ed::AcceptDeletedItem()) {
                // ノード本体を削除
                nodes_.remove_if([nodeId](const EditorNode& n) {
                    return n.id == nodeId;
                });

                // 削除されたノードに繋がっていたリンク線も同時に消去
                links_.erase(std::remove_if(links_.begin(), links_.end(), [nodeId, this](const EditorLink& l) {
                    EditorPin* outPin = FindPin(l.startPinId);
                    EditorPin* inPin = FindPin(l.endPinId);
                    return (outPin && outPin->node->id == nodeId) || (inPin && inPin->node->id == nodeId);
                }), links_.end());
            }
        }
    }
    ed::EndDelete();

    // E. 右クリックによるノード作成ポップアップの起動
    ed::NodeId contextNodeId = 0;
    ed::PinId contextPinId = 0;
    ed::LinkId contextLinkId = 0;
    if (ed::ShowBackgroundContextMenu()) {
        ImGui::OpenPopup("CreateNewNodePopup");
    }

    // ポップアップメニューの中身の描画
    ed::Suspend();
    if (ImGui::BeginPopup("CreateNewNodePopup")) {
        ImVec2 clickPos = ImGui::GetMousePosOnOpeningCurrentPopup();
        
        ImGui::Text("--- Create AI Node ---");
        if (ImGui::MenuItem("Add SequenceNode")) {
            CreateEditorNode("SequenceNode", clickPos);
        }
        if (ImGui::MenuItem("Add SelectorNode")) {
            CreateEditorNode("SelectorNode", clickPos);
        }
        if (ImGui::MenuItem("Add TestPrintNode")) {
            CreateEditorNode("TestPrintNode", clickPos);
        }
        if (ImGui::MenuItem("Add TestWaitNode")) {
            CreateEditorNode("TestWaitNode", clickPos);
        }
        if (ImGui::MenuItem("Add DetectCoverNode")) {
            CreateEditorNode("DetectCoverNode", clickPos);
        }
        if (ImGui::MenuItem("Add MoveToCoverNode")) {
            CreateEditorNode("MoveToCoverNode", clickPos);
        }
        ImGui::EndPopup();
    }
    ed::Resume();

    ed::End();

    // 🔎 [右側] インスペクター（選択されたノードの詳細編集パネル）
    ImGui::NextColumn();
    ImGui::Text("=== Node Inspector ===");
    ImGui::Separator();

    // 現在ノードエディタ上で選択されているノードがあるか確認
    std::vector<ed::NodeId> selectedNodes;
    selectedNodes.resize(ed::GetSelectedObjectCount());
    int selectCount = ed::GetSelectedNodes(selectedNodes.data(), static_cast<int>(selectedNodes.size()));
    
    if (selectCount > 0 && !selectedNodes.empty()) {
        // 最初に選択されたノードを取得
        EditorNode* selectedNode = FindNode(selectedNodes[0]);
        if (selectedNode) {
            ImGui::Text("Selected Type: %s", selectedNode->name.c_str());
            ImGui::Text("Node ID: %d", selectedNode->id.AsPointer());
            ImGui::Separator();
            ImGui::Spacing();

            // ノードの種類に応じた編集用フォームの表示
            if (selectedNode->name == "TestPrintNode") {
                char textBuffer[256] = "";
                std::string currentMsg = selectedNode->customProperties.value("Message", "");
                std::snprintf(textBuffer, sizeof(textBuffer), "%s", currentMsg.c_str());
                
                if (ImGui::InputText("Message", textBuffer, sizeof(textBuffer))) {
                    selectedNode->customProperties["Message"] = std::string(textBuffer);
                }
            } else if (selectedNode->name == "TestWaitNode") {
                int frames = selectedNode->customProperties.value("Frames", 30);
                if (ImGui::SliderInt("Wait Frames", &frames, 1, 300)) {
                    selectedNode->customProperties["Frames"] = frames;
                }
            } else if (selectedNode->name == "DetectCoverNode") {
                int rayCount = selectedNode->customProperties.value("RayCount", 8);
                if (ImGui::SliderInt("Ray Count", &rayCount, 4, 32)) {
                    selectedNode->customProperties["RayCount"] = rayCount;
                }

                float maxDistance = selectedNode->customProperties.value("MaxDistance", 15.0f);
                if (ImGui::SliderFloat("Max Distance", &maxDistance, 1.0f, 50.0f, "%.1f")) {
                    selectedNode->customProperties["MaxDistance"] = maxDistance;
                }

                float minCoverDistance = selectedNode->customProperties.value("MinCoverDistance", 1.5f);
                if (ImGui::SliderFloat("Min Cover Distance", &minCoverDistance, 0.5f, 5.0f, "%.1f")) {
                    selectedNode->customProperties["MinCoverDistance"] = minCoverDistance;
                }
            } else if (selectedNode->name == "MoveToCoverNode") {
                float speed = selectedNode->customProperties.value("Speed", 3.0f);
                if (ImGui::SliderFloat("Movement Speed", &speed, 0.5f, 20.0f, "%.1f")) {
                    selectedNode->customProperties["Speed"] = speed;
                }
            } else {
                ImGui::Text("No custom properties for this node.");
            }
        }
    } else {
        ImGui::TextDisabled("Select a node in the editor\nto modify its properties.");
    }

    ed::SetCurrentEditor(nullptr);

    ImGui::Columns(1); // カラムレイアウトのリセット
    ImGui::End();
}

} // namespace BaziruEngine::AI
