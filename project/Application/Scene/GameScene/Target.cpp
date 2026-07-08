#include "Target.h"
#include "TextureManager.h"
#include <algorithm>
#include <cmath>

void Target::Initialize(Object3dCom* object3dCom, Camera* camera, const Vector3& position, float radius)
{
    object3dCom_ = object3dCom;
    position_ = position;
    radius_ = radius;

    // teapot.obj (立体的なティーポット) を読み込んで的オブジェクトとして使用
    Object3d::ModelData model = Object3d::LoadObjFile("Resources", "teapot.obj");
    
    // チェッカーボードテクスチャをロードしてモデル情報へ強制設定
    defaultTextureIndex_ = TextureManager::GetInstance()->Load("Resources/checkerBoard.png");
    model.material.textureIndex = defaultTextureIndex_;

    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(object3dCom, model);
    object3d_->SetCamera(camera);

    object3d_->SetTranslate(position_);
    // 的の立体サイズに合わせてスケールを調整
    object3d_->SetScale({ radius_ * 0.7f, radius_ * 0.7f, radius_ * 0.7f });
    object3d_->SetRotate({ 0.0f, 0.0f, 0.0f });

    hp_ = maxHp_ = 3;
    isDead_ = false;

    object3d_->Update();
}

void Target::Update(float deltaTime)
{
    if (!object3d_ || isDead_) return;

    // 的を常にゆっくりと回転させておく (見つけやすく、見栄えを良くするため)
    Vector3 rot = object3d_->GetRotate();
    rot.y += 1.0f * deltaTime;
    if (rot.y > 6.2831853f) rot.y -= 6.2831853f;
    object3d_->SetRotate(rot);

    // HPに応じて色を変える（ダメージを受けると赤みが強くなる）
    float hpRatio = static_cast<float>(hp_) / static_cast<float>(maxHp_);
    object3d_->SetColor({ 1.0f, hpRatio, hpRatio, 1.0f }); // HPが低いと赤くなる

    // バウンド演出などのためにスケールを微調整（被弾時に脈動させるなどの拡張が可能）
    object3d_->Update();
}

void Target::Draw(const RenderContext& ctx)
{
    if (!object3d_ || isDead_) return;

    RenderContext targetCtx = ctx;
    const Object3d::ModelData& modelData = object3d_->GetModelData();
    uint32_t texIdx = defaultTextureIndex_;
    if (targetCtx.textureHandle.ptr == 0 && texIdx != 0 && texIdx != UINT32_MAX)
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
