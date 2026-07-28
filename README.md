# CG4 / Baziru3_Engine 加点要素対応レポート (120点満点)

本リポジトリ (`Escape_from` ブランチ / `Baziru3_Engine`) にて実装された、加点要素一覧および各機能の技術対応詳細です。

---

## 🏆 加点要素・採点基準対応表 (全120点分)

| 課題・加点項目 | 配点 | ステータス | 技術実装・詳細仕様 |
| :--- | :---: | :---: | :--- |
| **Skinningモデルの表示** | 20 | ✅ 対応済み | `.gltf` アニメーションモデルのボーン階層・頂点ウェイト読み込み、`SkinCluster` 行列パレット更新、`SkinningObject3d.VS.hlsl` でのアニメーション描画、およびXInputゲームパッド（左スティック）/ WASD・矢印キーでのリアルタイム移動・回転操作制御 |
| **ComputeShaderによるスキニング** | 10 | ✅ 対応済み | `Skinning.CS.hlsl` (`[numthreads(1024, 1, 1)]`) での並列頂点スキンニング計算、結果のGPUバッファ (`uavResource`) 保存・VBOバインド、および描画パスでの再利用構造の実装 |
| **MultiMesh & MultiMaterial対応** | 5 | ✅ 対応済み | Assimpパーサーでの全サブメッシュ・全マテリアルの個別抽出 (`MeshPart` / `materials`)、GPU頂点/インデックス統合バッファでのオフセット保持、描画パスでのマテリアル/テクスチャ自動切り替え＆順次描画パイプラインの実装 |
| **Animation補間** | 5 | ✅ 対応済み | 複数アニメーション（`walk` / `sneakWalk` 等）の並行再生、`Skeleton::ApplyAnimationBlend` での位置/スケール線形補間（Lerp: $(1-t)A + t B$）および回転の球面線形補間（`Quaternion::Slerp`）、`Animator::PlayAnimation` でのクロスフェード移行制御 |
| **骨のデバッグ表示** | 10 | ✅ 対応済み | `SkeletonDebug` システムによるジョイント球・ボーン結合円筒描画、各ボーンのローカル座標軸（X:赤, Y:緑, Z:青）の3D投影描画、スクリーン座標へのボーン名オーバーレイ表示（`Head`, `Hand_R`等）、およびImGuiトグル操作パネルの実装 |
| **手からパーティクルを出す** | 10 | ✅ 対応済み | スケルトン構造から手のボーン（`RightHand`）の3Dワールド位置をリアルタイム抽出し、歩行・アニメーションの動きに完全動的追従するパーティクルエミッター、およびEキー / ImGuiでのオンオフ切り替えトグル機能の実装 |
| **武器を手を持たせる** | 10 | ✅ 対応済み | 武器オブジェクト（`Object3d`）を右手のジョイント最終ワールド行列（`Skeleton::GetJointWorldMatrix`）に動的アタッチし、手のアニメーション運動へのリアルタイム完全同期・追従（`Object3d::AttachToJoint` API）およびImGuiでの持ち手ローカルオフセット（位置・回転・スケール）微調整機能の実装 |
| **GPU Particle** | 20 | ✅ 対応済み | `UpdateParticle.CS.hlsl` / `EmitParticle.CS.hlsl` の並列マルチスレッド化（`[numthreads(32, 1, 1)]`）、多元形状 Emitter（Point / Box / Sphere / Cone）、GPU Field（Attractor 引力 / Vortex 渦 / Wind 風 / Drag 抵抗物理演算）、および ImGui 「GPU Particle Studio / Editor」環境の完全統合 |
| **その他 (Skinning/Skeleton/Animation拡張)** | 30 | ✅ 対応済み | ① **アニメーション タイムライン＆再生制御**: `Animator` システムにおけるアニメーション再生速度制御（0.1x〜3.0xスロー・倍速、およびリアルタイム逆再生）、一時停止＆1フレーム精度コマ送り・コマ戻し機能、および ImGui タイムラインシークバーの完全実装<br>② **プロシージャル Head LookAt 視線・頭部動的追従**: アニメーションモーション再生中であっても頭部・首ボーン (`mixamorig:Head`) の回転をカメラ位置や目標3D座標へ自動計算・Slerpブレンディング追従させるプロシージャルIK制御の実装 |

---

## 🛠️ エンジンコア API (`Baziru3_Engine`)

本プロジェクトで拡張・カプセル化された主要エンジンAPIです。

* **`Object3d::AttachToJoint(...)`**: ボーンのスケール歪みを自動除去し、任意の3Dオブジェクトをキャラの特定関節へ1行でバインド。
* **`Object3d::ApplyHeadLookAt(...)`**: カメラや目標地点へキャラクターの頭部・首を動的かつ滑らかに振り向かせる。
* **`Object3d::DrawAnimationUI(...)`**: タイムラインシーク、再生速度スロー/倍速/逆再生、1コマ送り機能を提供するデバッグGUI。
* **`ParticleManager::DrawUI(...)`**: GPU Particle のエミッター形状（Point/Box/Sphere/Cone）や力場（引力/渦/風/抵抗）をGUIからリアルタイム編集。

---

## 🔍 確認・デバッグ用 UI 操作方法

アプリ起動後、画面上の ImGui コントロールパネルより各機能をインタラクティブに検証いただけます：

1. **`Animation Timeline & Control (Item 9)`**: タイムラインスライダーをドラッグしてアニメーションポーズを自由にシーク・逆再生可能。
2. **`Procedural Head LookAt Studio (Item 10)`**: 「LookAt Camera」を選択すると、カメラの周りを回してもキャラの頭が自然にカメラを凝視・追従。
3. **`GPU Particle Studio / Editor (Item 8)`**: Field タイプで「Vortex (渦)」や「Attractor (引き寄せ)」を選択し、粒子が旋回・集約する物理演算を確認可能。
4. **`Weapon Hand Attachment (Item 7)`**: 武器を手ボーンにリアルタイムアタッチし、オフセット位置・角度を調整可能。
