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

    // 3. 地形メッシュ群のコンテナ。中身は配置データ（JSON）から流し込む。
    //    以前はここに startLand / Land1 / toLandRoad / roadCell×6 が
    //    ハードコードされていたが、配置エディタで編集できるよう JSON へ移した。
    //    JSON に terrain が無い場合は StageLayout::MakeDefaultTerrain() が
    //    その旧ハードコードと同じ配置を返す
    SlimePhysics::ClearGroundMeshes();
    stageTerrain_ = std::make_unique<StageTerrain>();
    stageTerrain_->Initialize(object3dCom, playCamera_.get());
    stageTerrain_->SetBaseColor(groundBaseColor_);

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

    // 10. コイン・成長キューブのマネージャー初期化
    coinManager_ = std::make_unique<CoinManager>();
    coinManager_->Initialize(object3dCom, playCamera_.get());

    growthCubeManager_ = std::make_unique<GrowthCubeManager>();
    growthCubeManager_->Initialize(object3dCom, playCamera_.get());

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

    // 11.5 ボス戦フェーズ。ボス本体・弾・HPバー・カメラ演出はここが持つ。
    //      配置（座標・最大HP）は配置データから入る
    //      Initialize() はスライム群を作ったあと（下の配置データのブロック）でまとめて呼ぶ
    bossFight_ = std::make_unique<BossFight>();
    bossFreezeSlimes_ = false;

    // 12. 配置エディタの初期化と、配置データ（JSON）の読み込み
    //     SpawnDebugSet() による仮スポーンは廃止。配置は全部 JSON から復元する
    //     Initialize() はスライム群を作ったあと（すぐ下）でまとめて呼ぶ
    placementEditor_ = std::make_unique<PlacementEditor>();

    {
        StageLayout layout;
        const bool loaded = layout.LoadFromFile(StageLayout::kDefaultPath);
        if (!loaded)
        {
            OutputDebugStringA("[PlacementEditor] stage_layout not found. Using generated fallback layout.\n");
        }

        // 地形が空（＝旧フォーマットの JSON、またはファイルが無い）なら既定配置。
        // 中身は以前 InitializeScene にハードコードされていたものと同じ
        if (layout.terrain.empty())
        {
            layout.terrain = StageLayout::MakeDefaultTerrain();
        }

        // 【順番が重要】敵・コイン・キューブの検証もフォールバック生成も
        // 「地形へのレイキャスト」に依存しているので、地形を先に適用しておく
        stageTerrain_->ApplyLayout(layout.terrain);

        // 地形を差し替えたあとの JSON は座標がそのまま残っているので、
        // 島の外を指していないか必ず検証する。ここを通さないと
        // 敵とコインが全部宙に浮いて奈落へ落ちる（実際に startLand で起きた）
        float validRatio = 0.0f;
        if (!loaded || !layout.IsCompatibleWithCurrentTerrain(&validRatio))
        {
            if (loaded)
            {
                char msg[192];
                std::snprintf(msg, sizeof(msg),
                              "[PlacementEditor] stage_layout does not fit the current terrain "
                              "(only %.0f%% on ground). Using generated fallback layout.\n",
                              validRatio * 100.0f);
                OutputDebugStringA(msg);
            }

            // 地形とボスの設定だけは残す。フォールバックが作り直すのは
            // 敵・コイン・キューブ・プレイヤー初期位置だけ
            std::vector<StageTerrainEntry> keptTerrain = layout.terrain;
            StageBossEntry keptBoss = layout.boss;

            layout = StageLayout::MakeFallback();
            layout.terrain = std::move(keptTerrain);
            if (keptBoss.enabled) layout.boss = keptBoss;
        }

        // 4. スライムマネージャーの初期化と初期スライム群の配置。
        //    地形が登録されたあとでないと SpawnSlime() の地形スナップが効かない
        slimeManager_ = std::make_unique<SlimeManager>();
        slimeManager_->Initialize(object3dCom, playCamera_.get());

        // プレイヤー初期位置は配置データが正。床がある場所なら採用する
        if (SlimePhysics::QueryGroundLayers(layout.playerStart.x, layout.playerStart.z, nullptr, 0) > 0)
        {
            spawnBasePos_ = { layout.playerStart.x, layout.playerStart.y + 0.55f, layout.playerStart.z };
        }
        RespawnSlimesAtBase();

        // エディタとボスの初期化。ここまで来て初めて全部の参照先が揃う
        {
            PlacementEditor::SceneRefs refs;
            refs.object3dCom = object3dCom;
            refs.camera = playCamera_.get();
            refs.enemyManager = enemyManager_.get();
            refs.coinManager = coinManager_.get();
            refs.growthCubeManager = growthCubeManager_.get();
            refs.slimeManager = slimeManager_.get();
            refs.terrain = stageTerrain_.get();
            refs.bossFight = bossFight_.get();
            placementEditor_->Initialize(refs);
            placementEditor_->SetGroundBaseColor(groundBaseColor_);
        }
        {
            BossFight::SceneRefs refs;
            refs.object3dCom = object3dCom;
            refs.camera = playCamera_.get();
            refs.slimeManager = slimeManager_.get();
            refs.terrain = stageTerrain_.get();
            refs.fx = fx_.get();
            bossFight_->Initialize(refs);
        }

        placementEditor_->SetLayout(layout);
    }

    if (dxCommon_)
    {
        IrisTransition::GetInstance()->Initialize(dxCommon_);
    }
    isGameOverTransition_ = false;
    gameOverDelayTimer_ = 0.0f;

    // 成長キューブは配置データ（JSON の growthCubes）から
    // PlacementEditor::ApplyLayoutToScene() が置く。ここでのハードコードは廃止した

    isInitialized_ = true;
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
    if (growthCubeManager_) growthCubeManager_->RespawnAll();

    // ボス戦を最初の状態（トリガー待ち）へ戻す
    if (bossFight_) bossFight_->Restart();
    bossFreezeSlimes_ = false;

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
            // 【注意】ここで SetActive(false) を呼んではいけない。
            // あちらは ApplyLayoutToScene()（地形の再読み込みや Object3d の生成・破棄）と
            // Camera::Update() まで走らせるが、終了時は DirectXCom / CB アロケータの
            // 解放順が読めず、GPU を触った瞬間にアクセス違反になる
            //（engine-notes.md の「Finalize() で GPU を触ると落ちる」パターン）。
            // 保存に必要なのは placementEditor_ が持っている配置データだけなので、
            // シーンから吸い出して書き出すところまでで止める
            placementEditor_->SyncLayoutFromScene();
            placementEditor_->Save();
            isEditMode_ = false;
        }
        else if (placementEditor_->IsDirty())
        {
            placementEditor_->Save();
        }
        placementEditor_->Finalize();
        placementEditor_.reset();
    }

    if (bossFight_)
    {
        bossFight_->Finalize();
        bossFight_.reset();
    }

    if (growthCubeManager_)
    {
        growthCubeManager_->Finalize();
        growthCubeManager_.reset();
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

    // 地形の解除（SlimePhysics と CollisionManager からの登録解除もここでやる）
    if (stageTerrain_)
    {
        stageTerrain_->Finalize();
        stageTerrain_.reset();
    }
    SlimePhysics::ClearGroundMeshes();
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

#if defined(_DEBUG) || defined(USE_IMGUI)
        // Rキーで再スタート（初期配置でスライムを再生成、ステージ傾斜・カメラを初期化、デバッグ専用）
        if (keyInput_->TriggerKey(DIK_R))
        {
            RestartGame();
        }

        // F2キーでプレイ <-> 配置エディタ を切り替え（デバッグ専用）
        if (keyInput_->TriggerKey(DIK_F2))
        {
            SetEditMode(!isEditMode_);
        }

        // ENTERキーでクリアシーンへ強制遷移（デバッグスキップ用）
        // 配置エディタ中は誤爆を避けるため無効
        if (!isEditMode_ && keyInput_->TriggerKey(DIK_RETURN))
        {
            // リザルトへ値を渡す。ClearScene が同じキーを読む
            SetSceneDataInt("result.score", score_);
            SetSceneDataFloat("result.time", elapsedSeconds_);
            SetSceneDataInt("result.coin", coinManager_ ? coinManager_->GetCollectedCount() : 0);

            SceneManager::GetInstance()->ChangeScene("CLEAR");
        }

        // F1キーで当たり判定ワイヤーフレーム表示/非表示をトグル（デバッグ専用）
        if (keyInput_->TriggerKey(DIK_F1))
        {
            bool showColliders = CollisionManager::GetInstance()->IsShowDebugColliders();
            CollisionManager::GetInstance()->SetShowDebugColliders(!showColliders);
        }

        // F4 または C キーでデバッグカメラをトグル（デバッグ専用）
        if (keyInput_->TriggerKey(DIK_F4) || keyInput_->TriggerKey(DIK_C))
        {
            isDebugCamera_ = !isDebugCamera_;
            if (isDebugCamera_ && playCamera_)
            {
                debugCameraPos_ = playCamera_->GetTranslate();
                debugCameraRot_ = playCamera_->GetRotate();
            }
        }
#endif
    }

    if (mouseInput_)
    {
        mouseInput_->Update();
    }

    // --- ステージ傾斜（ティルト）の入力とスムーズ補間 ---
    targetTilt_ = { 0.0f, 0.0f };
    if (keyInput_ && !isEditMode_ && !isDebugCamera_)
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

    // 全ステージパーツの回転を傾斜角＋揺動に合わせて更新。
    // スライム群衆重心を回転中心（ピボット）にすることで、傾斜時にスライム直下の
    // 地面高さが変動しなくなり、めり込み・追従ズレを根本から解消する。
    //
    // 実際の行列計算（ピボット回転 + パーツごとの位置・Y回転・スケール）は
    // StageTerrain::UpdateTransforms() の中。以前はここに直接書いてあった
    if (stageTerrain_)
    {
        stageTerrain_->UpdateTransforms(currentTilt_, { slimeCenter.x, slimeCenter.z },
                                        stageBounceOffset_, shakeTilt);
    }

    // 照準ガイドはLocoRoco完全準拠のため無効化
    // if (aimGuide_ && slimeManager_ && playCamera_) ...

    // スライム群衆の更新（全スライムの入力、物理、合体、分裂、衝突分離）
    // 配置エディタ中は入力を一切渡さず、速度も毎フレーム殺してその場に留める。
    // Update 自体は呼ぶので、地面追従とスライムシェーダーの時間だけは進む
    // ボスの登場フォーカス・死亡演出の間も同じ扱いで固める
    //（bossFreezeSlimes_ は前フレームの BossFight::Update() が立てたもの）
    if (slimeManager_ && (isEditMode_ || bossFreezeSlimes_))
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

    // 成長キューブの更新（浮遊、自転、ステージ傾斜追従、スライム当たり判定、巨大化）
    if (growthCubeManager_)
    {
        growthCubeManager_->Update(deltaTime, currentTilt_, slimePivot, slimeManager_.get());
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

    // ボス戦フェーズの更新。中身は BossFight.cpp
    // （トリガー判定・カメラ演出・全方向弾・HPバー・死亡演出まで全部あちら）
    if (bossFight_)
    {
        BossFight::FrameInput bossInput;
        bossInput.deltaTime = deltaTime;
        bossInput.stageTilt = currentTilt_;
        bossInput.pivot = slimePivot;
        bossInput.playerLife = CalculateLifeCount();
        bossInput.editorMode = isEditMode_;

        const BossFight::FrameResult bossResult = bossFight_->Update(bossInput);

        bossFreezeSlimes_ = bossResult.freezeSlimes;
        if (bossResult.freezeSlimes && slimeManager_)
        {
            // 立った瞬間から効かせたいので、その場で速度も殺しておく
            for (const auto& slimePtr : slimeManager_->GetSlimes())
            {
                if (slimePtr) slimePtr->SetVelocity({ 0.0f, 0.0f, 0.0f });
            }
        }

        if (bossResult.cameraShake > 0.0f)
        {
            AddCameraShake(bossResult.cameraShake);
        }

        if (bossResult.scoreGain > 0)
        {
            // スコアの増分は「残機の三乗」。計算は BossFight 側でやっている
            score_ += bossResult.scoreGain;
            if (hud_)
            {
                hud_->PushScorePopup(bossResult.scoreGain, bossResult.scorePopupAt);
            }
        }

        if (bossResult.requestClear)
        {
            // リザルトへ値を渡す。ClearScene が同じキーを読む
            SetSceneDataInt("result.score", score_);
            SetSceneDataFloat("result.time", elapsedSeconds_);
            SetSceneDataInt("result.coin", coinManager_ ? coinManager_->GetCollectedCount() : 0);

            // フェード付きの予約遷移なので、この時点で this が消えることはない。
            // それでも「ボスを倒したあとの残りの処理」は意味が無いので抜ける
            SceneManager::GetInstance()->ChangeScene("CLEAR");
            return;
        }
    }

    // 衝突判定と押し出しの更新
    CollisionManager::GetInstance()->Update();

#if defined(_DEBUG) || defined(USE_IMGUI)
    // 配置エディタ中はカメラを真上からの見下ろしに乗っ取る（デバッグ専用）
    if (isEditMode_ && placementEditor_)
    {
        placementEditor_->Update(deltaTime);
    }

    // デバッグカメラまたは通常カメラの群れ重心追従
    if (isDebugCamera_)
    {
        UpdateDebugCamera(deltaTime);
    }
    else
#endif
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
        Vector3 finalCamRot = currentCameraRot_;

        // --- ボス戦のカメラ演出 ---
        // BossFight が「ボスの正面から見た位置・回転」と 0..1 の重みを返してくるので、
        // 通常のカメラとの間を補間する。
        // 【重要】currentCameraPos_ / currentCameraRot_ 自体は汚さない。
        // 汚すと次フレームの SmoothDamp の基準がずれて、戻るときにカメラが跳ねる
        if (bossFight_ && bossFight_->GetCameraBlend() > 0.0001f)
        {
            Vector3 focusPos, focusRot;
            if (bossFight_->GetCameraTarget(focusPos, focusRot))
            {
                const float t = std::clamp(bossFight_->GetCameraBlend(), 0.0f, 1.0f);

                finalCamPos.x += (focusPos.x - finalCamPos.x) * t;
                finalCamPos.y += (focusPos.y - finalCamPos.y) * t;
                finalCamPos.z += (focusPos.z - finalCamPos.z) * t;

                // 角度は最短回り（-pi..pi）で補間しないと、境目で1周してしまう
                auto LerpAngle = [](float from, float to, float rate) {
                    constexpr float kPi = 3.14159265358979323846f;
                    float diff = to - from;
                    while (diff < -kPi) diff += kPi * 2.0f;
                    while (diff > kPi)  diff -= kPi * 2.0f;
                    return from + diff * rate;
                };
                finalCamRot.x = LerpAngle(finalCamRot.x, focusRot.x, t);
                finalCamRot.y = LerpAngle(finalCamRot.y, focusRot.y, t);
                finalCamRot.z = LerpAngle(finalCamRot.z, focusRot.z, t);
            }
        }

        // カメラシェイクはこの値を土台に足す
        appliedCameraPos_ = finalCamPos;
        appliedCameraRot_ = finalCamRot;

        playCamera_->SetTranslate(finalCamPos);
        playCamera_->SetRotate(finalCamRot);
        playCamera_->SetFovY(cameraFov_);
        playCamera_->Update();

        // カメラシェイク（カメラ本体へオフセットを載せ直す。補間の基準は汚さない）
        UpdateCameraShake(deltaTime);
    }

    // カメラの最新ViewProjection行列に合わせて、各ステージパーツのWVP定数バッファを同期更新
    if (stageTerrain_)
    {
        stageTerrain_->SyncConstantBuffers();
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

    // 土台は appliedCameraPos_ / appliedCameraRot_（ステージ揺らしの
    // cameraShakeOffset_ と、ボス戦のフォーカス補間まで込みの最終値）。
    // currentCameraPos_ を土台にすると、その2つを打ち消してしまう
    const Vector3 shakenPos = {
        appliedCameraPos_.x + nx * shakeAmplitude_ * amount,
        appliedCameraPos_.y + ny * shakeAmplitude_ * amount,
        appliedCameraPos_.z + nz * shakeAmplitude_ * amount * 0.5f,
    };
    const Vector3 shakenRot = {
        appliedCameraRot_.x + ny * shakeRollAmount_ * amount * 0.4f,
        appliedCameraRot_.y + nx * shakeRollAmount_ * amount * 0.4f,
        appliedCameraRot_.z + nz * shakeRollAmount_ * amount,
    };

    playCamera_->SetTranslate(shakenPos);
    playCamera_->SetRotate(shakenRot);
    playCamera_->Update();
}

#if defined(_DEBUG) || defined(USE_IMGUI)
void GamePlayScene::UpdateDebugCamera(float deltaTime)
{
    if (!playCamera_ || !keyInput_) return;

    // --- 移動速度 ---
    float speed = debugCameraSpeed_;
    // Shiftキーで高速ブースト（3倍速）
    if (keyInput_->PushKey(DIK_LSHIFT) || keyInput_->PushKey(DIK_RSHIFT))
    {
        speed *= 3.0f;
    }
    // Altキーでスロー（0.25倍速、精密操作）
    if (keyInput_->PushKey(DIK_LALT) || keyInput_->PushKey(DIK_RALT))
    {
        speed *= 0.25f;
    }

    // --- 視点回転 ---
    bool rotating = false;
#ifdef USE_IMGUI
    ImGuiIO& io = ImGui::GetIO();
    // ImGui のウィンドウ操作中でなければマウス右ドラッグで視点回転
    if (ImGui::IsMouseDown(ImGuiMouseButton_Right) && !io.WantCaptureMouse)
    {
        debugCameraRot_.y += io.MouseDelta.x * debugCameraRotSpeed_;
        debugCameraRot_.x += io.MouseDelta.y * debugCameraRotSpeed_;
        rotating = true;
    }
    // マウスホイールで移動速度をスムーズに加減速
    if (std::abs(io.MouseWheel) > 0.01f && !io.WantCaptureMouse)
    {
        debugCameraSpeed_ += io.MouseWheel * 5.0f;
        debugCameraSpeed_ = std::clamp(debugCameraSpeed_, 2.0f, 250.0f);
    }
#endif

    if (!rotating && mouseInput_)
    {
        if (mouseInput_->PushButton(1)) // 右クリック押下中
        {
            debugCameraRot_.y += static_cast<float>(mouseInput_->GetMoveX()) * debugCameraRotSpeed_;
            debugCameraRot_.x += static_cast<float>(mouseInput_->GetMoveY()) * debugCameraRotSpeed_;
        }
    }

    // 矢印キーによるキーボード視点回転（マウスを使わない場合用）
    float keyRotSpeed = 2.0f * deltaTime;
    if (keyInput_->PushKey(DIK_UP))    debugCameraRot_.x -= keyRotSpeed;
    if (keyInput_->PushKey(DIK_DOWN))  debugCameraRot_.x += keyRotSpeed;
    if (keyInput_->PushKey(DIK_LEFT))  debugCameraRot_.y -= keyRotSpeed;
    if (keyInput_->PushKey(DIK_RIGHT)) debugCameraRot_.y += keyRotSpeed;

    // Pitch の制限（真上・真下を行き過ぎないようにクランプ）
    constexpr float kMaxPitch = 1.55f; // ~88.8度
    debugCameraRot_.x = std::clamp(debugCameraRot_.x, -kMaxPitch, kMaxPitch);

    // Yaw の正規化 (-pi .. pi)
    constexpr float kPi = 3.14159265358979323846f;
    while (debugCameraRot_.y < -kPi) debugCameraRot_.y += kPi * 2.0f;
    while (debugCameraRot_.y > kPi)  debugCameraRot_.y -= kPi * 2.0f;
    debugCameraRot_.z = 0.0f;

    // --- カメラの向きに基づいた移動ベクトルの算出 ---
    float cy = std::cos(debugCameraRot_.y);
    float sy = std::sin(debugCameraRot_.y);
    float cp = std::cos(debugCameraRot_.x);
    float sp = std::sin(debugCameraRot_.x);

    // 視線方向（3D Forward）
    Vector3 forward = { sy * cp, -sp, cy * cp };
    // 水平右方向（Horizontal Right）
    Vector3 right = { cy, 0.0f, -sy };

    Vector3 moveDir{ 0.0f, 0.0f, 0.0f };

    // W / S: 視線方向へ前進 / 後退
    if (keyInput_->PushKey(DIK_W)) { moveDir.x += forward.x; moveDir.y += forward.y; moveDir.z += forward.z; }
    if (keyInput_->PushKey(DIK_S)) { moveDir.x -= forward.x; moveDir.y -= forward.y; moveDir.z -= forward.z; }

    // A / D: 左右へ水平スライド
    if (keyInput_->PushKey(DIK_D)) { moveDir.x += right.x; moveDir.y += right.y; moveDir.z += right.z; }
    if (keyInput_->PushKey(DIK_A)) { moveDir.x -= right.x; moveDir.y -= right.y; moveDir.z -= right.z; }

    // Space / E: 上昇
    if (keyInput_->PushKey(DIK_SPACE) || keyInput_->PushKey(DIK_E)) moveDir.y += 1.0f;

    // Left Ctrl / Q: 下降
    if (keyInput_->PushKey(DIK_LCONTROL) || keyInput_->PushKey(DIK_Q)) moveDir.y -= 1.0f;

    float lenSq = moveDir.x * moveDir.x + moveDir.y * moveDir.y + moveDir.z * moveDir.z;
    if (lenSq > 1e-6f)
    {
        float invLen = 1.0f / std::sqrt(lenSq);
        debugCameraPos_ += moveDir * (invLen * speed * deltaTime);
    }

    // カメラへ適用
    appliedCameraPos_ = debugCameraPos_;
    appliedCameraRot_ = debugCameraRot_;

    playCamera_->SetTranslate(debugCameraPos_);
    playCamera_->SetRotate(debugCameraRot_);
    playCamera_->SetFovY(cameraFov_);
    playCamera_->Update();
}
#endif

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

                // ボスへダメージ。
                // SlimeManager::TakeSelfDestructEvent() は EnemyManager が
                // 1箇所で拾ってしまうので、こちらで拾ったものを回してやる。
                // BossFight は次の Update() でまとめて HP を減らす
                if (bossFight_) bossFight_->NotifySelfDestruct(ev.splitPosition);

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

    // --- 成長キューブ（食べると残機が増えるやつ）---
    if (growthCubeManager_)
    {
        for (const Vector3& position : growthCubeManager_->GetCollectEvents())
        {
            // コインのような光芒が弾ける。縮小してスライムへ吸い込まれる動きは
            // GrowthCube 側が持っていて、そのあいだ光は出続ける
            if (fx_) fx_->EmitGrowthCubeCollect(position, 0.6f);

            // TODO(SE): 成長キューブを食べたときの音をここで鳴らす
        }
    }

    // --- 常時出ている演出 ---
    if (fx_)
    {
        fx_->UpdateAll(deltaTime, playerPos, leader, slimeManager_.get(),
                       coinManager_.get(), enemyManager_.get(), growthCubeManager_.get());
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

    // 1. 地面の描画（配置データから作られた全ステージパーツ）
    if (stageTerrain_)
    {
        for (const auto& partPtr : stageTerrain_->GetParts())
        {
            if (!partPtr || !partPtr->object) continue;
            StageTerrain::Part& part = *partPtr;

            RenderContext partCtx = ctx;
            if (part.textureIndex != TextureManager::kInvalidTextureIndex) {
                partCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(part.textureIndex);
            }

            // 配置エディタ中は、オーバーハング越しに下が見えるようパーツごとに半透明で描く
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

    // 5. 成長キューブの描画（Slime シェーダーでゲーミング色に光る）
    if (growthCubeManager_)
    {
        growthCubeManager_->Draw(ctx);
    }

    // 5.5 ボスとボスの弾
    if (bossFight_)
    {
        bossFight_->Draw(ctx);
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

    // 11. ボスのHPバー（画面下）。ほかの HUD より手前でよい
    if (bossFight_)
    {
        bossFight_->DrawHud(ctx.commandList);
    }
}

void GamePlayScene::DrawDebugUI()
{
#if defined(_DEBUG) || defined(USE_IMGUI)
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

    // --- 演出 / HUD / カメラシェイク / ボス戦 ---
    if (fx_) fx_->DrawImGui();
    if (hud_) hud_->DrawImGui();
    if (bossFight_) bossFight_->DrawImGui();
    if (growthCubeManager_) growthCubeManager_->DrawImGui();

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

    // --- プレイ / 配置エディタ / デバッグカメラ の切り替え ---
    {
        ImGui::SeparatorText("Mode");
        ImGui::Text("Now: %s%s", isEditMode_ ? "EDIT (placement)" : "PLAY",
                    isDebugCamera_ ? " [DEBUG CAMERA ACTIVE]" : "");
        if (ImGui::Button(isEditMode_ ? "Back to Play (F2)" : "Placement Editor (F2)"))
        {
            SetEditMode(!isEditMode_);
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Debug Camera (F4 / C)", &isDebugCamera_))
        {
            if (isDebugCamera_ && playCamera_)
            {
                debugCameraPos_ = playCamera_->GetTranslate();
                debugCameraRot_ = playCamera_->GetRotate();
            }
        }

        if (isDebugCamera_)
        {
            ImGui::Indent();
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Debug Camera Active!");
            ImGui::Text("Pos: (%.1f, %.1f, %.1f) | Pitch: %.1f deg, Yaw: %.1f deg",
                        debugCameraPos_.x, debugCameraPos_.y, debugCameraPos_.z,
                        debugCameraRot_.x * 57.2957795f, debugCameraRot_.y * 57.2957795f);
            ImGui::DragFloat("Cam Speed", &debugCameraSpeed_, 1.0f, 2.0f, 250.0f);
            ImGui::TextDisabled("Controls: WASD=Fly | QE/Space/Ctrl=Up/Down | RightDrag/Arrows=Look | Shift=Boost");
            ImGui::Unindent();
        }
    }

    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "=== [ Pikmin x LocoRoco 3D Prototype ] ===");
    ImGui::Separator();

    // 1. 操作説明
    ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.2f, 1.0f), "[ Controls (LocoRoco 3D) ]");
    ImGui::BulletText("WASD / Arrows: Tilt Stage (ステージを傾けて全員で転がる)");
    ImGui::BulletText("F4 / C key: Toggle Free Debug Camera (自由デバッグカメラON/OFF)");
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

    // 3.5. 成長キューブ（配置は JSON。細かい調整は Growth Cube / Game FX パネル側）
    if (ImGui::CollapsingHeader("Growth Cubes", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (growthCubeManager_)
        {
            ImGui::Text("Placed: %d / Eaten: %d",
                        growthCubeManager_->GetTotalCount(), growthCubeManager_->GetCollectedCount());
            if (ImGui::Button("Respawn All Cubes", ImVec2(240, 28)))
            {
                growthCubeManager_->RespawnAll();
            }
            ImGui::SameLine();
            if (ImGui::Button("Spawn Cube At Focus", ImVec2(220, 28)))
            {
                growthCubeManager_->Spawn({ currentFocusPos_.x, 0.0f, currentFocusPos_.z + 2.0f }, 0.85f);
            }
            ImGui::TextDisabled("Placement is edited in the Placement Editor (F2).");
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
        // 地面のスケールはパーツごとの設定になったので、配置エディタ（F2）の
        // Terrain レイヤーで選択して変える

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
    if (stageTerrain_ && stageTerrain_->GetPartCount() > 0 &&
        ImGui::CollapsingHeader("Stage Parts", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "Terrain parts: %d models",
                           stageTerrain_->GetPartCount());
        const auto& parts = stageTerrain_->GetParts();
        for (size_t i = 0; i < parts.size(); ++i)
        {
            const StageTerrain::Part* part = parts[i].get();
            if (!part) continue;
            ImGui::BulletText("[%zu] %s%s  pos(%.1f, %.1f) rotY %.0fdeg scale %.2f",
                i, part->mesh.c_str(), part->bossTrigger ? " [BOSS TRIGGER]" : "",
                part->position.x, part->position.z,
                part->rotationY * 57.2958f, part->scale);
        }
        ImGui::TextDisabled("Edit terrain in the Placement Editor (F2) > Terrain layer.");
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
#endif // USE_IMGUI
#endif // _DEBUG || USE_IMGUI
}
