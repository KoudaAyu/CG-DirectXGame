#include "TutorialSign.h"
#include "TextureManager.h"
#include <cmath>

void TutorialSign::Initialize(Object3dCom* object3dCom, Camera* camera, const Vector3& position, const std::string& message, float triggerRadius)
{
    object3dCom_ = object3dCom;
    position_ = position;
    message_ = message;
    triggerRadius_ = triggerRadius;

    // plane.obj を看板の看板板として使用
    Object3d::ModelData model = Object3d::LoadObjFile("Resources", "plane.obj");
    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(object3dCom, model);
    object3d_->SetCamera(camera);

    // 地面から少し浮かせ、看板らしく見えるように直立（X軸に90度回転）させる
    Vector3 drawPos = position_;
    drawPos.y = 0.5f; // 少し浮かす
    object3d_->SetTranslate(drawPos);
    object3d_->SetScale({ 0.8f, 0.8f, 0.8f });
    object3d_->SetRotate({ 1.570796f, 0.0f, 0.0f }); // 直立させる

    // 看板らしく木目調のフェンステクスチャをロード
    if (model.material.textureFilePath.empty())
    {
        defaultTextureIndex_ = TextureManager::GetInstance()->Load("Resources/fence.png"); // 木目調を流用
    }

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
