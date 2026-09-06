#pragma once

#include "BaseScene.h"
#include "Camera.h"
#include "KeyInput.h"
#include "Baziru3_Engine/Core/IO/Mouse/MouseInput.h"
#include "Baziru3_Engine/Graphics/Graphics/SceneRenderRequests.h"
#include "Application/Player/PikminPlayer.h"
#include "Application/Minion/MinionManager.h"
#include "Application/GameObject/AimGuide.h"
#include "Application/GameObject/PropellerObstacle.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3d.h"
#include "Baziru3_Engine/Framework/Collision/MeshCollider.h"

#include <memory>

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

private:
    std::unique_ptr<KeyInput> keyInput_;
    std::unique_ptr<MouseInput> mouseInput_;
    std::unique_ptr<Camera> playCamera_;

    std::unique_ptr<PikminPlayer> player_;
    std::unique_ptr<MinionManager> minionManager_;
    std::unique_ptr<AimGuide> aimGuide_;
    std::vector<std::unique_ptr<PropellerObstacle>> propellerObstacles_;

    std::unique_ptr<Object3d> groundPlane_;
    std::unique_ptr<MeshCollider> groundCollider_;
    Object3d::ModelData groundModelData_;
    uint32_t groundTextureIndex_ = 0;
    float groundScale_ = 0.25f; // 地面ステージの縮小スケール (適度な広さ: 幅約75m)

    // --- カメラ制御パラメータ (プレイヤー相対座標一定モデル) ---
    float cameraDistance_ = 21.0f;        // プレイヤーからのカメラ距離（ゆったり見晴らせる高さ）
    float cameraPitch_ = 0.93f;           // 見下ろし角度 (rad, 0.93 rad ≈ 53.3度: 上空俯瞰視点)
    float cameraYaw_ = 0.0f;             // 方位角 (rad)
    float cameraFov_ = 0.85f;            // 垂直視野角 (rad, 0.85 rad ≈ 48.7度)
    float cameraTargetOffsetY_ = 1.2f;   // プレイヤー足元からの注視点高さ
    float cameraForwardOffset_ = 2.0f;   // 前方視界確保用の注視点Z前進オフセット
    float cameraDynamicZoom_ = 1.6f;     // 合体巨大化時のカメラ後退倍率（過剰なズーム変動を抑制）
    bool followStageTilt_ = false;       // ステージ傾斜にカメラ回転を連動させるか
    // 臨界減衰スプリング（SmoothDamp）パラメータ
    float cameraSmoothTimePos_ = 0.18f;  // カメラY/Z追従スムーズ時間 (秒: 急激なショックを緩和)
    float cameraSideLagTime_ = 0.20f;    // カメラX（左右）追従スムーズ時間 (秒: 心地よいラバーストラップ感)
    float cameraSmoothTimeRot_ = 0.24f;  // カメラ角度補間スムーズ時間 (秒: カクつきゼロの優雅な旋回)
    float cameraDynamicBank_ = 0.025f;   // 左右移動時の微小ロールバンク強度 (rad/(m/s))
    float tiltSmoothTime_ = 0.15f;       // ステージ傾斜の補間スムーズ時間 (秒: 重厚で滑らかな板の傾き)

    // ズーム（距離・広がり・重心）の急変を防止するスムーズダンピングパラメータ
    float currentCameraDist_ = 21.0f;     // 現在の補間カメラ距離
    float cameraDistVelocity_ = 0.0f;    // カメラ距離の補間速度
    float cameraZoomSmoothTime_ = 0.55f; // カメラ距離（ズーム）の追従スムーズ時間（合体・分裂時の急激なズーム変動を優雅に緩和）
    float currentGroupSpread_ = 0.0f;    // 補間された群れの広がり
    float groupSpreadVelocity_ = 0.0f;   // 群れ広がりの変化速度
    float groupSpreadSmoothTime_ = 0.60f;// 群れの広がり収縮のスムーズ時間（合体でミニオンが消えたときの急ズームを防止）
    Vector3 currentFocusPos_{ 0.0f, 0.0f, 0.0f }; // 補間注視点位置
    Vector3 focusPosVelocity_{ 0.0f, 0.0f, 0.0f }; // 注視点追従速度
    float focusSmoothTime_ = 0.20f;      // 注視点スムーズ時間（重心ジャンプの防止）

    Vector3 currentCameraPos_{ 0.0f, 18.0f, -12.0f }; // 現在の補間カメラ位置
    Vector3 currentCameraRot_{ 0.93f, 0.0f, 0.0f };   // 現在の補間カメラ回転
    Vector3 cameraPosVelocity_{ 0.0f, 0.0f, 0.0f };   // カメラ位置の追従速度
    Vector3 cameraRotVelocity_{ 0.0f, 0.0f, 0.0f };   // カメラ角度の追従角速度
    Vector2 tiltVelocity_{ 0.0f, 0.0f };              // ステージ傾斜の角速度
    bool cameraInitialized_ = false;

    // ステージ傾斜（ティルト）パラメータ
    Vector2 currentTilt_{ 0.0f, 0.0f };  // X: Pitch (手前/奥), Y: Roll (左/右)
    Vector2 targetTilt_{ 0.0f, 0.0f };
    float maxTiltAngle_ = 0.28f;         // 最大傾斜角 (約16度, rad)

    bool isInitialized_ = false;
};
