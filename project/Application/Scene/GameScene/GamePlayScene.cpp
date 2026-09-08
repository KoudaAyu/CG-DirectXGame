#include "GamePlayScene.h"
#include "SceneManager.h"
#include "DirectXCom.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3dCom.h"
#include "Baziru3_Engine/Framework/Collision/CollisionManager.h"
#include "TextureManager.h"
#include "RenderContext.h"
#include "Application/GameObject/SlimePhysics.h"
#include "Application/Editor/StageLayout.h"
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

    currentCameraDist_ = cameraDistance_;
    cameraDistVelocity_ = 0.0f;
    currentGroupSpread_ = 0.0f;
    groupSpreadVelocity_ = 0.0f;
    currentFocusPos_ = { 0.0f, 0.5f, 0.0f };
    focusPosVelocity_ = { 0.0f, 0.0f, 0.0f };

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

    // 3. 地面モデル（startLand.obj / startLand.mtl）の読み込みと初期化
    groundModelData_ = Object3d::LoadObjFile("Resources/10days", "startLand.obj");

    // 広大な地形メッシュのバウンディング半径を十分大きく設定（視錐台誤カリングを完全に防止）
    groundModelData_.boundingRadius = 10000.0f;

    if (!groundModelData_.material.textureFilePath.empty())
    {
        groundTextureIndex_ = TextureManager::GetInstance()->Load(groundModelData_.material.textureFilePath);
        groundModelData_.material.textureIndex = groundTextureIndex_;
    }
    else
    {
        groundTextureIndex_ = TextureManager::GetInstance()->Load("Resources/10days/checkerBoard.png");
        groundModelData_.material.textureIndex = groundTextureIndex_;
    }

    groundPlane_ = std::make_unique<Object3d>();
    if (groundPlane_) {
        groundPlane_->Initialize(object3dCom, groundModelData_);
        // 地面モデルをカメラと完全同期させ、描画・WVP変換行列と当たり判定のズレを完全に解消
        groundPlane_->SetCamera(playCamera_.get());
        groundPlane_->SetTranslate({ 0.0f, 0.0f, 0.0f });
        groundPlane_->SetScale({ groundScale_, groundScale_, groundScale_ });
        groundPlane_->SetRotate({ 0.0f, 0.0f, 0.0f });
        groundPlane_->SetColor(groundBaseColor_); // 鮮やかな草原カラー
        groundPlane_->SetEnableLighting(true);
        groundPlane_->Update();
    }

    // 地面メッシュコライダーの生成・登録と SlimePhysics への地形メッシュ登録
    groundCollider_ = std::make_unique<MeshCollider>(groundPlane_.get(), CollisionAttribute::Obstacle);
    CollisionManager::GetInstance()->RegisterCollider(groundCollider_.get());
    SlimePhysics::SetGroundMesh(groundPlane_.get(), groundCollider_.get());

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

    // 8. スライム同士（Minion-Minion, Player-Minion）はアプリ層の Multi-Sphere で高精度に処理するため、
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
        refs.player = player_.get();
        placementEditor_->Initialize(refs);
        placementEditor_->SetGroundBaseColor(groundBaseColor_);
    }

    {
        StageLayout layout;
        if (!layout.LoadFromFile(StageLayout::kDefaultPath))
        {
            // JSON が無い／壊れている。座標をハードコードすると地形差し替えで
            // 全部おかしくなるので、地形を実際にレイキャストして置ける場所を探す
            layout = StageLayout::MakeFallback();
        }
        placementEditor_->SetLayout(layout);
    }

    isInitialized_ = true;
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
    SlimePhysics::ClearGroundMesh();
    if (groundCollider_)
    {
        CollisionManager::GetInstance()->UnregisterCollider(groundCollider_.get());
        groundCollider_.reset();
    }
    groundPlane_.reset();
    minionManager_.reset();
    player_.reset();
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

    Vector3 playerPos = player_ ? player_->GetPosition() : Vector3{ 0.0f, 0.0f, 0.0f };
    Vector2 playerPivot = { playerPos.x, playerPos.z };

    // 地面プレーンの回転を傾斜角に合わせて更新
    // プレイヤー足元を回転中心（ピボット）にすることで、傾斜時にスライム直下の
    // 地面高さが変動しなくなり、めり込み・追従ズレを根本から解消
    if (groundPlane_)
    {
        Vector3 rot = { currentTilt_.x, 0.0f, -currentTilt_.y };

        // ピボット = プレイヤーの XZ 位置（地面上の点）
        float px = playerPos.x;
        float pz = playerPos.z;

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
            0.0f - pry,
            pz - prz
        };

        groundPlane_->SetTranslate(groundTranslate);
        groundPlane_->SetScale({ groundScale_, groundScale_, groundScale_ });
        groundPlane_->SetRotate(rot);
        groundPlane_->Update();

        if (groundCollider_)
        {
            // コライダーは groundPlane_ と同一 Object3d を参照するため
            // translate の上書きは行わない（ピボット回転の補正を維持）
            groundCollider_->Update();
        }
    }

    // 照準ガイドはLocoRoco完全準拠のため無効化
    // if (aimGuide_ && player_ && playCamera_) ...

    // プレイヤーの更新（ステージ傾斜を伝達）
    if (player_ && isEditMode_)
    {
        // 配置エディタ中は入力を一切渡さず、速度も毎フレーム殺してその場に留める。
        // Update 自体は呼ぶので、地面追従とスライムシェーダーの時間だけは進む
        player_->SetVelocity({ 0.0f, 0.0f, 0.0f });
        player_->Update(deltaTime, nullptr, nullptr, nullptr, nullptr, { 0.0f, 0.0f });
    }
    else if (player_)
    {
        player_->Update(deltaTime, keyInput_.get(), minionManager_.get(), mouseInput_.get(), aimGuide_.get(), currentTilt_);
    }

    // ミニオンマネージャーの更新（ステージ傾斜を伝達）
    // 配置エディタ中はミニオンを止めて画面から消す（配置対象ではないので邪魔になる）
    if (minionManager_ && player_ && !isEditMode_)
    {
        auto mergeResult = minionManager_->Update(deltaTime, player_->GetPosition(), player_->IsMerged(),
                                                 player_->GetCurrentScale(), currentTilt_,
                                                 player_->GetSlimeParams().squashStretch,
                                                 player_->GetVelocity(), player_->GetSize());
        if (mergeResult.playerPromoted || mergeResult.newlyMergedCount > 0)
        {
            // TODO(SE): プレイヤーの合体音（ミニオンを吸収した音）をここで鳴らす
        }

        if (mergeResult.playerPromoted)
        {
            player_->SetPosition(mergeResult.promotedPos);
            player_->SetSize(mergeResult.promotedSize);
            player_->GetSlimeParams().impulseStrength = 0.55f;
            player_->GetSlimeParams().squashStretch = { 0.25f, -0.20f, 0.25f };
        }
        else if (mergeResult.newlyMergedCount > 0)
        {
            player_->SetSize(player_->GetSize() + mergeResult.newlyMergedCount);
            player_->GetSlimeParams().impulseStrength = 0.45f;
            player_->GetSlimeParams().squashStretch = { 0.20f, -0.15f, 0.20f };
        }
    }


    // 回転プロペラ障害物の更新（自転とステージ傾斜の追従）
    for (auto& prop : propellerObstacles_)
    {
        if (prop) prop->Update(deltaTime, currentTilt_, playerPivot);
    }

    // プロペラ障害物メッシュとスライム（プレイヤーおよび全ミニオン）の精密メッシュ衝突解決
    for (auto& prop : propellerObstacles_)
    {
        if (!prop) continue;

        // プレイヤーとの精密メッシュ衝突
        if (player_)
        {
            Vector3 pPos = player_->GetPosition();
            Vector3 pVel = player_->GetVelocity();
            float pRadius = player_->GetCurrentScale() * 0.78f;
            Vector3 squash = player_->GetSlimeParams().squashStretch;
            float impulse = 0.0f;

            if (prop->ResolveSlimeCollision(pPos, pVel, pRadius, player_->IsMerged(), squash, impulse))
            {
                player_->SetPosition(pPos);
                player_->SetVelocity(pVel);
                player_->GetSlimeParams().squashStretch = squash;
                player_->GetSlimeParams().impulseStrength = (std::max)(player_->GetSlimeParams().impulseStrength, impulse);
            }
        }

        // 各ミニオンとの精密メッシュ衝突
        if (minionManager_)
        {
            for (auto& minion : minionManager_->GetMinions())
            {
                if (!minion || !minion->IsActive()) continue;

                Vector3 mPos = minion->GetPosition();
                Vector3 mVel = minion->GetVelocity();
                float mRadius = minion->GetRadius();
                Vector3 squash = minion->GetSlimeParams().squashStretch;
                float impulse = 0.0f;

                if (prop->ResolveSlimeCollision(mPos, mVel, mRadius, false, squash, impulse))
                {
                    minion->SetPosition(mPos);
                    minion->SetVelocity(mVel);
                    minion->SetState(MinionState::Thrown);
                    minion->GetSlimeParams().squashStretch = squash;
                    minion->GetSlimeParams().impulseStrength = (std::max)(minion->GetSlimeParams().impulseStrength, impulse);
                }
            }
        }
    }

    // 敵の更新（プレイヤーの塊との強弱判定・被弾ノックバックもここで解決される）
    if (enemyManager_)
    {
        enemyManager_->Update(deltaTime, currentTilt_, player_.get(),
                              isEditMode_ ? nullptr : minionManager_.get());
    }

    // コインの更新（地面追従と取得判定。エディタ中は取得しない）
    if (coinManager_)
    {
        coinManager_->Update(deltaTime, currentTilt_, player_.get());
    }

    // 衝突判定と押し出しの更新
    CollisionManager::GetInstance()->Update();

    // 配置エディタ中はカメラを真上からの見下ろしに乗っ取る
    if (isEditMode_ && placementEditor_)
    {
        placementEditor_->Update(deltaTime);
    }

    // カメラの群れ重心追従 (LocoRoco方式: 全ロコロコの重心と広がりを捉える)
    if (!isEditMode_ && playCamera_ && player_)
    {
        Vector3 rawFocusPos = player_->GetPosition();
        float rawSpread = 0.0f;
        if (minionManager_)
        {
            minionManager_->GetGroupCenterAndSpread(player_->GetPosition(), rawFocusPos, rawSpread);
        }

        // 注視点の高さ Y: プレイヤーと群れの自然な高さを追従
        rawFocusPos.y = (std::max)(player_->GetPosition().y + 0.3f, rawFocusPos.y);

        Vector3 playerVel = player_->GetVelocity();

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
            // 注視点中心のスムーズ補間（ミニオン合体による重心の瞬間ジャンプを防止）
            currentFocusPos_.x = SmoothDamp(currentFocusPos_.x, rawFocusPos.x, focusPosVelocity_.x, focusSmoothTime_, deltaTime);
            currentFocusPos_.y = SmoothDamp(currentFocusPos_.y, rawFocusPos.y, focusPosVelocity_.y, focusSmoothTime_, deltaTime);
            currentFocusPos_.z = SmoothDamp(currentFocusPos_.z, rawFocusPos.z, focusPosVelocity_.z, focusSmoothTime_, deltaTime);

            // 群れの広がりのスムーズ補間（合体でミニオンが消えたときの急激なズームインを完全に緩和）
            currentGroupSpread_ = SmoothDamp(currentGroupSpread_, rawSpread, groupSpreadVelocity_, groupSpreadSmoothTime_, deltaTime);
        }

        // 1. プレイヤースケールおよび群れの広がり（Spread）に応じた目標カメラ距離
        // 合体時の急激なズーム変化を防止するため、広がりの重みをマイルド化(0.35f)し、
        // かつ最低カメラ距離（18.5f）を下限ガード（ゆったり見晴らせる高さ）
        float scaleOffset = (player_->GetCurrentScale() - 0.4f);
        float targetDist = cameraDistance_ + (std::max)(0.0f, scaleOffset) * cameraDynamicZoom_ + currentGroupSpread_ * 0.35f;
        targetDist = (std::max)(18.5f, targetDist); // 最低距離ガード

        if (!cameraInitialized_)
        {
            currentCameraDist_ = targetDist;
            cameraDistVelocity_ = 0.0f;
        }
        else
        {
            currentCameraDist_ = SmoothDamp(currentCameraDist_, targetDist, cameraDistVelocity_, cameraZoomSmoothTime_, deltaTime);
        }

        float effectiveDist = (std::max)(18.5f, currentCameraDist_);

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
        float groundYAtTargetCam = SlimePhysics::CalculateGroundHeight(targetCamPos.x, targetCamPos.z, currentTilt_, playerPivot);
        float minTargetCamY = groundYAtTargetCam + 9.5f;
        if (targetCamPos.y < minTargetCamY)
        {
            targetCamPos.y = minTargetCamY;
        }

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

        // ★★★ 最終補間後位置に対する絶対安全クリアランスガード ★★★
        // 補間スプリングのオーバーシュートや激しい板の傾きでも、ステージに異様に近づくことを100%遮断（最低地上高8.5m）
        float currentGroundAtCam = SlimePhysics::CalculateGroundHeight(currentCameraPos_.x, currentCameraPos_.z, currentTilt_, playerPivot);
        float absoluteMinCamY = currentGroundAtCam + 8.5f;
        if (currentCameraPos_.y < absoluteMinCamY)
        {
            currentCameraPos_.y = absoluteMinCamY;
            if (cameraPosVelocity_.y < 0.0f) cameraPosVelocity_.y = 0.0f;
        }

        playCamera_->SetTranslate(currentCameraPos_);
        playCamera_->SetRotate(currentCameraRot_);
        playCamera_->SetFovY(cameraFov_);
        playCamera_->Update();
    }

    // カメラシェイク（カメラ本体へオフセットを載せ直す。補間の基準は汚さない）
    UpdateCameraShake(deltaTime);

    // カメラの最新ViewProjection行列に合わせて、地面メッシュのWVP定数バッファを同期更新
    if (groundPlane_)
    {
        groundPlane_->Update();
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

    const Vector3 shakenPos = {
        currentCameraPos_.x + nx * shakeAmplitude_ * amount,
        currentCameraPos_.y + ny * shakeAmplitude_ * amount,
        currentCameraPos_.z + nz * shakeAmplitude_ * amount * 0.5f,
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
    // 残機 ＝ スライムの数 ＝ プレイヤーの塊サイズ（吸収ぶんを含む）
    //                        + フィールドに残っているミニオンの強さの合計
    int life = player_ ? player_->GetSize() : 0;

    if (minionManager_)
    {
        for (const auto& minionPtr : minionManager_->GetMinions())
        {
            const Minion* minion = minionPtr.get();
            if (!minion || !minion->IsActive()) continue; // 吸収された子は非アクティブ
            life += minion->GetSize();
        }
    }
    return life;
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

    const Vector3 playerPos = player_ ? player_->GetPosition() : Vector3{ 0.0f, 0.0f, 0.0f };
    const float playerRadius = player_ ? player_->GetCurrentScale() * 0.78f : 0.5f;
    const Vector4 playerColor = player_ ? player_->GetSlimeParams().baseColor
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

    // --- プレイヤーのイベント（ジャンプ / 自爆）---
    if (player_)
    {
        PikminPlayer::FxEvents ev;
        if (player_->TakeFxEvents(ev))
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
        fx_->UpdateAll(deltaTime, playerPos, player_.get(), minionManager_.get(),
                       coinManager_.get(), enemyManager_.get());
    }

    // --- HUD ---
    if (hud_)
    {
        GamePlaySceneHud::FrameInput frame;
        frame.camera = playCamera_.get();
        frame.player = player_.get();
        frame.minionManager = minionManager_.get();
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

    // 1. 地面の描画
    if (groundPlane_)
    {
        RenderContext groundCtx = ctx;
        if (groundTextureIndex_ != TextureManager::kInvalidTextureIndex) {
            groundCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(groundTextureIndex_);
        }

        // 配置エディタ中は、上段（一本道）越しに下段が見えるよう半透明で描く。
        // 地形は1メッシュなので上段だけを透かすことはできず、全体が薄くなる
        bool drawnTranslucent = false;
        if (isEditMode_ && placementEditor_)
        {
            drawnTranslucent = placementEditor_->DrawGroundTranslucent(groundCtx, groundPlane_.get(), groundModelData_);
        }
        if (!drawnTranslucent)
        {
            object3dCom->Draw(groundPlane_.get(), groundCtx, groundModelData_, true);
        }
    }

    // 2. 回転プロペラ障害物の描画
    for (auto& prop : propellerObstacles_)
    {
        if (prop) prop->Draw(ctx);
    }

    // 3. 放物線照準ガイドの描画 (LocoRoco完全準拠のため非表示)
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

    // 5. ミニオン群衆の描画（配置エディタ中は非表示）
    if (minionManager_ && !isEditMode_)
    {
        minionManager_->Draw(ctx);
    }

    // 6. プレイヤーの描画
    if (player_)
    {
        player_->Draw(ctx);
    }

    // 7. パーティクル演出
    //    PSO はデプステスト有・書き込み無なので、デプスを書くものを全部描いたあとに
    if (fx_)
    {
        fx_->Draw(ctx.commandList);
    }

    // 8. 配置エディタのオーバーレイ（配置禁止領域・選択マーカー・カーソル）
    //    デプス書き込みを切ってあるので、3D の中では一番最後に描く
    if (isEditMode_ && placementEditor_)
    {
        placementEditor_->Draw(ctx);
    }

    // 9. HUD（2D）
    //    Sprite の PSO はデプス無効なので、最後に描けば必ず手前に来る
    if (hud_)
    {
        hud_->Draw(ctx.commandList);
    }
}

void GamePlayScene::DrawDebugUI()
{
    // コライダーのデバッグワイヤーフレーム描画（エンジン標準の MeshCollider ワイヤーフレーム描画）
    if (playCamera_)
    {
        CollisionManager::GetInstance()->DrawDebug(playCamera_.get());
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
    ImGui::BulletText("E key: Split (弾けて全員小ロコロコに分裂)");
    ImGui::BulletText("Contact: Auto Merge (触れ合うとポコッと自動合体)");
    ImGui::BulletText("F1 key: Toggle Collision Wireframes (当たり判定表示ON/OFF)");
    ImGui::BulletText("F3 key: Toggle All ImGui (全ImGui表示/非表示)");
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
        int maxMinionSize = minionManager_->GetMaxMinionSize();
        const char* tierLabel = "小 (1-2) [青]";
        ImVec4 tierColor = ImVec4(0.35f, 0.70f, 1.0f, 1.0f);
        if (currentSize >= 8) {
            tierLabel = "大 (8-10) [赤]";
            tierColor = ImVec4(1.0f, 0.35f, 0.3f, 1.0f);
        } else if (currentSize >= 3) {
            tierLabel = "中 (3-7) [黄色]";
            tierColor = ImVec4(1.0f, 0.90f, 0.2f, 1.0f);
        }

        ImGui::TextColored(tierColor, "Main Loco Size: %d / %d  Category: %s  (Scale: %.2f)",
                           currentSize, totalCount + 1, tierLabel, player_->GetCurrentScale());
        if (activeCount > 0) {
            ImGui::TextColored(ImVec4(0.5f, 0.85f, 1.0f, 1.0f), "Friends in Field: %d active | Max Friend Size: %d",
                               activeCount, maxMinionSize);
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "All Friends Merged into One!");
        }

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
        ImGui::SliderFloat("Tilt Smooth Time (傾斜スムーズ時間)", &tiltSmoothTime_, 0.05f, 1.00f, "%.2f s");
        ImGui::SliderFloat("Ground Scale (地面縮小スケール)", &groundScale_, 0.05f, 1.0f, "%.2f");

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

    // 配置エディタのウィンドウは別ウィンドウで出す
    if (placementEditor_)
    {
        placementEditor_->DrawImGui();
    }
#endif
}
