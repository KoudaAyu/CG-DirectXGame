#include "Player.h"
#include "PlayerTuning.h"
#include "DuckPartRig.h"
#include "Object3d.h"
#include "Object3dCom.h"
#include "TextureManager.h"
#include "SceneManager.h"
#include "Application/GameObject/Bullet.h"
#include "Application/Audio/TacticalAudio.h"
#include "Application/Config/AppDebugConfig.h"
#include "Application/UI/TacticalUICanvas.h"
#include "Baziru3_Engine/Core/IO/Mouse/MouseInput.h"
#include "Matrix4x4.h"
#include <Windows.h>
#include <cmath>
#include <algorithm>
#include <filesystem>
#include "../Scene/GameScene/RaidStats.h"

namespace
{
    constexpr Vector3 kZeroVec3  = { 0.0f, 0.0f, 0.0f };
    constexpr Vector3 kUnitScale = { 1.0f, 1.0f, 1.0f };

    inline float SmoothStep01(float t)
    {
        t = (std::clamp)(t, 0.0f, 1.0f);
        return t * t * (PlayerTuning::kSmoothStepCubicBias - 2.0f * t);
    }

    inline float ComputeReloadCantEnvelope(float progress)
    {
        using namespace PlayerTuning::ReloadAnim;
        return SmoothStep01(progress / kCantInSpan) * (1.0f - SmoothStep01((progress - kCantOutStart) / kCantOutSpan));
    }

    inline float RandRecoil01() { return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX); }
    inline float RandRecoilRange(float minVal, float maxVal) { return minVal + (maxVal - minVal) * RandRecoil01(); }

    inline Vector2 ResolveClientSize(float rawW, float rawH)
    {
        return { (rawW > 0.0f) ? rawW : kUIDefaultDisplayWidth, (rawH > 0.0f) ? rawH : kUIDefaultDisplayHeight };
    }

    inline void SetObjectTransform(Object3d* obj, const Vector3& pos, const Vector3& rot, const Vector3& scale, bool doUpdate = false)
    {
        if (!obj) return;
        obj->SetTranslate(pos);
        obj->SetRotate(rot);
        obj->SetScale(scale);
        if (doUpdate) obj->Update();
    }

    inline void AttachLimbPreset(
        Object3d* const (&limbs)[PlayerTuning::kDuckLimbCount],
        const PlayerTuning::Anim::LimbPoseSpec (&poses)[PlayerTuning::kDuckLimbCount],
        const Vector3& bodyPos, const Vector3& bodyScale, const Matrix4x4& bodyRotMat)
    {
        for (int i = 0; i < PlayerTuning::kDuckLimbCount; ++i)
        {
            DuckPartRig::AttachLimbToBody(limbs[i], poses[i].pivot, poses[i].extraOffset, poses[i].jointRot, bodyPos, bodyScale, bodyRotMat);
        }
    }
}

// =============================================================================
// 軍用アサルトライフル 3Dプロシージャルメッシュ生成 (BuildRifleModel)
// =============================================================================

void Player::BuildRifleModel(Object3d::ModelData& data, bool includeMagazine)
{
    using namespace PlayerTuning::Weapon;
    const size_t totalBoxes = std::size(kRifleBaseBoxes) + (includeMagazine ? std::size(kRifleMagazineBoxes) : 0u);
    data.vertices.clear();
    data.indices.clear();
    data.vertices.reserve(totalBoxes * static_cast<size_t>(DuckPartRig::kBoxFaceCount * DuckPartRig::kBoxFaceVertCount));
    data.indices.reserve(totalBoxes * static_cast<size_t>(DuckPartRig::kBoxFaceCount * DuckPartRig::kBoxFaceCount));

    for (const DuckPartRig::BoxBounds& box : kRifleBaseBoxes) DuckPartRig::AppendBox(data, box.minP, box.maxP);
    if (includeMagazine)
    {
        for (const DuckPartRig::BoxBounds& box : kRifleMagazineBoxes) DuckPartRig::AppendBox(data, box.minP, box.maxP);
    }
    data.boundingRadius = kRifleBoundingRadius;
}

// =============================================================================
// 初期化 & 解放 (Lifecycle)
// =============================================================================

void Player::Initialize(Object3dCom* object3dCom, Camera* camera)
{
    object3dCom_ = object3dCom;
    camera_ = camera;
    defaultTextureIndex_ = TextureManager::GetInstance()->Load("Resources/duck.png");

    // アヒルの5分割アーティキュレーションメッシュ（胴体＋左右水かき足＋左右の翼）を初期化
    const DuckPartRig::SplitMeshSet& split = DuckPartRig::GetCachedDuckSplitMeshes();
    auto initDuckPart = [&](std::unique_ptr<Object3d>& dst, Object3d::ModelData partData)
    {
        if (partData.material.textureFilePath.empty()) partData.material.textureFilePath = "Resources/duck.png";
        partData.material.textureIndex = (defaultTextureIndex_ != TextureManager::kInvalidTextureIndex) ? defaultTextureIndex_ : 0u;
        partData.boundingRadius = DuckPartRig::kDuckPartBoundingRadius;

        dst = std::make_unique<Object3d>();
        dst->Initialize(object3dCom_, partData);
        dst->SetCamera(camera_);
        dst->SetScale(kUnitScale);
        dst->SetRotate(kZeroVec3);
        dst->SetColor(PlayerTuning::Visual::kDefaultBodyColor);
    };

    initDuckPart(object3d_, split.body);
    initDuckPart(leftFootObject_, split.leftFoot);
    initDuckPart(rightFootObject_, split.rightFoot);
    initDuckPart(leftWingObject_, split.leftWing);
    initDuckPart(rightWingObject_, split.rightWing);

    position_ = drawPos_ = kZeroVec3;
    object3d_->SetTranslate(position_);

    collider_ = std::make_unique<SphereCollider>(kColliderRadius, &position_, CollisionAttribute::Player);
    collider_->SetPositionOffset({ 0.0f, kColliderRadius, 0.0f });
    CollisionManager::GetInstance()->RegisterCollider(collider_.get());

    auto initWeaponPart = [&](std::unique_ptr<Object3d>& dst, const Object3d::ModelData& model, const Vector4& color)
    {
        dst = std::make_unique<Object3d>();
        dst->Initialize(object3dCom_, model);
        dst->SetCamera(camera_);
        dst->SetScale(kUnitScale);
        dst->SetColor(color);
        dst->SetEnableLighting(true);
        dst->SetAllowWireframeOverlay(false);
    };

    // 軍用アサルトライフル3Dモデルの初期化（マグウェル開放型）
    Object3d::ModelData rifleModel{};
    BuildRifleModel(rifleModel, false);
    rifleTextureIndex_ = TextureManager::GetInstance()->Load("Resources/container_military.png");
    rifleModel.material.textureFilePath = "Resources/container_military.png";
    rifleModel.material.textureIndex = (rifleTextureIndex_ != TextureManager::kInvalidTextureIndex) ? rifleTextureIndex_ : 0u;
    initWeaponPart(rifleObject_, rifleModel, PlayerTuning::Weapon::kRifleGunmetalColor);

    // 着脱式3D湾曲PMAGの初期化
    rifleMagTextureIndex_ = TextureManager::GetInstance()->Load("Resources/duckov_magazine_tex.png");
    if (rifleMagTextureIndex_ == TextureManager::kInvalidTextureIndex) rifleMagTextureIndex_ = rifleTextureIndex_;

    const bool hasMagObj = std::filesystem::exists("Resources/duckov_magazine_rifle.obj")
                        || std::filesystem::exists("project/Resources/duckov_magazine_rifle.obj");
    Object3d::ModelData magModel{};
    if (hasMagObj) magModel = Object3d::LoadObjFile("Resources", "duckov_magazine_rifle.obj");
    else           BuildRifleModel(magModel, true);
    magModel.material.textureFilePath = "Resources/duckov_magazine_tex.png";
    magModel.material.textureIndex = (rifleMagTextureIndex_ != TextureManager::kInvalidTextureIndex) ? rifleMagTextureIndex_ : 0u;
    magModel.boundingRadius = PlayerTuning::Weapon::kMagazineBoundingRadius;

    initWeaponPart(rifleMagObject_, magModel, PlayerTuning::Visual::kDefaultBodyColor);
    rifleMagObject_->SetReflectionFactor(PlayerTuning::Weapon::kMagazineReflection);
    rifleMagObject_->SetFresnelF0(PlayerTuning::Weapon::kMagazineFresnelF0);

    Reset();
}

void Player::Finalize()
{
    if (collider_) { CollisionManager::GetInstance()->UnregisterCollider(collider_.get()); collider_.reset(); }
    for (auto* part : { &leftFootObject_, &rightFootObject_, &leftWingObject_, &rightWingObject_, &object3d_, &rifleMagObject_, &rifleObject_ })
    {
        part->reset();
    }
}

void Player::SetPosition(const Vector3& pos)
{
    position_ = drawPos_ = pos;
    if (object3d_) SyncDuckVisualAndWeaponPose();
}

bool Player::ConsumeFootstepDust(Vector3& outWorldPos)
{
    if (!pendingFootstepDust_) return false;
    pendingFootstepDust_ = false;
    outWorldPos = lastFootstepWorldPos_;
    return true;
}

void Player::Reset()
{
    hp_ = maxHp_ = kDefaultMaxHp;
    isDead_ = hasAttackerPosition_ = false;
    invincibilityTimer_ = hitFlashTimer_ = 0.0f;
    lastAttackerPosition_ = kZeroVec3;

    magazineAmmo_ = maxMagazineAmmo_ = kDefaultMaxMagazineAmmo;
    reserveAmmo_ = kDefaultReserveAmmo;
    isReloading_ = wasDryReload_ = false;
    reloadTimer_ = reloadCancelledTimer_ = 0.0f;
    emptyTriggerAlertTimer_ = dryFireRepeatTimer_ = reloadCompleteFlashTimer_ = 0.0f;
    suppressShootUntilMouseRelease_ = true;

    medkitCount_ = kDefaultMedkitCount;
    lootValue_ = 0;
    isHealing_ = false;
    healTimer_ = 0.0f;

    isDodging_ = prevSpacePressed_ = false;
    dodgeTimer_ = 0.0f;
    dodgeDirection_ = { 0.0f, 0.0f, 1.0f };
    stamina_ = maxStamina_ = kDefaultMaxStamina;

    currentSpread_ = recoilHeat_ = recoilKickVisual_ = 0.0f;
    reticleRecoilOffset_ = reticleRecoilVel_ = { 0.0f, 0.0f };
    effectiveReticleScreenPos_ = kDefaultReticleScreenPos;
    aimRayDistance_ = kDefaultAimRayDistance;
    recoilSideSign_ = 1;
    isMoving_ = isInCover_ = false;
    position_ = drawPos_ = kZeroVec3;

    aimYaw_ = walkBlend_ = sprintBlend_ = idleAnimTime_ = smoothedLocalMoveRight_ = 0.0f;
    walkPhase_ = PlayerTuning::Anim::kInitialWalkPhase;
    moveInputDir_ = kZeroVec3;
    smoothedLocalMoveFwd_ = 1.0f;
    isInIntroPose_ = pendingFootstepDust_ = false;
    lastFootstepWorldPos_ = position_;
    currentBodyColor_ = PlayerTuning::Visual::kDefaultBodyColor;

    if (object3d_)
    {
        SetObjectTransform(object3d_.get(), position_, kZeroVec3, kUnitScale);
        object3d_->SetColor(currentBodyColor_);
    }

    rifleRecoil_ = 0.0f;
    rifleMagVisible_ = true;
    isWeaponHolstered_ = prevCPressed_ = false;
    if (rifleObject_)    { rifleObject_->SetScale(kUnitScale);    rifleObject_->SetColor(PlayerTuning::Weapon::kRifleGunmetalColor); }
    if (rifleMagObject_) { rifleMagObject_->SetScale(kUnitScale); rifleMagObject_->SetColor(PlayerTuning::Visual::kDefaultBodyColor); }
    SyncDuckVisualAndWeaponPose();
}

// =============================================================================
// メイン更新ループ (Update)
// =============================================================================

void Player::CancelReloadWithNotify()
{
    isReloading_ = false;
    reloadTimer_ = 0.0f;
    reloadCancelledTimer_ = kReloadCancelledNotifyDuration;
    TacticalAudio::Play2D("Resources/Audio/SE_Gun_DryFire.wav", PlayerTuning::Audio::kReloadCancelVolume, PlayerTuning::Audio::kDryFireCooldownMs, "reload_cancel");
}

void Player::UpdateActionStates(float deltaTime, const MouseInput* mouseInput)
{
    using namespace PlayerTuning::Audio;
    const bool textCaptured = AppDebugState::IsImGuiCapturingTextInput();

    if (reloadCancelledTimer_ > 0.0f) reloadCancelledTimer_ = (std::max)(0.0f, reloadCancelledTimer_ - deltaTime);
    if (emptyTriggerAlertTimer_ > 0.0f) emptyTriggerAlertTimer_ = (std::max)(0.0f, emptyTriggerAlertTimer_ - deltaTime);
    if (reloadCompleteFlashTimer_ > 0.0f) reloadCompleteFlashTimer_ = (std::max)(0.0f, reloadCompleteFlashTimer_ - deltaTime);

    // 救急キット回復処理の進行 / [Q] または [1] キーで使用開始
    if (isHealing_)
    {
        if (isDodging_)
        {
            isHealing_ = false;
            healTimer_ = 0.0f;
        }
        else if ((healTimer_ -= deltaTime) <= 0.0f)
        {
            hp_ = (std::min)(maxHp_, hp_ + kMedkitHealAmount);
            if (medkitCount_ > 0) medkitCount_--;
            RaidStats::GetInstance().medkitsUsed++;
            isHealing_ = false;
            healTimer_ = 0.0f;
            TacticalAudio::Play2D("Resources/Audio/SE_Heal_Complete.wav", kHealCompleteVolume);
        }
    }
    else if (!isDodging_ && ((demoCtrl_.active && demoCtrl_.triggerHeal) || (!demoCtrl_.active && !textCaptured && (IsAppKeyDown('Q') || IsAppKeyDown('1')))))
    {
        if (demoCtrl_.active && medkitCount_ <= 0) medkitCount_ = 1;
        StartHealing();
    }

    // [C] キー：銃収納ダッシュ切替 ＆ 左クリック押下時の即座の構え直し
    if (demoCtrl_.active)
    {
        const bool wantHolster = demoCtrl_.holsterSprint && !demoCtrl_.fireHeld && !isReloading_ && !isDodging_;
        if (wantHolster != isWeaponHolstered_)
        {
            isWeaponHolstered_ = wantHolster;
            if (isWeaponHolstered_ && isReloading_) { isReloading_ = false; reloadTimer_ = 0.0f; }
        }
    }
    else
    {
        const bool isCDown = !textCaptured && IsAppKeyDown('C');
        const bool isCTriggered = isCDown && !prevCPressed_;
        prevCPressed_ = isCDown;

        if (isCTriggered && !isDodging_)
        {
            isWeaponHolstered_ = !isWeaponHolstered_;
            TacticalAudio::Play2D("Resources/Audio/SE_Weapon_Holster.wav", kHolsterToggleVolume, kHolsterToggleCooldownMs);
            if (isWeaponHolstered_ && isReloading_) { isReloading_ = false; reloadTimer_ = 0.0f; }
        }
        if (isWeaponHolstered_ && mouseInput && !AppDebugState::IsImGuiCapturingMouse() && mouseInput->PushButton(0))
        {
            isWeaponHolstered_ = false;
            TacticalAudio::Play2D("Resources/Audio/SE_Weapon_Holster.wav", kUnholsterAimVolume, kUnholsterAimCooldownMs);
        }
    }

    // デモプレイ中は予備弾薬が尽きないよう自動補給して継続的に射撃＆リロード演出を見せる
    if (demoCtrl_.active && reserveAmmo_ < 30)
    {
        reserveAmmo_ = 90;
    }

    // リロード処理の進行 / [R] キーでリロード開始
    if (isReloading_)
    {
        if (isDodging_)
        {
            CancelReloadWithNotify();
        }
        else if ((reloadTimer_ -= deltaTime) <= 0.0f)
        {
            const int loadAmount = (std::min)(maxMagazineAmmo_ - magazineAmmo_, reserveAmmo_);
            magazineAmmo_ += loadAmount;
            reserveAmmo_ -= loadAmount;
            isReloading_ = false;
            reloadTimer_ = 0.0f;
            emptyTriggerAlertTimer_ = 0.0f;
            dryFireRepeatTimer_ = 0.0f;
            reloadCompleteFlashTimer_ = kReloadCompleteFlashDuration;
            TacticalAudio::Play2D("Resources/Audio/SE_Reload_Ready.wav", kReloadCompleteVolume, kEmptyMagAlertCooldownMs, "reload_ready");
        }
    }
    else if (!isDodging_ && ((demoCtrl_.active && (demoCtrl_.triggerReload || magazineAmmo_ <= 0)) || (!demoCtrl_.active && !textCaptured && IsAppKeyDown('R'))))
    {
        if (magazineAmmo_ < maxMagazineAmmo_ && reserveAmmo_ > 0)
        {
            isWeaponHolstered_ = false;
            wasDryReload_ = (magazineAmmo_ <= 0);
            isReloading_ = true;
            reloadTimer_ = kReloadDuration;
            reloadCancelledTimer_ = 0.0f;
            emptyTriggerAlertTimer_ = 0.0f;
            reloadCompleteFlashTimer_ = 0.0f;
        }
        else if (magazineAmmo_ < maxMagazineAmmo_ && reserveAmmo_ <= 0)
        {
            // 予備弾薬もゼロの状態で [R] を押した場合は弾薬切れアラート音＋UI強調
            emptyTriggerAlertTimer_ = kEmptyTriggerAlertDuration;
            TacticalAudio::Play2D("Resources/Audio/SE_Ammo_EmptyAlert.wav", kDryFireVolume, kNoReserveAlertCooldownMs, "no_reserve_alert");
        }
    }
}

void Player::UpdateDamageVisualTimers(float deltaTime)
{
    using namespace PlayerTuning::Visual;
    if (hitFlashTimer_ > 0.0f)
    {
        hitFlashTimer_ = (std::max)(0.0f, hitFlashTimer_ - deltaTime);
        currentBodyColor_ = (hitFlashTimer_ > 0.0f) ? kHitActiveFlashColor : kDefaultBodyColor;
        object3d_->SetColor(currentBodyColor_);
    }
    else if (invincibilityTimer_ > 0.0f)
    {
        invincibilityTimer_ = (std::max)(0.0f, invincibilityTimer_ - deltaTime);
        if (invincibilityTimer_ <= 0.0f) currentBodyColor_ = kDefaultBodyColor;
        else
        {
            static int flashCount = 0;
            currentBodyColor_ = (((++flashCount / kInvincibleBlinkFrames) % 2) == 0) ? kInvincibleBlinkColor : kDefaultBodyColor;
        }
        object3d_->SetColor(currentBodyColor_);
    }
}

bool Player::UpdateDodgeAndMovement(float deltaTime, float frameScale)
{
    isInIntroPose_ = false;
    moveInputDir_ = kZeroVec3;

    if (isDodging_)
    {
        dodgeTimer_ -= deltaTime;
        position_.x += dodgeDirection_.x * kDodgeSpeed * frameScale;
        position_.z += dodgeDirection_.z * kDodgeSpeed * frameScale;

        // アヒルの形状を考慮した宙返りローリング放物線と地面埋もれ防止リフト包絡線
        const float progress  = 1.0f - (std::max)(0.0f, dodgeTimer_ / kDodgeDuration);
        const float rollAngle = progress * DuckPartRig::kTwoPi;
        const float liftY     = GameConfig::Player::kDodgeClearanceBase * (1.0f - std::cos(rollAngle))
                              + GameConfig::Player::kDodgeClearanceSide * std::abs(std::sin(rollAngle))
                              + std::sin(progress * DuckPartRig::kPi) * GameConfig::Player::kDodgeHopMax;

        if (dodgeTimer_ <= 0.0f)
        {
            isDodging_ = false;
            dodgeTimer_ = 0.0f;
            drawPos_ = position_;
            SetObjectTransform(object3d_.get(), position_, { 0.0f, aimYaw_, 0.0f }, kUnitScale);
        }
        else
        {
            drawPos_ = { position_.x, position_.y + liftY, position_.z };
            SetObjectTransform(object3d_.get(), drawPos_, { rollAngle, aimYaw_, 0.0f }, kUnitScale);
        }
        return true;
    }

    // WASD入力方向の計算（ImGuiテキスト入力中は移動入力を抑止 / F7デモプレイ中はAIベクトルを使用）
    const bool textCaptured = AppDebugState::IsImGuiCapturingTextInput();
    Vector3 moveDir = kZeroVec3;
    if (demoCtrl_.active)
    {
        moveDir = demoCtrl_.moveDir;
        moveDir.y = 0.0f;
    }
    else if (!textCaptured)
    {
        if (IsAppKeyDown('W')) moveDir.z += 1.0f;
        if (IsAppKeyDown('S')) moveDir.z -= 1.0f;
        if (IsAppKeyDown('A')) moveDir.x -= 1.0f;
        if (IsAppKeyDown('D')) moveDir.x += 1.0f;
    }

    bool isMoving = false;
    const float lenSq = moveDir.x * moveDir.x + moveDir.z * moveDir.z;
    if (lenSq > 0.0f)
    {
        const float invLen = 1.0f / std::sqrt(lenSq);
        moveDir.x *= invLen;
        moveDir.z *= invLen;
        moveInputDir_ = moveDir;
        isMoving = true;
    }

    const bool isSpaceDown = !demoCtrl_.active && !textCaptured && IsAppKeyDown(VK_SPACE);
    const bool isSpaceTriggered = demoCtrl_.active ? demoCtrl_.triggerDodge : (isSpaceDown && !prevSpacePressed_);
    prevSpacePressed_ = isSpaceDown;

    // [SPACE] キーで回避ローリング開始
    if (isSpaceTriggered && stamina_ >= kDodgeStaminaCost)
    {
        isDodging_ = true;
        dodgeTimer_ = kDodgeDuration;
        stamina_ -= kDodgeStaminaCost;
        if (isReloading_) CancelReloadWithNotify();

        dodgeDirection_ = (lenSq > 0.0f) ? moveDir : Vector3{ std::sin(aimYaw_), 0.0f, std::cos(aimYaw_) };
        aimYaw_ = std::atan2(dodgeDirection_.x, dodgeDirection_.z);
        drawPos_ = position_;
        SetObjectTransform(object3d_.get(), drawPos_, { 0.0f, aimYaw_, 0.0f }, kUnitScale);
        return true;
    }

    if (lenSq > 0.0f)
    {
        // F7自動デモ中は審査員がパーティクルをじっくり追えるよう 0.78倍 の落ち着いた歩調にする
        const float demoSpeedScale = demoCtrl_.active ? 0.78f : 1.0f;
        const float effectiveSpeed = kMoveSpeed * (isWeaponHolstered_ ? kHolsterSpeedMultiplier : 1.0f) * demoSpeedScale;
        position_.x += moveDir.x * effectiveSpeed * frameScale;
        position_.z += moveDir.z * effectiveSpeed * frameScale;
    }
    return isMoving;
}

void Player::Update(float deltaTime, MouseInput* mouseInput)
{
    if (!object3d_) return;
    if (isDead_) { object3d_->Update(); return; }

    const float frameScale = deltaTime * PlayerTuning::kReferenceFps;
    UpdateActionStates(deltaTime, mouseInput);
    UpdateDamageVisualTimers(deltaTime);

    if (!isDodging_ && !isDead_) stamina_ = (std::min)(maxStamina_, stamina_ + kStaminaRegenRate * deltaTime);

    const bool isMoving = UpdateDodgeAndMovement(deltaTime, frameScale);
    isMoving_ = isMoving;
    UpdateReticleRecoil(deltaTime);
    UpdateSpread(deltaTime, isMoving);

    // 地面接地保証・ロコモーション更新・ライフルストローク減衰
    position_.y = 0.0f;
    UpdateLocomotionState(deltaTime, moveInputDir_);
    if (rifleRecoil_ > 0.0f) rifleRecoil_ = (std::max)(0.0f, rifleRecoil_ - deltaTime * PlayerTuning::Weapon::kRifleRecoilDecayRate);

    if (isDodging_)
    {
        SyncDuckVisualAndWeaponPose();
        if (mouseInput) SyncAimWithCamera(mouseInput);
    }
    else if (mouseInput && camera_) SyncAimWithCamera(mouseInput);
    else                            SyncDuckVisualAndWeaponPose();
}

void Player::UpdateLocomotionState(float deltaTime, const Vector3& moveDirWorld)
{
    using namespace PlayerTuning::Anim;
    const float dt = (std::clamp)(deltaTime, 0.0f, PlayerTuning::kMaxPhysicsStepDt);
    idleAnimTime_ += dt;
    if (idleAnimTime_ > kIdleAnimWrapSec) idleAnimTime_ -= kIdleAnimWrapSec;

    const float targetWalk   = (!isDodging_ && isMoving_) ? 1.0f : 0.0f;
    const float targetSprint = (!isDodging_ && isMoving_ && isWeaponHolstered_) ? 1.0f : 0.0f;
    const float blendRate    = (targetWalk > walkBlend_) ? kWalkBlendInRate : kWalkBlendOutRate;

    walkBlend_ += (targetWalk - walkBlend_) * (std::min)(1.0f, blendRate * dt);
    if (walkBlend_ < kBlendSnapEpsilon && targetWalk == 0.0f) walkBlend_ = 0.0f;

    sprintBlend_ += (targetSprint - sprintBlend_) * (std::min)(1.0f, kSprintBlendRate * dt);
    if (sprintBlend_ < kBlendSnapEpsilon && targetSprint == 0.0f) sprintBlend_ = 0.0f;

    // [C] 銃収納ダッシュ中は進行方向へ素早く身体を向けて全力疾走する
    const float moveLenSq = moveDirWorld.x * moveDirWorld.x + moveDirWorld.z * moveDirWorld.z;
    if (!isDodging_ && isWeaponHolstered_ && isMoving_ && moveLenSq > kMoveDirMinLenSq)
    {
        const float targetRunYaw = std::atan2(moveDirWorld.x, moveDirWorld.z);
        const float yawDiff      = GameConfig::Math::WrapAngleSigned(targetRunYaw - aimYaw_);
        const float maxTurn      = kHolsterRunTurnRate * dt;
        aimYaw_ = (std::abs(yawDiff) <= maxTurn)
            ? targetRunYaw
            : GameConfig::Math::WrapAngleSigned(aimYaw_ + ((yawDiff > 0.0f) ? maxTurn : -maxTurn));
    }

    // ローカル移動方向を滑らかに追従させてムーンウォークを完全防止
    if (!isDodging_ && isMoving_ && moveLenSq > kMoveDirMinLenSq)
    {
        const float sy       = std::sin(aimYaw_);
        const float cy       = std::cos(aimYaw_);
        const float rawRight = moveDirWorld.x * cy - moveDirWorld.z * sy;
        const float rawFwd   = moveDirWorld.x * sy + moveDirWorld.z * cy;
        const float dirBlend = (std::min)(1.0f, kLocalMoveDirBlendRate * dt);
        smoothedLocalMoveRight_ += (rawRight - smoothedLocalMoveRight_) * dirBlend;
        smoothedLocalMoveFwd_   += (rawFwd   - smoothedLocalMoveFwd_)   * dirBlend;

        const float localLenSq = smoothedLocalMoveRight_ * smoothedLocalMoveRight_ + smoothedLocalMoveFwd_ * smoothedLocalMoveFwd_;
        if (localLenSq > GameConfig::Math::Square(PlayerTuning::kSmallLenEpsilon))
        {
            const float invLocalLen = 1.0f / std::sqrt(localLenSq);
            smoothedLocalMoveRight_ *= invLocalLen;
            smoothedLocalMoveFwd_   *= invLocalLen;
        }
    }

    // よちよち歩きサイクル位相更新 ＆ 左右の水かき足着地時の土埃トリガー発行
    if (!isDodging_ && walkBlend_ > kFootstepMinWalkBlend)
    {
        const float cadence   = kWalkCadenceBase + kWalkCadenceSprintAdd * sprintBlend_;
        const float prevPhase = walkPhase_;
        walkPhase_ += dt * cadence * (kWalkCadenceMinFactor + kWalkCadenceBlendFactor * walkBlend_);

        const float strideLen = (kDustStrideBase + kStrideSprintAdd * sprintBlend_) * walkBlend_;
        const float cy = std::cos(aimYaw_);
        const float sy = std::sin(aimYaw_);
        auto calcFootPlantWorld = [&](const Vector3& pivot) -> Vector3
        {
            const float lx = pivot.x + smoothedLocalMoveRight_ * strideLen * DuckPartRig::kFootLateralStrideRate;
            const float lz = pivot.z + smoothedLocalMoveFwd_   * strideLen;
            return { position_.x + lx * cy + lz * sy, position_.y, position_.z - lx * sy + lz * cy };
        };

        if (prevPhase < DuckPartRig::kPi && walkPhase_ >= DuckPartRig::kPi)
        {
            pendingFootstepDust_ = true;
            lastFootstepWorldPos_ = calcFootPlantWorld(DuckPartRig::kLeftFootPivot);
        }
        if (walkPhase_ >= DuckPartRig::kTwoPi)
        {
            walkPhase_ = std::fmod(walkPhase_, DuckPartRig::kTwoPi);
            pendingFootstepDust_ = true;
            lastFootstepWorldPos_ = calcFootPlantWorld(DuckPartRig::kRightFootPivot);
        }
    }
    else if (!isDodging_ && !isMoving_ && walkBlend_ <= kBlendSnapEpsilon)
    {
        walkPhase_ = kInitialWalkPhase;
    }
}

Vector3 Player::GetMuzzleWorldPosition() const
{
    const float yaw = GetRotation().y;
    return Bullet::ComputeSpawnPosition(position_, { std::sin(yaw), 0.0f, std::cos(yaw) }, bulletSpawnOffset_);
}

void Player::SyncAimWithCamera(const MouseInput* mouseInput)
{
    if (!object3d_ || !camera_ || !mouseInput || isDead_) return;

    using namespace PlayerTuning;
    WindowAPI* win = mouseInput->GetWindowAPI();
    const Vector2 clientSize = ResolveClientSize(
        win ? static_cast<float>(win->GetClientWidth())  : kUIDefaultDisplayWidth,
        win ? static_cast<float>(win->GetClientHeight()) : kUIDefaultDisplayHeight);

    const float resScale   = clientSize.y / kUIDefaultDisplayHeight;
    if (demoCtrl_.active)
    {
        // F7自動デモプレイ時：3D照準ターゲットをスクリーンへ投影してレティクル＆レーザーサイトを滑らかに追従させる
        const Matrix4x4& vp = camera_->GetViewProjectionMatrix();
        const Vector3& wp = demoCtrl_.aimWorldPoint;
        const float cw = wp.x * vp.m[0][kMatW] + wp.y * vp.m[1][kMatW] + wp.z * vp.m[2][kMatW] + vp.m[kMatW][kMatW];
        if (cw > 0.05f)
        {
            const float cx = (wp.x * vp.m[0][0] + wp.y * vp.m[1][0] + wp.z * vp.m[2][0] + vp.m[kMatW][0]) / cw;
            const float cy = (wp.x * vp.m[0][1] + wp.y * vp.m[1][1] + wp.z * vp.m[2][1] + vp.m[kMatW][1]) / cw;
            const float sx = (cx + 1.0f) * 0.5f * clientSize.x;
            const float sy = (1.0f - cy) * 0.5f * clientSize.y;
            effectiveReticleScreenPos_.x = (std::clamp)(sx + reticleRecoilOffset_.x * resScale, kReticleScreenMarginPx, clientSize.x - kReticleScreenMarginPx);
            effectiveReticleScreenPos_.y = (std::clamp)(sy + reticleRecoilOffset_.y * resScale, kReticleScreenMarginPx, clientSize.y - kReticleScreenMarginPx);
        }
        if (isDodging_) return;

        const float dx = wp.x - position_.x;
        const float dz = wp.z - position_.z;
        const float distXZ = std::sqrt(dx * dx + dz * dz);
        if (distXZ > kSmallLenEpsilon)
        {
            const float baseYaw    = std::atan2(dx, dz);
            const float closeBlend = SmoothStep01((distXZ - Aim::kCloseBlendStartDist) / Aim::kCloseBlendRange);
            const float effRight   = (std::min)(bulletSpawnOffset_.x * closeBlend, distXZ * Aim::kMaxRightOffsetDistRatio);
            const float sinArg     = (std::clamp)(effRight / distXZ, -Aim::kMaxSinArg, Aim::kMaxSinArg);
            const float exactYaw   = baseYaw - std::asin(sinArg);

            if (!(isWeaponHolstered_ && isMoving_))
            {
                const float diff = GameConfig::Math::WrapAngleSigned(exactYaw - aimYaw_);
                aimYaw_ = GameConfig::Math::WrapAngleSigned(aimYaw_ + diff * 0.22f);
            }

            const Vector3 fwd    = { std::sin(aimYaw_), 0.0f, std::cos(aimYaw_) };
            const Vector3 muzzle = Bullet::ComputeSpawnPosition(position_, fwd, bulletSpawnOffset_);
            const float fwdProj  = (wp.x - muzzle.x) * fwd.x + (wp.z - muzzle.z) * fwd.z;
            aimRayDistance_      = (std::clamp)(fwdProj > 0.5f ? fwdProj : distXZ, Aim::kAimRayMinDist, Aim::kAimRayMaxDist);
        }
        SyncDuckVisualAndWeaponPose();
        return;
    }

    const Vector2 rawMouse = mouseInput->GetScaledPosition();
    effectiveReticleScreenPos_.x = (std::clamp)(rawMouse.x + reticleRecoilOffset_.x * resScale, kReticleScreenMarginPx, clientSize.x - kReticleScreenMarginPx);
    effectiveReticleScreenPos_.y = (std::clamp)(rawMouse.y + reticleRecoilOffset_.y * resScale, kReticleScreenMarginPx, clientSize.y - kReticleScreenMarginPx);
    if (isDodging_) return;

    // NDCから最新カメラ ViewProjection 逆行列でワールド照準レイを復元
    const float nx = (effectiveReticleScreenPos_.x / clientSize.x) * 2.0f - 1.0f;
    const float ny = 1.0f - (effectiveReticleScreenPos_.y / clientSize.y) * 2.0f;
    const Matrix4x4 inv = Inverse(camera_->GetViewProjectionMatrix());

    auto transformClip = [&](float clipZ) -> Vector3
    {
        Vector3 r = {
            nx * inv.m[0][0] + ny * inv.m[1][0] + clipZ * inv.m[2][0] + inv.m[kMatW][0],
            nx * inv.m[0][1] + ny * inv.m[1][1] + clipZ * inv.m[2][1] + inv.m[kMatW][1],
            nx * inv.m[0][2] + ny * inv.m[1][2] + clipZ * inv.m[2][2] + inv.m[kMatW][2]
        };
        const float w = nx * inv.m[0][kMatW] + ny * inv.m[1][kMatW] + clipZ * inv.m[2][kMatW] + inv.m[kMatW][kMatW];
        if (std::fabs(w) > kRayParallelEpsilon)
        {
            const float invW = 1.0f / w;
            r.x *= invW; r.y *= invW; r.z *= invW;
        }
        return r;
    };

    const Vector3 worldNear = transformClip(0.0f);
    const Vector3 worldFar  = transformClip(1.0f);
    const Vector3 dir       = { worldFar.x - worldNear.x, worldFar.y - worldNear.y, worldFar.z - worldNear.z };

    // 3D銃口・レーザー・弾丸の水平飛行面 (y = position_.y + bulletSpawnOffset_.y) と厳密交差
    const float aimPlaneY = position_.y + bulletSpawnOffset_.y;
    if (std::fabs(dir.y) > kRayParallelEpsilon)
    {
        const float t = (aimPlaneY - worldNear.y) / dir.y;
        if (t > 0.0f)
        {
            const Vector3 hit  = { worldNear.x + dir.x * t, aimPlaneY, worldNear.z + dir.z * t };
            const float dx     = hit.x - position_.x;
            const float dz     = hit.z - position_.z;
            const float distXZ = std::sqrt(dx * dx + dz * dz);
            if (distXZ > kSmallLenEpsilon)
            {
                const float baseYaw    = std::atan2(dx, dz);
                const float closeBlend = SmoothStep01((distXZ - Aim::kCloseBlendStartDist) / Aim::kCloseBlendRange);
                const float effRight   = (std::min)(bulletSpawnOffset_.x * closeBlend, distXZ * Aim::kMaxRightOffsetDistRatio);
                const float sinArg     = (std::clamp)(effRight / distXZ, -Aim::kMaxSinArg, Aim::kMaxSinArg);
                const float exactYaw   = baseYaw - std::asin(sinArg);

                if (!(isWeaponHolstered_ && isMoving_)) aimYaw_ = exactYaw;

                const Vector3 fwd    = { std::sin(exactYaw), 0.0f, std::cos(exactYaw) };
                const Vector3 muzzle = Bullet::ComputeSpawnPosition(position_, fwd, bulletSpawnOffset_);
                const float fwdProj  = (hit.x - muzzle.x) * fwd.x + (hit.z - muzzle.z) * fwd.z;
                aimRayDistance_      = (std::clamp)(fwdProj, Aim::kAimRayMinDist, Aim::kAimRayMaxDist);
            }
        }
    }
    SyncDuckVisualAndWeaponPose();
}

void Player::SyncDuckVisualAndWeaponPose()
{
    if (!object3d_) return;

    using namespace PlayerTuning::Anim;
    Object3d* const limbs[PlayerTuning::kDuckLimbCount] = {
        leftFootObject_.get(), rightFootObject_.get(), leftWingObject_.get(), rightWingObject_.get()
    };
    const Vector4 bodyTint = isDead_ ? PlayerTuning::Visual::kDeadLimbTint : currentBodyColor_;
    for (Object3d* limb : limbs) { if (limb) limb->SetColor(bodyTint); }

    if (isDead_ || isInIntroPose_)
    {
        AttachLimbPreset(
            limbs, isDead_ ? kDeadLimbPoses : kIntroLimbPoses,
            object3d_->GetTranslate(), object3d_->GetScale(),
            DuckPartRig::MakeEulerMatrixXYZ(object3d_->GetRotate()));
        return;
    }

    if (isDodging_)
    {
        const float progress  = 1.0f - (std::clamp)(dodgeTimer_ / kDodgeDuration, 0.0f, 1.0f);
        const float tuckEnv   = std::sin(progress * DuckPartRig::kPi);
        const float landFlare = (progress > kDodgeLandFlareStart)
            ? std::sin(((progress - kDodgeLandFlareStart) / kDodgeLandFlareSpan) * DuckPartRig::kPi)
            : 0.0f;

        object3d_->SetTranslate(drawPos_);
        object3d_->SetScale(kUnitScale);
        object3d_->Update();

        const Matrix4x4 dodgeRot     = DuckPartRig::MakeEulerMatrixXYZ(object3d_->GetRotate());
        const Vector3 footTuckOffset = { 0.0f, kDodgeFootTuckY * tuckEnv, kDodgeFootTuckZ * tuckEnv };
        const float wingFlare        = kDodgeWingTuckFlare * tuckEnv + kDodgeWingLandFlare * landFlare;
        const LimbPoseSpec dodgePoses[PlayerTuning::kDuckLimbCount] = {
            { DuckPartRig::kLeftFootPivot,  footTuckOffset, { kDodgeFootPitch * tuckEnv, -kDodgeFootToeYaw, 0.0f } },
            { DuckPartRig::kRightFootPivot, footTuckOffset, { kDodgeFootPitch * tuckEnv,  kDodgeFootToeYaw, 0.0f } },
            { DuckPartRig::kLeftWingPivot,  kZeroVec3,      { kDodgeWingPitch * tuckEnv,  0.0f, -wingFlare } },
            { DuckPartRig::kRightWingPivot, kZeroVec3,      { kDodgeWingPitch * tuckEnv,  0.0f,  wingFlare } },
        };
        AttachLimbPreset(limbs, dodgePoses, drawPos_, kUnitScale, dodgeRot);
        SyncRifleAndMagazinePose();
        return;
    }

    // 地上ロコモーション（待機呼吸・よちよち歩行・[C] 銃収納全力ダッシュ）
    const float breathWave = std::sin(idleAnimTime_ * kBreathFreq);
    const float idleWeight = 1.0f - walkBlend_;
    const float idleBobY   = (breathWave * 0.5f + 0.5f) * kIdleBobAmp * idleWeight;
    const float idleSquash = breathWave * kIdleSquashAmp * idleWeight;

    const float hopAmp     = kHopAmpBase    + kHopAmpSprintAdd    * sprintBlend_;
    const float waddleAmp  = kWaddleAmpBase + kWaddleAmpSprintAdd * sprintBlend_;
    const float leanAmp    = kLeanAmpBase   + kLeanAmpSprintAdd   * sprintBlend_;
    const float hopSin     = std::abs(std::sin(walkPhase_));
    const float walkBobY   = hopSin * hopAmp * walkBlend_;
    const float stepSquash = std::cos(walkPhase_ * 2.0f) * (kStepSquashBase + kStepSquashSprintAdd * sprintBlend_) * walkBlend_;

    const float scaleXZ     = 1.0f + stepSquash * kScaleXZStepSquashRate + idleSquash;
    const float scaleY      = 1.0f - stepSquash * kScaleYStepSquashRate  - idleSquash * kScaleYIdleSquashRate;
    const Vector3 bodyScale = { scaleXZ, scaleY, scaleXZ };

    const float localPitchX = smoothedLocalMoveFwd_ * leanAmp * walkBlend_ - rifleRecoil_ * kBodyRecoilPitchBack + (isHealing_ ? kHealingBodyPitchAdd : 0.0f);
    const float localRollZ  = -std::sin(walkPhase_) * waddleAmp * walkBlend_ - smoothedLocalMoveRight_ * (kBodyStrafeRollRate * walkBlend_);

    const Matrix4x4 bodyRotMat = DuckPartRig::MakeLocalPitchRollThenYawMatrix(localPitchX, localRollZ, aimYaw_);
    const Vector3 bodyPos      = { position_.x, position_.y + idleBobY + walkBobY, position_.z };
    SetObjectTransform(object3d_.get(), bodyPos, DuckPartRig::DecomposeRotationToEulerXYZ(bodyRotMat), bodyScale, true);

    // 左右の水かき足IKソルバ
    const float strideLen = kVisualStrideBase + kStrideSprintAdd   * sprintBlend_;
    const float liftH     = kFootLiftBase     + kFootLiftSprintAdd * sprintBlend_;
    DuckPartRig::SolveGroundedFoot(leftFootObject_.get(),  DuckPartRig::kLeftFootPivot,  -1.0f, walkPhase_,                    walkBlend_, strideLen, liftH, smoothedLocalMoveRight_, smoothedLocalMoveFwd_, position_, aimYaw_, 1.0f);
    DuckPartRig::SolveGroundedFoot(rightFootObject_.get(), DuckPartRig::kRightFootPivot,  1.0f, walkPhase_ + DuckPartRig::kPi, walkBlend_, strideLen, liftH, smoothedLocalMoveRight_, smoothedLocalMoveFwd_, position_, aimYaw_, 1.0f);

    // 左右の翼アニメーション（歩行スイング・ダッシュ羽ばたき・リロード/回復アクション）
    const float nonSprint  = 1.0f - sprintBlend_;
    const float sprintFlap = (kSprintFlapBase + kSprintFlapWave * std::abs(std::sin(walkPhase_ * kSprintFlapFreqMult))) * sprintBlend_;
    const float cosWalk    = std::cos(walkPhase_);

    float leftFlapOut  = (kLeftIdleFlapBase  + kLeftIdleFlapBreath  * breathWave) * idleWeight + (kLeftWalkFlapBase  + kLeftWalkFlapHop  * hopSin) * walkBlend_ * nonSprint + sprintFlap;
    float rightFlapOut = (kRightIdleFlapBase + kRightIdleFlapBreath * breathWave) * idleWeight + (kRightWalkFlapBase + kRightWalkFlapHop * hopSin) * walkBlend_ * nonSprint + sprintFlap;
    float leftPitchX   = kLeftWalkPitchAmp * cosWalk * walkBlend_ * (1.0f - kLeftWalkSprintDamp * sprintBlend_) + kSprintWingPitchAmp * cosWalk * sprintBlend_;
    float rightPitchX  = kRightHoldRiflePitch * nonSprint - kSprintWingPitchAmp * cosWalk * sprintBlend_;
    float leftYawY     = 0.0f;
    float rightYawY    = 0.0f;

    if (isReloading_)
    {
        const float cantEnv = ComputeReloadCantEnvelope((std::clamp)(GetReloadProgress(), 0.0f, 1.0f));
        leftPitchX  = leftPitchX * (1.0f - cantEnv) + kReloadLeftWingPitch * cantEnv;
        leftYawY    = kReloadLeftWingYaw * cantEnv;
        leftFlapOut = (std::max)(leftFlapOut, kReloadLeftWingMinFlap * cantEnv);
    }
    else if (isHealing_)
    {
        const float pat = std::sin(idleAnimTime_ * kHealPatFreq) * kHealPatAmp;
        leftPitchX  = kHealWingPitchBase + pat;
        rightPitchX = kHealWingPitchBase - pat;
        leftYawY    = -kHealWingYawAbs;
        rightYawY   =  kHealWingYawAbs;
        leftFlapOut = rightFlapOut = kHealWingFlapOut;
    }

    DuckPartRig::AttachLimbToBody(leftWingObject_.get(),  DuckPartRig::kLeftWingPivot,  kZeroVec3, { leftPitchX,  leftYawY,  -leftFlapOut  }, bodyPos, bodyScale, bodyRotMat);
    DuckPartRig::AttachLimbToBody(rightWingObject_.get(), DuckPartRig::kRightWingPivot, kZeroVec3, { rightPitchX, rightYawY,  rightFlapOut }, bodyPos, bodyScale, bodyRotMat);

    SyncRifleAndMagazinePose();
}

Vector3 Player::GetMagwellWorldPosition() const
{
    using namespace PlayerTuning::Weapon;
    const float cp = std::cos(rifleRotation_.x);
    const float sp = std::sin(rifleRotation_.x);
    const float cy = std::cos(rifleRotation_.y);
    const float sy = std::sin(rifleRotation_.y);
    const float y1 = kMagwellLocalY * cp - kMagwellLocalZ * sp;
    const float z1 = kMagwellLocalY * sp + kMagwellLocalZ * cp;
    return { riflePosition_.x + z1 * sy, riflePosition_.y + y1, riflePosition_.z + z1 * cy };
}

void Player::ApplyWeaponTransforms(const Vector3& riflePos, const Vector3& rifleRot, const Vector3& magPos, const Vector3& magRot, const Vector3& scale)
{
    riflePosition_ = riflePos;
    rifleRotation_ = rifleRot;
    SetObjectTransform(rifleObject_.get(),    riflePosition_, rifleRotation_, scale, true);
    SetObjectTransform(rifleMagObject_.get(), magPos,         magRot,         scale, true);
}

void Player::SyncRifleAndMagazinePose()
{
    if (!object3d_ || !rifleObject_) return;

    using namespace PlayerTuning::Weapon;
    using namespace PlayerTuning::ReloadAnim;

    const Vector3 duckPos       = isDodging_ ? drawPos_ : position_;
    const Vector3 bodyVisualPos = object3d_->GetTranslate();
    const Vector3 bodyVisualRot = object3d_->GetRotate();
    const float yaw             = isDodging_ ? bodyVisualRot.y : aimYaw_;

    const Vector3 fwd   = { std::sin(yaw), 0.0f, std::cos(yaw) };
    const Vector3 right = { std::cos(yaw), 0.0f, -std::sin(yaw) };
    const Vector3 up    = { 0.0f, 1.0f, 0.0f };

    rifleMagVisible_  = true;
    Vector3 targetPos = { 0.0f, 0.0f, 0.0f };
    Vector3 targetRot = { 0.0f, 0.0f, 0.0f };
    Vector3 magPos    = { 0.0f, 0.0f, 0.0f };
    Vector3 magRot    = { 0.0f, 0.0f, 0.0f };

    if (isWeaponHolstered_)
    {
        // [C] 銃収納ダッシュ中：背中に斜め掛けスリング保持
        const Matrix4x4 bodyRotMat = DuckPartRig::MakeEulerMatrixXYZ(bodyVisualRot);
        targetPos = bodyVisualPos + DuckPartRig::TransformVec3ByRotation(kHolsterSlingOffset, bodyRotMat);
        targetRot = DuckPartRig::DecomposeRotationToEulerXYZ(Multiply(DuckPartRig::MakeEulerMatrixXYZ(kHolsterSlingEuler), bodyRotMat));
        magPos = targetPos;
        magRot = targetRot;
    }
    else if (isDodging_ || !isReloading_)
    {
        // 回避中または通常射撃構え姿勢（レーザーサイト・弾道原点と完全同期）
        const float fwdOffset = kHipAimOffset.z - rifleRecoil_ * kRifleRecoilFwdKick;
        targetPos = duckPos + right * kHipAimOffset.x + Vector3{ 0.0f, kHipAimOffset.y, 0.0f } + fwd * fwdOffset;
        targetRot = isDodging_ ? bodyVisualRot : Vector3{ -rifleRecoil_ * kRifleRecoilPitchKick, yaw, 0.0f };
        magPos = targetPos;
        magRot = targetRot;
    }
    else
    {
        // 3段階タクティカル/ドライ・リロードアニメーション
        const float p           = (std::clamp)(GetReloadProgress(), 0.0f, 1.0f);
        const float cantEnv     = ComputeReloadCantEnvelope(p);
        const float rifleYawOff = kCantYawOffset * cantEnv;
        float riflePitchOff     = kCantPitchOffset * cantEnv;
        float liftY             = kCantLiftY * cantEnv + (bodyVisualPos.y - position_.y) * kBodyBobFollow;
        const float rightPull   = kCantRightPull * cantEnv;
        float fwdPull           = kCantFwdPull * cantEnv;

        if (p >= kTapStart && p <= kTapEnd)
        {
            const float tapWave = std::sin(((p - kTapStart) / kTapSpan) * DuckPartRig::kPi);
            liftY         += kTapLiftY * tapWave;
            riflePitchOff -= kTapPitchDown * tapWave;
        }

        if (wasDryReload_ && p >= kDryRackStart && p <= kDryRackEnd)
        {
            if (p < kDrySlamStart)
            {
                const float rackBack = SmoothStep01((p - kDryRackStart) / kDryRackSpan);
                fwdPull       -= kDryRackFwdPull * rackBack;
                riflePitchOff -= kDryRackPitchUp * rackBack;
            }
            else
            {
                const float slamFwd = 1.0f - SmoothStep01((p - kDrySlamStart) / kDrySlamSpan);
                fwdPull -= kDryRackFwdPull * slamFwd - kDrySlamOvertravel * (1.0f - slamFwd);
            }
        }
        else if (!wasDryReload_ && p >= kTacSnapStart && p <= kTacSnapEnd)
        {
            fwdPull += kTacSnapFwdPush * std::sin(((p - kTacSnapStart) / kTacSnapSpan) * DuckPartRig::kPi);
        }

        targetPos = duckPos + right * (kHipAimOffset.x + rightPull) + up * (kHipAimOffset.y + liftY) + fwd * (kHipAimOffset.z + fwdPull);
        targetRot = { riflePitchOff, yaw + rifleYawOff, 0.0f };

        const float rPitch        = targetRot.x;
        const float rYaw          = targetRot.y;
        const Vector3 rFwd        = { std::sin(rYaw), 0.0f, std::cos(rYaw) };
        const Vector3 rRight      = { std::cos(rYaw), 0.0f, -std::sin(rYaw) };
        const Vector3 magAxisDown = { -std::sin(rPitch) * rFwd.x, -std::cos(rPitch), -std::sin(rPitch) * rFwd.z };

        magPos = targetPos;
        magRot = targetRot;
        if (p >= kMagReleaseEnd && p < kMagDropEnd)
        {
            const float dropT = (p - kMagReleaseEnd) / kMagDropSpan;
            magPos = targetPos + magAxisDown * (dropT * dropT * kMagDropSlideDist) + rFwd * (kMagDropFwdDrift * dropT);
            magRot = { rPitch + kMagDropPitchTilt * dropT, rYaw, 0.0f };
        }
        else if (p >= kMagDropEnd && p < kMagEmptyEnd)
        {
            rifleMagVisible_ = false;
        }
        else if (p >= kMagEmptyEnd && p < kMagInsertEnd)
        {
            const float invT = 1.0f - SmoothStep01((p - kMagEmptyEnd) / kMagInsertSpan);
            magPos = targetPos + magAxisDown * (kMagPouchDown * invT) - rRight * (kMagPouchLeft * invT) - rFwd * (kMagPouchBack * invT);
            magRot = { rPitch + kMagInsertPitch * invT, rYaw - kMagInsertYaw * invT, 0.0f };
        }
    }

    ApplyWeaponTransforms(targetPos, targetRot, magPos, magRot, kUnitScale);
}

void Player::PostCollisionUpdate()
{
    position_.y = 0.0f;
    if (!object3d_) return;
    if (isDodging_) { drawPos_.x = position_.x; drawPos_.z = position_.z; }
    SyncDuckVisualAndWeaponPose();
}

void Player::SetIntroTransform(const Vector3& offset, const Vector3& scale, const Vector3& rotate)
{
    if (!object3d_) return;

    isInIntroPose_ = true;
    aimYaw_ = rotate.y;

    const Vector3 p = position_ + offset;
    SetObjectTransform(object3d_.get(), p, rotate, scale, true);
    SyncDuckVisualAndWeaponPose();

    if (rifleObject_)
    {
        using namespace PlayerTuning::Weapon;
        const Vector3 localOff = { kHipAimOffset.x * scale.x, kHipAimOffset.y * scale.y, kHipAimOffset.z * scale.z };
        const Vector3 worldPos = p + DuckPartRig::TransformVec3ByRotation(localOff, DuckPartRig::MakeEulerMatrixXYZ(rotate));
        rifleMagVisible_ = true;
        ApplyWeaponTransforms(worldPos, rotate, worldPos, rotate, scale);
    }
}

void Player::RestoreIntroPose()
{
    if (!object3d_) return;

    isInIntroPose_ = false;
    aimYaw_ = walkBlend_ = sprintBlend_ = 0.0f;
    walkPhase_ = PlayerTuning::Anim::kInitialWalkPhase;
    SetObjectTransform(object3d_.get(), position_, kZeroVec3, kUnitScale, true);
    prevSpacePressed_ = true;               // イントロ終了・スキップ直後のSPACEキー暴発を抑止
    suppressShootUntilMouseRelease_ = true; // スタート・スキップ時の左クリック暴発を抑止
    isWeaponHolstered_ = prevCPressed_ = false;
    SyncDuckVisualAndWeaponPose();
}

// =============================================================================
// 射撃 & アクション (Combat & Action)
// =============================================================================

std::vector<std::unique_ptr<Bullet>> Player::TryShoot(const MouseInput* mouseInput, float deltaTime)
{
    using namespace PlayerTuning::Audio;
    std::vector<std::unique_ptr<Bullet>> bullets;
    if (shotCooldownTimer_ > 0.0f) shotCooldownTimer_ = (std::max)(0.0f, shotCooldownTimer_ - deltaTime);
    if (dryFireRepeatTimer_ > 0.0f) dryFireRepeatTimer_ = (std::max)(0.0f, dryFireRepeatTimer_ - deltaTime);

    if (isDead_ || isDodging_ || !object3d_ || !object3dCom_ || !camera_ || !mouseInput || (!demoCtrl_.active && AppDebugState::IsImGuiCapturingMouse()))
    {
        return bullets;
    }

    // スタート・演出スキップ時のマウス左クリック押しっぱなしによる初弾暴発を防止
    if (demoCtrl_.active)
    {
        suppressShootUntilMouseRelease_ = false;
    }
    else if (suppressShootUntilMouseRelease_)
    {
        const bool osLeftDown = (GetAsyncKeyState(VK_LBUTTON) & kUIAsyncKeyDownMask) != 0;
        if (mouseInput->PushButton(0) || osLeftDown) return bullets;
        suppressShootUntilMouseRelease_ = false;
    }

    const bool trigFire = demoCtrl_.active ? (demoCtrl_.fireHeld && shotCooldownTimer_ <= 0.0f) : mouseInput->TriggerButton(0);
    const bool holdFire = demoCtrl_.active ? demoCtrl_.fireHeld : mouseInput->PushButton(0);

    // リロード中に左クリックした場合はリロード継続のままインジケーターを強調表示
    if (isReloading_)
    {
        if (!demoCtrl_.active && trigFire)
        {
            emptyTriggerAlertTimer_ = kEmptyTriggerAlertDuration;
            TacticalAudio::Play2D("Resources/Audio/SE_Gun_DryFire.wav", kReloadBusyClickVolume, kReloadBusyClickCooldownMs, "reload_busy");
        }
        return bullets;
    }

    // 弾切れ状態：単発クリック時だけでなくフルオート押しっぱなし中も周期的に空撃ち警告音＋UIシェイクを発動
    if (magazineAmmo_ <= 0)
    {
        if (trigFire)
        {
            isWeaponHolstered_ = false;
            emptyTriggerAlertTimer_ = kEmptyTriggerAlertDuration;
            dryFireRepeatTimer_ = kHeldDryFireIntervalSec;
            TacticalAudio::Play2D("Resources/Audio/SE_Ammo_EmptyAlert.wav", kDryFireVolume, kDryFireCooldownMs, "player_dryfire");
        }
        else if (holdFire)
        {
            if (shotCooldownTimer_ <= 0.0f && dryFireRepeatTimer_ <= 0.0f)
            {
                emptyTriggerAlertTimer_ = kEmptyTriggerAlertDuration;
                dryFireRepeatTimer_ = kHeldDryFireIntervalSec;
                TacticalAudio::Play2D("Resources/Audio/SE_Ammo_EmptyAlert.wav", kDryFireVolume, kDryFireCooldownMs, "player_dryfire");
            }
        }
        else
        {
            dryFireRepeatTimer_ = 0.0f;
        }
        return bullets;
    }

    if (holdFire && shotCooldownTimer_ <= 0.0f)
    {
        isWeaponHolstered_ = false;
        SyncAimWithCamera(mouseInput);

        const float finalYaw  = GetRotation().y;
        const Vector3 forward = { std::sin(finalYaw), 0.0f, std::cos(finalYaw) };
        auto bullet = std::make_unique<Bullet>();
        bullet->Initialize(object3dCom_, camera_, GetMuzzleWorldPosition(), forward, kBulletSpeed, kBulletLifeTime, BulletOwner::Player);
        bullets.push_back(std::move(bullet));

        magazineAmmo_--;
        shotCooldownTimer_ = kShotCooldown;
        rifleRecoil_ = 1.0f;
        RaidStats::GetInstance().shotsFired++;

        // 最終弾発射時(1 -> 0)：ボルトストップ金属音＋弾切れアラートを即時再生、残弾わずか(1..4)では警告ティックを重ねる
        if (magazineAmmo_ <= 0)
        {
            emptyTriggerAlertTimer_ = kEmptyTriggerAlertDuration;
            dryFireRepeatTimer_ = kHeldDryFireIntervalSec;
            TacticalAudio::Play2D("Resources/Audio/SE_Ammo_Empty.wav", kEmptyMagAlertVolume, kEmptyMagAlertCooldownMs, "ammo_empty");
        }
        else if (magazineAmmo_ <= kLowAmmoSoundThreshold)
        {
            const float lowVol = kLowAmmoTickVolumeBase + static_cast<float>(kLowAmmoSoundThreshold - magazineAmmo_) * kLowAmmoTickVolumeStep;
            TacticalAudio::Play2D("Resources/Audio/SE_Ammo_LowTick.wav", lowVol, kLowAmmoTickCooldownMs, "ammo_low_tick");
        }

        ApplyReticleRecoilKick();
        currentSpread_ = (std::min)(kMaxSpread, currentSpread_ + kShootSpreadPenalty);
    }
    return bullets;
}

bool Player::StartHealing()
{
    if (isDead_ || isDodging_ || isHealing_ || medkitCount_ <= 0 || hp_ >= maxHp_) return false;
    isHealing_ = true;
    healTimer_ = kHealDuration;
    TacticalAudio::Play2D("Resources/Audio/SE_Heal_Start.wav", PlayerTuning::Audio::kHealStartVolume);
    return true;
}

void Player::TakeDamage(float damage, const std::string& attackerName, const std::string& cause, const Vector3* attackerPos)
{
    if (isDead_) return;
    // 即死イベント（1周目デモ演出やデバッグF8など damage >= 1000）は無敵時間・回避中でも確実に発動させる
    if (damage < 1000.0f && (invincibilityTimer_ > 0.0f || isDodging_)) return;
    if (damage >= 1000.0f)
    {
        isDodging_ = false;
        dodgeTimer_ = 0.0f;
    }

    hp_ -= damage;
    // F7自動デモプレイ中は実演が途中敗北で途切れないようHP下限を保護（救急キット回復パーティクル実演へ繋げる）
    if (demoCtrl_.active && damage < 1000.0f && hp_ < 28.0f)
    {
        hp_ = 28.0f;
    }
    RaidStats::GetInstance().damageTakenCount++;
    if (attackerPos) { lastAttackerPosition_ = *attackerPos; hasAttackerPosition_ = true; }

    if (hp_ <= 0.0f)
    {
        hp_ = hitFlashTimer_ = invincibilityTimer_ = 0.0f;
        isDead_ = true;
        lastAttackerName_ = attackerName;
        causeOfDeath_ = cause;
        RaidStats::GetInstance().causeOfDeath = cause;
        RaidStats::GetInstance().killerName = attackerName;
        currentBodyColor_ = PlayerTuning::Visual::kDeadBodyDimColor;
        if (object3d_) object3d_->SetColor(currentBodyColor_);
    }
    else
    {
        invincibilityTimer_ = kInvincibilityDuration;
        hitFlashTimer_ = kHitFlashDuration;
        currentBodyColor_ = PlayerTuning::Visual::kHitInitialFlashColor;
        object3d_->SetColor(currentBodyColor_);
    }
}

void Player::UpdateSpread(float deltaTime, bool isMoving)
{
    const float targetSpread = kBaseSpread + (isMoving ? kMoveSpreadPenalty : 0.0f);
    currentSpread_ = (currentSpread_ > targetSpread)
        ? (std::max)(targetSpread, currentSpread_ - kSpreadRecoverRate * deltaTime)
        : (std::min)(targetSpread, currentSpread_ + kSpreadRecoverRate * 2.0f * deltaTime);
    currentSpread_ = (std::min)(kMaxSpread, currentSpread_);
}

void Player::UpdateReticleRecoil(float deltaTime)
{
    using namespace PlayerTuning::Aim;
    const float clampedDt = (std::clamp)(deltaTime, 0.0f, PlayerTuning::kMaxPhysicsStepDt);
    const int steps       = (clampedDt > kRecoilSubstepThresholdDt) ? 2 : 1;
    const float stepDt    = clampedDt / static_cast<float>(steps);

    for (int i = 0; i < steps; ++i)
    {
        const Vector2 accel = {
            -GameConfig::Combat::kReticleSpringStiffness * reticleRecoilOffset_.x - GameConfig::Combat::kReticleSpringDamping * reticleRecoilVel_.x,
            -GameConfig::Combat::kReticleSpringStiffness * reticleRecoilOffset_.y - GameConfig::Combat::kReticleSpringDamping * reticleRecoilVel_.y
        };
        reticleRecoilVel_.x    += accel.x * stepDt;
        reticleRecoilVel_.y    += accel.y * stepDt;
        reticleRecoilOffset_.x += reticleRecoilVel_.x * stepDt;
        reticleRecoilOffset_.y += reticleRecoilVel_.y * stepDt;
    }

    const float maxR = GameConfig::Combat::kReticleMaxOffsetPx;
    const float len  = std::sqrt(reticleRecoilOffset_.x * reticleRecoilOffset_.x + reticleRecoilOffset_.y * reticleRecoilOffset_.y);
    if (len > maxR && len > PlayerTuning::kSmallLenEpsilon)
    {
        const float s = maxR / len;
        reticleRecoilOffset_.x *= s;
        reticleRecoilOffset_.y *= s;
        const float nx     = reticleRecoilOffset_.x / maxR;
        const float ny     = reticleRecoilOffset_.y / maxR;
        const float outVel = reticleRecoilVel_.x * nx + reticleRecoilVel_.y * ny;
        if (outVel > 0.0f)
        {
            reticleRecoilVel_.x -= nx * outVel * kRecoilBoundaryVelDamping;
            reticleRecoilVel_.y -= ny * outVel * kRecoilBoundaryVelDamping;
        }
    }
    else if (len < kRecoilRestOffsetPx && (reticleRecoilVel_.x * reticleRecoilVel_.x + reticleRecoilVel_.y * reticleRecoilVel_.y) < 1.0f)
    {
        reticleRecoilOffset_ = { 0.0f, 0.0f };
        reticleRecoilVel_    = { 0.0f, 0.0f };
    }

    if (shotCooldownTimer_ <= kRecoilCoolReadyTime) recoilHeat_ = (std::max)(0.0f, recoilHeat_ - kRecoilHeatCoolRate * clampedDt);
    if (recoilKickVisual_ > 0.0f) recoilKickVisual_ = (std::max)(0.0f, recoilKickVisual_ - kRecoilKickVisualDecay * clampedDt);
    recoilKickVisual_ = (std::max)(recoilKickVisual_, (std::clamp)(len / kRecoilVisualNormOffsetPx, 0.0f, 1.0f));
}

void Player::ApplyReticleRecoilKick()
{
    using namespace PlayerTuning;
    using namespace PlayerTuning::Aim;

    const Vector2 clientSize = ResolveClientSize(
        static_cast<float>(WindowAPI::GetClientWidth()),
        static_cast<float>(WindowAPI::GetClientHeight()));

    Vector2 aimDir2D = { 0.0f, -1.0f };
    if (camera_)
    {
        const Matrix4x4& vp  = camera_->GetViewProjectionMatrix();
        const Vector3 anchor = { position_.x, position_.y + bulletSpawnOffset_.y, position_.z };
        const float w = anchor.x * vp.m[0][kMatW] + anchor.y * vp.m[1][kMatW] + anchor.z * vp.m[2][kMatW] + vp.m[kMatW][kMatW];
        if (w > kSmallLenEpsilon)
        {
            const float clipX    = (anchor.x * vp.m[0][0] + anchor.y * vp.m[1][0] + anchor.z * vp.m[2][0] + vp.m[kMatW][0]) / w;
            const float clipY    = (anchor.x * vp.m[0][1] + anchor.y * vp.m[1][1] + anchor.z * vp.m[2][1] + vp.m[kMatW][1]) / w;
            const float dx       = effectiveReticleScreenPos_.x - (clipX + 1.0f) * 0.5f * clientSize.x;
            const float dy       = effectiveReticleScreenPos_.y - (1.0f - clipY) * 0.5f * clientSize.y;
            const float dLen     = std::sqrt(dx * dx + dy * dy);
            if (dLen > 2.0f) aimDir2D = { dx / dLen, dy / dLen };
        }
    }

    Vector2 climbDir     = { aimDir2D.x * kClimbAimFollowX, kClimbBaseScreenUpY + aimDir2D.y * kClimbAimFollowY };
    const float climbLen = std::sqrt(climbDir.x * climbDir.x + climbDir.y * climbDir.y);
    climbDir = (climbLen > kSmallLenEpsilon) ? Vector2{ climbDir.x / climbLen, climbDir.y / climbLen } : Vector2{ 0.0f, -1.0f };

    const Vector2 perpDir = { -aimDir2D.y, aimDir2D.x };
    if (recoilHeat_ <= kSideFlipGuaranteedHeat || RandRecoil01() > kSideKeepProb) recoilSideSign_ = -recoilSideSign_;
    const float side     = static_cast<float>(recoilSideSign_);
    const float moveMult = isMoving_ ? kRecoilMovingMultiplier : 1.0f;
    const float jitter   = RandRecoilRange(kRecoilJitterMin, kRecoilJitterMax);

    const float climbSnap = (GameConfig::Combat::kReticleClimbSnapPx + kHeatClimbSnapAdd * recoilHeat_) * moveMult;
    const float latSnap   = side * (GameConfig::Combat::kReticleLateralSnapPx + kHeatLatSnapAdd * recoilHeat_) * jitter * moveMult;
    const float climbVel  = (GameConfig::Combat::kReticleClimbVelPx  + kHeatClimbVelAdd  * recoilHeat_) * moveMult;
    const float latVel    = side * (GameConfig::Combat::kReticleLateralVelPx  + kHeatLatVelAdd  * recoilHeat_) * jitter * moveMult;

    reticleRecoilOffset_.x += climbDir.x * climbSnap + perpDir.x * latSnap;
    reticleRecoilOffset_.y += climbDir.y * climbSnap + perpDir.y * latSnap;
    reticleRecoilVel_.x    += climbDir.x * climbVel  + perpDir.x * latVel;
    reticleRecoilVel_.y    += climbDir.y * climbVel  + perpDir.y * latVel;

    recoilHeat_ = (std::min)(1.0f, recoilHeat_ + kRecoilHeatPerShot);
    recoilKickVisual_ = 1.0f;
}

// =============================================================================
// 描画 (Draw)
// =============================================================================

void Player::Draw(const RenderContext& ctx)
{
    if (!object3dCom_ || !object3d_) return;
    using namespace PlayerTuning::Visual;

    if (isDead_) SyncDuckVisualAndWeaponPose();

    auto makePartContext = [&](uint32_t texIndex) -> RenderContext
    {
        RenderContext partCtx = ctx;
        if (texIndex != TextureManager::kInvalidTextureIndex)
        {
            partCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(texIndex);
        }
        return partCtx;
    };

    auto drawPart = [&](Object3d* obj, const RenderContext& partCtx)
    {
        if (!obj || obj->GetModelData().vertices.empty()) return;
        if (ctx.camera) obj->SetCamera(ctx.camera);
        obj->Update();
        object3dCom_->Draw(obj, partCtx, obj->GetModelData(), true);
    };

    const Object3d::ModelData& modelData = object3d_->GetModelData();
    const uint32_t bodyTexIdx = (defaultTextureIndex_ != TextureManager::kInvalidTextureIndex)
        ? defaultTextureIndex_
        : modelData.material.textureIndex;
    const RenderContext playerCtx = makePartContext(bodyTexIdx);

    // 被弾時のノックバック・震動シェイク＆仰け反り（Flinch）演出
    const Vector3 originalPos = object3d_->GetTranslate();
    const Vector3 originalRot = object3d_->GetRotate();
    Vector3 shakeDelta        = kZeroVec3;
    if (hitFlashTimer_ > 0.0f)
    {
        const float ratio          = hitFlashTimer_ / kHitFlashDuration;
        const float shakeIntensity = kHitShakeIntensity * ratio;
        const float rx             = (RandRecoil01() * 2.0f - 1.0f) * shakeIntensity;
        const float rz             = (RandRecoil01() * 2.0f - 1.0f) * shakeIntensity;
        shakeDelta = { rx, kHitShakeLiftY * ratio, rz };
        object3d_->SetTranslate(originalPos + shakeDelta);

        const Matrix4x4 flinchMat = Multiply(MakeRotateXMatrix(kHitFlinchPitch * ratio), DuckPartRig::MakeEulerMatrixXYZ(originalRot));
        object3d_->SetRotate(DuckPartRig::DecomposeRotationToEulerXYZ(flinchMat));
    }

    const Vector4 bodyColor = isDead_ ? kDeadLimbTint : currentBodyColor_;
    Object3d* limbs[PlayerTuning::kDuckLimbCount] = {
        leftFootObject_.get(), rightFootObject_.get(), leftWingObject_.get(), rightWingObject_.get()
    };
    Vector3 origLimbPos[PlayerTuning::kDuckLimbCount]{};
    for (int i = 0; i < PlayerTuning::kDuckLimbCount; ++i)
    {
        if (!limbs[i]) continue;
        origLimbPos[i] = limbs[i]->GetTranslate();
        if (hitFlashTimer_ > 0.0f) limbs[i]->SetTranslate(origLimbPos[i] + shakeDelta);
        limbs[i]->SetColor(bodyColor);
    }

    drawPart(object3d_.get(), playerCtx);
    for (Object3d* limb : limbs) drawPart(limb, playerCtx);

    if (!isDead_ && rifleObject_)
    {
        drawPart(rifleObject_.get(), makePartContext(rifleTextureIndex_));
        if (rifleMagObject_ && rifleMagVisible_) drawPart(rifleMagObject_.get(), makePartContext(rifleMagTextureIndex_));
    }

    if (hitFlashTimer_ > 0.0f)
    {
        SetObjectTransform(object3d_.get(), originalPos, originalRot, object3d_->GetScale(), true);
        for (int i = 0; i < PlayerTuning::kDuckLimbCount; ++i)
        {
            if (limbs[i]) { limbs[i]->SetTranslate(origLimbPos[i]); limbs[i]->Update(); }
        }
    }
}
