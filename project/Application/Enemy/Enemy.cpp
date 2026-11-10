#include "Enemy.h"
#include "../Player/DuckPartRig.h"
#include "Object3d.h"
#include "Object3dCom.h"
#include "TextureManager.h"
#include "RenderContext.h"
#include "Matrix4x4.h"

#include "WindowsAPI.h"
#include "Sprite.h"
#include "Bullet.h"
#include "Obstacle.h"
#include "Baziru3_Engine/Framework/AI/NavMesh.h"
#include "Baziru3_Engine/Framework/Collision/CollisionManager.h"
#include "Baziru3_Engine/Framework/Collision/SphereCollider.h"
#include "Baziru3_Engine/Framework/Collision/BoxCollider.h"
#include "Baziru3_Engine/Framework/Collision/CapsuleCollider.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace
{
    using namespace EnemyTuning;

    inline float NormalizeAngle(float a)
    {
        return (std::abs(a) <= kPi) ? a : std::remainder(a, kTwoPi);
    }
}

bool Enemy::IsWalkableArenaPos(const Vector3& pos, float agentRadius)
{
    if (pos.x < kArenaMinX || pos.x > kArenaMaxX || pos.z < kArenaMinZ || pos.z > kArenaMaxZ)
    {
        return false;
    }
    // 川（東西2つの木造トラス橋 X = -14.0 / +14.0 以外で Z: 27.3..32.7）への落下防止
    const bool onBridge = (std::abs(pos.x + kBridgeCenterAbsX) <= kBridgeHalfWidth) ||
                          (std::abs(pos.x - kBridgeCenterAbsX) <= kBridgeHalfWidth);
    if (pos.z > kRiverMinZ && pos.z < kRiverMaxZ && !onBridge)
    {
        return false;
    }
    return BaziruEngine::AI::NavMesh::GetInstance()->IsWalkable(pos, agentRadius);
}

void Enemy::BuildEnemyRifleModel(Object3d::ModelData& data)
{
    data.vertices.clear();
    data.indices.clear();

    constexpr Vector3 kNormals[kBoxFaceCount] = {
        { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f },
        { 1.0f, 0.0f,  0.0f }, { 0.0f, -1.0f, 0.0f }, {  0.0f, 1.0f, 0.0f }
    };
    constexpr int kFaces[kBoxFaceCount][kBoxFaceVertCount] = {
        { 0, 3, 2, 1 }, { 4, 5, 6, 7 }, { 0, 4, 7, 3 },
        { 1, 2, 6, 5 }, { 0, 1, 5, 4 }, { 3, 7, 6, 2 }
    };

    constexpr size_t kBoxCount = sizeof(kEnemyRifleBoxes) / sizeof(kEnemyRifleBoxes[0]);
    data.vertices.reserve(kBoxCount * kBoxFaceCount * kBoxFaceVertCount);
    data.indices.reserve(kBoxCount * kBoxFaceCount * kBoxFaceCount);

    for (const auto& box : kEnemyRifleBoxes)
    {
        const Vector3& minP = box.minP;
        const Vector3& maxP = box.maxP;
        const Vector3 corners[kBoxCornerCount] = {
            { minP.x, minP.y, minP.z }, { maxP.x, minP.y, minP.z },
            { maxP.x, maxP.y, minP.z }, { minP.x, maxP.y, minP.z },
            { minP.x, minP.y, maxP.z }, { maxP.x, minP.y, maxP.z },
            { maxP.x, maxP.y, maxP.z }, { minP.x, maxP.y, maxP.z },
        };

        for (size_t f = 0; f < kBoxFaceCount; ++f)
        {
            const uint32_t faceStart = static_cast<uint32_t>(data.vertices.size());
            for (size_t v = 0; v < kBoxFaceVertCount; ++v)
            {
                const Vector3& c = corners[kFaces[f][v]];
                Sprite::VertexData vert{};
                vert.position = { c.x, c.y, c.z, 1.0f };
                vert.texcoord = { (v == 1 || v == 2) ? 1.0f : 0.0f, (v >= 2) ? 1.0f : 0.0f };
                vert.normal   = kNormals[f];
                data.vertices.push_back(vert);
            }
            data.indices.push_back(faceStart + 0);
            data.indices.push_back(faceStart + 1);
            data.indices.push_back(faceStart + 2);
            data.indices.push_back(faceStart + 0);
            data.indices.push_back(faceStart + 2);
            data.indices.push_back(faceStart + kBoxLastCorner);
        }
    }

    data.boundingRadius = kRifleBoundingRadius;
}

void Enemy::InitializeEnemyRifle()
{
    if (!object3dCom_ || !camera_) return;

    Object3d::ModelData rifleModel{};
    BuildEnemyRifleModel(rifleModel);
    rifleTextureIndex_ = TextureManager::GetInstance()->Load("Resources/container_military.png");
    rifleModel.material.textureFilePath = "Resources/container_military.png";
    rifleModel.material.textureIndex = (rifleTextureIndex_ != TextureManager::kInvalidTextureIndex) ? rifleTextureIndex_ : 0;

    rifleObject_ = std::make_unique<Object3d>();
    rifleObject_->Initialize(object3dCom_, rifleModel);
    rifleObject_->SetCamera(camera_);
    rifleObject_->SetScale({ 1.0f, 1.0f, 1.0f });
    rifleObject_->SetColor(kRifleDefaultColor);
    rifleObject_->SetEnableLighting(true);
    rifleObject_->SetAllowWireframeOverlay(false);
}

void Enemy::Initialize(Object3dCom* object3dCom, Camera* camera)
{
    object3dCom_ = object3dCom;
    camera_ = camera;
    defaultTextureIndex_ = TextureManager::GetInstance()->Load("Resources/duck_enemy.png");

    const DuckPartRig::SplitMeshSet& split = DuckPartRig::GetCachedDuckSplitMeshes();
    auto initEnemyPart = [&](std::unique_ptr<Object3d>& dst, Object3d::ModelData partData)
    {
        partData.material.textureFilePath = "Resources/duck_enemy.png";
        partData.material.textureIndex = defaultTextureIndex_;
        partData.boundingRadius = kPartBoundingRadius;

        dst = std::make_unique<Object3d>();
        dst->Initialize(object3dCom_, partData);
        dst->SetCamera(camera_);
        dst->SetScale({ 1.0f, 1.0f, 1.0f });
        dst->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    };

    initEnemyPart(object3d_, split.body);
    initEnemyPart(leftFootObject_, split.leftFoot);
    initEnemyPart(rightFootObject_, split.rightFoot);
    initEnemyPart(leftWingObject_, split.leftWing);
    initEnemyPart(rightWingObject_, split.rightWing);

    InitializeEnemyRifle();

    position_ = { 0.0f, 0.0f, kInitialSpawnZ };
    prevFramePos_ = position_;
    spawnPosition_ = position_;
    baseYaw_ = kPi;
    currentYaw_ = baseYaw_;
    localMoveRight_ = 0.0f;
    localMoveFwd_ = 1.0f;
    waddlePhase_ = kIdleWaddlePhase;

    object3d_->SetTranslate(position_);
    object3d_->SetRotate({ 0.0f, currentYaw_, 0.0f });
    object3d_->SetScale({ 1.0f, 1.0f, 1.0f });
    object3d_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

    collider_ = std::make_unique<SphereCollider>(kColliderRadius, &position_, CollisionAttribute::Enemy);
    collider_->SetPositionOffset({ 0.0f, kColliderOffsetY, 0.0f });
    CollisionManager::GetInstance()->RegisterCollider(collider_.get());

    hp_ = maxHp_;
    displayedHpRatio_ = 1.0f;
    isDead_ = false;
    justRespawned_ = false;
    respawnTimer_ = 0.0f;
    deathAnimTimer_ = 0.0f;
    shotCooldownTimer_ = kInitialShotCooldown;
    aimTelegraphTimer_ = 0.0f;
    burstShotsRemaining_ = 0;
    ammoInMag_ = maxMagAmmo_;
    reloadTimer_ = 0.0f;

    UpdateVisualAndWeaponPose(0.0f);
}

void Enemy::SetPosition(const Vector3& pos)
{
    position_ = pos;
    prevFramePos_ = pos;
    spawnPosition_ = pos;

    // ステージ両脇の歩哨は中央通路へやや内向きに角度をつけ、十字砲火の警戒網を形成
    float inwardAngle = 0.0f;
    if (pos.x > kSideLaneThresholdX) inwardAngle = kInwardCrossfireAngle;
    else if (pos.x < -kSideLaneThresholdX) inwardAngle = -kInwardCrossfireAngle;

    baseYaw_ = NormalizeAngle(kPi + inwardAngle);
    currentYaw_ = baseYaw_;
    scanPhase_ = std::fmod(std::abs(pos.x * kScanPhaseSeedX + pos.z * kScanPhaseSeedZ), kTwoPi);
    strafeSign_ = (std::sin(pos.x * kStrafeSignSeedX + pos.z * kStrafeSignSeedZ) >= 0.0f) ? 1.0f : -1.0f;
    shotCooldownTimer_ = kShotCdSeedBase + std::fmod(std::abs(pos.x * kShotCdSeedX + pos.z * kShotCdSeedZ), kShotCdSeedMod);

    if (object3d_)
    {
        object3d_->SetTranslate(position_);
        object3d_->SetRotate({ 0.0f, currentYaw_, 0.0f });
    }
    UpdateVisualAndWeaponPose(0.0f);
}

void Enemy::SetBaseYaw(float yaw)
{
    baseYaw_ = NormalizeAngle(yaw);
    currentYaw_ = baseYaw_;
    if (object3d_)
    {
        object3d_->SetRotate({ 0.0f, currentYaw_, 0.0f });
    }
}

void Enemy::SetTacticalRole(TacticalRole role)
{
    role_ = role;
    const size_t idx = static_cast<size_t>(role_);
    const RoleStatsSpec& spec = kRoleStats[(idx < kRoleCount) ? idx : 0];

    maxHp_                = spec.maxHp;
    roleScale_            = spec.roleScale;
    maxSightRange_        = spec.maxSightRange;
    fovAngle_             = spec.fovAngle;
    aimTelegraphDuration_ = spec.aimTelegraphDuration;
    bulletSpeed_          = spec.bulletSpeed;
    shotCooldown_         = spec.shotCooldown;
    burstCountPerTrigger_ = spec.burstCount;
    maxMagAmmo_           = spec.maxMagAmmo;
    baseBodyTint_         = spec.baseBodyTint;

    hp_ = maxHp_;
    displayedHpRatio_ = 1.0f;
    ammoInMag_ = maxMagAmmo_;
    if (object3d_ && hitFlashTimer_ <= 0.0f)
    {
        object3d_->SetColor(baseBodyTint_);
    }
    UpdateVisualAndWeaponPose(0.0f);
}

void Enemy::SyncCameraTransforms()
{
    Object3d* allParts[] = {
        object3d_.get(), leftFootObject_.get(), rightFootObject_.get(),
        leftWingObject_.get(), rightWingObject_.get(), rifleObject_.get()
    };
    for (Object3d* part : allParts)
    {
        if (part) part->Update();
    }
}

void Enemy::ApplySeparation(const Vector3& pushDelta)
{
    if (isDead_) return;
    const Vector3 candidate = { position_.x + pushDelta.x, 0.0f, position_.z + pushDelta.z };
    if (IsWalkableArenaPos(candidate))
    {
        position_ = candidate;
    }
}

float Enemy::GetAimTelegraphProgress() const
{
    if (isDead_ || state_ != AIState::Chase || reloadTimer_ > 0.0f || alertTimer_ > 0.0f || !hasCurrentLOS_)
    {
        return 0.0f;
    }
    if (burstShotsRemaining_ > 0 || readyToFireBurst_)
    {
        return 1.0f;
    }
    if (aimTelegraphTimer_ > 0.0f && aimTelegraphDuration_ > kMinTelegraphDurEps)
    {
        return (std::clamp)(aimTelegraphTimer_ / aimTelegraphDuration_, 0.0f, 1.0f);
    }
    return 0.0f;
}

Vector3 Enemy::GetMuzzleWorldPosition() const
{
    const float s = std::sin(currentYaw_);
    const float c = std::cos(currentYaw_);
    const Vector3 right = { c, 0.0f, -s };
    const Vector3 fwd   = { s, 0.0f,  c };
    return position_ +
           right * (kMuzzleRightOffset * roleScale_) +
           Vector3{ 0.0f, kMuzzleUpOffset * roleScale_, 0.0f } +
           fwd * (kMuzzleFwdOffset * roleScale_);
}

void Enemy::HideLegacyOverheadSprites()
{
    if (hpBarBg_) { hpBarBg_->SetSize({ 0.0f, 0.0f }); hpBarBg_->Update(); }
    if (hpBarFg_) { hpBarFg_->SetSize({ 0.0f, 0.0f }); hpBarFg_->Update(); }
}

bool Enemy::FaceTarget(const Vector3& targetPosition, float deltaTime)
{
    if (!object3d_) return false;

    const Vector3 enemyPos = GetPosition();
    const Vector3 toTarget = { targetPosition.x - enemyPos.x, 0.0f, targetPosition.z - enemyPos.z };
    const float lenSq = toTarget.x * toTarget.x + toTarget.z * toTarget.z;
    if (lenSq <= kFaceTargetMinLenSq) return false;

    const float invLen = 1.0f / std::sqrt(lenSq);
    const float targetYaw = std::atan2(toTarget.x * invLen, toTarget.z * invLen);

    currentYaw_ = NormalizeAngle(currentYaw_);
    float diff = NormalizeAngle(targetYaw - currentYaw_);

    const float turnSpeed = (state_ == AIState::Patrol) ? kPatrolTurnSpeed : kCombatTurnSpeed;
    const float maxRotate = turnSpeed * deltaTime;
    if (std::abs(diff) > maxRotate)
    {
        diff = (diff > 0.0f) ? maxRotate : -maxRotate;
    }

    currentYaw_ = NormalizeAngle(currentYaw_ + diff);
    return (std::abs(NormalizeAngle(targetYaw - currentYaw_)) < kFaceAlignedTolRad);
}

bool Enemy::UpdateTacticalWeaponState(float deltaTime, const Vector3* targetPosition, bool hasLOS)
{
    hasCurrentLOS_ = hasLOS;

    // マガジンリロード進行
    if (reloadTimer_ > 0.0f)
    {
        reloadTimer_ -= deltaTime;
        aimTelegraphTimer_ = 0.0f;
        burstShotsRemaining_ = 0;
        readyToFireBurst_ = false;
        if (reloadTimer_ <= 0.0f)
        {
            reloadTimer_ = 0.0f;
            ammoInMag_ = maxMagAmmo_;
            shotCooldownTimer_ = kPostReloadShotDelay;
        }
        return false;
    }

    if (shotCooldownTimer_ > 0.0f)
    {
        shotCooldownTimer_ = (std::max)(0.0f, shotCooldownTimer_ - deltaTime);
    }
    if (rifleRecoil_ > 0.0f)
    {
        rifleRecoil_ = (std::max)(0.0f, rifleRecoil_ - kRifleRecoilDecayRate * deltaTime);
    }

    // 2点バースト連射中のインターバル管理
    if (burstShotsRemaining_ > 0)
    {
        if (!hasLOS || !targetPosition || state_ != AIState::Chase)
        {
            burstShotsRemaining_ = 0;
            readyToFireBurst_ = false;
            shotCooldownTimer_ = kBurstAbortShotCooldown;
            return false;
        }
        burstIntervalTimer_ -= deltaTime;
        if (burstIntervalTimer_ <= 0.0f)
        {
            readyToFireBurst_ = true;
        }
        return readyToFireBurst_;
    }

    // 射撃準備（赤いタクティカルレーザーによるロックオン予兆）
    if (state_ == AIState::Chase && alertTimer_ <= 0.0f && shotCooldownTimer_ <= 0.0f && hasLOS && targetPosition)
    {
        if (ammoInMag_ <= 0)
        {
            reloadTimer_ = reloadDuration_;
            return false;
        }

        aimTelegraphTimer_ += deltaTime;
        if (aimTelegraphTimer_ >= aimTelegraphDuration_)
        {
            aimTelegraphTimer_ = 0.0f;
            burstShotsRemaining_ = (std::min)(burstCountPerTrigger_, ammoInMag_);
            burstIntervalTimer_ = 0.0f;
            readyToFireBurst_ = true;
            return true;
        }
    }
    else
    {
        aimTelegraphTimer_ = (std::max)(0.0f, aimTelegraphTimer_ - deltaTime * kAimTelegraphDecayMult);
    }

    return false;
}

void Enemy::UpdateVisualAndWeaponPose(float deltaTime)
{
    if (!object3d_) return;

    // 水平移動速度・進行方向からアヒルらしい小気味よい歩行よちよち＆水かき足ストライドを算出
    if (deltaTime > kPoseMinDeltaTime)
    {
        const float dx = position_.x - prevFramePos_.x;
        const float dz = position_.z - prevFramePos_.z;
        const float dist = std::sqrt(dx * dx + dz * dz);
        const float instSpeed = dist / deltaTime;
        horizSpeed_ = horizSpeed_ * kHorizSpeedKeepWeight + instSpeed * kHorizSpeedNewWeight;

        if (dist > kPoseMinMoveDist)
        {
            const float invDist = 1.0f / dist;
            const float ux = dx * invDist;
            const float uz = dz * invDist;
            const float sinYaw = std::sin(currentYaw_);
            const float cosYaw = std::cos(currentYaw_);
            const float rawR = ux * cosYaw - uz * sinYaw;
            const float rawF = ux * sinYaw + uz * cosYaw;
            localMoveRight_ = localMoveRight_ * kLocalMoveKeepWeight + rawR * kLocalMoveNewWeight;
            localMoveFwd_   = localMoveFwd_   * kLocalMoveKeepWeight + rawF * kLocalMoveNewWeight;
            const float lmLen = std::sqrt(localMoveRight_ * localMoveRight_ + localMoveFwd_ * localMoveFwd_);
            if (lmLen > kLocalMoveNormEps)
            {
                localMoveRight_ /= lmLen;
                localMoveFwd_   /= lmLen;
            }
        }
    }
    prevFramePos_ = position_;

    const float walkRatio = (std::clamp)(horizSpeed_ / kWalkFullSpeedRef, 0.0f, 1.0f);
    if (walkRatio > kWalkActiveRatioMin && !isDead_)
    {
        waddlePhase_ += deltaTime * (kWaddleBaseRate + kWaddleSpeedRate * walkRatio);
        if (waddlePhase_ >= DuckPartRig::kTwoPi)
        {
            waddlePhase_ = std::fmod(waddlePhase_, DuckPartRig::kTwoPi);
        }
    }
    else if (!isDead_)
    {
        waddlePhase_ = kIdleWaddlePhase;
    }

    const Vector4 bodyTint = (hitFlashTimer_ > 0.0f) ? kHitFlashColor : baseBodyTint_;
    Object3d* limbs[kLimbCount] = { leftFootObject_.get(), rightFootObject_.get(), leftWingObject_.get(), rightWingObject_.get() };
    for (Object3d* limb : limbs)
    {
        if (limb) limb->SetColor(bodyTint);
    }

    // 撃破時のノックアウト転倒演出（手足・翼も一体で吹き飛ぶ）
    if (isDead_ && deathAnimTimer_ > 0.0f)
    {
        const float t = 1.0f - (std::clamp)(deathAnimTimer_ / kDeathAnimDuration, 0.0f, 1.0f);
        const float oneMinusT = 1.0f - t;
        const float ease = 1.0f - oneMinusT * oneMinusT;
        Vector3 visualPos = position_;
        visualPos.y = (std::max)(0.0f, kDeathHopArcHeight * std::sin(t * kPi) - kDeathDropOffset * ease);
        const Matrix4x4 deadRotMat = DuckPartRig::MakeLocalPitchRollThenYawMatrix(
            kDeathPitchMax * ease, kDeathRollMax * deathFallDir_ * ease, currentYaw_);
        const Vector3 visualRot = DuckPartRig::DecomposeRotationToEulerXYZ(deadRotMat);
        const float shrink = ((t > kDeathShrinkStartRatio) ? (1.0f - (t - kDeathShrinkStartRatio) / kDeathShrinkSpan) : 1.0f) * roleScale_;
        const Vector3 visualScale = { shrink, shrink, shrink };

        object3d_->SetTranslate(visualPos);
        object3d_->SetRotate(visualRot);
        object3d_->SetScale(visualScale);
        object3d_->Update();

        DuckPartRig::AttachLimbToBody(leftFootObject_.get(),  DuckPartRig::kLeftFootPivot,  { 0.0f, kDeathFootOffsetY, 0.0f }, { kDeathLeftFootPitch * ease, -kDeathFootYaw,  kDeathFootRoll }, visualPos, visualScale, deadRotMat);
        DuckPartRig::AttachLimbToBody(rightFootObject_.get(), DuckPartRig::kRightFootPivot, { 0.0f, kDeathFootOffsetY, 0.0f }, { kDeathRightFootPitch * ease, kDeathFootYaw, -kDeathFootRoll }, visualPos, visualScale, deadRotMat);
        DuckPartRig::AttachLimbToBody(leftWingObject_.get(),  DuckPartRig::kLeftWingPivot,  { 0.0f, 0.0f, 0.0f }, { kDeathWingPitch, 0.0f, -kDeathWingFlap * ease }, visualPos, visualScale, deadRotMat);
        DuckPartRig::AttachLimbToBody(rightWingObject_.get(), DuckPartRig::kRightWingPivot, { 0.0f, 0.0f, 0.0f }, { kDeathWingPitch, 0.0f,  kDeathWingFlap * ease }, visualPos, visualScale, deadRotMat);

        if (rifleObject_)
        {
            const Vector3 localOff = { kRifleBaseLocalOffset.x * shrink, kRifleBaseLocalOffset.y * shrink, kRifleBaseLocalOffset.z * shrink };
            rifleObject_->SetTranslate(visualPos + DuckPartRig::TransformVec3ByRotation(localOff, deadRotMat));
            rifleObject_->SetRotate(visualRot);
            rifleObject_->SetScale(visualScale);
            rifleObject_->Update();
        }
        return;
    }

    const float sinWaddle  = std::sin(waddlePhase_);
    const float hopSin     = std::abs(sinWaddle);
    const float bobY       = hopSin * kBobHeightAmp * walkRatio * roleScale_;
    const float stepSquash = std::cos(waddlePhase_ * 2.0f) * kStepSquashAmp * walkRatio;

    float localPitchX = localMoveFwd_ * kMoveFwdPitchScale * walkRatio - rifleRecoil_ * kRecoilBodyPitchScale;
    float localRollZ  = -sinWaddle * kWaddleRollAmp * walkRatio - localMoveRight_ * kMoveRightRollScale * walkRatio;

    // 被弾時のけぞりリアクション
    if (flinchTimer_ > 0.0f)
    {
        flinchTimer_ = (std::max)(0.0f, flinchTimer_ - deltaTime);
        const float fl = flinchTimer_ / kBaseFlinchDuration;
        localPitchX -= kFlinchPitchScale * fl;
        localRollZ  += kFlinchRollScale * strafeSign_ * fl;
    }

    const Vector3 visualPos = { position_.x, position_.y + bobY, position_.z };
    const Vector3 visualScale = {
        roleScale_ * (1.0f + stepSquash * kSquashXZScale),
        roleScale_ * (1.0f - stepSquash * kSquashYScale),
        roleScale_ * (1.0f + stepSquash * kSquashXZScale)
    };
    const Matrix4x4 bodyRotMat = DuckPartRig::MakeLocalPitchRollThenYawMatrix(localPitchX, localRollZ, currentYaw_);
    const Vector3 visualRot = DuckPartRig::DecomposeRotationToEulerXYZ(bodyRotMat);

    object3d_->SetTranslate(visualPos);
    object3d_->SetRotate(visualRot);
    object3d_->SetScale(visualScale);
    object3d_->Update();

    // 左右の水かき足（巡回・追跡中に交互に踏み出す）
    DuckPartRig::SolveGroundedFoot(
        leftFootObject_.get(), DuckPartRig::kLeftFootPivot, -1.0f, waddlePhase_,
        walkRatio, kFootStrideLength, kFootLiftHeight, localMoveRight_, localMoveFwd_,
        position_, currentYaw_, roleScale_);
    DuckPartRig::SolveGroundedFoot(
        rightFootObject_.get(), DuckPartRig::kRightFootPivot, 1.0f, waddlePhase_ + DuckPartRig::kPi,
        walkRatio, kFootStrideLength, kFootLiftHeight, localMoveRight_, localMoveFwd_,
        position_, currentYaw_, roleScale_);

    // 左右の翼（左翼スイング＆リロード時のマガジン操作モーション、右翼はライフル保持）
    float leftWingPitch = kLeftWingSwingAmp * std::cos(waddlePhase_) * walkRatio;
    float leftWingYaw   = 0.0f;
    float leftWingFlap  = kLeftWingBaseFlap + (kLeftWingWalkFlap + kLeftWingHopFlap * hopSin) * walkRatio;
    if (reloadTimer_ > 0.0f)
    {
        leftWingPitch = kReloadLeftWingPitch;
        leftWingYaw   = kReloadLeftWingYaw;
        leftWingFlap  = kReloadLeftWingFlap;
    }
    DuckPartRig::AttachLimbToBody(
        leftWingObject_.get(), DuckPartRig::kLeftWingPivot, { 0.0f, 0.0f, 0.0f },
        { leftWingPitch, leftWingYaw, -leftWingFlap }, visualPos, visualScale, bodyRotMat);
    DuckPartRig::AttachLimbToBody(
        rightWingObject_.get(), DuckPartRig::kRightWingPivot, { 0.0f, 0.0f, 0.0f },
        { kRightWingHoldPitch, 0.0f, kRightWingBaseFlap + kRightWingHopFlap * hopSin * walkRatio }, visualPos, visualScale, bodyRotMat);

    // 3Dライフルの追従（リロード中は銃口を少し斜め下に傾ける）
    if (rifleObject_)
    {
        const float reloadTilt = (reloadTimer_ > 0.0f) ? kReloadRifleTiltPitch : 0.0f;
        const Matrix4x4 gunRotMat = DuckPartRig::MakeLocalPitchRollThenYawMatrix(
            reloadTilt - rifleRecoil_ * kRifleRecoilPitchScale, 0.0f, currentYaw_);
        const Vector3 gunRot = DuckPartRig::DecomposeRotationToEulerXYZ(gunRotMat);

        const float recoilBack = rifleRecoil_ * kRifleRecoilBackScale;
        const Vector3 localOff = {
            kRifleBaseLocalOffset.x * roleScale_,
            kRifleBaseLocalOffset.y * roleScale_,
            (kRifleBaseLocalOffset.z - recoilBack) * roleScale_
        };
        const Vector3 gunAnchor = { position_.x, position_.y + bobY * kRifleBobFollowFactor, position_.z };

        rifleObject_->SetTranslate(gunAnchor + DuckPartRig::TransformVec3ByRotation(localOff, gunRotMat));
        rifleObject_->SetRotate(gunRot);
        rifleObject_->SetScale({ roleScale_, roleScale_, roleScale_ });
        rifleObject_->SetColor((GetAimTelegraphProgress() > kRifleTelegraphColorThresh) ? kRifleTelegraphColor : kRifleDefaultColor);
        rifleObject_->Update();
    }
}

bool Enemy::UpdateCommonPreAI(float deltaTime)
{
    HideLegacyOverheadSprites();

    // ダメージ遅延ゲージ（白いヒットラグバー）の滑らかな追従
    const float targetHpRatio = GetHPRatio();
    displayedHpRatio_ = (displayedHpRatio_ > targetHpRatio)
        ? (std::max)(targetHpRatio, displayedHpRatio_ - kHpLagDecayRate * deltaTime)
        : targetHpRatio;

    if (isDead_)
    {
        if (deathAnimTimer_ > 0.0f)
        {
            deathAnimTimer_ = (std::max)(0.0f, deathAnimTimer_ - deltaTime);
            const Vector3 nextP = { position_.x + knockbackVel_.x * deltaTime, 0.0f, position_.z + knockbackVel_.z * deltaTime };
            if (IsWalkableArenaPos(nextP)) position_ = nextP;
            const float damp = (std::max)(0.0f, 1.0f - kDeadKnockbackDamp * deltaTime);
            knockbackVel_.x *= damp;
            knockbackVel_.z *= damp;
            UpdateVisualAndWeaponPose(deltaTime);
        }
        return false;
    }

    if (!object3d_) return false;

    // 被弾ノックバック ＆ 回避ステップの物理減衰移動
    if (std::abs(knockbackVel_.x) > kLiveKnockbackMinVel || std::abs(knockbackVel_.z) > kLiveKnockbackMinVel)
    {
        const Vector3 cand = { position_.x + knockbackVel_.x * deltaTime, 0.0f, position_.z + knockbackVel_.z * deltaTime };
        if (IsWalkableArenaPos(cand)) position_ = cand;
        const float damp = (std::max)(0.0f, 1.0f - kLiveKnockbackDamp * deltaTime);
        knockbackVel_.x *= damp;
        knockbackVel_.z *= damp;
    }

    if (evadeCooldown_ > 0.0f) evadeCooldown_ = (std::max)(0.0f, evadeCooldown_ - deltaTime);
    if (evadeTimer_ > 0.0f)
    {
        evadeTimer_ = (std::max)(0.0f, evadeTimer_ - deltaTime);
        const Vector3 cand = { position_.x + evadeVel_.x * deltaTime, 0.0f, position_.z + evadeVel_.z * deltaTime };
        if (IsWalkableArenaPos(cand)) position_ = cand;
    }

    return true;
}

bool Enemy::EvaluatePlayerSight(
    const Vector3* targetPosition,
    const std::vector<std::unique_ptr<Obstacle>>& obstacles,
    bool isPlayerInCover,
    float chaseSightMult,
    float& outDistToPlayer)
{
    outDistToPlayer = kInitFarPlayerDist;
    if (!targetPosition) return false;

    const Vector3 enemyPos = GetPosition();
    const Vector3 toPlayer = { targetPosition->x - enemyPos.x, 0.0f, targetPosition->z - enemyPos.z };
    outDistToPlayer = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);

    const float effectiveSight = isPlayerInCover ? (maxSightRange_ * kCoverSightMultiplier) : maxSightRange_;
    if (state_ == AIState::Chase && outDistToPlayer <= maxSightRange_ * chaseSightMult)
    {
        return HasLineOfSight(*targetPosition, obstacles);
    }
    if (outDistToPlayer <= effectiveSight && outDistToPlayer > kMinSightDistEps)
    {
        const float dot = (std::sin(currentYaw_) * toPlayer.x + std::cos(currentYaw_) * toPlayer.z) / outDistToPlayer;
        const float angle = std::acos((std::clamp)(dot, -1.0f, 1.0f));
        if (angle <= fovAngle_ * 0.5f)
        {
            return HasLineOfSight(*targetPosition, obstacles);
        }
    }
    return false;
}

void Enemy::FinalizeCommonFrameState(float deltaTime, const Vector3* targetPosition, bool canSeePlayer)
{
    UpdateTacticalWeaponState(deltaTime, targetPosition, canSeePlayer);

    if (hitFlashTimer_ > 0.0f)
    {
        hitFlashTimer_ -= deltaTime;
        if (hitFlashTimer_ <= 0.0f)
        {
            hitFlashTimer_ = 0.0f;
            object3d_->SetColor(baseBodyTint_);
        }
    }
    else
    {
        object3d_->SetColor(baseBodyTint_);
    }

    if (alertTimer_ > 0.0f)
    {
        alertTimer_ = (std::max)(0.0f, alertTimer_ - deltaTime);
    }

    position_.y = 0.0f;
    UpdateVisualAndWeaponPose(deltaTime);
}

void Enemy::Update(WindowAPI* /*windowAPI*/, const Vector3* targetPosition, const std::vector<std::unique_ptr<Obstacle>>& obstacles, float deltaTime, bool isPlayerInCover)
{
    if (!UpdateCommonPreAI(deltaTime)) return;

    // --- 索敵 ＆ 視界チェック ---
    float distToPlayer = kInitFarPlayerDist;
    const bool canSeePlayer = EvaluatePlayerSight(targetPosition, obstacles, isPlayerInCover, kStaticChaseSightMult, distToPlayer);

    // 索敵メーター更新
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
                alertTimer_ = kStaticChaseAlertDur;
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
        investigateTarget_ = lastSeenPlayerPosition_;
        searchTimer_ = kStaticSearchDuration;
        alertTimer_ = kLostSightAlertDur;
        detectionMeter_ = kLostSightMeterRetain;
    }
    else
    {
        detectionMeter_ = (std::max)(0.0f, detectionMeter_ - kDetectionDecayRate * deltaTime);
    }

    // --- AI状態ごとの行動ロジック ---
    if (state_ == AIState::Patrol)
    {
        scanTime_ += deltaTime;
        const float scanArc = (role_ == TacticalRole::Overwatch) ? kOverwatchScanArc : kDefaultScanArc;
        const float targetYaw = NormalizeAngle(baseYaw_ + std::sin(scanTime_ * kPatrolScanFreq + scanPhase_) * scanArc);
        float diff = NormalizeAngle(targetYaw - currentYaw_);
        const float maxRot = kPatrolMaxTurnRate * deltaTime;
        if (std::abs(diff) > maxRot) diff = (diff > 0.0f) ? maxRot : -maxRot;
        currentYaw_ = NormalizeAngle(currentYaw_ + diff);

        const Vector3 rightPost = { std::cos(baseYaw_), 0.0f, -std::sin(baseYaw_) };
        const float idleShift = std::sin(scanTime_ * kIdleShiftFreq + scanPhase_) *
                                ((role_ == TacticalRole::Overwatch) ? kOverwatchIdleShift : kDefaultIdleShift);
        const Vector3 desiredPost = { spawnPosition_.x + rightPost.x * idleShift, 0.0f, spawnPosition_.z + rightPost.z * idleShift };

        const Vector3 toHome = { desiredPost.x - position_.x, 0.0f, desiredPost.z - position_.z };
        const float homeDist = std::sqrt(toHome.x * toHome.x + toHome.z * toHome.z);
        if (homeDist > kHomeReturnDeadzone)
        {
            const float spd = (std::min)(kHomeReturnMaxSpeed, homeDist * kHomeReturnGain);
            const Vector3 cand = {
                position_.x + (toHome.x / homeDist) * spd * deltaTime,
                0.0f,
                position_.z + (toHome.z / homeDist) * spd * deltaTime
            };
            if (IsWalkableArenaPos(cand)) position_ = cand;
        }
    }
    else if (state_ == AIState::Investigate)
    {
        scanTime_ += deltaTime;
        const float phase = scanTime_ * kInvestigateScanFreq + scanPhase_;
        const Vector3 scanOffset = {
            investigateTarget_.x + std::cos(phase) * kInvestigateScanRadius,
            0.0f,
            investigateTarget_.z + std::sin(phase) * kInvestigateScanRadius
        };
        FaceTarget(scanOffset, deltaTime);

        searchTimer_ -= deltaTime;
        if (searchTimer_ <= 0.0f)
        {
            state_ = AIState::Patrol;
        }
    }
    else if (state_ == AIState::Chase && targetPosition)
    {
        FaceTarget(*targetPosition, deltaTime);

        // 拠点歩哨・狙撃兵の「遮蔽ピークアウト射撃 ＆ リロード時カバー退避」サイクル
        if (alertTimer_ <= 0.0f && evadeTimer_ <= 0.0f)
        {
            const Vector3 toP = { targetPosition->x - position_.x, 0.0f, targetPosition->z - position_.z };
            const float len = std::sqrt(toP.x * toP.x + toP.z * toP.z);
            if (len > kMinCombatTargetDist)
            {
                const Vector3 right = { toP.z / len, 0.0f, -toP.x / len };
                const bool shouldDuckToAnchor = (reloadTimer_ > 0.0f || shotCooldownTimer_ > kDuckToAnchorShotCd);

                if (shouldDuckToAnchor)
                {
                    const Vector3 toAnchor = { spawnPosition_.x - position_.x, 0.0f, spawnPosition_.z - position_.z };
                    const float aDist = std::sqrt(toAnchor.x * toAnchor.x + toAnchor.z * toAnchor.z);
                    if (aDist > kAnchorReturnDeadzone)
                    {
                        const Vector3 cand = {
                            position_.x + (toAnchor.x / aDist) * kAnchorReturnSpeed * deltaTime,
                            0.0f,
                            position_.z + (toAnchor.z / aDist) * kAnchorReturnSpeed * deltaTime
                        };
                        if (IsWalkableArenaPos(cand)) position_ = cand;
                    }
                }
                else
                {
                    const float flipPeriod = canSeePlayer ? kCombatStrafeFlipPeriod : kPeekOutFlipPeriod;
                    const float maxRadSq   = canSeePlayer ? kCombatStrafeMaxRadSq   : kPeekOutMaxRadiusSq;
                    const float strafeSpd  = !canSeePlayer
                        ? kPeekOutSpeed
                        : ((aimTelegraphTimer_ > 0.0f)
                            ? ((role_ == TacticalRole::Overwatch) ? kOverwatchAimStrafeSpd : kDefaultAimStrafeSpd)
                            : kFreeCombatStrafeSpd);

                    strafeTimer_ += deltaTime;
                    if (strafeTimer_ >= flipPeriod)
                    {
                        strafeTimer_ = 0.0f;
                        strafeSign_ = -strafeSign_;
                    }
                    const Vector3 cand = {
                        position_.x + right.x * strafeSign_ * strafeSpd * deltaTime,
                        0.0f,
                        position_.z + right.z * strafeSign_ * strafeSpd * deltaTime
                    };
                    const float dxHome = cand.x - spawnPosition_.x;
                    const float dzHome = cand.z - spawnPosition_.z;
                    if ((dxHome * dxHome + dzHome * dzHome) <= maxRadSq && IsWalkableArenaPos(cand))
                    {
                        position_ = cand;
                    }
                    else
                    {
                        strafeSign_ = -strafeSign_;
                    }
                }
            }
        }
    }

    FinalizeCommonFrameState(deltaTime, targetPosition, canSeePlayer);
}

bool Enemy::HasLineOfSight(const Vector3& playerPos, const std::vector<std::unique_ptr<Obstacle>>& obstacles)
{
    (void)obstacles;
    const Vector3 enemyEye = GetPosition() + Vector3{ 0.0f, kEyeHeightBase * roleScale_, 0.0f };

    constexpr uint32_t kObsAttr = static_cast<uint32_t>(CollisionAttribute::Obstacle);
    CollisionManager* colMgr = CollisionManager::GetInstance();
    int visiblePoints = 0;

    for (size_t i = 0; i < kLosSampleCount; ++i)
    {
        // 3本中2本以上のレイが通れば視認成立のため、2本目終了時点で勝敗が確定していれば3本目のレイキャストを省略
        if (i == 2)
        {
            if (visiblePoints >= kLosRequiredHits) return true;
            if (visiblePoints == 0) return false;
        }

        const Vector3 targetPoint = { playerPos.x, playerPos.y + kLosTargetYOffsets[i], playerPos.z };
        const Vector3 toTarget = { targetPoint.x - enemyEye.x, targetPoint.y - enemyEye.y, targetPoint.z - enemyEye.z };
        const float distSq = toTarget.x * toTarget.x + toTarget.y * toTarget.y + toTarget.z * toTarget.z;
        if (distSq < kLosMinDistSq) { visiblePoints++; continue; }

        const float dist = std::sqrt(distSq);
        const Vector3 dir = { toTarget.x / dist, toTarget.y / dist, toTarget.z / dist };
        Collider* hitCollider = nullptr;
        float hitDist = 0.0f;

        bool hitObstacle = false;
        if (colMgr->Raycast(enemyEye, dir, dist, hitCollider, hitDist, kObsAttr))
        {
            if (hitCollider && hitCollider->GetAttribute() == CollisionAttribute::Obstacle)
            {
                hitObstacle = true;
            }
        }
        if (!hitObstacle) visiblePoints++;
    }

    return (visiblePoints >= kLosRequiredHits);
}

void Enemy::HearNoise(const Vector3& noisePosition)
{
    if (isDead_ || state_ == AIState::Chase) return;

    // 高さ1.65mで遮蔽チェック：低い土嚢越しには銃声が届き、高いコンテナ隔壁・フェンス越しには次ゾーンへ伝播しない
    const Vector3 earPos = GetPosition() + Vector3{ 0.0f, kEarHeightY, 0.0f };
    const Vector3 toNoise = { noisePosition.x - earPos.x, 0.0f, noisePosition.z - earPos.z };
    const float distSq = toNoise.x * toNoise.x + toNoise.z * toNoise.z;
    if (distSq > kHearingMaxDistSq) return;

    const float dist = std::sqrt(distSq);
    if (dist > kMinSightDistEps)
    {
        const Vector3 dir = { toNoise.x / dist, 0.0f, toNoise.z / dist };
        Collider* hitCollider = nullptr;
        float hitDist = 0.0f;
        constexpr uint32_t kObsAttr = static_cast<uint32_t>(CollisionAttribute::Obstacle);
        if (CollisionManager::GetInstance()->Raycast(earPos, dir, dist, hitCollider, hitDist, kObsAttr))
        {
            if (hitCollider && hitCollider->GetAttribute() == CollisionAttribute::Obstacle)
            {
                return;
            }
        }
    }

    state_ = AIState::Investigate;
    investigateTarget_ = noisePosition;
    searchTimer_ = kNoiseSearchDuration;
    alertTimer_ = kNoiseAlertDuration;
    detectionMeter_ = (std::max)(detectionMeter_, kNoiseMinDetectionMeter);
}

void Enemy::AlertEnemy(const Vector3& targetPos)
{
    if (isDead_) return;
    if (state_ != AIState::Chase)
    {
        state_ = AIState::Chase;
        alertTimer_ = kSquadAlertDuration;
        detectionMeter_ = 1.0f;
        lastSeenPlayerPosition_ = targetPos;
        shotCooldownTimer_ = (std::max)(shotCooldownTimer_, kSquadMinShotCooldown);
    }
    else
    {
        lastSeenPlayerPosition_ = targetPos;
    }
}

std::unique_ptr<Bullet> Enemy::TryShoot(const Vector3& targetPosition)
{
    if (isDead_ || !object3d_ || !object3dCom_ || !camera_ || !readyToFireBurst_ || state_ != AIState::Chase || reloadTimer_ > 0.0f)
    {
        return nullptr;
    }

    readyToFireBurst_ = false;
    FaceTarget(targetPosition, kPreShotFaceDeltaTime);

    const Vector3 enemyPos = GetPosition();
    const Vector3 toTarget = { targetPosition.x - enemyPos.x, 0.0f, targetPosition.z - enemyPos.z };
    const float lenSq = toTarget.x * toTarget.x + toTarget.z * toTarget.z;
    if (lenSq <= kMinSightDistEps) return nullptr;

    const float invLen = 1.0f / std::sqrt(lenSq);
    const float baseAngle = std::atan2(toTarget.x * invLen, toTarget.z * invLen);
    const float maxSpread = (role_ == TacticalRole::Overwatch) ? kOverwatchSpreadRad : kDefaultSpreadRad;
    const float spread = ((static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX)) * 2.0f - 1.0f) * maxSpread;
    const Vector3 forward = { std::sin(baseAngle + spread), 0.0f, std::cos(baseAngle + spread) };
    const Vector3 spawnPos = GetMuzzleWorldPosition();

    // 銃口の至近距離（前方1.1m以内）に障害物がある場合は壁撃ちを抑制
    {
        Collider* hitCol = nullptr;
        float checkDist = kMuzzleWallCheckDist;
        constexpr uint32_t kObsAttr = static_cast<uint32_t>(CollisionAttribute::Obstacle);
        if (CollisionManager::GetInstance()->Raycast(spawnPos, forward, checkDist, hitCol, checkDist, kObsAttr))
        {
            if (hitCol && hitCol->GetAttribute() == CollisionAttribute::Obstacle)
            {
                burstShotsRemaining_ = 0;
                shotCooldownTimer_ = kMuzzleBlockedCooldown;
                return nullptr;
            }
        }
    }

    auto bullet = std::make_unique<Bullet>();
    bullet->Initialize(object3dCom_, camera_, spawnPos, forward, bulletSpeed_, bulletLifeTime_, BulletOwner::Enemy);

    rifleRecoil_ = 1.0f;
    ammoInMag_ = (std::max)(0, ammoInMag_ - 1);
    burstShotsRemaining_ = (std::max)(0, burstShotsRemaining_ - 1);

    if (burstShotsRemaining_ > 0 && ammoInMag_ > 0)
    {
        burstIntervalTimer_ = (role_ == TacticalRole::Warden) ? kWardenBurstInterval : kDefaultBurstInterval;
    }
    else
    {
        burstShotsRemaining_ = 0;
        if (ammoInMag_ <= 0) reloadTimer_ = reloadDuration_;
        else shotCooldownTimer_ = shotCooldown_;
    }

    return bullet;
}

void Enemy::Draw(const RenderContext& ctx)
{
    if (isDead_ && deathAnimTimer_ <= 0.0f) return;
    if (!object3dCom_ || !object3d_) return;

    RenderContext enemyCtx = ctx;
    const Object3d::ModelData& modelData = object3d_->GetModelData();
    const uint32_t texIdx = (defaultTextureIndex_ != TextureManager::kInvalidTextureIndex)
        ? defaultTextureIndex_
        : modelData.material.textureIndex;
    if (texIdx != TextureManager::kInvalidTextureIndex)
    {
        enemyCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(texIdx);
    }

    const Vector4 bodyCol = (hitFlashTimer_ > 0.0f) ? kHitFlashColor : baseBodyTint_;
    if (ctx.camera)
    {
        object3d_->SetCamera(ctx.camera);
        object3d_->Update();
    }
    object3dCom_->Draw(object3d_.get(), enemyCtx, modelData, true);

    Object3d* limbs[kLimbCount] = { leftFootObject_.get(), rightFootObject_.get(), leftWingObject_.get(), rightWingObject_.get() };
    for (size_t i = 0; i < kLimbCount; ++i)
    {
        if (!limbs[i] || limbs[i]->GetModelData().vertices.empty()) continue;
        limbs[i]->SetColor(bodyCol);
        if (ctx.camera)
        {
            limbs[i]->SetCamera(ctx.camera);
            limbs[i]->Update();
        }
        object3dCom_->Draw(limbs[i], enemyCtx, limbs[i]->GetModelData(), true);
    }

    if (rifleObject_)
    {
        rifleObject_->Draw(ctx);
    }
}

void Enemy::OnHit(const Vector3& attackerPos)
{
    if (isDead_ || !object3d_) return;

    Vector3 away = { position_.x - attackerPos.x, 0.0f, position_.z - attackerPos.z };
    const float awayLen = std::sqrt(away.x * away.x + away.z * away.z);
    if (awayLen > kMinSightDistEps)
    {
        away.x /= awayLen;
        away.z /= awayLen;
    }
    else
    {
        away = { 0.0f, 0.0f, 1.0f };
    }

    hp_--;
    flinchTimer_ = (role_ == TacticalRole::Warden) ? kWardenFlinchDuration : kBaseFlinchDuration;
    const float kbMult = (role_ == TacticalRole::Warden) ? kWardenKnockbackMult : 1.0f;
    knockbackVel_ = { away.x * kHitKnockbackSpeed * kbMult, 0.0f, away.z * kHitKnockbackSpeed * kbMult };

    if (hp_ <= 0)
    {
        hp_ = 0;
        isDead_ = true;
        justRespawned_ = false;
        deathAnimTimer_ = kDeathAnimDuration;
        deathFallDir_ = (away.x >= 0.0f) ? 1.0f : -1.0f;
        knockbackVel_ = { away.x * kDeathKnockbackSpeed, 0.0f, away.z * kDeathKnockbackSpeed };

        if (collider_)
        {
            CollisionManager::GetInstance()->UnregisterCollider(collider_.get());
        }
        HideLegacyOverheadSprites();
        return;
    }

    hitFlashTimer_ = hitFlashDuration_;
    object3d_->SetColor(kHitFlashColor);

    aimTelegraphTimer_ = (std::max)(0.0f, aimTelegraphTimer_ - ((role_ == TacticalRole::Warden) ? kWardenAimSuppress : kDefaultAimSuppress));

    if (evadeCooldown_ <= 0.0f)
    {
        evadeCooldown_ = kEvadeCooldownDuration;
        evadeTimer_ = kEvadeStepDuration;
        strafeSign_ = -strafeSign_;
        const Vector3 side = { -away.z * strafeSign_, 0.0f, away.x * strafeSign_ };
        evadeVel_ = { side.x * kEvadeStepSpeed, 0.0f, side.z * kEvadeStepSpeed };
    }

    if (awayLen <= kHitDirectAlertMaxDist)
    {
        AlertEnemy(attackerPos);
        alertTimer_ = (std::min)(alertTimer_, kHitMaxAlertPopupTime);
    }
    else if (state_ != AIState::Chase)
    {
        state_ = AIState::Investigate;
        investigateTarget_ = attackerPos;
        searchTimer_ = kFarHitSearchDuration;
        alertTimer_ = kFarHitAlertDuration;
        detectionMeter_ = kFarHitDetectionMeter;
    }
}

void Enemy::Finalize()
{
    if (collider_)
    {
        CollisionManager::GetInstance()->UnregisterCollider(collider_.get());
        collider_.reset();
    }
    leftFootObject_.reset();
    rightFootObject_.reset();
    leftWingObject_.reset();
    rightWingObject_.reset();
    rifleObject_.reset();
    object3d_.reset();
}
