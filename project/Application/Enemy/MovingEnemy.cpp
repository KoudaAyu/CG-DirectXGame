#include "MovingEnemy.h"
#include "Object3d.h"
#include "Object3dCom.h"
#include "TextureManager.h"
#include "RenderContext.h"
#include "CustomObject3dRenderer.h"
#include "WindowsAPI.h"
#include "Sprite.h"
#include "Bullet.h"
#include <cmath>

void MovingEnemy::Initialize(Object3dCom* object3dCom, Camera* camera)
{
    object3dCom_ = object3dCom;
    camera_ = camera;

    Object3d::ModelData model = Object3d::LoadObjFile("Resources", "plane.obj");
    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(object3dCom_, model);

    // 初期配置 (パトロールA地点)
    object3d_->SetTranslate(patrolA_);
    object3d_->SetScale({ 1.0f, 1.0f, 1.0f });
    object3d_->SetColor({ 0.8f, 0.9f, 1.0f, 1.0f }); // わずかに青みがかったアヒルにして差別化

    if (model.material.textureFilePath.empty())
    {
        defaultTextureIndex_ = TextureManager::GetInstance()->Load("Resources/uvChecker.png");
    }

    hp_ = maxHp_;
    isDead_ = false;
    respawnTimer_ = 0.0f;
    shotCooldownTimer_ = shotCooldown_;
    state_ = AIState::Patrol;
    movingToB_ = true;
}

bool MovingEnemy::FaceTarget(const Vector3& targetPosition)
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
    const float yaw = std::atan2(toTarget.x * invLen, toTarget.z * invLen);
    object3d_->SetRotate({ 0.0f, yaw, 0.0f });
    return true;
}

void MovingEnemy::Update(WindowAPI* windowAPI, const Vector3* targetPosition, float deltaTime)
{
    if (isDead_)
    {
        respawnTimer_ -= deltaTime;
        if (respawnTimer_ <= 0.0f)
        {
            isDead_ = false;
            hp_ = maxHp_;
            state_ = AIState::Patrol;
            movingToB_ = true;
            if (object3d_)
            {
                object3d_->SetTranslate(patrolA_);
                object3d_->SetColor({ 0.8f, 0.9f, 1.0f, 1.0f });
            }
        }

        if (hpBarBg_ && windowAPI)
        {
            hpBarBg_->SetSize({ 0.0f, 0.0f });
            hpBarBg_->UpdateTransformOnly(windowAPI);
        }
        if (hpBarFg_ && windowAPI)
        {
            hpBarFg_->SetSize({ 0.0f, 0.0f });
            hpBarFg_->UpdateTransformOnly(windowAPI);
        }
        if (alertBar_ && windowAPI)
        {
            alertBar_->SetSize({ 0.0f, 0.0f });
            alertBar_->UpdateTransformOnly(windowAPI);
        }
        if (alertDot_ && windowAPI)
        {
            alertDot_->SetSize({ 0.0f, 0.0f });
            alertDot_->UpdateTransformOnly(windowAPI);
        }

        if (object3d_)
        {
            object3d_->Update();
        }
        return;
    }

    if (!object3d_) return;

    // AI 意思決定 & 移動処理
    Vector3 currentPos = GetPosition();
    const float frameScale = deltaTime * 60.0f;

    if (targetPosition)
    {
        // ターゲット（プレイヤー）との距離
        float dx = targetPosition->x - currentPos.x;
        float dz = targetPosition->z - currentPos.z;
        float distToPlayer = std::sqrt(dx * dx + dz * dz);

        // 状態遷移
        if (state_ == AIState::Patrol && distToPlayer < 7.5f)
        {
            state_ = AIState::Chase;
            // 発見時に警戒色（少し黄色みがかる）
            object3d_->SetColor({ 1.0f, 0.9f, 0.6f, 1.0f });
            alertTimer_ = 0.6f; // アラートポップアップタイマー起動
        }
        else if (state_ == AIState::Chase && distToPlayer > 11.5f)
        {
            state_ = AIState::Patrol;
            object3d_->SetColor({ 0.8f, 0.9f, 1.0f, 1.0f });
        }

        // 行動実行
        if (state_ == AIState::Chase)
        {
            FaceTarget(*targetPosition);

            // 驚きフリーズ中（!マーク表示中）は移動しない
            if (alertTimer_ <= 0.0f)
            {
                // プレイヤーと一定距離を保ちながら追跡
                if (distToPlayer > 4.5f)
                {
                    float vx = (dx / distToPlayer) * moveSpeed_ * 1.3f * frameScale; // 追跡時は少し速く
                    float vz = (dz / distToPlayer) * moveSpeed_ * 1.3f * frameScale;
                    currentPos.x += vx;
                    currentPos.z += vz;
                    object3d_->SetTranslate(currentPos);
                }
            }
        }
    }

    if (state_ == AIState::Patrol)
    {
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

        FaceTarget(dest);

        if (distToDest > 0.0f)
        {
            float vx = (dx / distToDest) * moveSpeed_ * frameScale;
            float vz = (dz / distToDest) * moveSpeed_ * frameScale;
            currentPos.x += vx;
            currentPos.z += vz;
            object3d_->SetTranslate(currentPos);
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
                object3d_->SetColor({ 1.0f, 0.9f, 0.6f, 1.0f });
            }
            else
            {
                object3d_->SetColor({ 0.8f, 0.9f, 1.0f, 1.0f });
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
            hpBarBg_->UpdateTransformOnly(windowAPI);

            // 前景バー (オレンジレッドで差別化)
            hpBarFg_->SetPosition({ screenX - bgWidth * 0.5f, screenY });
            hpBarFg_->SetSize({ fgWidth, bgHeight });
            hpBarFg_->SetColor({ 1.0f, 0.5f, 0.0f, 1.0f });
            hpBarFg_->UpdateTransformOnly(windowAPI);
        }
        else
        {
            hpBarBg_->SetSize({ 0.0f, 0.0f });
            hpBarBg_->UpdateTransformOnly(windowAPI);
            hpBarFg_->SetSize({ 0.0f, 0.0f });
            hpBarFg_->UpdateTransformOnly(windowAPI);
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
                alertBar_->UpdateTransformOnly(windowAPI);

                // 点 (alertDot_) Anchor={0.5f, 0.0f}
                alertDot_->SetPosition({ screenX, screenY });
                alertDot_->SetSize({ 6.0f, 6.0f });
                alertDot_->SetColor({ 1.0f, 0.15f, 0.15f, 1.0f });
                alertDot_->UpdateTransformOnly(windowAPI);
            }
            else
            {
                alertBar_->SetSize({ 0.0f, 0.0f });
                alertBar_->UpdateTransformOnly(windowAPI);
                alertDot_->SetSize({ 0.0f, 0.0f });
                alertDot_->UpdateTransformOnly(windowAPI);
            }
        }
        else
        {
            alertBar_->SetSize({ 0.0f, 0.0f });
            alertBar_->UpdateTransformOnly(windowAPI);
            alertDot_->SetSize({ 0.0f, 0.0f });
            alertDot_->UpdateTransformOnly(windowAPI);
        }
    }
}

std::unique_ptr<Bullet> MovingEnemy::TryShoot(const Vector3& targetPosition)
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

void MovingEnemy::Draw(const RenderContext& ctx)
{
    if (isDead_) return;
    if (!object3dCom_ || !object3d_) return;

    RenderContext enemyCtx = ctx;
    const Object3d::ModelData& modelData = object3d_->GetModelData();
    uint32_t texIdx = modelData.material.textureIndex;
    if (texIdx == 0 || texIdx == UINT32_MAX)
    {
        texIdx = defaultTextureIndex_;
    }
    if (enemyCtx.textureHandle.ptr == 0 && texIdx != 0 && texIdx != UINT32_MAX)
    {
        enemyCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(texIdx);
    }

    CustomObject3dRenderer::GetInstance()->Draw(object3d_.get(), enemyCtx, modelData, true);
}

void MovingEnemy::OnHit()
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
        respawnTimer_ = respawnDuration_;

        if (hpBarBg_) hpBarBg_->SetSize({ 0.0f, 0.0f });
        if (hpBarFg_) hpBarFg_->SetSize({ 0.0f, 0.0f });
        return;
    }

    hitFlashTimer_ = hitFlashDuration_;
    object3d_->SetColor({ 1.0f, 0.2f, 0.2f, 1.0f });
}

void MovingEnemy::Finalize()
{
    if (object3d_)
    {
        object3d_.reset();
    }
}
