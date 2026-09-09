#define NOMINMAX
#include "Application/GameObject/GrowthCube.h"

#include "Baziru3_Engine/Core/Base/Matrix4x4.h"
#include "Baziru3_Engine/Core/Base/DirectXCom.h"
#include "Baziru3_Engine/Core/Base/Allocator/ConstantBufferAllocator.h"
#include "Baziru3_Engine/Core/Base/Pipeline/PipelineStateManager.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3dCom.h"
#include "Baziru3_Engine/Graphics/2D/Texture/TextureManager.h"
#include "Application/GameObject/SlimeMesh.h"
#include "Application/GameObject/SlimeManager.h"
#include "Application/GameObject/Slime.h"
#include "SceneManager.h"
#include "Light.h"

#include <algorithm>
#include <cmath>
#include <cstring>

// ===================================================================
// 見た目の共有パラメータ
// ===================================================================
float GrowthCube::sHoverAmplitude = 0.16f;
float GrowthCube::sHoverSpeed = 2.6f;
float GrowthCube::sSpinSpeed = 1.4f;
float GrowthCube::sHeightOffset = 0.55f;
float GrowthCube::sCollectSeconds = 0.50f;
float GrowthCube::sGamingTimeScale = 0.55f;
float GrowthCube::sGamingSpaceScale = 0.035f;
float GrowthCube::sGamingGain = 1.0f;
float GrowthCube::sPickupRadius = 0.35f;

namespace
{
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kTwoPi = kPi * 2.0f;

    /// @brief 行ベクトル規約（v * M）での回転適用
    inline Vector3 ApplyRotation(const Vector3& v, const Matrix4x4& m)
    {
        return {
            v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0],
            v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1],
            v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2]
        };
    }

    /// @brief GamePlayScene が地面に掛けているのと同じ回転行列 R = Rx(pitch) * Rz(-roll)
    inline Matrix4x4 BuildStageRotation(const Vector2& stageTilt)
    {
        return Multiply(MakeRotateXMatrix(stageTilt.x), MakeRotateZMatrix(-stageTilt.y));
    }

    /// @brief 3x3 回転行列から engine のオイラー角 (rx, ry, rz) を逆算
    /// @note engine の MakeAffineMatrix は R = Rx(x) * Ry(y) * Rz(z)（行ベクトル規約）
    Vector3 MatrixToEulerXYZ(const Matrix4x4& R)
    {
        Vector3 euler;
        float sy = -R.m[0][2];
        sy = std::clamp(sy, -1.0f, 1.0f);
        euler.y = std::asin(sy);

        float cy = std::cos(euler.y);
        if (std::abs(cy) > 1e-4f)
        {
            euler.x = std::atan2(R.m[1][2], R.m[2][2]);
            euler.z = std::atan2(R.m[0][1], R.m[0][0]);
        }
        else
        {
            euler.x = std::atan2(-R.m[2][1], R.m[1][1]);
            euler.z = 0.0f;
        }
        return euler;
    }
}

Vector4 GrowthCube::SampleGamingColor(float time, const Vector3& position)
{
    // GamePlaySceneFx::EvaluateGamingField と同じ Inigo Quilez のコサインパレット。
    // 式をそろえてあるので、本体の色とまわりのパーティクルの色が連動して流れる
    const float u = time * sGamingTimeScale
                  + (position.x + position.y * 0.6f + position.z) * sGamingSpaceScale;

    const float r = 0.5f + 0.5f * std::cos(kTwoPi * u);
    const float g = 0.5f + 0.5f * std::cos(kTwoPi * (u + 1.0f / 3.0f));
    const float b = 0.5f + 0.5f * std::cos(kTwoPi * (u + 2.0f / 3.0f));

    return { std::clamp(r * sGamingGain, 0.0f, 1.0f),
             std::clamp(g * sGamingGain, 0.0f, 1.0f),
             std::clamp(b * sGamingGain, 0.0f, 1.0f),
             1.0f };
}

void GrowthCube::Initialize(Object3dCom* object3dCom, Camera* camera, const Vector3& stageLocalPos, float size)
{
    object3dCom_ = object3dCom;
    camera_ = camera;
    anchorLocal_ = stageLocalPos;
    position_ = stageLocalPos;
    baseSize_ = (size > 0.01f) ? size : 0.85f;

    currentScale_ = 1.0f;
    rotation_ = { 0.0f, 0.0f, 0.0f };
    state_ = State::Active;
    collectTimer_ = 0.0f;
    respawnTimer_ = 0.0f;
    lifeTime_ = 0.0f;
    spin_ = 0.0f;
    needsGroundSnap_ = true;
    collectedBySlime_ = nullptr;
    autoRespawn_ = false;

    // プレイヤーと同じ球メッシュ。分割はプレイヤー（64x32）より控えめでよい
    modelData_ = SlimeMesh::GenerateSphere(32, 16, 0.5f);
    textureIndex_ = TextureManager::GetInstance()->Load("Resources/uvChecker.png");
    modelData_.material.textureIndex = textureIndex_;

    // Slime シェーダーのパラメータ。プレイヤーより少しだけよく揺れて、よく光る
    slimeParams_ = SlimeParamsCPU{};
    slimeParams_.wobbleStrength = 0.20f;
    slimeParams_.wobbleFrequency = 5.0f;
    slimeParams_.fresnelPower = 2.2f;
    slimeParams_.envReflection = 0.55f;
    slimeParams_.innerGlow = 0.85f;
    slimeParams_.specularShininess = 72.0f;
    slimeParams_.baseColor = SampleGamingColor(0.0f, position_);

    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(object3dCom_, modelData_);
    object3d_->SetCamera(camera_);
    object3d_->SetTranslate(position_);
    object3d_->SetScale({ baseSize_, baseSize_, baseSize_ });
    object3d_->SetRotate(rotation_);
    object3d_->SetEnableLighting(true);
    object3d_->Update();
}

bool GrowthCube::Update(float deltaTime, const Vector2& stageTilt, const Vector2& pivot, SlimeManager* slimeManager)
{
    if (!object3d_) return false;

    if (state_ == State::Inactive)
    {
        if (autoRespawn_)
        {
            respawnTimer_ += deltaTime;
            if (respawnTimer_ >= respawnCooldown_)
            {
                Respawn();
            }
        }
        return false;
    }

    lifeTime_ += deltaTime;
    slimeParams_.time = lifeTime_;

    bool collectedThisFrame = false;

    const Matrix4x4 rTilt = BuildStageRotation(stageTilt);
    const Vector3 pivot3 = { pivot.x, 0.0f, pivot.y };

    if (state_ == State::Active)
    {
        // --- 自転（Y 軸まわり。少し傾けたほうが立体感が出る）---
        spin_ += sSpinSpeed * deltaTime;
        if (spin_ > kTwoPi) spin_ -= kTwoPi;

        const Matrix4x4 rSpin = Multiply(MakeRotateXMatrix(0.18f), MakeRotateYMatrix(spin_));
        rotation_ = MatrixToEulerXYZ(Multiply(rSpin, rTilt));

        // --- 1. ステージ傾斜に合わせて XZ のワールド座標を出す（Coin と同じ変換）---
        const Vector3 world = ApplyRotation(anchorLocal_ - pivot3, rTilt) + pivot3;
        position_.x = world.x;
        position_.z = world.z;

        // --- 2. 床の高さを毎フレーム取り直す ---
        // 【重要】レイキャストは「いまの（傾いた）地形メッシュの行列」に対して行われるので、
        // 必ずワールド XZ ＋ 実際の stageTilt / pivot で問い合わせること。
        // ステージローカル座標のまま tilt=0 で聞くと、傾けた瞬間に床を見失う
        //
        // 初回（と、エディタで動かされた直後）だけ「最上面」を取る。
        // 2回目以降は「頭より下で一番高い床」にして、上の段へ吸い上げられるのを防ぐ
        bool hasGround = false;
        Vector3 normal{ 0.0f, 1.0f, 0.0f };
        const float currentYArg = needsGroundSnap_ ? SlimePhysics::kIgnoreCurrentY : position_.y;

        const float floorY = SlimePhysics::CalculateGroundHeightEx(
            position_.x, position_.z, currentYArg, stageTilt,
            &hasGround, &normal, pivot, false, 0.0f);

        // --- 3. 上下ホバリング ---
        const float hoverY = std::sin(lifeTime_ * sHoverSpeed) * sHoverAmplitude;

        if (hasGround)
        {
            position_.y = floorY + sHeightOffset + hoverY;
            groundNormal_ = normal;
            needsGroundSnap_ = false;
        }
        else
        {
            // 床が見つからない（島の外に置かれた）。落とさず、その場に浮かせたままにする。
            // コインと同じ方針で、物理は一切やらない
            position_.y = world.y + sHeightOffset + hoverY;
        }

        currentScale_ = 1.0f;

        // --- ゲーミング色 ---
        slimeParams_.baseColor = SampleGamingColor(lifeTime_, position_);
        slimeParams_.baseColor.w = 0.95f;

        // ぷるぷる。ゆっくり脈打たせて「生きている」感じを出す
        slimeParams_.squashStretch = {
            0.06f * std::sin(lifeTime_ * 3.1f),
            -0.06f * std::sin(lifeTime_ * 3.1f),
            0.06f * std::sin(lifeTime_ * 3.1f),
        };

        // --- スライムとの接触判定 ---
        collectedThisFrame = CheckSlimeCollision(slimeManager);
    }
    else if (state_ == State::Collecting)
    {
        collectTimer_ += deltaTime;
        const float duration = (std::max)(0.05f, sCollectSeconds);
        const float progress = std::clamp(collectTimer_ / duration, 0.0f, 1.0f);

        // 高速回転
        spin_ += 9.0f * deltaTime;
        rotation_ = MatrixToEulerXYZ(Multiply(MakeRotateYMatrix(spin_), rTilt));

        // 吸い込まれる先。合体で実体が消えることがあるので毎回見に行かない
        // （collectedBySlime_ は SlimeManager が erase するとダングリングするため、
        //   位置は取得した瞬間に控えたものを使う）
        const Vector3 target = collectTarget_;

        if (progress < 0.30f)
        {
            // 前半: ボヨン！と 1.55 倍まで膨らむ
            const float p = progress / 0.30f;
            currentScale_ = 1.0f + 0.55f * std::sin(p * (kPi * 0.5f));
            position_ = collectStartPos_;
            position_.y += 0.30f * std::sin(p * kPi);
        }
        else
        {
            // 後半: 縮みながら加速してスライムへ吸い込まれる
            const float p = (progress - 0.30f) / 0.70f;
            currentScale_ = 1.55f * (1.0f - p);

            const float t = p * p;
            position_ = {
                collectStartPos_.x + (target.x - collectStartPos_.x) * t,
                collectStartPos_.y + (target.y - collectStartPos_.y) * t,
                collectStartPos_.z + (target.z - collectStartPos_.z) * t,
            };
        }

        // 消えるまでゲーミングのまま。縮むほど白く飛ばす
        Vector4 color = SampleGamingColor(lifeTime_, position_);
        const float whiten = 1.0f - std::clamp(currentScale_ / 1.55f, 0.0f, 1.0f);
        slimeParams_.baseColor = {
            color.x + (1.0f - color.x) * whiten,
            color.y + (1.0f - color.y) * whiten,
            color.z + (1.0f - color.z) * whiten,
            0.95f,
        };
        slimeParams_.impulseStrength = (std::max)(0.0f, 1.0f - progress);

        if (progress >= 1.0f)
        {
            state_ = State::Inactive;
            respawnTimer_ = 0.0f;
        }
    }

    const float s = baseSize_ * currentScale_;
    object3d_->SetTranslate(position_);
    object3d_->SetRotate(rotation_);
    object3d_->SetScale({ s, s, s });
    object3d_->Update();

    return collectedThisFrame;
}

bool GrowthCube::CheckSlimeCollision(SlimeManager* slimeManager)
{
    if (!slimeManager || state_ != State::Active) return false;

    const float cubeRadius = baseSize_ * 0.5f + sPickupRadius;

    for (const auto& slimePtr : slimeManager->GetSlimes())
    {
        Slime* slime = slimePtr.get();
        if (!slime || !slime->IsActive()) continue;

        // 吸い込まれている最中の個体は判定から外す（位置が補間で飛ぶため）
        if (slime->GetState() == SlimeState::Merging) continue;

        const Vector3 sPos = slime->GetPosition();
        const float dx = sPos.x - position_.x;
        const float dy = sPos.y - position_.y;
        const float dz = sPos.z - position_.z;
        const float distSq = dx * dx + dy * dy + dz * dz;

        const float hitRadius = cubeRadius + slime->GetRadius();
        if (distSq > hitRadius * hitRadius) continue;

        // --- 食べられた ---
        collectedBySlime_ = slime;
        collectTarget_ = sPos;
        state_ = State::Collecting;
        collectTimer_ = 0.0f;
        collectStartPos_ = position_;

        // 残機（＝スライムのサイズ）を1つ増やす
        slime->SetSize(slime->GetSize() + 1);

        // 大喜びのポヨン！
        slime->GetSlimeParams().squashStretch = { 0.35f, -0.38f, 0.35f };
        slime->GetSlimeParams().impulseStrength = 1.0f;
        return true;
    }

    return false;
}

void GrowthCube::Draw(const RenderContext& ctx)
{
    if (state_ == State::Inactive || !object3d_ || !object3dCom_ || !ctx.commandList) return;

    DirectXCom* dx = object3dCom_->GetDirectXCom();
    if (!dx) return;

    auto* cbAllocator = dx->GetCBAllocator();
    if (!cbAllocator) return;

    // Slime シェーダー。engine の初期化時に用意されているので、どのシーンからでも使える
    auto rootSig = PipelineStateManager::GetInstance()->GetRootSignature("Slime");
    auto slimePSO = PipelineStateManager::GetInstance()->GetPipelineState("Slime_Normal");
    if (!rootSig || !slimePSO)
    {
        // フォールバック描画（通常の Object3D PSO）。色だけは合わせておく
        object3d_->SetColor(slimeParams_.baseColor);
        RenderContext localCtx = ctx;
        if (textureIndex_ != TextureManager::kInvalidTextureIndex)
        {
            localCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(textureIndex_);
        }
        object3dCom_->Draw(object3d_.get(), localCtx, modelData_, true);
        return;
    }

    object3d_->PrepareConstantBuffers(dx);

    auto slimeAlloc = cbAllocator->Allocate(sizeof(SlimeParamsCPU));
    if (!slimeAlloc.cpuAddress)
    {
        return; // 定数バッファ枯渇時のクラッシュ防止
    }
    std::memcpy(slimeAlloc.cpuAddress, &slimeParams_, sizeof(SlimeParamsCPU));

    ctx.commandList->SetGraphicsRootSignature(rootSig.Get());
    ctx.commandList->SetPipelineState(slimePSO.Get());

    // 0: Material / 1: TransformationMatrix
    ctx.commandList->SetGraphicsRootConstantBufferView(0, object3d_->GetMaterialGPUAddress());
    ctx.commandList->SetGraphicsRootConstantBufferView(1, object3d_->GetTransformationMatrixGPUAddress());

    // 2: Main Texture
    D3D12_GPU_DESCRIPTOR_HANDLE texHandle{};
    if (textureIndex_ != TextureManager::kInvalidTextureIndex)
    {
        texHandle = TextureManager::GetInstance()->GetSrvHandleGPU(textureIndex_);
    }
    if (texHandle.ptr == 0)
    {
        return; // 未バインド描画による GPU クラッシュを防ぐ
    }
    ctx.commandList->SetGraphicsRootDescriptorTable(2, texHandle);

    // 3: SlimeParams
    ctx.commandList->SetGraphicsRootConstantBufferView(3, slimeAlloc.gpuAddress);

    // 4: DirectionalLight
    if (ctx.light && ctx.light->GetDirectionalLightResource())
    {
        ctx.commandList->SetGraphicsRootConstantBufferView(4, ctx.light->GetDirectionalLightResource()->GetGPUVirtualAddress());
    }
    else
    {
        ctx.commandList->SetGraphicsRootConstantBufferView(4, object3d_->GetDirectionalLightGPUAddress());
    }

    // 5: Camera
    if (ctx.camera && ctx.camera->GetCameraGpuAddress() != 0)
    {
        ctx.commandList->SetGraphicsRootConstantBufferView(5, ctx.camera->GetCameraGpuAddress());
    }

    // 6: Cube Environment Map（スカイボックスの映り込み）
    uint32_t skyboxIndex = SceneManager::GetInstance()->GetSkyboxTextureIndex();
    D3D12_GPU_DESCRIPTOR_HANDLE skyboxHandle{};
    if (skyboxIndex != TextureManager::kInvalidTextureIndex)
    {
        skyboxHandle = TextureManager::GetInstance()->GetSrvHandleGPU(skyboxIndex);
    }
    if (skyboxHandle.ptr == 0)
    {
        skyboxHandle = texHandle;
    }
    ctx.commandList->SetGraphicsRootDescriptorTable(6, skyboxHandle);

    auto vbv = object3d_->GetVertexBufferView();
    ctx.commandList->IASetVertexBuffers(0, 1, &vbv);
    ctx.commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    if (object3d_->HasIndexBuffer())
    {
        auto ibv = object3d_->GetIndexBufferView();
        ctx.commandList->IASetIndexBuffer(&ibv);
        ctx.commandList->DrawIndexedInstanced(static_cast<UINT>(modelData_.indices.size()), 1, 0, 0, 0);
    }
    else
    {
        ctx.commandList->DrawInstanced(static_cast<UINT>(modelData_.vertices.size()), 1, 0, 0);
    }
}

void GrowthCube::Respawn()
{
    state_ = State::Active;
    collectTimer_ = 0.0f;
    respawnTimer_ = 0.0f;
    currentScale_ = 1.0f;
    collectedBySlime_ = nullptr;
    needsGroundSnap_ = true;
    slimeParams_.impulseStrength = 0.0f;
}

void GrowthCube::SetBaseSize(float size)
{
    baseSize_ = (size > 0.01f) ? size : 0.85f;
    // メッシュは半径 0.5 の単位球なので、スケールを変えるだけでよい
    // （旧実装はメッシュを作り直していたが、Object3d::Initialize を呼び直すと
    //   頂点バッファを毎回確保し直すことになるので避けた）
    if (object3d_)
    {
        const float s = baseSize_ * currentScale_;
        object3d_->SetScale({ s, s, s });
    }
}
