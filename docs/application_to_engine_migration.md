# アプリケーション層からエンジン層への移行・リファクタリング提案書

本ドキュメントでは、現在アプリケーション層（`project/Application`）で実装されている機能のうち、ゲームエンジンとしての汎用性・再利用性・カプセル化を高めるために**エンジン層（`Baziru3_Engine`）へ移行すべきコンポーネント**について整理し、その移行方法を提案します。

---

## 🛠️ 移行推奨コンポーネント一覧

| コンポーネント名 | 現在の位置 | 移行先の候補 | 移行の目的 |
| :--- | :--- | :--- | :--- |
| **`AppParticleManager`** | `Application/Particle` | `Baziru3_Engine/Particle` | 重複するパーティクル描画パイプラインの統合と、GPU/CPU双方をエンジン側で一元管理するため。 |
| **`FadeApplication`** | `Application/Fade` | `Baziru3_Engine/Scene` または `Base` | ゲーム内で広く使われるフェード処理（画面演出）を、シーン遷移システムの一部としてエンジン化するため。 |
| **`SubsystemFactory`** | `Application/Subsystem` | `Baziru3_Engine/Base` | アプリケーションがエンジンの内部初期化（DirectXやWindowAPIなど）を直接管理するのを防ぎ、隠蔽（カプセル化）するため。 |

---

## 📂 各コンポーネントの移行＆拡張設計案

### 1. `AppParticleManager` の統合とエンジンへの移行

#### 現状の課題
現在、エンジン層には GPU Particle を実行する `ParticleManager` があるのに対し、アプリケーション層には CPU でシミュレーションを行う [AppParticleManager](file:///c:/Users/k024g/OneDrive/デスクトップ/Engine_ver2026/project/Application/Particle/AppParticleManager.h) が存在しています。
このため、**描画パイプライン（PSO、ルートシグネチャ）やインスタンシング用バッファが二重に管理される**状態になっており、エンジン設計として不自然です。

#### 移行・拡張案
*   **物理挙動の拡張（CS対応）:**
    `AppParticle` が持つ「重力」「角速度」「バウンド」「プレイヤー追従」などのパラメータを、エンジン層の GPU Particle シェーダー（`UpdateParticle.CS.hlsl`）で扱えるように拡張します。これにより、すべてのパーティクルを高速な GPU 計算で処理可能にします。
*   **エミッター（発生器）のデータ化:**
    アプリケーション側で `EmitSpark` や `EmitDust` といった個別の関数を作るのではなく、パラメータを設定した「エミッター設定構造体」をエンジンに登録し、エンジン側の `ParticleManager` がそれを解ーストして発生させる設計に変更します。
*   **CPUパーティクルが必要な場合:**
    どうしても CPU シミュレーションが必要な場合であっても、[ParticleManager](file:///c:/Users/k024g/OneDrive/デスクトップ/Engine_ver2026/project/Baziru3_Engine/Particle/ParticleManager.h) の内部クラスとして取り込み、アプリ層は「発生コマンド」を送るだけに抑えます。

---

### 2. `FadeApplication` のエンジン層移行

#### 現状の課題
シーン切り替え時などの「フェードイン・フェードアウト」を行う [FadeApplication](file:///c:/Users/k024g/OneDrive/デスクトップ/Engine_ver2026/project/Application/Fade/FadeApplication.h) は、黒いスプライトを描画するだけの汎用的な仕組みです。これらはゲーム全体の共通ユーティリティであり、アプリケーション層にあるべきではありません。

#### 移行・拡張案
*   **`Baziru3_Engine/Scene` または `Base` への移行:**
    `Fade` クラスとしてエンジン側に移動します。
*   **シーン遷移システム（`SceneManager`）との連動:**
    シーンマネージャと統合し、以下のようにシーン切り替え時に自動的、あるいは簡単なAPI呼び出しでフェードが実行されるようにします。
    ```cpp
    // シーン遷移時にフェードアウト -> ロード -> フェードインを自動化する例
    SceneManager::GetInstance()->ChangeSceneWithFade("GameScene", 1.0f); // 1秒かけてフェードして遷移
    ```

---

### 3. `SubsystemFactory` のエンジン層移行（カプセル化）

#### 現状の課題
[SubsystemFactory](file:///c:/Users/k024g/OneDrive/デスクトップ/Engine_ver2026/project/Application/Subsystem/SubsystemFactory.h) は、`WindowAPI`, `DirectXCom`, `SpriteCom` などの「エンジンを構成する主要機能（サブシステム）」を初期化してアプリケーションに返す役割を持っています。
これだと、アプリケーション層（`main.cpp`など）がエンジンのパーツを直接個別に所有し、管理しなければなりません。

#### 移行・拡張案
*   **「Engineクラス（ファサード）」の作成:**
    `SubsystemFactory` を `Baziru3_Engine` の配下に移行するか、より根本的に `Baziru3_Engine` 全体をカプセル化する **`Engine` クラス**（あるいは `Application` 基底クラス）をエンジン層に作成します。
*   **アプリケーション起動の簡略化:**
    アプリケーション側は、エンジン内部のクラスを意識せず、以下のように「エンジン全体の初期化」と「実行」だけを行うようにします。
    ```cpp
    int WINAPI WinMain(...)
    {
        // エンジン全体の初期化（内部でWindow、DirectX、Sprite、Audioなどを自動初期化）
        Engine::Initialize("MyGame", 1280, 720);
        
        // メインループの実行
        Engine::Run(std::make_unique<TitleScene>());
        
        // 終了処理
        Engine::Finalize();
        return 0;
    }
    ```
    これにより、アプリケーションは「ゲームのロジックやアセット」に集中でき、エンジン自体の所有・管理責任をエンジン自身の中に閉じ込めることができます。

---

## 📈 移行によるメリット

1.  **保守性の向上:** アプリケーション側のコードから「DirectX12 の生リソース」や「描画周りの初期化」の記述が消え、ゲームプレイのコードが非常にシンプルになります。
2.  **パフォーマンス向上:** パーティクルシステムが一本化されることで、不要なコマンドリストのバインドや PSO の切り替えが減少し、GPU の並列処理能力をより活かせます。
3.  **ポートフォリオにおけるアピール:** 「オブジェクト指向設計」や「エンジンのカプセル化（疎結合な設計）」がしっかりとできていることを証明でき、技術力の評価につながります。
