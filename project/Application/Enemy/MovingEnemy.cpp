#include "MovingEnemy.h"
#include "Baziru3_Engine/Framework/AI/BehaviorTree.h"
#include "Baziru3_Engine/Framework/AI/NavMesh.h"
#include "Object3d.h"
#include "Object3dCom.h"
#include "TextureManager.h"
#include "RenderContext.h"

#include "WindowsAPI.h"
#include "Sprite.h"
#include "Bullet.h"
#include "Obstacle.h"
#include "Baziru3_Engine/Framework/Collision/CollisionManager.h"
#include "Baziru3_Engine/Framework/Collision/SphereCollider.h"
#include "Baziru3_Engine/Framework/Collision/BoxCollider.h"
#include "Baziru3_Engine/Framework/Collision/CapsuleCollider.h"
#include <algorithm>
#include <cmath>

namespace
{
    using namespace EnemyTuning;

    Vector3 GetNearestWalkablePos(
        const Vector3& pos,
        float agentRadius = kQueryWalkableRadius,
        float searchRadiusMax = kDefaultSearchRadius,
        bool (*walkableFn)(const Vector3&, float) = nullptr)
    {
        const auto checkCell = [&](const Vector3& p) -> bool
        {
            return walkableFn(p, agentRadius);
        };

        if (checkCell(pos)) return pos;

        float minDistSq = kLargeInitDist;
        Vector3 bestPos = pos;
        bool found = false;

        for (float r = kWalkableGridStep; r <= searchRadiusMax; r += kWalkableGridStep)
        {
            for (float dx = -r; dx <= r; dx += kWalkableGridStep)
            {
                for (float dz = -r; dz <= r; dz += kWalkableGridStep)
                {
                    if (std::abs(dx) < r && std::abs(dz) < r) continue;
                    const Vector3 testPos = { pos.x + dx, 0.0f, pos.z + dz };
                    if (checkCell(testPos))
                    {
                        const float distSq = dx * dx + dz * dz;
                        if (distSq < minDistSq)
                        {
                            minDistSq = distSq;
                            bestPos = testPos;
                            found = true;
                        }
                    }
                }
            }
            if (found) break;
        }
        return bestPos;
    }
}

void MovingEnemy::Initialize(Object3dCom* object3dCom, Camera* camera)
{
    Enemy::Initialize(object3dCom, camera);

    position_ = patrolA_;
    prevFramePos_ = position_;
    spawnPosition_ = position_;
    object3d_->SetTranslate(position_);
    SetTacticalRole(TacticalRole::Patrol);

    state_ = AIState::Patrol;
    movingToB_ = true;

    behaviorTree_ = std::make_unique<BaziruEngine::AI::BehaviorTree>();
    if (!behaviorTree_->LoadFromJSON("test_cover_tree.json"))
    {
        useBehaviorTree_ = false;
    }
}

void MovingEnemy::SetTacticalRole(TacticalRole role)
{
    Enemy::SetTacticalRole(role);
    moveSpeed_ = (role == TacticalRole::Flanker) ? kFlankerMoveSpeed : kDefaultMoveSpeed;
}

void MovingEnemy::SetPatrolPath(const std::vector<Vector3>& points, bool isLoop, float speed)
{
    if (points.size() < 2) return;

    // 各巡回ウェイポイントを障害物外の安全な歩行可能セルへ補正
    std::vector<Vector3> safePoints;
    safePoints.reserve(points.size());
    for (const auto& pt : points)
    {
        safePoints.push_back(GetNearestWalkablePos(pt, kQueryWalkableRadius, kWaypointSnapRadius, &Enemy::IsWalkableArenaPos));
    }

    if (!splinePath_)
    {
        splinePath_ = std::make_unique<BaziruEngine::SplinePath>();
    }
    splinePath_->SetPoints(safePoints);
    splinePath_->SetLoop(isLoop);
    splinePath_->BuildDistanceTable(kSplineDistTableSamples);

    const auto mode = isLoop ? BaziruEngine::SplinePlayMode::Loop : BaziruEngine::SplinePlayMode::PingPong;

    if (!splineFollower_)
    {
        splineFollower_ = std::make_unique<BaziruEngine::SplineFollower>(splinePath_.get(), speed, mode);
        splineFollower_->SetAlignRotation(true);
    }
    else
    {
        splineFollower_->SetPath(splinePath_.get());
        splineFollower_->SetSpeed(speed);
        splineFollower_->SetPlayMode(mode);
        splineFollower_->Restart();
    }

    useSplinePatrol_ = true;
    returningToSpline_ = false;
    position_ = splineFollower_->GetPosition();
    splineRejoinPos_ = position_;
    prevFramePos_ = position_;
    spawnPosition_ = position_;
    if (object3d_)
    {
        object3d_->SetTranslate(position_);
    }
    UpdateVisualAndWeaponPose(0.0f);
}

void MovingEnemy::SetSimplePatrolPoints(const Vector3& a, const Vector3& b, float speed)
{
    patrolA_ = GetNearestWalkablePos(a, kQueryWalkableRadius, kWaypointSnapRadius, &Enemy::IsWalkableArenaPos);
    patrolB_ = GetNearestWalkablePos(b, kQueryWalkableRadius, kWaypointSnapRadius, &Enemy::IsWalkableArenaPos);
    moveSpeed_ = speed;
    useSplinePatrol_ = false;
    SetPosition(patrolA_);
}

void MovingEnemy::Update(WindowAPI* /*windowAPI*/, const Vector3* targetPosition, const std::vector<std::unique_ptr<Obstacle>>& obstacles, float deltaTime, bool isPlayerInCover)
{
    if (alertBar_) { alertBar_->SetSize({ 0.0f, 0.0f }); alertBar_->Update(); }
    if (alertDot_) { alertDot_->SetSize({ 0.0f, 0.0f }); alertDot_->Update(); }

    if (!UpdateCommonPreAI(deltaTime)) return;

    const float frameScale = deltaTime * kReferenceFps;
    if (coverIgnoreTimer_ > 0.0f)
    {
        coverIgnoreTimer_ = (std::max)(0.0f, coverIgnoreTimer_ - deltaTime);
    }

    // --- 索敵 ＆ 視界チェック ---
    float distToPlayer = kInitFarPlayerDist;
    const bool canSeePlayer = EvaluatePlayerSight(targetPosition, obstacles, isPlayerInCover, kMovingChaseSightMult, distToPlayer);

    if (canSeePlayer)
    {
        float detectSpeed = isPlayerInCover ? kDetectSpeedInCover : kDetectSpeedOpen;
        if (distToPlayer < kCloseDetectDistance) detectSpeed *= kCloseDetectMultiplier;
        detectionMeter_ += detectSpeed * deltaTime;
        if (detectionMeter_ >= 1.0f)
        {
            detectionMeter_ = 1.0f;
            if (state_ != AIState::Chase)
            {
                state_ = AIState::Chase;
                returningToSpline_ = false;
                if (splineFollower_) splineFollower_->Pause();
                alertTimer_ = kMovingChaseAlertDur;
                shotCooldownTimer_ = (std::min)(shotCooldownTimer_, kSpotMaxShotCooldown);
            }
        }
        if (targetPosition)
        {
            lastSeenPlayerPosition_ = *targetPosition;
        }
    }
    else if (state_ == AIState::Chase)
    {
        state_ = AIState::Investigate;
        returningToSpline_ = false;
        if (splineFollower_) splineFollower_->Pause();
        investigateTarget_ = lastSeenPlayerPosition_;
        searchTimer_ = kMovingSearchDuration;
        alertTimer_ = kLostSightAlertDur;
        detectionMeter_ = kLostSightMeterRetain;
        pathRepathTimer_ = 0.0f;
    }
    else
    {
        detectionMeter_ = (std::max)(0.0f, detectionMeter_ - kDetectionDecayRate * deltaTime);
    }

    const auto snapWalkable = [](const Vector3& p)
    {
        return GetNearestWalkablePos(p, kQueryWalkableRadius, kDefaultSearchRadius, &Enemy::IsWalkableArenaPos);
    };

    // 経路追従ヘルパー（A* NavMesh で障害物に引っかからず滑らかに前進）
    auto AdvanceAlongNavPath = [&](const Vector3& goalWorld, float speedMult)
    {
        pathRepathTimer_ -= deltaTime;
        if (pathRepathTimer_ <= 0.0f || cachedChasePath_.empty())
        {
            pathRepathTimer_ = kNavRepathInterval;
            cachedChasePath_ = BaziruEngine::AI::NavMesh::GetInstance()->FindPath(snapWalkable(position_), snapWalkable(goalWorld));
        }

        Vector3 nextPt = goalWorld;
        while (!cachedChasePath_.empty())
        {
            const Vector3 diff = cachedChasePath_.front() - position_;
            if (diff.x * diff.x + diff.z * diff.z < kWaypointReachRadiusSq)
            {
                cachedChasePath_.erase(cachedChasePath_.begin());
            }
            else
            {
                nextPt = cachedChasePath_.front();
                break;
            }
        }

        const float dx = nextPt.x - position_.x;
        const float dz = nextPt.z - position_.z;
        const float d  = std::sqrt(dx * dx + dz * dz);
        if (d > kMinNavStepDist)
        {
            const float stepLen = moveSpeed_ * speedMult * frameScale;
            const float stepX = (dx / d) * stepLen;
            const float stepZ = (dz / d) * stepLen;
            const Vector3 cand = { position_.x + stepX, 0.0f, position_.z + stepZ };
            if (IsWalkableArenaPos(cand))
            {
                position_ = cand;
            }
            else
            {
                const Vector3 candX = { position_.x + stepX, 0.0f, position_.z };
                const Vector3 candZ = { position_.x, 0.0f, position_.z + stepZ };
                if (IsWalkableArenaPos(candX)) position_ = candX;
                else if (IsWalkableArenaPos(candZ)) position_ = candZ;
            }
        }
    };

    // --- AI状態ごとの行動ロジック ---
    if (state_ == AIState::Patrol)
    {
        if (patrolPauseCooldown_ > 0.0f) patrolPauseCooldown_ = (std::max)(0.0f, patrolPauseCooldown_ - deltaTime);

        if (useSplinePatrol_ && splineFollower_)
        {
            if (returningToSpline_)
            {
                // 警戒・追跡から巡回へ戻る際、ワープせずNavMesh経路でスプライン復帰地点まで歩いて合流する
                const float rdx = splineRejoinPos_.x - position_.x;
                const float rdz = splineRejoinPos_.z - position_.z;
                const float rejoinDist = std::sqrt(rdx * rdx + rdz * rdz);
                const float directStep = moveSpeed_ * kRejoinSpeedMult * frameScale;
                if (rejoinDist <= directStep)
                {
                    if (IsWalkableArenaPos(splineRejoinPos_))
                    {
                        position_ = splineRejoinPos_;
                    }
                    returningToSpline_ = false;
                    splineFollower_->Resume();
                }
                else if (rejoinDist < kRejoinDirectDist)
                {
                    FaceTarget(splineRejoinPos_, deltaTime);
                    const Vector3 cand = {
                        position_.x + (rdx / rejoinDist) * directStep,
                        0.0f,
                        position_.z + (rdz / rejoinDist) * directStep
                    };
                    if (IsWalkableArenaPos(cand)) position_ = cand;
                    else AdvanceAlongNavPath(splineRejoinPos_, kRejoinSpeedMult);
                }
                else
                {
                    FaceTarget(splineRejoinPos_, deltaTime);
                    AdvanceAlongNavPath(splineRejoinPos_, kRejoinSpeedMult);
                }
            }
            else if (patrolPauseTimer_ > 0.0f)
            {
                patrolPauseTimer_ -= deltaTime;
                scanTime_ += deltaTime;
                const float lookSweep = currentYaw_ + std::sin(scanTime_ * kPauseSweepFreq) * kPauseSweepAmp;
                FaceTarget({ position_.x + std::sin(lookSweep) * 2.0f, 0.0f, position_.z + std::cos(lookSweep) * 2.0f }, deltaTime);
                if (patrolPauseTimer_ <= 0.0f)
                {
                    splineFollower_->Resume();
                }
            }
            else
            {
                if (!splineFollower_->IsPlaying()) splineFollower_->Resume();
                splineFollower_->Update(deltaTime);
                const Vector3 spPos = splineFollower_->GetPosition();
                const float sdx = spPos.x - position_.x;
                const float sdz = spPos.z - position_.z;
                if (sdx * sdx + sdz * sdz > kSplineDriftMaxDistSq)
                {
                    // ノックバックや押し出し等でスプラインから大きく離れた場合も瞬間移動せず歩いて復帰
                    splineRejoinPos_ = snapWalkable(spPos);
                    returningToSpline_ = true;
                    pathRepathTimer_ = 0.0f;
                    cachedChasePath_.clear();
                    splineFollower_->Pause();
                }
                else if (IsWalkableArenaPos(spPos))
                {
                    position_ = spPos;
                }

                const Vector3 tangent = splineFollower_->GetTangent();
                if (!returningToSpline_ && (std::abs(tangent.x) > kMinSightDistEps || std::abs(tangent.z) > kMinSightDistEps))
                {
                    FaceTarget({ position_.x + tangent.x, 0.0f, position_.z + tangent.z }, deltaTime);
                }

                if (!returningToSpline_ && splinePath_ && splineFollower_->GetPlayMode() == BaziruEngine::SplinePlayMode::PingPong && patrolPauseCooldown_ <= 0.0f)
                {
                    const float totalLen = splinePath_->GetTotalLength();
                    const float curDist = splineFollower_->GetCurrentDistance();
                    if (totalLen > kEndpointMinSplineLen && (curDist < kEndpointDistMargin || curDist > totalLen - kEndpointDistMargin))
                    {
                        patrolPauseTimer_ = kEndpointPauseDuration;
                        patrolPauseCooldown_ = kEndpointPauseCooldown;
                        splineFollower_->Pause();
                    }
                }
            }
        }
        else
        {
            Vector3 dest = movingToB_ ? patrolB_ : patrolA_;
            const float dx = dest.x - position_.x;
            const float dz = dest.z - position_.z;
            if (dx * dx + dz * dz < kSimpleDestReachDist * kSimpleDestReachDist)
            {
                movingToB_ = !movingToB_;
                dest = movingToB_ ? patrolB_ : patrolA_;
            }
            FaceTarget(dest, deltaTime);
            AdvanceAlongNavPath(dest, kSimplePatrolSpeedMult);
        }
    }
    else if (state_ == AIState::Investigate)
    {
        const float dx = investigateTarget_.x - position_.x;
        const float dz = investigateTarget_.z - position_.z;
        if (dx * dx + dz * dz > kInvestigateStopDist * kInvestigateStopDist)
        {
            FaceTarget(investigateTarget_, deltaTime);
            AdvanceAlongNavPath(investigateTarget_, 1.0f);
        }
        else
        {
            scanTime_ += deltaTime;
            const float phase = scanTime_ * kInvestigateScanFreq + scanPhase_;
            FaceTarget({ position_.x + std::cos(phase) * 2.0f, 0.0f, position_.z + std::sin(phase) * 2.0f }, deltaTime);
        }

        searchTimer_ -= deltaTime;
        if (searchTimer_ <= 0.0f)
        {
            state_ = AIState::Patrol;
            if (splineFollower_ && splinePath_ && splinePath_->GetTotalLength() > 0.0f)
            {
                const float totalLen = splinePath_->GetTotalLength();
                float bestDist = 0.0f;
                float minSqDist = kLargeInitDist;
                bool foundWalkable = false;
                for (float d = 0.0f; d <= totalLen; d += kSplineNearestSampleStep)
                {
                    const auto sample = splinePath_->EvaluateByDistance(d);
                    const bool walkable = IsWalkableArenaPos(sample.position);
                    if (foundWalkable && !walkable) continue;
                    const float diffX = sample.position.x - position_.x;
                    const float diffZ = sample.position.z - position_.z;
                    const float sq = diffX * diffX + diffZ * diffZ;
                    if ((walkable && !foundWalkable) || sq < minSqDist)
                    {
                        minSqDist = sq;
                        bestDist = d;
                        foundWalkable = walkable;
                    }
                }
                splineFollower_->SetDistance(bestDist);
                splineRejoinPos_ = foundWalkable ? splineFollower_->GetPosition() : snapWalkable(splineFollower_->GetPosition());
                const float rdx = splineRejoinPos_.x - position_.x;
                const float rdz = splineRejoinPos_.z - position_.z;
                const float directStep = moveSpeed_ * kRejoinSpeedMult * frameScale;
                if (rdx * rdx + rdz * rdz > directStep * directStep)
                {
                    returningToSpline_ = true;
                    pathRepathTimer_ = 0.0f;
                    cachedChasePath_.clear();
                    splineFollower_->Pause();
                }
                else
                {
                    returningToSpline_ = false;
                    splineFollower_->Resume();
                }
            }
        }
    }
    else if (state_ == AIState::Chase && targetPosition)
    {
        FaceTarget(*targetPosition, deltaTime);

        if (alertTimer_ <= 0.0f && evadeTimer_ <= 0.0f)
        {
            if (!IsWalkableArenaPos(position_))
            {
                position_ = snapWalkable(position_);
            }

            const Vector3 toP = { targetPosition->x - position_.x, 0.0f, targetPosition->z - position_.z };
            const float len = (std::max)(kMinTargetDistClamp, std::sqrt(toP.x * toP.x + toP.z * toP.z));
            const Vector3 dirToP = { toP.x / len, 0.0f, toP.z / len };
            const Vector3 right  = { dirToP.z, 0.0f, -dirToP.x };

            const bool tryUseCover = (behaviorTree_ && useBehaviorTree_ && coverIgnoreTimer_ <= 0.0f &&
                                      (reloadTimer_ > 0.0f || hp_ <= kLowHpCoverThreshold));
            bool handledByCover = false;

            if (tryUseCover)
            {
                auto blackboard = behaviorTree_->GetBlackboard();
                blackboard->Set<Vector3>("AgentPosition", snapWalkable(position_));
                blackboard->Set<Vector3>("ThreatPosition", *targetPosition);
                const BaziruEngine::AI::BehaviorStatus status = behaviorTree_->Update();
                if (status == BaziruEngine::AI::BehaviorStatus::Success || status == BaziruEngine::AI::BehaviorStatus::Failure)
                {
                    if (behaviorTree_->GetRoot()) behaviorTree_->GetRoot()->Reset();
                }
                if (status != BaziruEngine::AI::BehaviorStatus::Failure && blackboard->Has("CoverPosition"))
                {
                    const Vector3 coverPos = blackboard->Get<Vector3>("CoverPosition");
                    if (IsWalkableArenaPos(coverPos))
                    {
                        AdvanceAlongNavPath(coverPos, kCoverAdvanceSpeedMult);
                        handledByCover = true;
                    }
                    else
                    {
                        coverIgnoreTimer_ = kInvalidCoverIgnoreDur;
                    }
                }
            }

            if (!handledByCover)
            {
                const bool isFlanker = (role_ == TacticalRole::Flanker);
                strafeTimer_ += deltaTime;
                const float switchPeriod = isFlanker ? kFlankerStrafePeriod : kPatrolStrafePeriod;
                if (strafeTimer_ >= switchPeriod)
                {
                    strafeTimer_ = 0.0f;
                    strafeSign_ = -strafeSign_;
                }

                const float maxOptDist = isFlanker ? kFlankerMaxOptDist : kPatrolMaxOptDist;
                const float minOptDist = isFlanker ? kFlankerMinOptDist : kPatrolMinOptDist;

                if (!canSeePlayer || distToPlayer > maxOptDist)
                {
                    const float lateralSpread = isFlanker ? kFlankerLateralSpread : kPatrolLateralSpread;
                    const float standOffDist  = isFlanker ? kFlankerStandOffDist  : kPatrolStandOffDist;
                    const float advanceMult   = isFlanker ? kFlankerAdvanceMult   : kPatrolAdvanceMult;
                    const Vector3 flankGoal = {
                        targetPosition->x + right.x * (strafeSign_ * lateralSpread) - dirToP.x * standOffDist,
                        0.0f,
                        targetPosition->z + right.z * (strafeSign_ * lateralSpread) - dirToP.z * standOffDist
                    };
                    AdvanceAlongNavPath(flankGoal, advanceMult);
                }
                else if (distToPlayer < minOptDist)
                {
                    const float stepSpd = moveSpeed_ * kRetreatSpeedMult * frameScale;
                    const Vector3 retreatDir = {
                        -dirToP.x * kRetreatBackWeight + right.x * strafeSign_ * kRetreatSideWeight,
                        0.0f,
                        -dirToP.z * kRetreatBackWeight + right.z * strafeSign_ * kRetreatSideWeight
                    };
                    const Vector3 cand = { position_.x + retreatDir.x * stepSpd, 0.0f, position_.z + retreatDir.z * stepSpd };
                    if (IsWalkableArenaPos(cand)) position_ = cand;
                    else strafeSign_ = -strafeSign_;
                }
                else
                {
                    const float strafeMult = (aimTelegraphTimer_ > 0.0f)
                        ? kAimingMoveStrafeMult
                        : (isFlanker ? kFlankerCircleMult : kPatrolCircleMult);
                    const float stepSpd = moveSpeed_ * strafeMult * frameScale;
                    const Vector3 cand = { position_.x + right.x * strafeSign_ * stepSpd, 0.0f, position_.z + right.z * strafeSign_ * stepSpd };
                    if (IsWalkableArenaPos(cand)) position_ = cand;
                    else strafeSign_ = -strafeSign_;
                }
            }
        }
    }

    FinalizeCommonFrameState(deltaTime, targetPosition, canSeePlayer);
}

std::unique_ptr<Bullet> MovingEnemy::TryShoot(const Vector3& targetPosition)
{
    return Enemy::TryShoot(targetPosition);
}

void MovingEnemy::Draw(const RenderContext& ctx)
{
    Enemy::Draw(ctx);
}

void MovingEnemy::OnHit(const Vector3& attackerPos)
{
    returningToSpline_ = false;
    if (splineFollower_ && state_ != AIState::Chase)
    {
        splineFollower_->Pause();
    }
    Enemy::OnHit(attackerPos);
}

void MovingEnemy::HearNoise(const Vector3& noisePosition)
{
    const AIState prevState = state_;
    Enemy::HearNoise(noisePosition);
    if (state_ == AIState::Investigate && prevState != AIState::Investigate)
    {
        returningToSpline_ = false;
        if (splineFollower_) splineFollower_->Pause();
        pathRepathTimer_ = 0.0f;
    }
}

void MovingEnemy::AlertEnemy(const Vector3& targetPos)
{
    if (isDead_) return;
    returningToSpline_ = false;
    if (splineFollower_) splineFollower_->Pause();
    Enemy::AlertEnemy(targetPos);
}

void MovingEnemy::Finalize()
{
    if (splineFollower_)
    {
        splineFollower_->Stop();
        splineFollower_.reset();
    }
    splinePath_.reset();
    Enemy::Finalize();
}
