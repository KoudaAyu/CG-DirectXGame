#include "Enemy.h"
#include "Object3d.h"
#include "Object3dCom.h"
#include "TextureManager.h"
#include "RenderContext.h"

#include "WindowsAPI.h"
#include "Sprite.h"
#include "Bullet.h"
#include "Obstacle.h"
#include "Baziru3_Engine/Collision/CollisionManager.h"
#include "Baziru3_Engine/Collision/SphereCollider.h"
#include "Baziru3_Engine/Collision/BoxCollider.h"
#include "Baziru3_Engine/Collision/CapsuleCollider.h"
#include <cmath>
#include <random>

void Enemy::Initialize(Object3dCom* object3dCom, Camera* camera)
{
    object3dCom_ = object3dCom;
    camera_ = camera;

    Object3d::ModelData model = Object3d::LoadObjFile("Resources", "plane.obj");

    // 敵として分かりやすくするため、警告色である monsterBall.png を強制設定
    defaultTextureIndex_ = TextureManager::GetInstance()->Load("Resources/monsterBall.png");
    model.material.textureIndex = defaultTextureIndex_;

    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(object3dCom_, model);

    position_ = { 4.0f, 0.0f, 13.0f };
    object3d_->SetTranslate(position_);
    object3d_->SetScale({ 1.0f, 1.0f, 1.0f });
    object3d_->SetColor({ 1.2f, 0.4f, 0.4f, 1.0f });

    // コライダーの初期化と登録
    collider_ = std::make_unique<SphereCollider>(0.6f, &position_, CollisionAttribute::Enemy);
    CollisionManager::GetInstance()->RegisterCollider(collider_.get());

    hp_ = maxHp_;
    isDead_ = false;
    justRespawned_ = false;
    respawnTimer_ = 0.0f;
    shotCooldownTimer_ = shotCooldown_;
}

bool Enemy::FaceTarget(const Vector3& targetPosition, float deltaTime)
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

void Enemy::Update(WindowAPI* windowAPI, const Vector3* targetPosition, const std::vector<std::unique_ptr<Obstacle>>& obstacles, float deltaTime)
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
            detectionMeter_ = 0.0f;
            alertTimer_ = 0.0f;
            if (object3d_)
            {
                position_ = { 4.0f, 0.0f, 13.0f };
                object3d_->SetTranslate(position_);
                object3d_->SetColor({ 1.2f, 0.4f, 0.4f, 1.0f });
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

        if (object3d_)
        {
            object3d_->Update();
        }
        return;
    }

    if (!object3d_) return;

    // --- 索敵 ＆ 視界チェック ---
    bool canSeePlayer = false;
    if (targetPosition)
    {
        const Vector3 enemyPos = GetPosition();
        Vector3 toPlayer = { targetPosition->x - enemyPos.x, 0.0f, targetPosition->z - enemyPos.z };
        float dist = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);

        if (dist <= maxSightRange_)
        {
            // 視線の向きを計算（回転角度 object3d_->GetRotate().y から前方方向）
            float yaw = object3d_->GetRotate().y;
            Vector3 forward = { std::sin(yaw), 0.0f, std::cos(yaw) };
            
            // プレイヤー方向ベクトルの正規化
            Vector3 toPlayerNorm = { toPlayer.x / dist, 0.0f, toPlayer.z / dist };
            
            // 内積を計算
            float dot = forward.x * toPlayerNorm.x + forward.z * toPlayerNorm.z;
            float angle = std::acos((std::max)(-1.0f, (std::min)(1.0f, dot)));
            
            // 視界角の半分（fovAngle_ / 2.0f）の範囲内かチェック
            if (angle <= fovAngle_ * 0.5f)
            {
                // 壁の遮蔽判定
                if (HasLineOfSight(*targetPosition, obstacles))
                {
                    canSeePlayer = true;
                }
            }
        }
    }

    // 索敵メーターの更新
    if (canSeePlayer)
    {
        detectionMeter_ += 1.5f * deltaTime; // 約0.6秒で発見状態へ
        if (detectionMeter_ >= 1.0f)
        {
            detectionMeter_ = 1.0f;
            if (state_ != AIState::Chase)
            {
                state_ = AIState::Chase;
                alertTimer_ = 1.0f; // 「！」マーク表示タイマー開始
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
            searchTimer_ = 3.0f; // 3秒間捜索
            alertTimer_ = 1.0f;  // 「？」マーク
            detectionMeter_ = 0.5f;
        }
        else
        {
            detectionMeter_ -= 0.6f * deltaTime; // 緩やかに見失う
            if (detectionMeter_ < 0.0f)
            {
                detectionMeter_ = 0.0f;
            }
        }
    }

    // --- AI状態ごとの行動ロジック ---
    if (state_ == AIState::Patrol)
    {
        // 巡回中（定点）: ゆっくり左右に首を振る（視野スキャン）
        static float scanTime = 0.0f;
        scanTime += deltaTime;
        // 定点座標の向きから左右にスイング
        float targetYaw = std::sin(scanTime * 1.5f) * 0.6f;

        // スナップ（急激な角度変化）を防ぐため、スムーズに回転させる
        float currentYaw = object3d_->GetRotate().y;
        while (currentYaw < -3.14159265f) currentYaw += 6.2831853f;
        while (currentYaw > 3.14159265f) currentYaw -= 6.2831853f;

        float diff = targetYaw - currentYaw;
        while (diff < -3.14159265f) diff += 6.2831853f;
        while (diff > 3.14159265f) diff -= 6.2831853f;

        // 巡回中はゆっくり（速度3.0f）振り向かせる
        float turnSpeed = 3.0f;
        float maxRotate = turnSpeed * deltaTime;
        if (std::abs(diff) > maxRotate)
        {
            diff = (diff > 0.0f) ? maxRotate : -maxRotate;
        }

        float newYaw = currentYaw + diff;
        object3d_->SetRotate({ 0.0f, newYaw, 0.0f });
    }
    else if (state_ == AIState::Investigate)
    {
        // 音源の捜索: 音がした方向を向いて留まる
        FaceTarget(investigateTarget_, deltaTime);
        
        searchTimer_ -= deltaTime;
        if (searchTimer_ <= 0.0f)
        {
            state_ = AIState::Patrol; // 捜索終了、通常パトロールへ戻る
        }
    }
    else if (state_ == AIState::Chase)
    {
        // 戦闘状態: プレイヤーをロックオンして撃つ
        if (targetPosition)
        {
            FaceTarget(*targetPosition, deltaTime);
        }
    }

    // タイマーデクリメント
    if (shotCooldownTimer_ > 0.0f)
    {
        shotCooldownTimer_ -= deltaTime;
        if (shotCooldownTimer_ < 0.0f)
        {
            shotCooldownTimer_ = 0.0f;
        }
    }

    if (hitFlashTimer_ > 0.0f)
    {
        hitFlashTimer_ -= deltaTime;
        if (hitFlashTimer_ <= 0.0f)
        {
            hitFlashTimer_ = 0.0f;
            object3d_->SetColor({ 1.2f, 0.4f, 0.4f, 1.0f });
        }
    }

    if (alertTimer_ > 0.0f)
    {
        alertTimer_ -= deltaTime;
        if (alertTimer_ < 0.0f) alertTimer_ = 0.0f;
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

            // 背景バー (左端揃え)
            hpBarBg_->SetPosition({ screenX - bgWidth * 0.5f, screenY });
            hpBarBg_->SetSize({ bgWidth, bgHeight });
            hpBarBg_->SetColor({ 0.1f, 0.1f, 0.1f, 1.0f });
            hpBarBg_->Update();

            // 前景バー (左端揃えでゲージ変化)
            hpBarFg_->SetPosition({ screenX - bgWidth * 0.5f, screenY });
            hpBarFg_->SetSize({ fgWidth, bgHeight });
            hpBarFg_->SetColor({ 0.0f, 1.0f, 0.0f, 1.0f });
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
}

bool Enemy::HasLineOfSight(const Vector3& playerPos, const std::vector<std::unique_ptr<Obstacle>>& obstacles)
{
    Vector3 enemyPos = GetPosition();
    Vector3 toPlayer = { playerPos.x - enemyPos.x, playerPos.y - enemyPos.y, playerPos.z - enemyPos.z };
    float distToPlayer = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y + toPlayer.z * toPlayer.z);
    if (distToPlayer < 1e-4f) return true;

    Vector3 rayDir = { toPlayer.x / distToPlayer, toPlayer.y / distToPlayer, toPlayer.z / distToPlayer };

    for (const auto& obs : obstacles)
    {
        if (!obs || !obs->GetCollider()) continue;
        float hitDist = 0.0f;
        
        Collider* col = obs->GetCollider();
        CollisionData data;
        data.originalCollider = col;
        data.type = col->GetType();
        data.attribute = col->GetAttribute();
        data.worldPosition = col->GetWorldPosition();
        data.isTrigger = col->IsTrigger();

        if (data.type == ColliderType::Sphere)
        {
            SphereCollider* sphere = static_cast<SphereCollider*>(col);
            data.shape.radius = sphere->GetRadius();
        }
        else if (data.type == ColliderType::Box)
        {
            BoxCollider* box = static_cast<BoxCollider*>(col);
            data.shape.size = box->GetSize();
            data.shape.rotation = box->GetWorldRotation();
        }
        else if (data.type == ColliderType::Capsule)
        {
            CapsuleCollider* capsule = static_cast<CapsuleCollider*>(col);
            data.shape.radius = capsule->GetRadius();
            data.shape.height = capsule->GetHeight();
        }

        if (CollisionManager::CheckRayCollider(enemyPos, rayDir, distToPlayer, data, hitDist))
        {
            return false; // 障害物に遮蔽されている
        }
    }
    return true; // 視線が通っている
}

void Enemy::HearNoise(const Vector3& noisePosition)
{
    if (isDead_ || state_ == AIState::Chase) return;

    state_ = AIState::Investigate;
    investigateTarget_ = noisePosition;
    searchTimer_ = 3.0f; // 3秒間捜索
    alertTimer_ = 1.0f;  // 「？」マーク表示タイマー
}

std::unique_ptr<Bullet> Enemy::TryShoot(const Vector3& targetPosition)
{
    if (isDead_ || !object3d_ || !object3dCom_ || !camera_ || shotCooldownTimer_ > 0.0f || state_ != AIState::Chase || alertTimer_ > 0.0f)
    {
        return nullptr;
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

void Enemy::Draw(const RenderContext& ctx)
{
    if (isDead_) return;
    if (!object3dCom_ || !object3d_) return;

    RenderContext enemyCtx = ctx;
    const Object3d::ModelData& modelData = object3d_->GetModelData();
    uint32_t texIdx = defaultTextureIndex_;
    if (enemyCtx.textureHandle.ptr == 0 && texIdx != 0 && texIdx != UINT32_MAX)
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

void Enemy::OnHit(const Vector3& attackerPos)
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
        searchTimer_ = 3.5f;
        alertTimer_ = 0.8f;
        detectionMeter_ = 0.8f; // Set alert to 80%
    }
}

void Enemy::Finalize()
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
