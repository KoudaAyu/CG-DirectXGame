#pragma once

#include "Baziru3_Engine/Graphics/3D/Object/Object3d.h"

/**
 * @brief スライム用の高分割UVスフィアメッシュをプロシージャルに生成するユーティリティ
 */
namespace SlimeMesh
{
    /**
     * @brief UVスフィアの ModelData を生成する
     * @param sliceCount  経度方向の分割数（デフォルト32）
     * @param stackCount  緯度方向の分割数（デフォルト16）
     * @param radius      球の半径（デフォルト1.0f）
     * @return Object3d::ModelData 形式のメッシュデータ
     */
    Object3d::ModelData GenerateSphere(uint32_t sliceCount = 64, uint32_t stackCount = 32, float radius = 1.0f);

    /**
     * @brief 影用のフラット円盤メッシュを生成する
     * @param sliceCount 分割数（デフォルト24）
     * @param radius     円盤の半径（デフォルト1.0f）
     * @return Object3d::ModelData 形式のメッシュデータ
     * @note 頂点カラーは使わず、テクスチャ座標で中心→端の放射グラデーションを表現
     */
    Object3d::ModelData GenerateDisc(uint32_t sliceCount = 24, float radius = 1.0f);
}

class Object3dCom;
class Camera;

/**
 * @brief スライム・敵キャラクター足元の丸影（ドロップシャドウ）描画コンポーネント
 */
class CharacterShadow
{
public:
    CharacterShadow() = default;
    ~CharacterShadow() = default;

    void Initialize(Object3dCom* object3dCom, Camera* camera);
    void Update(const Vector3& charWorldPos, float shadowRadius, float charFootOffset,
                const Vector2& stageTilt, const Vector2& pivot = { 0.0f, 0.0f });
    void Draw(const RenderContext& ctx);

    bool HasGround() const { return hasGround_; }

private:
    Object3dCom* object3dCom_ = nullptr;
    Camera* camera_ = nullptr;
    std::unique_ptr<Object3d> object_;
    Object3d::ModelData modelData_;
    uint32_t textureIndex_ = 0;
    bool hasGround_ = false;
};
