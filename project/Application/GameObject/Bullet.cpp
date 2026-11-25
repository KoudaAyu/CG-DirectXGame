#include "Bullet.h"
#include <cmath>
#include <algorithm>
#include <filesystem>
#include <string>
#include "TextureManager.h"

namespace
{
    // リソースパス定数
    constexpr const char* kResourceDir          = "Resources";
    constexpr const char* kProjectResourcePrefix = "project/Resources/";
    constexpr const char* kFMJModelFile         = "duckov_bullet_fmj.obj";
    constexpr const char* kFallbackModelFile    = "sphere.obj";
    constexpr const char* kTracerModelFile      = "duckov_bullet_tracer.obj";
    constexpr const char* kBallisticsAtlasPath  = "Resources/duckov_ballistics_atlas.png";
    constexpr const char* kFallbackWhiteTexPath = "Resources/CG4/human/white.png";

    // メッシュ境界半径・数値閾値
    constexpr float kFMJBoundingRadius     = 1.0f;
    constexpr float kTracerBoundingRadius  = 4.0f;
    constexpr float kInitDirNormalizeEps   = 0.0001f;
    constexpr float kStepDirNormalizeEps   = 1e-5f;
    constexpr float kMaxPitchSinClamp      = 0.99f;

    // 弾頭・曳光火球マテリアル定数
    constexpr Vector3 kUnitScale              = { 1.0f, 1.0f, 1.0f };
    constexpr Vector3 kDefaultForward         = { 0.0f, 0.0f, 1.0f };
    constexpr float   kBulletReflectionFactor = 0.36f;
    constexpr float   kBulletFresnelF0        = 0.68f;
    constexpr Vector4 kPlayerBulletColor      = { 1.08f, 0.96f, 0.86f, 1.0f }; // 5.56mm ポリッシュ銅ジャケット徹甲弾頭
    constexpr Vector4 kEnemyBulletColor       = { 1.04f, 0.86f, 0.74f, 1.0f }; // 東側 カッパーウォッシュ鋼芯弾頭
    constexpr Vector4 kPlayerTracerColor      = { 0.96f, 0.58f, 0.28f, 1.0f }; // 純白コア＋アンバーオレンジ曳光火球
    constexpr Vector4 kEnemyTracerColor       = { 1.15f, 0.22f, 0.10f, 1.0f }; // 白熱コア＋クリムゾンレッド曳光火球

    // 曳光ストリーク＆衝撃波パルス定数
    constexpr float kPlayerMaxWakeScale   = 0.96f;
    constexpr float kEnemyWakeSpeedScale  = 1.85f;
    constexpr float kEnemyMinWakeScale    = 0.68f;
    constexpr float kEnemyMaxWakeScale    = 0.92f;
    constexpr float kBaseStreakLength     = 0.46f;
    constexpr float kStreakDistanceScale  = 0.85f;
    constexpr float kShimmerBase          = 0.96f;
    constexpr float kShimmerAmp           = 0.06f;
    constexpr float kShimmerFreq          = 72.0f;
    constexpr float kWakePulseInterval    = 0.024f;
    constexpr float kMinWakeDistance      = 0.55f;

    // 微細重力ドロップ演出定数
    constexpr float kGravityDropMinY      = 0.24f;
    constexpr float kGravityDropRate      = 0.0085f;

    bool sBulletModelsLoaded = false;
    Object3d::ModelData sBulletFMJModel;
    Object3d::ModelData sBulletTracerModel;

    bool HasResourceFile(const char* filename)
    {
        const std::string relPath = std::string(kResourceDir) + "/" + filename;
        const std::string projPath = std::string(kProjectResourcePrefix) + filename;
        return std::filesystem::exists(relPath) || std::filesystem::exists(projPath);
    }

    Vector3 NormalizeOrFallback(const Vector3& v, float eps, const Vector3& fallback)
    {
        const float lenSq = v.x * v.x + v.y * v.y + v.z * v.z;
        if (lenSq > eps * eps)
        {
            const float invLen = 1.0f / std::sqrt(lenSq);
            return { v.x * invLen, v.y * invLen, v.z * invLen };
        }
        return fallback;
    }

    void EnsureBulletModelsLoaded()
    {
        if (sBulletModelsLoaded) return;

        const char* fmjFile = HasResourceFile(kFMJModelFile) ? kFMJModelFile : kFallbackModelFile;
        sBulletFMJModel = Object3d::LoadObjFile(kResourceDir, fmjFile);
        sBulletFMJModel.boundingRadius = kFMJBoundingRadius;

        if (HasResourceFile(kTracerModelFile))
        {
            sBulletTracerModel = Object3d::LoadObjFile(kResourceDir, kTracerModelFile);
            sBulletTracerModel.boundingRadius = kTracerBoundingRadius;
        }
        else
        {
            sBulletTracerModel = sBulletFMJModel;
        }

        sBulletModelsLoaded = true;
    }
}

Vector3 Bullet::ComputeSpawnPosition(const Vector3& ownerPosition, const Vector3& forward, const Vector3& spawnOffset)
{
    const Vector3 right = { forward.z, 0.0f, -forward.x };
    return {
        ownerPosition.x + right.x * spawnOffset.x + forward.x * spawnOffset.z,
        ownerPosition.y + spawnOffset.y,
        ownerPosition.z + right.z * spawnOffset.x + forward.z * spawnOffset.z
    };
}

void Bullet::Initialize(Object3dCom* object3dCom, Camera* camera, const Vector3& startPosition, const Vector3& direction, float speed, float lifeTime, BulletOwner owner)
{
    object3dCom_ = object3dCom;
    camera_ = camera;
    speed_ = speed;
    lifeTime_ = lifeTime;
    age_ = 0.0f;
    distanceTraveled_ = 0.0f;
    wakeTimer_ = 0.0f;
    isDead_ = false;
    nearMissTriggered_ = false;
    owner_ = owner;
    direction_ = NormalizeOrFallback(direction, kInitDirNormalizeEps, kDefaultForward);

    EnsureBulletModelsLoaded();
    ballisticsTexIndex_ = TextureManager::GetInstance()->Load(kBallisticsAtlasPath);
    if (ballisticsTexIndex_ == TextureManager::kInvalidTextureIndex)
    {
        ballisticsTexIndex_ = TextureManager::GetInstance()->Load(kFallbackWhiteTexPath);
    }

    const bool isPlayer = (owner_ == BulletOwner::Player);

    // 1. 重厚な丸みを持つ3D銅被甲・鋼芯チップ実弾頭（ライフル腔線スピン＋強い金属フレネル光沢）
    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(object3dCom_, sBulletFMJModel);
    object3d_->SetCamera(camera_);
    object3d_->SetTranslate(startPosition);
    object3d_->SetScale(kUnitScale);
    object3d_->SetEnableLighting(true);
    object3d_->SetReflectionFactor(kBulletReflectionFactor);
    object3d_->SetFresnelF0(kBulletFresnelF0);
    object3d_->SetColor(isPlayer ? kPlayerBulletColor : kEnemyBulletColor);

    // 2. 弾底の曳光火球＋1フレーム相当の短い涙滴シャッターブラー（プリマルチプライ加算合成専用）
    tracerObject_ = std::make_unique<Object3d>();
    tracerObject_->Initialize(object3dCom_, sBulletTracerModel);
    tracerObject_->SetCamera(camera_);
    tracerObject_->SetTranslate(startPosition);
    tracerObject_->SetEnableLighting(false);
    tracerObject_->SetReflectionFactor(0.0f);
    tracerObject_->SetAllowWireframeOverlay(false);
    tracerObject_->SetColor(isPlayer ? kPlayerTracerColor : kEnemyTracerColor);

    prevPosition_ = startPosition;
    UpdateVisualTransforms();
}

void Bullet::SetDirection(const Vector3& newDir)
{
    direction_ = NormalizeOrFallback(newDir, kInitDirNormalizeEps, kDefaultForward);
    UpdateVisualTransforms();
}

void Bullet::UpdateVisualTransforms()
{
    if (!object3d_) return;

    const Vector3 pos = object3d_->GetTranslate();
    const float yaw = std::atan2(direction_.x, direction_.z);
    const float clampedY = std::clamp(direction_.y, -kMaxPitchSinClamp, kMaxPitchSinClamp);
    const float pitch = -std::asin(clampedY);

    // 進行方向へ真っ直ぐ固定（オイラー角 Z回転を入れると横射撃時にワールドXY平面でプロペラ回転するため 0.0f 固定）
    object3d_->SetRotate({ pitch, yaw, 0.0f });
    object3d_->Update();

    if (tracerObject_)
    {
        // 棒状（つまようじ状）に見える長大な尾を廃止し、弾速1フレーム分（約0.4m〜0.55m）の丸みある火球＋短残像に抑制
        const float maxWakeScale = (owner_ == BulletOwner::Player)
            ? kPlayerMaxWakeScale
            : std::clamp(speed_ * kEnemyWakeSpeedScale, kEnemyMinWakeScale, kEnemyMaxWakeScale);
        const float streakLen = std::clamp(kBaseStreakLength + distanceTraveled_ * kStreakDistanceScale, kBaseStreakLength, maxWakeScale);
        const float pyrotechnicShimmer = kShimmerBase + kShimmerAmp * std::sin(age_ * kShimmerFreq);

        tracerObject_->SetTranslate(pos);
        tracerObject_->SetRotate({ pitch, yaw, 0.0f });
        tracerObject_->SetScale({ pyrotechnicShimmer, pyrotechnicShimmer, streakLen });
        tracerObject_->Update();
    }
}

bool Bullet::ConsumeWakePulse(float deltaTime)
{
    wakeTimer_ += deltaTime;
    if (wakeTimer_ >= kWakePulseInterval)
    {
        wakeTimer_ = std::fmod(wakeTimer_, kWakePulseInterval);
        return (distanceTraveled_ > kMinWakeDistance);
    }
    return false;
}

void Bullet::Update(float deltaTime)
{
    if (isDead_) return;

    if (object3d_)
    {
        if (camera_)
        {
            object3d_->SetCamera(camera_);
            if (tracerObject_) tracerObject_->SetCamera(camera_);
        }

        prevPosition_ = object3d_->GetTranslate();

        // 命中判定を一切損なわない微細な実弾重力ドロップ演出
        if (prevPosition_.y > kGravityDropMinY)
        {
            direction_.y -= kGravityDropRate * deltaTime;
            direction_ = NormalizeOrFallback(direction_, kStepDirNormalizeEps, direction_);
        }

        const float stepDist = speed_ * (deltaTime * kReferenceFrameRate);
        const Vector3 pos = {
            prevPosition_.x + direction_.x * stepDist,
            prevPosition_.y + direction_.y * stepDist,
            prevPosition_.z + direction_.z * stepDist
        };

        distanceTraveled_ += stepDist;
        object3d_->SetTranslate(pos);
        UpdateVisualTransforms();
    }

    age_ += deltaTime;
    if (age_ >= lifeTime_)
    {
        isDead_ = true;
    }
}

void Bullet::Draw(const RenderContext& ctx, ID3D12PipelineState* additiveTracerPSO)
{
    if (isDead_ || !object3d_ || !object3dCom_) return;

    if (ctx.camera)
    {
        object3d_->SetCamera(ctx.camera);
        object3d_->Update();
        if (tracerObject_)
        {
            tracerObject_->SetCamera(ctx.camera);
            tracerObject_->Update();
        }
    }

    RenderContext bulletCtx = ctx;
    if (ballisticsTexIndex_ != TextureManager::kInvalidTextureIndex)
    {
        bulletCtx.SetTextureHandle(TextureManager::GetInstance()->GetSrvHandleGPU(ballisticsTexIndex_));
    }

    // 1. 重厚な3D銅ジャケット・鋼芯チップ弾頭を不透明メタル描画（デプス書き込みON）
    object3dCom_->Draw(object3d_.get(), bulletCtx, object3d_->GetModelData(), true);

    // 2. 弾底の曳光火球＋短残像をGPUプリマルチプライ加算合成（ONE+ONE, デプス書き込みOFF）で発光描画
    if (tracerObject_ && bulletCtx.commandList)
    {
        ID3D12PipelineState* pso = additiveTracerPSO;
        if (!pso)
        {
            auto effectPSO = object3dCom_->GetEffectPipelineState();
            pso = effectPSO.Get();
        }
        if (pso)
        {
            bulletCtx.commandList->SetPipelineState(pso);
        }
        tracerObject_->DrawInternal(bulletCtx);
    }
}

void Bullet::Finalize()
{
    tracerObject_.reset();
    object3d_.reset();
    isDead_ = true;
}
