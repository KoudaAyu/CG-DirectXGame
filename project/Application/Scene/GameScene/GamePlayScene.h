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

    // カメラパラメータ
    Vector3 cameraOffset_{ 0.0f, 12.0f, -16.0f };

    // ステージ傾斜（ティルト）パラメータ
    Vector2 currentTilt_{ 0.0f, 0.0f };  // X: Pitch (手前/奥), Y: Roll (左/右)
    Vector2 targetTilt_{ 0.0f, 0.0f };
    float maxTiltAngle_ = 0.28f;         // 最大傾斜角 (約16度, rad)
    float tiltSpeed_ = 4.5f;             // 傾斜速度 (rad/s)
    float tiltReturnSpeed_ = 6.0f;       // 水平復帰速度 (rad/s)

    bool isInitialized_ = false;
};
