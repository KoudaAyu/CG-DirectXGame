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
}
