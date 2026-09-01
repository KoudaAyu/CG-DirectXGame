#include "TrajectoryRenderer.h"
#include "DirectXCom.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3dCom.h"
#include "TextureManager.h"
#include "Camera.h"
#include "KeyInput.h"
#include <cmath>
#include <algorithm>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace {
    constexpr float kPi = 3.14159265358979323846f;
}

TrajectoryRenderer* TrajectoryRenderer::GetInstance() {
    static TrajectoryRenderer instance;
    return &instance;
}

void TrajectoryRenderer::Initialize(DirectXCom* dxCommon, Object3dCom* object3dCom) {
    dxCommon_ = dxCommon;
    object3dCom_ = object3dCom;

    dotModelData_ = Object3d::LoadObjFile("Resources", "plane.obj");
    dotTextureIndex_ = TextureManager::GetInstance()->Load("Resources/uvChecker.png");
    dotModelData_.material.textureIndex = dotTextureIndex_;

    for (int i = 0; i < kMaxSteps; ++i) {
        dotObjects_[i] = std::make_unique<Object3d>();
        if (dotObjects_[i]) {
            dotObjects_[i]->Initialize(object3dCom_, dotModelData_);
            dotObjects_[i]->SetScale({ 0.12f, 0.12f, 0.12f });
            dotObjects_[i]->SetColor(dotColor_);
            dotObjects_[i]->SetAllowWireframeOverlay(false);
            dotObjects_[i]->Update();
        }
    }

    landingMarker_ = std::make_unique<Object3d>();
    if (landingMarker_) {
        landingMarker_->Initialize(object3dCom_, dotModelData_);
        landingMarker_->SetScale({ 0.55f, 0.04f, 0.55f });
        landingMarker_->SetColor(landingColor_);
        landingMarker_->SetAllowWireframeOverlay(false);
        landingMarker_->Update();
    }
}

void TrajectoryRenderer::Finalize() {
    for (int i = 0; i < kMaxSteps; ++i) {
        dotObjects_[i].reset();
    }
    landingMarker_.reset();
    dxCommon_ = nullptr;
    object3dCom_ = nullptr;
}

void TrajectoryRenderer::Update(float deltaTime, KeyInput* keyInput, Camera* activeCamera) {
    if (activeCamera) {
        Vector3 camPos = activeCamera->GetTranslate();
        currentLaunchPos_ = { camPos.x, 0.5f, camPos.z + 16.0f };
    }

    if (autoFollowInput_ && keyInput) {
        Vector3 moveDir = { 0.0f, 0.0f, 0.0f };
        if (keyInput->PushKey(DIK_W) || keyInput->PushKey(DIK_UP)) moveDir.z += 1.0f;
        if (keyInput->PushKey(DIK_S) || keyInput->PushKey(DIK_DOWN)) moveDir.z -= 1.0f;
        if (keyInput->PushKey(DIK_A) || keyInput->PushKey(DIK_LEFT)) moveDir.x -= 1.0f;
        if (keyInput->PushKey(DIK_D) || keyInput->PushKey(DIK_RIGHT)) moveDir.x += 1.0f;

        float len = std::sqrt(moveDir.x * moveDir.x + moveDir.z * moveDir.z);
        if (len > 0.001f) {
            float targetYaw = std::atan2(moveDir.x, moveDir.z);
            float diff = targetYaw - aimYaw_;
            while (diff > kPi) diff -= 2.0f * kPi;
            while (diff < -kPi) diff += 2.0f * kPi;
            aimYaw_ += diff * (std::min)(1.0f, 12.0f * deltaTime);
        }
    }
}

Vector3 TrajectoryRenderer::CalculateLaunchVelocity(const Vector3& forwardDir) const {
    float len = std::sqrt(forwardDir.x * forwardDir.x + forwardDir.z * forwardDir.z);
    Vector3 normDir = forwardDir;
    if (len > 0.0001f) {
        normDir.x /= len;
        normDir.z /= len;
    }
    return { normDir.x * throwPower_, upPower_, normDir.z * throwPower_ };
}

Vector3 TrajectoryRenderer::CalculateLandingPoint(const Vector3& startPos, const Vector3& forwardDir) const {
    Vector3 vel = CalculateLaunchVelocity(forwardDir);
    
    float a = 0.5f * gravity_;
    float b = vel.y;
    float c = startPos.y - groundLevel_;

    float d = b * b - 4.0f * a * c;
    if (d >= 0.0f && std::abs(a) > 0.0001f) {
        float t1 = (-b - std::sqrt(d)) / (2.0f * a);
        float t2 = (-b + std::sqrt(d)) / (2.0f * a);
        float landingT = (std::max)(t1, t2);
        if (landingT > 0.0f) {
            return {
                startPos.x + vel.x * landingT,
                groundLevel_,
                startPos.z + vel.z * landingT
            };
        }
    }
    return { startPos.x + vel.x * 0.5f, groundLevel_, startPos.z + vel.z * 0.5f };
}

void TrajectoryRenderer::Draw(const RenderContext& ctx, const Vector3& startPos, const Vector3& forwardDir) {
    if (!isVisible_ || !object3dCom_) return;

    Vector3 vel = CalculateLaunchVelocity(forwardDir);

    RenderContext localCtx = ctx;
    localCtx.camera = ctx.camera;
    if (dotTextureIndex_ != TextureManager::kInvalidTextureIndex) {
        localCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(dotTextureIndex_);
    }

    int activeSteps = (std::min)(stepCount_, kMaxSteps);
    for (int i = 0; i < activeSteps; ++i) {
        float t = (i + 1) * timeStep_;
        Vector3 pos = {
            startPos.x + vel.x * t,
            startPos.y + vel.y * t + 0.5f * gravity_ * t * t,
            startPos.z + vel.z * t
        };

        if (pos.y < groundLevel_) {
            pos.y = groundLevel_;
        }

        if (dotObjects_[i]) {
            dotObjects_[i]->SetCamera(ctx.camera);
            dotObjects_[i]->SetTranslate(pos);
            float scaleFactor = 0.14f * (1.0f - (float)i / (float)activeSteps * 0.4f);
            dotObjects_[i]->SetScale({ scaleFactor, scaleFactor, scaleFactor });
            dotObjects_[i]->SetColor(dotColor_);
            dotObjects_[i]->Update();
            object3dCom_->Draw(dotObjects_[i].get(), localCtx, dotModelData_, true);
        }
    }

    // 着地サークルの描画
    if (showLandingMarker_ && landingMarker_) {
        Vector3 landingPos = CalculateLandingPoint(startPos, forwardDir);
        landingMarker_->SetCamera(ctx.camera);
        landingMarker_->SetTranslate(landingPos);
        landingMarker_->SetScale({ 0.55f, 0.04f, 0.55f });
        landingMarker_->SetColor(landingColor_);
        landingMarker_->Update();
        object3dCom_->Draw(landingMarker_.get(), localCtx, dotModelData_, true);
    }
}

void TrajectoryRenderer::DrawAuto(const RenderContext& ctx) {
    if (!isVisible_) return;

    Vector3 forward = { std::sin(aimYaw_), 0.0f, std::cos(aimYaw_) };
    Vector3 launchPos = {
        currentLaunchPos_.x + forward.x * 0.8f,
        currentLaunchPos_.y + 0.5f,
        currentLaunchPos_.z + forward.z * 0.8f
    };

    Draw(ctx, launchPos, forward);
}

void TrajectoryRenderer::DrawImGui() {
#ifdef USE_IMGUI
    if (ImGui::CollapsingHeader("Trajectory Tuning Tool (投擲軌道ツール)", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Show Trajectory", &isVisible_);
        ImGui::SameLine();
        ImGui::Checkbox("Show Landing Circle", &showLandingMarker_);

        ImGui::Checkbox("Auto Follow Player Input", &autoFollowInput_);

        float yawDeg = aimYaw_ * 180.0f / kPi;
        if (ImGui::SliderFloat("Aim Angle (Yaw)", &yawDeg, -180.0f, 180.0f, "%.1f deg")) {
            aimYaw_ = yawDeg * kPi / 180.0f;
        }

        ImGui::SliderFloat("Throw Power (初速)", &throwPower_, 3.0f, 35.0f, "%.1f m/s");
        ImGui::SliderFloat("Upward Power (上向き角)", &upPower_, 1.0f, 20.0f, "%.1f m/s");
        ImGui::SliderFloat("Gravity (重力)", &gravity_, -40.0f, -5.0f, "%.1f m/s²");
        ImGui::SliderInt("Step Count (ドット数)", &stepCount_, 3, kMaxSteps);
        ImGui::SliderFloat("Time Step (間隔)", &timeStep_, 0.02f, 0.12f, "%.3f s");

        ImGui::ColorEdit4("Dot Color", &dotColor_.x);
        ImGui::ColorEdit4("Landing Color", &landingColor_.x);
    }
#endif
}
