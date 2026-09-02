#include "Enemy.h"
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

void Enemy::Initialize(Object3dCom* object3dCom, Camera* camera)
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

    position_ = { 4.0f, 0.0f, 13.0f };
    object3d_->SetTranslate(position_);
    object3d_->SetScale({ 1.0f, 1.0f, 1.0f });
    object3d_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

    // コライダーの初期化と登録
    collider_ = std::make_unique<SphereCollider>(0.55f, &position_, CollisionAttribute::Enemy);
    collider_->SetPositionOffset({ 0.0f, 0.55f, 0.0f });
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

void Enemy::Update(WindowAPI* windowAPI, const Vector3* targetPosition, const std::vector<std::unique_ptr<Obstacle>>& obstacles, float deltaTime, bool isPlayerInCover)
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
            SetPosition({ 4.0f, 0.0f, 13.0f });
            object3d_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
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

        // プレイヤーが遮蔽中の場合は視認距離が55%に半減（ステルスボーナス）
        float effectiveSight = isPlayerInCover ? (maxSightRange_ * 0.55f) : maxSightRange_;

        if (dist <= effectiveSight)
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
        if (CollisionManager::GetInstance()->Raycast(enemyEye, dir, dist, hitCollider, hitDist, static_cast<uint32_t>(CollisionAttribute::Obstacle)))
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

    // 遮蔽物に完全に隠れている（visiblePoints == 0）または大部分隠れている場合は視線遮断
    return (visiblePoints >= 2);
}

void Enemy::HearNoise(const Vector3& noisePosition)
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
    searchTimer_ = 3.0f; // 3秒間捜索
    alertTimer_ = 1.0f;  // 「？」マーク表示タイマー
}

void Enemy::AlertEnemy(const Vector3& targetPos)
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
