# Baziru3 Engine (自作3Dゲームエンジン)

[![DebugBuild](https://github.com/KoudaAyu/CG-DirectXGame/actions/workflows/DebugBuild.yml/badge.svg)](https://github.com/KoudaAyu/CG-DirectXGame/actions/workflows/DebugBuild.yml)
[![DevelopmentBuild](https://github.com/KoudaAyu/CG-DirectXGame/actions/workflows/Development.yml/badge.svg)](https://github.com/KoudaAyu/CG-DirectXGame/actions/workflows/Development.yml)
[![ReleaseBuild](https://github.com/KoudaAyu/CG-DirectXGame/actions/workflows/Release.yml/badge.svg)](https://github.com/KoudaAyu/CG-DirectXGame/actions/workflows/Release.yml)

C++ および DirectX 12 を用いてスクラッチから構築した、ゲームデベロッパー向け技術アピール用自作3Dゲームエンジンプロジェクトです。
商用コンソールゲーム開発におけるパフォーマンス要求（ロード時間の極小化、動的メモリ確保の抑制、空間分割による物理演算最適化など）をクリアするための低レイヤでの最適化アーキテクチャ設計を実証することを目的としています。

---

## 🛠️ 技術スタック

* **言語**: C++ (C++20 / ISO C++ 最新規格)
* **グラフィックス API**: DirectX 12 (Direct3D 12)
* **シェーダー言語**: HLSL (Shader Model 6.x)
* **開発環境**: Visual Studio 2026 / 2022 (MSBuild, Platform Toolset v143 / v145)
* **主要外部ライブラリ**:
  * **ImGui**: デバッグインターフェース用
  * **imgui-node-editor**: Behavior Tree 等のノード編集ツール用
  * **Assimp**: 3Dモデル（GLTF/OBJ等）インポート用
  * **DirectXTex**: テクスチャロード・処理用

---

## 🎯 技術的アピールポイント（最適化への取り組み）

### 1. メモリ管理システム (Memory Management System) - 動的確保の抑制

**【背景と課題】**
ゲーム実行中の頻繁な動的メモリ確保（`new` / `delete`）は、**メモリ断片化（フラグメンテーション）**の原因となり、最悪の場合メモリアロケータのボトルネックによるフレームレート低下（ヒッチング）を引き起こします。特に DirectX 12 における定数バッファの都度生成は非常に大きな負荷となります。

**【解決手法】**
* **定数バッファ用リングバッファアロケータ (ConstantBufferAllocator)**: 毎フレームのバッファ生成を廃止し、起動時に一括確保したアップロードバッファをCPU-GPU同期フェンスを用いてリング状に使い回す仕組み。
* **スタックアロケータ / プールアロケータ**: 短寿命オブジェクトや同一サイズオブジェクト（コライダー等）の超高速な切り出し・一括リセットを $O(1)$ で実現。

### 2. 衝突判定の高速化 (Collision Optimization)
* **八分木 (Octree) 空間分割**: ワールド空間を再帰的に分割し、総当たり判定回数を削減。
* **データ指向設計 (DOD)**: キャッシュミスを極小化するため、判定に必要なデータ（AABB、Sphere等）のみをメモリ上に連続して並べるデータ構造設計を適用。

### 3. DirectX 12 メガヒープ管理
* **ディスクリプタヒープマネージャ**: SRV/UAV 用に大きなメガヒープを起動時に1つだけ確保し、描画ごとのヒープ切り替えオーバーヘッドを削減。

### 4. ツール・エディタ基盤の構築
* **BehaviorTree エディタ**: `imgui-node-editor` を統合し、AIロジックをGUIノードベースでビジュアル編集・デバッグできる仕組みをエンジン層に構築。

---

## 🎨 ポートフォリオ技術発表スライド

本エンジン、および実装ゲームシステムに関する詳細な技術解説・設計図面をまとめた提出用スライド資料を公開しています。

### 1. エンジン最適化スライド（動的物理とメモリ最適化）

> 【要約】自作エンジン「Baziru3 Engine」の低レイヤにおけるメモリ・衝突判定の最適化（定数バッファ用リングバッファアロケータ、短寿命用スタックアロケータ、空間ハッシュによる衝突判定高速化、データ指向設計）の設計と数学的解説資料です。

- 📄 コウダ_アユ_Baziru3_Engineについて (PDF) （※提出用・日本語ファイル名）
- 📄 技術発表スライド資料 (PDF) （※英数字ファイル名リンク）
  - スライドのMarkdownソースはこちら：`sega_portfolio_slides.md`

### 2. ゲーム・AIシステムスライド（6月度 月間進捗報告 / 技術研究発表資料）

> 【要約】開発中の3Dアクションシューティングゲームにおける、ゲームプレイとAIシステム（対角OBBマルチコライダー、トンネル現象を防ぐ連続線分CCD判定、レーザー照準同期、敵AIのカバー・捜索・巡回行動、およびAABBツリー階層構造による計算最適化）の実証とスケジュール資料です。

- 📄 LE3B_09_コウダアユ_「射撃と遮蔽物の動的物理インタラクション」の実装 (PDF) （※提出用・日本語ファイル名）
- 📄 ゲーム・AI技術スライド資料 (PDF) （※英数字ファイル名リンク）
  - スライドのMarkdownソースはこちら：`sega_gameplay_interaction_slides.md`

### 3. アジャイル開発スプリント評価・進捗報告（7月度 週間報告まとめ）

> 【要約】7月度の3回にわたるスプリント開発（敵AI基本機能、MeshCollider & NavMesh/A*経路探索、CSベースGPUパーティクル & FreeListメモリ最適化）における、アジャイル・スプリントメンターからの評価・スコア推移と技術的成果の記録です。

#### 📊 7月度 スプリント評価スコア推移
| スプリント (日付) | 計画の質 | 実行の質 | 総合評価 | 主なマイルストーン・達成項目 | 原本レポート |
| :--- | :---: | :---: | :---: | :--- | :---: |
| [`07/12 スプリント`](#sprint-0712) | 85点 | 70点 | **60点** | 敵AI基本ステート（Patrol/Investigate/Chase）およびデバッグ表示の統合 | [📄 閲覧](docs/sprints/LE3B_09_コウダ_アユ_週間進捗07_12.md) |
| [`07/17 スプリント`](#sprint-0717) | 95点 | 90点 | **86点** | 当たり判定バグ完全修正（MeshCollider & Push）、NavMesh / A* 経路探索の実装 | [📄 閲覧](docs/sprints/LE3B_09_コウダ_アユ_週間報告07_17.md) |
| [`07/24 スプリント`](#sprint-0724) | 90点 | 95点 | **86点** | CSベースGPUパーティクル統合、`FreeList` による動的メモリ最適化 | [📄 閲覧](docs/sprints/LE3B_09_コウダ_アユ_週間報告07_24.md) |

---

#### <a id="sprint-0712"></a>🔹 1. [07/12 スプリント](docs/sprints/LE3B_09_コウダ_アユ_週間進捗07_12.md)：敵AI基本システムの構築と課題抽出
- **主な成果**: 敵AIの基本行動ステート（巡回スイング、音検知移動、射撃）を構築し、DebugUIによるステートのビジュアル検証を完了。
- **振り返り・カイゼン**: 当たり判定の視覚的ズレ（DoD未達）が発覚したため、次週でバグ修正（テクニカルデット解消）を最優先タスクに配置する計画へと軌道修正。
- 🔗 **評価資料原本**: [LE3B_09_コウダ_アユ_週間進捗07_12.md](docs/sprints/LE3B_09_コウダ_アユ_週間進捗07_12.md)

#### <a id="sprint-0717"></a>🔹 2. [07/17 スプリント](docs/sprints/LE3B_09_コウダ_アユ_週間報告07_17.md)：エンジンレイヤ移行と当たり判定・経路探索の完成
- **主な成果**:
  - **精密衝突判定**: `MeshCollider` によるポリゴン精度判定および重複衝突の多重押し出し（Accumulated Push）処理を実装し、当たり判定のズレを解消。
  - **AI経路探索**: NavMesh生成とA*経路探索を先行完了させ、ビヘイビアツリー（`test_cover_tree.json`）と接続。
  - **エンジン共通化**: アプリ層からエンジン層への共通コンテキスト・パーティクル描画の一本化リファクタリングを実施。
- 🔗 **評価資料原本**: [LE3B_09_コウダ_アユ_週間報告07_17.md](docs/sprints/LE3B_09_コウダ_アユ_週間報告07_17.md)

#### <a id="sprint-0724"></a>🔹 3. [07/24 スプリント](docs/sprints/LE3B_09_コウダ_アユ_週間報告07_24.md)：GPUコンピュートシェーダーとFreeListメモリ最適化
- **主な成果**:
  - **GPUパーティクル**: Compute Shader (`UpdateParticle.CS`, `EmitParticle.CS`) による大量描画基盤と `ParticleEmitter` を構築。
  - **FreeListメモリ管理**: アロケーション競合のない動的インデックス管理（`FreeList` システム）を導入し、CPU負荷ゼロに近い高速パーティクル生成・破棄を実現。
  - **DebugUI統合**: パラメータのリアルタイム調整・検証環境を整備。
- 🔗 **評価資料原本**: [LE3B_09_コウダ_アユ_週間報告07_24.md](docs/sprints/LE3B_09_コウダ_アユ_週間報告07_24.md)

---

## 🚀 開発ロードマップ & 進捗

現在のエンジン実装および最適化の進捗状況です。

- [x] **DirectX 12 描画基礎**: パイプライン、シェーダバインド、定数バッファ管理
- [x] **アニメーション**: スケルトン、ジョイント、スキンクアスタ対応（Assimp統合）
- [x] **デバッグ環境**: ImGui 統合、および GUI Behavior Tree エディタ
- [x] **当たり判定 & AI基盤**: MeshCollider, Accumulated Push, NavMesh & A* 経路探索
- [x] **GPUコンピューティング**: Compute Shader GPUパーティクル基盤 & FreeListメモリ最適化
- [ ] **レベルエディタ連携**: Blender配置Stage JSONロードと動的オブジェクト・NavMesh構築
- [ ] **グラフィックス強化**: カスケードシャドウマップ (CSM) の実装

---

## ⚙️ 動作環境・ビルド手順

### 前提要件
* **OS**: Windows 10 / 11
* **開発ツール**: Visual Studio 2026 / 2022
* **SDK**: Windows SDK 10.0.x 以上

### ビルド手順
1. このリポジトリをクローンします。
   ```bash
   git clone https://github.com/KoudaAyu/CG-DirectXGame.git
   ```
2. リポジトリ直下の `project/DirectXGame.sln` を Visual Studio で開きます。
3. 構成を `Debug`, `Development`, `Release` のいずれかに選択し、ビルド（F7）を実行します。
4. 実行（F5）して動作を確認します。
