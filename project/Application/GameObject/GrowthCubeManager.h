#pragma once

#include <memory>
#include <vector>

#include "Application/GameObject/GrowthCube.h"

class Object3dCom;
class Camera;
class SlimeManager;

/**
 * @brief 成長キューブをまとめて管理する
 *
 * CoinManager と同じ作り。生成・更新・描画・取得イベント・ImGui。
 * 配置データ（StageLayout::growthCubes）から流し込んで使う。
 */
class GrowthCubeManager
{
public:
    GrowthCubeManager() = default;
    ~GrowthCubeManager();

    void Initialize(Object3dCom* object3dCom, Camera* camera);
    void Finalize();

    /// @brief キューブを1個置く
    GrowthCube* Spawn(const Vector3& stageLocalPos, float size = 0.85f);

    /// @brief 指定のキューブを取り除く（エディタの右クリック削除用）
    void Remove(GrowthCube* cube);

    void ClearAll();

    /**
     * @brief 更新
     * @param slimeManager スライム群（nullptr なら取得判定をしない）
     * @note ロコロコ準拠で「どのスライムでも食べられる」
     */
    void Update(float deltaTime, const Vector2& stageTilt, const Vector2& pivot, SlimeManager* slimeManager);

    void Draw(const RenderContext& ctx);

    /// @brief ImGui デバッグパネル（USE_IMGUI 無効時は何もしない）
    void DrawImGui();

    /// @brief エディタモード。true の間は取得判定をしない（置いた瞬間に消えないように）
    void SetEditorMode(bool on) { editorMode_ = on; }
    bool IsEditorMode() const { return editorMode_; }

    /// @brief 全部を再出現させる（R キーのリスタート用）
    void RespawnAll();

    int GetTotalCount() const { return static_cast<int>(cubes_.size()); }
    int GetCollectedCount() const { return collectedCount_; }

    const std::vector<std::unique_ptr<GrowthCube>>& GetCubes() const { return cubes_; }

    /**
     * @brief このフレームに食べられたキューブのワールド座標
     * @note Update() の頭でクリアされる。演出と SE のトリガに使う
     */
    const std::vector<Vector3>& GetCollectEvents() const { return collectEvents_; }

private:
    Object3dCom* object3dCom_ = nullptr;
    Camera* camera_ = nullptr;

    std::vector<std::unique_ptr<GrowthCube>> cubes_;
    std::vector<Vector3> collectEvents_;
    int collectedCount_ = 0;
    bool editorMode_ = false;
};
