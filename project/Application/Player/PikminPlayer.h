#pragma once

#include "Vector.h"
#include "Matrix4x4.h"
#include "RenderContext.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3d.h"
#include <memory>

class Object3dCom;
class Camera;
class KeyInput;
class MinionManager;

/**
 * @brief ピクミン×ロコロコ プレイヤーキャラクター
 */
class PikminPlayer {
public:
    PikminPlayer();
    ~PikminPlayer() = default;

    void Initialize(Object3dCom* object3dCom, Camera* camera, const Vector3& startPos = { 0.0f, 0.0f, 0.0f });
    void Update(float deltaTime, KeyInput* keyInput, MinionManager* minionManager);
    void Draw(const RenderContext& ctx);

    // --- ゲッター / セッター ---
    const Vector3& GetPosition() const { return position_; }
    void SetPosition(const Vector3& pos);

    float GetYaw() const { return rotation_.y; }
    Vector3 GetForwardVector() const;

    bool IsMerged() const { return isMerged_; }
    void ToggleMerge();
    void SetMerged(bool merged);

    // パラメータ
    float GetNormalSpeed() const { return normalMoveSpeed_; }
    void SetNormalSpeed(float s) { normalMoveSpeed_ = s; }
    float GetMergedSpeed() const { return mergedMoveSpeed_; }
    void SetMergedSpeed(float s) { mergedMoveSpeed_ = s; }

    float GetThrowPower() const { return throwPower_; }
    void SetThrowPower(float p) { throwPower_ = p; }

private:
    Object3dCom* object3dCom_ = nullptr;
    Camera* camera_ = nullptr;

    std::unique_ptr<Object3d> normalModel_;
    Object3d::ModelData normalModelData_;
    uint32_t normalTextureIndex_ = 0;

    std::unique_ptr<Object3d> giantModel_;
    Object3d::ModelData giantModelData_;
    uint32_t giantTextureIndex_ = 0;

    Vector3 position_{ 0.0f, 0.5f, 0.0f };
    Vector3 velocity_{ 0.0f, 0.0f, 0.0f };
    Vector3 rotation_{ 0.0f, 0.0f, 0.0f };
    Vector3 scale_{ 1.0f, 1.0f, 1.0f };

    bool isMerged_ = false;

    // パラメータ
    float normalMoveSpeed_ = 8.0f;
    float mergedMoveSpeed_ = 14.0f;
    float rotationSpeed_ = 12.0f;
    float throwPower_ = 16.0f;
    float throwUpPower_ = 7.5f;

    // 投擲クールダウン
    float throwCooldownTimer_ = 0.0f;
    float throwCooldown_ = 0.2f;

    // 合体時スケールイージング
    float mergeScaleAnimation_ = 1.0f;
};
