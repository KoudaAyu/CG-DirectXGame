#include "Player.h"
#include "Object3d.h"
#include "Object3dCom.h"
#include "TextureManager.h"
#include "SceneManager.h"
#include "RenderContext.h"
#include <Windows.h>
#include "MouseInput.h"
#include "Matrix4x4.h"
#include <cmath>

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

void Player::Update(MouseInput* mouseInput)
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
     if (mouseInput && camera_)
    {
        WindowAPI* win = mouseInput->GetWindowAPI();
        if (win)
        {
            int mx = mouseInput->GetX();
            int my = mouseInput->GetY();
            float clientW = static_cast<float>(win->GetClientWidth());
            float clientH = static_cast<float>(win->GetClientHeight());
            if (clientW > 0.0f && clientH > 0.0f)
            {
                // NDC coordinates
                float nx = (static_cast<float>(mx) / clientW) * 2.0f - 1.0f;
                float ny = 1.0f - (static_cast<float>(my) / clientH) * 2.0f;

                // Prepare clip space positions at near and far (z in [0,1])
                Vector4 clipNear = { nx, ny, 0.0f, 1.0f };
                Vector4 clipFar = { nx, ny, 1.0f, 1.0f };

                // Inverse of view * projection (matches WVP construction used elsewhere)
                Matrix4x4 inv = Inverse(Multiply(camera_->GetViewMatrix(), camera_->GetProjectionMatrix()));

                // このエンジンは行ベクトル方式（clip = pos * VP）なので
                // 逆変換は world = clip * VP^-1 → r = c * inv の順で乗算する
                auto transformClip = [&](const Vector4& c)->Vector3 {
                    Vector3 r;
                    r.x = c.x * inv.m[0][0] + c.y * inv.m[1][0] + c.z * inv.m[2][0] + c.w * inv.m[3][0];
                    r.y = c.x * inv.m[0][1] + c.y * inv.m[1][1] + c.z * inv.m[2][1] + c.w * inv.m[3][1];
                    r.z = c.x * inv.m[0][2] + c.y * inv.m[1][2] + c.z * inv.m[2][2] + c.w * inv.m[3][2];
                    float w = c.x * inv.m[0][3] + c.y * inv.m[1][3] + c.z * inv.m[2][3] + c.w * inv.m[3][3];
                    if (w != 0.0f)
                    {
                        r.x /= w; r.y /= w; r.z /= w;
                    }
                    return r;
                };

                Vector3 worldNear = transformClip(clipNear);
                Vector3 worldFar = transformClip(clipFar);

                // Ray from camera through mouse
                Vector3 dir = { worldFar.x - worldNear.x, worldFar.y - worldNear.y, worldFar.z - worldNear.z };

                // Intersect with ground plane y = 0
                if (std::fabs(dir.y) > 1e-6f)
                {
                    float t = -worldNear.y / dir.y;
                    if (t > 0.0f)
                    {
                        Vector3 hit = { worldNear.x + dir.x * t, 0.0f, worldNear.z + dir.z * t };
                        Vector3 ppos = object3d_->GetTranslate();
                        Vector3 to = { hit.x - ppos.x, 0.0f, hit.z - ppos.z };
                        // Compute yaw; forward is +Z so use atan2(x, z)
                        float yaw = std::atan2(to.x, to.z);
                        Vector3 r = object3d_->GetRotate();
                        r.y = yaw;
                        object3d_->SetRotate(r);
                    }
                }
            }
        }
    }

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
