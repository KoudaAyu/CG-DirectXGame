# Baziru3 Engine (自作3Dゲームエンジン)

[![DebugBuild](https://github.com/KoudaAyu/CG-DirectXGame/actions/workflows/DebugBuild.yml/badge.svg)](https://github.com/KoudaAyu/CG-DirectXGame/actions/workflows/DebugBuild.yml)
[![DevelopmentBuild](https://github.com/KoudaAyu/CG-DirectXGame/actions/workflows/Development.yml/badge.svg)](https://github.com/KoudaAyu/CG-DirectXGame/actions/workflows/Development.yml)
[![ReleaseBuild](https://github.com/KoudaAyu/CG-DirectXGame/actions/workflows/Release.yml/badge.svg)](https://github.com/KoudaAyu/CG-DirectXGame/actions/workflows/Release.yml)


C++ および DirectX 12 を用いてスクラッチから構築した、自作3Dゲームエンジンプロジェクトです。
商用コンソールゲーム開発におけるパフォーマンス要求（ロード時間の極小化、動的メモリ確保の抑制、空間分割による物理演算最適化など）をクリアするための、低レイヤでの最適化アーキテクチャ設計を実証することを目的としています。

## 🎨 ポートフォリオ技術発表スライド
本エンジン、および実装ゲームシステムに関する詳細な技術解説・設計図面をまとめた提出用スライド資料を公開しています。

### 1. エンジン最適化スライド（動的物理とメモリ最適化）
> **【要約】自作エンジン「Baziru3 Engine」の低レイヤにおけるメモリ・衝突判定の最適化（定数バッファ用リングバッファアロケータ、短寿命用スタックアロケータ、空間ハッシュによる衝突判定高速化、データ指向設計）の設計と数学的解説資料です。**
* **📄 [コウダ_アユ_Baziru3_Engineについて (PDF)](%E3%82%B3%E3%82%A6%E3%83%80_%E3%82%A2%E3%83%A6_Baziru3_Engine%E3%81%AB%E3%81%A4%E3%81%84%E3%81%A6.pdf)** （※提出用・日本語ファイル名）
* **📄 [技術発表スライド資料 (PDF)](sega_portfolio_slides.pdf)** （※英数字ファイル名リンク）
  * スライドのMarkdownソースはこちら：[sega_portfolio_slides.md](sega_portfolio_slides.md)

### 2. ゲーム・AIシステムスライド（6月度 月間進捗報告 / 技術研究発表資料）
> **【要約】開発中の3Dアクションシューティングゲームにおける、ゲームプレイとAIシステム（対角OBBマルチコライダー、トンネル現象を防ぐ連続線分CCD判定、レーザー照準同期、敵AIのカバー・捜索・巡回行動、およびAABBツリー階層構造による計算最適化）の実証とスケジュール資料です。**
* **📄 [コウダ_アユ_射撃と遮蔽物の動的物理インタラクションについて (PDF)](%E3%82%B3%E3%82%A6%E3%83%80_%E3%82%A2%E3%83%A6_%E5%B0%84%E6%92%83%E3%81%A8%E9%81%AE%E8%94%BD%E7%89%A9%E3%81%AE%E5%8B%95%E7%9A%84%E7%89%A9%E7%90%86%E3%82%A4%E3%83%B3%E3%82%BF%E3%83%A9%E3%82%AF%E3%82%B7%E3%83%A7%E3%83%B3%E3%81%AB%E3%81%A4%E3%81%84%E3%81%A6.pdf)** （※提出用・日本語ファイル名）
* **📄 [ゲーム・AI技術スライド資料 (PDF)](sega_gameplay_interaction_slides.pdf)** （※英数字ファイル名リンク）
  * スライドのMarkdownソースはこちら：[sega_gameplay_interaction_slides.md](sega_gameplay_interaction_slides.md)

---


## 🛠️ 技術スタック

* **言語**: C++ (C++20 / ISO C++ 最新規格)
* **グラフィックス API**: DirectX 12 (Direct3D 12)
* **シェーダー言語**: HLSL (Shader Model 6.x)
* **開発環境**: Visual Studio 2022 (MSBuild, Platform Toolset v143 / v145)
* **主要外部ライブラリ**:
  * **ImGui**: デバッグインターフェース用
  * **imgui-node-editor**: Behavior Tree 等のノード編集ツール用
  * **Assimp**: 3Dモデル（GLTF/OBJ等）インポート用
  * **DirectXTex**: テクスチャロード・処理用

---

## 🎯 技術的アピールポイント（最適化への取り組み）

### 1. メモリ管理システム (Memory Management System) - 動的確保の抑制

#### 【背景と課題】
ゲーム実行中の頻繁な動的メモリ確保（`new`/`delete`）は、**メモリ断片化（フラグメンテーション）**の原因となり、最悪の場合メモリアロケータのボトルネックによるフレームレート低下（ヒッチング）を引き起こします。特に DirectX 12 における定数バッファの都度生成は非常に大きな負荷となります。

#### 【解決手法】
* **定数バッファ用リングバッファアロケータ (`ConstantBufferAllocator`)**
  * トリプルバッファリング構成（3フレーム分の領域を事前に一括確保）を採用し、CPUとGPUのアクセス非同期競合を回避しつつ、256バイトの境界アラインメントを自動調整して高速にメモリを切り出します。
* **短寿命オブジェクト用スタックアロケータ (`StackAllocator`)**
  * 1フレームで使い捨てる一時オブジェクト用に起動時に一括でメモリプールを確保。ポインタを前進させるだけ（計算量 $O(1)$）でアロケートし、フレーム終了時にポインタを先頭に戻すだけで一括論理解放する仕組みを構築しました。

#### 【効果】
* 実行中の動的アロケーションを完全に排除し、メモリ断片化およびアロケーション負荷を防止。

#### 【図面：定数バッファアロケータのトリプルバッファリング同期構造】
```mermaid
graph TD
    subgraph CPU ["CPU 側 (データ書き込み)"]
        Alloc[ConstantBufferAllocator] -->|Allocate| Frame0[Frame Buffer 0]
        Alloc -->|Allocate| Frame1[Frame Buffer 1]
        Alloc -->|Allocate| Frame2[Frame Buffer 2]
    end
    subgraph GPU ["GPU 側 (レンダリング実行)"]
        Frame0 -->|GPUが読み出し中| Draw[描画コマンド実行]
    end
    subgraph Sync ["同期制御"]
        Fence[Fence Value 同期] -.->|GPU読み出し完了まで| Frame0
        Fence -.->|安全になったらCPU書き込み許可| Alloc
    end
    style Frame0 fill:#2b6cb0,stroke:#3182ce,stroke-width:2px,color:#fff
    style Frame1 fill:#2d3748,stroke:#4a5568,stroke-width:1px,color:#a0aec0
    style Frame2 fill:#2d3748,stroke:#4a5568,stroke-width:1px,color:#a0aec0
```

---

### 2. 衝突判定の最適化 (Collision Optimization) - 空間分割とデータ指向

#### 【背景と課題】
オブジェクト数が数百〜数千規模に増大した際、総当たり判定（計算量 $O(N^2)$）を行うとCPU負荷が跳ね上がり、ゲームの処理落ちを引き起こします。

#### 【解決手法】
* **グリッドベース空間ハッシュ分割 (`SpatialHashCell`)**
  * 3D空間をグリッドセル（サイズ 10.0f）に区切り、各オブジェクトをハッシュ値に変換して登録。隣接セル内のオブジェクト同士のみを判定対象とします。
* **データ指向設計 (Data-Oriented Design)**
  * クラス階層を巡るポインタ参照を廃止し、判定に必要なパラメータ（座標、サイズ、回転等）のみをメモリ上で連続する配列に並べて一括イテレート処理します。

#### 【効果】
* 判定の計算量を $O(N)$ に削減し、メモリアクセスのキャッシュミス（Cache Miss）によるCPUボトルネックを極小化。

#### 【図面：空間ハッシュによる衝突判定高速化アルゴリズムフロー】
```mermaid
graph TD
    Pos[オブジェクトのワールド座標] -->|セルサイズ 10.0f で除算| Grid[グリッド座標を算出]
    Grid -->|ビット演算子による合成| Hash[ユニークなハッシュキーを生成]
    Hash -->|ハッシュテーブルへ登録| Table[SpatialHashCells テーブル]
    Table -->|同一 & 隣接セル内のみ抽出| Filter[衝突判定ペアを構築]
    Filter -->|総当たり回数を大幅削減| Compare[精密衝突判定実行]
    Compare -->|めり込みあり| Push["押し出し解決 & コールバック起動"]
```

---

### 3. データ駆動AIシステム (Data-Driven AI System) - イテレーション効率化

#### 【背景と課題】
AI（Behavior Tree）の挙動調整のたびにソースコードの再コンパイル・ビルドを行うと、イテレーション速度が低下しゲームのゲームプレイ調整の妨げになります。

#### 【解決手法】
* **ビジュアルノードエディタ (`imgui-node-editor` の統合)**
  * ゲーム実行中にImGui上でノード（Selector, Sequence, Action）を接続してAIの思考木をリアルタイムに構築可能に。
  * 編集結果は瞬時にJSONアセットとしてファイル出力され、それをエンジン側が動的に読み込み・再構成（Blackboardの変数共有も対応）します。

#### 【効果】
* プログラマー以外のプランナー等でも、ゲームを実行したままAIの挙動調整が可能になり、イテレーション効率を向上。

#### 【図面：Behavior Tree と Blackboard メモリの連携構成】
```mermaid
graph TD
    subgraph Decision ["意思決定部 (BehaviorTree)"]
        Root[Root Node] -->|Tick| Selector[Selector Node]
        Selector -->|失敗なら次へ| Sequence[Sequence Node]
        Sequence -->|成功なら次へ| Cond[Condition Node]
        Cond -->|True| Action["Action Node (移動/攻撃等)"]
    end
    subgraph Data ["データ部 (Blackboard)"]
        BB[Blackboard Memory] <-->|標的・速度等のパラメータ読み書き| Action
        BB <-->|状態チェック| Cond
    end
```

---

## 📖 API取扱説明書 (API Usage)

### 1. エンジンの初期化とサブシステム取得

#### [main.cpp](file:///c:/Users/k024g/OneDrive/デスクトップ/Engine_ver2026/project/main.cpp) の実装例
```cpp
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{    
    std::unique_ptr<Framework> game = std::make_unique<Game>();
    game->Run();
    return 0;
}
```

#### サブシステム取得例 ([Game.cpp](file:///c:/Users/k024g/OneDrive/デスクトップ/Engine_ver2026/project/Game.cpp))
```cpp
auto* dx = engine_->GetDirectXCom();       // DirectX 12 描画デバイス
auto* window = engine_->GetWindowAPI();     // Window管理
auto* spriteCom = engine_->GetSpriteCom(); // スプライト共通コンポーネント
```

### 2. 定数バッファの割り当て例

```cpp
auto cbAllocator = engine_->GetConstantBufferAllocator();

// 256バイトアラインメント調整されたメモリの切り出し
auto allocation = cbAllocator->Allocate(sizeof(MyConstBufferData));

// CPU書き込み
MyConstBufferData* cbData = static_cast<MyConstBufferData*>(allocation.cpuAddress);
cbData->worldMatrix = worldMatrix;

// GPUコマンドへセット
commandList->SetGraphicsRootConstantBufferView(rootParamIndex, allocation.gpuAddress);
```

### 3. コライダーの登録と衝突イベントコールバック

```cpp
#include "CollisionManager.h"
#include "SphereCollider.h"

// 衝突形状とカテゴリの決定
auto collider = std::make_unique<SphereCollider>(CollisionAttribute::Player);
collider->SetRadius(2.5f);

// 衝突時イベントのバインド
collider->SetOnCollision([](const CollisionInfo& info) {
    if (info.other->GetAttribute() == CollisionAttribute::Bullet) {
        TakeDamage();
    }
});

// マネージャへの登録
CollisionManager::GetInstance()->RegisterCollider(collider.get());
```

---

## 🚀 開発ロードマップ & 進捗

- [x] **DirectX 12 描画基礎**: パイプライン、シェーダバインド、定数バッファ管理
- [x] **アニメーション**: スケルトン、ジョイント、スキンクアスタ対応（Assimp統合）
- [x] **デバッグ環境**: ImGui 統合、および GUI Behavior Tree エディタ
- [ ] **メモリ最適化**: CBV用リングバッファ、スタック/プールアロケータのフル統合
- [ ] **衝突最適化**: 八分木空間分割およびデータ指向設計 (DOD)
- [ ] **グラフィックス強化**: カスケードシャドウマップ (CSM) の実装
- [ ] **Assetバイナリ化**: パースを伴わない高速ロードとマルチスレッド非同期読み込み

---

## ⚙️ 動作環境・ビルド手順

### 前提要件
* **OS**: Windows 10 / 11
* **開発ツール**: Visual Studio 2022
* **SDK**: Windows SDK 10.0.x 以上

### ビルド手順
1. このリポジトリをクローンします。
   ```bash
   git clone https://github.com/KoudaAyu/CG-DirectXGame.git
   ```
2. `project/DirectXGame.sln` を Visual Studio 2022 で開きます。
3. 構成（Debug/Development/Release）を選択し、ビルド（F7）を実行します。
