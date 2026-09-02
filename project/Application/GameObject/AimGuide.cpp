#include "AimGuide.h"
#include "Application/GameObject/SlimePhysics.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3dCom.h"
#include "Baziru3_Engine/Core/Camera/Camera.h"
#include "Baziru3_Engine/Core/IO/Mouse/MouseInput.h"
#include "Baziru3_Engine/Core/Base/Windows/WindowsAPI.h"
#include "TextureManager.h"
#include <cmath>
#include <algorithm>

namespace {
    constexpr float kPi = 3.14159265358979323846f;

    Vector3 TransformCoord(const Vector3& v, const Matrix4x4& m) {
        float w = v.x * m.m[0][3] + v.y * m.m[1][3] + v.z * m.m[2][3] + m.m[3][3];
        if (std::abs(w) < 1e-6f) w = 1.0f;
        return {
            (v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + m.m[3][0]) / w,
            (v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + m.m[3][1]) / w,
            (v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + m.m[3][2]) / w
        };
    }
}

AimGuide::AimGuide() {
    reticleObject_ = std::make_unique<Object3d>();
    for (size_t i = 0; i < kDotCount; ++i) {
        dotObjects_.push_back(std::make_unique<Object3d>());
    }
}

AimGuide::~AimGuide() = default;

Object3d::ModelData AimGuide::GenerateRingMesh(float innerRadius, float outerRadius, uint32_t segments) {
    Object3d::ModelData modelData;

    for (uint32_t i = 0; i <= segments; ++i) {
        float theta = (2.0f * kPi * static_cast<float>(i)) / static_cast<float>(segments);
        float sinT = std::sin(theta);
        float cosT = std::cos(theta);

        // 内側の頂点
        Sprite::VertexData vInner{};
        vInner.position = { cosT * innerRadius, 0.0f, sinT * innerRadius, 1.0f };
        vInner.normal   = { 0.0f, 1.0f, 0.0f };
        vInner.texcoord = { static_cast<float>(i) / segments, 0.0f };
        modelData.vertices.push_back(vInner);

        // 外側の頂点
        Sprite::VertexData vOuter{};
        vOuter.position = { cosT * outerRadius, 0.0f, sinT * outerRadius, 1.0f };
        vOuter.normal   = { 0.0f, 1.0f, 0.0f };
        vOuter.texcoord = { static_cast<float>(i) / segments, 1.0f };
        modelData.vertices.push_back(vOuter);
    }

    // インデックス生成
    for (uint32_t i = 0; i < segments; ++i) {
        uint32_t i0 = i * 2;
        uint32_t i1 = i * 2 + 1;
        uint32_t i2 = (i + 1) * 2;
        uint32_t i3 = (i + 1) * 2 + 1;

        // 三角形1
        modelData.indices.push_back(i0);
        modelData.indices.push_back(i1);
        modelData.indices.push_back(i2);

        // 三角形2
        modelData.indices.push_back(i2);
        modelData.indices.push_back(i1);
        modelData.indices.push_back(i3);
    }

    return modelData;
}

Object3d::ModelData AimGuide::GenerateSphereMesh(uint32_t sliceCount, uint32_t stackCount, float radius) {
    Object3d::ModelData modelData;

    // 北極
    {
        Sprite::VertexData v{};
        v.position = { 0.0f, radius, 0.0f, 1.0f };
        v.normal   = { 0.0f, 1.0f, 0.0f };
        v.texcoord = { 0.5f, 0.0f };
        modelData.vertices.push_back(v);
    }

    for (uint32_t i = 1; i < stackCount; ++i) {
        float phi = kPi * static_cast<float>(i) / static_cast<float>(stackCount);
        float sinPhi = std::sin(phi);
        float cosPhi = std::cos(phi);

        for (uint32_t j = 0; j <= sliceCount; ++j) {
            float theta = 2.0f * kPi * static_cast<float>(j) / static_cast<float>(sliceCount);
            float sinTheta = std::sin(theta);
            float cosTheta = std::cos(theta);

            Sprite::VertexData v{};
            v.position = { sinPhi * cosTheta * radius, cosPhi * radius, sinPhi * sinTheta * radius, 1.0f };
            v.normal   = { sinPhi * cosTheta, cosPhi, sinPhi * sinTheta };
            v.texcoord = { static_cast<float>(j) / sliceCount, static_cast<float>(i) / stackCount };
            modelData.vertices.push_back(v);
        }
    }

    // 南極
    {
        Sprite::VertexData v{};
        v.position = { 0.0f, -radius, 0.0f, 1.0f };
        v.normal   = { 0.0f, -1.0f, 0.0f };
        v.texcoord = { 0.5f, 1.0f };
        modelData.vertices.push_back(v);
    }

    uint32_t ringVertexCount = sliceCount + 1;
    // 北極キャップ
    for (uint32_t j = 0; j < sliceCount; ++j) {
        modelData.indices.push_back(0);
        modelData.indices.push_back(1 + j);
        modelData.indices.push_back(1 + j + 1);
    }

    // 中間
    for (uint32_t i = 0; i < stackCount - 2; ++i) {
        uint32_t rowA = 1 + i * ringVertexCount;
        uint32_t rowB = 1 + (i + 1) * ringVertexCount;
        for (uint32_t j = 0; j < sliceCount; ++j) {
            modelData.indices.push_back(rowA + j);
            modelData.indices.push_back(rowB + j);
            modelData.indices.push_back(rowA + j + 1);

            modelData.indices.push_back(rowA + j + 1);
            modelData.indices.push_back(rowB + j);
            modelData.indices.push_back(rowB + j + 1);
        }
    }

    // 南極キャップ
    uint32_t southPoleIndex = static_cast<uint32_t>(modelData.vertices.size() - 1);
    uint32_t lastRowStart = 1 + (stackCount - 2) * ringVertexCount;
    for (uint32_t j = 0; j < sliceCount; ++j) {
        modelData.indices.push_back(southPoleIndex);
        modelData.indices.push_back(lastRowStart + j + 1);
        modelData.indices.push_back(lastRowStart + j);
    }

    return modelData;
}

void AimGuide::Initialize(Object3dCom* object3dCom, Camera* camera) {
    object3dCom_ = object3dCom;
    camera_ = camera;

    reticleTextureIndex_ = TextureManager::GetInstance()->Load("Resources/CG4/human/white.png");

    // 照準リングメッシュ初期化 (内径0.55, 外径0.75, 36分割)
    reticleModelData_ = GenerateRingMesh(0.55f, 0.75f, 36);
    reticleModelData_.material.textureIndex = reticleTextureIndex_;

    if (reticleObject_) {
        reticleObject_->Initialize(object3dCom_, reticleModelData_);
        reticleObject_->SetCamera(camera_);
        reticleObject_->SetTranslate({ 0.0f, 0.05f, 0.0f });
        reticleObject_->SetScale({ 1.0f, 1.0f, 1.0f });
        reticleObject_->SetColor({ 0.2f, 0.9f, 1.0f, 0.85f });
        reticleObject_->SetEnableLighting(false);
        reticleObject_->SetAllowWireframeOverlay(false); // ガイドUIのためワイヤーフレームデバッグ表示の対象外
        reticleObject_->Update();
    }

    // ドット球体メッシュ初期化
    dotModelData_ = GenerateSphereMesh(16, 8, 1.0f);
    dotTextureIndex_ = reticleTextureIndex_;
    dotModelData_.material.textureIndex = dotTextureIndex_;

    for (auto& dot : dotObjects_) {
        dot->Initialize(object3dCom_, dotModelData_);
        dot->SetCamera(camera_);
        dot->SetTranslate({ 0.0f, -100.0f, 0.0f }); // 初期は見えない位置
        dot->SetScale({ 0.12f, 0.12f, 0.12f });
        dot->SetColor({ 0.3f, 0.95f, 1.0f, 0.75f });
        dot->SetEnableLighting(false);
        dot->SetAllowWireframeOverlay(false); // ガイドUIのためワイヤーフレームデバッグ表示の対象外
        dot->Update();
    }
}

void AimGuide::Update(const Vector3& launchPos, MouseInput* mouseInput, Camera* camera, bool isMerged, const Vector2& stageTilt) {
    isMerged_ = isMerged;
    camera_ = camera;
    if (reticleObject_) reticleObject_->SetCamera(camera_);
    for (auto& dot : dotObjects_) {
        if (dot) dot->SetCamera(camera_);
    }

    if (isMerged_ || !mouseInput || !camera_) {
        isValidTarget_ = false;
        return;
    }

    pulseTimer_ += 0.0166f;

    // 1. マウスのスクリーン座標 ➔ 正規化デバイス座標 (NDC)
    Vector2 mousePos = mouseInput->GetScaledPosition();
    float clientW = static_cast<float>(WindowAPI::GetClientWidth());
    float clientH = static_cast<float>(WindowAPI::GetClientHeight());

    float ndcX = (2.0f * mousePos.x) / clientW - 1.0f;
    float ndcY = 1.0f - (2.0f * mousePos.y) / clientH;

    // 2. View-Projection の逆行列を取得
    Matrix4x4 vp = camera_->GetViewProjectionMatrix();
    Matrix4x4 invVP = Inverse(vp);

    // 3. レイの算出
    Vector3 nearPos = TransformCoord({ ndcX, ndcY, 0.0f }, invVP);
    Vector3 farPos  = TransformCoord({ ndcX, ndcY, 1.0f }, invVP);
    Vector3 rayDir  = farPos - nearPos;
    float rayLength = std::sqrt(rayDir.x * rayDir.x + rayDir.y * rayDir.y + rayDir.z * rayDir.z);
    if (rayLength > 0.0001f) {
        rayDir.x /= rayLength;
        rayDir.y /= rayLength;
        rayDir.z /= rayLength;
    }

    // 4. 傾斜面との厳密な交差判定
    // 地面プレーン法線 N = (cos(tilt.x)*sin(tilt.y), cos(tilt.x)*cos(tilt.y), sin(tilt.x))
    float cosPitch = std::cos(stageTilt.x);
    float cosRoll = std::cos(stageTilt.y);
    float nx = cosPitch * std::sin(stageTilt.y);
    float ny = cosPitch * cosRoll;
    float nz = std::sin(stageTilt.x);
    float denom = nx * rayDir.x + ny * rayDir.y + nz * rayDir.z;

    if (std::abs(denom) < 0.0001f) {
        isValidTarget_ = false;
        return;
    }

    float t = -(nx * nearPos.x + ny * nearPos.y + nz * nearPos.z) / denom;
    if (t < 0.0f) {
        isValidTarget_ = false;
        return;
    }

    targetPos_ = nearPos + rayDir * t;
    isValidTarget_ = true;

    // 5. 射程距離クランプ（最大射程 15m）
    Vector3 toTarget = targetPos_ - launchPos;
    float horizontalDist = std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z);
    isInRange_ = (horizontalDist <= maxRange_);
    Vector3 clampedTarget = targetPos_;
    if (horizontalDist > maxRange_ && horizontalDist > 0.0001f) {
        float ratio = maxRange_ / horizontalDist;
        clampedTarget.x = launchPos.x + toTarget.x * ratio;
        clampedTarget.z = launchPos.z + toTarget.z * ratio;
        // 傾斜面上の厳密な高さを再計算
        clampedTarget.y = SlimePhysics::CalculateGroundHeight(clampedTarget.x, clampedTarget.z, stageTilt);
        horizontalDist = maxRange_;
    }

    // 6. 飛行時間の計算
    float flightTime = 0.35f + 0.024f * horizontalDist;

    // 7. 初速度ベクトルの計算
    calculatedVelocity_.x = (clampedTarget.x - launchPos.x) / flightTime;
    calculatedVelocity_.z = (clampedTarget.z - launchPos.z) / flightTime;
    calculatedVelocity_.y = ((clampedTarget.y - launchPos.y) - 0.5f * gravity_ * flightTime * flightTime) / flightTime;

    // 8. 照準リングの更新（傾斜面に完全平行に配置・法線オフセットでめり込み防止）
    if (reticleObject_) {
        float normalOffset = 0.08f; // 地面から法線方向にわずかに浮かせる

        Vector3 reticlePos = {
            targetPos_.x + nx * normalOffset,
            targetPos_.y + ny * normalOffset,
            targetPos_.z + nz * normalOffset
        };

        reticleObject_->SetTranslate(reticlePos);
        // 地面プレーンの回転と完全に一致させる（Y回転による傾きズレを防ぐ）
        reticleObject_->SetRotate({ stageTilt.x, 0.0f, -stageTilt.y });

        float scaleAnim = 1.0f + 0.06f * std::sin(pulseTimer_ * 8.0f);
        reticleObject_->SetScale({ scaleAnim, 1.0f, scaleAnim });

        if (isInRange_) {
            reticleObject_->SetColor({ 0.2f, 0.9f, 1.0f, 0.9f }); // 有効射程：シアン
        } else {
            reticleObject_->SetColor({ 1.0f, 0.3f, 0.3f, 0.6f }); // 射程外：赤
        }
        reticleObject_->Update();
    }

    // 9. 軌道ドット群の更新
    for (size_t i = 0; i < kDotCount; ++i) {
        float progress = static_cast<float>(i + 1) / static_cast<float>(kDotCount + 1);
        float t = flightTime * progress;

        Vector3 dotPos;
        dotPos.x = launchPos.x + calculatedVelocity_.x * t;
        dotPos.y = launchPos.y + calculatedVelocity_.y * t + 0.5f * gravity_ * t * t;
        dotPos.z = launchPos.z + calculatedVelocity_.z * t;

        if (dotObjects_[i]) {
            dotObjects_[i]->SetTranslate(dotPos);

            float dotScale = 0.13f * (1.0f - progress * 0.35f);
            dotObjects_[i]->SetScale({ dotScale, dotScale, dotScale });

            float alpha = 0.85f * (1.0f - progress * 0.25f);
            if (isInRange_) {
                dotObjects_[i]->SetColor({ 0.3f, 0.95f, 1.0f, alpha });
            } else {
                dotObjects_[i]->SetColor({ 1.0f, 0.4f, 0.4f, alpha * 0.6f });
            }
            dotObjects_[i]->Update();
        }
    }
}

void AimGuide::Draw(const RenderContext& ctx) {
    if (isMerged_ || !isValidTarget_) return;

    if (reticleObject_) {
        reticleObject_->Draw(ctx);
    }

    for (auto& dot : dotObjects_) {
        if (dot) {
            dot->Draw(ctx);
        }
    }
}
