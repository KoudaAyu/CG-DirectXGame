#include "GamePlayScene.h"
#include "SceneManager.h"
#include "DirectXCom.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3dCom.h"
#include "Baziru3_Engine/Framework/Collision/CollisionManager.h"
#include "TextureManager.h"
#include "RenderContext.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

void GamePlayScene::InitializeScene()
{
    // 0. 衝突判定マネージャーの初期化
    CollisionManager::GetInstance()->Initialize();

    // 1. 入力システムの初期化
    if (dxCommon_ && dxCommon_->GetWindowAPI())
    {
        keyInput_ = std::make_unique<KeyInput>();
        keyInput_->Initialize(dxCommon_->GetWindowAPI());

        mouseInput_ = std::make_unique<MouseInput>();
        mouseInput_->Initialize(dxCommon_->GetWindowAPI());
    }

    // 2. カメラの初期化 (斜め見下ろし追従カメラ)
    playCamera_ = std::make_unique<Camera>();
    playCamera_->Initialize(dxCommon_);
    playCamera_->SetTranslate({ 0.0f, 12.0f, -16.0f });
    playCamera_->SetRotate({ 0.55f, 0.0f, 0.0f });
    playCamera_->Update();

    if (sceneManager_) {
        sceneManager_->SetCamera(playCamera_.get());
    }

    Object3dCom* object3dCom = GetObject3dCom();

    // 3. 地面プレーンの初期化
    groundModelData_ = Object3d::LoadObjFile("Resources", "plane.obj");
    groundTextureIndex_ = TextureManager::GetInstance()->Load("Resources/uvChecker.png");
    groundModelData_.material.textureIndex = groundTextureIndex_;

    groundPlane_ = std::make_unique<Object3d>();
    if (groundPlane_) {
        groundPlane_->Initialize(object3dCom, groundModelData_);
        groundPlane_->SetCamera(playCamera_.get());
        groundPlane_->SetTranslate({ 0.0f, 0.0f, 0.0f });
        groundPlane_->SetScale({ 60.0f, 1.0f, 60.0f });
        groundPlane_->SetRotate({ 0.0f, 0.0f, 0.0f });
        groundPlane_->SetColor({ 0.35f, 0.75f, 0.35f, 1.0f }); // 鮮やかな草原カラー
        groundPlane_->Update();
    }

    // 4. プレイヤーの初期化
    player_ = std::make_unique<PikminPlayer>();
    player_->Initialize(object3dCom, playCamera_.get(), { 0.0f, 0.5f, 0.0f });

    // 5. ミニオンマネージャーの初期化（初期12体をスポーン）
    minionManager_ = std::make_unique<MinionManager>();
    minionManager_->Initialize(object3dCom, playCamera_.get());
    minionManager_->SpawnMinion({ 0.0f, 0.0f, -2.0f }, 12, MinionType::Red);

    // 6. マウス照準・放物線ガイドの初期化
    aimGuide_ = std::make_unique<AimGuide>();
    aimGuide_->Initialize(object3dCom, playCamera_.get());

    isInitialized_ = true;
}

void GamePlayScene::Finalize()
{
    aimGuide_.reset();
    groundPlane_.reset();
    minionManager_.reset();
    player_.reset();
    playCamera_.reset();
    mouseInput_.reset();
    keyInput_.reset();
    isInitialized_ = false;
}

void GamePlayScene::Update()
{
    float deltaTime = 1.0f / 60.0f;

    if (keyInput_)
    {
        keyInput_->Update();

        // SPACEキーでクリアシーンへ遷移
        if (keyInput_->TriggerKey(DIK_SPACE))
        {
            SceneManager::GetInstance()->ChangeScene("CLEAR");
        }
    }

    if (mouseInput_)
    {
        mouseInput_->Update();
    }

    // 放物線照準ガイドの更新
    if (aimGuide_ && player_ && playCamera_)
    {
        Vector3 launchOrigin = player_->GetPosition();
        launchOrigin.y += 0.5f;
        aimGuide_->Update(launchOrigin, mouseInput_.get(), playCamera_.get(), player_->IsMerged());
    }

    // プレイヤーの更新
    if (player_)
    {
        player_->Update(deltaTime, keyInput_.get(), minionManager_.get(), mouseInput_.get(), aimGuide_.get());
    }

    // ミニオンマネージャーの更新
    if (minionManager_ && player_)
    {
        minionManager_->Update(deltaTime, player_->GetPosition(), player_->GetYaw(), player_->IsMerged(), player_->GetCurrentScale());
    }

    // 衝突判定と押し出しの更新
    CollisionManager::GetInstance()->Update();

    // 地面プレーン更新
    if (groundPlane_)
    {
        groundPlane_->Update();
    }

    // カメラのスムーズ追従 (Smooth Damping)
    if (playCamera_ && player_)
    {
        Vector3 playerPos = player_->GetPosition();
        Vector3 targetCamPos = {
            playerPos.x + cameraOffset_.x,
            playerPos.y + cameraOffset_.y,
            playerPos.z + cameraOffset_.z
        };
        Vector3 currentCamPos = playCamera_->GetTranslate();
        float smoothRate = (std::min)(1.0f, deltaTime * 8.0f);
        Vector3 newCamPos;
        newCamPos.x = currentCamPos.x + (targetCamPos.x - currentCamPos.x) * smoothRate;
        newCamPos.y = currentCamPos.y + (targetCamPos.y - currentCamPos.y) * smoothRate;
        newCamPos.z = currentCamPos.z + (targetCamPos.z - currentCamPos.z) * smoothRate;

        playCamera_->SetTranslate(newCamPos);
        playCamera_->SetRotate({ 0.55f, 0.0f, 0.0f });
        playCamera_->Update();
    }

    DrawDebugUI();
}

void GamePlayScene::Draw(SceneRenderRequests& renderRequests)
{
    renderRequests.sceneDrawn = true;

    // 背景スカイボックスの描画
    if (dxCommon_ && dxCommon_->GetCommandList())
    {
        SceneManager::GetInstance()->DrawSkybox(dxCommon_->GetCommandList().Get());
    }

    Object3dCom* object3dCom = GetObject3dCom();
    if (!object3dCom || !dxCommon_ || !dxCommon_->GetCommandList()) return;

    RenderContext ctx;
    ctx.commandList = dxCommon_->GetCommandList().Get();
    ctx.camera = playCamera_.get();

    // 1. 地面の描画
    if (groundPlane_)
    {
        RenderContext groundCtx = ctx;
        if (groundTextureIndex_ != TextureManager::kInvalidTextureIndex) {
            groundCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(groundTextureIndex_);
        }
        object3dCom->Draw(groundPlane_.get(), groundCtx, groundModelData_, true);
    }

    // 2. 放物線照準ガイドの描画 (地面の上に重なるように先に描画)
    if (aimGuide_)
    {
        aimGuide_->Draw(ctx);
    }

    // 3. ミニオン群衆の描画
    if (minionManager_)
    {
        minionManager_->Draw(ctx);
    }

    // 4. プレイヤーの描画
    if (player_)
    {
        player_->Draw(ctx);
    }
}

void GamePlayScene::DrawDebugUI()
{
#ifdef USE_IMGUI
    // コライダーのデバッグワイヤーフレーム描画
    if (playCamera_)
    {
        CollisionManager::GetInstance()->DrawDebug(playCamera_.get());
    }

    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(460, 480), ImGuiCond_FirstUseEver);
    ImGui::Begin("Pikmin x LocoRoco Debug Panel", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "=== [ Pikmin x LocoRoco 3D Prototype ] ===");
    ImGui::Separator();

    // 1. 操作説明
    ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.2f, 1.0f), "[ Controls ]");
    ImGui::BulletText("WASD / Arrows: Move");
    ImGui::BulletText("E key: Merge (LocoRoco) <-> Split (Pikmin)");
    ImGui::BulletText("Mouse Left Click: Aim & Throw to Target (放物線投擲)");
    ImGui::BulletText("Mouse Right Click: Whistle at Cursor (マウス位置へ笛)");
    ImGui::BulletText("F / J key: Forward Throw");
    ImGui::BulletText("Q key: Whistle around player");
    ImGui::BulletText("SPACE key: Clear Scene");
    ImGui::Separator();

    // 2. ステート表示 & 合体トグル
    if (player_ && minionManager_)
    {
        bool isMerged = player_->IsMerged();
        int mergedCount = minionManager_->GetMergedCount();
        int totalCount = minionManager_->GetTotalCount();
        if (isMerged) {
            ImGui::Text("Player Mode: LocoRoco (Giant / Absorbed: %d / %d, Scale: %.2f)",
                        mergedCount, totalCount, player_->GetCurrentScale());
        } else {
            ImGui::Text("Player Mode: Pikmin (Split / Squad, Scale: %.2f)", player_->GetCurrentScale());
        }

        if (ImGui::Button(isMerged ? "SPLIT (Pikmin Mode)" : "MERGE (LocoRoco Mode)", ImVec2(280, 36)))
        {
            player_->ToggleMerge();
        }
    }

    ImGui::Separator();

    // 3. ミニオン群衆管理
    if (minionManager_ && player_)
    {
        int activeCount = minionManager_->GetActiveCount();
        int readyCount = minionManager_->GetReadyCount(player_->GetPosition());
        int totalCount = minionManager_->GetTotalCount();
        ImGui::Text("Minions: %d Ready to Throw | %d Active | %d Total", readyCount, activeCount, totalCount);

        if (ImGui::Button("+1 Spawn")) {
            minionManager_->SpawnMinion(player_->GetPosition(), 1);
        }
        ImGui::SameLine();
        if (ImGui::Button("+5 Spawn")) {
            minionManager_->SpawnMinion(player_->GetPosition(), 5);
        }
        ImGui::SameLine();
        if (ImGui::Button("+10 Spawn")) {
            minionManager_->SpawnMinion(player_->GetPosition(), 10);
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear All")) {
            minionManager_->ClearMinions();
        }

        // スライダー調整
        float followSpeed = minionManager_->GetFollowSpeed();
        if (ImGui::SliderFloat("Follow Speed", &followSpeed, 2.0f, 30.0f)) {
            minionManager_->SetFollowSpeed(followSpeed);
        }

        float slotRadius = minionManager_->GetSlotRadius();
        if (ImGui::SliderFloat("Formation Radius", &slotRadius, 0.5f, 4.0f)) {
            minionManager_->SetSlotRadius(slotRadius);
        }
    }

    ImGui::Separator();

    // 4. プレイヤー調整
    if (player_)
    {
        float normalSpeed = player_->GetNormalSpeed();
        if (ImGui::SliderFloat("Normal Move Speed", &normalSpeed, 2.0f, 25.0f)) {
            player_->SetNormalSpeed(normalSpeed);
        }

        float mergedSpeed = player_->GetMergedSpeed();
        if (ImGui::SliderFloat("Merged Roll Speed", &mergedSpeed, 5.0f, 35.0f)) {
            player_->SetMergedSpeed(mergedSpeed);
        }

        float throwPower = player_->GetThrowPower();
        if (ImGui::SliderFloat("Throw Power", &throwPower, 5.0f, 35.0f)) {
            player_->SetThrowPower(throwPower);
        }

        // --- スライム表現パラメータ調整 ---
        auto& slimeParams = player_->GetSlimeParams();
        if (ImGui::CollapsingHeader("Slime Jelly & Wobble Params", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::SliderFloat("Wobble Strength", &slimeParams.wobbleStrength, 0.0f, 0.4f, "%.3f");
            ImGui::SliderFloat("Wobble Frequency", &slimeParams.wobbleFrequency, 1.0f, 12.0f, "%.1f");
            ImGui::SliderFloat("Fresnel Power", &slimeParams.fresnelPower, 0.5f, 6.0f, "%.1f");
            ImGui::SliderFloat("Env Reflection", &slimeParams.envReflection, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Inner Glow", &slimeParams.innerGlow, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Shininess", &slimeParams.specularShininess, 8.0f, 128.0f, "%.0f");

            float color[4] = { slimeParams.baseColor.x, slimeParams.baseColor.y, slimeParams.baseColor.z, slimeParams.baseColor.w };
            if (ImGui::ColorEdit4("Slime Color", color))
            {
                slimeParams.baseColor = { color[0], color[1], color[2], color[3] };
            }

            if (ImGui::Button("Trigger Impulse Ripple"))
            {
                slimeParams.impulseStrength = 0.4f;
            }
        }
    }

    ImGui::Separator();

    // 5. シーン遷移
    if (ImGui::Button("Go To CLEAR", ImVec2(130, 28)))
    {
        SceneManager::GetInstance()->ChangeScene("CLEAR");
    }
    ImGui::SameLine();
    if (ImGui::Button("Go To GAMEOVER", ImVec2(130, 28)))
    {
        SceneManager::GetInstance()->ChangeScene("GAMEOVER");
    }
    ImGui::SameLine();
    if (ImGui::Button("TITLE", ImVec2(90, 28)))
    {
        SceneManager::GetInstance()->ChangeScene("TITLE");
    }

    ImGui::End();
#endif
}
