#define NOMINMAX
#include "EnemyBase.h"

#include "Baziru3_Engine/Core/Base/Matrix4x4.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3dCom.h"
#include "Baziru3_Engine/Framework/Collision/CollisionManager.h"
#include "Application/GameObject/SlimePhysics.h"

#include <algorithm>
#include <cmath>

namespace
{
    /// @brief 行ベクトル規約（v * M）での回転適用
    inline Vector3 ApplyRotation(const Vector3& v, const Matrix4x4& m)
    {
        return {
            v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0],
            v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1],
            v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2]
        };
    }

    /// @brief GamePlayScene が地面に掛けているのと同じ回転行列 R = Rx(pitch) * Rz(-roll)
    inline Matrix4x4 BuildStageRotation(const Vector2& stageTilt)
    {
        return Multiply(MakeRotateXMatrix(stageTilt.x), MakeRotateZMatrix(-stageTilt.y));
    }

    EnemyBase::ScaleFromStrengthFunc g_defaultScaleFunc = nullptr;
}

float EnemyBase::DefaultScaleFromStrength(int strength)
{
    // strength 1 で 0.70。以降ゆるやかに大きくなる
    // （PikminPlayer::CalculateScaleBySize と似た曲線にして、並んだときの見え方をそろえている）
    int s = (std::max)(1, strength);
    if (s == 1) return 0.70f;
    return 0.70f + 0.13f * static_cast<float>(s - 1)
                 + 0.045f * std::pow(static_cast<float>(s - 1), 1.2f);
}

void EnemyBase::SetDefaultScaleFromStrength(ScaleFromStrengthFunc func)
{
    g_defaultScaleFunc = std::move(func);
}

const EnemyBase::ScaleFromStrengthFunc& EnemyBase::GetDefaultScaleFromStrengthFunc()
{
    static ScaleFromStrengthFunc fallback = [](int s) { return DefaultScaleFromStrength(s); };
    return g_defaultScaleFunc ? g_defaultScaleFunc : fallback;
}

Vector3 EnemyBase::StageLocalToWorld(const Vector3& local, const Vector2& stageTilt, const Vector2& pivot)
{
    Matrix4x4 r = BuildStageRotation(stageTilt);
    Vector3 pivot3{ pivot.x, 0.0f, pivot.y };
    Vector3 rel = local - pivot3;
    return ApplyRotation(rel, r) + pivot3;
}

Vector3 EnemyBase::StageWorldToLocal(const Vector3& world, const Vector2& stageTilt, const Vector2& pivot)
{
    Matrix4x4 r = Transpose(BuildStageRotation(stageTilt)); // 回転行列の逆行列 = 転置
    Vector3 pivot3{ pivot.x, 0.0f, pivot.y };
    Vector3 rel = world - pivot3;
    return ApplyRotation(rel, r) + pivot3;
}

EnemyBase::~EnemyBase()
{
    if (collider_ && CollisionManager::GetInstance())
    {
        CollisionManager::GetInstance()->UnregisterCollider(collider_.get());
    }
}

void EnemyBase::Initialize(Object3dCom* object3dCom, Camera* camera, const Vector3& stageLocalPos, int strength)
{
    object3dCom_ = object3dCom;
    camera_ = camera;

    const ModelSpec spec = GetModelSpec();

    modelScale_ = spec.modelScale;
    groundOffsetRatio_ = spec.groundOffsetRatio;
    hitRadiusRatio_ = spec.hitRadiusRatio;
    hitHalfRatio_ = spec.hitHalfRatio;
    hitOffsetRatio_ = spec.hitOffsetRatio;
    hitShape_ = spec.hitShape;
    isPushable_ = spec.isPushable;

    // --- モデル読み込み ---
    // .obj は LoadObjFile、それ以外（.gltf など）は Assimp 経由の LoadModelFile
    const std::string& file = spec.fileName;
    bool isObj = (file.size() >= 4) && (file.compare(file.size() - 4, 4, ".obj") == 0);
    modelData_ = isObj ? Object3d::LoadObjFile(spec.directory, file)
                       : Object3d::LoadModelFile(spec.directory, file);

    // バウンディング半径を実測して視錐台カリングの誤判定を防ぐ
    float maxLenSq = 0.0f;
    for (const auto& v : modelData_.vertices)
    {
        float lenSq = v.position.x * v.position.x + v.position.y * v.position.y + v.position.z * v.position.z;
        maxLenSq = (std::max)(maxLenSq, lenSq);
    }
    modelData_.boundingRadius = (maxLenSq > 0.0f) ? std::sqrt(maxLenSq) : 2.0f;

    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(object3dCom_, modelData_); // テクスチャは material.textureFilePath から自動ロードされる
    object3d_->SetCamera(camera_);
    object3d_->SetColor(spec.tintColor);
    object3d_->SetEnableLighting(true);

    // --- 配置と強さ ---
    anchorLocal_ = stageLocalPos;
    anchorLocal_.y = 0.0f;
    position_ = stageLocalPos;
    isDead_ = false;
    lifeTime_ = 0.0f;
    needsGroundSnap_ = true;

    SetStrengthInternal(strength, false); // scale_ / groundOffset_ がここで決まる

    position_.y = stageLocalPos.y + groundOffset_;

    object3d_->SetTranslate(position_);
    object3d_->SetScale(scale_);
    object3d_->SetRotate(rotation_);
    object3d_->Update();

    // --- コライダー ---
    // プレイヤーの塊との判定は EnemyCollision で個別に解決するので、
    // ここで登録するのは主にミニオン・障害物との押し合い用
    // （Player <-> Enemy のフィルタは EnemyManager::Initialize() で切ってある）
    collider_ = std::make_unique<SphereCollider>(scale_.x * hitRadiusRatio_, &position_, CollisionAttribute::Enemy);
    collider_->SetPositionOffset({ 0.0f, scale_.y * hitOffsetRatio_, 0.0f });
    if (CollisionManager::GetInstance())
    {
        CollisionManager::GetInstance()->RegisterCollider(collider_.get());
    }

    OnInitialized();
}

void EnemyBase::SetStrength(int strength)
{
    SetStrengthInternal(strength, true);
}

void EnemyBase::SetStrengthInternal(int strength, bool refreshCollider)
{
    strength_ = (std::max)(1, strength);

    const ScaleFromStrengthFunc& func = scaleFunc_ ? scaleFunc_ : GetDefaultScaleFromStrengthFunc();
    float s = modelScale_ * func(strength_);
    if (s < 0.01f) s = 0.01f;

    scale_ = { s, s, s };
    groundOffset_ = s * groundOffsetRatio_;

    if (refreshCollider)
    {
        RefreshCollider();
    }
}

void EnemyBase::RefreshFromSpec()
{
    const ModelSpec spec = GetModelSpec();

    modelScale_ = spec.modelScale;
    groundOffsetRatio_ = spec.groundOffsetRatio;
    hitRadiusRatio_ = spec.hitRadiusRatio;
    hitHalfRatio_ = spec.hitHalfRatio;
    hitOffsetRatio_ = spec.hitOffsetRatio;
    hitShape_ = spec.hitShape;
    isPushable_ = spec.isPushable;

    SetStrengthInternal(strength_, true);
}

void EnemyBase::SetScaleFromStrength(ScaleFromStrengthFunc func)
{
    scaleFunc_ = std::move(func);
    SetStrength(strength_);
}

void EnemyBase::RefreshCollider()
{
    if (collider_)
    {
        collider_->SetRadius(scale_.x * hitRadiusRatio_);
        collider_->SetPositionOffset({ 0.0f, scale_.y * hitOffsetRatio_, 0.0f });
    }
}

void EnemyBase::Defeat()
{
    if (isDead_) return;
    isDead_ = true;

    if (collider_)
    {
        collider_->SetIsEnabled(false);
    }
}

void EnemyBase::ApplyPush(const Vector3& worldDelta, const Vector2& stageTilt, const Vector2& pivot)
{
    if (!isPushable_ || isDead_) return;

    Vector3 pushedWorld = position_ + worldDelta;
    Vector3 local = StageWorldToLocal(pushedWorld, stageTilt, pivot);
    anchorLocal_.x = local.x;
    anchorLocal_.z = local.z;
}

Vector3 EnemyBase::GetHitCenter() const
{
    return { position_.x, position_.y + scale_.y * hitOffsetRatio_, position_.z };
}

EnemyCollision::EnemyBody EnemyBase::MakeHitBody() const
{
    EnemyCollision::EnemyBody body;
    body.position = GetHitCenter();
    body.shape = hitShape_;
    body.radius = scale_.x * hitRadiusRatio_;
    body.halfExtents = { scale_.x * hitHalfRatio_.x, scale_.y * hitHalfRatio_.y, scale_.z * hitHalfRatio_.z };
    body.strength = strength_;
    body.isPushable = isPushable_;
    return body;
}

void EnemyBase::Update(const EnemyUpdateContext& ctx)
{
    if (isDead_ || !object3d_) return;

    lifeTime_ += ctx.deltaTime;

    // 1. 挙動（派生クラスが anchorLocal_ を動かす）
    UpdateBehavior(ctx);

    // 2. ステージ傾斜に合わせてワールド座標を導出
    //    こうしておくと、傾けて地面が動いても敵だけ取り残されない
    Vector3 world = StageLocalToWorld(anchorLocal_, ctx.stageTilt, ctx.pivot);
    position_.x = world.x;
    position_.z = world.z;

    // 3. 地面へ吸着（地形メッシュがあればポリゴン、無ければ傾斜平面）
    bool hasGround = false;
    Vector3 normal{ 0.0f, 1.0f, 0.0f };
    float groundedY = position_.y;

    // 初回だけ「currentY を無視して最上段の床」を取る。
    // そうしないと、地形が高い場所にスポーンさせたときに
    // 自力登坂限界 (kMaxStepUp = 0.35m) で床が候補から外れて落下してしまう。
    if (needsGroundSnap_)
    {
        float snapY = SlimePhysics::CalculateGroundedCenterYEx(
            position_.x, position_.z, SlimePhysics::kIgnoreCurrentY, ctx.stageTilt, groundOffset_,
            &hasGround, &normal, ctx.pivot, false);

        if (hasGround)
        {
            position_.y = snapY;
            groundNormal_ = normal;
            rotation_.x = std::atan2(groundNormal_.z, groundNormal_.y);
            rotation_.z = -std::atan2(groundNormal_.x, groundNormal_.y);
            needsGroundSnap_ = false;
        }
        else if (lifeTime_ > 0.5f)
        {
            // 地面が見つからないまま。以降は通常処理（落下）に任せる
            needsGroundSnap_ = false;
        }
    }

    if (!needsGroundSnap_)
    {
        groundedY = SlimePhysics::CalculateGroundedCenterYEx(
            position_.x, position_.z, position_.y, ctx.stageTilt, groundOffset_,
            &hasGround, &normal, ctx.pivot, true);
    }

    if (needsGroundSnap_)
    {
        // 初回スナップ待ち。位置は動かさない
    }
    else if (hasGround)
    {
        // 段差でワープしないよう少しだけ補間して追従
        float follow = (std::min)(1.0f, ctx.deltaTime * 30.0f);
        position_.y += (groundedY - position_.y) * follow;
        groundNormal_ = normal;
    }
    else
    {
        // 足場が無い（島の外へ出た）。落下させて奈落で消す
        position_.y -= 18.0f * ctx.deltaTime;
        if (position_.y < -80.0f)
        {
            Defeat();
            return;
        }
    }

    // 4. 姿勢：地形法線に沿って傾け、Y は自分の向き
    float targetRotX = std::atan2(groundNormal_.z, groundNormal_.y);
    float targetRotZ = -std::atan2(groundNormal_.x, groundNormal_.y);
    float lerp = (std::min)(1.0f, ctx.deltaTime * 20.0f);
    rotation_.x += (targetRotX - rotation_.x) * lerp;
    rotation_.z += (targetRotZ - rotation_.z) * lerp;
    rotation_.y = yaw_;

    // 5. 描画オブジェクトとコライダーへ反映
    Vector3 visualPos = position_;
    visualPos.y += GetVisualOffsetY();

    object3d_->SetTranslate(visualPos);
    object3d_->SetRotate(rotation_);
    object3d_->SetScale(GetRenderScale());
    object3d_->SetDeltaTime(ctx.deltaTime);
    object3d_->Update();

    RefreshCollider();
}

void EnemyBase::Draw(const RenderContext& ctx)
{
    if (isDead_ || !object3d_) return;

    // Object3d::Draw(ctx) が modelData_.material.textureIndex からテクスチャを解決してくれる
    object3d_->Draw(ctx);
}

void EnemyBase::Finalize()
{
    if (collider_ && CollisionManager::GetInstance())
    {
        CollisionManager::GetInstance()->UnregisterCollider(collider_.get());
    }
    collider_.reset();
    object3d_.reset();
    isDead_ = true;
}
