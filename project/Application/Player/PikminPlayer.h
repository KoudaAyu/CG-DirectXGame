#pragma once

#include "Vector.h"
#include "Matrix4x4.h"
#include "RenderContext.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3d.h"
#include "Baziru3_Engine/Framework/Collision/SphereCollider.h"
#include <memory>

class Object3dCom;
class Camera;
class KeyInput;
class MouseInput;
class MinionManager;
class AimGuide;

/// @brief スライム用GPU定数バッファのCPU側構造体（Slime.hlsli の SlimeParams と一致させる）
struct SlimeParamsCPU
{
    float time = 0.0f;
    float wobbleStrength = 0.12f;
    float wobbleFrequency = 4.0f;
    float impulseStrength = 0.0f;
    Vector3 squashStretch{ 0.0f, 0.0f, 0.0f };
    float padding1 = 0.0f;
    Vector4 baseColor{ 0.2f, 0.85f, 1.0f, 0.9f };
    float fresnelPower = 3.0f;
    float envReflection = 0.4f;
    float innerGlow = 0.4f;
    float specularShininess = 64.0f;
};

/**
 * @brief ピクミン×ロコロコ プレイヤーキャラクター（スライム描画版）
 */
class PikminPlayer {
public:
    PikminPlayer();
    ~PikminPlayer();

    void Initialize(Object3dCom* object3dCom, Camera* camera, const Vector3& startPos = { 0.0f, 0.0f, 0.0f });
    void Update(float deltaTime, KeyInput* keyInput, MinionManager* minionManager, MouseInput* mouseInput = nullptr, AimGuide* aimGuide = nullptr, const Vector2& stageTilt = { 0.0f, 0.0f });
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
    float GetTiltAccel() const { return tiltAccel_; }
    void SetTiltAccel(float a) { tiltAccel_ = a; }
    float GetFriction() const { return friction_; }
    void SetFriction(float f) { friction_ = f; }

    // スライムパラメータの公開（ImGui調整用）
    SlimeParamsCPU& GetSlimeParams() { return slimeParams_; }
    SphereCollider* GetCollider() const { return collider_.get(); }
    float GetCurrentScale() const { return scale_.x; }

    float CalculateMergedScale(int minionCount) const;

    // 衝突時の弾性リアクション
    void OnCollision(const CollisionInfo& info);

private:
    void DrawSlime(Object3d* object, const Object3d::ModelData& modelData,
                   const RenderContext& ctx, uint32_t textureIndex);

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
    float tiltAccel_ = 38.0f;
    float friction_ = 2.6f;
    float rotationSpeed_ = 12.0f;

    // 投擲クールダウン
    float throwCooldownTimer_ = 0.0f;
    float throwCooldown_ = 0.2f;

    // 合体時スケールイージング
    float mergeScaleAnimation_ = 1.0f;
    float currentMergedScale_ = 0.8f;
    int lastAbsorbedCount_ = 0;

    // スライム固有
    SlimeParamsCPU slimeParams_;
    float totalTime_ = 0.0f;         // シェーダーに渡す累積時間
    Vector3 prevVelocity_{ 0.0f, 0.0f, 0.0f }; // スクワッシュ計算用の前フレーム速度

    // 合体時の当たり判定
    std::unique_ptr<SphereCollider> collider_;
};
