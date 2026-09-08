#include "GrowthCube.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3dCom.h"
#include "Baziru3_Engine/Graphics/2D/Texture/TextureManager.h"
#include "Application/GameObject/SlimeManager.h"
#include "Application/GameObject/Slime.h"
#include <cmath>
#include <algorithm>

namespace
{
    constexpr float kTwoPi = 6.283185307f;

    // 3x3 回転行列（行ベクトル形式）からエンジンのオイラー角 (rx, ry, rz) を逆算
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

Object3d::ModelData GrowthCube::GeneratePrimitiveCube(float size)
{
    Object3d::ModelData modelData;
    float h = size * 0.5f;

    // 6面 × 4頂点 = 24頂点（各面独立法線によりエッジのシャープなキューブを形成）
    modelData.vertices.reserve(24);
    modelData.indices.reserve(36);

    // 面定義ヘルパーラムダ
    auto AddFace = [&](const Vector3& normal,
                       const Vector3& p0, const Vector3& p1,
                       const Vector3& p2, const Vector3& p3)
    {
        uint32_t baseIdx = static_cast<uint32_t>(modelData.vertices.size());

        Sprite::VertexData v0{ { p0.x, p0.y, p0.z, 1.0f }, { 0.0f, 0.0f }, normal };
        Sprite::VertexData v1{ { p1.x, p1.y, p1.z, 1.0f }, { 1.0f, 0.0f }, normal };
        Sprite::VertexData v2{ { p2.x, p2.y, p2.z, 1.0f }, { 1.0f, 1.0f }, normal };
        Sprite::VertexData v3{ { p3.x, p3.y, p3.z, 1.0f }, { 0.0f, 1.0f }, normal };

        modelData.vertices.push_back(v0);
        modelData.vertices.push_back(v1);
        modelData.vertices.push_back(v2);
        modelData.vertices.push_back(v3);

        // 時計回り三角形 (0, 1, 2) と (0, 2, 3)
        modelData.indices.push_back(baseIdx + 0);
        modelData.indices.push_back(baseIdx + 1);
        modelData.indices.push_back(baseIdx + 2);

        modelData.indices.push_back(baseIdx + 0);
        modelData.indices.push_back(baseIdx + 2);
        modelData.indices.push_back(baseIdx + 3);
    };

    // 1. 前面 (+Z)
    AddFace({ 0.0f, 0.0f, 1.0f },
            { -h,  h, h }, {  h,  h, h },
            {  h, -h, h }, { -h, -h, h });

    // 2. 背面 (-Z)
    AddFace({ 0.0f, 0.0f, -1.0f },
            {  h,  h, -h }, { -h,  h, -h },
            { -h, -h, -h }, {  h, -h, -h });

    // 3. 右面 (+X)
    AddFace({ 1.0f, 0.0f, 0.0f },
            { h,  h,  h }, { h,  h, -h },
            { h, -h, -h }, { h, -h,  h });

    // 4. 左面 (-X)
    AddFace({ -1.0f, 0.0f, 0.0f },
            { -h,  h, -h }, { -h,  h,  h },
            { -h, -h,  h }, { -h, -h, -h });

    // 5. 上面 (+Y)
    AddFace({ 0.0f, 1.0f, 0.0f },
            { -h, h, -h }, {  h, h, -h },
            {  h, h,  h }, { -h, h,  h });

    // 6. 下面 (-Y)
    AddFace({ 0.0f, -1.0f, 0.0f },
            { -h, -h,  h }, {  h, -h,  h },
            {  h, -h, -h }, { -h, -h, -h });

    modelData.boundingRadius = size * 0.866025f; // sqrt(3)/2 * size
    return modelData;
}

void GrowthCube::Initialize(Object3dCom* object3dCom, Camera* camera, const Vector3& basePos, float size)
{
    object3dCom_ = object3dCom;
    camera_ = camera;
    basePosition_ = basePos;
    baseSize_ = size;
    currentWorldPos_ = basePos;
    currentScale_ = { 1.0f, 1.0f, 1.0f };
    currentRotation_ = { 0.0f, 0.0f, 0.0f };
    state_ = State::Active;
    collectTimer_ = 0.0f;
    respawnTimer_ = 0.0f;
    hoverTimer_ = 0.0f;
    currentAngle_ = 0.0f;
    collectedBySlime_ = nullptr;
    autoRespawn_ = false;

    // プリミティブキューブメッシュの生成
    modelData_ = GeneratePrimitiveCube(baseSize_);

    // テクスチャ設定
    textureIndex_ = TextureManager::GetInstance()->Load("Resources/uvChecker.png");
    modelData_.material.textureIndex = textureIndex_;

    // Object3d の初期化
    object3d_ = std::make_unique<Object3d>();
    if (object3d_)
    {
        object3d_->Initialize(object3dCom_, modelData_);
        object3d_->SetCamera(camera_);
        object3d_->SetTranslate(currentWorldPos_);
        object3d_->SetScale(currentScale_);
        object3d_->SetRotate(currentRotation_);
        // 輝くゴールド/アンバーカラー
        object3d_->SetColor({ 1.0f, 0.82f, 0.15f, 1.0f });
        object3d_->SetEnableLighting(true);
        object3d_->Update();
    }
}

void GrowthCube::Update(float deltaTime, const Vector2& stageTilt, const Vector2& pivot, SlimeManager* slimeManager)
{
    if (!object3d_) return;

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
        return;
    }

    // 1. ステージ傾斜の合成回転行列
    Matrix4x4 R_tilt = Multiply(MakeRotateXMatrix(stageTilt.x), MakeRotateZMatrix(-stageTilt.y));

    if (state_ == State::Active)
    {
        // 上下ホバリング
        hoverTimer_ += deltaTime;
        float hoverOffsetY = std::sin(hoverTimer_ * 2.8f) * 0.15f;

        // 自転
        currentAngle_ += 1.8f * deltaTime;
        if (currentAngle_ > kTwoPi) currentAngle_ -= kTwoPi;

        // 自転行列 (Y軸回転 + 少し斜めの傾きで魅力的な回転)
        Matrix4x4 R_spin = Multiply(MakeRotateXMatrix(0.20f), MakeRotateYMatrix(currentAngle_));
        Matrix4x4 R_combined = Multiply(R_spin, R_tilt);

        // ステージ傾斜に伴う配置位置の回転
        Vector3 P = { basePosition_.x, basePosition_.y + hoverOffsetY, basePosition_.z };
        Vector3 P_rel = { P.x - pivot.x, P.y, P.z - pivot.y };
        Vector3 RP_rel = {
            P_rel.x * R_tilt.m[0][0] + P_rel.y * R_tilt.m[1][0] + P_rel.z * R_tilt.m[2][0],
            P_rel.x * R_tilt.m[0][1] + P_rel.y * R_tilt.m[1][1] + P_rel.z * R_tilt.m[2][1],
            P_rel.x * R_tilt.m[0][2] + P_rel.y * R_tilt.m[1][2] + P_rel.z * R_tilt.m[2][2]
        };
        currentWorldPos_ = { RP_rel.x + pivot.x, RP_rel.y, RP_rel.z + pivot.y };

        currentRotation_ = MatrixToEulerXYZ(R_combined);
        currentScale_ = { 1.0f, 1.0f, 1.0f };

        // スライムとの接触判定
        CheckSlimeCollision(slimeManager);
    }
    else if (state_ == State::Collecting)
    {
        collectTimer_ += deltaTime;
        float progress = std::clamp(collectTimer_ / collectDuration_, 0.0f, 1.0f);

        // 高速回転
        currentAngle_ += 8.0f * deltaTime;
        Matrix4x4 R_spin = MakeRotateYMatrix(currentAngle_);
        Matrix4x4 R_combined = Multiply(R_spin, R_tilt);
        currentRotation_ = MatrixToEulerXYZ(R_combined);

        // 拡大＆吸い込みアニメーション
        // 前半 (0.0〜0.3): 一瞬ボヨン！と1.65倍に急拡大（Pop!）
        // 後半 (0.3〜1.0): 対象スライムの中心へ引き寄せられながらゼロへ収縮
        float s = 1.0f;
        Vector3 targetPos = collectStartPos_;
        if (collectedBySlime_ && collectedBySlime_->IsActive())
        {
            targetPos = collectedBySlime_->GetPosition();
        }

        if (progress < 0.30f)
        {
            float p = progress / 0.30f;
            // 1.0 -> 1.65 へのイージングアウト拡大
            s = 1.0f + 0.65f * std::sin(p * 1.5707963f);
            currentWorldPos_ = collectStartPos_;
            currentWorldPos_.y += 0.3f * std::sin(p * 3.14159f);
        }
        else
        {
            float p = (progress - 0.30f) / 0.70f;
            // 1.65 -> 0.0 への収縮
            s = 1.65f * (1.0f - p);
            // スライムへの補間吸い込み
            float t = p * p; // 加速吸い込み
            currentWorldPos_ = {
                collectStartPos_.x + (targetPos.x - collectStartPos_.x) * t,
                collectStartPos_.y + (targetPos.y - collectStartPos_.y) * t,
                collectStartPos_.z + (targetPos.z - collectStartPos_.z) * t
            };
        }

        currentScale_ = { s, s, s };

        if (progress >= 1.0f)
        {
            state_ = State::Inactive;
            respawnTimer_ = 0.0f;
        }
    }

    object3d_->SetTranslate(currentWorldPos_);
    object3d_->SetRotate(currentRotation_);
    object3d_->SetScale(currentScale_);
    object3d_->Update();
}

void GrowthCube::CheckSlimeCollision(SlimeManager* slimeManager)
{
    if (!slimeManager || state_ != State::Active) return;

    const auto& slimes = slimeManager->GetSlimes();
    float cubeRadius = baseSize_ * 0.65f;

    for (const auto& slime : slimes)
    {
        if (!slime || !slime->IsActive()) continue;

        Vector3 sPos = slime->GetPosition();
        float distSq = (sPos.x - currentWorldPos_.x) * (sPos.x - currentWorldPos_.x)
                     + (sPos.y - currentWorldPos_.y) * (sPos.y - currentWorldPos_.y)
                     + (sPos.z - currentWorldPos_.z) * (sPos.z - currentWorldPos_.z);

        float hitRadius = cubeRadius + slime->GetRadius();
        if (distSq <= hitRadius * hitRadius)
        {
            // 取得成功！
            collectedBySlime_ = slime.get();
            state_ = State::Collecting;
            collectTimer_ = 0.0f;
            collectStartPos_ = currentWorldPos_;

            // スライムを1サイズ巨大化！
            int currentSize = slime->GetSize();
            slime->SetSize(currentSize + 1);

            // 弾性変形エフェクト（大喜びのポヨン！弾み）
            slime->GetSlimeParams().squashStretch = { 0.35f, -0.38f, 0.35f };
            slime->GetSlimeParams().impulseStrength = 1.0f;
            break;
        }
    }
}

void GrowthCube::Draw(const RenderContext& ctx)
{
    if (state_ == State::Inactive || !object3d_ || !object3dCom_) return;

    RenderContext cubeCtx = ctx;
    if (textureIndex_ != TextureManager::kInvalidTextureIndex)
    {
        cubeCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(textureIndex_);
    }

    object3dCom_->Draw(object3d_.get(), cubeCtx, modelData_, true);
}

void GrowthCube::Respawn()
{
    state_ = State::Active;
    collectTimer_ = 0.0f;
    respawnTimer_ = 0.0f;
    currentScale_ = { 1.0f, 1.0f, 1.0f };
    collectedBySlime_ = nullptr;
}

void GrowthCube::SetBaseSize(float size)
{
    if (std::abs(baseSize_ - size) > 0.01f && object3d_)
    {
        baseSize_ = size;
        modelData_ = GeneratePrimitiveCube(baseSize_);
        modelData_.material.textureIndex = textureIndex_;
        object3d_->Initialize(object3dCom_, modelData_);
        object3d_->SetCamera(camera_);
        object3d_->SetColor({ 1.0f, 0.82f, 0.15f, 1.0f });
        object3d_->SetEnableLighting(true);
        object3d_->Update();
    }
}
