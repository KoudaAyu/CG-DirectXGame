#pragma once
#include <memory>
#include <string>
#include "Object3d.h"
#include "Object3dCom.h"
#include "Camera.h"
#include "RenderContext.h"
#include "Baziru3_Engine/Framework/Collision/BoxCollider.h"
#include "Baziru3_Engine/Framework/Collision/MeshCollider.h"

class Obstacle
{
public:
    void Initialize(Object3dCom* object3dCom, Camera* camera, const Vector3& position, float radius, const std::string& modelFilename = "fence.obj", const Vector3& scale = { 1.0f, 1.0f, 1.0f }, const Vector3& rotation = { 0.0f, 0.0f, 0.0f });
    void Update();
    void Draw(const RenderContext& ctx);
    void Finalize();

    Vector3 GetPosition() const { return position_; }
    float GetRadius() const { return radius_; }
    const std::string& GetModelFilename() const { return modelFilename_; }
    bool IsTree() const { return isTree_; }
    bool IsBirchTree() const { return isBirchTree_; }
    bool IsRiverWalkBarrier() const { return isRiverWalkBarrier_; }
    bool IsBroken() const { return isBroken_; }
    bool IsBreakableSupplyCrate() const { return isBreakableSupplyCrate_; }
    bool IsExplosiveBarrel() const { return isExplosiveBarrel_ && !isBroken_; }
    bool IsBarrelLeaking() const { return isBarrelLeaking_ && !isBroken_; }
    const Vector3& GetBarrelLeakPos() const { return barrelLeakPos_; }
    const Vector3& GetBarrelLeakDir() const { return barrelLeakDir_; }
    void ConfigureAsBreakableSupplyCrate();
    bool DamageBarrel(const Vector3& hitPos, const Vector3& leakNormal, float chainFuseOverride = -1.0f);
    bool TickBarrelFuse(float dt);
    void TriggerHitFlinch(const Vector3& hitDir = { 0.0f, 0.0f, 1.0f });
    void BreakObstacle(const Vector3& hitDir = { 0.0f, 0.0f, 1.0f });
    BoxCollider* GetCollider() const { return collider_.get(); }
    BoxCollider* GetCollider2() const { return extraColliders_.empty() ? nullptr : extraColliders_[0].get(); }
    const std::vector<std::unique_ptr<BoxCollider>>& GetExtraColliders() const { return extraColliders_; }
    MeshCollider* GetMeshCollider() const { return meshCollider_.get(); }

private:
    struct CrateDebrisPiece
    {
        std::unique_ptr<Object3d> obj;
        Vector3 pos{};
        Vector3 vel{};
        Vector3 rot{};
        Vector3 angVel{};
        Vector3 baseScale{ 1.0f, 1.0f, 1.0f };
        Vector4 baseColor{ 1.0f, 1.0f, 1.0f, 1.0f };
        float age = 0.0f;
        float lifeTime = 2.6f;
        int bounceCount = 0;
        bool settled = false;
    };

    std::unique_ptr<Object3d> object3d_;
    std::string modelFilename_;
    Vector3 position_;
    Vector3 rotation_;
    float radius_;
    float treeSwayPhase_ = 0.0f;
    float hitFlinchTimer_ = 0.0f;
    Vector3 hitFlinchDir_ = { 0.0f, 0.0f, 1.0f };
    bool isTree_ = false;
    bool isBirchTree_ = false;
    bool isRiverWalkBarrier_ = false;
    bool isBroken_ = false;
    bool isBreakableSupplyCrate_ = false;
    bool isExplosiveBarrel_ = false;
    bool isBarrelLeaking_ = false;
    int barrelHp_ = 3;
    float barrelFuseTimer_ = 2.6f;
    Vector3 barrelLeakPos_ = { 0.0f, 0.6f, 0.0f };
    Vector3 barrelLeakDir_ = { 0.0f, 0.35f, 1.0f };
    Vector3 baseScale_ = { 1.0f, 1.0f, 1.0f };
    Vector4 baseColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };
    std::unique_ptr<BoxCollider> collider_;
    std::vector<std::unique_ptr<BoxCollider>> extraColliders_;
    std::unique_ptr<MeshCollider> meshCollider_;
    std::vector<CrateDebrisPiece> debrisPieces_;
    Vector3 rot1_;
    Vector3 rot2_;
    std::vector<Vector3> extraRotations_;
    Object3dCom* object3dCom_ = nullptr;
    Camera* camera_ = nullptr;
    uint32_t defaultTextureIndex_ = UINT32_MAX;
    uint32_t customTextureIndex_ = UINT32_MAX;
    uint32_t woodCrateTexIndex_ = UINT32_MAX;
    uint32_t woodCrateDamagedTexIndex_ = UINT32_MAX;
};
