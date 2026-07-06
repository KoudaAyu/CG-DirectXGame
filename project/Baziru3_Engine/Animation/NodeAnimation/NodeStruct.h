#pragma once
#pragma once

#include <string>
#include <vector>

#include "../../Base/Matrix4x4.h"
#include "../../Base/Vector.h"
#include"Quaternion.h"


namespace AnimNodeData
{
    // 軽量なトランスフォーム（スケール・回転(Quaternion)・平行移動）
    struct QuaternionTransform
    {
        Vector3 scale{1.0f, 1.0f, 1.0f};
        Quaternion rotate{}; // 単位クォータニオンがデフォルト
        Vector3 translate{0.0f, 0.0f, 0.0f};

        // ローカル行列を作成
        Matrix4x4 MakeLocalMatrix() const
        {
            return MakeAffineMatrix(scale, rotate, translate);
        }
    };

    // スライドにあるようなシンプルな値型 Node
    struct Node
    {
        QuaternionTransform transform;
        Matrix4x4 localMatrix{};
        std::string name;
        std::vector<Node> children;
    };

    // ヘルパー: 分解した S, R(quaternion), T から Node を作る。
    // Assimp 等で aiNode::mTransformation.Decompose(scale, rotate, translate) を使った後に
    // この関数を呼ぶ想定。右手系->左手系の変換を行う。
    inline Node MakeNodeFromComponents(const std::string& name,
                                      const Vector3& scale,
                                      const Quaternion& rotateFromImporter,
                                      const Vector3& translateFromImporter)
    {
        Node result;
        result.name = name;

        // scale はそのまま
        result.transform.scale = scale;

        // 回転はインポーター(Assimp)の座標系と本エンジンの座標系の違いに合わせて
        // X軸のみ反転ではなく、スライドに合わせて Y,Z を反転している実装
        result.transform.rotate.x = rotateFromImporter.x;
        result.transform.rotate.y = -rotateFromImporter.y;
        result.transform.rotate.z = -rotateFromImporter.z;
        result.transform.rotate.w = rotateFromImporter.w;

        // 平行移動はX,Y軸を反転（スライドの例に従う）
        result.transform.translate.x = -translateFromImporter.x;
        result.transform.translate.y = -translateFromImporter.y;
        result.transform.translate.z = translateFromImporter.z;

        // localMatrix を再構築
        result.localMatrix = result.transform.MakeLocalMatrix();

        return result;
    }
}

// Global alias for convenience: use `AnimNode` to refer to the lightweight animation Node
// This avoids colliding with other `Node` types in the project while keeping a short name.
using AnimNode = AnimNodeData::Node;
