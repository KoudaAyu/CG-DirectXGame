/**
 * GamePlayScene.cpp
 * エンジン機能確認用デモシーン
 *
 * このシーンは GE3_Game ブランチにおける「エンジン基盤の動作確認」を目的とします。
 * Player / Enemy / Obstacle などのゲームロジックは含みません。
 *
 * 確認できる機能:
 *   - 3D オブジェクト描画 (Object3d / SceneManager)
 *   - スプライト描画 (Sprite / SpriteCom)
 *   - パーティクル (ParticleManager / ParticleEmitter)
 *   - ImGui パネル (カメラ操作 / デモオブジェクト制御)
 *   - スカイボックス (SceneManager 経由で自動描画)
 */
#include "GamePlayScene.h"

#include "SceneManager.h"
#include "TextureManager.h"
#include "Object3dCom.h"
#include "Light.h"
#include "MaterialManager.h"
#include "ParticleManager.h"
#include "Camera.h"
#include "SpriteCom.h"
#include "SpriteManager.h"
#include "KeyInput.h"
#include "DirectXCom.h"
#include "Baziru3_Engine/Graphics/Graphics/SceneRenderRequests.h"
#include "RenderContext.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

// -------------------------------------------------------
GamePlayScene::GamePlayScene() = default;

GamePlayScene::~GamePlayScene() = default;

// -------------------------------------------------------
void GamePlayScene::InitializeScene()
{
    // --- エンジン各サブシステムを SceneManager 経由で取得 ---
    auto* sm = SceneManager::GetInstance();
    camera_          = sm->GetCamera();
    object3dCom_     = sm->GetObject3dCom();
    light_           = sm->GetLight();
    materialManager_ = sm->GetMaterialManager();
    particleManager_ = sm->GetParticleManager();
    spriteCom_       = sm->GetSpriteCom();

    // --- カメラ初期位置（正面から見下ろす） ---
    if (camera_)
    {
        camera_->SetTranslate({ 0.0f, 5.0f, -10.0f });
        camera_->SetRotate(   { 0.3f, 0.0f,   0.0f });
    }

    // --- デモ用 3D オブジェクト（plane.obj） ---
    if (object3dCom_)
    {
        demoObject_ = std::make_unique<Object3d>();
        demoObject_->Initialize(object3dCom_, demoObject_->LoadObjFile("Resources", "plane.obj"));
        demoObject_->SetTranslate({ 0.0f, 0.0f, 0.0f });
        demoObject_->SetScale(    { 1.0f, 1.0f, 1.0f });
    }

    // --- テクスチャ ---
    uvCheckerIndex_ = TextureManager::GetInstance()->Load("Resources/uvChecker.png");

    // --- UV チェッカースプライト（左上に小さく表示） ---
    if (spriteCom_)
    {
        Sprite::Transform t{ {1.0f,1.0f,1.0f}, {0.0f,0.0f,0.0f}, {0.0f,0.0f,0.0f} };
        if (auto sp = Sprite::Create(spriteCom_, t, "Resources/uvChecker.png"))
        {
            sp->SetPosition({ 16.0f, 16.0f });
            sp->SetSize(    { 128.0f, 128.0f });
            sprites_.emplace_back(std::move(sp));
        }
    }

    // --- パーティクルエミッター初期設定 ---
    emitter_.transform.SetTranslate({ 0.0f, 0.5f, 0.0f });
    emitter_.count    = 10;
    emitter_.frequency = 0.5f;
    emitter_.frequencyTime = 0.0f;

    // --- マウス入力 ---
    if (dxCommon_)
    {
        mouseInput_.Initialize(dxCommon_->GetWindowAPI());
    }
}

// -------------------------------------------------------
void GamePlayScene::Finalize()
{
    for (auto& sp : sprites_)
    {
        if (sp) sp->Finalize();
    }
    sprites_.clear();

    demoObject_.reset();
}

// -------------------------------------------------------
void GamePlayScene::Update()
{
    mouseInput_.Update();

    // --- デモオブジェクト自転 ---
    rotY_ += 0.01f;
    if (demoObject_)
    {
        demoObject_->SetRotate({ 0.0f, rotY_, 0.0f });
        demoObject_->Update();
    }

    // --- スプライト更新 ---
    for (auto& sp : sprites_)
    {
        if (sp) sp->Update();
    }

    // --- パーティクル ---
    if (showParticles_ && particleManager_)
    {
        particleEmitter_.Emit(emitter_, particleManager_->GetRandomEngine(), *particleManager_);
    }

    // --- ImGui パネル ---
#ifdef USE_IMGUI
    UpdateImGuiPanel();
#endif
}

// -------------------------------------------------------
void GamePlayScene::Draw(SceneRenderRequests& renderRequests)
{
    // 3D オブジェクトの描画リクエストをシーンが行ったことを通知
    // (skybox は SceneManager::DrawSkybox が Game.cpp 側で呼ぶ)
    if (demoObject_ && object3dCom_)
    {
        // SceneManager.Draw 呼び出し元の Game.cpp 側で
        // renderRequests.sceneDrawn = true のとき object3dCom_->Draw が
        // 呼ばれないため、ここで直接描画リクエストを立てる。
        renderRequests.sceneDrawn = true;

        RenderContext ctx{};
        ctx.commandList = dxCommon_ ? dxCommon_->GetCommandList().Get() : nullptr;
        ctx.camera      = camera_;
        ctx.light       = light_;
        if (uvCheckerIndex_ != TextureManager::kInvalidTextureIndex)
        {
            ctx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(uvCheckerIndex_);
        }

        auto modelData = demoObject_->LoadObjFile("Resources", "plane.obj");
        object3dCom_->Draw(demoObject_.get(), ctx, modelData, true);
    }
}

// -------------------------------------------------------
void GamePlayScene::UpdateImGuiPanel()
{
#ifdef USE_IMGUI
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(300, 340), ImGuiCond_Once);
    ImGui::Begin("Engine Feature Demo");

    ImGui::TextColored(ImVec4(0.4f,1.0f,0.4f,1.0f), "GE3_Game - Engine Base Branch");
    ImGui::Separator();

    // --- カメラ ---
    if (camera_ && ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
    {
        Vector3 pos = camera_->GetTranslate();
        Vector3 rot = camera_->GetRotate();
        if (ImGui::DragFloat3("Position", &pos.x, 0.1f)) camera_->SetTranslate(pos);
        if (ImGui::DragFloat3("Rotation", &rot.x, 0.01f)) camera_->SetRotate(rot);
    }

    // --- デモオブジェクト ---
    if (ImGui::CollapsingHeader("3D Object", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::SliderFloat("Rotation Y", &rotY_, -3.14159f, 3.14159f);
        if (demoObject_)
        {
            Vector3 pos = demoObject_->GetTranslate();
            if (ImGui::DragFloat3("Obj Position", &pos.x, 0.1f))
                demoObject_->SetTranslate(pos);
        }
    }

    // --- パーティクル ---
    if (particleManager_ && ImGui::CollapsingHeader("Particle"))
    {
        ImGui::Checkbox("Emit Particles", &showParticles_);
        Vector3 epos = emitter_.transform.GetTranslate();
        if (ImGui::DragFloat3("Emitter Pos", &epos.x, 0.1f)) emitter_.transform.SetTranslate(epos);
        int countInt = static_cast<int>(emitter_.count);
        if (ImGui::DragInt("Count", &countInt, 1, 1, 100)) emitter_.count = static_cast<uint32_t>(countInt);
        ImGui::DragFloat("Frequency", &emitter_.frequency, 0.05f, 0.05f, 5.0f);
    }

    // --- シーン遷移 ---
    ImGui::Separator();
    if (ImGui::Button("Back to Title"))
    {
        SceneManager::GetInstance()->ChangeScene("TITLE");
    }

    ImGui::End();
#endif
}
