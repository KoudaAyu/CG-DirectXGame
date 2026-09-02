#include "Target.h"
#include "TextureManager.h"
#include <algorithm>
#include <cmath>

void Target::Initialize(Object3dCom* object3dCom, Camera* camera, const Vector3& position, float radius)
{
    object3dCom_ = object3dCom;
    position_ = position;
    radius_ = radius;

    // shooting_target.obj (木脚スタンド付きの立体射撃標的) を使用
    Object3d::ModelData model = Object3d::LoadObjFile("Resources", "shooting_target.obj");
    model.material.textureFilePath = "Resources/target.png";
    
    defaultTextureIndex_ = TextureManager::GetInstance()->Load("Resources/target.png");
    model.material.textureIndex = defaultTextureIndex_;

    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(object3dCom, model);
    object3d_->SetCamera(camera);

    Vector3 drawPos = position_;
    drawPos.y = 0.0f; // 地面に自然に接地
    object3d_->SetTranslate(drawPos);
    object3d_->SetScale({ 1.0f, 1.0f, 1.0f });
    object3d_->SetRotate({ 0.0f, 0.0f, 0.0f });

    hp_ = maxHp_ = 3;
    isDead_ = false;

    object3d_->Update();
}

void Target::Update(float deltaTime)
{
    if (!object3d_ || isDead_) return;

    // HPに応じて色を変える（ダメージを受けると赤みが強くなる）
    float hpRatio = static_cast<float>(hp_) / static_cast<float>(maxHp_);
    object3d_->SetColor({ 1.0f, hpRatio, hpRatio, 1.0f });

    object3d_->Update();
}

void Target::Draw(const RenderContext& ctx)
{
    if (!object3d_ || isDead_) return;

    RenderContext targetCtx = ctx;
    const Object3d::ModelData& modelData = object3d_->GetModelData();
    uint32_t texIdx = (defaultTextureIndex_ != TextureManager::kInvalidTextureIndex) ? defaultTextureIndex_ : modelData.material.textureIndex;
    if (texIdx != TextureManager::kInvalidTextureIndex)
    {
        targetCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(texIdx);
    }

    object3dCom_->Draw(object3d_.get(), targetCtx, modelData, true);
}

void Target::Finalize()
{
    if (object3d_)
    {
        object3d_.reset();
    }
}

void Target::OnHit(int damage)
{
    if (isDead_) return;

    hp_ -= damage;
    if (hp_ <= 0)
    {
        hp_ = 0;
        isDead_ = true;
    }
}

void Target::Reset()
{
    hp_ = maxHp_;
    isDead_ = false;
    if (object3d_)
    {
        object3d_->SetTranslate(position_);
        object3d_->SetRotate({ 0.0f, 0.0f, 0.0f });
        object3d_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        object3d_->Update();
    }
}
