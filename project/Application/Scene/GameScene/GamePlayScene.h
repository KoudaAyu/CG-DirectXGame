#pragma once

#include "BaseScene.h"
#include "Camera.h"
#include "KeyInput.h"
#include "Baziru3_Engine/Core/IO/Mouse/MouseInput.h"
#include "Baziru3_Engine/Graphics/Graphics/SceneRenderRequests.h"
#include "Application/GameObject/SlimeManager.h"
#include "Application/GameObject/AimGuide.h"
#include "Application/GameObject/PropellerObstacle.h"
#include "Application/Enemy/EnemyManager.h"
#include "Application/GameObject/CoinManager.h"
#include "Application/GameObject/GrowthCubeManager.h"
#include "Application/GameObject/StageTerrain.h"
#include "Application/Editor/PlacementEditor.h"
#include "Application/Scene/GameScene/BossFight.h"
#include "Application/Scene/GameScene/GamePlaySceneFX.h"
#include "Application/Scene/GameScene/GamePlaySceneHUD.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3d.h"
#include "Baziru3_Engine/Framework/Collision/MeshCollider.h"
#include "Application/GameObject/IrisTransition.h"

#include <memory>
#include <vector>

/**
 * @brief ピクミン×ロコロコ ゲームプレイシーン (GamePlayScene)
 */
class GamePlayScene : public BaseScene
{
public:
    GamePlayScene() = default;
    ~GamePlayScene() override = default;

    void InitializeScene() override;
    void Finalize() override;
    void Update() override;
    void Draw(SceneRenderRequests& renderRequests) override;

    const char* GetSceneType() const { return "GAMEPLAY"; }

private:
    void DrawDebugUI();

    /// @brief カメラシェイクを足す（0..1。足しこまれて上限 1.0 でクランプ）
    void AddCameraShake(float trauma);

    /// @brief カメラシェイクの減衰と、カメラへのオフセット適用
    /// @note currentCameraPos_ / currentCameraRot_ 自体は汚さない。
    ///       汚すと次フレームの SmoothDamp の基準がぶれて揺れが残り続ける
    void UpdateCameraShake(float deltaTime);

    /// @brief マウス右ドラッグでカメラの方位角（cameraYaw_）を回す
    /// @note 俯瞰角（cameraPitch_）は動かさない。lookAt でいうと
    ///       「eye が target を通る Y 軸のまわりを回る」だけ
    void UpdateCameraOrbit();

    /// @brief 残機 ＝ スライムの数 ＝ プレイヤーの塊サイズ + フィールドのミニオンの強さの合計
    int CalculateLifeCount() const;

    /**
     * @brief リザルト（score / time / coin）を SceneContext へ書き出す
     * @note ClearScene::LoadResultFromSceneContext() が同じキーを読む。
     *       データの実体は SceneManager が持っているのでシーンをまたいで生き残る。
     *       CLEAR へ抜ける直前と Finalize() の両方で呼んでいて、
     *       どの経路で抜けても最新の値が入っている状態にしてある
     *       （Finalize() は新しいシーンの Initialize() より先に走る）
     */
    void PublishResultToSceneContext();

    /// @brief 演出・HUD へイベントを流し込む（実装は GamePlaySceneFX / HUD 側）
    void UpdateFxAndHud(float deltaTime);

    /// @brief プレイ <-> 配置エディタ の切り替え（F2）
    void SetEditMode(bool edit);

#if defined(_DEBUG) || defined(USE_IMGUI)
    /// @brief デバッグカメラの更新（F4 / C キーでトグル）
    void UpdateDebugCamera(float deltaTime);
#endif

private:
    std::unique_ptr<KeyInput> keyInput_;
    std::unique_ptr<MouseInput> mouseInput_;
    std::unique_ptr<Camera> playCamera_;

    std::unique_ptr<SlimeManager> slimeManager_;
    std::unique_ptr<AimGuide> aimGuide_;
    std::vector<std::unique_ptr<PropellerObstacle>> propellerObstacles_;
    std::unique_ptr<EnemyManager> enemyManager_;
    std::unique_ptr<CoinManager> coinManager_;
    std::unique_ptr<GrowthCubeManager> growthCubeManager_;
    std::unique_ptr<PlacementEditor> placementEditor_;

    // 地形メッシュ群。以前はここに stageParts_ をハードコードで持っていたが、
    // 配置エディタから編集できるよう StageTerrain へ切り出して JSON 駆動にした
    std::unique_ptr<StageTerrain> stageTerrain_;

    // ボス戦フェーズ（ボス本体・弾・HPバー・カメラ演出）
    std::unique_ptr<BossFight> bossFight_;

    // 演出と HUD。中身はそれぞれ GamePlaySceneFX.cpp / GamePlaySceneHUD.cpp にある
    std::unique_ptr<GamePlaySceneFx> fx_;
    std::unique_ptr<GamePlaySceneHud> hud_;

    // シーンに入る前のカメラ。抜けるときに必ず戻す。
    // ここを nullptr のままにして抜けると、TITLE / CLEAR が
    // Object3dCom::GetDefaultCamera() を借りられず、
    // スライムや花火が「2回目以降だけ出ない」状態になる
    Camera* previousDefaultCamera_ = nullptr;
    Camera* previousSceneCamera_ = nullptr;

    // --- リザルト（CLEAR へ渡す値）---
    int score_ = 0;                //!< 敵を倒したときに増える
    float elapsedSeconds_ = 0.0f;  //!< 経過時間（秒）

    // --- カメラシェイク ---
    float shakeTrauma_ = 0.0f;        //!< 0..1。時間で減衰する
    float shakeTime_ = 0.0f;          //!< 疑似ノイズの位相
    float shakeDecay_ = 2.2f;         //!< 1秒あたりの減衰量
    float shakeAmplitude_ = 0.55f;    //!< trauma 1.0 のときの最大ずれ (m)
    float shakeRollAmount_ = 0.05f;   //!< trauma 1.0 のときの最大ロール (rad)
    float shakeFrequency_ = 26.0f;    //!< 揺れの速さ
    float shakeOnEnemyHit_ = 0.32f;   //!< 敵と衝突したとき（軽め）
    float shakeOnSelfDestruct_ = 0.8f;//!< プレイヤー自爆（やや強め）

    bool isEditMode_ = false;                          //!< 配置エディタ中か
    Vector4 groundBaseColor_{ 0.55f, 0.85f, 0.50f, 1.0f }; //!< 地面の草原カラー

    /// @brief ボス戦の演出中で、プレイヤーを動かせない状態か
    /// @note BossFight::Update() が返してくるので、次のフレームの入力抑制に使う
    bool bossFreezeSlimes_ = false;

    // --- スライム初期スポーン位置 ---
    Vector3 spawnBasePos_{ 0.0f, 0.55f, 30.0f }; // 初期スポーン基準位置（島中央の平原: Z = 30.0f）
    float spawnGroupOffsetZ_ = 4.0f;            // 小スライム群の前方オフセット
    void RespawnSlimesAtBase();
    void RestartGame();

    // --- カメラ制御パラメータ (プレイヤー相対座標一定モデル) ---
    float cameraDistance_ = 30.0f;        // プレイヤーからの基準カメラ距離（ステージ全体を見渡しやすいゆったりとした距離）
    float cameraPitch_ = 0.93f;           // 見下ろし角度 (rad, 0.93 rad ≈ 53.3度: 上空俯瞰視点)
    float cameraYaw_ = 0.0f;             // 方位角 (rad)。右ドラッグで回る

    // --- 右ドラッグによるカメラ旋回 ---
    float cameraOrbitSensitivity_ = 0.006f; // マウス移動1ドットあたりの回転量 (rad)
    bool cameraOrbitInvert_ = false;        // ドラッグの向きを反転させる
    bool isCameraOrbiting_ = false;         // いま右ドラッグ中か
    float cameraFov_ = 0.85f;            // 垂直視野角 (rad, 0.85 rad ≈ 48.7度)
    float cameraTargetOffsetY_ = 0.8f;   // プレイヤー足元からの注視点高さ
    float cameraForwardOffset_ = 0.5f;   // 注視点Z前進オフセット（スライムを画面中央にしっかりと捉える）
    float cameraDynamicZoom_ = 1.5f;     // 合体巨大化時のカメラ後退倍率
    float cameraSpreadZoom_ = 0.12f;     // 群れの広がりに対するカメラ後退倍率
    float maxSpreadOffset_ = 4.0f;       // 広がりによる追加後退の最大上限値 (m)
    float minCameraDist_ = 22.0f;        // カメラ距離の下限ガード (m)
    float maxCameraDist_ = 42.0f;        // カメラ距離の上限ガード (m)
    bool followStageTilt_ = false;       // ステージ傾斜にカメラ回転を連動させるか
    // 臨界減衰スプリング（SmoothDamp）パラメータ
    float cameraSmoothTimePos_ = 0.10f;  // カメラY/Z追従スムーズ時間 (高速移動時もフレームアウトしない機敏な追従)
    float cameraSideLagTime_ = 0.10f;    // カメラX（左右）追従スムーズ時間
    float cameraSmoothTimeRot_ = 0.20f;  // カメラ角度補間スムーズ時間
    float cameraDynamicBank_ = 0.025f;   // 左右移動時の微小ロールバンク強度 (rad/(m/s))
    float tiltSmoothTime_ = 0.35f;       // ステージ傾斜の補間スムーズ時間 (秒: 重厚で滑らかな板の傾き)

    // ズーム（距離・広がり・重心）の急変を防止するスムーズダンピングパラメータ
    float currentCameraDist_ = 30.0f;     // 現在の補間カメラ距離
    float cameraDistVelocity_ = 0.0f;    // カメラ距離の補間速度
    float cameraZoomSmoothTime_ = 0.55f; // カメラ距離（ズーム）の追従スムーズ時間（合体・分裂時の急激なズーム変動を優雅に緩和）
    float currentGroupSpread_ = 0.0f;    // 補間された群れの広がり
    float groupSpreadVelocity_ = 0.0f;   // 群れ広がりの変化速度
    float groupSpreadSmoothTime_ = 0.60f;// 群れの広がり収縮のスムーズ時間（合体でミニオンが消えたときの急ズームを防止）
    Vector3 currentFocusPos_{ 0.0f, 0.0f, 0.0f }; // 補間注視点位置
    Vector3 focusPosVelocity_{ 0.0f, 0.0f, 0.0f }; // 注視点追従速度
    float focusSmoothTime_ = 0.10f;      // 注視点スムーズ時間（機敏な重心追従）

    Vector3 currentCameraPos_{ 0.0f, 18.0f, -12.0f }; // 現在の補間カメラ位置
    Vector3 currentCameraRot_{ 0.93f, 0.0f, 0.0f };   // 現在の補間カメラ回転

    // 実際にカメラへ入れた最終値（ステージ揺らし＋ボスへのフォーカス補間まで込み）。
    // カメラシェイクはこれを土台に足す。currentCameraPos_ を土台にすると
    // ボス演出中のフォーカスがシェイクで打ち消されてしまう
    Vector3 appliedCameraPos_{ 0.0f, 18.0f, -12.0f };
    Vector3 appliedCameraRot_{ 0.93f, 0.0f, 0.0f };
    Vector3 cameraPosVelocity_{ 0.0f, 0.0f, 0.0f };   // カメラ位置の追従速度
    Vector3 cameraRotVelocity_{ 0.0f, 0.0f, 0.0f };   // カメラ角度の追従角速度
    Vector2 tiltVelocity_{ 0.0f, 0.0f };              // ステージ傾斜の角速度
    bool cameraInitialized_ = false;

    // ステージ傾斜（ティルト）パラメータ
    Vector2 currentTilt_{ 0.0f, 0.0f };  // X: Pitch (手前/奥), Y: Roll (左/右)
    Vector2 targetTilt_{ 0.0f, 0.0f };
    float maxTiltAngle_ = 0.28f;         // 最大傾斜角 (約16度, rad)

    // ステージ揺らし（バウンス・シェイク）パラメータ
    float stageBounceOffset_ = 0.0f;     // ステージの瞬間垂直浮上量 (m)
    float stageBounceVelocity_ = 0.0f;   // ステージ垂直バウンス速度
    float stageShakeTimer_ = 0.0f;       // ステージ回転シェイク減衰タイマー
    float stageShakeDuration_ = 0.28f;   // シェイク持続時間
    float stageShakeIntensity_ = 0.035f; // シェイク回転強度 (rad)
    Vector3 cameraShakeOffset_{ 0.0f, 0.0f, 0.0f }; // カメラ衝撃オフセット
    float cameraShakeIntensity_ = 0.0f;  // カメラ衝撃強度
    float stageShakeCooldown_ = 0.0f;    // 連打防止クールダウン

    bool isInitialized_ = false;

    // ゲームオーバー演出（Iris Out）
    bool isGameOverTransition_ = false;
    float gameOverDelayTimer_ = 0.0f;

    // --- デバッグカメラ ---
    bool isDebugCamera_ = false;
    Vector3 debugCameraPos_{ 0.0f, 20.0f, -20.0f };
    Vector3 debugCameraRot_{ 0.6f, 0.0f, 0.0f };
    float debugCameraSpeed_ = 30.0f;
    float debugCameraRotSpeed_ = 0.003f;
};
