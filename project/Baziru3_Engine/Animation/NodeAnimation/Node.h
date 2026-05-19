#pragma once

#include <string>
#include <vector>
#include <memory>

#include "Matrix4x4.h"
#include "Transform.h"

class Node
{
public:
    explicit Node(const std::string& name = "");
    ~Node() = default;

    // ノード階層に子を追加（所有権移動）
    void AddChild(std::unique_ptr<Node> child);

    // 親ワールド行列を受け取って更新（再帰）
    void Update(const Matrix4x4& parentWorld = MakeIdentity4x4());

    // アクセサ
    const Matrix4x4& GetLocalMatrix() const { return localMatrix_; }
    const Matrix4x4& GetWorldMatrix() const { return worldMatrix_; }
    const std::string& GetName() const { return name_; }
    Transform& GetTransform() { return transform_; }

private:
    std::string name_;
    Transform transform_;

    Matrix4x4 localMatrix_{};
    Matrix4x4 worldMatrix_{};

    std::vector<std::unique_ptr<Node>> children_;
};
