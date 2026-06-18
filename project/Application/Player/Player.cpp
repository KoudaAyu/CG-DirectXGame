#include "Player.h"
#include "Object3d.h"
#include "Object3dCom.h"
#include "TextureManager.h"
#include "SceneManager.h"
#include "CustomObject3dRenderer.h"
#include "RenderContext.h"
#include "Bullet.h"
#include <Windows.h>
#include "MouseInput.h"
#include "Matrix4x4.h"
#include <cmath>

void Player::Initialize(Object3dCom* object3dCom, Camera* camera)
{
    object3dCom_ = object3dCom;
    camera_ = camera;

    // OBJ を読み込んで Object3d を初期化
    Object3d::ModelData model = Object3d::LoadObjFile("Resources", "plane.obj");
    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(object3dCom_, model);

    // デフォルトのトランスフォーム
    object3d_->SetTranslate({ 0.0f, 0.0f, 0.0f });
    object3d_->SetScale({ 1.0f, 1.0f, 1.0f });

    // テクスチャがない OBJ の場合、デフォルトテクスチャをバインドしておく
    // GPU-based validation でディスクリプタ未初期化エラーが出ないようにするため
    if (model.material.textureFilePath.empty())
    {
        defaultTextureIndex_ = TextureManager::GetInstance()->Load("Resources/uvChecker.png");
    }

    // プレイヤーのステータス初期化
    hp_ = maxHp_ = 100.0f;
    isDead_ = false;
    invincibilityTimer_ = 0.0f;

    // 弾薬の初期化
    magazineAmmo_ = maxMagazineAmmo_;
    isReloading_ = false;
    reloadTimer_ = 0.0f;

    // 回避の初期化
    isDodging_ = false;
    dodgeTimer_ = 0.0f;
    dodgeDirection_ = { 0.0f, 0.0f, 1.0f };

    // スタミナの初期化
    stamina_ = maxStamina_;
}

void Player::Update(float deltaTime, MouseInput* mouseInput)
{
    if (!object3d_) return;

    // 死亡時は入力を受け付けず、Updateのみ行って早期リターン
    if (isDead_)
    {
        object3d_->Update();
        return;
    }

    const float frameScale = deltaTime * 60.0f;

    // リロード処理の更新
    if (isReloading_)
    {
        reloadTimer_ -= deltaTime;
        if (reloadTimer_ <= 0.0f)
        {
            magazineAmmo_ = maxMagazineAmmo_;
            isReloading_ = false;
            reloadTimer_ = 0.0f;
        }
    }
    else
    {
        // Rキーでリロード開始
        if ((GetAsyncKeyState('R') & 0x8000) != 0 && magazineAmmo_ < maxMagazineAmmo_)
        {
            isReloading_ = true;
            reloadTimer_ = reloadDuration_;
        }
    }

    // 無敵時間タイマーと点滅処理の更新
    if (invincibilityTimer_ > 0.0f)
    {
        invincibilityTimer_ -= deltaTime;
        if (invincibilityTimer_ <= 0.0f)
        {
            invincibilityTimer_ = 0.0f;
            object3d_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        }
        else
        {
            // 5フレームごとに赤く点滅
            static int flashCount = 0;
            flashCount++;
            if ((flashCount / 5) % 2 == 0)
            {
                object3d_->SetColor({ 1.0f, 0.3f, 0.3f, 0.5f }); // 赤みがかった半透明
            }
            else
            {
                object3d_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
            }
        }
    }

    // スタミナの自動回復
    if (!isDodging_ && !isDead_)
    {
        stamina_ += staminaRegenRate_ * deltaTime;
        if (stamina_ > maxStamina_)
        {
            stamina_ = maxStamina_;
        }
    }

    bool isMoving = false;

    // 回避処理の更新
    if (isDodging_)
    {
        isMoving = true;
        dodgeTimer_ -= deltaTime;
        
        // 移動処理
        Vector3 pos = object3d_->GetTranslate();
        pos.x += dodgeDirection_.x * dodgeSpeed_ * frameScale;
        pos.z += dodgeDirection_.z * dodgeSpeed_ * frameScale;
        object3d_->SetTranslate(pos);

        // ビジュアル演出：X軸（前転方向）に1回転
        float ratio = dodgeTimer_ / dodgeDuration_;
        if (ratio < 0.0f) ratio = 0.0f;
        Vector3 rot = object3d_->GetRotate();
        rot.x = (1.0f - ratio) * 6.2831853f;
        object3d_->SetRotate(rot);

        if (dodgeTimer_ <= 0.0f)
        {
            isDodging_ = false;
            dodgeTimer_ = 0.0f;
            Vector3 finalRot = object3d_->GetRotate();
            finalRot.x = 0.0f;
            object3d_->SetRotate(finalRot);
        }
    }
    else
    {
        // WASD入力方向の計算
        Vector3 moveDir = { 0.0f, 0.0f, 0.0f };
        if ((GetAsyncKeyState('W') & 0x8000) != 0) moveDir.z += 1.0f;
        if ((GetAsyncKeyState('S') & 0x8000) != 0) moveDir.z -= 1.0f;
        if ((GetAsyncKeyState('A') & 0x8000) != 0) moveDir.x -= 1.0f;
        if ((GetAsyncKeyState('D') & 0x8000) != 0) moveDir.x += 1.0f;

        float len = std::sqrt(moveDir.x * moveDir.x + moveDir.z * moveDir.z);
        if (len > 0.0f)
        {
            isMoving = true;
            moveDir.x /= len;
            moveDir.z /= len;
        }
        // SPACEキーで回避開始（スタミナが必要分あるかチェック）
        if ((GetAsyncKeyState(VK_SPACE) & 0x8000) != 0 && stamina_ >= dodgeStaminaCost_)
        {
            isDodging_ = true;
            isMoving = true;
            dodgeTimer_ = dodgeDuration_;
            stamina_ -= dodgeStaminaCost_;

            // リロードのキャンセル
            if (isReloading_)
            {
                isReloading_ = false;
                reloadTimer_ = 0.0f;
            }

            if (len > 0.0f)
            {
                dodgeDirection_ = moveDir;
            }
            else
            {
                // 移動入力がない場合はプレイヤーの前方方向
                float yaw = GetRotation().y;
                dodgeDirection_ = { std::sin(yaw), 0.0f, std::cos(yaw) };
            }
        }
        else
        {
            // 通常のWASD移動処理
            const float kSpeed = 0.05f;
            Vector3 pos = object3d_->GetTranslate();

            if ((GetAsyncKeyState('W') & 0x8000) != 0)
            {
                pos.z += kSpeed * frameScale;
                isMoving = true;
            }
            if ((GetAsyncKeyState('S') & 0x8000) != 0)
            {
                pos.z -= kSpeed * frameScale;
                isMoving = true;
            }
            if ((GetAsyncKeyState('A') & 0x8000) != 0)
            {
                pos.x -= kSpeed * frameScale;
                isMoving = true;
            }
            if ((GetAsyncKeyState('D') & 0x8000) != 0)
            {
                pos.x += kSpeed * frameScale;
                isMoving = true;
            }
            object3d_->SetTranslate(pos);
        }
    }

    if (!isDodging_ && mouseInput && camera_)
    {
        WindowAPI* win = mouseInput->GetWindowAPI();
        if (win)
        {
            Vector2 mousePos = mouseInput->GetScaledPosition();
            float clientW = static_cast<float>(win->GetClientWidth());
            float clientH = static_cast<float>(win->GetClientHeight());
            if (clientW > 0.0f && clientH > 0.0f)
            {
                // NDC (正規化デバイス座標) の計算
                float nx = (mousePos.x / clientW) * 2.0f - 1.0f;
                float ny = 1.0f - (mousePos.y / clientH) * 2.0f;

                // ニアプレーンとファープレーンでのクリップ空間座標を用意 (z は [0,1])
                Vector4 clipNear = { nx, ny, 0.0f, 1.0f };
                Vector4 clipFar = { nx, ny, 1.0f, 1.0f };

                // Inverse of view * projection (matches WVP construction used elsewhere)
                Matrix4x4 inv = Inverse(Multiply(camera_->GetViewMatrix(), camera_->GetProjectionMatrix()));

                // このエンジンは行ベクトル方式（clip = pos * VP）なので
                // 逆変換は world = clip * VP^-1 → r = c * inv の順で乗算する
                auto transformClip = [&](const Vector4& c)->Vector3 {
                    Vector3 r;
                    r.x = c.x * inv.m[0][0] + c.y * inv.m[1][0] + c.z * inv.m[2][0] + c.w * inv.m[3][0];
                    r.y = c.x * inv.m[0][1] + c.y * inv.m[1][1] + c.z * inv.m[2][1] + c.w * inv.m[3][1];
                    r.z = c.x * inv.m[0][2] + c.y * inv.m[1][2] + c.z * inv.m[2][2] + c.w * inv.m[3][2];
                    float w = c.x * inv.m[0][3] + c.y * inv.m[1][3] + c.z * inv.m[2][3] + c.w * inv.m[3][3];
                    if (w != 0.0f)
                    {
                        r.x /= w; r.y /= w; r.z /= w;
                    }
                    return r;
                };

                Vector3 worldNear = transformClip(clipNear);
                Vector3 worldFar = transformClip(clipFar);

                // マウスを通過するカメラからのレイ
                Vector3 dir = { worldFar.x - worldNear.x, worldFar.y - worldNear.y, worldFar.z - worldNear.z };

                // 地面 (y = 0 平面) との交差判定
                if (std::fabs(dir.y) > 1e-6f)
                {
                    float t = -worldNear.y / dir.y;
                    if (t > 0.0f)
                    {
                        Vector3 hit = { worldNear.x + dir.x * t, 0.0f, worldNear.z + dir.z * t };
                        Vector3 ppos = object3d_->GetTranslate();
                        Vector3 to = { hit.x - ppos.x, 0.0f, hit.z - ppos.z };
                        // ヨーの計算。前方が +Z なので atan2(x, z) を使用
                        float yaw = std::atan2(to.x, to.z);
                        Vector3 r = object3d_->GetRotate();
                        r.y = yaw;
                        object3d_->SetRotate(r);
                    }
                }
            }
        }
    }

    if (camera_)
    {
        Vector3 playerPos = object3d_->GetTranslate();
        Vector3 cameraOffset = { 0.0f, 20.0f, -20.0f };
        camera_->SetTranslate(playerPos + cameraOffset);
    }

    UpdateSpread(deltaTime, isMoving);
    isMoving_ = isMoving;
    object3d_->Update();
}

std::unique_ptr<Bullet> Player::TryShoot(const MouseInput* mouseInput, float deltaTime)
{
    if (shotCooldownTimer_ > 0.0f)
    {
        shotCooldownTimer_ -= deltaTime;
        if (shotCooldownTimer_ < 0.0f)
        {
            shotCooldownTimer_ = 0.0f;
        }
    }

    if (isDead_ || isDodging_ || !object3d_ || !object3dCom_ || !camera_ || !mouseInput)
    {
        return nullptr;
    }

    // リロード中、または弾薬切れの場合は射撃不可
    if (isReloading_ || magazineAmmo_ <= 0)
    {
        return nullptr;
    }

    if (!mouseInput->PushButton(0) || shotCooldownTimer_ > 0.0f)
    {
        return nullptr;
    }

    const float yaw = GetRotation().y;
    
    // Add random spread angle to bullet direction
    float spreadOffset = 0.0f;
    if (currentSpread_ > 0.001f)
    {
        float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        spreadOffset = (r * 2.0f - 1.0f) * currentSpread_;
    }
    const float finalYaw = yaw + spreadOffset;
    const Vector3 forward = { std::sin(finalYaw), 0.0f, std::cos(finalYaw) };
    const Vector3 spawnPos = Bullet::ComputeSpawnPosition(GetPosition(), forward, bulletSpawnOffset_);

    auto bullet = std::make_unique<Bullet>();
    bullet->Initialize(object3dCom_, camera_, spawnPos, forward, bulletSpeed_, bulletLifeTime_, BulletOwner::Player);
    shotCooldownTimer_ = shotCooldown_;
    
    // 弾薬を減算
    magazineAmmo_--;

    // Apply shooting recoil spread penalty
    currentSpread_ += kShootSpreadPenalty;
    if (currentSpread_ > kMaxSpread)
    {
        currentSpread_ = kMaxSpread;
    }

    return bullet;
}

void Player::Draw(const RenderContext& ctx)
{
    if (!object3dCom_ || !object3d_) return;

    // ctx.textureHandle をモデルのテクスチャインデックスから設定する
    RenderContext playerCtx = ctx;
    const Object3d::ModelData& modelData = object3d_->GetModelData();
    uint32_t texIdx = modelData.material.textureIndex;
    // テクスチャが割り当てられていない場合はデフォルトを使う
    if (texIdx == 0 || texIdx == UINT32_MAX)
    {
        texIdx = defaultTextureIndex_;
    }
    if (playerCtx.textureHandle.ptr == 0 && texIdx != 0 && texIdx != UINT32_MAX)
    {
        playerCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(texIdx);
    }

    CustomObject3dRenderer::GetInstance()->Draw(object3d_.get(), playerCtx, modelData, true);
}

void Player::Finalize()
{
    if (object3d_)
    {
        object3d_.reset();
    }
}

void Player::TakeDamage(float damage)
{
    if (isDead_ || invincibilityTimer_ > 0.0f || isDodging_) return;

    hp_ -= damage;
    if (hp_ <= 0.0f)
    {
        hp_ = 0.0f;
        isDead_ = true;
        if (object3d_)
        {
            object3d_->SetColor({ 0.2f, 0.2f, 0.2f, 1.0f }); // 死亡時は暗い灰色にする
        }
    }
    else
    {
        invincibilityTimer_ = invincibilityDuration_;
    }
}

void Player::Reset()
{
    hp_ = maxHp_;
    isDead_ = false;
    invincibilityTimer_ = 0.0f;
    magazineAmmo_ = maxMagazineAmmo_;
    isReloading_ = false;
    reloadTimer_ = 0.0f;
    isDodging_ = false;
    dodgeTimer_ = 0.0f;
    dodgeDirection_ = { 0.0f, 0.0f, 1.0f };
    stamina_ = maxStamina_;
    currentSpread_ = 0.0f;
    isMoving_ = false;
    if (object3d_)
    {
        object3d_->SetTranslate({ 0.0f, 0.0f, 0.0f });
        object3d_->SetRotate({ 0.0f, 0.0f, 0.0f });
        object3d_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    }
}

void Player::UpdateSpread(float deltaTime, bool isMoving)
{
    float targetSpread = kBaseSpread;
    if (isMoving)
    {
        targetSpread += kMoveSpreadPenalty;
    }

    if (currentSpread_ > targetSpread)
    {
        currentSpread_ -= kSpreadRecoverRate * deltaTime;
        if (currentSpread_ < targetSpread)
        {
            currentSpread_ = targetSpread;
        }
    }
    else
    {
        currentSpread_ += kSpreadRecoverRate * 2.0f * deltaTime;
        if (currentSpread_ > targetSpread)
        {
            currentSpread_ = targetSpread;
        }
    }

    if (currentSpread_ > kMaxSpread)
    {
        currentSpread_ = kMaxSpread;
    }
}
