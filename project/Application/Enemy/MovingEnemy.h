#pragma once
#include <memory>
#include <vector>
#include "Object3d.h"
#include "Object3dCom.h"
#include "Camera.h"
#include "RenderContext.h"
#include "Baziru3_Engine/Framework/Collision/SphereCollider.h"
#include "Baziru3_Engine/Framework/Collision/CollisionManager.h"
#include "SplinePath.h"
#include "SplineFollower.h"

#include "Enemy.h"

class Bullet;
class Sprite;
class WindowAPI;
class Obstacle;
#include "Baziru3_Engine/Framework/AI/BehaviorTree.h"

class MovingEnemy : public Enemy
{
public:
    MovingEnemy() = default;
    ~MovingEnemy() override = default;

    void Initialize(Object3dCom* object3dCom, Camera* camera) override;
    void Update(WindowAPI* windowAPI, const Vector3* targetPosition, const std::vector<std::unique_ptr<Obstacle>>& obstacles, float deltaTime, bool isPlayerInCover = false) override;
    void Draw(const RenderContext& ctx) override;
    void Finalize() override;
    void OnHit(const Vector3& attackerPos) override;

    std::unique_ptr<Bullet> TryShoot(const Vector3& targetPosition) override;

    SphereCollider* GetCollider() const { return collider_.get(); }
    void SetAlertSprites(Sprite* bar, Sprite* dot) { alertBar_ = bar; alertDot_ = dot; }

    // スプライン巡回移動システム
    void SetPatrolPath(const std::vector<Vector3>& points, bool isLoop = true, float speed = EnemyTuning::kDefaultSplineSpeed);
    void SetSimplePatrolPoints(const Vector3& a, const Vector3& b, float speed = EnemyTuning::kDefaultSimpleSpeed);
    void SetTacticalRole(TacticalRole role) override;

    // 音源検知・無線リンクのトリガー
    void HearNoise(const Vector3& noisePosition) override;
    void AlertEnemy(const Vector3& targetPos) override;

protected:
    Sprite* alertBar_ = nullptr;
    Sprite* alertDot_ = nullptr;

    // AI Patrol parameters
    Vector3 patrolA_ = EnemyTuning::kDefaultPatrolPointA;
    Vector3 patrolB_ = EnemyTuning::kDefaultPatrolPointB;
    bool movingToB_ = true;
    float moveSpeed_ = EnemyTuning::kDefaultMoveSpeed; // 戦術機動速度

    // スプライン巡回制御
    std::unique_ptr<BaziruEngine::SplinePath> splinePath_;
    std::unique_ptr<BaziruEngine::SplineFollower> splineFollower_;
    bool useSplinePatrol_ = true;

    std::unique_ptr<BaziruEngine::AI::BehaviorTree> behaviorTree_;
    bool useBehaviorTree_ = true;

    // --- カバー＆ピーク射撃・側面回り込み用パラメータ ---
    bool isPeeking_ = false;
    float peekTimer_ = 0.0f;
    Vector3 activePeekPos_ = { 0.0f, 0.0f, 0.0f };
    Vector3 actualCoverPos_ = { 0.0f, 0.0f, 0.0f };
    float coverIgnoreTimer_ = 0.0f;
    float pathRepathTimer_ = 0.0f;
    float patrolPauseTimer_ = 0.0f;
    float patrolPauseCooldown_ = 0.0f;
    bool returningToSpline_ = false;
    Vector3 splineRejoinPos_ = { 0.0f, 0.0f, 0.0f };
    std::vector<Vector3> cachedChasePath_;
};
