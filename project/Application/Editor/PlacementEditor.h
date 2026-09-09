#pragma once

#include <d3d12.h>

#include <memory>
#include <string>
#include <vector>

#include "Baziru3_Engine/Core/Base/Vector.h"
#include "Baziru3_Engine/Core/Base/RenderContext.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3d.h"
#include "Application/Editor/StageLayout.h"
#include "Application/GameObject/StageTerrain.h"

class Object3dCom;
class Camera;
class EnemyManager;
class CoinManager;
class GrowthCubeManager;
class MobEnemy;
class Coin;
class GrowthCube;
class Slime;
class SlimeManager;
class BossFight;

/**
 * @brief ステージ配置エディタ（地形・敵・コイン・成長キューブ・ボス・プレイヤー初期位置）
 *
 * GamePlayScene の中のモードとして動く。真上からの見下ろしカメラに切り替えて、
 * 2Dマップにものを置く感覚で配置する。操作は全部マウス。
 *
 *   左クリック（ドラッグなし）
 *     - 何も無いところ -> 今のブラシを配置
 *     - 置いてあるもの -> 選択（パラメータのパネルが出る）
 *   左ドラッグ
 *     - 置いてあるものの上から -> そのオブジェクトを移動
 *     - 何も無いところから    -> マップをパン（スクロール）
 *   右クリック
 *     - 置いてあるもの -> 削除
 *     - 何も無いところ -> 選択解除
 *   ホイール
 *     - カーソル位置を中心にズームイン／アウト
 *
 * ## レイヤー（Objects / Terrain）
 *
 * 地形メッシュは島まるごとの大きさがあるので、敵やコインと同じ土俵でピッキングすると
 * 何を掴んでいるのか分からなくなる。そこで**編集レイヤー**を分けてある。
 *
 *   Objects レイヤー: 敵・コイン・成長キューブ・ボス・プレイヤー初期位置だけを触る
 *   Terrain レイヤー: 地形メッシュだけを触る。各パーツの中心に「ハンドル」が出る
 *
 * 地形パーツは XZ しか動かせない（Y は JSON を手で書けば効く）。
 * 選択中のパーツは Y 軸回転とスケールを ImGui から変えられる。
 * 「ボス戦トリガー」もパーツ単位で設定する。
 *
 * @note マウス入力は ImGui の IO から取る。
 *       ImGuiManager がウィンドウサイズによらず仮想解像度 1280x720 に
 *       正規化してくれているので、MouseInput::GetScaledPosition() の
 *       ウィンドウ拡大時のズレ問題を踏まずに済む。
 *       その代わり F3 で ImGui を隠すとエディタ操作も止まる。
 *
 * @note 配置データの正は layout_（StageLayout）側。
 *       エディタを抜けるときに layout_ をシーンへ流し直すので、
 *       プレイ中に敵が倒されたりコインが取られたりしても保存内容は汚れない。
 */
class PlacementEditor
{
public:
    /// @brief エディタが触るシーン側のオブジェクト
    struct SceneRefs
    {
        Object3dCom* object3dCom = nullptr;
        Camera* camera = nullptr;
        EnemyManager* enemyManager = nullptr;
        CoinManager* coinManager = nullptr;
        GrowthCubeManager* growthCubeManager = nullptr;
        // スライム群。プレイヤー開始位置の反映・読み取りは
        // 「一番大きい個体（GetLeader）」を代表として行う。
        // 合体でスライムの実体が破棄されるので、Slime* を持ち越さず毎回引き直すこと
        SlimeManager* slimeManager = nullptr;
        // 地形メッシュ群。エディタが直接いじる
        StageTerrain* terrain = nullptr;
        // ボス。配置座標と最大HPだけを触る
        BossFight* bossFight = nullptr;
    };

    /// @brief 編集レイヤー。掴めるもの・置けるものが切り替わる
    enum class EditLayer
    {
        Objects, //!< 敵・コイン・成長キューブ・ボス・プレイヤー初期位置
        Terrain, //!< 地形メッシュ
    };

    /// @brief 今おいているもの（ブラシ）
    enum class Brush
    {
        Enemy,
        Coin,
        GrowthCube,
        Boss,
        PlayerStart,
    };

    /// @brief 選択中のもの
    enum class SelectionKind
    {
        None,
        Enemy,
        Coin,
        GrowthCube,
        Boss,
        PlayerStart,
        Terrain,
    };

public:
    PlacementEditor() = default;
    ~PlacementEditor();

    void Initialize(const SceneRefs& refs);
    void Finalize();

    // --- モード ---
    /// @brief エディタの開始／終了。終了時に自動保存する
    void SetActive(bool active);
    bool IsActive() const { return isActive_; }

    // --- 毎フレーム ---
    /// @brief マウス処理とカメラ更新。IsActive() が false なら何もしない
    void Update(float deltaTime);

    /// @brief オーバーレイ（配置禁止領域）と選択マーカーの描画
    void Draw(const RenderContext& ctx);

    /// @brief 地形を半透明で描く（オーバーハング越しに下が見えるようにする）
    /// @return 半透明で描いた場合 true。false なら呼び出し側が通常描画する
    bool DrawGroundTranslucent(const RenderContext& ctx, Object3d* ground,
                               const Object3d::ModelData& modelData);

    void DrawImGui();

    // --- 配置データ ---
    const StageLayout& GetLayout() const { return layout_; }

    /// @brief 配置データを差し替えてシーンへ流し込む（シーン初期化時に使う）
    void SetLayout(const StageLayout& layout);

    /// @brief layout_ をシーン（地形 / 敵 / コイン / キューブ / ボス / プレイヤー）へ反映する
    void ApplyLayoutToScene();

    /// @brief シーンの現状を layout_ へ吸い出す（エディタ中のみ意味がある）
    void SyncLayoutFromScene();

    bool Save();
    bool Load();
    bool IsDirty() const { return isDirty_; }

    const std::string& GetLayoutPath() const { return layoutPath_; }
    void SetLayoutPath(const std::string& path) { layoutPath_ = path; }

    /// @brief 配置禁止領域のオーバーレイメッシュを作り直す（地形を差し替えたあと用）
    void RebuildOverlay();

    /// @brief 地形全体が画面に収まる位置までカメラを引く
    void FrameAll();

    /// @brief 地形の元の色を教える（半透明描画から戻すときに使う）
    void SetGroundBaseColor(const Vector4& color) { groundBaseColor_ = color; }

private:
    /// @brief 配置エディタが「プレイヤー」として扱うスライム（＝群れの代表）
    /// @note 合体でスライムの実体が破棄されるので、Slime* を保持せず必ずここから引き直す
    Slime* PlayerSlime() const;

    // --- マウス / 座標 ---
    struct MouseState
    {
        bool valid = false;
        Vector2 screen{ 0.0f, 0.0f };   //!< 仮想解像度 1280x720 上の座標
        Vector3 world{ 0.0f, 0.0f, 0.0f }; //!< カーソル下のワールド座標
        float wheel = 0.0f;
        bool overUI = false;
    };

    /// @brief 画面座標 -> 地形上のワールド座標（真上カメラ前提）
    Vector3 ScreenToWorld(const Vector2& screen) const;

    /// @brief 画面座標 -> ワールド座標（カメラ位置を指定して計算。ズーム補正用）
    Vector3 ScreenToWorldAt(const Vector2& screen, float camX, float camZ, float camHeight) const;

    void UpdateCamera();
    void HandleMouse(const MouseState& mouse);

    // --- ピッキング ---
    /// @brief 掴んだもの1つぶん
    struct PickResult
    {
        SelectionKind kind = SelectionKind::None;
        MobEnemy* enemy = nullptr;
        Coin* coin = nullptr;
        GrowthCube* cube = nullptr;
        StageTerrain::Part* terrain = nullptr;
    };

    MobEnemy* PickEnemy(const Vector3& world, float* outDistSq = nullptr) const;
    Coin* PickCoin(const Vector3& world, float* outDistSq = nullptr) const;
    GrowthCube* PickGrowthCube(const Vector3& world, float* outDistSq = nullptr) const;
    bool PickBoss(const Vector3& world, float* outDistSq = nullptr) const;
    bool PickPlayer(const Vector3& world, float* outDistSq = nullptr) const;

    /// @brief 地形パーツのハンドル（ワールド AABB の中心）を掴む
    StageTerrain::Part* PickTerrain(const Vector3& world, float* outDistSq = nullptr) const;

    /// @brief 今のレイヤーで掴めるものを全部試して、一番近いものを返す
    PickResult PickAny(const Vector3& world) const;

    // --- 配置操作 ---
    void PlaceAt(const Vector3& world);
    void DeleteAt(const Vector3& world);
    void MoveSelectionTo(const Vector3& world);
    void ClearSelection();
    void MarkDirty() { isDirty_ = true; }

    /// @brief 地形が変わったので、配置判定まわりを作り直す
    void OnTerrainChanged();

    // --- 描画ヘルパー ---
    void BuildMarkerMesh();
    void DrawMarker(const RenderContext& ctx, const Vector3& worldPos, float radius, const Vector4& color);
    void DrawWithPipeline(const RenderContext& ctx, Object3d* object,
                          ID3D12PipelineState* pipelineState);

private:
    SceneRefs refs_{};
    bool isActive_ = false;
    bool isDirty_ = false;

    StageLayout layout_;
    std::string layoutPath_ = StageLayout::kDefaultPath;

    // --- カメラ ---
    float camX_ = 0.0f;          //!< 画面中心が見ているワールド X
    float camZ_ = 0.0f;          //!< 画面中心が見ているワールド Z
    float camHeight_ = 60.0f;    //!< 投影面からの高さ。これがズーム量
    float camHeightMin_ = 6.0f;
    float camHeightMax_ = 400.0f;
    float planeY_ = 0.0f;        //!< 見下ろしの基準平面（プレイ面の代表的な高さ）

    // エディタに入る前のカメラ設定（戻すために退避しておく）
    Vector3 savedCamPos_{ 0.0f, 0.0f, 0.0f };
    Vector3 savedCamRot_{ 0.0f, 0.0f, 0.0f };
    float savedFovY_ = 0.85f;
    bool hasSavedCamera_ = false;

    // --- マウス状態 ---
    bool leftDown_ = false;
    bool pressBlocked_ = false;      //!< 押した瞬間が ImGui ウィンドウの上／画面外だった。この押下は最後まで無視する
    bool dragStartedOnObject_ = false;
    bool isPanning_ = false;
    bool isDraggingObject_ = false;
    bool movedWhileDown_ = false;
    Vector2 pressScreen_{ 0.0f, 0.0f };
    Vector3 panAnchorWorld_{ 0.0f, 0.0f, 0.0f };

    /// @brief 地形パーツを掴んだときの「ハンドルとカーソルのずれ」。
    ///        これを保持しないと、掴んだ瞬間にパーツが中心へ飛ぶ
    Vector3 terrainDragOffset_{ 0.0f, 0.0f, 0.0f };

    // --- 選択 / ブラシ ---
    EditLayer layer_ = EditLayer::Objects;
    SelectionKind selectionKind_ = SelectionKind::None;
    MobEnemy* selectedEnemy_ = nullptr;
    Coin* selectedCoin_ = nullptr;
    GrowthCube* selectedCube_ = nullptr;
    StageTerrain::Part* selectedTerrain_ = nullptr;

    Brush brush_ = Brush::Enemy;
    EnemyType brushEnemyType_ = EnemyType::Slime;
    int brushStrength_ = -1;   //!< -1 なら種類ごとの範囲からランダム
    float brushCubeSize_ = 0.85f;

    // --- 地形ブラシ ---
    int terrainMeshIndex_ = 0;      //!< カタログ内の番号
    float terrainNewRotationY_ = 0.0f;
    float terrainNewScale_ = 0.25f;
    bool terrainNewBossTrigger_ = false;

    // --- 配置禁止オーバーレイ ---
    std::unique_ptr<Object3d> overlayObject_;
    Object3d::ModelData overlayModel_;
    float overlayCellSize_ = 2.0f;
    bool showOverlay_ = true;
    int overlayCellCount_ = 0;
    bool overlayReady_ = false;

    // --- 選択マーカー（薄いリング）---
    std::unique_ptr<Object3d> markerObject_;
    Object3d::ModelData markerModel_;
    bool markerReady_ = false;

    // --- 見た目の設定 ---
    bool translucentGround_ = true;
    float groundAlpha_ = 0.45f;
    Vector4 groundBaseColor_{ 0.55f, 0.85f, 0.50f, 1.0f }; //!< 地形の元の色（半透明描画のあとに戻す）
    Vector4 forbiddenColor_{ 1.0f, 0.25f, 0.20f, 0.35f };
    Vector4 selectionColor_{ 0.20f, 0.95f, 1.00f, 0.85f };
    Vector4 cursorOkColor_{ 0.30f, 1.00f, 0.45f, 0.70f };
    Vector4 cursorNgColor_{ 1.00f, 0.30f, 0.25f, 0.70f };
    Vector4 terrainHandleColor_{ 0.85f, 0.75f, 0.25f, 0.55f };
    Vector4 terrainTriggerColor_{ 1.00f, 0.35f, 0.85f, 0.75f };
    float zoomSpeed_ = 0.12f;
    bool showTerrainHandles_ = true;

    // --- 直近のカーソル情報（ImGui 表示用）---
    Vector3 lastCursorWorld_{ 0.0f, 0.0f, 0.0f };
    StagePlacement::Result lastCursorResult_ = StagePlacement::Result::NoGround;
    bool hasCursor_ = false;
    char statusText_[128] = { 0 };
};
