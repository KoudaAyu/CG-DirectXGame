#pragma once

#include <vector>

class Collider;
class Camera;

/// <summary>
/// コライダーの3Dデバッグワイヤーフレーム描画
/// </summary>
namespace CollisionDebugDraw
{
    /// <summary>
    /// 全コライダーのワイヤーフレームを描画
    /// </summary>
    /// <param name="colliders">コライダーのリスト</param>
    /// <param name="camera">カメラ</param>
    void Draw(const std::vector<Collider*>& colliders, Camera* camera);
}
