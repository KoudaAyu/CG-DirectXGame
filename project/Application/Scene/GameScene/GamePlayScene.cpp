#include "GamePlayScene.h"
#include "SceneManager.h"
#include "DirectXCom.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3dCom.h"
#include "Baziru3_Engine/Framework/Collision/CollisionManager.h"
#include "TextureManager.h"
#include "RenderContext.h"
#include "Application/GameObject/SlimePhysics.h"
#include "Application/Editor/StageLayout.h"
#include <cstdio>
#include "Game.h"

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
    currentFocusPos_ = { spawnBasePos_.x, 0.5f, spawnBasePos_.z };
    focusPosVelocity_ = { 0.0f, 0.0f, 0.0f };

    Vector3 initLookAt = {
        currentFocusPos_.x,
        currentFocusPos_.y + cameraTargetOffsetY_,
        currentFocusPos_.z + cameraForwardOffset_
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

    currentCameraDist_ = cameraDistance_;
    cameraDistVelocity_ = 0.0f;
    currentGroupSpread_ = 0.0f;
    groupSpreadVelocity_ = 0.0f;

    if (sceneManager_) {
        // 抜けるときに戻せるよう、入る前の値を控えておく
        previousSceneCamera_ = sceneManager_->GetCamera();
        sceneManager_->SetCamera(playCamera_.get());
    }

    Object3dCom* object3dCom = GetObject3dCom();
    if (object3dCom) {
        // 【重要】ここで控えたものを Finalize() で必ず戻す。
        // 入る前の値は Game が握っている engine カメラで、
        // TitleScene / ClearScene はこれを GetDefaultCamera() で借りている。
        // nullptr のまま抜けると、あちらのスライムや花火が出なくなる
        previousDefaultCamera_ = object3dCom->GetDefaultCamera();
        object3dCom->SetDefaultCamera(playCamera_.get());
    }

    // 3. プロペラを除くステージOBJモデル群（startLand, Land1, toLandRoad, roadCell）の読み込みと配置
    stageParts_.clear();
    SlimePhysics::ClearGroundMeshes();

    auto AddStagePart = [&](const std::string& name, const std::string& objFile, const Vector3& baseOffset, const std::string& defaultTex = "Resources/10days/land.png") {
        StagePart part;
        part.name = name;
        part.baseOffset = baseOffset;
        part.modelData = Object3d::LoadObjFile("Resources/10days", objFile);
        part.modelData.boundingRadius = 10000.0f; // 視錐台誤カリングを完全に防止

        std::string texPath = part.modelData.material.textureFilePath;
        if (texPath.empty()) {
            texPath = defaultTex;
        }
        part.textureIndex = TextureManager::GetInstance()->Load(texPath);
        part.modelData.material.textureIndex = part.textureIndex;

        part.object = std::make_unique<Object3d>();
        if (part.object) {
            part.object->Initialize(object3dCom, part.modelData);
            part.object->SetCamera(playCamera_.get());
            part.object->SetTranslate(baseOffset * groundScale_);
            part.object->SetScale({ groundScale_, groundScale_, groundScale_ });
            part.object->SetRotate({ 0.0f, 0.0f, 0.0f });
            part.object->SetColor({ 0.55f, 0.85f, 0.50f, 1.0f });
            part.object->SetEnableLighting(true);
            part.object->Update();

            part.collider = std::make_unique<MeshCollider>(part.object.get(), CollisionAttribute::Obstacle);
            CollisionManager::GetInstance()->RegisterCollider(part.collider.get());
            SlimePhysics::AddGroundMesh(part.object.get(), part.collider.get());
        }
        stageParts_.push_back(std::move(part));
    };

    // (1) 初期島: startLand.obj (マテリアル指定テクスチャ land.png)
    AddStagePart("startLand", "startLand.obj", { 0.0f, 0.0f, 0.0f }, "Resources/10days/land.png");

    // (2) 第1の島: Land1.obj (マテリアル指定テクスチャ land2.png)
    AddStagePart("Land1", "Land1.obj", { 0.0f, 0.0f, 0.0f }, "Resources/10days/land2.png");

    // (3) Land1接続路: toLandRoad.obj
    AddStagePart("toLandRoad", "toLandRoad.obj", { 0.0f, 0.0f, 0.0f }, "Resources/10days/land.png");

    // (4) 道ユニットセル: roadCell.obj (基本幅 45.563m)
    // Blender元位置セル
    AddStagePart("roadCell_0", "roadCell.obj", { 0.0f, 0.0f, 0.0f }, "Resources/10days/land.png");

    // 橋連結モード: startLandの開口部（X ≈ -189m）まで roadCell を5ステップ連結配置し、島の間を渡れるようにする
    if (bridgeConnectMode_)
    {
        const float stepWidth = 45.563018f;
        for (int i = 1; i <= 5; ++i)
        {
            AddStagePart("roadCell_" + std::to_string(i), "roadCell.obj", { stepWidth * static_cast<float>(i), 0.0f, 0.0f }, "Resources/10days/land.png");
        }
    }

    // 4. スライムマネージャーの初期化と初期スライム群の配置（前方に配置）
    slimeManager_ = std::make_unique<SlimeManager>();
    slimeManager_->Initialize(object3dCom, playCamera_.get());

    RespawnSlimesAtBase();

    // 6. マウス照準・放物線ガイドの初期化
    aimGuide_ = std::make_unique<AimGuide>();
    aimGuide_->Initialize(object3dCom, playCamera_.get());

    // 7. 回転プロペラ障害物の初期化と配置（※ユーザー要望により一時的に無効化）
    propellerObstacles_.clear();
    /*
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
    */

    // 8. スライム同士（エンジン側の属性では Minion-Minion / Player-Minion）は
    //    アプリ層の Multi-Sphere で高精度に処理するため、
    // エンジン側の単一球判定の重複適用（二重押し出し）を解除
    CollisionManager::GetInstance()->SetCollisionFilter(CollisionAttribute::Minion, CollisionAttribute::Minion, false);
    CollisionManager::GetInstance()->SetCollisionFilter(CollisionAttribute::Player, CollisionAttribute::Minion, false);

    // 9. 敵マネージャーの初期化
    //    ※ Initialize() の中で Player <-> Enemy のエンジン側フィルタを切っている。
    //      CollisionManager::Initialize() より後に呼ぶこと
    enemyManager_ = std::make_unique<EnemyManager>();
    enemyManager_->Initialize(object3dCom, playCamera_.get());

    // 10. コインマネージャーの初期化
    coinManager_ = std::make_unique<CoinManager>();
    coinManager_->Initialize(object3dCom, playCamera_.get());

    // 11. 演出（パーティクル）と HUD の初期化
    //     中身は GamePlaySceneFX.cpp / GamePlaySceneHUD.cpp にある。
    //     このシーンは「イベントを拾って渡す」だけに留めている
    fx_ = std::make_unique<GamePlaySceneFx>();
    fx_->Initialize(dxCommon_, playCamera_.get());

    hud_ = std::make_unique<GamePlaySceneHud>();
    hud_->Initialize();

    score_ = 0;
    elapsedSeconds_ = 0.0f;
    shakeTrauma_ = 0.0f;
    shakeTime_ = 0.0f;

    // 12. 配置エディタの初期化と、配置データ（JSON）の読み込み
    //     SpawnDebugSet() による仮スポーンは廃止。配置は全部 JSON から復元する
    placementEditor_ = std::make_unique<PlacementEditor>();
    {
        PlacementEditor::SceneRefs refs;
        refs.object3dCom = object3dCom;
        refs.camera = playCamera_.get();
        refs.enemyManager = enemyManager_.get();
        refs.coinManager = coinManager_.get();
        refs.slimeManager = slimeManager_.get();
        placementEditor_->Initialize(refs);
        placementEditor_->SetGroundBaseColor(groundBaseColor_);
    }

    {
        StageLayout layout;
        if (!layout.LoadFromFile(StageLayout::kDefaultPath))
        {
            // JSON が無い／壊れている。座標をハードコードすると地形差し替えで
            // 全部おかしくなるので、地形を実際にレイキャストして置ける場所を探す
            OutputDebugStringA("[PlacementEditor] stage_layout not found. Using generated fallback layout.\n");
            layout = StageLayout::MakeFallback();
        }
        else
        {
            // 地形を差し替えたあとの JSON は座標がそのまま残っているので、
            // 島の外を指していないか必ず検証する。ここを通さないと
            // 敵とコインが全部宙に浮いて奈落へ落ちる（実際に startLand で起きた）
            float validRatio = 0.0f;
            if (!layout.IsCompatibleWithCurrentTerrain(&validRatio))
            {
                char msg[192];
                std::snprintf(msg, sizeof(msg),
                              "[PlacementEditor] stage_layout does not fit the current terrain "
                              "(only %.0f%% on ground). Using generated fallback layout.\n",
                              validRatio * 100.0f);
                OutputDebugStringA(msg);
                layout = StageLayout::MakeFallback();
            }
        }
        placementEditor_->SetLayout(layout);
    }

    if (dxCommon_)
    {
        IrisTransition::GetInstance()->Initialize(dxCommon_);
    }
    isGameOverTransition_ = false;
    gameOverDelayTimer_ = 0.0f;

    // 9. プリミティブ生成による成長キューブアイテム (GrowthCube) の初期化・配置
    growthCubes_.clear();
    {
        // キューブ1: 小スライム群の前方（まっすぐ進むとすぐ取れる位置）
        auto cube1 = std::make_unique<GrowthCube>();
        cube1->Initialize(object3dCom, playCamera_.get(), { spawnBasePos_.x, spawnBasePos_.y + 0.15f, spawnBasePos_.z + 8.0f }, 0.85f);
        growthCubes_.push_back(std::move(cube1));

        // キューブ2: スタート平原の左側
        auto cube2 = std::make_unique<GrowthCube>();
        cube2->Initialize(object3dCom, playCamera_.get(), { spawnBasePos_.x - 3.8f, spawnBasePos_.y + 0.15f, spawnBasePos_.z + 3.0f }, 0.80f);
        growthCubes_.push_back(std::move(cube2));

        // キューブ3: スタート平原の右側
        auto cube3 = std::make_unique<GrowthCube>();
        cube3->Initialize(object3dCom, playCamera_.get(), { spawnBasePos_.x + 3.8f, spawnBasePos_.y + 0.15f, spawnBasePos_.z + 3.0f }, 0.80f);
        growthCubes_.push_back(std::move(cube3));

        // キューブ4: 通路・橋の手前（Z=48m）
        auto cube4 = std::make_unique<GrowthCube>();
        cube4->Initialize(object3dCom, playCamera_.get(), { spawnBasePos_.x, spawnBasePos_.y + 0.15f, spawnBasePos_.z + 18.0f }, 0.90f);
        growthCubes_.push_back(std::move(cube4));
    }

    isInitialized_ = true;
}

void GamePlayScene::ResetGrowthCubes()
{
    for (auto& cube : growthCubes_)
    {
        if (cube)
        {
            cube->Respawn();
        }
    }
}

void GamePlayScene::RespawnSlimesAtBase()
{
    if (!slimeManager_) return;
    slimeManager_->Clear();

    // レベル3のスライム1体のみ生成
    slimeManager_->SpawnSlime(spawnBasePos_, 3);

    // カメラ注視点もスポーン位置へ同期
    currentFocusPos_ = { spawnBasePos_.x, 0.5f, spawnBasePos_.z };
    focusPosVelocity_ = { 0.0f, 0.0f, 0.0f };
}

void GamePlayScene::RestartGame()
{
    // スライム群を初期配置で再生成
    RespawnSlimesAtBase();

    // 成長キューブを再出現
    ResetGrowthCubes();

    // ステージ傾斜を水平にリセット
    currentTilt_ = { 0.0f, 0.0f };
    targetTilt_ = { 0.0f, 0.0f };
    tiltVelocity_ = { 0.0f, 0.0f };

    // ステージ揺らし・バウンスをリセット
    stageBounceOffset_ = 0.0f;
    stageBounceVelocity_ = 0.0f;
    stageShakeTimer_ = 0.0f;
    stageShakeCooldown_ = 0.0f;
    cameraShakeIntensity_ = 0.0f;
    cameraShakeOffset_ = { 0.0f, 0.0f, 0.0f };

    // カメラのズーム・広がり追従を初期化
    currentCameraDist_ = cameraDistance_;
    cameraDistVelocity_ = 0.0f;
    currentGroupSpread_ = 0.0f;
    groupSpreadVelocity_ = 0.0f;

    isGameOverTransition_ = false;
    gameOverDelayTimer_ = 0.0f;
    if (dxCommon_)
    {
        IrisTransition::GetInstance()->Initialize(dxCommon_);
    }

    cameraInitialized_ = false;
}

void GamePlayScene::Finalize()
{
    if (hud_)
    {
        hud_->Finalize();
        hud_.reset();
    }
    if (fx_)
    {
        fx_->Finalize();
        fx_.reset();
    }

    // 配置エディタを開いたまま抜けたときの取りこぼしを防ぐ。
    // 保存対象は placementEditor_ が持つ配置データなので、
    // プレイ中に敵が倒されていてもファイルは汚れない
    if (placementEditor_)
    {
        if (isEditMode_)
        {
            placementEditor_->SetActive(false); // 中で自動保存される
            isEditMode_ = false;
        }
        else if (placementEditor_->IsDirty())
        {
            placementEditor_->Save();
        }
        placementEditor_->Finalize();
        placementEditor_.reset();
    }

    if (coinManager_)
    {
        coinManager_->Finalize();
        coinManager_.reset();
    }

    for (auto& prop : propellerObstacles_)
    {
        if (prop) prop->Finalize();
    }
    propellerObstacles_.clear();

    if (enemyManager_)
    {
        enemyManager_->Finalize();
        enemyManager_.reset();
    }

    aimGuide_.reset();
    SlimePhysics::ClearGroundMeshes();
    for (auto& part : stageParts_)
    {
        if (part.collider)
        {
            CollisionManager::GetInstance()->UnregisterCollider(part.collider.get());
            part.collider.reset();
        }
        part.object.reset();
    }
    stageParts_.clear();
    slimeManager_.reset();
    playCamera_.reset();
    mouseInput_.reset();
    keyInput_.reset();
    // 【バグ修正】以前はここで nullptr にしていたため、
    //   TITLE を再度読み込むとスライムが消える
    //   CLEAR を再度読み込むと花火が消える
    // という状態になっていた。どちらも Object3dCom::GetDefaultCamera() を
    // 借りているので、入る前の値（＝ Game が握っている engine カメラ）へ必ず戻す
    if (auto* object3dCom = GetObject3dCom())
    {
        object3dCom->SetDefaultCamera(previousDefaultCamera_);
    }
    if (sceneManager_ && previousSceneCamera_)
    {
        // SceneManager 側も戻す。ここが解放済みの playCamera_ を指したままだと、
        // 次のシーンが SceneManager::GetCamera() を触った瞬間に落ちる
        sceneManager_->SetCamera(previousSceneCamera_);
    }
    previousDefaultCamera_ = nullptr;
    previousSceneCamera_ = nullptr;
    cameraInitialized_ = false;
    isInitialized_ = false;
}

void GamePlayScene::SetEditMode(bool edit)
{
    if (isEditMode_ == edit) return;
    if (!placementEditor_) return;

    isEditMode_ = edit;
    placementEditor_->SetActive(edit);

    if (edit)
    {
        // 配置は「傾き0のときのワールド座標」で持っているので、
        // 編集中は板を水平に固定する。補間で傾いたままだと判定がぶれる
        targetTilt_ = { 0.0f, 0.0f };
        currentTilt_ = { 0.0f, 0.0f };
        tiltVelocity_ = { 0.0f, 0.0f };
    }
    else
    {
        // プレイに戻ったらカメラの補間状態を組み直す
        cameraInitialized_ = false;
    }
}

void GamePlayScene::Update()
{
    float deltaTime = 1.0f / 60.0f;

    if (keyInput_)
    {
        keyInput_->Update();

        // F2キーでプレイ <-> 配置エディタ を切り替え
        if (keyInput_->TriggerKey(DIK_F2))
        {
            SetEditMode(!isEditMode_);
        }

        // ENTERキーでクリアシーンへ遷移（SPACEキーはスライムのジャンプに割り当て）
        // 配置エディタ中は誤爆を避けるため無効
        if (!isEditMode_ && keyInput_->TriggerKey(DIK_RETURN))
        {
            // リザルトへ値を渡す。ClearScene が同じキーを読む
            SetSceneDataInt("result.score", score_);
            SetSceneDataFloat("result.time", elapsedSeconds_);
            SetSceneDataInt("result.coin", coinManager_ ? coinManager_->GetCollectedCount() : 0);

            SceneManager::GetInstance()->ChangeScene("CLEAR");
        }

        // Rキーで再スタート（初期配置でスライムを再生成、ステージ傾斜・カメラを初期化）
        if (keyInput_->TriggerKey(DIK_R))
        {
            RestartGame();
        }

        // F1キーで当たり判定ワイヤーフレーム表示/非表示をトグル
        if (keyInput_->TriggerKey(DIK_F1))
        {
            bool showColliders = CollisionManager::GetInstance()->IsShowDebugColliders();
            CollisionManager::GetInstance()->SetShowDebugColliders(!showColliders);
        }
    }

    if (mouseInput_)
    {
        mouseInput_->Update();
    }

    // --- ステージ傾斜（ティルト）の入力とスムーズ補間 ---
    targetTilt_ = { 0.0f, 0.0f };
    if (keyInput_ && !isEditMode_)
    {
        // W: 奥へ傾ける (Pitch > 0) / S: 手前へ傾ける (Pitch < 0)
        if (keyInput_->PushKey(DIK_W) || keyInput_->PushKey(DIK_UP))   targetTilt_.x += maxTiltAngle_;
        if (keyInput_->PushKey(DIK_S) || keyInput_->PushKey(DIK_DOWN)) targetTilt_.x -= maxTiltAngle_;
        // A: 左へ傾ける (Roll < 0) / D: 右へ傾ける (Roll > 0)
        if (keyInput_->PushKey(DIK_A) || keyInput_->PushKey(DIK_LEFT))  targetTilt_.y -= maxTiltAngle_;
        if (keyInput_->PushKey(DIK_D) || keyInput_->PushKey(DIK_RIGHT)) targetTilt_.y += maxTiltAngle_;
    }

    if (isEditMode_)
    {
        // 配置エディタ中は板を完全に水平へ固定する（補間させない）
        currentTilt_ = { 0.0f, 0.0f };
        tiltVelocity_ = { 0.0f, 0.0f };
    }
    else
    {
        currentTilt_.x = SmoothDamp(currentTilt_.x, targetTilt_.x, tiltVelocity_.x, tiltSmoothTime_, deltaTime);
        currentTilt_.y = SmoothDamp(currentTilt_.y, targetTilt_.y, tiltVelocity_.y, tiltSmoothTime_, deltaTime);
    }

    Vector3 slimeCenter = currentFocusPos_;
    Vector2 slimePivot = { slimeCenter.x, slimeCenter.z };

    // --- SPACEキーによるステージ揺らし（バウンス・シェイク）ジャンプ ---
    if (stageShakeCooldown_ > 0.0f)
    {
        stageShakeCooldown_ -= deltaTime;
    }

    if (keyInput_ && keyInput_->TriggerKey(DIK_SPACE) && stageShakeCooldown_ <= 0.0f)
    {
        stageShakeCooldown_ = 0.22f;          // 連打防止クールダウン
        stageBounceVelocity_ = 11.0f;         // ステージ上向き突き上げ初速 (ボヨン！)
        stageShakeTimer_ = stageShakeDuration_; // ステージ回転揺動開始
        cameraShakeIntensity_ = 0.35f;        // カメラ衝撃シェイク強度

        // スライム群衆のステージ突き上げジャンプ（接地スライムのみが床法線方向に打ち上げられる）
        if (slimeManager_)
        {
            slimeManager_->TriggerStageBounce(currentTilt_, slimePivot, 13.5f);
        }
    }

    // ステージ垂直バウンス（減衰バネ運動: 突き上がった後、弾力をもって元の高さに美しく収束）
    float bounceK = 360.0f;    // バネ定数
    float bounceDamp = 22.0f;  // 減衰係数
    float bounceAccel = -bounceK * stageBounceOffset_ - bounceDamp * stageBounceVelocity_;
    stageBounceVelocity_ += bounceAccel * deltaTime;
    stageBounceOffset_ += stageBounceVelocity_ * deltaTime;
    if (std::abs(stageBounceOffset_) < 0.001f && std::abs(stageBounceVelocity_) < 0.01f)
    {
        stageBounceOffset_ = 0.0f;
        stageBounceVelocity_ = 0.0f;
    }

    // ステージ回転揺動（シェイク: ドンと突いたときの微小な振動）
    Vector2 shakeTilt = { 0.0f, 0.0f };
    if (stageShakeTimer_ > 0.0f)
    {
        stageShakeTimer_ -= deltaTime;
        float progress = (std::max)(0.0f, stageShakeTimer_ / stageShakeDuration_);
        // 高周波減衰振動
        float wave = std::sin((stageShakeDuration_ - stageShakeTimer_) * 48.0f) * progress;
        shakeTilt.x = wave * stageShakeIntensity_;
        shakeTilt.y = std::cos((stageShakeDuration_ - stageShakeTimer_) * 40.0f) * progress * stageShakeIntensity_ * 0.7f;
    }

    // 全ステージパーツの回転を傾斜角＋揺動に合わせて更新
    // スライム群衆重心を回転中心（ピボット）にすることで、傾斜時にスライム直下の
    // 地面高さが変動しなくなり、めり込み・追従ズレを根本から解消
    if (!stageParts_.empty())
    {
        Vector3 rot = { currentTilt_.x + shakeTilt.x, 0.0f, -currentTilt_.y + shakeTilt.y };

        // ピボット = スライム重心の XZ 位置
        float px = slimeCenter.x;
        float pz = slimeCenter.z;

        // rot = {pitch(α), 0, roll(β)} の回転行列 R = Rx(α) * Rz(β) を手計算し、
        // ピボット点を R で変換した結果との差分を平行移動に設定
        // → ピボットが回転前後で同じワールド座標に留まる
        float cx = std::cos(rot.x), sx = std::sin(rot.x);
        float cz = std::cos(rot.z), sz = std::sin(rot.z);

        // R = Rx(α) * Rz(β) の行列（行ベクトル v * R）:
        //   Row0 = ( cβ,      sβ,      0  )
        //   Row1 = (-cα·sβ,   cα·cβ,   sα )  ← pivot.y = 0 なので寄与なし
        //   Row2 = ( sα·sβ,  -sα·cβ,   cα )
        // pivot_rotated = px * Row0 + pz * Row2
        float prx = px * cz + pz * (sx * sz);
        float pry = px * sz + pz * (-sx * cz);
        float prz = pz * cx;

        Vector3 groundTranslate = {
            px - prx,
            0.0f - pry + stageBounceOffset_,
            pz - prz
        };

        for (auto& part : stageParts_)
        {
            if (!part.object) continue;

            Vector3 offsetWorld = part.baseOffset * groundScale_;
            float ox = offsetWorld.x * cz + offsetWorld.z * (sx * sz);
            float oy = offsetWorld.x * sz + offsetWorld.z * (-sx * cz);
            float oz = offsetWorld.z * cx;

            Vector3 partTranslate = {
                groundTranslate.x + ox,
                groundTranslate.y + oy,
                groundTranslate.z + oz
            };

            part.object->SetTranslate(partTranslate);
            part.object->SetScale({ groundScale_, groundScale_, groundScale_ });
            part.object->SetRotate(rot);
            part.object->Update();

            if (part.collider)
            {
                part.collider->Update();
            }
        }
    }

    // 照準ガイドはLocoRoco完全準拠のため無効化
    // if (aimGuide_ && slimeManager_ && playCamera_) ...

    // スライム群衆の更新（全スライムの入力、物理、合体、分裂、衝突分離）
    // 配置エディタ中は入力を一切渡さず、速度も毎フレーム殺してその場に留める。
    // Update 自体は呼ぶので、地面追従とスライムシェーダーの時間だけは進む
    if (slimeManager_ && isEditMode_)
    {
        for (const auto& slimePtr : slimeManager_->GetSlimes())
        {
            if (slimePtr) slimePtr->SetVelocity({ 0.0f, 0.0f, 0.0f });
        }
        slimeManager_->Update(deltaTime, nullptr, { 0.0f, 0.0f });
    }
    else if (slimeManager_)
    {
        slimeManager_->Update(deltaTime, keyInput_.get(), currentTilt_);
    }

    // 回転プロペラ障害物の更新（自転とステージ傾斜の追従）
    for (auto& prop : propellerObstacles_)
    {
        if (prop) prop->Update(deltaTime, currentTilt_, slimePivot);
    }

    // プロペラ障害物メッシュと全スライムの精密メッシュ衝突解決
    for (auto& prop : propellerObstacles_)
    {
        if (!prop || !slimeManager_) continue;

        for (auto& slime : slimeManager_->GetSlimes())
        {
            if (!slime || !slime->IsActive()) continue;

            Vector3 sPos = slime->GetPosition();
            Vector3 sVel = slime->GetVelocity();
            float sRadius = slime->GetRadius();
            Vector3 squash = slime->GetSlimeParams().squashStretch;
            float impulse = 0.0f;

            if (prop->ResolveSlimeCollision(sPos, sVel, sRadius, slime->IsMerged(), squash, impulse))
            {
                slime->SetPosition(sPos);
                slime->SetVelocity(sVel);
                slime->SetState(SlimeState::Thrown);
                slime->GetSlimeParams().squashStretch = squash;
                slime->GetSlimeParams().impulseStrength = (std::max)(slime->GetSlimeParams().impulseStrength, impulse);
            }
        }
    }

    // 成長キューブアイテム (GrowthCube) の更新（浮遊、自転、ステージ傾斜追従、スライム当たり判定、巨大化）
    for (auto& cube : growthCubes_)
    {
        if (cube)
        {
            cube->Update(deltaTime, currentTilt_, slimePivot, slimeManager_.get());
        }
    }

    // 敵の更新（各スライムとの強弱判定・被弾ノックバックもここで解決される）
    // 配置エディタ中の停止は PlacementEditor が SetEditorMode() で伝えている
    if (enemyManager_)
    {
        enemyManager_->Update(deltaTime, currentTilt_, slimeManager_.get());
    }

    // コインの更新（地面追従と取得判定。エディタ中は取得しない）
    if (coinManager_)
    {
        coinManager_->Update(deltaTime, currentTilt_, slimeManager_.get());
    }

    // 衝突判定と押し出しの更新
    CollisionManager::GetInstance()->Update();

    // 配置エディタ中はカメラを真上からの見下ろしに乗っ取る
    if (isEditMode_ && placementEditor_)
    {
        placementEditor_->Update(deltaTime);
    }

    // カメラの群れ重心追従 (LocoRoco方式: 全ロコロコの重心と広がりを捉える)
    if (!isEditMode_ && playCamera_ && slimeManager_)
    {
        int livingCount = slimeManager_->GetLivingCount();
        if (livingCount == 0 && cameraInitialized_)
        {
            // ★ 全員死亡時: カメラを初期位置に戻さず、直前の位置・回転・距離のままその場に静止保持！
            focusPosVelocity_ = { 0.0f, 0.0f, 0.0f };
            cameraPosVelocity_ = { 0.0f, 0.0f, 0.0f };
            cameraRotVelocity_ = { 0.0f, 0.0f, 0.0f };
            cameraDistVelocity_ = 0.0f;
            groupSpreadVelocity_ = 0.0f;
        }
        else
        {
            Vector3 rawFocusPos = { 0.0f, 0.0f, 0.0f };
            float rawSpread = 1.0f;
            slimeManager_->GetGroupCenterAndSpread(rawFocusPos, rawSpread);

            // 注視点の高さ Y: 群れの自然な高さを追従
            rawFocusPos.y = (std::max)(0.5f, rawFocusPos.y + 0.3f);

            // 初回初期化
            if (!cameraInitialized_)
            {
                currentFocusPos_ = rawFocusPos;
                currentGroupSpread_ = rawSpread;
                focusPosVelocity_ = { 0.0f, 0.0f, 0.0f };
                groupSpreadVelocity_ = 0.0f;
            }
            else
            {
                // 注視点中心のスムーズ補間（合体による重心の瞬間ジャンプを防止）
                currentFocusPos_.x = SmoothDamp(currentFocusPos_.x, rawFocusPos.x, focusPosVelocity_.x, focusSmoothTime_, deltaTime);
                currentFocusPos_.y = SmoothDamp(currentFocusPos_.y, rawFocusPos.y, focusPosVelocity_.y, focusSmoothTime_, deltaTime);
                currentFocusPos_.z = SmoothDamp(currentFocusPos_.z, rawFocusPos.z, focusPosVelocity_.z, focusSmoothTime_, deltaTime);

                // 群れの広がりのスムーズ補間（合体でスライムが消えたときの急激なズームインを完全に緩和）
                currentGroupSpread_ = SmoothDamp(currentGroupSpread_, rawSpread, groupSpreadVelocity_, groupSpreadSmoothTime_, deltaTime);
            }

            // スケールおよび群れの広がり（Spread）に応じた目標カメラ距離
            float maxScale = 0.4f;
            for (const auto& s : slimeManager_->GetSlimes()) {
                if (s && s->IsActive()) maxScale = (std::max)(maxScale, s->GetCurrentScale());
            }
            float scaleOffset = (maxScale - 0.4f);
            float spreadOffset = (std::min)(maxSpreadOffset_, currentGroupSpread_ * cameraSpreadZoom_);
            float targetDist = cameraDistance_ + (std::max)(0.0f, scaleOffset) * cameraDynamicZoom_ + spreadOffset;
            targetDist = std::clamp(targetDist, minCameraDist_, maxCameraDist_);

            if (!cameraInitialized_)
            {
                currentCameraDist_ = targetDist;
                cameraDistVelocity_ = 0.0f;
            }
            else
            {
                currentCameraDist_ = SmoothDamp(currentCameraDist_, targetDist, cameraDistVelocity_, cameraZoomSmoothTime_, deltaTime);
            }

            float effectiveDist = std::clamp(currentCameraDist_, minCameraDist_, maxCameraDist_);

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
            // 滑らかに補間された注視点を基準にし、視界を安定確保
            Vector3 lookAtTarget = {
                currentFocusPos_.x,
                currentFocusPos_.y + cameraTargetOffsetY_,
                currentFocusPos_.z + cameraForwardOffset_
            };

            Vector3 targetCamPos = {
                lookAtTarget.x + relativeOffset.x,
                lookAtTarget.y + relativeOffset.y,
                lookAtTarget.z + relativeOffset.z
            };

            // ★★★ カメラの最低地上高クリアランスガード（ステージ接近・めり込み防止） ★★★
            // カメラ直下の傾斜面（ステージ）高さを算出し、常に十分な高度（地面から最低9.5m上空）を維持
            float groundYAtTargetCam = SlimePhysics::CalculateGroundHeight(targetCamPos.x, targetCamPos.z, currentTilt_, slimePivot);
            float minTargetCamY = groundYAtTargetCam + 9.5f;
            if (targetCamPos.y < minTargetCamY)
            {
                targetCamPos.y = minTargetCamY;
            }

            // 4. カメラ位置の適用（臨界減衰スプリング SmoothDamp で極上のなめらかさを実現）
            if (!cameraInitialized_)
            {
                currentCameraPos_ = targetCamPos;
                cameraPosVelocity_ = { 0.0f, 0.0f, 0.0f };
            }
            else
            {
                currentCameraPos_.x = SmoothDamp(currentCameraPos_.x, targetCamPos.x, cameraPosVelocity_.x, cameraSideLagTime_, deltaTime);
                currentCameraPos_.y = SmoothDamp(currentCameraPos_.y, targetCamPos.y, cameraPosVelocity_.y, cameraSmoothTimePos_, deltaTime);
                currentCameraPos_.z = SmoothDamp(currentCameraPos_.z, targetCamPos.z, cameraPosVelocity_.z, cameraSmoothTimePos_, deltaTime);
            }

            // ★★★ 最終補間後位置に対する絶対安全クリアランスガード ★★★
            float currentGroundAtCam = SlimePhysics::CalculateGroundHeight(currentCameraPos_.x, currentCameraPos_.z, currentTilt_, slimePivot);
            float absoluteMinCamY = currentGroundAtCam + 8.5f;
            if (currentCameraPos_.y < absoluteMinCamY)
            {
                currentCameraPos_.y = absoluteMinCamY;
                if (cameraPosVelocity_.y < 0.0f) cameraPosVelocity_.y = 0.0f;
            }

            // 5. 目標カメラ回転の算出（常にスライム注視点を真ん中に捉える Dynamic Look-At）
            Vector3 toTarget = lookAtTarget - currentCameraPos_;
            float distHoriz = std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z);
            float dynamicPitch = std::atan2(-toTarget.y, (std::max)(0.1f, distHoriz));
            float dynamicYaw = std::atan2(toTarget.x, toTarget.z);

            // 左右移動速度に応じた微小なダイナミックバンク
            float avgVelX = 0.0f;
            int totalWeight = 0;
            for (const auto& s : slimeManager_->GetSlimes()) {
                if (s && s->IsActive() && s->GetPosition().y >= SlimePhysics::GetVoidY()) {
                    int w = (std::max)(1, s->GetSize());
                    avgVelX += s->GetVelocity().x * static_cast<float>(w);
                    totalWeight += w;
                }
            }
            if (totalWeight > 0) avgVelX /= static_cast<float>(totalWeight);
            float sideBank = -std::clamp(avgVelX * cameraDynamicBank_, -0.05f, 0.05f);

            Vector3 targetCamRot = {
                dynamicPitch + (followStageTilt_ ? currentTilt_.x : 0.0f),
                dynamicYaw,
                sideBank + (followStageTilt_ ? -currentTilt_.y : 0.0f)
            };

            if (!cameraInitialized_)
            {
                currentCameraRot_ = targetCamRot;
                cameraRotVelocity_ = { 0.0f, 0.0f, 0.0f };
                cameraInitialized_ = true;
            }
            else
            {
                currentCameraRot_.x = SmoothDamp(currentCameraRot_.x, targetCamRot.x, cameraRotVelocity_.x, cameraSmoothTimeRot_, deltaTime);
                currentCameraRot_.y = SmoothDamp(currentCameraRot_.y, targetCamRot.y, cameraRotVelocity_.y, cameraSmoothTimeRot_, deltaTime);
                currentCameraRot_.z = SmoothDamp(currentCameraRot_.z, targetCamRot.z, cameraRotVelocity_.z, cameraSmoothTimeRot_, deltaTime);
            }
        }

        // カメラ衝撃シェイクの減衰と適用
        if (cameraShakeIntensity_ > 0.001f)
        {
            cameraShakeIntensity_ *= (1.0f - (std::min)(1.0f, deltaTime * 12.0f));
            float camWaveY = std::sin(stageShakeTimer_ * 55.0f) * cameraShakeIntensity_;
            float camWaveX = std::cos(stageShakeTimer_ * 45.0f) * cameraShakeIntensity_ * 0.5f;
            cameraShakeOffset_ = { camWaveX, camWaveY, 0.0f };
        }
        else
        {
            cameraShakeIntensity_ = 0.0f;
            cameraShakeOffset_ = { 0.0f, 0.0f, 0.0f };
        }

        Vector3 finalCamPos = currentCameraPos_ + cameraShakeOffset_;
        playCamera_->SetTranslate(finalCamPos);
        playCamera_->SetRotate(currentCameraRot_);
        playCamera_->SetFovY(cameraFov_);
        playCamera_->Update();
    }

    // カメラシェイク（カメラ本体へオフセットを載せ直す。補間の基準は汚さない）
    UpdateCameraShake(deltaTime);

    // カメラの最新ViewProjection行列に合わせて、各ステージパーツのWVP定数バッファを同期更新
    for (auto& part : stageParts_)
    {
        if (part.object)
        {
            part.object->Update();
        }
    }

    // 6. トランジション（IrisTransition）の更新と生存スライム死活監視
    IrisTransition::GetInstance()->Update(deltaTime);

    if (isInitialized_ && slimeManager_)
    {
        int livingCount = slimeManager_->GetLivingCount();
        if (livingCount == 0)
        {
            if (!isGameOverTransition_)
            {
                gameOverDelayTimer_ += deltaTime;
                // スライム落下後の余韻（0.3秒）を経てアイリスアウトを開始
                if (gameOverDelayTimer_ >= 0.3f)
                {
                    isGameOverTransition_ = true;

                    // アイリスの中心: 最後に生存していたスライムの画面位置（または画面中央）
                    Vector2 irisCenter = { 0.5f, 0.5f };
                    if (playCamera_)
                    {
                        Matrix4x4 vp = Multiply(playCamera_->GetViewMatrix(), playCamera_->GetProjectionMatrix());
                        float x = currentFocusPos_.x * vp.m[0][0] + currentFocusPos_.y * vp.m[1][0] + currentFocusPos_.z * vp.m[2][0] + vp.m[3][0];
                        float y = currentFocusPos_.x * vp.m[0][1] + currentFocusPos_.y * vp.m[1][1] + currentFocusPos_.z * vp.m[2][1] + vp.m[3][1];
                        float w = currentFocusPos_.x * vp.m[0][3] + currentFocusPos_.y * vp.m[1][3] + currentFocusPos_.z * vp.m[2][3] + vp.m[3][3];
                        if (w > 0.05f)
                        {
                            float ndcX = x / w;
                            float ndcY = y / w;
                            float u = (ndcX + 1.0f) * 0.5f;
                            float v = (1.0f - ndcY) * 0.5f;
                            if (u >= 0.1f && u <= 0.9f && v >= 0.1f && v <= 0.9f)
                            {
                                irisCenter = { u, v };
                            }
                        }
                    }

                    IrisTransition::GetInstance()->StartIrisOut(1.0f, irisCenter);
                }
            }
            else
            {
                // 暗転完了（完全黒画面）でゲームオーバーシーンへ即座に切り替え
                if (IrisTransition::GetInstance()->IsIrisOutComplete())
                {
                    Fade* savedFade = SceneManager::GetInstance()->GetFadeApplication();
                    SceneManager::GetInstance()->SetFadeApplication(nullptr);
                    SceneManager::GetInstance()->ChangeScene("GAMEOVER");
                    SceneManager::GetInstance()->SetFadeApplication(savedFade);

                    // 【重要・触るな】ここで必ず return すること。
                    // SetFadeApplication(nullptr) にしているので ChangeScene() は
                    // 「フェード後に切り替える予約」ではなく **即時切り替え** になり、
                    // その中で SceneManager::CommitPendingSceneChange() が
                    //   scene_->Finalize(); scene_.reset();
                    // を実行する。つまりこの時点で this は解放済み。
                    // ここから下（UpdateFxAndHud / DrawDebugUI）へ進むと
                    // 解放済みの slimeManager_ を触ってアクセス違反で落ちる
                    return;
                }
            }
        }
        else
        {
            gameOverDelayTimer_ = 0.0f;
        }
    }

    // 演出と HUD。中身は GamePlaySceneFX.cpp / GamePlaySceneHUD.cpp
    UpdateFxAndHud(deltaTime);

    DrawDebugUI();
}

void GamePlayScene::AddCameraShake(float trauma)
{
    shakeTrauma_ = (std::min)(1.0f, shakeTrauma_ + trauma);
}

void GamePlayScene::UpdateCameraShake(float deltaTime)
{
    if (!playCamera_) return;

    shakeTime_ += deltaTime;
    shakeTrauma_ = (std::max)(0.0f, shakeTrauma_ - shakeDecay_ * deltaTime);
    if (shakeTrauma_ <= 0.0001f) return;

    // trauma の2乗にすると、減衰の終わりぎわがすっと収まる
    const float amount = shakeTrauma_ * shakeTrauma_;

    // 位相と周期をずらした sin を重ねて疑似ノイズにする。
    // 乱数を使わないので、同じ時刻なら必ず同じ揺れになる（デバッグしやすい）
    const float t = shakeTime_ * shakeFrequency_;
    const float nx = std::sin(t * 1.00f) * 0.6f + std::sin(t * 2.37f + 1.7f) * 0.4f;
    const float ny = std::sin(t * 1.31f + 2.4f) * 0.6f + std::sin(t * 2.71f + 0.3f) * 0.4f;
    const float nz = std::sin(t * 0.87f + 4.1f) * 0.6f + std::sin(t * 1.93f + 5.2f) * 0.4f;

    // ステージ揺らし（SPACE のドン）による cameraShakeOffset_ を土台に足しこむ。
    // ここで currentCameraPos_ だけを基準にすると、あちらの揺れを打ち消してしまう
    const Vector3 shakenPos = {
        currentCameraPos_.x + cameraShakeOffset_.x + nx * shakeAmplitude_ * amount,
        currentCameraPos_.y + cameraShakeOffset_.y + ny * shakeAmplitude_ * amount,
        currentCameraPos_.z + cameraShakeOffset_.z + nz * shakeAmplitude_ * amount * 0.5f,
    };
    const Vector3 shakenRot = {
        currentCameraRot_.x + ny * shakeRollAmount_ * amount * 0.4f,
        currentCameraRot_.y + nx * shakeRollAmount_ * amount * 0.4f,
        currentCameraRot_.z + nz * shakeRollAmount_ * amount,
    };

    playCamera_->SetTranslate(shakenPos);
    playCamera_->SetRotate(shakenRot);
    playCamera_->Update();
}

int GamePlayScene::CalculateLifeCount() const
{
    // 残機 ＝ フィールドに残っている全スライムのサイズ合計。
    // スライムが一本化されて「プレイヤー本体 ＋ ミニオン」の区別が無くなったので、
    // SlimeManager がそのまま合計を持っている
    return slimeManager_ ? slimeManager_->GetTotalSize() : 0;
}

// ===================================================================
// 演出と HUD へのイベントの流し込み
//
// 演出そのものは GamePlaySceneFX.cpp、UI そのものは GamePlaySceneHUD.cpp。
// ここは「誰が何をしたか」を拾って渡すだけ。
// SE を入れる場所も、まとめてここにマークしてある
// ===================================================================
void GamePlayScene::UpdateFxAndHud(float deltaTime)
{
    if (!isEditMode_)
    {
        elapsedSeconds_ += deltaTime;
    }

    // 演出・HUD が「プレイヤー」として見る対象は群れの代表（一番大きい個体）。
    // 合体でスライムの実体が破棄されるので、毎フレームここで引き直すこと
    Slime* leader = slimeManager_ ? slimeManager_->GetLeader() : nullptr;

    const Vector3 playerPos = leader ? leader->GetPosition() : currentFocusPos_;
    const float playerRadius = leader ? leader->GetCurrentScale() * 0.78f : 0.5f;
    const Vector4 playerColor = leader ? leader->GetSlimeParams().baseColor
                                       : Vector4{ 0.2f, 0.85f, 1.0f, 1.0f };

    // --- 敵まわりのイベント ---
    if (enemyManager_ && !isEditMode_)
    {
        // 撃破。スコアの増分は「このフレームに倒された敵の強さの合計」の二乗
        int defeatedStrengthSum = 0;
        for (const auto& ev : enemyManager_->GetDefeatEvents())
        {
            defeatedStrengthSum += ev.strength;
            if (fx_) fx_->EmitEnemyDefeat(ev.position, ev.strength);
        }

        if (defeatedStrengthSum > 0)
        {
            const int gain = defeatedStrengthSum * defeatedStrengthSum;
            score_ += gain;

            // 増分をプレイヤーの頭の上に浮かばせる
            if (hud_)
            {
                hud_->PushScorePopup(gain, { playerPos.x,
                                             playerPos.y + playerRadius * hud_->popupOffsetY_,
                                             playerPos.z });
            }
        }

        // 倒せなかった接触（跳ね返された／押し合った）
        for (const auto& ev : enemyManager_->GetHitEvents())
        {
            if (fx_) fx_->EmitEnemyHitSplash(ev.position, playerColor);

            // 軽くカメラを揺らす
            AddCameraShake(shakeOnEnemyHit_);

            // TODO(SE): 敵とプレイヤー（ミニオン）の衝突音をここで鳴らす
            //           ev.isPlayer で本体とミニオンを鳴らし分けられる
        }
    }

    // --- スライムのイベント（ジャンプ / 自爆）---
    if (slimeManager_)
    {
        SlimeManager::FxEvents ev;
        if (slimeManager_->TakeFxEvents(ev))
        {
            if (ev.jumped)
            {
                // TODO(SE): プレイヤーのジャンプ音をここで鳴らす
            }

            if (ev.split)
            {
                if (fx_) fx_->EmitPlayerSplit(ev.splitPosition, ev.splitSizeBefore);

                // やや強めにカメラを揺らす
                AddCameraShake(shakeOnSelfDestruct_);

                // TODO(SE): プレイヤーの自爆（E キー分裂）音をここで鳴らす
            }
        }
    }

    // --- コイン取得 ---
    if (coinManager_)
    {
        for (const Vector3& position : coinManager_->GetCollectEvents())
        {
            (void)position;
            // 光は CoinManager 側のコインが「消えきる」まで、
            // GamePlaySceneFx::UpdateCoins() が出し続ける

            // TODO(SE): コイン取得音をここで鳴らす
        }
    }

    // --- 常時出ている演出 ---
    if (fx_)
    {
        fx_->UpdateAll(deltaTime, playerPos, leader, slimeManager_.get(),
                       coinManager_.get(), enemyManager_.get());
    }

    // --- HUD ---
    if (hud_)
    {
        GamePlaySceneHud::FrameInput frame;
        frame.camera = playCamera_.get();
        frame.player = leader;
        frame.slimeManager = slimeManager_.get();
        frame.enemyManager = enemyManager_.get();
        frame.score = score_;
        frame.elapsedSeconds = elapsedSeconds_;
        frame.coin = coinManager_ ? coinManager_->GetCollectedCount() : 0;
        frame.life = CalculateLifeCount();
        frame.showHeadNumbers = !isEditMode_;

        hud_->Update(deltaTime, frame);

        if (hud_->TakeLifeLostEvent())
        {
            // TODO(SE): プレイヤーの残機（スライムの数）が減ったときの音をここで鳴らす
        }

        if (hud_->TakeCounterTickEvent())
        {
            // TODO(SE): カウンター（スコア・コイン）が増えていくときの音をここで鳴らす
            //           クリアシーン側の同じ音は ClearScene::UpdateNumbers() にマークしてある
        }
    }
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
    // ライトを入れておかないと Object3dCom::Draw() が b1 に アドレス0 を張ってしまう。
    // スキニング経路（Object3d::Draw()）は自前でライトを解決するので、
    // 入れておかないと敵だけ陰影が変わる
    ctx.light = SceneManager::GetInstance()->GetLight();

    // 1. 地面の描画（プロペラを除く全ステージOBJモデル群）
    for (auto& part : stageParts_)
    {
        if (!part.object) continue;

        RenderContext partCtx = ctx;
        if (part.textureIndex != TextureManager::kInvalidTextureIndex) {
            partCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(part.textureIndex);
        }

        // 配置エディタ中は、上段（一本道）越しに下段が見えるようパーツごとに半透明で描く。
        // 地形が複数パーツに分かれたので、旧実装（単一メッシュ）と違い
        // 「上段だけを透かす」ことも原理的には可能になっている
        bool drawnTranslucent = false;
        if (isEditMode_ && placementEditor_)
        {
            drawnTranslucent = placementEditor_->DrawGroundTranslucent(partCtx, part.object.get(), part.modelData);
        }
        if (!drawnTranslucent)
        {
            object3dCom->Draw(part.object.get(), partCtx, part.modelData, true);
        }
    }

    // 2. 回転プロペラ障害物の描画
    for (auto& prop : propellerObstacles_)
    {
        if (prop)
        {
            prop->Draw(ctx);
        }
    }

    // （放物線照準ガイドは LocoRoco 完全準拠のため非表示）
    // if (aimGuide_) { aimGuide_->Draw(ctx); }

    // 3. 敵と敵弾の描画
    if (enemyManager_)
    {
        enemyManager_->Draw(ctx);
    }

    // 4. コインの描画
    if (coinManager_)
    {
        coinManager_->Draw(ctx);
    }

    // 5. プリミティブ成長キューブアイテムの描画
    for (auto& cube : growthCubes_)
    {
        if (cube)
        {
            cube->Draw(ctx);
        }
    }

    // 6. 全スライムの描画
    if (slimeManager_)
    {
        slimeManager_->Draw(ctx);
    }

    // 7. 最前面アイリストランジションの描画（円形暗転マスク）
    if (dxCommon_ && dxCommon_->GetCommandList())
    {
        IrisTransition::GetInstance()->Draw(dxCommon_->GetCommandList().Get());
    }

    // 8. パーティクル演出
    //    PSO はデプステスト有・書き込み無なので、デプスを書くものを全部描いたあとに
    if (fx_)
    {
        fx_->Draw(ctx.commandList);
    }

    // 9. 配置エディタのオーバーレイ（配置禁止領域・選択マーカー・カーソル）
    //    デプス書き込みを切ってあるので、3D の中では一番最後に描く
    if (isEditMode_ && placementEditor_)
    {
        placementEditor_->Draw(ctx);
    }

    // 10. HUD（2D）
    //    Sprite の PSO はデプス無効なので、最後に描けば必ず手前に来る
    if (hud_)
    {
        hud_->Draw(ctx.commandList);
    }
}

void GamePlayScene::DrawDebugUI()
{
    // コライダーのデバッグワイヤーフレーム描画
    if (playCamera_)
    {
        CollisionManager::GetInstance()->DrawDebug(playCamera_.get());
        if (slimeManager_)
        {
            slimeManager_->DrawDebug(playCamera_.get());
        }
    }

#ifdef USE_IMGUI
    // F3キーで全ImGuiが非表示に設定されている場合は描画スキップ
    if (!Game::IsImGuiVisible()) return;

    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(460, 480), ImGuiCond_FirstUseEver);
    ImGui::Begin("Pikmin x LocoRoco Debug Panel", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

    // --- 演出 / HUD / カメラシェイク ---
    if (fx_) fx_->DrawImGui();
    if (hud_) hud_->DrawImGui();

    if (ImGui::CollapsingHeader("Camera Shake"))
    {
        ImGui::Text("Trauma: %.3f", shakeTrauma_);
        ImGui::DragFloat("Decay", &shakeDecay_, 0.05f, 0.1f, 12.0f);
        ImGui::DragFloat("Amplitude (m)", &shakeAmplitude_, 0.01f, 0.0f, 3.0f);
        ImGui::DragFloat("Roll (rad)", &shakeRollAmount_, 0.005f, 0.0f, 0.5f);
        ImGui::DragFloat("Frequency", &shakeFrequency_, 0.5f, 1.0f, 90.0f);
        ImGui::DragFloat("On Enemy Hit", &shakeOnEnemyHit_, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("On Self Destruct", &shakeOnSelfDestruct_, 0.01f, 0.0f, 1.0f);
        if (ImGui::Button("Shake (hit)")) AddCameraShake(shakeOnEnemyHit_);
        ImGui::SameLine();
        if (ImGui::Button("Shake (self destruct)")) AddCameraShake(shakeOnSelfDestruct_);
    }

    if (ImGui::CollapsingHeader("Result"))
    {
        ImGui::Text("Score: %d", score_);
        ImGui::Text("Time : %.1f s", elapsedSeconds_);
        ImGui::Text("Coin : %d", coinManager_ ? coinManager_->GetCollectedCount() : 0);
        ImGui::Text("Life : %d", CalculateLifeCount());
        ImGui::DragInt("Score (debug)", &score_, 10.0f, 0, 999999);
        if (ImGui::Button("Reset Score / Time"))
        {
            score_ = 0;
            elapsedSeconds_ = 0.0f;
        }
    }

    // --- プレイ / 配置エディタ の切り替え（F2 と同じ）---
    {
        ImGui::SeparatorText("Mode");
        ImGui::Text("Now: %s", isEditMode_ ? "EDIT (placement)" : "PLAY");
        ImGui::SameLine();
        if (ImGui::Button(isEditMode_ ? "Back to Play (F2)" : "Placement Editor (F2)"))
        {
            SetEditMode(!isEditMode_);
        }
    }

    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "=== [ Pikmin x LocoRoco 3D Prototype ] ===");
    ImGui::Separator();

    // 1. 操作説明
    ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.2f, 1.0f), "[ Controls (LocoRoco 3D) ]");
    ImGui::BulletText("WASD / Arrows: Tilt Stage (ステージを傾けて全員で転がる)");
    ImGui::BulletText("SPACE key: Stage Shake Jump (ステージをドンと揺らして一斉ジャンプ！)");
    ImGui::BulletText("E key: Split (弾けて全員小ロコロコに分裂)");
    ImGui::BulletText("F key: Merge / Stick (閾値内の仲間スライムを合体・くっつける)");
    ImGui::BulletText("Contact: Bounce & Separation (通常接触時は弾性反発・くっつかない)");
    ImGui::BulletText("F1 key: Toggle Collision Wireframes (当たり判定表示ON/OFF)");
    ImGui::BulletText("F3 key: Toggle All ImGui (全ImGui表示/非表示)");
    ImGui::Separator();

    // 2. スライム状態表示 & 合体/分裂コントロール
    if (slimeManager_)
    {
        int totalCount = slimeManager_->GetTotalCount();
        int activeCount = slimeManager_->GetActiveCount();
        int maxSlimeSize = slimeManager_->GetMaxSlimeSize();
        int totalSize = slimeManager_->GetTotalSize();

        const char* tierLabel = "小 (1-2) [青]";
        ImVec4 tierColor = ImVec4(0.35f, 0.70f, 1.0f, 1.0f);
        if (maxSlimeSize >= 8) {
            tierLabel = "大 (8-10) [赤]";
            tierColor = ImVec4(1.0f, 0.35f, 0.3f, 1.0f);
        } else if (maxSlimeSize >= 3) {
            tierLabel = "中 (3-7) [黄色]";
            tierColor = ImVec4(1.0f, 0.90f, 0.2f, 1.0f);
        }

        ImGui::TextColored(tierColor, "Max Slime Size: %d | Total Mass: %d | Category: %s",
                           maxSlimeSize, totalSize, tierLabel);
        ImGui::TextColored(ImVec4(0.5f, 0.85f, 1.0f, 1.0f), "Active Slimes in Field: %d / %d",
                           activeCount, totalCount);

        if (ImGui::Button("STAGE SHAKE JUMP (ステージ揺らしジャンプ: SPACE key)", ImVec2(485, 36)))
        {
            if (stageShakeCooldown_ <= 0.0f)
            {
                stageShakeCooldown_ = 0.22f;
                stageBounceVelocity_ = 11.0f;
                stageShakeTimer_ = stageShakeDuration_;
                cameraShakeIntensity_ = 0.35f;
                Vector3 cPos = currentFocusPos_;
                slimeManager_->TriggerStageBounce(currentTilt_, { cPos.x, cPos.z }, 13.5f);
            }
        }

        if (ImGui::Button("SPLIT (全員分裂: E key)", ImVec2(240, 36)))
        {
            slimeManager_->TriggerSplit();
        }
        ImGui::SameLine();
        if (ImGui::Button("MERGE (合体: F key)", ImVec2(240, 36)))
        {
            slimeManager_->RequestMerge();
        }

        float mergeThreshold = slimeManager_->GetMergeThreshold();
        if (ImGui::SliderFloat("Merge Threshold (合体距離閾値)", &mergeThreshold, 0.5f, 10.0f, "%.2f m")) {
            slimeManager_->SetMergeThreshold(mergeThreshold);
        }

        float splitPop = slimeManager_->GetSplitPopPower();
        if (ImGui::SliderFloat("Split Pop Power (はじけ水平威力)", &splitPop, 1.0f, 30.0f, "%.1f")) {
            slimeManager_->SetSplitPopPower(splitPop);
        }

        float splitUp = slimeManager_->GetSplitUpPower();
        if (ImGui::SliderFloat("Split Up Power (はじけ上昇威力)", &splitUp, 1.0f, 25.0f, "%.1f")) {
            slimeManager_->SetSplitUpPower(splitUp);
        }

        ImGui::Separator();

        // 3. スライムスポーン管理
        ImGui::Text("Spawn Controls:");
        ImGui::DragFloat3("Spawn Base Pos (基準位置)", &spawnBasePos_.x, 0.2f, -50.0f, 50.0f, "%.1f m");
        ImGui::SliderFloat("Group Forward Offset (群れ前方オフセット)", &spawnGroupOffsetZ_, 1.0f, 10.0f, "%.1f m");

        if (ImGui::Button("Restart Game [R] (再スタート)", ImVec2(280, 30))) {
            RestartGame();
        }

        Vector3 spawnCenter = spawnBasePos_;
        if (playCamera_) spawnCenter = currentFocusPos_;

        if (ImGui::Button("+1 Spawn")) {
            slimeManager_->SpawnSlime(spawnCenter, 1);
        }
        ImGui::SameLine();
        if (ImGui::Button("+5 Spawn")) {
            slimeManager_->SpawnSlimes(spawnCenter, 5, 1);
        }
        ImGui::SameLine();
        if (ImGui::Button("+10 Spawn")) {
            slimeManager_->SpawnSlimes(spawnCenter, 10, 1);
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear All")) {
            slimeManager_->Clear();
        }
    }

    ImGui::Separator();

    // 3.5. 成長キューブアイテム (Growth Cubes)
    if (ImGui::CollapsingHeader("Growth Cubes (成長キューブアイテム)", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "Primitive-Generated Collectable Cubes:");
        ImGui::Text("Active Cubes: %zu", growthCubes_.size());

        if (ImGui::Button("Respawn All Cubes (全キューブ復活)", ImVec2(240, 28)))
        {
            ResetGrowthCubes();
        }

        ImGui::SameLine();
        if (ImGui::Button("Spawn Cube At Camera Target", ImVec2(220, 28)))
        {
            auto newCube = std::make_unique<GrowthCube>();
            Vector3 pos = currentFocusPos_;
            pos.y += 0.5f;
            pos.z += 2.0f;
            newCube->Initialize(GetObject3dCom(), playCamera_.get(), pos, 0.85f);
            growthCubes_.push_back(std::move(newCube));
        }

        for (size_t i = 0; i < growthCubes_.size(); ++i)
        {
            if (!growthCubes_[i]) continue;
            auto state = growthCubes_[i]->GetState();
            const char* stateStr = "Active (出現中)";
            ImVec4 stateColor = ImVec4(0.2f, 1.0f, 0.4f, 1.0f);
            if (state == GrowthCube::State::Collecting) {
                stateStr = "Collecting (取得演出中)";
                stateColor = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
            } else if (state == GrowthCube::State::Inactive) {
                stateStr = "Inactive (取得済み)";
                stateColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
            }

            ImGui::Text("Cube [%zu]: %s", i, stateStr);
            ImGui::SameLine();
            std::string respawnBtnId = "Respawn##" + std::to_string(i);
            if (ImGui::SmallButton(respawnBtnId.c_str()))
            {
                growthCubes_[i]->Respawn();
            }
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
        ImGui::SliderFloat("Tilt Smooth Time (傾斜スムーズ時間)", &tiltSmoothTime_, 0.05f, 1.00f, "%.2f s");
        ImGui::SliderFloat("Stage Shake Intensity (揺れ強度)", &stageShakeIntensity_, 0.005f, 0.10f, "%.3f rad");
        ImGui::SliderFloat("Stage Shake Duration (揺れ持続時間)", &stageShakeDuration_, 0.10f, 0.60f, "%.2f s");
        ImGui::SliderFloat("Ground Scale (地面縮小スケール)", &groundScale_, 0.05f, 1.0f, "%.2f");

        float friction = SlimePhysics::GetFriction();
        if (ImGui::SliderFloat("Slime Friction (共通摩擦係数: 1.3)", &friction, 0.2f, 5.0f, "%.1f")) {
            SlimePhysics::SetFriction(friction);
        }
    }

    ImGui::Separator();

    // 5. スライム見た目調整
    if (slimeManager_ && !slimeManager_->GetSlimes().empty())
    {
        // 先頭スライムのパラメータを参照・調整し、全スライムへ反映可能
        auto& firstSlimeParams = slimeManager_->GetSlimes().front()->GetSlimeParams();
        if (ImGui::CollapsingHeader("Slime Jelly & Wobble Params (ぷるぷる弾性調整)", ImGuiTreeNodeFlags_DefaultOpen))
        {
            float wobbleStr = firstSlimeParams.wobbleStrength;
            if (ImGui::SliderFloat("Wobble Strength (表面ぷるぷる強度)", &wobbleStr, 0.0f, 0.5f, "%.3f")) {
                for (auto& s : slimeManager_->GetSlimes()) if (s) s->GetSlimeParams().wobbleStrength = wobbleStr;
            }
            float wobbleFreq = firstSlimeParams.wobbleFrequency;
            if (ImGui::SliderFloat("Wobble Frequency (揺れ周波数)", &wobbleFreq, 1.0f, 15.0f, "%.1f")) {
                for (auto& s : slimeManager_->GetSlimes()) if (s) s->GetSlimeParams().wobbleFrequency = wobbleFreq;
            }
            float fresnel = firstSlimeParams.fresnelPower;
            if (ImGui::SliderFloat("Fresnel Power (エッジ発光)", &fresnel, 0.5f, 6.0f, "%.1f")) {
                for (auto& s : slimeManager_->GetSlimes()) if (s) s->GetSlimeParams().fresnelPower = fresnel;
            }
            float envRef = firstSlimeParams.envReflection;
            if (ImGui::SliderFloat("Env Reflection (環境反射)", &envRef, 0.0f, 1.0f, "%.2f")) {
                for (auto& s : slimeManager_->GetSlimes()) if (s) s->GetSlimeParams().envReflection = envRef;
            }
            float glow = firstSlimeParams.innerGlow;
            if (ImGui::SliderFloat("Inner Glow (内側グロー)", &glow, 0.0f, 1.0f, "%.2f")) {
                for (auto& s : slimeManager_->GetSlimes()) if (s) s->GetSlimeParams().innerGlow = glow;
            }
            float shininess = firstSlimeParams.specularShininess;
            if (ImGui::SliderFloat("Shininess (ハイライト光沢)", &shininess, 8.0f, 128.0f, "%.0f")) {
                for (auto& s : slimeManager_->GetSlimes()) if (s) s->GetSlimeParams().specularShininess = shininess;
            }

            float color[4] = { firstSlimeParams.baseColor.x, firstSlimeParams.baseColor.y, firstSlimeParams.baseColor.z, firstSlimeParams.baseColor.w };
            if (ImGui::ColorEdit4("Slime Color", color))
            {
                for (auto& s : slimeManager_->GetSlimes()) if (s) s->GetSlimeParams().baseColor = { color[0], color[1], color[2], color[3] };
            }

            if (ImGui::Button("Trigger Impulse Ripple (衝撃波紋テスト)"))
            {
                for (auto& s : slimeManager_->GetSlimes()) if (s) s->GetSlimeParams().impulseStrength = 0.5f;
            }
        }
    }

    ImGui::Separator();

    // 6. ステージパーツ（地形モデル群）の状態表示
    if (!stageParts_.empty() && ImGui::CollapsingHeader("Stage Parts (ステージ地形パーツ)", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "Configured Stage Parts: %zu models", stageParts_.size());
        for (size_t i = 0; i < stageParts_.size(); ++i)
        {
            const auto& part = stageParts_[i];
            ImGui::BulletText("[%zu] %s (Offset: %.1f, %.1f, %.1f)",
                i, part.name.c_str(), part.baseOffset.x, part.baseOffset.y, part.baseOffset.z);
        }
    }

    // 7. プロペラ障害物のデバッグ調整
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
        ImGui::SliderFloat("Tilt Smooth Time (ステージ傾斜スムーズ時間: 板の重厚感)", &tiltSmoothTime_, 0.05f, 1.00f, "%.2f s");
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
        ImGui::SliderFloat("Spread Zoom Rate (広がりズーム倍率)", &cameraSpreadZoom_, 0.0f, 0.40f, "%.2f");
        ImGui::SliderFloat("Max Spread Offset (広がりズーム上限)", &maxSpreadOffset_, 0.0f, 8.0f, "%.1f m");
        ImGui::SliderFloat("Max Camera Dist (最大カメラ距離ガード)", &maxCameraDist_, 18.0f, 50.0f, "%.1f m");

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
            tiltSmoothTime_ = 0.35f;
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
            tiltSmoothTime_ = 0.35f;
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
            tiltSmoothTime_ = 0.40f;
            cameraDynamicBank_ = 0.030f;
            cameraDynamicZoom_ = 4.0f;
        }
    }


    ImGui::Separator();

    // 4.5 敵のデバッグパネル
    if (enemyManager_)
    {
        enemyManager_->DrawImGui();
    }

    // 4.6 コインのデバッグパネル
    if (coinManager_)
    {
        coinManager_->DrawImGui();
    }

    ImGui::Separator();

    // 6. シーン遷移
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

    // 配置エディタのウィンドウは別ウィンドウで出す
    if (placementEditor_)
    {
        placementEditor_->DrawImGui();
    }
#endif
}
