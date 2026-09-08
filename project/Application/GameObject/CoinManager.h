#pragma once

#include <memory>
#include <vector>

#include "Application/GameObject/Coin.h"

class Object3dCom;
class Camera;
class PikminPlayer;

/// @brief コインの見た目・挙動の設定（ImGui から調整する用）
struct CoinConfig
{
    // --- メッシュ（プロシージャル生成）---
    // モデルファイルを使いたくなったら modelDirectory / modelFileName を埋める。
    // 空のままならこの寸法で円盤メッシュを自動生成する
    const char* modelDirectory = "";
    const char* modelFileName = "";
    float radius = 0.45f;        //!< コインの半径 (m)
    float thickness = 0.10f;     //!< コインの厚み (m)
    int   segments = 24;         //!< 円周の分割数

    // --- 見た目 ---
    Vector4 color{ 1.0f, 0.84f, 0.25f, 1.0f }; //!< 金色
    float heightOffset = 0.55f;  //!< 床からの浮かせ量 (m)
    float spinSpeed = 2.6f;      //!< 自転速度 (rad/s)
    float bobHeight = 0.12f;     //!< 上下ふわふわの振幅 (m)
    float bobSpeed = 2.2f;       //!< 上下ふわふわの速さ

    // --- 取得判定 ---
    float collectRadius = 0.9f;  //!< プレイヤーの見た目半径に加算される取得半径 (m)
    float collectHeight = 2.5f;  //!< 高さ方向の許容差 (m)。上下段の取り違えを防ぐ
};

/**
 * @brief コインをまとめて管理する
 *
 * 生成・更新・描画・取得判定・カウント。配置データ（StageLayout）から流し込んで使う。
 */
class CoinManager
{
public:
    CoinManager() = default;
    ~CoinManager();

    void Initialize(Object3dCom* object3dCom, Camera* camera);
    void Finalize();

    /// @brief コインを1枚置く
    Coin* Spawn(const Vector3& stageLocalPos);

    /// @brief 指定のコインを取り除く（エディタの右クリック削除用）
    void Remove(Coin* coin);

    void ClearAll();

    /**
     * @brief 更新
     * @param deltaTime デルタタイム
     * @param stageTilt ステージ傾斜
     * @param player プレイヤー（nullptr なら取得判定をしない）
     */
    void Update(float deltaTime, const Vector2& stageTilt, PikminPlayer* player);

    void Draw(const RenderContext& ctx);

    /// @brief ImGui デバッグパネル（USE_IMGUI 無効時は何もしない）
    void DrawImGui();

    /**
     * @brief エディタモード。true の間は取得判定をしない（置いた瞬間に消えないように）
     */
    void SetEditorMode(bool on) { editorMode_ = on; }
    bool IsEditorMode() const { return editorMode_; }

    /// @brief 設定を書き換えたあと、見た目に反映する（メッシュも作り直す）
    void RefreshFromConfig();

    int GetTotalCount() const { return static_cast<int>(coins_.size()); }
    int GetCollectedCount() const { return collectedCount_; }
    int GetRemainingCount() const { return GetTotalCount() - collectedCount_; }

    const std::vector<std::unique_ptr<Coin>>& GetCoins() const { return coins_; }

    static CoinConfig& GetConfig();

private:
    void BuildMesh();

private:
    Object3dCom* object3dCom_ = nullptr;
    Camera* camera_ = nullptr;

    Object3d::ModelData modelData_;
    uint32_t textureIndex_ = 0;
    bool meshReady_ = false;

    std::vector<std::unique_ptr<Coin>> coins_;
    int collectedCount_ = 0;
    bool editorMode_ = false;
};
