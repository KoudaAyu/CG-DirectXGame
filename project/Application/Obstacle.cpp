#include "Obstacle.h"
#include "TextureManager.h"
#include "CustomObject3dRenderer.h"

void Obstacle::Initialize(Object3dCom* object3dCom, Camera* camera, const Vector3& position, float radius)
{
    position_ = position;
    radius_ = radius;

    // fence.obj を読み込み
    Object3d::ModelData model = Object3d::LoadObjFile("Resources", "fence.obj");
    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(object3dCom, model);
    object3d_->SetCamera(camera);

    object3d_->SetTranslate(position_);
    object3d_->SetScale({ radius_, radius_, radius_ });

    // テクスチャ設定
    if (model.material.textureFilePath.empty())
    {
        defaultTextureIndex_ = TextureManager::GetInstance()->Load("Resources/fence.png");
    }

    object3d_->Update();
}

void Obstacle::Update()
{
    if (object3d_)
    {
        object3d_->Update();
    }
}

void Obstacle::Draw(const RenderContext& ctx)
{
    if (!object3d_) return;

    RenderContext obsCtx = ctx;
    const Object3d::ModelData& modelData = object3d_->GetModelData();
    uint32_t texIdx = modelData.material.textureIndex;
    if (texIdx == 0 || texIdx == UINT32_MAX)
    {
        texIdx = defaultTextureIndex_;
    }
    if (obsCtx.textureHandle.ptr == 0 && texIdx != 0 && texIdx != UINT32_MAX)
    {
        obsCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(texIdx);
    }

    CustomObject3dRenderer::GetInstance()->Draw(object3d_.get(), obsCtx, modelData, true);
}

void Obstacle::Finalize()
{
    if (object3d_)
    {
        object3d_.reset();
    }
}
