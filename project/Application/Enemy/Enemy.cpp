#include "Enemy.h"
#include "Object3d.h"
#include "Object3dCom.h"
#include "TextureManager.h"
#include "RenderContext.h"

void Enemy::Initialize(Object3dCom* object3dCom, Camera* camera)
{
    object3dCom_ = object3dCom;
    camera_ = camera;

    Object3d::ModelData model = Object3d::LoadObjFile("Resources", "plane.obj");
    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(object3dCom_, model);

    object3d_->SetTranslate({ 3.0f, 0.0f, 3.0f });
    object3d_->SetScale({ 1.0f, 1.0f, 1.0f });

    if (model.material.textureFilePath.empty())
    {
        defaultTextureIndex_ = TextureManager::GetInstance()->Load("Resources/uvChecker.png");
    }
}

void Enemy::Update()
{
    if (!object3d_) return;

    object3d_->Update();
}

void Enemy::Draw(const RenderContext& ctx)
{
    if (!object3dCom_ || !object3d_) return;

    RenderContext enemyCtx = ctx;
    const Object3d::ModelData& modelData = object3d_->GetModelData();
    uint32_t texIdx = modelData.material.textureIndex;
    if (texIdx == 0 || texIdx == UINT32_MAX)
    {
        texIdx = defaultTextureIndex_;
    }
    if (enemyCtx.textureHandle.ptr == 0 && texIdx != 0 && texIdx != UINT32_MAX)
    {
        enemyCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(texIdx);
    }

    object3dCom_->Draw(object3d_.get(), enemyCtx, modelData, true);
}

void Enemy::Finalize()
{
    if (object3d_)
    {
        object3d_.reset();
    }
}
