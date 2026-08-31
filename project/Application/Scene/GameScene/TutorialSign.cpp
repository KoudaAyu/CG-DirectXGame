#include "TutorialSign.h"
#include "TextureManager.h"
#include <cmath>

void TutorialSign::Initialize(Object3dCom* object3dCom, Camera* camera, const Vector3& position, const std::string& message, float triggerRadius)
{
    object3dCom_ = object3dCom;
    position_ = position;
    message_ = message;
    triggerRadius_ = triggerRadius;

    // signpost.obj (立体的な木製ミリタリーサインポスト) を使用
    Object3d::ModelData model = Object3d::LoadObjFile("Resources", "signpost.obj");
    model.material.textureFilePath = "Resources/signpost.png";
    defaultTextureIndex_ = TextureManager::GetInstance()->Load("Resources/signpost.png");
    model.material.textureIndex = defaultTextureIndex_;

    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(object3dCom, model);
    object3d_->SetCamera(camera);

    Vector3 drawPos = position_;
    drawPos.y = 0.0f; // 地面に自然に接地
    object3d_->SetTranslate(drawPos);
    object3d_->SetScale({ 0.9f, 0.9f, 0.9f });
    object3d_->SetRotate({ 0.0f, 0.0f, 0.0f });

    object3d_->Update();
}

void TutorialSign::Update(const Vector3& playerPosition)
{
    if (!object3d_) return;

    // プレイヤーとの距離（XZ平面）を測定
    float dx = position_.x - playerPosition.x;
    float dz = position_.z - playerPosition.z;
    float dist = std::sqrt(dx * dx + dz * dz);

    isPlayerNear_ = (dist <= triggerRadius_);

    // プレイヤーが接近している場合は看板を少し光らせる/回転を小さく揺らすなどの演出
    if (isPlayerNear_)
    {
        object3d_->SetColor({ 1.2f, 1.2f, 1.2f, 1.0f }); // 明るくする
    }
    else
    {
        object3d_->SetColor({ 0.8f, 0.8f, 0.8f, 1.0f }); // 少し暗くする
    }

    object3d_->Update();
}

void TutorialSign::Draw(const RenderContext& ctx)
{
    if (!object3d_) return;

    RenderContext signCtx = ctx;
    const Object3d::ModelData& modelData = object3d_->GetModelData();
    uint32_t texIdx = defaultTextureIndex_;
    if (signCtx.textureHandle.ptr == 0 && texIdx != 0 && texIdx != UINT32_MAX)
    {
        signCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(texIdx);
    }

    object3dCom_->Draw(object3d_.get(), signCtx, modelData, true);
}

void TutorialSign::Finalize()
{
    if (object3d_)
    {
        object3d_.reset();
    }
}
