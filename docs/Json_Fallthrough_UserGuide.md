# JSON即死防止・フォールスルー（Fallback）機能 利用説明書

本エンジンには、JSON設定ファイル（ステージ配置・AI挙動・パラメータ等）の構文ミス、存在しないファイル、型不一致、書き込み途中の破損によるゲームの強制終了（即死クラッシュ）を完全に防ぐ **`JsonSafeLoader`** および **フォールスルー機構** がエンジン層（`project/Baziru3_Engine/Framework/IO/`）に実装されています。

---

## 1. なぜJSONで即死クラッシュが起きていたのか？

従来のJSON読み込み処理では、以下のような状況でC++例外が捕捉されず `std::terminate()` による強制終了（即死）が発生していました。

1. **構文エラー（カンマ忘れ・閉じカッコ不足など）**:
   - `file >> json;` 時に `nlohmann::json::parse_error` が発生しクラッシュ。
2. **キーの型不一致や欠損**:
   - `json["speed"]` を float に代入しようとして文字列や null だった場合、`nlohmann::json::type_error` が発生してクラッシュ。
3. **ファイル保存中の強制終了・電源断・ディスクフル**:
   - `std::ofstream` で上書き中にアプリが落ちるとファイルサイズが0バイトになり、次回起動時に即死。
4. **外部ライブラリ（imgui-node-editor等）のアサート**:
   - 内部パーサー（`crude_json`）が破損したJSONを読み込み `CRUDE_ASSERT` で停止。

これらを解決するため、エンジン層で **すべての例外をトラップして安全な初期値（フォールバック）を代替適用するフォールスルー機能** を導入しました。

---

## 2. エンジン機能: `JsonSafeLoader` の使い方

ヘッダーをインクルードするだけで利用できます。
```cpp
#include "Framework/IO/JsonSafeLoader.h"
using namespace BaziruEngine::IO;
```

### ① 安全なJSONロード（`LoadSafe`）
ファイルが存在しない・破損している・0バイトの場合でも、クラッシュせず警告ログ（`OutputDebugStringA`）を出力し、指定した**フォールバックJSON**を展開して処理を続行（フォールスルー）します。

```cpp
nlohmann::json data;

// フォールバック用の初期構造を用意（空の配列や空のオブジェクト）
nlohmann::json fallback = nlohmann::json::array();

bool isOk = JsonSafeLoader::LoadSafe("Resources/stage_layout.json", data, fallback);
if (!isOk) {
    // 警告ログはエンジン内で自動出力されます。
    // dataには fallback が代入されているため、以降の処理が安全に続行できます。
}
```

---

### ② 型安全な値の取得（`SafeGet` / `TryGet`）
キーが存在しない場合や、型が違っていても例外を投げず、安全に**デフォルト値**を返します。

```cpp
// 第3引数に「キーが存在しない・型不一致時のデフォルト値」を指定
float speed     = JsonSafeLoader::SafeGet<float>(item, "Speed", 3.0f);
int rayCount    = JsonSafeLoader::SafeGet<int>(item, "RayCount", 8);
std::string tag = JsonSafeLoader::SafeGet<std::string>(item, "Tag", "Enemy");
bool isStatic   = JsonSafeLoader::SafeGet<bool>(item, "IsStatic", false);

// 存在確認と取得を同時に行う場合
float customVal = 0.0f;
if (JsonSafeLoader::TryGet<float>(item, "CustomVal", customVal)) {
    // 正常に値が存在した場合のみ実行
}
```

---

### ③ アトミック安全保存（`SaveSafe`）
保存中にクラッシュや強制終了が発生しても既存のJSONが破損（0バイト化）しないよう、一時ファイル（`.tmp`）へ完全に書き出してからアトミック置換を行います。

```cpp
nlohmann::json exportJson = nlohmann::json::array();
// ... データの構築 ...

// インデント指定（デフォルトは4、-1で最小化）
bool saveSuccess = JsonSafeLoader::SaveSafe("Resources/stage_layout.json", exportJson, 4);
```

---

### ④ 外部ライブラリ用事前サニタイズ（`SanitizeFile`）
外部GUIツールやサードパーティライブラリ（`crude_json` 等）に渡す前に、JSON構文が壊れていないか検査し、壊れている場合は安全な初期状態に修復します。

```cpp
// bt_editor_layout.json が破損していたら安全な空オブジェクト "{}" に修復
JsonSafeLoader::SanitizeFile("bt_editor_layout.json", nlohmann::json::object());
```

---

## 3. アプリケーション層への適用例（LevelEditor等）

もし `Application/LevelEditor.cpp` の JSON ロード・セーブを堅牢化したい場合は、以下のように書き換えることができます。

### 【ロード側】の書き換え例 (`LevelEditor::LoadFromJson`)

```cpp
#include "../Baziru3_Engine/Framework/IO/JsonSafeLoader.h"
using namespace BaziruEngine::IO;

bool LevelEditor::LoadFromJson(const std::string& filepath)
{
    nlohmann::json j;
    // 破損時や空ファイル時は空配列 [] をフォールバックとして展開
    if (!JsonSafeLoader::LoadSafe(filepath, j, nlohmann::json::array()))
    {
        // 破損・不在時でもクラッシュせず初期状態（空）で安全にエディタを開く
        objectDatas_.clear();
        return false;
    }

    // 万が一トップレベルが配列でない場合もガード
    if (!j.is_array())
    {
        return false;
    }

    std::vector<LevelObjectData> tempDatas;
    for (const auto& item : j)
    {
        if (!item.is_object()) continue;

        LevelObjectData obj;
        // SafeGet でキー欠損・型違いでも即死しない
        obj.name = JsonSafeLoader::SafeGet<std::string>(item, "name", "Unnamed");
        obj.type = JsonSafeLoader::SafeGet<std::string>(item, "type", "");
        obj.isStatic = JsonSafeLoader::SafeGet<bool>(item, "isStatic", false);

        if (item.contains("position") && item["position"].is_object())
        {
            auto& pos = item["position"];
            obj.position.x = JsonSafeLoader::SafeGet<float>(pos, "x", 0.0f);
            obj.position.y = JsonSafeLoader::SafeGet<float>(pos, "y", 0.0f);
            obj.position.z = JsonSafeLoader::SafeGet<float>(pos, "z", 0.0f);
        }

        tempDatas.push_back(obj);
    }

    objectDatas_ = std::move(tempDatas);
    RefreshRuntimeObjects();
    return true;
}
```

### 【セーブ側】の書き換え例 (`LevelEditor::SaveToJson`)

```cpp
bool LevelEditor::SaveToJson(const std::string& filepath)
{
    nlohmann::json j = nlohmann::json::array();
    // ... item の追加処理 ...

    // SaveSafe を使用してアトミックに書き込み（0バイト破損を防止）
    return JsonSafeLoader::SaveSafe(filepath, j, 4);
}
```

---

## 4. ビヘイビアツリー（AI）の自動フォールスルー

エンジン側の `BehaviorTree::LoadFromJSON` には、フォールスルー機能が標準組み込みされています。

```cpp
auto tree = std::make_unique<BehaviorTree>();

// 第2引数の enableFallthrough はデフォルトで true
// AIファイルが壊れていても、クラッシュせずに空の安全なシーケンスノードが構築されゲームが継続動作します
tree->LoadFromJSON("Resources/ai_trees/enemy_behavior.json");
```

ノード追加用ファクトリー（`BehaviorNodeFactory`）も、ノード内部のパラメータ解析（`Deserialize`）で例外が発生しても自動でキャッチしてデフォルト値でノードを生かすため、手動編集ミスによる即死は発生しません。

---

## 5. `.gitignore` での除外設定

自動生成されるエディタウィンドウ設定やテンポラリファイルは `.gitignore` に登録され、誤って壊れたキャッシュが Git に同期されるのを防止しています。

```gitignore
# Editor layout and temporary JSON caches
**/bt_editor_layout.json
**/NodeEditor.json
**/camera_sync.json
*.json.bak
*.json.tmp
```
※ ステージ配置データ（`stage_layout.json`）やツリー定義（`test_tree.json`）などのゲームアセットは Git で正常に共有・バージョン管理され、中身が壊れていても上記のフォールスルー機構によって安全に動作します。
