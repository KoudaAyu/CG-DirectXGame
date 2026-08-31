#pragma once

#include "Vector.h"
#include "Matrix4x4.h"
#include "RenderContext.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3d.h"
#include <vector>
#include <memory>

class Object3dCom;
class Camera;
class MouseInput;

/**
 * @brief マウス照準による目標地点算出および放物線軌跡（レティクル・ドット線）の描画・管理クラス
 */
class AimGuide
{
public:
    AimGuide();
    ~AimGuide();

    void Initialize(Object3dCom* object3dCom, Camera* camera);
    void Update(const Vector3& launchPos, MouseInput* mouseInput, Camera* camera, bool isMerged);
    void Draw(const RenderContext& ctx);

    // --- ゲッター ---
    bool IsTargetValid() const { return isValidTarget_ && !isMerged_; }
    bool IsInRange() const { return isInRange_; }
    const Vector3& GetTargetPosition() const { return targetPos_; }
    const Vector3& GetCalculatedVelocity() const { return calculatedVelocity_; }
    float GetMaxRange() const { return maxRange_; }

private:
    Object3d::ModelData GenerateRingMesh(float innerRadius, float outerRadius, uint32_t segments);
    Object3d::ModelData GenerateSphereMesh(uint32_t sliceCount, uint32_t stackCount, float radius);

private:
    Object3dCom* object3dCom_ = nullptr;
    Camera* camera_ = nullptr;

    // 照準リング（レティクル）
    std::unique_ptr<Object3d> reticleObject_;
    Object3d::ModelData reticleModelData_;
    uint32_t reticleTextureIndex_ = 0;

    // 軌道プレビュードット群
    static constexpr size_t kDotCount = 12;
    std::vector<std::unique_ptr<Object3d>> dotObjects_;
    Object3d::ModelData dotModelData_;
    uint32_t dotTextureIndex_ = 0;

    // 計算結果
    Vector3 targetPos_{ 0.0f, 0.0f, 0.0f };
    Vector3 calculatedVelocity_{ 0.0f, 0.0f, 0.0f };
    bool isValidTarget_ = false;
    bool isInRange_ = false;
    bool isMerged_ = false;

    // パラメータ
    float maxRange_ = 24.0f;
    float gravity_ = -24.0f; // Minion の重力と一致
    float pulseTimer_ = 0.0f;
};
