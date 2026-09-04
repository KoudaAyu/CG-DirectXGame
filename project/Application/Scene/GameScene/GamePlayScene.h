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
    Object3d::ModelData groundModelData_;
    uint32_t groundTextureIndex_ = 0;

    // --- カメラ制御パラメータ (プレイヤー相対座標一定モデル) ---
    float cameraDistance_ = 18.0f;        // プレイヤーからのカメラ距離
    float cameraPitch_ = 0.82f;           // 見下ろし角度 (rad, 0.82 rad ≈ 47.0度)
    float cameraYaw_ = 0.0f;             // 方位角 (rad)
    float cameraFov_ = 0.85f;            // 垂直視野角 (rad, 0.85 rad ≈ 48.7度)
    float cameraTargetOffsetY_ = 1.0f;   // プレイヤー足元からの注視点高さ
    float cameraForwardOffset_ = 2.0f;   // 前方視界確保用の注視点Z前進オフセット
    float cameraDynamicZoom_ = 3.0f;     // 合体巨大化時のカメラ後退倍率
    bool followStageTilt_ = false;       // ステージ傾斜にカメラ回転を連動させるか
    // 臨界減衰スプリング（SmoothDamp）パラメータ
    float cameraSmoothTimePos_ = 0.08f;  // カメラY/Z追従スムーズ時間 (秒)
    float cameraSideLagTime_ = 0.18f;    // カメラX（左右）追従スムーズ時間 (秒: 心地よいラバーストラップ感)
    float cameraSmoothTimeRot_ = 0.24f;  // カメラ角度補間スムーズ時間 (秒: カクつきゼロの優雅な旋回)
    float cameraDynamicBank_ = 0.025f;   // 左右移動時の微小ロールバンク強度 (rad/(m/s))
    float tiltSmoothTime_ = 0.15f;       // ステージ傾斜の補間スムーズ時間 (秒: 重厚で滑らかな板の傾き)

    Vector3 currentCameraPos_{ 0.0f, 15.0f, -10.0f }; // 現在の補間カメラ位置
    Vector3 currentCameraRot_{ 0.82f, 0.0f, 0.0f };   // 現在の補間カメラ回転
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
