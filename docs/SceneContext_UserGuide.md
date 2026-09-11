# シーン間データ受け渡し（Scene Context）利用説明書

本エンジンでは、ゲームプレイシーンからクリアシーンやゲームオーバーシーンなどへ、**スコアやプレイ時間、撃破数、任意の構造体**を簡単かつ型安全に受け渡す機能（**Scene Context**）がエンジン層に備わっています。

---

## 1. 基本的な使い方

`BaseScene` を継承している各シーンクラス（`GamePlayScene` や `ClearScene` など）の中では、特別なマネージャ呼び出しを書かずに **直接メソッドを呼ぶだけ** でデータの保存と取得ができます。

### データの保存（送信側シーン: `GamePlayScene` 等）

```cpp
// 基本型の保存
SetSceneDataInt("Score", 15400);            // スコア (int)
SetSceneDataFloat("ClearTime", 42.5f);       // クリア時間 (float)
SetSceneDataBool("IsNoDamage", true);        // ノーダメージ達成フラグ (bool)
SetSceneDataString("Rank", "S");             // ランク評価 (std::string)

// または SetSceneData による型推論（どれでも自動判定されます）
SetSceneData("Score", 15400);
SetSceneData("ClearTime", 42.5f);
SetSceneData("PlayerName", "Player1");

// データをセットした後にシーン遷移
ChangeScene("CLEAR");
```

---

### データの取得（受信側シーン: `ClearScene` 等）

遷移先のシーンの `InitializeScene()` や `Update()` などで、キー名を指定して取得します。
第2引数には、もしデータが存在しなかった場合の**デフォルト値**を指定できます。

```cpp
void ClearScene::InitializeScene()
{
    // 第2引数はデフォルト値
    int score        = GetSceneDataInt("Score", 0);
    float clearTime  = GetSceneDataFloat("ClearTime", 0.0f);
    bool isNoDamage  = GetSceneDataBool("IsNoDamage", false);
    std::string rank = GetSceneDataString("Rank", "C");

    // 取得した値をメンバ変数に保持してUI描画等で使用
    score_ = score;
    clearTime_ = clearTime;
}
```

---

## 2. 構造体・カスタムクラスのまるごと受け渡し

スコアやリザルト情報を1つの構造体にまとめて、まるごと渡すことも可能です（C++20 `std::any` 対応）。

### ① 構造体の定義（共通ヘッダーまたはGameSceneヘッダー等）
```cpp
struct ClearResultData
{
    int score = 0;
    float clearTime = 0.0f;
    int defeatedCount = 0;
    int remainingSlimes = 0;
};
```

### ② 送信側（`GamePlayScene` 等）
```cpp
ClearResultData result;
result.score = currentScore_;
result.clearTime = playTimer_;
result.defeatedCount = defeatedEnemyCount_;
result.remainingSlimes = minionManager_->GetActiveCount();

// 構造体をそのままセット
SetSceneData<ClearResultData>("ResultData", result);

ChangeScene("CLEAR");
```

### ③ 受信側（`ClearScene` 等）
```cpp
void ClearScene::InitializeScene()
{
    // 構造体をそのまま復元取得
    ClearResultData result = GetSceneData<ClearResultData>("ResultData");

    score_ = result.score;
    time_  = result.clearTime;
}
```

---

## 3. その他の便利API一覧

| メソッド名 | 説明 |
| :--- | :--- |
| `HasSceneData(key)` | 指定したキーのデータが存在するか確認（`bool` を返します） |
| `RemoveSceneData(key)` | 指定したキーのデータを削除 |
| `ClearAllSceneData()` | 保存されているすべての共有データを初期化 |
| `SceneManager::GetInstance()->SetSceneData(...)` | シーンクラスの外側（マネージャ等）からデータを読み書きしたい場合に使用 |

---

## 4. `ClearScene` での実装おすすめ手順

現在の `ClearScene` にスコアを表示する場合、以下の2ステップで簡単に組み込めます。

### ステップ1: `ClearScene.h` の修正
```cpp
class ClearScene : public BaseScene
{
    // ...
private:
    KeyInput* input_ = nullptr;

    // ★ 表示用のメンバ変数を追加
    int score_ = 0;
    float clearTime_ = 0.0f;
};
```

> [!TIP]
> **ワンポイント注意**:
> `ClearScene.h` の private に `DirectXCom* dxCommon_ = nullptr;` が宣言されている場合、基底クラス `BaseScene::dxCommon_` が隠蔽されて nullptr のままになることがあります。
> キーボード入力やスカイボックス描画では `GetDirectXCom()` を使用するか、`ClearScene.h` 側の `dxCommon_` を削除して `GetDirectXCom()` をご利用ください。

### ステップ2: `ClearScene.cpp` での表示
```cpp
void ClearScene::InitializeScene()
{
    DirectXCom* dx = GetDirectXCom();
    if (dx) {
        input_ = new KeyInput();
        input_->Initialize(dx->GetWindowAPI());
    }

    // ★ ゲームシーンからスコアを取得！
    score_ = GetSceneDataInt("Score", 0);
    clearTime_ = GetSceneDataFloat("ClearTime", 0.0f);
}

void ClearScene::Update()
{
    // ...
#ifdef USE_IMGUI
    ImGui::Begin("CLEAR_HUD", ...);
    ImGui::Text("Score : %d", score_);
    ImGui::Text("Time  : %.2f sec", clearTime_);
    ImGui::End();
#endif
}
```
