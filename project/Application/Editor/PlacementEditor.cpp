#include "Application/Editor/PlacementEditor.h"

#include "Baziru3_Engine/Core/Base/DirectXCom.h"
#include "Baziru3_Engine/Core/Camera/Camera.h"
#include "Baziru3_Engine/Graphics/2D/Texture/TextureManager.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3dCom.h"
#include "Baziru3_Engine/Graphics/Light/Light.h"
#include "Baziru3_Engine/Framework/Scene/Manager/SceneManager.h"
#include "Application/Enemy/EnemyManager.h"
#include "Application/GameObject/CoinManager.h"
#include "Application/GameObject/GrowthCubeManager.h"
#include "Application/GameObject/SlimePhysics.h"
#include "Application/GameObject/SlimeManager.h"
#include "Application/GameObject/StageTerrain.h"
#include "Application/Scene/GameScene/BossFight.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace
{
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kHalfPi = kPi * 0.5f;

    /// 仮想解像度。ImGuiManager がマウス座標をここへ正規化してくれている
    constexpr float kVirtualWidth = 1280.0f;
    constexpr float kVirtualHeight = 720.0f;
    constexpr float kAspect = kVirtualWidth / kVirtualHeight;

    /// 見下ろしカメラの視野角。狭めにして正投影に近い見え方にする
    constexpr float kEditorFovY = 0.60f;

    /// クリックとドラッグの境目（仮想解像度のピクセル）
    constexpr float kDragThreshold = 5.0f;

    const char* kEnemyTypeNames[] = { "Slime", "FlowerClover", "FlowerLotus", "FlowerSunward" };

    /// @brief 平たいリング（ドーナツ）メッシュ。選択・カーソルのマーカーに使う
    Object3d::ModelData GenerateRingMesh(float innerRatio, int segments)
    {
        Object3d::ModelData model;
        segments = (std::clamp)(segments, 8, 128);
        innerRatio = (std::clamp)(innerRatio, 0.05f, 0.95f);

        for (int i = 0; i <= segments; ++i)
        {
            float t = 2.0f * kPi * static_cast<float>(i % segments) / static_cast<float>(segments);
            float c = std::cos(t);
            float s = std::sin(t);

            Sprite::VertexData outer{};
            outer.position = { c, 0.0f, s, 1.0f };
            outer.normal = { 0.0f, 1.0f, 0.0f };
            outer.texcoord = { static_cast<float>(i) / static_cast<float>(segments), 0.0f };
            model.vertices.push_back(outer);

            Sprite::VertexData inner{};
            inner.position = { c * innerRatio, 0.0f, s * innerRatio, 1.0f };
            inner.normal = { 0.0f, 1.0f, 0.0f };
            inner.texcoord = { static_cast<float>(i) / static_cast<float>(segments), 1.0f };
            model.vertices.push_back(inner);
        }

        for (int i = 0; i < segments; ++i)
        {
            uint32_t o0 = static_cast<uint32_t>(i * 2);
            uint32_t i0 = o0 + 1;
            uint32_t o1 = o0 + 2;
            uint32_t i1 = o0 + 3;

            model.indices.push_back(o0);
            model.indices.push_back(o1);
            model.indices.push_back(i0);

            model.indices.push_back(i0);
            model.indices.push_back(o1);
            model.indices.push_back(i1);
        }

        model.material.textureIndex = TextureManager::kInvalidTextureIndex;
        model.boundingRadius = 10000.0f; // マーカーは常に見えていてほしいのでカリングしない
        return model;
    }
}

PlacementEditor::~PlacementEditor()
{
    Finalize();
}

Slime* PlacementEditor::PlayerSlime() const
{
    return refs_.slimeManager ? refs_.slimeManager->GetLeader() : nullptr;
}

namespace
{
    /**
     * @brief スライムを「その XZ の床にちゃんと乗る中心 Y」で置き直す
     *
     * 【重要】床の Y をそのまま SetPosition() に渡してはいけない。
     * スライムの原点は**中心**なので、床の高さを中心に入れると足元が床より下に潜り、
     * 次のフレームの空中判定（着地上限 = 足元 + 0.15m）で上面が弾かれて
     * 下の段まで落ちる。実際に起動直後クラッシュの原因になった挙動そのもの
     * （claude/stage-terrain-and-crash-fixes.md 参照）。
     * 床の高さ + 接地オフセット（GetGroundY）で中心を出すこと
     */
    void PlaceSlimeOnGround(Slime* slime, float x, float z, float fallbackFloorY)
    {
        if (!slime) return;

        bool hasGround = false;
        const float centerY = SlimePhysics::CalculateGroundedCenterYEx(
            x, z, SlimePhysics::kIgnoreCurrentY, { 0.0f, 0.0f },
            slime->GetGroundY(), &hasGround, nullptr, { 0.0f, 0.0f }, false);

        slime->SetPosition({ x, hasGround ? centerY : (fallbackFloorY + slime->GetGroundY()), z });
        slime->SetVelocity({ 0.0f, 0.0f, 0.0f });
    }
}

void PlacementEditor::Initialize(const SceneRefs& refs)
{
    refs_ = refs;
    isActive_ = false;
    isDirty_ = false;
    ClearSelection();
    BuildMarkerMesh();
}

void PlacementEditor::Finalize()
{
    overlayObject_.reset();
    markerObject_.reset();
    overlayReady_ = false;
    markerReady_ = false;
    refs_ = SceneRefs{};
}

// ------------------------------------------------------------------
// モード切り替え
// ------------------------------------------------------------------

void PlacementEditor::SetActive(bool active)
{
    if (isActive_ == active) return;

    if (active)
    {
        // 入るとき: 保存されている配置をシーンへ戻してから編集を始める。
        // こうしないと「プレイ中に倒された敵」がそのまま消えた状態で編集されてしまう
        ApplyLayoutToScene();

        if (refs_.enemyManager)      refs_.enemyManager->SetEditorMode(true);
        if (refs_.coinManager)       refs_.coinManager->SetEditorMode(true);
        if (refs_.growthCubeManager) refs_.growthCubeManager->SetEditorMode(true);

        if (refs_.camera)
        {
            savedCamPos_ = refs_.camera->GetTranslate();
            savedCamRot_ = refs_.camera->GetRotate();
            savedFovY_ = refs_.camera->GetFovY();
            hasSavedCamera_ = true;
        }

        isActive_ = true;
        RebuildOverlay();
        FrameAll();
        UpdateCamera();
    }
    else
    {
        // 抜けるとき: シーンの現状を配置データへ吸い出して保存し、
        // そのあと配置データからシーンを作り直してプレイ状態をリセットする
        SyncLayoutFromScene();
        if (isDirty_)
        {
            Save();
        }

        isActive_ = false;
        ClearSelection();

        if (refs_.enemyManager)      refs_.enemyManager->SetEditorMode(false);
        if (refs_.coinManager)       refs_.coinManager->SetEditorMode(false);
        if (refs_.growthCubeManager) refs_.growthCubeManager->SetEditorMode(false);

        ApplyLayoutToScene();

        if (refs_.camera && hasSavedCamera_)
        {
            refs_.camera->SetTranslate(savedCamPos_);
            refs_.camera->SetRotate(savedCamRot_);
            refs_.camera->SetFovY(savedFovY_);
            refs_.camera->Update();
        }
    }
}

// ------------------------------------------------------------------
// 配置データ <-> シーン
// ------------------------------------------------------------------

void PlacementEditor::SetLayout(const StageLayout& layout)
{
    layout_ = layout;
    isDirty_ = false;
    ApplyLayoutToScene();
}

void PlacementEditor::ApplyLayoutToScene()
{
    // --- 地形が先。敵・コイン・キューブは地形へのレイキャストで高さが決まるので、
    //     地形を差し替えたあとでないと足元が無い場所に置かれてしまう ---
    if (refs_.terrain)
    {
        // JSON に terrain が無い（旧フォーマット）なら、
        // 以前 GamePlayScene にハードコードされていた既定配置を使う
        if (layout_.terrain.empty())
        {
            layout_.terrain = StageLayout::MakeDefaultTerrain();
        }
        refs_.terrain->ApplyLayout(layout_.terrain);
    }

    if (refs_.enemyManager)
    {
        refs_.enemyManager->ClearAll();
        for (const auto& e : layout_.enemies)
        {
            MobEnemy* spawned = refs_.enemyManager->Spawn(e.type, { e.position.x, 0.0f, e.position.z }, e.strength);
            if (spawned) spawned->SetFrozen(refs_.enemyManager->IsEditorMode());
        }
    }

    if (refs_.coinManager)
    {
        refs_.coinManager->ClearAll();
        for (const auto& c : layout_.coins)
        {
            refs_.coinManager->Spawn({ c.position.x, 0.0f, c.position.z });
        }
    }

    if (refs_.growthCubeManager)
    {
        refs_.growthCubeManager->ClearAll();
        for (const auto& g : layout_.growthCubes)
        {
            refs_.growthCubeManager->Spawn({ g.position.x, 0.0f, g.position.z }, g.size);
        }
    }

    if (refs_.bossFight)
    {
        refs_.bossFight->SetLayout(layout_.boss);
    }

    if (refs_.slimeManager)
    {
        if (Slime* leader = refs_.slimeManager->GetLeader())
        {
            // JSON の Y は参考値なので、床から中心 Y を取り直す。
            // MakeFallback() が書く playerStart.y は「床の高さ」なので、
            // そのまま入れると足元が床の下に潜って下の段へ落ちる
            PlaceSlimeOnGround(leader, layout_.playerStart.x, layout_.playerStart.z,
                               layout_.playerStart.y);
        }
    }

    ClearSelection();
}

void PlacementEditor::SyncLayoutFromScene()
{
    if (refs_.enemyManager)
    {
        layout_.enemies.clear();
        for (const auto& e : refs_.enemyManager->GetEnemies())
        {
            if (!e || e->IsDead()) continue;

            StageEnemyEntry entry;
            entry.type = e->GetType();
            entry.position = e->GetStageLocalPosition();
            entry.position.y = e->GetPosition().y; // 参考値。実行時はレイキャストで取り直される
            entry.strength = e->GetStrength();
            layout_.enemies.push_back(entry);
        }
    }

    if (refs_.coinManager)
    {
        layout_.coins.clear();
        for (const auto& c : refs_.coinManager->GetCoins())
        {
            if (!c) continue;

            StageCoinEntry entry;
            entry.position = c->GetStageLocalPosition();
            entry.position.y = c->GetPosition().y;
            layout_.coins.push_back(entry);
        }
    }

    if (refs_.growthCubeManager)
    {
        layout_.growthCubes.clear();
        for (const auto& g : refs_.growthCubeManager->GetCubes())
        {
            if (!g) continue;

            StageGrowthCubeEntry entry;
            entry.position = g->GetStageLocalPosition();
            entry.position.y = g->GetPosition().y;
            entry.size = g->GetBaseSize();
            layout_.growthCubes.push_back(entry);
        }
    }

    if (refs_.terrain)
    {
        refs_.terrain->WriteLayout(layout_.terrain);
    }

    if (refs_.bossFight)
    {
        refs_.bossFight->WriteLayout(layout_.boss);
    }

    if (refs_.slimeManager)
    {
        if (const Slime* leader = refs_.slimeManager->GetLeader())
        {
            layout_.playerStart = leader->GetPosition();
        }
    }
}

void PlacementEditor::OnTerrainChanged()
{
    // 地形が変わると「置ける場所」も基準平面も全部変わるので作り直す。
    // 実際の配置判定は毎回レイキャストしているので、オーバーレイは見た目だけの問題
    RebuildOverlay();
    MarkDirty();
    SyncLayoutFromScene();
}

bool PlacementEditor::Save()
{
    bool ok = layout_.SaveToFile(layoutPath_);
    if (ok)
    {
        isDirty_ = false;
        std::snprintf(statusText_, sizeof(statusText_), "Saved: %s", layoutPath_.c_str());
    }
    else
    {
        std::snprintf(statusText_, sizeof(statusText_), "Save FAILED: %s", layoutPath_.c_str());
    }
    return ok;
}

bool PlacementEditor::Load()
{
    StageLayout loaded;
    if (!loaded.LoadFromFile(layoutPath_))
    {
        std::snprintf(statusText_, sizeof(statusText_), "Load failed: %s", layoutPath_.c_str());
        return false;
    }

    SetLayout(loaded);
    std::snprintf(statusText_, sizeof(statusText_), "Loaded: %s", layoutPath_.c_str());
    return true;
}

// ------------------------------------------------------------------
// カメラ
// ------------------------------------------------------------------

void PlacementEditor::FrameAll()
{
    Vector3 bmin, bmax;
    if (!SlimePhysics::GetGroundWorldBounds(bmin, bmax))
    {
        camX_ = 0.0f;
        camZ_ = 0.0f;
        camHeight_ = 60.0f;
        return;
    }

    camX_ = (bmin.x + bmax.x) * 0.5f;
    camZ_ = (bmin.z + bmax.z) * 0.5f;

    float halfW = (bmax.x - bmin.x) * 0.5f;
    float halfD = (bmax.z - bmin.z) * 0.5f;

    float tanHalf = std::tan(kEditorFovY * 0.5f);
    float needX = (tanHalf > 1e-4f) ? (halfW / (tanHalf * kAspect)) : 60.0f;
    float needZ = (tanHalf > 1e-4f) ? (halfD / tanHalf) : 60.0f;

    camHeight_ = (std::max)(needX, needZ) * 1.10f;
    camHeightMax_ = (std::max)(camHeight_ * 2.0f, 400.0f);
    camHeight_ = std::clamp(camHeight_, camHeightMin_, camHeightMax_);
}

void PlacementEditor::UpdateCamera()
{
    if (!refs_.camera) return;

    refs_.camera->SetFovY(kEditorFovY);
    refs_.camera->SetRotate({ kHalfPi, 0.0f, 0.0f }); // 真下を向く
    refs_.camera->SetTranslate({ camX_, planeY_ + camHeight_, camZ_ });
    refs_.camera->Update();
}

Vector3 PlacementEditor::ScreenToWorldAt(const Vector2& screen, float camX, float camZ, float camHeight) const
{
    // 真上から見下ろす（pitch = +90 度、yaw / roll = 0）ので、
    // カメラのローカル軸は right = +X / up = +Z / forward = -Y になる。
    // よって視線ベクトルはこの形に潰れる:
    //   dir = ( ndcX * tan(fov/2) * aspect,  -1,  ndcY * tan(fov/2) )
    float ndcX = (screen.x / (kVirtualWidth * 0.5f)) - 1.0f;
    float ndcY = 1.0f - (screen.y / (kVirtualHeight * 0.5f));

    float tanHalf = std::tan(kEditorFovY * 0.5f);
    float dirX = ndcX * tanHalf * kAspect;
    float dirZ = ndcY * tanHalf;

    Vector3 camPos{ camX, planeY_ + camHeight, camZ };

    // 基準平面（planeY_）との交点。カメラは真下を向いているので t = camHeight
    auto IntersectAtHeight = [&](float targetY) -> Vector3 {
        float t = camPos.y - targetY; // dir.y = -1 なので進む距離 = 高さの差
        return { camPos.x + dirX * t, targetY, camPos.z + dirZ * t };
    };

    Vector3 p = IntersectAtHeight(planeY_);

    // 基準平面と実際の床の高さがずれていると、画面端ほど視差でずれる。
    // 「その XZ の床の高さ」で平面を取り直す、を2回まわして詰める
    for (int i = 0; i < 2; ++i)
    {
        SlimePhysics::GroundLayer layers[4];
        int count = SlimePhysics::QueryGroundLayers(p.x, p.z, layers, 4);
        if (count <= 0) break;

        // 【変更】以前は「プレイ area は下段」として一番下の床に合わせていたが、
        // いまの島は全域が上下2段で、遊ぶのは**上面**。StagePlacement::Test() も
        // 最上面を採るようになったので、そちらに合わせる
        float floorY = layers[0].y;
        if (std::abs(floorY - p.y) < 0.01f) break;

        p = IntersectAtHeight(floorY);
    }

    return p;
}

Vector3 PlacementEditor::ScreenToWorld(const Vector2& screen) const
{
    return ScreenToWorldAt(screen, camX_, camZ_, camHeight_);
}

// ------------------------------------------------------------------
// 更新（マウス処理）
// ------------------------------------------------------------------

void PlacementEditor::Update(float deltaTime)
{
    (void)deltaTime;
    if (!isActive_) return;

    MouseState mouse;

#ifdef USE_IMGUI
    ImGuiIO& io = ImGui::GetIO();
    mouse.valid = true;
    mouse.screen = { io.MousePos.x, io.MousePos.y };
    mouse.wheel = io.MouseWheel;
    mouse.overUI = io.WantCaptureMouse;

    // ウィンドウ外へ出たときは ImGui が -FLT_MAX を入れてくる
    if (mouse.screen.x < 0.0f || mouse.screen.y < 0.0f ||
        mouse.screen.x > kVirtualWidth || mouse.screen.y > kVirtualHeight)
    {
        mouse.valid = false;
    }
#endif

    if (mouse.valid)
    {
        mouse.world = ScreenToWorld(mouse.screen);
        lastCursorWorld_ = mouse.world;
        lastCursorResult_ = StagePlacement::Test(mouse.world.x, mouse.world.z, nullptr);
        hasCursor_ = true;
    }
    else
    {
        hasCursor_ = false;
    }

    HandleMouse(mouse);
    UpdateCamera();
}

void PlacementEditor::HandleMouse(const MouseState& mouse)
{
#ifdef USE_IMGUI
    ImGuiIO& io = ImGui::GetIO();

    const bool leftNow = io.MouseDown[0];
    const bool rightClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Right);

    // --- ホイール: カーソル位置を中心にズーム ---
    if (mouse.valid && !mouse.overUI && std::abs(mouse.wheel) > 0.0f)
    {
        Vector3 before = ScreenToWorldAt(mouse.screen, camX_, camZ_, camHeight_);

        float factor = std::pow(1.0f + zoomSpeed_, -mouse.wheel);
        float newHeight = std::clamp(camHeight_ * factor, camHeightMin_, camHeightMax_);

        Vector3 after = ScreenToWorldAt(mouse.screen, camX_, camZ_, newHeight);

        // カーソルの下にあるワールド座標が動かないようにカメラをずらす
        camX_ += (before.x - after.x);
        camZ_ += (before.z - after.z);
        camHeight_ = newHeight;
    }

    // --- 右クリック: 削除 / 選択解除 ---
    if (rightClicked && mouse.valid && !mouse.overUI)
    {
        DeleteAt(mouse.world);
    }

    // --- 左ボタン ---
    if (leftNow && !leftDown_)
    {
        // 押した瞬間
        leftDown_ = true;
        movedWhileDown_ = false;
        isPanning_ = false;
        isDraggingObject_ = false;
        pressScreen_ = mouse.screen;

        if (!mouse.valid || mouse.overUI)
        {
            // ImGui ウィンドウの上（タイトルバーのドラッグなど）や画面外で押された。
            // この押下は離すまで丸ごと無視する。
            // ここでフラグを立てないと、ウィンドウを動かすつもりの
            // ドラッグがそのままマップのパンになってしまう
            pressBlocked_ = true;
            dragStartedOnObject_ = false;
        }
        else
        {
            pressBlocked_ = false;
            const PickResult picked = PickAny(mouse.world);

            if (picked.kind != SelectionKind::None)
            {
                // 置いてあるものを掴んだ -> 選択してドラッグ移動の候補にする
                selectionKind_ = picked.kind;
                selectedEnemy_ = picked.enemy;
                selectedCoin_ = picked.coin;
                selectedCube_ = picked.cube;
                selectedTerrain_ = picked.terrain;
                dragStartedOnObject_ = true;

                // 地形はハンドルとカーソルのずれを覚えておく。
                // 覚えないと掴んだ瞬間にパーツがカーソルの下へ飛ぶ
                if (picked.kind == SelectionKind::Terrain && picked.terrain)
                {
                    const Vector3 handle = picked.terrain->HandlePosition();
                    terrainDragOffset_ = { picked.terrain->position.x - handle.x,
                                           0.0f,
                                           picked.terrain->position.z - handle.z };
                }
            }
            else
            {
                dragStartedOnObject_ = false;
                panAnchorWorld_ = mouse.world;
            }
        }
    }
    else if (leftNow && leftDown_)
    {
        // 押しっぱなし
        if (pressBlocked_) return;

        float dx = mouse.screen.x - pressScreen_.x;
        float dy = mouse.screen.y - pressScreen_.y;
        if (!movedWhileDown_ && (dx * dx + dy * dy) > (kDragThreshold * kDragThreshold))
        {
            movedWhileDown_ = true;
            if (dragStartedOnObject_) isDraggingObject_ = true;
            else                      isPanning_ = true;
        }

        if (isDraggingObject_ && mouse.valid)
        {
            MoveSelectionTo(mouse.world);
        }
        else if (isPanning_ && mouse.valid)
        {
            // 掴んだワールド座標がカーソルの下に留まるようにカメラを動かす
            Vector3 current = ScreenToWorldAt(mouse.screen, camX_, camZ_, camHeight_);
            camX_ += (panAnchorWorld_.x - current.x);
            camZ_ += (panAnchorWorld_.z - current.z);
        }
    }
    else if (!leftNow && leftDown_)
    {
        // 離した瞬間
        leftDown_ = false;

        if (pressBlocked_)
        {
            pressBlocked_ = false;
            movedWhileDown_ = false;
            isPanning_ = false;
            isDraggingObject_ = false;
            dragStartedOnObject_ = false;
            return;
        }

        if (!movedWhileDown_ && mouse.valid && !mouse.overUI)
        {
            // ドラッグしていない = クリック
            if (dragStartedOnObject_)
            {
                // 選択済み（PickAny の時点で選択している）。パラメータのパネルが出る
            }
            else
            {
                PlaceAt(mouse.world);
            }
        }
        else if (isDraggingObject_ && selectionKind_ == SelectionKind::Terrain)
        {
            // 地形を動かし終わった。ここで初めて配置禁止オーバーレイを作り直す
            // （ドラッグ中に毎フレームやると数千セルのレイキャストで固まる）
            OnTerrainChanged();
        }

        movedWhileDown_ = false;
        isPanning_ = false;
        isDraggingObject_ = false;
        dragStartedOnObject_ = false;
    }
#else
    (void)mouse;
#endif
}

// ------------------------------------------------------------------
// ピッキング
// ------------------------------------------------------------------

MobEnemy* PlacementEditor::PickEnemy(const Vector3& world, float* outDistSq) const
{
    if (!refs_.enemyManager) return nullptr;

    MobEnemy* best = nullptr;
    float bestDistSq = 1e18f;

    for (const auto& e : refs_.enemyManager->GetEnemies())
    {
        if (!e || e->IsDead()) continue;

        const Vector3& p = e->GetPosition();
        float dx = p.x - world.x;
        float dz = p.z - world.z;
        float distSq = dx * dx + dz * dz;

        // 掴める範囲は見た目の大きさに比例させる
        float grab = e->GetScale().x * 1.2f + 0.5f;
        if (distSq > grab * grab) continue;

        if (distSq < bestDistSq)
        {
            bestDistSq = distSq;
            best = e.get();
        }
    }

    if (best && outDistSq) *outDistSq = bestDistSq;
    return best;
}

Coin* PlacementEditor::PickCoin(const Vector3& world, float* outDistSq) const
{
    if (!refs_.coinManager) return nullptr;

    float grab = CoinManager::GetConfig().radius + 0.5f;
    float grabSq = grab * grab;

    Coin* best = nullptr;
    float bestDistSq = 1e18f;

    for (const auto& c : refs_.coinManager->GetCoins())
    {
        if (!c) continue;

        const Vector3& p = c->GetPosition();
        float dx = p.x - world.x;
        float dz = p.z - world.z;
        float distSq = dx * dx + dz * dz;
        if (distSq > grabSq) continue;

        if (distSq < bestDistSq)
        {
            bestDistSq = distSq;
            best = c.get();
        }
    }

    if (best && outDistSq) *outDistSq = bestDistSq;
    return best;
}

bool PlacementEditor::PickPlayer(const Vector3& world, float* outDistSq) const
{
    if (!PlayerSlime()) return false;

    const Vector3& p = PlayerSlime()->GetPosition();
    float dx = p.x - world.x;
    float dz = p.z - world.z;
    float distSq = dx * dx + dz * dz;

    float grab = PlayerSlime()->GetCurrentScale() + 0.6f;
    if (distSq > grab * grab) return false;

    if (outDistSq) *outDistSq = distSq;
    return true;
}

GrowthCube* PlacementEditor::PickGrowthCube(const Vector3& world, float* outDistSq) const
{
    if (!refs_.growthCubeManager) return nullptr;

    GrowthCube* best = nullptr;
    float bestDistSq = 1e18f;

    for (const auto& g : refs_.growthCubeManager->GetCubes())
    {
        if (!g) continue;

        const Vector3& p = g->GetPosition();
        float dx = p.x - world.x;
        float dz = p.z - world.z;
        float distSq = dx * dx + dz * dz;

        float grab = g->GetBaseSize() * 0.5f + 0.6f;
        if (distSq > grab * grab) continue;

        if (distSq < bestDistSq)
        {
            bestDistSq = distSq;
            best = g.get();
        }
    }

    if (best && outDistSq) *outDistSq = bestDistSq;
    return best;
}

bool PlacementEditor::PickBoss(const Vector3& world, float* outDistSq) const
{
    if (!refs_.bossFight || !refs_.bossFight->IsEnabled()) return false;

    const Vector3 p = refs_.bossFight->GetWorldPosition();
    float dx = p.x - world.x;
    float dz = p.z - world.z;
    float distSq = dx * dx + dz * dz;

    float grab = refs_.bossFight->GetPickRadius() + 0.5f;
    if (distSq > grab * grab) return false;

    if (outDistSq) *outDistSq = distSq;
    return true;
}

StageTerrain::Part* PlacementEditor::PickTerrain(const Vector3& world, float* outDistSq) const
{
    if (!refs_.terrain) return nullptr;

    StageTerrain::Part* best = nullptr;
    float bestDistSq = 1e18f;

    // 地形パーツは島まるごとの大きさがあるので、AABB で掴むと何を触っているか
    // 分からなくなる。ワールド AABB の XZ 中心に置いた「ハンドル」だけを掴ませる
    for (const auto& partPtr : refs_.terrain->GetParts())
    {
        if (!partPtr) continue;

        const Vector3 handle = partPtr->HandlePosition();
        float dx = handle.x - world.x;
        float dz = handle.z - world.z;
        float distSq = dx * dx + dz * dz;

        float grab = partPtr->HandleRadius();
        if (distSq > grab * grab) continue;

        if (distSq < bestDistSq)
        {
            bestDistSq = distSq;
            best = partPtr.get();
        }
    }

    if (best && outDistSq) *outDistSq = bestDistSq;
    return best;
}

PlacementEditor::PickResult PlacementEditor::PickAny(const Vector3& world) const
{
    PickResult result;
    float bestDistSq = 1e18f;
    float d = 0.0f;

    if (layer_ == EditLayer::Terrain)
    {
        // 地形レイヤーでは地形パーツしか掴めない。
        // こうしないと敵とハンドルが混ざって、どちらを動かしているのか分からなくなる
        d = 0.0f;
        if (StageTerrain::Part* part = PickTerrain(world, &d))
        {
            result.kind = SelectionKind::Terrain;
            result.terrain = part;
        }
        return result;
    }

    // --- Objects レイヤー。小さいものほど優先的に拾えるよう、距離が近い順に決める ---
    d = 0.0f;
    if (Coin* coin = PickCoin(world, &d))
    {
        bestDistSq = d;
        result.kind = SelectionKind::Coin;
        result.coin = coin;
    }

    d = 0.0f;
    if (GrowthCube* cube = PickGrowthCube(world, &d))
    {
        if (d < bestDistSq)
        {
            bestDistSq = d;
            result = PickResult{};
            result.kind = SelectionKind::GrowthCube;
            result.cube = cube;
        }
    }

    d = 0.0f;
    if (MobEnemy* enemy = PickEnemy(world, &d))
    {
        if (d < bestDistSq)
        {
            bestDistSq = d;
            result = PickResult{};
            result.kind = SelectionKind::Enemy;
            result.enemy = enemy;
        }
    }

    d = 0.0f;
    if (PickPlayer(world, &d))
    {
        if (d < bestDistSq)
        {
            bestDistSq = d;
            result = PickResult{};
            result.kind = SelectionKind::PlayerStart;
        }
    }

    d = 0.0f;
    if (PickBoss(world, &d))
    {
        // ボスは図体が大きいので一番あとに見る（ほかを掴めなかったときだけ）
        if (d < bestDistSq)
        {
            result = PickResult{};
            result.kind = SelectionKind::Boss;
        }
    }

    return result;
}

// ------------------------------------------------------------------
// 配置操作
// ------------------------------------------------------------------

void PlacementEditor::ClearSelection()
{
    selectionKind_ = SelectionKind::None;
    selectedEnemy_ = nullptr;
    selectedCoin_ = nullptr;
    selectedCube_ = nullptr;
    selectedTerrain_ = nullptr;
}

void PlacementEditor::PlaceAt(const Vector3& world)
{
    // --- 地形レイヤー: 置ける場所かどうかは見ない（地形そのものを置くので当然）---
    if (layer_ == EditLayer::Terrain)
    {
        if (!refs_.terrain)
        {
            std::snprintf(statusText_, sizeof(statusText_), "No terrain container");
            return;
        }

        const auto& catalog = StageTerrain::GetMeshCatalog();
        if (catalog.empty())
        {
            std::snprintf(statusText_, sizeof(statusText_), "No .obj found in %s",
                          StageLayout::kTerrainDirectory);
            return;
        }

        const int index = std::clamp(terrainMeshIndex_, 0, static_cast<int>(catalog.size()) - 1);

        // 位置は XZ だけ。Y は 0 に置く（JSON を手で書けば効く）
        StageTerrain::Part* added = refs_.terrain->AddPart(catalog[static_cast<size_t>(index)],
                                                           { world.x, 0.0f, world.z },
                                                           terrainNewRotationY_, terrainNewScale_,
                                                           terrainNewBossTrigger_);
        if (!added)
        {
            std::snprintf(statusText_, sizeof(statusText_), "Failed to load mesh: %s",
                          catalog[static_cast<size_t>(index)].c_str());
            return;
        }

        selectionKind_ = SelectionKind::Terrain;
        selectedEnemy_ = nullptr;
        selectedCoin_ = nullptr;
        selectedCube_ = nullptr;
        selectedTerrain_ = added;

        std::snprintf(statusText_, sizeof(statusText_), "Placed terrain: %s", added->mesh.c_str());
        OnTerrainChanged();
        return;
    }

    float floorY = 0.0f;
    StagePlacement::Result result = StagePlacement::Test(world.x, world.z, &floorY);
    if (result != StagePlacement::Result::Ok)
    {
        std::snprintf(statusText_, sizeof(statusText_), "Cannot place here: %s",
                      StagePlacement::ResultLabel(result));
        return;
    }

    Vector3 pos{ world.x, floorY, world.z };

    switch (brush_)
    {
    case Brush::Enemy:
        if (refs_.enemyManager)
        {
            MobEnemy* spawned = refs_.enemyManager->Spawn(brushEnemyType_, { pos.x, 0.0f, pos.z }, brushStrength_);
            if (spawned)
            {
                spawned->SetFrozen(true);
                selectionKind_ = SelectionKind::Enemy;
                selectedEnemy_ = spawned;
                selectedCoin_ = nullptr;
                MarkDirty();
                std::snprintf(statusText_, sizeof(statusText_), "Placed enemy (%s, strength %d)",
                              StageLayout::TypeToName(brushEnemyType_), spawned->GetStrength());
            }
        }
        break;

    case Brush::Coin:
        if (refs_.coinManager)
        {
            Coin* spawned = refs_.coinManager->Spawn({ pos.x, 0.0f, pos.z });
            if (spawned)
            {
                selectionKind_ = SelectionKind::Coin;
                selectedCoin_ = spawned;
                selectedEnemy_ = nullptr;
                MarkDirty();
                std::snprintf(statusText_, sizeof(statusText_), "Placed coin");
            }
        }
        break;

    case Brush::GrowthCube:
        if (refs_.growthCubeManager)
        {
            GrowthCube* spawned = refs_.growthCubeManager->Spawn({ pos.x, 0.0f, pos.z }, brushCubeSize_);
            if (spawned)
            {
                ClearSelection();
                selectionKind_ = SelectionKind::GrowthCube;
                selectedCube_ = spawned;
                MarkDirty();
                std::snprintf(statusText_, sizeof(statusText_), "Placed growth cube");
            }
        }
        break;

    case Brush::Boss:
        if (refs_.bossFight)
        {
            // ボスは1体だけ。置き直すと前のボスは消える
            refs_.bossFight->SetStageLocalPosition({ pos.x, 0.0f, pos.z });
            refs_.bossFight->SetEnabled(true);
            ClearSelection();
            selectionKind_ = SelectionKind::Boss;
            MarkDirty();
            std::snprintf(statusText_, sizeof(statusText_), "Placed boss (HP %d)",
                          refs_.bossFight->GetMaxHp());
        }
        break;

    case Brush::PlayerStart:
        if (PlayerSlime())
        {
            PlaceSlimeOnGround(PlayerSlime(), pos.x, pos.z, floorY);
            ClearSelection();
            selectionKind_ = SelectionKind::PlayerStart;
            MarkDirty();
            std::snprintf(statusText_, sizeof(statusText_), "Moved player start");
        }
        break;
    }

    SyncLayoutFromScene();
}

void PlacementEditor::DeleteAt(const Vector3& world)
{
    const PickResult picked = PickAny(world);

    switch (picked.kind)
    {
    case SelectionKind::Enemy:
        if (refs_.enemyManager && picked.enemy)
        {
            if (selectedEnemy_ == picked.enemy) ClearSelection();
            refs_.enemyManager->Remove(picked.enemy);
            MarkDirty();
            SyncLayoutFromScene();
            std::snprintf(statusText_, sizeof(statusText_), "Deleted enemy");
        }
        break;

    case SelectionKind::Coin:
        if (refs_.coinManager && picked.coin)
        {
            if (selectedCoin_ == picked.coin) ClearSelection();
            refs_.coinManager->Remove(picked.coin);
            MarkDirty();
            SyncLayoutFromScene();
            std::snprintf(statusText_, sizeof(statusText_), "Deleted coin");
        }
        break;

    case SelectionKind::GrowthCube:
        if (refs_.growthCubeManager && picked.cube)
        {
            if (selectedCube_ == picked.cube) ClearSelection();
            refs_.growthCubeManager->Remove(picked.cube);
            MarkDirty();
            SyncLayoutFromScene();
            std::snprintf(statusText_, sizeof(statusText_), "Deleted growth cube");
        }
        break;

    case SelectionKind::Boss:
        if (refs_.bossFight)
        {
            ClearSelection();
            refs_.bossFight->SetEnabled(false);
            MarkDirty();
            SyncLayoutFromScene();
            std::snprintf(statusText_, sizeof(statusText_), "Removed boss");
        }
        break;

    case SelectionKind::Terrain:
        if (refs_.terrain && picked.terrain)
        {
            if (selectedTerrain_ == picked.terrain) ClearSelection();
            std::snprintf(statusText_, sizeof(statusText_), "Deleted terrain: %s",
                          picked.terrain->mesh.c_str());
            refs_.terrain->RemovePart(picked.terrain);
            OnTerrainChanged();
        }
        break;

    case SelectionKind::PlayerStart:
        // プレイヤーは消せない
        std::snprintf(statusText_, sizeof(statusText_), "Player start cannot be deleted");
        break;

    default:
        ClearSelection();
        break;
    }
}

void PlacementEditor::MoveSelectionTo(const Vector3& world)
{
    // --- 地形は「置ける場所」の制限を受けない。XZ だけ動かす ---
    if (selectionKind_ == SelectionKind::Terrain)
    {
        if (refs_.terrain && selectedTerrain_)
        {
            refs_.terrain->SetPartPositionXZ(selectedTerrain_,
                                             world.x + terrainDragOffset_.x,
                                             world.z + terrainDragOffset_.z);
            // ドラッグ中はオーバーレイを作り直さない（毎フレームやると重い）。
            // 離したときに OnTerrainChanged() が走る
            MarkDirty();
        }
        return;
    }

    float floorY = 0.0f;
    StagePlacement::Result result = StagePlacement::Test(world.x, world.z, &floorY);
    if (result != StagePlacement::Result::Ok)
    {
        // 置けないところへは動かさない（掴んだまま禁止領域に入っても元の場所に残る）
        std::snprintf(statusText_, sizeof(statusText_), "Cannot move there: %s",
                      StagePlacement::ResultLabel(result));
        return;
    }

    switch (selectionKind_)
    {
    case SelectionKind::Enemy:
        if (selectedEnemy_)
        {
            selectedEnemy_->SetStageLocalPosition({ world.x, 0.0f, world.z });
            selectedEnemy_->RequestGroundSnap();
            MarkDirty();
        }
        break;

    case SelectionKind::Coin:
        if (selectedCoin_)
        {
            selectedCoin_->SetStageLocalPosition({ world.x, 0.0f, world.z });
            MarkDirty();
        }
        break;

    case SelectionKind::GrowthCube:
        if (selectedCube_)
        {
            selectedCube_->SetStageLocalPosition({ world.x, 0.0f, world.z });
            MarkDirty();
        }
        break;

    case SelectionKind::Boss:
        if (refs_.bossFight)
        {
            refs_.bossFight->SetStageLocalPosition({ world.x, 0.0f, world.z });
            MarkDirty();
        }
        break;

    case SelectionKind::PlayerStart:
        if (PlayerSlime())
        {
            PlaceSlimeOnGround(PlayerSlime(), world.x, world.z, floorY);
            MarkDirty();
        }
        break;

    default:
        break;
    }

    SyncLayoutFromScene();
}

// ------------------------------------------------------------------
// 配置禁止オーバーレイ
// ------------------------------------------------------------------

void PlacementEditor::RebuildOverlay()
{
    overlayModel_ = Object3d::ModelData{};
    overlayObject_.reset();
    overlayReady_ = false;
    overlayCellCount_ = 0;

    Vector3 bmin, bmax;
    if (!SlimePhysics::GetGroundWorldBounds(bmin, bmax)) return;

    float cell = (std::max)(0.25f, overlayCellSize_);

    // セル数が爆発しないように、必要ならセルを粗くする
    const int kMaxCells = 60000;
    for (int guard = 0; guard < 8; ++guard)
    {
        float nx = (bmax.x - bmin.x) / cell + 1.0f;
        float nz = (bmax.z - bmin.z) / cell + 1.0f;
        if (nx * nz <= static_cast<float>(kMaxCells)) break;
        cell *= 1.5f;
    }
    overlayCellSize_ = cell;

    const float half = cell * 0.48f;
    const float lift = 0.15f; // Zファイト防止に少しだけ床から浮かせる

    double sumOkY = 0.0;
    int okCount = 0;

    // 【重要】判定は必ず StagePlacement::Test() と同じものを使うこと。
    // 以前は「床が2層あったら禁止」で塗っていたが、Test() 側が
    // 「最上面を採る + 頭上クリアランス」に変わったので、オーバーレイだけ
    // 古い規則のままだと赤い場所に普通に置けてしまい、見た目が嘘になる
    for (float z = bmin.z; z <= bmax.z; z += cell)
    {
        for (float x = bmin.x; x <= bmax.x; x += cell)
        {
            float floorY = 0.0f;
            StagePlacement::Result result = StagePlacement::Test(x, z, &floorY);

            if (result == StagePlacement::Result::Ok)
            {
                // プレイ面。カメラの基準平面を決めるために高さを平均しておく
                sumOkY += floorY;
                ++okCount;
                continue;
            }
            if (result == StagePlacement::Result::NoGround) continue; // 島の外。描くものが無い

            // 頭上が詰まっている = オーバーハングの下。ここを赤く塗る
            float y = floorY + lift;

            uint32_t base = static_cast<uint32_t>(overlayModel_.vertices.size());

            const Vector3 corners[4] = {
                { x - half, y, z - half },
                { x + half, y, z - half },
                { x + half, y, z + half },
                { x - half, y, z + half },
            };
            const Vector2 uvs[4] = { {0.0f,0.0f}, {1.0f,0.0f}, {1.0f,1.0f}, {0.0f,1.0f} };

            for (int i = 0; i < 4; ++i)
            {
                Sprite::VertexData v{};
                v.position = { corners[i].x, corners[i].y, corners[i].z, 1.0f };
                v.normal = { 0.0f, 1.0f, 0.0f };
                v.texcoord = uvs[i];
                overlayModel_.vertices.push_back(v);
            }

            overlayModel_.indices.push_back(base + 0);
            overlayModel_.indices.push_back(base + 2);
            overlayModel_.indices.push_back(base + 1);
            overlayModel_.indices.push_back(base + 0);
            overlayModel_.indices.push_back(base + 3);
            overlayModel_.indices.push_back(base + 2);

            ++overlayCellCount_;
        }
    }

    if (okCount > 0)
    {
        planeY_ = static_cast<float>(sumOkY / static_cast<double>(okCount));
    }
    else
    {
        planeY_ = bmin.y;
    }

    if (overlayModel_.vertices.empty() || !refs_.object3dCom) return;

    overlayModel_.material.textureIndex = TextureManager::kInvalidTextureIndex;
    overlayModel_.boundingRadius = 100000.0f; // 島全体を覆うのでカリングさせない

    overlayObject_ = std::make_unique<Object3d>();
    overlayObject_->Initialize(refs_.object3dCom, overlayModel_);
    overlayObject_->SetCamera(refs_.camera);
    overlayObject_->SetEnableLighting(false);
    overlayObject_->SetColor(forbiddenColor_);
    overlayObject_->SetTranslate({ 0.0f, 0.0f, 0.0f });
    overlayObject_->SetRotate({ 0.0f, 0.0f, 0.0f });
    overlayObject_->SetScale({ 1.0f, 1.0f, 1.0f });
    overlayObject_->Update();

    overlayReady_ = true;
}

void PlacementEditor::BuildMarkerMesh()
{
    if (!refs_.object3dCom) return;

    markerModel_ = GenerateRingMesh(0.72f, 40);

    markerObject_ = std::make_unique<Object3d>();
    markerObject_->Initialize(refs_.object3dCom, markerModel_);
    markerObject_->SetCamera(refs_.camera);
    markerObject_->SetEnableLighting(false);
    markerObject_->Update();

    markerReady_ = true;
}

// ------------------------------------------------------------------
// 描画
// ------------------------------------------------------------------

void PlacementEditor::DrawWithPipeline(const RenderContext& ctx, Object3d* object,
                                       ID3D12PipelineState* pipelineState)
{
    if (!object || !refs_.object3dCom || !ctx.commandList || !ctx.camera) return;
    if (!pipelineState) return;
    if (ctx.camera->GetCameraGpuAddress() == 0) return;

    // Object3dCom::Draw() と同じバインドを、パイプラインだけ差し替えて自前でやる。
    // Object3dCom::Draw() は Object3D_Normal 決め打ちなので、
    // デプス書き込みを切った半透明描画にはこの経路が要る
    ctx.commandList->SetGraphicsRootSignature(refs_.object3dCom->GetRootSignature().Get());
    ctx.commandList->SetPipelineState(pipelineState);

    D3D12_GPU_DESCRIPTOR_HANDLE mainTextureHandle = ctx.textureHandle;
    if (mainTextureHandle.ptr == 0)
    {
        mainTextureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(
            TextureManager::GetInstance()->GetTextureIndexByFilePath("Resources/CG4/human/white.png"));
    }
    if (mainTextureHandle.ptr != 0)
    {
        ctx.commandList->SetGraphicsRootDescriptorTable(2, mainTextureHandle);
    }

    // 環境マップ（ルートパラメータ5）。無ければメインテクスチャで代用する
    uint32_t skyboxIndex = SceneManager::GetInstance()->GetSkyboxTextureIndex();
    D3D12_GPU_DESCRIPTOR_HANDLE skyboxHandle = mainTextureHandle;
    if (skyboxIndex != TextureManager::kInvalidTextureIndex)
    {
        skyboxHandle = TextureManager::GetInstance()->GetSrvHandleGPU(skyboxIndex);
    }
    if (skyboxHandle.ptr != 0)
    {
        ctx.commandList->SetGraphicsRootDescriptorTable(5, skyboxHandle);
    }

    if (ctx.light)
    {
        ctx.commandList->SetGraphicsRootConstantBufferView(
            3, ctx.light->GetDirectionalLightResource()->GetGPUVirtualAddress());
    }

    ctx.commandList->SetGraphicsRootConstantBufferView(4, ctx.camera->GetCameraGpuAddress());

    object->DrawInternal(ctx);
}

bool PlacementEditor::DrawGroundTranslucent(const RenderContext& ctx, Object3d* ground,
                                            const Object3d::ModelData& modelData)
{
    (void)modelData;
    if (!isActive_ || !translucentGround_ || !ground || !refs_.object3dCom) return false;

    // 上段（一本道）越しに下段が見えるように、デプス書き込みを切った
    // 半透明パイプライン（Object3D_Effect）で描く。
    // 地形は1メッシュなので「上段だけ半透明」にはできず、全体が薄くなる
    ground->SetColor({ groundBaseColor_.x, groundBaseColor_.y, groundBaseColor_.z, groundAlpha_ });
    ground->Update();

    DrawWithPipeline(ctx, ground, refs_.object3dCom->GetEffectPipelineState().Get());

    // 次のフレームの通常描画で色が薄いままにならないよう、その場で戻す
    ground->SetColor(groundBaseColor_);
    return true;
}

void PlacementEditor::DrawMarker(const RenderContext& ctx, const Vector3& worldPos,
                                 float radius, const Vector4& color)
{
    if (!markerReady_ || !markerObject_ || !refs_.object3dCom) return;

    markerObject_->SetTranslate({ worldPos.x, worldPos.y + 0.20f, worldPos.z });
    markerObject_->SetScale({ radius, 1.0f, radius });
    markerObject_->SetRotate({ 0.0f, 0.0f, 0.0f });
    markerObject_->SetColor(color);
    markerObject_->SetCamera(ctx.camera);
    markerObject_->Update();

    DrawWithPipeline(ctx, markerObject_.get(), refs_.object3dCom->GetOverlayPipelineState().Get());
}

void PlacementEditor::Draw(const RenderContext& ctx)
{
    if (!isActive_) return;

    if (!refs_.object3dCom) return;

    // 1. 配置禁止領域
    if (showOverlay_ && overlayReady_ && overlayObject_)
    {
        overlayObject_->SetCamera(ctx.camera);
        overlayObject_->SetColor(forbiddenColor_);
        overlayObject_->Update();
        DrawWithPipeline(ctx, overlayObject_.get(), refs_.object3dCom->GetEffectPipelineState().Get());
    }

    // 2. 選択マーカー
    switch (selectionKind_)
    {
    case SelectionKind::Enemy:
        if (selectedEnemy_)
        {
            DrawMarker(ctx, selectedEnemy_->GetPosition(),
                       selectedEnemy_->GetScale().x * 1.3f + 0.5f, selectionColor_);
        }
        break;

    case SelectionKind::Coin:
        if (selectedCoin_)
        {
            DrawMarker(ctx, selectedCoin_->GetPosition(),
                       CoinManager::GetConfig().radius + 0.5f, selectionColor_);
        }
        break;

    case SelectionKind::GrowthCube:
        if (selectedCube_)
        {
            DrawMarker(ctx, selectedCube_->GetPosition(),
                       selectedCube_->GetBaseSize() * 0.5f + 0.6f, selectionColor_);
        }
        break;

    case SelectionKind::Boss:
        if (refs_.bossFight && refs_.bossFight->IsEnabled())
        {
            DrawMarker(ctx, refs_.bossFight->GetWorldPosition(),
                       refs_.bossFight->GetPickRadius() + 0.6f, selectionColor_);
        }
        break;

    case SelectionKind::Terrain:
        if (selectedTerrain_)
        {
            DrawMarker(ctx, selectedTerrain_->HandlePosition(),
                       selectedTerrain_->HandleRadius() * 1.15f, selectionColor_);
        }
        break;

    case SelectionKind::PlayerStart:
        if (PlayerSlime())
        {
            DrawMarker(ctx, PlayerSlime()->GetPosition(),
                       PlayerSlime()->GetCurrentScale() + 0.7f, selectionColor_);
        }
        break;

    default:
        break;
    }

    // 3. 地形パーツのハンドル。Terrain レイヤーのときだけ全部出す。
    //    ボス戦トリガー付きのパーツは別の色にして、どれが引き金か一目で分かるようにする
    if (showTerrainHandles_ && layer_ == EditLayer::Terrain && refs_.terrain)
    {
        for (const auto& partPtr : refs_.terrain->GetParts())
        {
            if (!partPtr || partPtr.get() == selectedTerrain_) continue;
            DrawMarker(ctx, partPtr->HandlePosition(), partPtr->HandleRadius(),
                       partPtr->bossTrigger ? terrainTriggerColor_ : terrainHandleColor_);
        }
    }
    else if (showTerrainHandles_ && refs_.terrain)
    {
        // Objects レイヤーでも、トリガー付きのパーツだけは出しておく。
        // 「どこに踏み入れるとボス戦が始まるか」を確認したいことのほうが多い
        for (const auto& partPtr : refs_.terrain->GetParts())
        {
            if (!partPtr || !partPtr->bossTrigger) continue;
            DrawMarker(ctx, partPtr->HandlePosition(), partPtr->HandleRadius(), terrainTriggerColor_);
        }
    }

    // 4. ボスの位置（選択していなくても常に出す。図体が大きいので目印が要る）
    if (refs_.bossFight && refs_.bossFight->IsEnabled() && selectionKind_ != SelectionKind::Boss)
    {
        DrawMarker(ctx, refs_.bossFight->GetWorldPosition(),
                   refs_.bossFight->GetPickRadius(), { 1.0f, 0.35f, 0.25f, 0.5f });
    }

    // 5. カーソル（置けるかどうかを色で出す）
    if (hasCursor_)
    {
        // 地形レイヤーではどこにでも置けるので、常に「置ける」色にする
        const bool ok = (layer_ == EditLayer::Terrain) ||
                        (lastCursorResult_ == StagePlacement::Result::Ok);
        DrawMarker(ctx, lastCursorWorld_, 0.8f, ok ? cursorOkColor_ : cursorNgColor_);
    }
}

// ------------------------------------------------------------------
// ImGui
// ------------------------------------------------------------------

void PlacementEditor::DrawImGui()
{
#ifdef USE_IMGUI
    // このエンジンの ImGui フォントには日本語グリフが無く、日本語を渡すと全部「?」になる。
    // ここに書く文字列（ラベル・ボタン名・statusText_）は必ず ASCII にすること
    if (!isActive_) return;

    ImGui::SetNextWindowPos(ImVec2(500, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360, 520), ImGuiCond_FirstUseEver);
    ImGui::Begin("Placement Editor");

    ImGui::TextWrapped("L-Click: place / select    L-Drag: move object, or pan map");
    ImGui::TextWrapped("R-Click: delete            Wheel: zoom at cursor");
    ImGui::Separator();

    // --- 編集レイヤー ---
    // 地形は島まるごとの大きさがあるので、敵やコインと同じ土俵で掴ませない
    ImGui::SeparatorText("Layer");
    if (ImGui::RadioButton("Objects", layer_ == EditLayer::Objects))
    {
        layer_ = EditLayer::Objects;
        ClearSelection();
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Terrain", layer_ == EditLayer::Terrain))
    {
        layer_ = EditLayer::Terrain;
        ClearSelection();
    }

    if (layer_ == EditLayer::Terrain)
    {
        // --- 地形ブラシ ---
        ImGui::SeparatorText("Terrain brush");

        const auto& catalog = StageTerrain::GetMeshCatalog();
        if (catalog.empty())
        {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "No .obj found in %s",
                               StageLayout::kTerrainDirectory);
        }
        else
        {
            terrainMeshIndex_ = std::clamp(terrainMeshIndex_, 0, static_cast<int>(catalog.size()) - 1);
            if (ImGui::BeginCombo("Mesh", catalog[static_cast<size_t>(terrainMeshIndex_)].c_str()))
            {
                for (int i = 0; i < static_cast<int>(catalog.size()); ++i)
                {
                    const bool selected = (i == terrainMeshIndex_);
                    if (ImGui::Selectable(catalog[static_cast<size_t>(i)].c_str(), selected))
                    {
                        terrainMeshIndex_ = i;
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }

        ImGui::SliderAngle("Rotation Y", &terrainNewRotationY_, -180.0f, 180.0f);
        ImGui::DragFloat("Scale", &terrainNewScale_, 0.005f, 0.01f, 4.0f);
        ImGui::Checkbox("Boss trigger", &terrainNewBossTrigger_);

        if (ImGui::Button("Refresh mesh list"))
        {
            StageTerrain::RefreshMeshCatalog();
        }
        ImGui::SameLine();
        ImGui::Checkbox("Show handles", &showTerrainHandles_);

        ImGui::TextDisabled("Click empty space to place. Drag a handle to move (XZ only).");
    }
    else
    {
        // --- オブジェクトのブラシ ---
        ImGui::SeparatorText("Brush");
        if (ImGui::RadioButton("Enemy", brush_ == Brush::Enemy)) brush_ = Brush::Enemy;
        ImGui::SameLine();
        if (ImGui::RadioButton("Coin", brush_ == Brush::Coin)) brush_ = Brush::Coin;
        ImGui::SameLine();
        if (ImGui::RadioButton("Growth Cube", brush_ == Brush::GrowthCube)) brush_ = Brush::GrowthCube;
        if (ImGui::RadioButton("Boss", brush_ == Brush::Boss)) brush_ = Brush::Boss;
        ImGui::SameLine();
        if (ImGui::RadioButton("Player Start", brush_ == Brush::PlayerStart)) brush_ = Brush::PlayerStart;

        if (brush_ == Brush::Enemy)
        {
            int typeIndex = static_cast<int>(brushEnemyType_);
            if (ImGui::Combo("Type", &typeIndex, kEnemyTypeNames, IM_ARRAYSIZE(kEnemyTypeNames)))
            {
                brushEnemyType_ = static_cast<EnemyType>(typeIndex);
            }

            bool randomStrength = (brushStrength_ < 1);
            if (ImGui::Checkbox("Random strength", &randomStrength))
            {
                brushStrength_ = randomStrength ? -1 : 3;
            }
            if (!randomStrength)
            {
                ImGui::SliderInt("Strength", &brushStrength_, 1, 10);
            }
        }
        else if (brush_ == Brush::GrowthCube)
        {
            ImGui::DragFloat("Cube size", &brushCubeSize_, 0.01f, 0.2f, 4.0f);
        }
        else if (brush_ == Brush::Boss)
        {
            if (refs_.bossFight)
            {
                int hp = refs_.bossFight->GetMaxHp();
                if (ImGui::DragInt("Boss max HP", &hp, 1.0f, 1, 9999))
                {
                    refs_.bossFight->SetMaxHp(hp);
                    MarkDirty();
                    SyncLayoutFromScene();
                }
                ImGui::TextDisabled("Only one boss can be placed.");
            }
        }
    }

    // --- 選択中のもの ---
    ImGui::SeparatorText("Selection");
    switch (selectionKind_)
    {
    case SelectionKind::Enemy:
        if (selectedEnemy_)
        {
            ImGui::Text("Enemy: %s", selectedEnemy_->GetTypeName());

            int typeIndex = static_cast<int>(selectedEnemy_->GetType());
            if (ImGui::Combo("Change type", &typeIndex, kEnemyTypeNames, IM_ARRAYSIZE(kEnemyTypeNames)))
            {
                EnemyType newType = static_cast<EnemyType>(typeIndex);
                if (refs_.enemyManager)
                {
                    // 中で作り直されるので、古いポインタは必ず捨てる
                    MobEnemy* replaced = refs_.enemyManager->ReplaceType(selectedEnemy_, newType);
                    selectedEnemy_ = replaced;
                    if (!replaced) ClearSelection();
                    MarkDirty();
                    SyncLayoutFromScene();
                }
            }

            if (selectedEnemy_)
            {
                int strength = selectedEnemy_->GetStrength();
                if (ImGui::SliderInt("Change strength", &strength, 1, 20))
                {
                    selectedEnemy_->SetStrength(strength);
                    MarkDirty();
                    SyncLayoutFromScene();
                }

                const Vector3& p = selectedEnemy_->GetPosition();
                ImGui::Text("Pos: (%.2f, %.2f, %.2f)", p.x, p.y, p.z);

                if (ImGui::Button("Delete this enemy"))
                {
                    if (refs_.enemyManager)
                    {
                        refs_.enemyManager->Remove(selectedEnemy_);
                        ClearSelection();
                        MarkDirty();
                        SyncLayoutFromScene();
                    }
                }
            }
        }
        break;

    case SelectionKind::Coin:
        if (selectedCoin_)
        {
            const Vector3& p = selectedCoin_->GetPosition();
            ImGui::Text("Coin");
            ImGui::Text("Pos: (%.2f, %.2f, %.2f)", p.x, p.y, p.z);
            if (ImGui::Button("Delete this coin"))
            {
                if (refs_.coinManager)
                {
                    refs_.coinManager->Remove(selectedCoin_);
                    ClearSelection();
                    MarkDirty();
                    SyncLayoutFromScene();
                }
            }
        }
        break;

    case SelectionKind::GrowthCube:
        if (selectedCube_)
        {
            const Vector3& p = selectedCube_->GetPosition();
            ImGui::Text("Growth Cube");
            ImGui::Text("Pos: (%.2f, %.2f, %.2f)", p.x, p.y, p.z);

            float size = selectedCube_->GetBaseSize();
            if (ImGui::DragFloat("Size", &size, 0.01f, 0.2f, 4.0f))
            {
                selectedCube_->SetBaseSize(size);
                MarkDirty();
                SyncLayoutFromScene();
            }

            if (ImGui::Button("Delete this cube"))
            {
                if (refs_.growthCubeManager)
                {
                    refs_.growthCubeManager->Remove(selectedCube_);
                    ClearSelection();
                    MarkDirty();
                    SyncLayoutFromScene();
                }
            }
        }
        break;

    case SelectionKind::Boss:
        if (refs_.bossFight)
        {
            const Vector3 p = refs_.bossFight->GetWorldPosition();
            ImGui::Text("Boss");
            ImGui::Text("Pos: (%.2f, %.2f, %.2f)", p.x, p.y, p.z);

            int hp = refs_.bossFight->GetMaxHp();
            if (ImGui::DragInt("Max HP##sel", &hp, 1.0f, 1, 9999))
            {
                refs_.bossFight->SetMaxHp(hp);
                MarkDirty();
                SyncLayoutFromScene();
            }

            if (!refs_.terrain || !refs_.terrain->HasBossTrigger())
            {
                ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f),
                                   "No terrain has a boss trigger.\n"
                                   "Switch to the Terrain layer and check\n"
                                   "'Boss trigger' on one of the meshes.");
            }

            if (ImGui::Button("Remove boss"))
            {
                refs_.bossFight->SetEnabled(false);
                ClearSelection();
                MarkDirty();
                SyncLayoutFromScene();
            }
        }
        break;

    case SelectionKind::Terrain:
        if (selectedTerrain_ && refs_.terrain)
        {
            ImGui::Text("Terrain: %s", selectedTerrain_->mesh.c_str());
            ImGui::Text("Pos: (%.2f, %.2f, %.2f)",
                        selectedTerrain_->position.x, selectedTerrain_->position.y,
                        selectedTerrain_->position.z);
            ImGui::Text("Bounds XZ: %.1f x %.1f",
                        selectedTerrain_->worldMax.x - selectedTerrain_->worldMin.x,
                        selectedTerrain_->worldMax.z - selectedTerrain_->worldMin.z);

            // XZ は数値でも動かせるようにしておく（微調整用）
            float px = selectedTerrain_->position.x;
            float pz = selectedTerrain_->position.z;
            bool moved = false;
            moved |= ImGui::DragFloat("Pos X", &px, 0.1f, -2000.0f, 2000.0f);
            moved |= ImGui::DragFloat("Pos Z", &pz, 0.1f, -2000.0f, 2000.0f);
            if (moved)
            {
                refs_.terrain->SetPartPositionXZ(selectedTerrain_, px, pz);
                OnTerrainChanged();
            }

            float rot = selectedTerrain_->rotationY;
            if (ImGui::SliderAngle("Rotation Y##sel", &rot, -180.0f, 180.0f))
            {
                refs_.terrain->SetPartRotationY(selectedTerrain_, rot);
                OnTerrainChanged();
            }

            float scale = selectedTerrain_->scale;
            if (ImGui::DragFloat("Scale##sel", &scale, 0.005f, 0.01f, 4.0f))
            {
                refs_.terrain->SetPartScale(selectedTerrain_, scale);
                OnTerrainChanged();
            }

            bool trigger = selectedTerrain_->bossTrigger;
            if (ImGui::Checkbox("Boss trigger (start boss fight here)", &trigger))
            {
                refs_.terrain->SetPartBossTrigger(selectedTerrain_, trigger);
                MarkDirty();
                SyncLayoutFromScene();
            }

            if (ImGui::Button("Delete this terrain"))
            {
                StageTerrain::Part* victim = selectedTerrain_;
                ClearSelection();
                refs_.terrain->RemovePart(victim);
                OnTerrainChanged();
            }
        }
        break;

    case SelectionKind::PlayerStart:
        if (PlayerSlime())
        {
            const Vector3& p = PlayerSlime()->GetPosition();
            ImGui::Text("Player Start");
            ImGui::Text("Pos: (%.2f, %.2f, %.2f)", p.x, p.y, p.z);
        }
        break;

    default:
        ImGui::TextDisabled("Nothing selected");
        break;
    }

    // --- 地形の一覧（Terrain レイヤーのときだけ）---
    if (layer_ == EditLayer::Terrain && refs_.terrain)
    {
        ImGui::SeparatorText("Terrain list");
        ImGui::Text("Parts: %d", refs_.terrain->GetPartCount());

        if (ImGui::BeginChild("terrain_list", ImVec2(0.0f, 130.0f), true))
        {
            const auto& parts = refs_.terrain->GetParts();
            for (int i = 0; i < static_cast<int>(parts.size()); ++i)
            {
                StageTerrain::Part* part = parts[static_cast<size_t>(i)].get();
                if (!part) continue;

                char label[160];
                std::snprintf(label, sizeof(label), "%d: %s%s##terrain%d",
                              i, part->mesh.c_str(), part->bossTrigger ? "  [BOSS]" : "", i);

                if (ImGui::Selectable(label, part == selectedTerrain_))
                {
                    ClearSelection();
                    selectionKind_ = SelectionKind::Terrain;
                    selectedTerrain_ = part;

                    // 選んだパーツを画面の真ん中へ持ってくる
                    const Vector3 handle = part->HandlePosition();
                    camX_ = handle.x;
                    camZ_ = handle.z;
                }
            }
        }
        ImGui::EndChild();

        if (ImGui::Button("Reset terrain to default"))
        {
            layout_.terrain = StageLayout::MakeDefaultTerrain();
            refs_.terrain->ApplyLayout(layout_.terrain);
            ClearSelection();
            OnTerrainChanged();
            std::snprintf(statusText_, sizeof(statusText_), "Terrain reset to default layout");
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear terrain"))
        {
            refs_.terrain->ClearParts();
            ClearSelection();
            OnTerrainChanged();
            std::snprintf(statusText_, sizeof(statusText_), "All terrain removed");
        }
    }

    // --- カーソル情報 ---
    ImGui::SeparatorText("Cursor");
    if (hasCursor_)
    {
        ImGui::Text("(%.2f, %.2f)  %s", lastCursorWorld_.x, lastCursorWorld_.z,
                    StagePlacement::ResultLabel(lastCursorResult_));
    }
    else
    {
        ImGui::TextDisabled("outside window");
    }

    // --- 表示 ---
    ImGui::SeparatorText("View");
    ImGui::Checkbox("Show blocked area", &showOverlay_);
    ImGui::SameLine();
    ImGui::Checkbox("Translucent ground", &translucentGround_);
    ImGui::SliderFloat("Ground alpha", &groundAlpha_, 0.05f, 1.0f);
    ImGui::ColorEdit4("Blocked color", &forbiddenColor_.x);

    ImGui::DragFloat("Zoom (camera height)", &camHeight_, 0.5f, camHeightMin_, camHeightMax_);
    if (ImGui::Button("Frame All"))
    {
        FrameAll();
    }

    ImGui::DragFloat("Cell size", &overlayCellSize_, 0.05f, 0.25f, 5.0f);
    ImGui::SameLine();
    if (ImGui::Button("Rebuild blocked area"))
    {
        RebuildOverlay();
    }
    ImGui::Text("Blocked cells: %d / Base height: %.2f", overlayCellCount_, planeY_);

    // --- ファイル ---
    ImGui::SeparatorText("File");
    ImGui::Text("File: %s", layoutPath_.c_str());
    ImGui::Text("Terrain: %d / Enemies: %d / Coins: %d",
                static_cast<int>(layout_.terrain.size()),
                static_cast<int>(layout_.enemies.size()),
                static_cast<int>(layout_.coins.size()));
    ImGui::Text("Cubes: %d / Boss: %s%s",
                static_cast<int>(layout_.growthCubes.size()),
                layout_.boss.enabled ? "yes" : "no",
                isDirty_ ? "   * unsaved changes" : "");

    if (ImGui::Button("Save"))
    {
        SyncLayoutFromScene();
        Save();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload"))
    {
        Load();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear All"))
    {
        // 地形は消さない（Terrain レイヤーの "Clear terrain" で消す）。
        // 地形まで消えると置ける場所が無くなって復帰しづらい
        if (refs_.enemyManager)      refs_.enemyManager->ClearAll();
        if (refs_.coinManager)       refs_.coinManager->ClearAll();
        if (refs_.growthCubeManager) refs_.growthCubeManager->ClearAll();
        if (refs_.bossFight)         refs_.bossFight->SetEnabled(false);
        ClearSelection();
        MarkDirty();
        SyncLayoutFromScene();
    }

    if (ImGui::Button("Auto-place from terrain (fallback)"))
    {
        StageLayout generated = StageLayout::MakeFallback();
        generated.playerStart = layout_.playerStart;
        // 地形は「いま置いてあるもの」を正とする。
        // MakeFallback() は地形をレイキャストして敵とコインの位置を決めるだけで、
        // 地形自体は作らない（作ってしまうと足元が消える）
        generated.terrain = layout_.terrain;
        generated.boss = layout_.boss;
        layout_ = generated;
        ApplyLayoutToScene();
        if (refs_.enemyManager)
        {
            for (const auto& e : refs_.enemyManager->GetEnemies())
            {
                if (e) e->SetFrozen(true);
            }
        }
        MarkDirty();
        SyncLayoutFromScene();
    }

    if (statusText_[0] != '\0')
    {
        ImGui::Separator();
        ImGui::TextWrapped("%s", statusText_);
    }

    ImGui::End();
#endif
}
