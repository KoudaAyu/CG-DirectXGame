# 🚀 GE3_Game エンジン ゲーム開発スタートガイド

本プロジェクト（`GE3_Game`）は、DirectX 12 ベースの自作ゲームエンジン **`Baziru3_Engine`** と、その上で動作する **クリーンなゲーム開発スターターテンプレート** です。

本リポジトリから Git ブランチを切ることで、誰でも即座に新作ゲームの開発を開始できます。

---

## 📂 プロジェクト構成

```
project/
├── Baziru3_Engine/           <-- 【完全なゲームエンジン層（変更不要）】
│   ├── Core/                 (DirectXCom, WinApp, EngineContext, Input, Camera, Audio...)
│   ├── Graphics/             (Object3d, Sprite, Model, Pipeline, Texture, Light, Skybox...)
│   └── Framework/            (SceneManager, Collision, Particle, Animation...)
│
├── Application/              <-- 【ゲーム開発領域（ここを編集・拡張してゲームを作ります）】
│   ├── Scene/
│   │   ├── TitleScene.h/.cpp       (タイトル画面)
│   │   ├── GamePlayScene.h/.cpp    (メインゲーム画面スターター)
│   │   ├── ClearScene.h/.cpp       (クリア画面)
│   │   └── GameOverScene.h/.cpp    (ゲームオーバー画面)
│   └── SceneRegistration.cpp       (シーン登録テーブル)
│
├── Game.cpp / Game.h         <-- 【エンジン駆動エントリポイント】
└── main.cpp
```

---

## 🛠️ 新規ゲーム開発の流れ

### 1. 新しいブランチを作成する
```bash
git checkout -b feature/my-new-game
```

---

### 2. メインゲームロジックを実装する (`GamePlayScene.cpp`)

#### ① 3D オブジェクトを出す
```cpp
// メンバ変数として宣言
std::unique_ptr<Object3d> playerObj_;

// InitializeScene() 内で初期化 (1行でOK!)
playerObj_ = std::make_unique<Object3d>();
playerObj_->Initialize("Resources", "suzanne.obj");
playerObj_->SetCamera(camera_.get());
playerObj_->SetTranslate({ 0.0f, 1.0f, 0.0f });

// Update() 内で行列更新
playerObj_->SetTranslate(newPos);
playerObj_->Update();

// Draw() 内で描画
playerObj_->Draw(ctx);
```

#### ② 2D スプライト / UI を出す
```cpp
// メンバ変数として宣言
std::unique_ptr<Sprite> hpBar_;

// InitializeScene() 内で生成
hpBar_ = Sprite::Create("Resources/uvChecker.png", { 50.0f, 50.0f });
hpBar_->SetSize({ 200.0f, 20.0f });

// Update() 内で更新
hpBar_->Update();

// Draw() 内で描画
hpBar_->Draw();
```

#### ③ キー入力・マウス入力を受け取る
```cpp
// キーボード入力
if (keyInput_->PushKey(DIK_W)) {
    // 前進
}
if (keyInput_->TriggerKey(DIK_SPACE)) {
    // ジャンプ / アクション（押した瞬間のみ）
}

// マウス入力
if (mouseInput_->IsTrigger(MouseInput::MouseButton::Left)) {
    // 攻撃 / 決定
}
```

#### ④ シーンを切り替える
```cpp
// タイトル、クリア、ゲームオーバー等へ即座に安全遷移
SceneManager::GetInstance()->ChangeScene("CLEAR");
SceneManager::GetInstance()->ChangeScene("GAMEOVER");
SceneManager::GetInstance()->ChangeScene("TITLE");
```

---

## 🎨 ImGui によるリアルタイムパラメータ調整

`GamePlayScene::UpdateImGui()` 内にスライダーやボタンを追加するだけで、ビルドし直すことなく実行中にパラメータをチューニングできます。

```cpp
void GamePlayScene::UpdateImGui()
{
#ifdef USE_IMGUI
    ImGui::Begin("Game Tuning");
    ImGui::SliderFloat("Move Speed", &moveSpeed_, 1.0f, 20.0f);
    ImGui::DragFloat3("Player Pos", &playerPosition_.x, 0.1f);
    ImGui::End();
#endif
}
```

---

## 🏆 面接・ポートフォリオでのアピールポイント

1. **完全なレイヤー分離**: エンジン層（`Baziru3_Engine`）とアプリケーション層（`Application`）が完全に疎結合になっており、ライブラリとしての再利用性が極めて高い。
2. **GPU同期と堅牢性**: シーン破棄時の `WaitForGpu`、未ロード画像に対する自動フォールバックテクスチャ生成など、商用ゲーム開発基準の耐障害性を装備。
3. **ゼロアロケーション API**: `std::string_view` 経由のテクスチャ/モデルロードにより、フレーム中の不要なヒープ確保をゼロ化。
