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

namespace
{
    // 臨界減衰スプリング補間（SmoothDamp: C2級連続の極上滑らか補間）
    float SmoothDamp(float current, float target, float& currentVelocity, float smoothTime, float deltaTime, float maxSpeed = 10000.0f)
    {
        smoothTime = (std::max)(0.0001f, smoothTime);
        float omega = 2.0f / smoothTime;

        float x = omega * deltaTime;
        float exp = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
        float change = current - target;
        float originalTo = target;

        float maxChange = maxSpeed * smoothTime;
        change = std::clamp(change, -maxChange, maxChange);
        target = current - change;

        float temp = (currentVelocity + omega * change) * deltaTime;
        currentVelocity = (currentVelocity - omega * temp) * exp;
        float output = target + (change + temp) * exp;

        if ((originalTo - current > 0.0f) == (output > originalTo))
        {
            output = originalTo;
            currentVelocity = (output - originalTo) / (std::max)(0.0001f, deltaTime);
        }

        return output;
    }
}

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

    // 2. カメラの初期化 (プレイヤー相対座標一定モデル)
    playCamera_ = std::make_unique<Camera>();
    playCamera_->Initialize(dxCommon_);
    playCamera_->SetFovY(cameraFov_);

    float cosPitch = std::cos(cameraPitch_);
    float sinPitch = std::sin(cameraPitch_);
    float cosYaw = std::cos(cameraYaw_);
    float sinYaw = std::sin(cameraYaw_);
    Vector3 initOffset = {
        -cameraDistance_ * sinYaw * cosPitch,
        cameraDistance_ * sinPitch,
        -cameraDistance_ * cosYaw * cosPitch
    };
    Vector3 initLookAt = {
        0.0f,
        0.5f + cameraTargetOffsetY_,
        cameraForwardOffset_
    };
    Vector3 initCamPos = {
        initLookAt.x + initOffset.x,
        initLookAt.y + initOffset.y,
        initLookAt.z + initOffset.z
    };
    currentCameraPos_ = initCamPos;
    currentCameraRot_ = { cameraPitch_, cameraYaw_, 0.0f };
    playCamera_->SetTranslate(initCamPos);
    playCamera_->SetRotate(currentCameraRot_);
    playCamera_->Update();
    cameraInitialized_ = true;
    cameraPosVelocity_ = { 0.0f, 0.0f, 0.0f };
    cameraRotVelocity_ = { 0.0f, 0.0f, 0.0f };
    tiltVelocity_ = { 0.0f, 0.0f };

    if (sceneManager_) {
        sceneManager_->SetCamera(playCamera_.get());
    }

    Object3dCom* object3dCom = GetObject3dCom();

    // 3. 地面グリッドメッシュの動的生成（広大なチェッカーボードグリッド）
    {
        const int gridN = 41; // 41x41 高解像度グリッド
        const float uvTile = 40.0f; // UV繰り返し回数（チェッカーボード模様用）

        groundModelData_.vertices.clear();
        groundModelData_.indices.clear();

        // 頂点の生成
        for (int iz = 0; iz < gridN; ++iz) {
            for (int ix = 0; ix < gridN; ++ix) {
                float x = -1.0f + 2.0f * static_cast<float>(ix) / (gridN - 1);
                float z = -1.0f + 2.0f * static_cast<float>(iz) / (gridN - 1);
                float u = static_cast<float>(ix) / (gridN - 1) * uvTile;
                float v = static_cast<float>(iz) / (gridN - 1) * uvTile;

                Sprite::VertexData vtx{};
                vtx.position = { x, 0.0f, z, 1.0f };
                vtx.texcoord = { u, v };
                vtx.normal = { 0.0f, 1.0f, 0.0f };
                groundModelData_.vertices.push_back(vtx);
            }
        }

        // インデックスの生成（2三角形 / セル）
        for (int iz = 0; iz < gridN - 1; ++iz) {
            for (int ix = 0; ix < gridN - 1; ++ix) {
                uint32_t bl = static_cast<uint32_t>(iz * gridN + ix);
                uint32_t br = bl + 1;
                uint32_t tl = bl + static_cast<uint32_t>(gridN);
                uint32_t tr = tl + 1;
                groundModelData_.indices.push_back(bl);
                groundModelData_.indices.push_back(tl);
                groundModelData_.indices.push_back(br);
                groundModelData_.indices.push_back(br);
                groundModelData_.indices.push_back(tl);
                groundModelData_.indices.push_back(tr);
            }
        }

        groundTextureIndex_ = TextureManager::GetInstance()->Load("Resources/checkerBoard.png");
        groundModelData_.material.textureIndex = groundTextureIndex_;
    }

    groundPlane_ = std::make_unique<Object3d>();
    if (groundPlane_) {
        groundPlane_->Initialize(object3dCom, groundModelData_);
        groundPlane_->SetCamera(playCamera_.get());
        groundPlane_->SetTranslate({ 0.0f, 0.0f, 0.0f });
        groundPlane_->SetScale({ 100.0f, 1.0f, 100.0f });
        groundPlane_->SetRotate({ 0.0f, 0.0f, 0.0f });
        groundPlane_->SetColor({ 0.45f, 0.82f, 0.45f, 1.0f }); // 鮮やかな草原カラー
        groundPlane_->SetEnableLighting(true);
        groundPlane_->Update();
    }

    // 4. プレイヤーの初期化
    player_ = std::make_unique<PikminPlayer>();
    player_->Initialize(object3dCom, playCamera_.get(), { 0.0f, 0.2f, 0.0f });

    // 5. ミニオンマネージャーの初期化（初期9体をスポーン: プレイヤー含め合計10体）
    minionManager_ = std::make_unique<MinionManager>();
    minionManager_->Initialize(object3dCom, playCamera_.get());
    // 残り7匹の小ロコロコをステージ奥に配置
    minionManager_->SpawnMinion({ 0.0f, 0.0f, 4.0f }, 9, MinionType::Blue);

    // プレイヤーの最初の大きさは 3（中・黄色: 本体1 + 吸収2 = 3、残り7匹がフィールドで待機）
    minionManager_->SetInitialAbsorbedCount(2);
    player_->SetSize(3);

    // 6. マウス照準・放物線ガイドの初期化
    aimGuide_ = std::make_unique<AimGuide>();
    aimGuide_->Initialize(object3dCom, playCamera_.get());

    // 7. 回転プロペラ障害物の初期化と配置
    propellerObstacles_.clear();
    {
        // プロペラ1: ステージ中央奥 (直径約3m, 時計回り回転)
        auto prop1 = std::make_unique<PropellerObstacle>();
        prop1->Initialize(object3dCom, playCamera_.get(), { 0.0f, 0.0f, 5.0f }, { 1.5f, 1.5f, 1.5f }, 2.5f);
        propellerObstacles_.push_back(std::move(prop1));

        // プロペラ2: ステージ左側 (直径約2.4m, 高速反時計回り回転)
        auto prop2 = std::make_unique<PropellerObstacle>();
        prop2->Initialize(object3dCom, playCamera_.get(), { -5.0f, 0.0f, -1.0f }, { 1.2f, 1.2f, 1.2f }, -3.2f);
        propellerObstacles_.push_back(std::move(prop2));
    }

    // 8. スライム同士（Minion-Minion, Player-Minion）はアプリ層の Multi-Sphere で高精度に処理するため、
    // エンジン側の単一球判定の重複適用（二重押し出し）を解除
    CollisionManager::GetInstance()->SetCollisionFilter(CollisionAttribute::Minion, CollisionAttribute::Minion, false);
    CollisionManager::GetInstance()->SetCollisionFilter(CollisionAttribute::Player, CollisionAttribute::Minion, false);

    isInitialized_ = true;
}

void GamePlayScene::Finalize()
{
    for (auto& prop : propellerObstacles_)
    {
        if (prop) prop->Finalize();
    }
    propellerObstacles_.clear();

    aimGuide_.reset();
    groundPlane_.reset();
    minionManager_.reset();
    player_.reset();
    playCamera_.reset();
    mouseInput_.reset();
    keyInput_.reset();
    cameraInitialized_ = false;
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

    // --- ステージ傾斜（ティルト）の入力とスムーズ補間 ---
    targetTilt_ = { 0.0f, 0.0f };
    if (keyInput_)
    {
        // W: 奥へ傾ける (Pitch > 0) / S: 手前へ傾ける (Pitch < 0)
        if (keyInput_->PushKey(DIK_W) || keyInput_->PushKey(DIK_UP))   targetTilt_.x += maxTiltAngle_;
        if (keyInput_->PushKey(DIK_S) || keyInput_->PushKey(DIK_DOWN)) targetTilt_.x -= maxTiltAngle_;
        // A: 左へ傾ける (Roll < 0) / D: 右へ傾ける (Roll > 0)
        if (keyInput_->PushKey(DIK_A) || keyInput_->PushKey(DIK_LEFT))  targetTilt_.y -= maxTiltAngle_;
        if (keyInput_->PushKey(DIK_D) || keyInput_->PushKey(DIK_RIGHT)) targetTilt_.y += maxTiltAngle_;
    }

    currentTilt_.x = SmoothDamp(currentTilt_.x, targetTilt_.x, tiltVelocity_.x, tiltSmoothTime_, deltaTime);
    currentTilt_.y = SmoothDamp(currentTilt_.y, targetTilt_.y, tiltVelocity_.y, tiltSmoothTime_, deltaTime);

    Vector3 playerPos = player_ ? player_->GetPosition() : Vector3{ 0.0f, 0.0f, 0.0f };
    Vector2 playerPivot = { playerPos.x, playerPos.z };

    // 地面プレーンの回転と位置を傾斜角に合わせて更新 (Z回転は負で右下がり、正で左下がり)
    // プレイヤーの足元 (playerPivot) をピボットとして回転させるため、平行移動オフセット P - R * P を適用
    if (groundPlane_)
    {
        Matrix4x4 R_tilt = Multiply(MakeRotateXMatrix(currentTilt_.x), MakeRotateZMatrix(-currentTilt_.y));
        Vector3 P = { playerPivot.x, 0.0f, playerPivot.y };
        Vector3 RP = {
            P.x * R_tilt.m[0][0] + P.y * R_tilt.m[1][0] + P.z * R_tilt.m[2][0],
            P.x * R_tilt.m[0][1] + P.y * R_tilt.m[1][1] + P.z * R_tilt.m[2][1],
            P.x * R_tilt.m[0][2] + P.y * R_tilt.m[1][2] + P.z * R_tilt.m[2][2]
        };
        groundPlane_->SetTranslate({ P.x - RP.x, P.y - RP.y, P.z - RP.z });
        groundPlane_->SetRotate({ currentTilt_.x, 0.0f, -currentTilt_.y });
        groundPlane_->Update();
    }

    // 照準ガイドはLocoRoco完全準拠のため無効化
    // if (aimGuide_ && player_ && playCamera_) ...

    // プレイヤーの更新（ステージ傾斜を伝達）
    if (player_)
    {
        player_->Update(deltaTime, keyInput_.get(), minionManager_.get(), mouseInput_.get(), aimGuide_.get(), currentTilt_);
    }

    // ミニオンマネージャーの更新（ステージ傾斜を伝達）
    if (minionManager_ && player_)
    {
        minionManager_->Update(deltaTime, player_->GetPosition(), player_->IsMerged(), player_->GetCurrentScale(), currentTilt_, player_->GetSlimeParams().squashStretch, player_->GetVelocity());
    }

    // 回転プロペラ障害物の更新（自転とステージ傾斜の追従）
    for (auto& prop : propellerObstacles_)
    {
        if (prop) prop->Update(deltaTime, currentTilt_, playerPivot);
    }

    // 衝突判定と押し出しの更新
    CollisionManager::GetInstance()->Update();

    // カメラの群れ重心追従 (LocoRoco方式: 全ロコロコの重心と広がりを捉える)
    if (playCamera_ && player_)
    {
        Vector3 focusTargetPos = player_->GetPosition();
        float groupSpread = 0.0f;
        if (minionManager_)
        {
            minionManager_->GetGroupCenterAndSpread(player_->GetPosition(), focusTargetPos, groupSpread);
        }

        Vector3 playerVel = player_->GetVelocity();

        // 1. プレイヤースケールおよび群れの広がり（Spread）に応じたカメラ距離
        // 合体巨大化時はスケールで後退し、分裂時は散らばり具合（Spread）で引いて全員を画面内に綺麗に収める
        float scaleOffset = (player_->GetCurrentScale() - 0.4f);
        float effectiveDist = cameraDistance_ + (std::max)(0.0f, scaleOffset) * cameraDynamicZoom_ + groupSpread * 0.75f;

        // 2. カメラの見下ろし角・方位角
        float pitch = cameraPitch_;
        float yaw = cameraYaw_;

        float cosPitch = std::cos(pitch);
        float sinPitch = std::sin(pitch);
        float cosYaw = std::cos(yaw);
        float sinYaw = std::sin(yaw);

        // 注視点からカメラ位置への相対オフセット（球面座標）
        Vector3 relativeOffset = {
            -effectiveDist * sinYaw * cosPitch,
            effectiveDist * sinPitch,
            -effectiveDist * cosYaw * cosPitch
        };

        // 3. 注視点（LookAt Target）と目標カメラ位置の算出
        // 群れの重心を捉え、視界を安定確保
        Vector3 lookAtTarget = {
            focusTargetPos.x,
            focusTargetPos.y + cameraTargetOffsetY_,
            focusTargetPos.z + cameraForwardOffset_
        };

        Vector3 targetCamPos = {
            lookAtTarget.x + relativeOffset.x,
            lookAtTarget.y + relativeOffset.y,
            lookAtTarget.z + relativeOffset.z
        };

        // 4. 目標カメラ回転の算出
        // 左右移動速度に応じた微小なダイナミックバンク（ロール傾斜: 左右に曲がった感覚を強調）
        float sideBank = -std::clamp(playerVel.x * cameraDynamicBank_, -0.05f, 0.05f);

        Vector3 targetCamRot = {
            pitch + (followStageTilt_ ? currentTilt_.x : 0.0f),
            yaw,
            sideBank + (followStageTilt_ ? -currentTilt_.y : 0.0f)
        };

        // 5. カメラ位置と回転の適用（臨界減衰スプリング SmoothDamp で極上のなめらかさを実現）
        if (!cameraInitialized_)
        {
            currentCameraPos_ = targetCamPos;
            currentCameraRot_ = targetCamRot;
            cameraPosVelocity_ = { 0.0f, 0.0f, 0.0f };
            cameraRotVelocity_ = { 0.0f, 0.0f, 0.0f };
            cameraInitialized_ = true;
        }
        else
        {
            // X軸（左右）: ラバーストラップラグでプレイヤーが左右に自然にシフトして移動が明確化
            currentCameraPos_.x = SmoothDamp(currentCameraPos_.x, targetCamPos.x, cameraPosVelocity_.x, cameraSideLagTime_, deltaTime);
            // Y, Z軸: プレイヤーとの高低差・距離を一定に保ちつつ、微細な段差ショックをシルクのようにいなす
            currentCameraPos_.y = SmoothDamp(currentCameraPos_.y, targetCamPos.y, cameraPosVelocity_.y, cameraSmoothTimePos_, deltaTime);
            currentCameraPos_.z = SmoothDamp(currentCameraPos_.z, targetCamPos.z, cameraPosVelocity_.z, cameraSmoothTimePos_, deltaTime);

            // 回転: C2級連続の超滑らかなスプリングイージング（急反転でもカクつき・ジャークが物理的にゼロ！）
            currentCameraRot_.x = SmoothDamp(currentCameraRot_.x, targetCamRot.x, cameraRotVelocity_.x, cameraSmoothTimeRot_, deltaTime);
            currentCameraRot_.y = SmoothDamp(currentCameraRot_.y, targetCamRot.y, cameraRotVelocity_.y, cameraSmoothTimeRot_, deltaTime);
            currentCameraRot_.z = SmoothDamp(currentCameraRot_.z, targetCamRot.z, cameraRotVelocity_.z, cameraSmoothTimeRot_, deltaTime);
        }

        playCamera_->SetTranslate(currentCameraPos_);
        playCamera_->SetRotate(currentCameraRot_);
        playCamera_->SetFovY(cameraFov_);
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

    // 2. 回転プロペラ障害物の描画
    for (auto& prop : propellerObstacles_)
    {
        if (prop) prop->Draw(ctx);
    }

    // 3. 放物線照準ガイドの描画 (LocoRoco完全準拠のため非表示)
    // if (aimGuide_) { aimGuide_->Draw(ctx); }

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
        // スライムはアプリ層の Multi-Sphere で正確にデバッグ描画するため、
        // エンジン側の単一球ワイヤーフレームの重複描画を一時的にスキップ
        if (player_ && player_->GetCollider()) player_->GetCollider()->SetIsEnabled(false);
        if (minionManager_) {
            for (const auto& m : minionManager_->GetMinions()) {
                if (m && m->GetCollider()) m->GetCollider()->SetIsEnabled(false);
            }
        }

        CollisionManager::GetInstance()->DrawDebug(playCamera_.get());

        if (player_ && player_->GetCollider()) player_->GetCollider()->SetIsEnabled(true);
        if (minionManager_) {
            for (const auto& m : minionManager_->GetMinions()) {
                if (m && m->GetCollider()) m->GetCollider()->SetIsEnabled(true);
            }
        }

        // アプリ層の Multi-Sphere ワイヤーフレームを描画（F1キーのデバッグ表示トグルと完全連動）
        if (CollisionManager::GetInstance()->IsShowDebugColliders())
        {
            if (minionManager_) minionManager_->DrawDebug(playCamera_.get());
            if (player_) player_->DrawDebug(playCamera_.get());
        }
    }

    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(460, 480), ImGuiCond_FirstUseEver);
    ImGui::Begin("Pikmin x LocoRoco Debug Panel", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "=== [ Pikmin x LocoRoco 3D Prototype ] ===");
    ImGui::Separator();

    // 1. 操作説明
    ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.2f, 1.0f), "[ Controls (LocoRoco 3D) ]");
    ImGui::BulletText("WASD / Arrows: Tilt Stage (ステージを傾けて全員で転がる)");
    ImGui::BulletText("E key: Split (弾けて全員小ロコロコに分裂)");
    ImGui::BulletText("Contact: Auto Merge (触れ合うとポコッと自動合体)");
    ImGui::BulletText("F1 key: Toggle Collision Wireframes (当たり判定表示ON/OFF)");
    ImGui::BulletText("SPACE key: Clear Scene");
    ImGui::Separator();

    // 2. ステート表示 & 合体トグル
    if (player_ && minionManager_)
    {
        bool isMerged = player_->IsMerged();
        int mergedCount = minionManager_->GetMergedCount();
        int totalCount = minionManager_->GetTotalCount();
        int activeCount = minionManager_->GetActiveCount();

        int currentSize = player_->GetSize();
        const char* tierLabel = "小 (1-2) [青]";
        ImVec4 tierColor = ImVec4(0.35f, 0.70f, 1.0f, 1.0f);
        if (currentSize >= 8) {
            tierLabel = "大 (8-10) [赤]";
            tierColor = ImVec4(1.0f, 0.35f, 0.3f, 1.0f);
        } else if (currentSize >= 3) {
            tierLabel = "中 (3-7) [黄色]";
            tierColor = ImVec4(1.0f, 0.90f, 0.2f, 1.0f);
        }

        ImGui::TextColored(tierColor, "Loco Size: %d / %d  Category: %s  (Scale: %.2f)",
                           currentSize, totalCount + 1, tierLabel, player_->GetCurrentScale());

        if (ImGui::Button("SPLIT (全員分裂: E key)", ImVec2(280, 36)))
        {
            if (minionManager_->GetMergedCount() > 0) {
                minionManager_->TriggerSplit(player_->GetPosition());
            }
        }

        float splitPop = minionManager_->GetSplitPopPower();
        if (ImGui::SliderFloat("Split Pop Power (はじけ水平威力)", &splitPop, 1.0f, 30.0f, "%.1f")) {
            minionManager_->SetSplitPopPower(splitPop);
        }

        float splitUp = minionManager_->GetSplitUpPower();
        if (ImGui::SliderFloat("Split Up Power (はじけ上昇威力)", &splitUp, 1.0f, 25.0f, "%.1f")) {
            minionManager_->SetSplitUpPower(splitUp);
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
    }

    ImGui::Separator();

    // 4. ステージ傾斜（ティルト）＆物理調整
    if (ImGui::CollapsingHeader("Stage Tilt & Rolling Physics", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Current Tilt: Pitch %.2f deg | Roll %.2f deg",
                    currentTilt_.x * 57.2958f, currentTilt_.y * 57.2958f);

        float maxDeg = maxTiltAngle_ * 57.2958f;
        if (ImGui::SliderFloat("Max Tilt Angle (deg)", &maxDeg, 5.0f, 35.0f, "%.1f")) {
            maxTiltAngle_ = maxDeg * 0.0174533f;
        }
        ImGui::SliderFloat("Tilt Smooth Time (傾斜スムーズ時間)", &tiltSmoothTime_, 0.05f, 0.40f, "%.2f s");

        if (player_) {
            float accel = player_->GetTiltAccel();
            if (ImGui::SliderFloat("Slime Tilt Accel", &accel, 10.0f, 60.0f, "%.1f")) {
                player_->SetTiltAccel(accel);
            }
            float friction = player_->GetFriction();
            if (ImGui::SliderFloat("Slime Friction (共通摩擦係数: 1.3)", &friction, 0.2f, 5.0f, "%.1f")) {
                player_->SetFriction(friction);
            }
        }
    }

    ImGui::Separator();

    // 5. プレイヤー調整
    if (player_)
    {

        // --- スライム表現パラメータ調整 ---
        auto& slimeParams = player_->GetSlimeParams();
        if (ImGui::CollapsingHeader("Slime Jelly & Wobble Params (ぷるぷる弾性調整)", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::SliderFloat("Wobble Strength (表面ぷるぷる強度)", &slimeParams.wobbleStrength, 0.0f, 0.5f, "%.3f");
            ImGui::SliderFloat("Wobble Frequency (揺れ周波数)", &slimeParams.wobbleFrequency, 1.0f, 15.0f, "%.1f");
            ImGui::SliderFloat("Fresnel Power (エッジ発光)", &slimeParams.fresnelPower, 0.5f, 6.0f, "%.1f");
            ImGui::SliderFloat("Env Reflection (環境反射)", &slimeParams.envReflection, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Inner Glow (内側グロー)", &slimeParams.innerGlow, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Shininess (ハイライト光沢)", &slimeParams.specularShininess, 8.0f, 128.0f, "%.0f");

            float color[4] = { slimeParams.baseColor.x, slimeParams.baseColor.y, slimeParams.baseColor.z, slimeParams.baseColor.w };
            if (ImGui::ColorEdit4("Slime Color", color))
            {
                slimeParams.baseColor = { color[0], color[1], color[2], color[3] };
            }

            if (ImGui::Button("Trigger Impulse Ripple (衝撃波紋テスト)"))
            {
                slimeParams.impulseStrength = 0.5f;
            }
        }
    }

    ImGui::Separator();

    // 6. プロペラ障害物のデバッグ調整
    if (!propellerObstacles_.empty() && ImGui::CollapsingHeader("Propeller Obstacles (プロペラ障害物)", ImGuiTreeNodeFlags_DefaultOpen))
    {
        for (size_t i = 0; i < propellerObstacles_.size(); ++i)
        {
            auto& prop = propellerObstacles_[i];
            if (!prop) continue;
            ImGui::PushID(static_cast<int>(i));
            ImGui::Text("Propeller [%zu] Pos: (%.1f, %.1f, %.1f)", i + 1, prop->GetPosition().x, prop->GetPosition().y, prop->GetPosition().z);
            ImGui::Text("  %d Wings | Radius: %.2fm",
                prop->GetDetectedWingCount(),
                prop->GetDetectedRadius() * prop->GetScale().x);
            ImGui::Text("  OBB: Len=%.2fm, Thick=%.2fm, Width=%.2fm, Y=%.2fm",
                prop->GetDetectedWingLength() * prop->GetScale().x,
                prop->GetDetectedWingThickness() * prop->GetScale().y,
                prop->GetDetectedWingWidth() * prop->GetScale().z,
                prop->GetDetectedWingCenterY() * prop->GetScale().y);
            float speed = prop->GetSpinSpeed();
            if (ImGui::SliderFloat("Spin Speed (自転速度)", &speed, -15.0f, 15.0f, "%.1f rad/s"))
            {
                prop->SetSpinSpeed(speed);
            }
            ImGui::PopID();
        }
    }

    ImGui::Separator();

    // 7. カメラ視認性・相対追従調整
    if (playCamera_ && ImGui::CollapsingHeader("Camera Settings (カメラ視認性・相対追従調整)", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.7f, 1.0f), "[ Critical Damped Spring (極上のなめらかさ) ]");
        ImGui::SliderFloat("Rotation Smooth Time (角度スムーズ時間: カクつきゼロ)", &cameraSmoothTimeRot_, 0.05f, 0.50f, "%.2f s");
        ImGui::SliderFloat("Side Lag Time (左右移動ラグ時間: 左右の視認性向上)", &cameraSideLagTime_, 0.05f, 0.40f, "%.2f s");
        ImGui::SliderFloat("Position Smooth Time (位置スムーズ時間: 段差ショック吸収)", &cameraSmoothTimePos_, 0.02f, 0.25f, "%.2f s");
        ImGui::SliderFloat("Tilt Smooth Time (ステージ傾斜スムーズ時間: 板の重厚感)", &tiltSmoothTime_, 0.05f, 0.40f, "%.2f s");
        ImGui::SliderFloat("Dynamic Bank (左右移動時バンク傾斜強度)", &cameraDynamicBank_, 0.0f, 0.06f, "%.3f");
        ImGui::Checkbox("Follow Stage Tilt (ステージの傾きにカメラ角度を連動)", &followStageTilt_);

        ImGui::Separator();
        float fovDeg = cameraFov_ * 57.2958f;
        if (ImGui::SliderFloat("FOV (視野角 deg)", &fovDeg, 30.0f, 90.0f, "%.1f deg"))
        {
            cameraFov_ = fovDeg * 0.0174533f;
        }

        ImGui::SliderFloat("Distance (カメラ距離)", &cameraDistance_, 8.0f, 45.0f, "%.1f m");

        float pitchDeg = cameraPitch_ * 57.2958f;
        if (ImGui::SliderFloat("Pitch Angle (見下ろし角度 deg)", &pitchDeg, 15.0f, 85.0f, "%.1f deg"))
        {
            cameraPitch_ = pitchDeg * 0.0174533f;
        }

        float yawDeg = cameraYaw_ * 57.2958f;
        if (ImGui::SliderFloat("Yaw Angle (水平旋回 deg)", &yawDeg, -180.0f, 180.0f, "%.1f deg"))
        {
            cameraYaw_ = yawDeg * 0.0174533f;
        }

        ImGui::SliderFloat("Target Height Y (注視点の高さ)", &cameraTargetOffsetY_, 0.0f, 5.0f, "%.1f m");
        ImGui::SliderFloat("Forward Look Offset (前方視界オフセット)", &cameraForwardOffset_, -5.0f, 10.0f, "%.1f m");
        ImGui::SliderFloat("Dynamic Zoom (巨大化時ズーム倍率)", &cameraDynamicZoom_, 0.0f, 8.0f, "%.1f");

        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "Camera Presets:");
        if (ImGui::Button("Default (広角見下ろし: 47 deg)"))
        {
            cameraDistance_ = 18.0f;
            cameraPitch_ = 0.82f;
            cameraYaw_ = 0.0f;
            cameraFov_ = 0.85f;
            cameraTargetOffsetY_ = 1.0f;
            cameraForwardOffset_ = 2.0f;
            cameraSideLagTime_ = 0.18f;
            cameraSmoothTimeRot_ = 0.24f;
            cameraSmoothTimePos_ = 0.08f;
            tiltSmoothTime_ = 0.15f;
            cameraDynamicBank_ = 0.025f;
            cameraDynamicZoom_ = 3.0f;
        }
        ImGui::SameLine();
        if (ImGui::Button("High Overhead (高所俯瞰: 60 deg)"))
        {
            cameraDistance_ = 22.0f;
            cameraPitch_ = 1.05f;
            cameraYaw_ = 0.0f;
            cameraFov_ = 0.90f;
            cameraTargetOffsetY_ = 0.5f;
            cameraForwardOffset_ = 1.0f;
            cameraSideLagTime_ = 0.16f;
            cameraSmoothTimeRot_ = 0.24f;
            cameraSmoothTimePos_ = 0.08f;
            tiltSmoothTime_ = 0.15f;
            cameraDynamicBank_ = 0.020f;
            cameraDynamicZoom_ = 3.5f;
        }
        ImGui::SameLine();
        if (ImGui::Button("Wide Panoramic (広域パノラマ)"))
        {
            cameraDistance_ = 26.0f;
            cameraPitch_ = 0.75f;
            cameraYaw_ = 0.0f;
            cameraFov_ = 1.00f;
            cameraTargetOffsetY_ = 1.5f;
            cameraForwardOffset_ = 3.0f;
            cameraSideLagTime_ = 0.20f;
            cameraSmoothTimeRot_ = 0.28f;
            cameraSmoothTimePos_ = 0.10f;
            tiltSmoothTime_ = 0.18f;
            cameraDynamicBank_ = 0.030f;
            cameraDynamicZoom_ = 4.0f;
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
