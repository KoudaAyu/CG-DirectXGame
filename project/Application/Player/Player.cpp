#include "Player.h"
#include "Object3d.h"
#include "Object3dCom.h"
#include "TextureManager.h"
#include "SceneManager.h"
#include "Application/GameObject/Bullet.h"
#include "Baziru3_Engine/Core/IO/Mouse/MouseInput.h"
#include "Matrix4x4.h"
#include <Windows.h>
#include <cmath>
#include <algorithm>
#include "../Scene/GameScene/RaidStats.h"

// =============================================================================
// 初期化 & 解放 (Lifecycle)
// =============================================================================

void Player::Initialize(Object3dCom* object3dCom, Camera* camera)
{
    object3dCom_ = object3dCom;
    camera_ = camera;

    // OBJモデルの読み込みとObject3dコンポーネントの初期化
    Object3d::ModelData model = Object3d::LoadObjFile("Resources", "player.obj");
    if (model.material.textureFilePath.empty())
    {
        model.material.textureFilePath = "Resources/duck.png";
    }

    float maxDistSq = 0.0f;
    for (const auto& v : model.vertices)
    {
        float distSq = v.position.x * v.position.x + v.position.y * v.position.y + v.position.z * v.position.z;
        if (distSq > maxDistSq) maxDistSq = distSq;
    }
    model.boundingRadius = (std::max)(std::sqrt(maxDistSq), 4.0f);

    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(object3dCom_, model);
    object3d_->SetCamera(camera_);

    // 初期トランスフォームの設定
    position_ = { 0.0f, 0.0f, 0.0f };
    drawPos_ = position_;
    object3d_->SetTranslate(position_);
    object3d_->SetScale({ 1.0f, 1.0f, 1.0f });
    object3d_->SetRotate({ 0.0f, 0.0f, 0.0f });
    object3d_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

    // アヒルモデルの体型に合わせたスフィアコライダーの初期化と登録
    collider_ = std::make_unique<SphereCollider>(kColliderRadius, &position_, CollisionAttribute::Player);
    collider_->SetPositionOffset({ 0.0f, kColliderRadius, 0.0f });
    CollisionManager::GetInstance()->RegisterCollider(collider_.get());

    defaultTextureIndex_ = TextureManager::GetInstance()->Load("Resources/duck.png");

    // 全ステータスをデフォルト定数でリセット
    Reset();
}

void Player::Finalize()
{
    // コライダーの登録解除とメモリ解放
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

void Player::Reset()
{
    // 体力とステートの初期化
    hp_ = maxHp_ = kDefaultMaxHp;
    isDead_ = false;
    invincibilityTimer_ = 0.0f;
    hitFlashTimer_ = 0.0f;

    // 弾薬とリロードの初期化
    magazineAmmo_ = maxMagazineAmmo_ = kDefaultMaxMagazineAmmo;
    reserveAmmo_ = kDefaultReserveAmmo;
    isReloading_ = false;
    reloadTimer_ = 0.0f;
    reloadCancelledTimer_ = 0.0f;

    // 物資と治療の初期化
    medkitCount_ = kDefaultMedkitCount;
    lootValue_ = 0;
    isHealing_ = false;
    healTimer_ = 0.0f;

    // 回避とスタミナの初期化
    isDodging_ = false;
    dodgeTimer_ = 0.0f;
    dodgeDirection_ = { 0.0f, 0.0f, 1.0f };
    stamina_ = maxStamina_ = kDefaultMaxStamina;

    // 照準拡散とトランスフォームの初期化
    currentSpread_ = 0.0f;
    isMoving_ = false;
    isInCover_ = false;
    position_ = { 0.0f, 0.0f, 0.0f };
    drawPos_ = position_;

    if (object3d_)
    {
        object3d_->SetTranslate(position_);
        object3d_->SetRotate({ 0.0f, 0.0f, 0.0f });
        object3d_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    }
}

// =============================================================================
// メイン更新ループ (Update)
// =============================================================================

void Player::Update(float deltaTime, MouseInput* mouseInput)
{
    if (!object3d_) return;

    // 死亡時は入力を受け付けず、描画トランスフォームの更新のみ行って早期リターン
    if (isDead_)
    {
        object3d_->Update();
        return;
    }

    const float frameScale = deltaTime * 60.0f;

    // --- 1. リロードキャンセル通知タイマーの減算 ---
    if (reloadCancelledTimer_ > 0.0f)
    {
        reloadCancelledTimer_ -= deltaTime;
        if (reloadCancelledTimer_ < 0.0f) reloadCancelledTimer_ = 0.0f;
    }

    // --- 2. 救急キット回復処理の進行 ---
    if (isHealing_)
    {
        if (isDodging_)
        {
            // 回避（ローリング）動作で治療アクションを中断
            isHealing_ = false;
            healTimer_ = 0.0f;
        }
        else
        {
            healTimer_ -= deltaTime;
            if (healTimer_ <= 0.0f)
            {
                hp_ = (std::min)(maxHp_, hp_ + kMedkitHealAmount);
                if (medkitCount_ > 0) medkitCount_--;
                RaidStats::GetInstance().medkitsUsed++;
                isHealing_ = false;
                healTimer_ = 0.0f;
            }
        }
    }
    else
    {
        // [Q] または [1] キーで救急キット使用開始
        if (!isDodging_ && ((GetAsyncKeyState('Q') & 0x8000) != 0 || (GetAsyncKeyState('1') & 0x8000) != 0))
        {
            StartHealing();
        }
    }

    // --- 3. リロード処理の進行 (予備弾薬からマガジンへ装填) ---
    if (isReloading_)
    {
        if (isDodging_)
        {
            // 回避（ローリング）動作でリロードを即座にキャンセル
            isReloading_ = false;
            reloadTimer_ = 0.0f;
            reloadCancelledTimer_ = kReloadCancelledNotifyDuration;
        }
        else
        {
            reloadTimer_ -= deltaTime;
            if (reloadTimer_ <= 0.0f)
            {
                int needed = maxMagazineAmmo_ - magazineAmmo_;
                int loadAmount = (std::min)(needed, reserveAmmo_);
                magazineAmmo_ += loadAmount;
                reserveAmmo_ -= loadAmount;
                isReloading_ = false;
                reloadTimer_ = 0.0f;
            }
        }
    }
    else
    {
        // [R] キーでリロード開始（予備弾薬があり、マガジンに空きがある場合）
        if (!isDodging_ && (GetAsyncKeyState('R') & 0x8000) != 0 && magazineAmmo_ < maxMagazineAmmo_ && reserveAmmo_ > 0)
        {
            isReloading_ = true;
            reloadTimer_ = kReloadDuration;
            reloadCancelledTimer_ = 0.0f;
        }
    }

    // --- 4. 被弾フラッシュ & 無敵点滅演出の更新 ---
    if (hitFlashTimer_ > 0.0f)
    {
        hitFlashTimer_ -= deltaTime;
        if (hitFlashTimer_ <= 0.0f)
        {
            hitFlashTimer_ = 0.0f;
            object3d_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        }
        else
        {
            object3d_->SetColor({ 4.5f, 0.35f, 0.35f, 1.0f }); // 鮮烈な高輝度レッド被弾フラッシュ
        }
    }
    else if (invincibilityTimer_ > 0.0f)
    {
        invincibilityTimer_ -= deltaTime;
        if (invincibilityTimer_ <= 0.0f)
        {
            invincibilityTimer_ = 0.0f;
            object3d_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        }
        else
        {
            // 5フレームごとに点滅表示
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

    // --- 5. スタミナの自動回復 ---
    if (!isDodging_ && !isDead_)
    {
        stamina_ += kStaminaRegenRate * deltaTime;
        if (stamina_ > maxStamina_)
        {
            stamina_ = maxStamina_;
        }
    }

    bool isMoving = false;

    // --- 6. 回避（ローリング）または通常移動 ---
    if (isDodging_)
    {
        isMoving = true;
        dodgeTimer_ -= deltaTime;
        
        // 回避方向への高速平行移動
        position_.x += dodgeDirection_.x * kDodgeSpeed * frameScale;
        position_.z += dodgeDirection_.z * kDodgeSpeed * frameScale;
        
        // ビジュアル演出：アヒルの形状を考慮した宙返りローリング放物線
        float ratio = dodgeTimer_ / kDodgeDuration;
        if (ratio < 0.0f) ratio = 0.0f;
        float progress = 1.0f - ratio;
        float rollAngle = progress * 6.2831853f; // 360度回転

        // 地面へのくちばし・お尻の埋もれを防止するリフト包絡線
        float rollSin = std::sin(rollAngle);
        float rollCos = std::cos(rollAngle);
        float clearanceLift = 1.05f * (1.0f - rollCos) + 0.40f * std::abs(rollSin);
        float hopOffset = std::sin(progress * 3.14159f) * 0.65f;

        drawPos_ = position_;
        drawPos_.y = position_.y + clearanceLift + hopOffset;
        object3d_->SetTranslate(drawPos_);

        Vector3 rot = object3d_->GetRotate();
        rot.x = rollAngle;
        rot.z = 0.0f;
        object3d_->SetRotate(rot);

        if (dodgeTimer_ <= 0.0f)
        {
            isDodging_ = false;
            dodgeTimer_ = 0.0f;
            object3d_->SetTranslate(position_);
            Vector3 finalRot = object3d_->GetRotate();
            finalRot.x = 0.0f;
            finalRot.z = 0.0f;
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

        // [SPACE] キーで回避ローリング開始
        if ((GetAsyncKeyState(VK_SPACE) & 0x8000) != 0 && stamina_ >= kDodgeStaminaCost)
        {
            isDodging_ = true;
            isMoving = true;
            dodgeTimer_ = kDodgeDuration;
            stamina_ -= kDodgeStaminaCost;

            if (isReloading_)
            {
                isReloading_ = false;
                reloadTimer_ = 0.0f;
                reloadCancelledTimer_ = kReloadCancelledNotifyDuration;
            }

            if (len > 0.0f)
            {
                dodgeDirection_ = moveDir;
            }
            else
            {
                float yaw = GetRotation().y;
                dodgeDirection_ = { std::sin(yaw), 0.0f, std::cos(yaw) };
            }

            float dodgeYaw = std::atan2(dodgeDirection_.x, dodgeDirection_.z);
            Vector3 initRot = object3d_->GetRotate();
            initRot.x = 0.0f;
            initRot.y = dodgeYaw;
            initRot.z = 0.0f;
            object3d_->SetRotate(initRot);
        }
        else
        {
            // 通常のWASD移動処理
            if ((GetAsyncKeyState('W') & 0x8000) != 0)
            {
                position_.z += kMoveSpeed * frameScale;
                isMoving = true;
            }
            if ((GetAsyncKeyState('S') & 0x8000) != 0)
            {
                position_.z -= kMoveSpeed * frameScale;
                isMoving = true;
            }
            if ((GetAsyncKeyState('A') & 0x8000) != 0)
            {
                position_.x -= kMoveSpeed * frameScale;
                isMoving = true;
            }
            if ((GetAsyncKeyState('D') & 0x8000) != 0)
            {
                position_.x += kMoveSpeed * frameScale;
                isMoving = true;
            }
            object3d_->SetTranslate(position_);
        }
    }

    // --- 7. マウス照準レイキャスト（3D地面交点への旋回） ---
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

                Vector4 clipNear = { nx, ny, 0.0f, 1.0f };
                Vector4 clipFar  = { nx, ny, 1.0f, 1.0f };

                Matrix4x4 inv = Inverse(Multiply(camera_->GetViewMatrix(), camera_->GetProjectionMatrix()));

                auto transformClip = [&](const Vector4& c) -> Vector3 {
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
                Vector3 worldFar  = transformClip(clipFar);
                Vector3 dir = { worldFar.x - worldNear.x, worldFar.y - worldNear.y, worldFar.z - worldNear.z };

                // 地面 (y = 0 平面) との交差判定
                if (std::fabs(dir.y) > 1e-6f)
                {
                    float t = -worldNear.y / dir.y;
                    if (t > 0.0f)
                    {
                        Vector3 hit = { worldNear.x + dir.x * t, 0.0f, worldNear.z + dir.z * t };
                        Vector3 ppos = position_;
                        Vector3 to = { hit.x - ppos.x, 0.0f, hit.z - ppos.z };
                        float yaw = std::atan2(to.x, to.z);
                        Vector3 r = object3d_->GetRotate();
                        r.x = 0.0f;
                        r.y = yaw;
                        r.z = 0.0f;
                        object3d_->SetRotate(r);
                    }
                }
            }
        }
    }

    // --- 8. 照準拡散率（レティクルブレ）の更新 ---
    UpdateSpread(deltaTime, isMoving);
    isMoving_ = isMoving;

    if (isDodging_)
    {
        object3d_->SetTranslate(drawPos_);
    }
    else
    {
        object3d_->SetTranslate(position_);
    }
    object3d_->Update();
}

void Player::PostCollisionUpdate()
{
    if (object3d_)
    {
        if (isDodging_)
        {
            drawPos_.x = position_.x;
            drawPos_.z = position_.z;
            object3d_->SetTranslate(drawPos_);
        }
        else
        {
            object3d_->SetTranslate(position_);
        }
        object3d_->Update();
    }
}

// =============================================================================
// 射撃 & アクション (Combat & Action)
// =============================================================================

std::vector<std::unique_ptr<Bullet>> Player::TryShoot(const MouseInput* mouseInput, float deltaTime)
{
    std::vector<std::unique_ptr<Bullet>> bullets;

    // クールダウンタイマーの減算
    if (shotCooldownTimer_ > 0.0f)
    {
        shotCooldownTimer_ -= deltaTime;
        if (shotCooldownTimer_ < 0.0f) shotCooldownTimer_ = 0.0f;
    }

    // 射撃不可条件（死亡・回避中・リロード中・弾薬切れ）
    if (isDead_ || isDodging_ || !object3d_ || !object3dCom_ || !camera_ || !mouseInput)
    {
        return bullets;
    }
    if (isReloading_ || magazineAmmo_ <= 0)
    {
        return bullets;
    }

    // 左クリック押下時かつクールダウン完了時に射撃実行
    if (mouseInput->PushButton(0) && shotCooldownTimer_ <= 0.0f)
    {
        const float yaw = GetRotation().y;

        // 拡散角（レティクルブレ）の適用
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
        bullets.push_back(std::move(bullet));

        // 弾薬消費 & レイド統計への記録
        magazineAmmo_--;
        shotCooldownTimer_ = shotCooldown_;
        RaidStats::GetInstance().shotsFired++;

        // 射撃反動による照準拡散ペナルティの加算
        currentSpread_ += kShootSpreadPenalty;
        if (currentSpread_ > kMaxSpread)
        {
            currentSpread_ = kMaxSpread;
        }
    }

    return bullets;
}

bool Player::StartHealing()
{
    if (isDead_ || isDodging_ || isHealing_ || medkitCount_ <= 0 || hp_ >= maxHp_)
    {
        return false;
    }
    isHealing_ = true;
    healTimer_ = kHealDuration;
    return true;
}

void Player::TakeDamage(float damage, const std::string& attackerName, const std::string& cause)
{
    if (isDead_ || invincibilityTimer_ > 0.0f || isDodging_) return;

    hp_ -= damage;
    if (hp_ <= 0.0f)
    {
        hp_ = 0.0f;
        isDead_ = true;
        lastAttackerName_ = attackerName;
        causeOfDeath_ = cause;
        RaidStats::GetInstance().causeOfDeath = cause;
        RaidStats::GetInstance().killerName = attackerName;
        if (object3d_)
        {
            object3d_->SetColor({ 0.2f, 0.2f, 0.2f, 1.0f }); // 死亡時は暗い灰色に変化
        }
    }
    else
    {
        invincibilityTimer_ = invincibilityDuration_;
        hitFlashTimer_ = hitFlashDuration_;
        object3d_->SetColor({ 5.0f, 5.0f, 5.0f, 1.0f });
    }
}

void Player::UpdateSpread(float deltaTime, bool isMoving)
{
    float targetSpread = kBaseSpread;
    if (isMoving)
    {
        targetSpread += kMoveSpreadPenalty;
    }

    // 移動停止時は徐々に照準が収束（Recover）する
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

// =============================================================================
// 描画 (Draw)
// =============================================================================

void Player::Draw(const RenderContext& ctx)
{
    if (!object3dCom_ || !object3d_) return;

    RenderContext playerCtx = ctx;
    const Object3d::ModelData& modelData = object3d_->GetModelData();
    uint32_t texIdx = (defaultTextureIndex_ != TextureManager::kInvalidTextureIndex) ? defaultTextureIndex_ : modelData.material.textureIndex;
    if (texIdx != TextureManager::kInvalidTextureIndex)
    {
        playerCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(texIdx);
    }

    // 被弾時のノックバック・震動シェイク＆仰け反り（Flinch）演出の適用
    Vector3 originalPos = object3d_->GetTranslate();
    Vector3 originalRot = object3d_->GetRotate();
    if (hitFlashTimer_ > 0.0f)
    {
        float ratio = hitFlashTimer_ / hitFlashDuration_;
        float shakeIntensity = 0.40f * ratio;
        float rx = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * shakeIntensity;
        float rz = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * shakeIntensity;
        object3d_->SetTranslate(originalPos + Vector3{ rx, 0.05f * ratio, rz });

        // 被弾による仰け反り角度
        Vector3 flinchRot = originalRot;
        flinchRot.x -= 0.50f * ratio;
        object3d_->SetRotate(flinchRot);
        object3d_->Update();
    }

    object3dCom_->Draw(object3d_.get(), playerCtx, modelData, true);

    // 描画後は論理座標・回転を元に戻して座標ドリフトを防ぐ
    if (hitFlashTimer_ > 0.0f)
    {
        object3d_->SetTranslate(originalPos);
        object3d_->SetRotate(originalRot);
        object3d_->Update();
    }
}
