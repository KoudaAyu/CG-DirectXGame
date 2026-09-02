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
#include <cmath>
#include <random>

namespace {
Vector3 GetNearestWalkablePosition(const Vector3& pos, float agentRadius, float searchRadiusMax = 6.0f)
{
    if (BaziruEngine::AI::NavMesh::GetInstance()->IsWalkable(pos, agentRadius))
    {
        return pos;
    }

    float step = 0.5f; // グリッド解像度
    float minDist = 999999.0f;
    Vector3 bestPos = pos;
    bool found = false;

    for (float r = step; r <= searchRadiusMax; r += step)
    {
        for (float dx = -r; dx <= r; dx += step)
        {
            for (float dz = -r; dz <= r; dz += step)
            {
                if (std::abs(dx) < r && std::abs(dz) < r) continue;

                Vector3 testPos = { pos.x + dx, pos.y, pos.z + dz };
                if (BaziruEngine::AI::NavMesh::GetInstance()->IsWalkable(testPos, agentRadius))
                {
                    float dist = std::sqrt(dx * dx + dz * dz);
                    if (dist < minDist)
                    {
                        minDist = dist;
                        bestPos = testPos;
                        found = true;
                    }
                }
            }
        }
        if (found)
        {
            break;
        }
    }

    return bestPos;
}
}

void MovingEnemy::Initialize(Object3dCom* object3dCom, Camera* camera)
{
    object3dCom_ = object3dCom;
    camera_ = camera;

    Object3d::ModelData model = Object3d::LoadObjFile("Resources", "player.obj");
    model.material.textureFilePath = "Resources/duck_enemy.png";

    defaultTextureIndex_ = TextureManager::GetInstance()->Load("Resources/duck_enemy.png");
    model.material.textureIndex = defaultTextureIndex_;

    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(object3dCom_, model);
    object3d_->SetCamera(camera_);

    // 初期配置 (パトロールA地点)
    position_ = patrolA_;
    object3d_->SetTranslate(position_);
    object3d_->SetScale({ 1.0f, 1.0f, 1.0f });
    object3d_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

    // コライダーの初期化と登録
    collider_ = std::make_unique<SphereCollider>(0.55f, &position_, CollisionAttribute::Enemy);
    collider_->SetPositionOffset({ 0.0f, 0.55f, 0.0f });
    CollisionManager::GetInstance()->RegisterCollider(collider_.get());

    // ナビメッシュグリッドを初期化時に一度だけ構築（毎フレームの全再構築による処理落ちを回避）
    BaziruEngine::AI::NavMesh::GetInstance()->BuildGrid(-20.0f, 20.0f, -5.0f, 45.0f, 0.5f, 0.85f);

    hp_ = maxHp_;
    isDead_ = false;
    justRespawned_ = false;
    respawnTimer_ = 0.0f;
    shotCooldownTimer_ = shotCooldown_;
    state_ = AIState::Patrol;
    movingToB_ = true;

    behaviorTree_ = std::make_unique<BaziruEngine::AI::BehaviorTree>();
    if (behaviorTree_->LoadFromJSON("test_cover_tree.json"))
    {
        OutputDebugStringA("[MovingEnemy] Successfully loaded test_cover_tree.json\n");
    }
    else
    {
        OutputDebugStringA("[MovingEnemy] Failed to load test_cover_tree.json\n");
    }
}

bool MovingEnemy::FaceTarget(const Vector3& targetPosition, float deltaTime)
{
    if (!object3d_)
    {
        return false;
    }

    const Vector3 enemyPos = GetPosition();
    const Vector3 toTarget = { targetPosition.x - enemyPos.x, 0.0f, targetPosition.z - enemyPos.z };
    const float lenSq = toTarget.x * toTarget.x + toTarget.z * toTarget.z;
    if (lenSq <= 1e-6f)
    {
        return false;
    }

    const float invLen = 1.0f / std::sqrt(lenSq);
    const float targetYaw = std::atan2(toTarget.x * invLen, toTarget.z * invLen);

    // Smooth rotation towards target yaw (max turn speed ~200 deg/sec)
    float currentYaw = object3d_->GetRotate().y;

    // Normalize currentYaw to [-PI, PI] to prevent precision issues
    while (currentYaw < -3.14159265f) currentYaw += 6.2831853f;
    while (currentYaw > 3.14159265f) currentYaw -= 6.2831853f;

    float diff = targetYaw - currentYaw;
    while (diff < -3.14159265f) diff += 6.2831853f;
    while (diff > 3.14159265f) diff -= 6.2831853f;

    // Tie-breaker for 180 degree turns to prevent visual jittering/oscillation
    if (std::abs(diff) > 3.14f)
    {
        diff = 3.14f; // Force clockwise rotation
    }

    float turnSpeed = 3.5f;
    if (state_ == AIState::Investigate || state_ == AIState::Chase)
    {
        turnSpeed = 12.0f; // 警戒時・追跡時はより高速に振り向く
    }
    float maxRotate = turnSpeed * deltaTime;
    if (std::abs(diff) > maxRotate)
    {
        diff = (diff > 0.0f) ? maxRotate : -maxRotate;
    }

    float newYaw = currentYaw + diff;
    while (newYaw < -3.14159265f) newYaw += 6.2831853f;
    while (newYaw > 3.14159265f) newYaw -= 6.2831853f;

    object3d_->SetRotate({ 0.0f, newYaw, 0.0f });
    return true;
}

void MovingEnemy::Update(WindowAPI* windowAPI, const Vector3* targetPosition, const std::vector<std::unique_ptr<Obstacle>>& obstacles, float deltaTime, bool isPlayerInCover)
{
    if (isDead_)
    {
        respawnTimer_ -= deltaTime;
        if (respawnTimer_ <= 0.0f)
        {
            isDead_ = false;
            justRespawned_ = true;
            hp_ = maxHp_;
            state_ = AIState::Patrol;
            movingToB_ = true;
            detectionMeter_ = 0.0f;
            alertTimer_ = 0.0f;
            if (object3d_)
            {
                position_ = patrolA_;
                object3d_->SetTranslate(position_);
                object3d_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
            }
        }

        if (hpBarBg_ && windowAPI)
        {
            hpBarBg_->SetSize({ 0.0f, 0.0f });
            hpBarBg_->Update();
        }
        if (hpBarFg_ && windowAPI)
        {
            hpBarFg_->SetSize({ 0.0f, 0.0f });
            hpBarFg_->Update();
        }
        if (alertBar_ && windowAPI)
        {
            alertBar_->SetSize({ 0.0f, 0.0f });
            alertBar_->Update();
        }
        if (alertDot_ && windowAPI)
        {
            alertDot_->SetSize({ 0.0f, 0.0f });
            alertDot_->Update();
        }

        if (object3d_)
        {
            object3d_->Update();
        }
        return;
    }

    if (!object3d_) return;

    Vector3 currentPos = GetPosition();
    const float frameScale = deltaTime * 60.0f;

    if (coverIgnoreTimer_ > 0.0f)
    {
        coverIgnoreTimer_ -= deltaTime;
        if (coverIgnoreTimer_ < 0.0f)
        {
            coverIgnoreTimer_ = 0.0f;
        }
    }

    // --- 索敵 ＆ 視界チェック ---
    bool canSeePlayer = false;
    if (targetPosition)
    {
        Vector3 toPlayer = { targetPosition->x - currentPos.x, 0.0f, targetPosition->z - currentPos.z };
        float dist = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);

        // プレイヤーが遮蔽中の場合は視認距離が55%に半減（ステルスボーナス）
        float effectiveSight = isPlayerInCover ? (maxSightRange_ * 0.55f) : maxSightRange_;

        if (dist <= effectiveSight)
        {
            float yaw = object3d_->GetRotate().y;
            Vector3 forward = { std::sin(yaw), 0.0f, std::cos(yaw) };
            Vector3 toPlayerNorm = { toPlayer.x / dist, 0.0f, toPlayer.z / dist };

            float dot = forward.x * toPlayerNorm.x + forward.z * toPlayerNorm.z;
            float angle = std::acos((std::max)(-1.0f, (std::min)(1.0f, dot)));

            if (angle <= fovAngle_ * 0.5f)
            {
                // 壁・コンテナの堅牢マルチポイント遮蔽判定
                if (HasLineOfSight(*targetPosition, obstacles))
                {
                    canSeePlayer = true;
                }
            }
        }
    }

    // 索敵メーターの更新 (遮蔽中は緩やかに発見、逃げ込む猶予を確保)
    if (canSeePlayer)
    {
        float detectSpeed = isPlayerInCover ? 0.60f : 0.85f;
        detectionMeter_ += detectSpeed * deltaTime;
        if (detectionMeter_ >= 1.0f)
        {
            detectionMeter_ = 1.0f;
            if (state_ != AIState::Chase)
            {
                state_ = AIState::Chase;
                alertTimer_ = 1.0f; // 「！」マーク表示タイマー開始
                object3d_->SetColor({ 1.0f, 0.9f, 0.6f, 1.0f }); // 発見時に警戒色
            }
        }

        if (state_ == AIState::Chase && targetPosition)
        {
            lastSeenPlayerPosition_ = *targetPosition;
        }
    }
    else
    {
        if (state_ == AIState::Chase)
        {
            state_ = AIState::Investigate;
            investigateTarget_ = lastSeenPlayerPosition_;
            searchTimer_ = 4.0f; // 4秒間捜索
            alertTimer_ = 1.0f;  // 「？」マーク
            detectionMeter_ = 0.5f;
            object3d_->SetColor({ 0.9f, 0.9f, 0.7f, 1.0f }); // 捜索用カラーに変更
        }
        else
        {
            detectionMeter_ -= 0.5f * deltaTime; // 緩やかに見失う
            if (detectionMeter_ < 0.0f)
            {
                detectionMeter_ = 0.0f;
            }
        }
    }

    // --- AI状態ごとの行動ロジック ---
    if (state_ == AIState::Patrol)
    {
        if (hitFlashTimer_ <= 0.0f) object3d_->SetColor({ 1.2f, 0.4f, 0.4f, 1.0f });
        // パトロール移動
        Vector3 dest = movingToB_ ? patrolB_ : patrolA_;
        float dx = dest.x - currentPos.x;
        float dz = dest.z - currentPos.z;
        float distToDest = std::sqrt(dx * dx + dz * dz);

        if (distToDest < 0.2f)
        {
            movingToB_ = !movingToB_;
            dest = movingToB_ ? patrolB_ : patrolA_;
            dx = dest.x - currentPos.x;
            dz = dest.z - currentPos.z;
            distToDest = std::sqrt(dx * dx + dz * dz);
        }

        FaceTarget(dest, deltaTime);

        if (distToDest > 0.0f)
        {
            float vx = (dx / distToDest) * moveSpeed_ * frameScale;
            float vz = (dz / distToDest) * moveSpeed_ * frameScale;
            position_.x += vx;
            position_.z += vz;
            object3d_->SetTranslate(position_);
        }
    }
    else if (state_ == AIState::Investigate)
    {
        if (hitFlashTimer_ <= 0.0f) object3d_->SetColor({ 1.2f, 0.8f, 0.4f, 1.0f });
        // 音源の捜索: 音がした方向へ移動して捜索
        float dx = investigateTarget_.x - currentPos.x;
        float dz = investigateTarget_.z - currentPos.z;
        float distToDest = std::sqrt(dx * dx + dz * dz);

        FaceTarget(investigateTarget_, deltaTime);

        if (distToDest > 0.3f)
        {
            float vx = (dx / distToDest) * moveSpeed_ * 1.1f * frameScale; // 捜索はパトロールより少し早歩き
            float vz = (dz / distToDest) * moveSpeed_ * 1.1f * frameScale;
            position_.x += vx;
            position_.z += vz;
            object3d_->SetTranslate(position_);
        }

        searchTimer_ -= deltaTime;
        if (searchTimer_ <= 0.0f)
        {
            state_ = AIState::Patrol; // パトロールへ戻る
            object3d_->SetColor({ 1.2f, 0.4f, 0.4f, 1.0f });
        }
    }
    else if (state_ == AIState::Chase)
    {
        if (hitFlashTimer_ <= 0.0f) object3d_->SetColor({ 1.5f, 0.2f, 0.2f, 1.0f });
        if (targetPosition)
        {
            FaceTarget(*targetPosition, deltaTime);

            // 驚きフリーズ中（!マーク表示中）は移動しない
            if (alertTimer_ <= 0.0f)
            {
                // NavMeshクランプ：敵が障害物の通行不可領域に入り込んでいる場合、最も近い安全な場所へ押し戻す
                if (!BaziruEngine::AI::NavMesh::GetInstance()->IsWalkable(position_, 0.85f))
                {
                    position_ = GetNearestWalkablePosition(position_, 0.85f);
                    object3d_->SetTranslate(position_);
                }

                if (behaviorTree_ && useBehaviorTree_)
                {
                    // Blackboard状態の同期
                    auto blackboard = behaviorTree_->GetBlackboard();

                    if (coverIgnoreTimer_ > 0.0f)
                    {
                        blackboard->Clear("CoverPosition");
                        blackboard->Clear("CoverPath");

                        // A* 経路探索で障害物を賢く迂回しながらプレイヤーを追尾（フォールバック）
                        Vector3 walkableStart = GetNearestWalkablePosition(position_, 0.85f);
                        Vector3 walkableTarget = GetNearestWalkablePosition(*targetPosition, 0.85f);
                        std::vector<Vector3> path = BaziruEngine::AI::NavMesh::GetInstance()->FindPath(walkableStart, walkableTarget);

                        if (!path.empty() && !(path.front().x == position_.x && path.front().y == position_.y && path.front().z == position_.z))
                        {
                            path.insert(path.begin(), position_);
                        }

                        float dxToPlayer = targetPosition->x - position_.x;
                        float dzToPlayer = targetPosition->z - position_.z;
                        float distToPlayer = std::sqrt(dxToPlayer * dxToPlayer + dzToPlayer * dzToPlayer);

                        // プレイヤーと一定距離（3.5m）を保つように移動（視界が通らない場合はさらに接近）
                        if ((distToPlayer > 3.5f || !HasLineOfSight(*targetPosition, obstacles)) && !path.empty())
                        {
                            Vector3 nextPt = path.front();
                            if (path.size() > 1)
                            {
                                Vector3 diff = nextPt - position_;
                                if (std::sqrt(diff.x * diff.x + diff.z * diff.z) < 0.4f)
                                {
                                    nextPt = path[1];
                                }
                            }

                            float dx = nextPt.x - position_.x;
                            float dz = nextPt.z - position_.z;
                            float dist = std::sqrt(dx * dx + dz * dz);
                            if (dist > 0.02f)
                            {
                                float vx = (dx / dist) * moveSpeed_ * 0.9f * frameScale;
                                float vz = (dz / dist) * moveSpeed_ * 0.9f * frameScale;
                                position_.x += vx;
                                position_.z += vz;
                                object3d_->SetTranslate(position_);
                            }
                        }
                    }
                    else
                    {
                        // 1. カバー地点への到達判定
                    bool reachedCover = false;
                    Vector3 coverPos = { 0.0f, 0.0f, 0.0f };
                    if (blackboard->Has("CoverPosition"))
                    {
                        coverPos = blackboard->Get<Vector3>("CoverPosition");
                        Vector3 diff = coverPos - position_;
                        float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
                        if (isPeeking_)
                        {
                            diff = coverPos - actualCoverPos_;
                            dist = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
                        }
                        if (dist < 0.4f)
                        {
                            reachedCover = true;
                        }
                    }

                    // 2. ピーク＆射撃ステートマシンの更新
                    if (reachedCover)
                    {
                        if (!isPeeking_ && shotCooldownTimer_ <= 0.0f)
                        {
                            // 左右のピークポイントを計算
                            Vector3 toPlayer = *targetPosition - coverPos;
                            float len = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);
                            if (len > 0.01f)
                            {
                                Vector3 dirToPlayer = { toPlayer.x / len, 0.0f, toPlayer.z / len };
                                // 垂直方向のベクトル
                                Vector3 peekLeft = { -dirToPlayer.z, 0.0f, dirToPlayer.x };
                                Vector3 peekRight = { dirToPlayer.z, 0.0f, -dirToPlayer.x };
                                
                                Vector3 testLeft = coverPos + peekLeft * 1.5f;
                                Vector3 testRight = coverPos + peekRight * 1.5f;
                                
                                Vector3 chosenPeekOffset = { 0.0f, 0.0f, 0.0f };
                                bool hasPeekOffset = false;
                                
                                bool leftWalkable = BaziruEngine::AI::NavMesh::GetInstance()->IsWalkable(testLeft, 0.85f);
                                bool rightWalkable = BaziruEngine::AI::NavMesh::GetInstance()->IsWalkable(testRight, 0.85f);

                                if (HasLineOfSight(testLeft, obstacles) && leftWalkable)
                                {
                                    chosenPeekOffset = peekLeft * 1.5f;
                                    hasPeekOffset = true;
                                }
                                else if (HasLineOfSight(testRight, obstacles) && rightWalkable)
                                {
                                    chosenPeekOffset = peekRight * 1.5f;
                                    hasPeekOffset = true;
                                }
                                
                                if (hasPeekOffset)
                                {
                                    activePeekPos_ = coverPos + chosenPeekOffset;
                                    actualCoverPos_ = coverPos;
                                    isPeeking_ = true;
                                    peekTimer_ = 0.8f; // ピーク動作継続時間
                                }
                                else
                                {
                                    // 左右どちらのピークも障害物に遮られるか侵入不可の場合、このカバー地点を放棄してプレイヤーを一定時間追いかける
                                    coverIgnoreTimer_ = 4.0f;
                                    blackboard->Clear("CoverPosition");
                                    blackboard->Clear("CoverPath");
                                }
                            }
                        }
                    }

                    if (isPeeking_)
                    {
                        // ピーク中の移動と反転戻り処理
                        peekTimer_ -= deltaTime;
                        Vector3 targetPeekPos = (peekTimer_ > 0.4f) ? activePeekPos_ : actualCoverPos_;
                        
                        Vector3 toTargetPeek = targetPeekPos - position_;
                        float distToTargetPeek = std::sqrt(toTargetPeek.x * toTargetPeek.x + toTargetPeek.z * toTargetPeek.z);
                        
                        if (distToTargetPeek > 0.05f)
                        {
                            float moveStep = 6.0f * deltaTime;
                            if (moveStep >= distToTargetPeek) position_ = targetPeekPos;
                            else
                            {
                                Vector3 dirToTargetPeek = { toTargetPeek.x / distToTargetPeek, 0.0f, toTargetPeek.z / distToTargetPeek };
                                position_.x += dirToTargetPeek.x * moveStep;
                                position_.z += dirToTargetPeek.z * moveStep;
                            }
                        }
                        else if (peekTimer_ <= 0.4f)
                        {
                            // カバー位置に戻り終わったらピーク終了
                            isPeeking_ = false;
                            position_ = actualCoverPos_;
                        }
                        
                        FaceTarget(*targetPosition, deltaTime);
                        object3d_->SetTranslate(position_);
                        // 黒板の位置も同期
                        blackboard->Set<Vector3>("AgentPosition", position_);
                        
                        if (peekTimer_ <= 0.0f)
                        {
                            isPeeking_ = false;
                            position_ = actualCoverPos_;
                            blackboard->Set<Vector3>("AgentPosition", position_);
                        }
                    }
                    else
                    {
                        // 通常のビヘイビアツリー更新
                        bool hasPath = false;
                        if (blackboard->Has("CoverPath"))
                        {
                            auto path = blackboard->Get<std::vector<Vector3>>("CoverPath");
                            if (!path.empty())
                            {
                                hasPath = true;
                            }
                        }

                        if (!hasPath)
                        {
                            // 経路がないときは、スタート位置を最も近い歩行可能な安全位置にしてパスを計算させる
                            Vector3 walkableStart = GetNearestWalkablePosition(position_, 0.85f);
                            blackboard->Set<Vector3>("AgentPosition", walkableStart);
                        }
                        else
                        {
                            // 経路がある（FollowPathNode移動中）ときは、現在の実際の位置を同期
                            blackboard->Set<Vector3>("AgentPosition", position_);
                        }

                        blackboard->Set<Vector3>("ThreatPosition", *targetPosition);

                        BaziruEngine::AI::BehaviorStatus status = behaviorTree_->Update();

                        // エンジン側の自動リセットがオミットされているため、ツリーが完了ステータスを返した場合はアプリケーション側で明示的にルートノードをリセットする
                        if (status == BaziruEngine::AI::BehaviorStatus::Success || status == BaziruEngine::AI::BehaviorStatus::Failure)
                        {
                            if (behaviorTree_->GetRoot())
                            {
                                behaviorTree_->GetRoot()->Reset();
                            }
                        }

                        // 新たにパスが生成された直後のフレームで、実際の現在地（障害物にめり込んでいる可能性がある座標）をパスの先頭に差し込み、そこから移動を開始させる
                        if (blackboard->Has("CoverPath"))
                        {
                            auto path = blackboard->Get<std::vector<Vector3>>("CoverPath");
                            if (!path.empty() && !(path.front().x == position_.x && path.front().y == position_.y && path.front().z == position_.z))
                            {
                                path.insert(path.begin(), position_);
                                blackboard->Set<std::vector<Vector3>>("CoverPath", path);
                            }
                        }

                        // 計算されたカバー位置が進入可能（Walkability）か検証
                        if (status == BaziruEngine::AI::BehaviorStatus::Success)
                        {
                            if (blackboard->Has("CoverPosition"))
                            {
                                Vector3 coverPos = blackboard->Get<Vector3>("CoverPosition");
                                if (!BaziruEngine::AI::NavMesh::GetInstance()->IsWalkable(coverPos, 0.85f))
                                {
                                    // 計算されたカバー位置が障害物と重なっている場合は、無効（Failure）として扱いフォールバック追従へ移行させる
                                    status = BaziruEngine::AI::BehaviorStatus::Failure;
                                }
                            }
                        }

                        if (status == BaziruEngine::AI::BehaviorStatus::Failure)
                        {
                            // A* 経路探索で障害物を賢く迂回しながらプレイヤーを追尾（フォールバック）
                            Vector3 walkableStart = GetNearestWalkablePosition(position_, 0.85f);
                            Vector3 walkableTarget = GetNearestWalkablePosition(*targetPosition, 0.85f);
                            std::vector<Vector3> path = BaziruEngine::AI::NavMesh::GetInstance()->FindPath(walkableStart, walkableTarget);

                            if (!path.empty() && !(path.front().x == position_.x && path.front().y == position_.y && path.front().z == position_.z))
                            {
                                path.insert(path.begin(), position_);
                            }

                            float dxToPlayer = targetPosition->x - position_.x;
                            float dzToPlayer = targetPosition->z - position_.z;
                            float distToPlayer = std::sqrt(dxToPlayer * dxToPlayer + dzToPlayer * dzToPlayer);

                            // プレイヤーと一定距離（3.5m）を保つように移動（視界が通らない場合はさらに接近）
                            if ((distToPlayer > 3.5f || !HasLineOfSight(*targetPosition, obstacles)) && !path.empty())
                            {
                                Vector3 nextPt = path.front();
                                if (path.size() > 1)
                                {
                                    Vector3 diff = nextPt - position_;
                                    if (std::sqrt(diff.x * diff.x + diff.z * diff.z) < 0.4f)
                                    {
                                        nextPt = path[1];
                                    }
                                }

                                float dx = nextPt.x - position_.x;
                                float dz = nextPt.z - position_.z;
                                float dist = std::sqrt(dx * dx + dz * dz);
                                if (dist > 0.02f)
                                {
                                    float vx = (dx / dist) * moveSpeed_ * 0.9f * frameScale;
                                    float vz = (dz / dist) * moveSpeed_ * 0.9f * frameScale;
                                    position_.x += vx;
                                    position_.z += vz;
                                    object3d_->SetTranslate(position_);
                                }
                            }
                        }
                        else
                        {
                            position_ = blackboard->Get<Vector3>("AgentPosition");
                            object3d_->SetTranslate(position_);
                        }
                    }
                    } // カバー無視タイマー用 else の閉じカッコ
                }
                else
                {
                    float dx = targetPosition->x - currentPos.x;
                    float dz = targetPosition->z - currentPos.z;
                    float distToPlayer = std::sqrt(dx * dx + dz * dz);
                    // プレイヤーと一定距離を保ちながら追跡（視界が通らない場合はさらに接近）
                    if (distToPlayer > 3.5f || !HasLineOfSight(*targetPosition, obstacles))
                    {
                        float vx = (dx / distToPlayer) * moveSpeed_ * 0.9f * frameScale; // プレイヤーが逃げ切れるように追跡速度を少し落とす (0.9f)
                        float vz = (dz / distToPlayer) * moveSpeed_ * 0.9f * frameScale;
                        position_.x += vx;
                        position_.z += vz;
                        object3d_->SetTranslate(position_);
                    }
                }
            }
        }
    }

    // 射撃クールダウン
    if (shotCooldownTimer_ > 0.0f)
    {
        shotCooldownTimer_ -= deltaTime;
        if (shotCooldownTimer_ < 0.0f)
        {
            shotCooldownTimer_ = 0.0f;
        }
    }

    // 被弾フラッシュの復帰
    if (hitFlashTimer_ > 0.0f)
    {
        hitFlashTimer_ -= deltaTime;
        if (hitFlashTimer_ <= 0.0f)
        {
            hitFlashTimer_ = 0.0f;
            if (state_ == AIState::Chase)
            {
                object3d_->SetColor({ 1.5f, 0.2f, 0.2f, 1.0f });
            }
            else
            {
                object3d_->SetColor({ 1.2f, 0.4f, 0.4f, 1.0f });
            }
        }
    }

    // アラートポップアップタイマーの更新
    if (alertTimer_ > 0.0f)
    {
        alertTimer_ -= deltaTime;
        if (alertTimer_ < 0.0f)
        {
            alertTimer_ = 0.0f;
        }
    }

    object3d_->SetTranslate(position_);
    object3d_->Update();

    // HPバーの座標・サイズ更新
    if (camera_ && hpBarBg_ && hpBarFg_ && windowAPI)
    {
        Vector3 enemyPos = GetPosition();
        Vector3 barPos3D = enemyPos;
        barPos3D.y += 1.5f;

        const Matrix4x4& vp = camera_->GetViewProjectionMatrix();
        float x = barPos3D.x * vp.m[0][0] + barPos3D.y * vp.m[1][0] + barPos3D.z * vp.m[2][0] + vp.m[3][0];
        float y = barPos3D.x * vp.m[0][1] + barPos3D.y * vp.m[1][1] + barPos3D.z * vp.m[2][1] + vp.m[3][1];
        float z = barPos3D.x * vp.m[0][2] + barPos3D.y * vp.m[1][2] + barPos3D.z * vp.m[2][2] + vp.m[3][2];
        float w = barPos3D.x * vp.m[0][3] + barPos3D.y * vp.m[1][3] + barPos3D.z * vp.m[2][3] + vp.m[3][3];

        if (w > 0.0f)
        {
            x /= w;
            y /= w;

            float width = static_cast<float>(windowAPI->GetClientWidth());
            float height = static_cast<float>(windowAPI->GetClientHeight());

            float screenX = (x + 1.0f) * 0.5f * width;
            float screenY = (1.0f - y) * 0.5f * height;

            float bgWidth = 80.0f;
            float bgHeight = 8.0f;

            float hpRatio = static_cast<float>(hp_) / static_cast<float>(maxHp_);
            if (hpRatio < 0.0f) hpRatio = 0.0f;
            float fgWidth = bgWidth * hpRatio;

            // 背景バー
            hpBarBg_->SetPosition({ screenX - bgWidth * 0.5f, screenY });
            hpBarBg_->SetSize({ bgWidth, bgHeight });
            hpBarBg_->SetColor({ 0.1f, 0.1f, 0.1f, 1.0f });
            hpBarBg_->Update();

            // 前景バー (オレンジレッドで差別化)
            hpBarFg_->SetPosition({ screenX - bgWidth * 0.5f, screenY });
            hpBarFg_->SetSize({ fgWidth, bgHeight });
            hpBarFg_->SetColor({ 1.0f, 0.5f, 0.0f, 1.0f });
            hpBarFg_->Update();
        }
        else
        {
            hpBarBg_->SetSize({ 0.0f, 0.0f });
            hpBarBg_->Update();
            hpBarFg_->SetSize({ 0.0f, 0.0f });
            hpBarFg_->Update();
        }
    }

    // アラート「！」マークの表示・座標更新
    if (camera_ && alertBar_ && alertDot_ && windowAPI)
    {
        if (alertTimer_ > 0.0f && !isDead_)
        {
            Vector3 enemyPos = GetPosition();
            Vector3 alertPos3D = enemyPos;
            alertPos3D.y += 2.0f; // HPバーのさらに上

            const Matrix4x4& vp = camera_->GetViewProjectionMatrix();
            float x = alertPos3D.x * vp.m[0][0] + alertPos3D.y * vp.m[1][0] + alertPos3D.z * vp.m[2][0] + vp.m[3][0];
            float y = alertPos3D.x * vp.m[0][1] + alertPos3D.y * vp.m[1][1] + alertPos3D.z * vp.m[2][1] + vp.m[3][1];
            float z = alertPos3D.x * vp.m[0][2] + alertPos3D.y * vp.m[1][2] + alertPos3D.z * vp.m[2][2] + vp.m[3][2];
            float w = alertPos3D.x * vp.m[0][3] + alertPos3D.y * vp.m[1][3] + alertPos3D.z * vp.m[2][3] + vp.m[3][3];

            if (w > 0.0f)
            {
                x /= w;
                y /= w;

                float width = static_cast<float>(windowAPI->GetClientWidth());
                float height = static_cast<float>(windowAPI->GetClientHeight());

                float screenX = (x + 1.0f) * 0.5f * width;
                float screenY = (1.0f - y) * 0.5f * height;

                // びっくりマークの描画 (上部の太い棒と下部の点)
                // 棒 (alertBar_) Anchor={0.5f, 1.0f}
                alertBar_->SetPosition({ screenX, screenY - 5.0f });
                alertBar_->SetSize({ 6.0f, 16.0f });
                alertBar_->SetColor({ 1.0f, 0.15f, 0.15f, 1.0f }); // 警告の赤色
                alertBar_->Update();

                // 点 (alertDot_) Anchor={0.5f, 0.0f}
                alertDot_->SetPosition({ screenX, screenY });
                alertDot_->SetSize({ 6.0f, 6.0f });
                alertDot_->SetColor({ 1.0f, 0.15f, 0.15f, 1.0f });
                alertDot_->Update();
            }
            else
            {
                alertBar_->SetSize({ 0.0f, 0.0f });
                alertBar_->Update();
                alertDot_->SetSize({ 0.0f, 0.0f });
                alertDot_->Update();
            }
        }
        else
        {
            alertBar_->SetSize({ 0.0f, 0.0f });
            alertBar_->Update();
            alertDot_->SetSize({ 0.0f, 0.0f });
            alertDot_->Update();
        }
    }
}

std::unique_ptr<Bullet> MovingEnemy::TryShoot(const Vector3& targetPosition)
{
    if (isDead_ || !object3d_ || !object3dCom_ || !camera_ || shotCooldownTimer_ > 0.0f || state_ != AIState::Chase || alertTimer_ > 0.0f)
    {
        return nullptr;
    }

    // カバー中かつピークしていない間は射撃しない
    if (behaviorTree_ && useBehaviorTree_)
    {
        auto blackboard = behaviorTree_->GetBlackboard();
        if (blackboard->Has("CoverPosition"))
        {
            Vector3 coverPos = blackboard->Get<Vector3>("CoverPosition");
            Vector3 diff = coverPos - position_;
            float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
            if (dist < 0.4f && !isPeeking_)
            {
                return nullptr;
            }
        }
    }

    if (!FaceTarget(targetPosition))
    {
        return nullptr;
    }

    const Vector3 enemyPos = GetPosition();
    const Vector3 toTarget = { targetPosition.x - enemyPos.x, 0.0f, targetPosition.z - enemyPos.z };
    const float lenSq = toTarget.x * toTarget.x + toTarget.z * toTarget.z;
    const float invLen = 1.0f / std::sqrt(lenSq);
    const Vector3 forward = { toTarget.x * invLen, 0.0f, toTarget.z * invLen };
    const Vector3 spawnPos = Bullet::ComputeSpawnPosition(enemyPos, forward, bulletSpawnOffset_);

    auto bullet = std::make_unique<Bullet>();
    bullet->Initialize(object3dCom_, camera_, spawnPos, forward, bulletSpeed_, bulletLifeTime_, BulletOwner::Enemy);
    shotCooldownTimer_ = shotCooldown_;
    return bullet;
}

void MovingEnemy::Draw(const RenderContext& ctx)
{
    if (isDead_) return;
    if (!object3dCom_ || !object3d_) return;

    RenderContext enemyCtx = ctx;
    const Object3d::ModelData& modelData = object3d_->GetModelData();
    uint32_t texIdx = (defaultTextureIndex_ != TextureManager::kInvalidTextureIndex) ? defaultTextureIndex_ : modelData.material.textureIndex;
    if (texIdx != TextureManager::kInvalidTextureIndex)
    {
        enemyCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(texIdx);
    }

    // 被弾時のノックバック・震動シェイク演出の適用
    Vector3 originalPos = object3d_->GetTranslate();
    if (hitFlashTimer_ > 0.0f)
    {
        float shakeIntensity = 0.35f * (hitFlashTimer_ / hitFlashDuration_);
        float rx = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * shakeIntensity;
        float rz = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * shakeIntensity;
        object3d_->SetTranslate(originalPos + Vector3{ rx, 0.0f, rz });
        object3d_->Update(); // WVP行列を再計算してGPUに送るためUpdate
    }

    object3dCom_->Draw(object3d_.get(), enemyCtx, modelData, true);

    // 描画後は論理座標を元に戻して座標ドリフトを防ぐ
    if (hitFlashTimer_ > 0.0f)
    {
        object3d_->SetTranslate(originalPos);
        object3d_->Update();
    }
}

void MovingEnemy::OnHit(const Vector3& attackerPos)
{
    if (isDead_ || !object3d_)
    {
        return;
    }

    hp_--;
    if (hp_ <= 0)
    {
        hp_ = 0;
        isDead_ = true;
        justRespawned_ = false;
        respawnTimer_ = respawnDuration_;

        if (hpBarBg_) hpBarBg_->SetSize({ 0.0f, 0.0f });
        if (hpBarFg_) hpBarFg_->SetSize({ 0.0f, 0.0f });
        return;
    }

    hitFlashTimer_ = hitFlashDuration_;
    object3d_->SetColor({ 6.0f, 6.0f, 6.0f, 1.0f });

    // Attack reaction: turn around to search attacker's direction
    if (state_ != AIState::Chase)
    {
        state_ = AIState::Investigate;
        investigateTarget_ = attackerPos;
        searchTimer_ = 4.0f;
        alertTimer_ = 0.8f;
        detectionMeter_ = 0.8f; // Set alert to 80%
        object3d_->SetColor({ 0.9f, 0.9f, 0.7f, 1.0f }); // 捜索用カラーに変更
    }
}

void MovingEnemy::Finalize()
{
    if (collider_)
    {
        CollisionManager::GetInstance()->UnregisterCollider(collider_.get());
        collider_.reset();
    }
    if (object3d_)
    {
        object3d_.reset();
    }
}

bool MovingEnemy::HasLineOfSight(const Vector3& playerPos, const std::vector<std::unique_ptr<Obstacle>>& obstacles)
{
    (void)obstacles;
    Vector3 enemyEye = GetPosition() + Vector3{ 0.0f, 0.55f, 0.0f };

    // プレイヤーの頭(0.75m)、胴体(0.40m)、足元(0.15m)の3点判定
    const float yOffsets[3] = { 0.75f, 0.40f, 0.15f };
    int visiblePoints = 0;

    for (float yOff : yOffsets)
    {
        Vector3 targetPoint = { playerPos.x, playerPos.y + yOff, playerPos.z };
        Vector3 toTarget = { targetPoint.x - enemyEye.x, targetPoint.y - enemyEye.y, targetPoint.z - enemyEye.z };
        float dist = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y + toTarget.z * toTarget.z);
        if (dist < 1e-4f) { visiblePoints++; continue; }

        Vector3 dir = { toTarget.x / dist, toTarget.y / dist, toTarget.z / dist };
        Collider* hitCollider = nullptr;
        float hitDist = 0.0f;

        bool hitObstacle = false;
        if (CollisionManager::GetInstance()->Raycast(enemyEye, dir, dist, hitCollider, hitDist))
        {
            if (hitCollider && hitCollider->GetAttribute() == CollisionAttribute::Obstacle)
            {
                hitObstacle = true;
            }
        }

        if (!hitObstacle)
        {
            visiblePoints++;
        }
    }

    // 遮蔽物に隠れている場合は視線遮断
    return (visiblePoints >= 2);
}

void MovingEnemy::HearNoise(const Vector3& noisePosition)
{
    if (isDead_ || state_ == AIState::Chase) return;

    // 音源と敵の間に障害物があるかチェック（壁越しはノイズ遮断）
    Vector3 enemyPos = GetPosition() + Vector3{ 0.0f, 0.5f, 0.0f };
    Vector3 toNoise = { noisePosition.x - enemyPos.x, 0.0f, noisePosition.z - enemyPos.z };
    float dist = std::sqrt(toNoise.x * toNoise.x + toNoise.z * toNoise.z);
    if (dist > 1e-4f)
    {
        Vector3 dir = { toNoise.x / dist, 0.0f, toNoise.z / dist };
        Collider* hitCollider = nullptr;
        float hitDist = 0.0f;
        if (CollisionManager::GetInstance()->Raycast(enemyPos, dir, dist, hitCollider, hitDist))
        {
            if (hitCollider && hitCollider->GetAttribute() == CollisionAttribute::Obstacle)
            {
                return; // 壁越しの足音は遮断されて聞こえない
            }
        }
    }

    state_ = AIState::Investigate;
    investigateTarget_ = noisePosition;
    searchTimer_ = 4.0f; // 4秒間捜索
    alertTimer_ = 1.0f;  // 「？」マーク表示タイマー
    object3d_->SetColor({ 0.9f, 0.9f, 0.7f, 1.0f }); // 捜索用カラーに変更
}

void MovingEnemy::AlertEnemy(const Vector3& targetPos)
{
    if (isDead_) return;
    if (state_ != AIState::Chase)
    {
        state_ = AIState::Chase;
        alertTimer_ = 1.0f; // 「！」マーク表示タイマー
        detectionMeter_ = 1.0f;
        lastSeenPlayerPosition_ = targetPos;
        if (object3d_)
        {
            object3d_->SetColor({ 1.0f, 0.9f, 0.6f, 1.0f });
        }
    }
}
