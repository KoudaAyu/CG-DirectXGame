#include "Obstacle.h"
#include "TextureManager.h"

#include "Baziru3_Engine/Framework/Collision/CollisionManager.h"

void Obstacle::Initialize(Object3dCom* object3dCom, Camera* camera, const Vector3& position, float radius, const std::string& modelFilename, const Vector3& scale, const Vector3& rotation)
{
    object3dCom_ = object3dCom;
    position_ = position;
    radius_ = radius;
    rotation_ = rotation;

    // 指定されたモデルファイルをロード
    std::string filename = modelFilename.empty() ? "fence.obj" : modelFilename;
    Object3d::ModelData model = Object3d::LoadObjFile("Resources", filename);
    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(object3dCom, model);
    object3d_->SetCamera(camera);

    object3d_->SetTranslate(position_);
    object3d_->SetRotate(rotation_);
    object3d_->SetScale({ scale.x * radius_, scale.y * radius_, scale.z * radius_ });

    if (filename == "container.obj" || filename.find("container") != std::string::npos)
    {
        rot1_ = { 0.0f, 0.0f, 0.0f };
        Vector3 boxSize = { 2.2f * scale.x, 2.2f * scale.y, 5.0f * scale.z };
        collider_ = std::make_unique<BoxCollider>(boxSize, &position_, &rotation_, CollisionAttribute::Obstacle);
        CollisionManager::GetInstance()->RegisterCollider(collider_.get());
    }
    else
    {
        // X字のフェンスを構成する2枚の板の回転角 (45度と-45度) を設定
        rot1_ = { 0.0f, 0.785398f, 0.0f };
        rot2_ = { 0.0f, -0.785398f, 0.0f };

        Vector3 boxSize = { 4.24f * radius_, 2.0f * radius_, 0.4f * radius_ };
        
        collider_ = std::make_unique<BoxCollider>(boxSize, &position_, &rot1_, CollisionAttribute::Obstacle);
        collider2_ = std::make_unique<BoxCollider>(boxSize, &position_, &rot2_, CollisionAttribute::Obstacle);
        CollisionManager::GetInstance()->RegisterCollider(collider_.get());
        CollisionManager::GetInstance()->RegisterCollider(collider2_.get());
    }
    
    // 超高速化のためMeshColliderの重いポリゴン判定は登録せず軽量なBoxColliderのみ使用
    // meshCollider_ = std::make_unique<MeshCollider>(object3d_.get(), CollisionAttribute::Obstacle);
    // CollisionManager::GetInstance()->RegisterCollider(meshCollider_.get());

    // テクスチャ設定
    if (model.material.textureFilePath.empty())
    {
        defaultTextureIndex_ = TextureManager::GetInstance()->Load("Resources/fence.png");
    }

    object3d_->Update();
}

void Obstacle::Update()
{
    // 静的オブジェクトのため毎フレームの重い行列再計算をスキップして爆速化
    // object3d_->Update();
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

    object3dCom_->Draw(object3d_.get(), obsCtx, modelData, true);
}

void Obstacle::Finalize()
{
    if (meshCollider_)
    {
        CollisionManager::GetInstance()->UnregisterCollider(meshCollider_.get());
        meshCollider_.reset();
    }
    if (collider_)
    {
        CollisionManager::GetInstance()->UnregisterCollider(collider_.get());
        collider_.reset();
    }
    if (collider2_)
    {
        CollisionManager::GetInstance()->UnregisterCollider(collider2_.get());
        collider2_.reset();
    }
    if (object3d_)
    {
        object3d_.reset();
    }
}

