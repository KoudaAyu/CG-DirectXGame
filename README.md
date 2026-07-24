# Baziru3 Engine (自作3Dゲームエンジン)

[![DebugBuild](https://github.com/KoudaAyu/CG-DirectXGame/actions/workflows/DebugBuild.yml/badge.svg)](https://github.com/KoudaAyu/CG-DirectXGame/actions/workflows/DebugBuild.yml)
[![DevelopmentBuild](https://github.com/KoudaAyu/CG-DirectXGame/actions/workflows/Development.yml/badge.svg)](https://github.com/KoudaAyu/CG-DirectXGame/actions/workflows/Development.yml)
[![ReleaseBuild](https://github.com/KoudaAyu/CG-DirectXGame/actions/workflows/Release.yml/badge.svg)](https://github.com/KoudaAyu/CG-DirectXGame/actions/workflows/Release.yml)

C++ および DirectX 12 を用いてスクラッチから構築した、自作3Dゲームエンジンプロジェクトです。商用コンソールゲーム開発におけるパフォーマンス要求（ロード時間の極小化、動的メモリ確保の抑制、空間分割による物理演算最適化など）をクリアするための、低レイヤでの最適化アーキテクチャ設計を実証することを目的としています。

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
