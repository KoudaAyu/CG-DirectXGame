#include "Player.h"
#include "Object3d.h"
#include "Object3dCom.h"
#include "TextureManager.h"
#include "SceneManager.h"
#include "RenderContext.h"
#include <Windows.h>

void Player::Initialize(Object3dCom* object3dCom, Camera* camera)
{
    object3dCom_ = object3dCom;
    camera_ = camera;

    // OBJ を読み込んで Object3d を初期化
    Object3d::ModelData model = Object3d::LoadObjFile("Resources", "plane.obj");
    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(object3dCom_, model);

    // デフォルトのトランスフォーム
    object3d_->SetTranslate({ 0.0f, 0.0f, 0.0f });
    object3d_->SetScale({ 1.0f, 1.0f, 1.0f });

    // テクスチャがない OBJ の場合、デフォルトテクスチャをバインドしておく
    // GPU-based validation でディスクリプタ未初期化エラーが出ないようにするため
    if (model.material.textureFilePath.empty())
    {
        defaultTextureIndex_ = TextureManager::GetInstance()->Load("Resources/uvChecker.png");
    }
}

void Player::Update()
{
    if (!object3d_) return;

    // WASD で移動（簡易実装）。フレーム当たりの移動量は固定値。
    const float kSpeed = 0.05f; // 1フレームあたりの移動量（必要に応じて調整）
    Vector3 pos = object3d_->GetTranslate();

    if ((GetAsyncKeyState('W') & 0x8000) != 0)
    {
        pos.z += kSpeed;
    }
    if ((GetAsyncKeyState('S') & 0x8000) != 0)
    {
        pos.z -= kSpeed;
    }
    if ((GetAsyncKeyState('A') & 0x8000) != 0)
    {
        pos.x -= kSpeed;
    }
    if ((GetAsyncKeyState('D') & 0x8000) != 0)
    {
        pos.x += kSpeed;
    }

    object3d_->SetTranslate(pos);
    object3d_->Update();
}

void Player::Draw(const RenderContext& ctx)
{
    if (!object3dCom_ || !object3d_) return;

    // ctx.textureHandle をモデルのテクスチャインデックスから設定する
    RenderContext playerCtx = ctx;
    const Object3d::ModelData& modelData = object3d_->GetModelData();
    uint32_t texIdx = modelData.material.textureIndex;
    // テクスチャが割り当てられていない場合はデフォルトを使う
    if (texIdx == 0 || texIdx == UINT32_MAX)
    {
        texIdx = defaultTextureIndex_;
    }
    if (playerCtx.textureHandle.ptr == 0 && texIdx != 0 && texIdx != UINT32_MAX)
    {
        playerCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(texIdx);
    }

    object3dCom_->Draw(object3d_.get(), playerCtx, modelData, true);
}

void Player::Finalize()
{
    if (object3d_)
    {
        object3d_.reset();
    }
}
