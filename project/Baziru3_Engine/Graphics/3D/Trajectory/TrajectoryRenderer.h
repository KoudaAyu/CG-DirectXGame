#pragma once

#include "Vector.h"
#include "Matrix4x4.h"
#include "RenderContext.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3d.h"
#include <vector>
#include <memory>

class DirectXCom;
class Object3dCom;
class Camera;
class KeyInput;

/**
 * @brief 放物線・投擲軌道予測描画ツール (Trajectory Visualizer)
 */
class TrajectoryRenderer {
public:
    static TrajectoryRenderer* GetInstance();

    void Initialize(DirectXCom* dxCommon, Object3dCom* object3dCom);
    void Finalize();

    /**
     * @brief 毎フレームの入力・カメラ更新（エンジン内部で自動追従）
     */
    void Update(float deltaTime, KeyInput* keyInput, Camera* activeCamera);

    /**
     * @brief 軌道予測パラメータに基づいて放物線ドットと着地サークルを描画
     */
    void Draw(const RenderContext& ctx, const Vector3& startPos, const Vector3& forwardDir);

    /**
     * @brief 自動追従された発射位置・向きで描画
     */
    void DrawAuto(const RenderContext& ctx);

    /**
     * @brief 射出初速ベクトルを計算
     */
    Vector3 CalculateLaunchVelocity(const Vector3& forwardDir) const;

    /**
     * @brief 着地予定地点の座標を計算
     */
    Vector3 CalculateLandingPoint(const Vector3& startPos, const Vector3& forwardDir) const;

    /**
     * @brief ImGui 調整ツールの描画
     */
    void DrawImGui();

    // ゲッター / セッター
    float GetThrowPower() const { return throwPower_; }
    void SetThrowPower(float p) { throwPower_ = p; }
    float GetUpPower() const { return upPower_; }
    void SetUpPower(float u) { upPower_ = u; }
    float GetGravity() const { return gravity_; }
    void SetGravity(float g) { gravity_ = g; }
    bool IsVisible() const { return isVisible_; }
    void SetVisible(bool v) { isVisible_ = v; }

    float GetAimYaw() const { return aimYaw_; }
    void SetAimYaw(float yaw) { aimYaw_ = yaw; }

private:
    TrajectoryRenderer() = default;
    ~TrajectoryRenderer() = default;
    TrajectoryRenderer(const TrajectoryRenderer&) = delete;
    TrajectoryRenderer& operator=(const TrajectoryRenderer&) = delete;

private:
    DirectXCom* dxCommon_ = nullptr;
    Object3dCom* object3dCom_ = nullptr;

    static constexpr int kMaxSteps = 16;
    std::unique_ptr<Object3d> dotObjects_[kMaxSteps];
    std::unique_ptr<Object3d> landingMarker_;

    Object3d::ModelData dotModelData_;
    uint32_t dotTextureIndex_ = 0;

    // 自動追従ステート
    Vector3 currentLaunchPos_{ 0.0f, 0.5f, 0.0f };
    float aimYaw_ = 0.0f; // ラジアン
    bool autoFollowInput_ = true;

    // チューニングパラメータ (PikminPlayer / Minion の物理値と完全一致)
    float throwPower_ = 16.0f;
    float upPower_ = 7.5f;
    float gravity_ = -24.0f;
    int stepCount_ = 11;
    float timeStep_ = 0.055f;
    float groundLevel_ = 0.2f;

    bool isVisible_ = true;
    bool showLandingMarker_ = true;
    Vector4 dotColor_{ 1.0f, 0.95f, 0.2f, 0.9f };    // 鮮やかな投擲イエロー
    Vector4 landingColor_{ 0.2f, 1.0f, 0.5f, 0.85f }; // 着地点グリーン
};
