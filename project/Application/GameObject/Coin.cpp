#include "Application/GameObject/Coin.h"

#include "Baziru3_Engine/Graphics/3D/Object/Object3dCom.h"
#include "Application/GameObject/SlimePhysics.h"
#include "Application/Enemy/EnemyBase.h"

#include <cmath>

Coin::~Coin()
{
    Finalize();
}

void Coin::Initialize(Object3dCom* object3dCom, Camera* camera,
                      const Object3d::ModelData& modelData, const Vector3& stageLocalPos)
{
    if (!object3dCom) return;

    anchorLocal_ = stageLocalPos;
    anchorLocal_.y = 0.0f; // Y は毎フレーム床から取り直すので持たない
    position_ = stageLocalPos;

    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(object3dCom, modelData);
    object3d_->SetCamera(camera);
    object3d_->SetEnableLighting(true);
    object3d_->SetTranslate(position_);
    object3d_->SetScale({ scale_, scale_, scale_ });
    object3d_->Update();

    needsGroundSnap_ = true;
    isCollected_ = false;
    lifeTime_ = 0.0f;
}

void Coin::Update(float deltaTime, const Vector2& stageTilt, const Vector2& pivot,
                  float spinSpeed, float bobHeight, float bobSpeed, float heightOffset)
{
    if (!object3d_ || isCollected_) return;

    lifeTime_ += deltaTime;
    spin_ += spinSpeed * deltaTime;
    if (spin_ > 6.28318530718f) spin_ -= 6.28318530718f;

    // 1. ステージ傾斜に合わせてワールド座標を導出（敵と同じ変換）
    Vector3 world = EnemyBase::StageLocalToWorld(anchorLocal_, stageTilt, pivot);
    position_.x = world.x;
    position_.z = world.z;

    // 2. 床の高さを取り直す。
    //    初回だけ「最上段」を取る（配置可能な場所は床が1枚だけなので、これで正しい床に乗る）。
    //    2回目以降は「頭より下で一番高い床」。上の段へ吸い上げられるのを防ぐ
    bool hasGround = false;
    Vector3 normal{ 0.0f, 1.0f, 0.0f };
    float currentYArg = needsGroundSnap_ ? SlimePhysics::kIgnoreCurrentY : position_.y;

    float floorY = SlimePhysics::CalculateGroundHeightEx(
        position_.x, position_.z, currentYArg, stageTilt,
        &hasGround, &normal, pivot, false, 0.0f);

    if (hasGround)
    {
        float targetY = floorY + heightOffset;
        if (needsGroundSnap_)
        {
            position_.y = targetY;
            groundNormal_ = normal;
            needsGroundSnap_ = false;
        }
        else
        {
            // 段差でワープしないよう少しだけ補間して追従
            float follow = (std::min)(1.0f, deltaTime * 25.0f);
            position_.y += (targetY - position_.y) * follow;
            groundNormal_ = normal;
        }
    }
    // 床が見つからなくても落とさない。コインは物理を持たず、その場に浮いたまま残る

    // 3. 姿勢：地形法線に沿って寝かせつつ、Y 軸でくるくる回す
    float targetRotX = std::atan2(groundNormal_.z, groundNormal_.y);
    float targetRotZ = -std::atan2(groundNormal_.x, groundNormal_.y);
    float lerp = (std::min)(1.0f, deltaTime * 20.0f);
    rotation_.x += (targetRotX - rotation_.x) * lerp;
    rotation_.z += (targetRotZ - rotation_.z) * lerp;
    rotation_.y = spin_;

    // 4. 描画オブジェクトへ反映（ふわふわは見た目だけ）
    Vector3 visualPos = position_;
    visualPos.y += std::sin(lifeTime_ * bobSpeed) * bobHeight;

    object3d_->SetTranslate(visualPos);
    object3d_->SetRotate(rotation_);
    object3d_->SetScale({ scale_, scale_, scale_ });
    object3d_->Update();
}

void Coin::Draw(const RenderContext& ctx)
{
    if (!object3d_ || isCollected_) return;
    object3d_->Draw(ctx);
}

void Coin::Finalize()
{
    object3d_.reset();
}
