#include "TutorialSign.h"
#include "TextureManager.h"

namespace
{
    constexpr const char* kResourceDir     = "Resources";
    constexpr const char* kSignModelFile   = "signpost.obj";
    constexpr const char* kSignTexturePath = "Resources/signpost.png";
    constexpr float       kGroundY         = 0.0f;
    constexpr Vector3     kSignScale       = { 0.9f, 0.9f, 0.9f };
    constexpr Vector3     kZeroRotation    = { 0.0f, 0.0f, 0.0f };
    constexpr Vector4     kNearTintColor   = { 1.2f, 1.2f, 1.2f, 1.0f }; // 接近時ハイライト
    constexpr Vector4     kIdleTintColor   = { 0.8f, 0.8f, 0.8f, 1.0f }; // 通常時減光
}

void TutorialSign::Initialize(Object3dCom* object3dCom, Camera* camera, const Vector3& position, const std::string& message, float triggerRadius)
{
    object3dCom_ = object3dCom;
    position_ = position;
    message_ = message;
    triggerRadius_ = triggerRadius;

    // signpost.obj (立体的な木製ミリタリーサインポスト) を使用
    Object3d::ModelData model = Object3d::LoadObjFile(kResourceDir, kSignModelFile);
    model.material.textureFilePath = kSignTexturePath;
    defaultTextureIndex_ = TextureManager::GetInstance()->Load(kSignTexturePath);
    model.material.textureIndex = defaultTextureIndex_;

    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(object3dCom, model);
    object3d_->SetCamera(camera);

    Vector3 drawPos = position_;
    drawPos.y = kGroundY; // 地面に自然に接地
    object3d_->SetTranslate(drawPos);
    object3d_->SetScale(kSignScale);
    object3d_->SetRotate(kZeroRotation);

    object3d_->Update();
}

void TutorialSign::Update(const Vector3& playerPosition)
{
    if (!object3d_) return;

    // プレイヤーとの二乗距離（XZ平面）で接近判定（平方根演算を省略）
    const float dx = position_.x - playerPosition.x;
    const float dz = position_.z - playerPosition.z;
    const float distSq = dx * dx + dz * dz;

    isPlayerNear_ = (distSq <= triggerRadius_ * triggerRadius_);
    object3d_->SetColor(isPlayerNear_ ? kNearTintColor : kIdleTintColor);
    object3d_->Update();
}

void TutorialSign::Draw(const RenderContext& ctx)
{
    if (!object3d_) return;

    RenderContext signCtx = ctx;
    const Object3d::ModelData& modelData = object3d_->GetModelData();
    const uint32_t texIdx = (defaultTextureIndex_ != TextureManager::kInvalidTextureIndex) ? defaultTextureIndex_ : modelData.material.textureIndex;
    if (texIdx != TextureManager::kInvalidTextureIndex)
    {
        signCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(texIdx);
    }

    object3dCom_->Draw(object3d_.get(), signCtx, modelData, true);
}

void TutorialSign::Finalize()
{
    object3d_.reset();
}
