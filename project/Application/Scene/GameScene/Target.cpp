#include "Target.h"
#include "TextureManager.h"

namespace
{
    constexpr const char* kResourceDir       = "Resources";
    constexpr const char* kTargetModelFile   = "shooting_target.obj";
    constexpr const char* kTargetTexturePath = "Resources/target.png";
    constexpr float       kGroundY           = 0.0f;
    constexpr Vector3     kDefaultScale      = { 1.0f, 1.0f, 1.0f };
    constexpr Vector3     kZeroRotation      = { 0.0f, 0.0f, 0.0f };
    constexpr Vector4     kDefaultColor      = { 1.0f, 1.0f, 1.0f, 1.0f };
}

void Target::Initialize(Object3dCom* object3dCom, Camera* camera, const Vector3& position, float radius)
{
    object3dCom_ = object3dCom;
    position_ = position;
    radius_ = radius;

    // shooting_target.obj (木脚スタンド付きの立体射撃標的) を使用
    Object3d::ModelData model = Object3d::LoadObjFile(kResourceDir, kTargetModelFile);
    model.material.textureFilePath = kTargetTexturePath;

    defaultTextureIndex_ = TextureManager::GetInstance()->Load(kTargetTexturePath);
    model.material.textureIndex = defaultTextureIndex_;

    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(object3dCom, model);
    object3d_->SetCamera(camera);

    Vector3 drawPos = position_;
    drawPos.y = kGroundY; // 地面に自然に接地
    object3d_->SetTranslate(drawPos);
    object3d_->SetScale(kDefaultScale);
    object3d_->SetRotate(kZeroRotation);

    hp_ = maxHp_ = kDefaultMaxHp;
    isDead_ = false;

    object3d_->Update();
}

void Target::Update(float deltaTime)
{
    (void)deltaTime;
    if (!object3d_ || isDead_) return;

    // HPに応じて色を変える（ダメージを受けると赤みが強くなる）
    const float hpRatio = static_cast<float>(hp_) / static_cast<float>(maxHp_);
    object3d_->SetColor({ 1.0f, hpRatio, hpRatio, 1.0f });

    object3d_->Update();
}

void Target::Draw(const RenderContext& ctx)
{
    if (!object3d_ || isDead_) return;

    RenderContext targetCtx = ctx;
    const Object3d::ModelData& modelData = object3d_->GetModelData();
    const uint32_t texIdx = (defaultTextureIndex_ != TextureManager::kInvalidTextureIndex) ? defaultTextureIndex_ : modelData.material.textureIndex;
    if (texIdx != TextureManager::kInvalidTextureIndex)
    {
        targetCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(texIdx);
    }

    object3dCom_->Draw(object3d_.get(), targetCtx, modelData, true);
}

void Target::Finalize()
{
    object3d_.reset();
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
        object3d_->SetRotate(kZeroRotation);
        object3d_->SetColor(kDefaultColor);
        object3d_->Update();
    }
}
