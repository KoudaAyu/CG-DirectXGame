#pragma once
#include <memory>
#include "Object3d.h"
#include "Object3dCom.h"
#include "Camera.h"
#include "RenderContext.h"

class Target
{
public:
    void Initialize(Object3dCom* object3dCom, Camera* camera, const Vector3& position, float radius = 1.0f);
    void Update(float deltaTime);
    void Draw(const RenderContext& ctx);
    void Finalize();

    Vector3 GetPosition() const { return position_; }
    float GetRadius() const { return radius_; }

    // 弾丸が着弾した時の処理
    void OnHit(int damage);
    bool IsDead() const { return isDead_; }
    int GetHP() const { return hp_; }
    void Reset();

private:
    std::unique_ptr<Object3d> object3d_;
    Vector3 position_;
    float radius_;
    Object3dCom* object3dCom_ = nullptr;
    uint32_t defaultTextureIndex_ = UINT32_MAX;

    int hp_ = 3;
    int maxHp_ = 3;
    bool isDead_ = false;
};
