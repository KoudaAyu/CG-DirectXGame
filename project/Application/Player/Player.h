#pragma once
#include <memory>
#include "Object3d.h"
#include "Object3dCom.h"
#include "Camera.h"
#include "RenderContext.h"

class Player
{
public:
    void Initialize(Object3dCom* object3dCom, Camera* camera);
    // Update now accepts optional MouseInput pointer so player can face cursor
    void Update(class MouseInput* mouseInput = nullptr);
    void Draw(const RenderContext& ctx);
    void Finalize();
    Vector3 GetPosition() const { return object3d_ ? object3d_->GetTranslate() : Vector3{ 0.0f, 0.0f, 0.0f }; }
    Vector3 GetRotation() const { return object3d_ ? object3d_->GetRotate() : Vector3{ 0.0f, 0.0f, 0.0f }; }

    // 被弾・HP関連のメソッド
    void TakeDamage(float damage);
    float GetHP() const { return hp_; }
    float GetMaxHP() const { return maxHp_; }
    float GetHPRatio() const { return maxHp_ > 0.0f ? hp_ / maxHp_ : 0.0f; }
    bool IsDead() const { return isDead_; }
    void Reset();

private:
    std::unique_ptr<Object3d> object3d_;
    Object3dCom* object3dCom_ = nullptr;
    Camera* camera_ = nullptr;
    uint32_t defaultTextureIndex_ = UINT32_MAX;

    // プレイヤーのステータス
    float hp_ = 100.0f;
    float maxHp_ = 100.0f;
    bool isDead_ = false;
    float invincibilityTimer_ = 0.0f;
    const float invincibilityDuration_ = 1.0f; // 被弾後の無敵時間（秒）
};

