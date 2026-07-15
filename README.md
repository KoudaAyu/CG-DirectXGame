# Baziru3 Engine

[![DebugBuild](https://github.com/KoudaAyu/CG-DirectXGame/actions/workflows/DebugBuild.yml/badge.svg)](https://github.com/KoudaAyu/CG-DirectXGame/actions/workflows/DebugBuild.yml)
[![DevelopmentBuild](https://github.com/KoudaAyu/CG-DirectXGame/actions/workflows/Development.yml/badge.svg)](https://github.com/KoudaAyu/CG-DirectXGame/actions/workflows/Development.yml)
[![ReleaseBuild](https://github.com/KoudaAyu/CG-DirectXGame/actions/workflows/Release.yml/badge.svg)](https://github.com/KoudaAyu/CG-DirectXGame/actions/workflows/Release.yml)

C++ および DirectX 12 を用いてスクラッチから構築した、ゲームデベロッパー向け技術アピール用自作3Dゲームエンジンプロジェクトです。
商用コンソールゲーム開発におけるパフォーマンス要求（ロード時間極小化、動的メモリ確保の抑制、空間分割による物理演算最適化など）をクリアするためのアーキテクチャ設計を意識しています。

---

## 🛠️ 技術スタック

* **言語**: C++ (C++20 / ISO C++ 最新規格)
* **グラフィックス API**: DirectX 12 (Direct3D 12)
* **シェーダー言語**: HLSL (Shader Model 6.x)
* **開発環境**: Visual Studio 2022 (MSBuild, Platform Toolset v143 / v145)
* **サードパーティライブラリ**:
  * **ImGui**: デバッグインターフェース用
  * **imgui-node-editor**: Behavior Tree 等のノード編集ツール用
  * **Assimp**: 3Dモデル（GLTF/OBJ等）インポート用
  * **DirectXTex**: テクスチャロード・処理用

---

## ✨ 主要機能 & 技術アピールポイント

### 1. メモリ管理の最適化 (Custom Allocators)
ゲーム実行中の動的メモリ確保（`new`/`delete`）による断片化やフレームレート低下を防ぐため、用途に合わせたカスタムアロケータを実装。
* **CBV用リングバッファアロケータ**: 毎フレームのバッファ生成を廃止し、起動時に一括確保したアップロードバッファをCPU-GPU同期フェンスを用いてリング状に使い回す仕組み。
* **スタックアロケータ / プールアロケータ**: 短寿命オブジェクトや同一サイズオブジェクト（コライダー等）の超高速な切り出し・一括リセットを $O(1)$ で実現。

### 2. 衝突判定の高速化 (Collision Optimization)
* **八分木 (Octree) 空間分割**: ワールド空間を再帰的に分割し、総当たり判定回数を削減。
* **データ指向設計 (DOD)**: キャッシュミスを極小化するため、判定に必要なデータ（AABB、Sphere等）のみをメモリ上に連続して並べるデータ構造設計を適用。

### 3. DirectX 12 メガヒープ管理
* **ディスクリプタヒープマネージャ**: SRV/UAV 用に大きなメガヒープを起動時に1つだけ確保し、描画ごとのヒープ切り替えオーバーヘッドを削減。

### 4. ツール・エディタ基盤の構築
* **BehaviorTree エディタ**: `imgui-node-editor` を統合し、AIロジックをGUIノードベースでビジュアル編集・デバッグできる仕組みをエンジン層に構築。

---

## 🚀 開発ロードマップ & 進捗

現在のエンジン実装および最適化の進捗状況です。

- [x] **DirectX 12 描画基礎**: パイプライン、シェーダバインド、定数バッファ管理
- [x] **アニメーション**: スケルトン、ジョイント、スキンクアスタ対応（Assimp統合）
- [x] **デバッグ環境**: ImGui 統合、および GUI Behavior Tree エディタ
- [ ] **メモリ最適化**: CBV用リングバッファ、スタック/プールアロケータのフル統合
- [ ] **衝突最適化**: 八分木空間分割およびデータ指向設計 (DOD)
- [ ] **グラフィックス強化**: カスケードシャドウマップ (CSM) の実装
- [ ] **アassetバイナリ化**: パースを伴わない高速ロードとマルチスレッド非同期読み込み

---

## ⚙️ 動作環境・ビルド手順

### 前提要件
* **OS**: Windows 10 / 11
* **開発ツール**: Visual Studio 2022 (C++ によるデスクトップ開発ワークロードがインストールされていること)
* **SDK**: Windows SDK 10.0.x 以上

### ビルド手順
1. このリポジトリをクローンします。
   ```bash
   git clone https://github.com/KoudaAyu/CG-DirectXGame.git
   ```
2. リポジトリ直下の `project/DirectXGame.sln` を Visual Studio 2022 で開きます。
3. 構成を `Debug`, `Development`, `Release` のいずれかに選択し、ビルド（F7）を実行します。
4. 実行（F5）して動作を確認します。
