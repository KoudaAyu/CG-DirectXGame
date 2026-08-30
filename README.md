# Baziru3 Game Engine (自作3Dゲームエンジン)

[![DebugBuild](https://github.com/KoudaAyu/CG-DirectXGame/actions/workflows/DebugBuild.yml/badge.svg)](https://github.com/KoudaAyu/CG-DirectXGame/actions/workflows/DebugBuild.yml)
[![DevelopmentBuild](https://github.com/KoudaAyu/CG-DirectXGame/actions/workflows/Development.yml/badge.svg)](https://github.com/KoudaAyu/CG-DirectXGame/actions/workflows/Development.yml)
[![ReleaseBuild](https://github.com/KoudaAyu/CG-DirectXGame/actions/workflows/Release.yml/badge.svg)](https://github.com/KoudaAyu/CG-DirectXGame/actions/workflows/Release.yml)

C++ および DirectX 12 を用いてスクラッチから構築した、**「即座にゲーム開発へ着手できる」完全な自作3Dゲームエンジン基盤**です。  
商用コンソールゲーム開発におけるパフォーマンス要求（ロード時間の極小化、動的メモリ確保の抑制、空間分割による物理演算最適化、GPUコンピュートシェーダー活用など）をクリアするための低レイヤ最適化アーキテクチャを実装しています。

---

## 🚀 クイックスタート (ゲーム開発の始め方)

本ブランチ (`master` / `GE3_Game`) は、**純粋なゲームエンジンと最小スターターテンプレート** として完全に整理されています。ここから新しいブランチを切ることで、即座にオリジナルゲームの開発を開始できます。

```bash
# 1. リポジトリのクローン
git clone https://github.com/KoudaAyu/CG-DirectXGame.git

# 2. 新規ゲーム開発用ブランチの作成
git checkout -b feature/my_awesome_game
```

### 🎮 ゲーム実装の起点
- **[`project/Application/Scene/GameScene/GamePlayScene.cpp`](project/Application/Scene/GameScene/GamePlayScene.cpp)**  
  ゲームのメインループとなるシーンです。ここにキャラクター、ステージ、敵AI、UI等のゲームロジックを追加していきます。
- **シーン遷移ステートマシン**:
  - `TitleScene` ⇄ `GamePlayScene` ⇄ `ClearScene` / `GameOverScene`
  - **SPACE キー** または 画面上のデバッグボタンで各シーンへシームレスに遷移します。

> 📖 詳細な実装チュートリアルは [docs/How_To_Start_Game_Development.md](docs/How_To_Start_Game_Development.md) をご覧ください。

---

## 🛠️ エンジン主要機能・アーキテクチャ一覧

### 1. レンダリング・グラフィックス基盤 (DirectX 12)
- **3D オブジェクト描画**: OBJ / glTF (Assimp) 形式の3Dモデルロード、マテリアル・テクスチャ自動バインド。
- **スキニングメッシュアニメーション**: GPU スキニングとボーン行列パレットの高速更新。
- **スカイボックス & 環境マップ**: キューブマップ DDS テクスチャによる全天球レンダリング。
- **2D スプライト & UI**: 画面直交座標系 (Orthographic) に最適化された 2D レンダラー。
- **GPU パーティクルシステム**: Compute Shader (`EmitParticle.CS`, `UpdateParticle.CS`) による超大量パーティクルシミュレーション。

### 2. メモリ & パフォーマンス最適化
- **定数バッファ・リングアロケータ (`ConstantBufferAllocator`)**: 1フレーム内の動的メモリ確保をゼロにし、GPU への定数バッファ転送を極小オーバーヘッドで実現。
- **FreeList メモリ管理**: パーティクルやオブジェクトのインデックス再利用により、断片化のないメモリ再利用を実現。
- **GPU プロファイラ (`GpuProfiler`)**: パイプライン・各描画パスごとの GPU 実行時間をミリ秒単位でリアルタイム計測。

### 3. 物理・衝突判定 (Collision System)
- **空間分割ハッシュ (`SpatialHashCell`)**: 大量のコライダー同士の判定計算量を $O(N^2)$ から $O(N)$ へ削減。
- **多彩なコライダー形状**: `SphereCollider`, `BoxCollider` (OBB), `CapsuleCollider`, `MeshCollider` (ポリゴン三角メッシュ判定)。
- **連続衝突判定 (CCD)**: 高速移動する弾丸のすり抜け（トンネル現象）を防止。
- **多重押し出し解決 (Accumulated Push)**: 複数オブジェクトとの同時接触時のめり込みを正確に解決。

### 4. ゲームプレイ支援 & ツール連携
- **AI ビヘイビアツリー & NavMesh/A* 経路探索**: 視界判定・音検知・カバーリング行動をサポートする敵AI基盤。
- **Blender レベルエディタ連携**: Blender 上で配置したステージ・障害物データを JSON 経由で一括インポート。
- **Dear ImGui デバッグシステム**: リアルタイムパラメータ調整、コライダーのワイヤーフレーム可視化、シーン手動遷移。

---

## 📁 ディレクトリ構成

```
Engine/
├── docs/                        # ドキュメント・週間スプリント報告
│   ├── How_To_Start_Game_Development.md  # ゲーム開発開始ガイド
│   └── sprints/                 # アジャイル開発スプリント報告書
├── project/
│   ├── Application/             # アプリケーション層 (ゲーム本体ロジック)
│   │   ├── Scene/               # 各種シーン (Title, GamePlay, Clear, GameOver)
│   │   ├── GameObject/          # ゲームオブジェクト (障害物, 弾丸, キャラクター)
│   │   └── Particle/            # アプリ層軽量パーティクル
│   ├── Baziru3_Engine/          # エンジンコア層 (DirectX12, メモリ, 物理, レンダラー)
│   │   ├── Core/                # DirectX12基盤, カメラ, アロケータ
│   │   ├── Framework/           # シーン管理, 空間ハッシュ衝突判定, AI基盤
│   │   └── Graphics/            # 2D/3D描画, パイプライン, パーティクル
│   ├── Resources/               # テクスチャ, 3Dモデル, 音声, シェーダー
│   ├── Game.cpp / Game.h        # エンジン統括メインループ
│   └── main.cpp                 # エントリーポイント
└── README.md                    # 本ドキュメント
```

---

## 🔨 ビルド & 実行方法

### 動作環境
- **OS**: Windows 10 / 11 (64-bit)
- **IDE**: Visual Studio 2022 以降 (C++20 対応)
- **グラフィックス**: DirectX 12 対応 GPU (Feature Level 11_0 以上)

### 手順
1. `project/DirectXGame.sln` を Visual Studio で開きます。
2. 構成を **`Debug | x64`** または **`Release | x64`** に設定します。
3. **F5 キー**（デバッグ実行）を押してビルド・実行します。

---

## 📊 スプリント開発・技術発表資料

- 📄 **[ポートフォリオ技術発表スライド (PDF)](sega_portfolio_slides.pdf)**
- 📄 **[ゲーム・AI技術スライド資料 (PDF)](sega_gameplay_interaction_slides.pdf)**
- 📁 **[週間スプリント評価・レポート一覧](docs/sprints/)**
