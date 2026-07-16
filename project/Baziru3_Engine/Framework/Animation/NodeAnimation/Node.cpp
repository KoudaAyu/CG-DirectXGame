#include "Node.h"

Node::Node(const std::string& name) : name_(name)
{
}

void Node::AddChild(std::unique_ptr<Node> child)
{
    children_.push_back(std::move(child));
}

void Node::Update(const Matrix4x4& parentWorld)
{
    // Transform の内部を最新化してからローカル行列を作成
    transform_.TransferMatrix();

    localMatrix_ = MakeAffineMatrix(transform_.GetScale(), transform_.GetRotate(), transform_.GetTranslate());

    worldMatrix_ = Multiply(parentWorld, localMatrix_);

    for (auto& c : children_)
    {
        if (c)
        {
            c->Update(worldMatrix_);
        }
    }
}
