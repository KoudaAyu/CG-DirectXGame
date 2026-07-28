#pragma once
#include "Collider.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3d.h"
#include <unordered_map>
#include <string>

class SkeletonCollider : public Collider
{
public:
    SkeletonCollider(Object3d* object3d, CollisionAttribute attribute, float defaultRadius = 0.25f)
        : Collider(ColliderType::Skeleton, attribute)
        , object3d_(object3d)
        , defaultRadius_(defaultRadius)
    {}

    virtual ~SkeletonCollider() override = default;

    virtual Vector3 GetWorldPosition() const override
    {
        return object3d_ ? object3d_->GetTranslate() : Vector3{ 0.0f, 0.0f, 0.0f };
    }

    virtual void SetWorldPosition(const Vector3& pos) override
    {
        if (object3d_)
        {
            object3d_->SetTranslate(pos);
        }
    }

    Object3d* GetObject3d() const { return object3d_; }
    float GetDefaultRadius() const { return defaultRadius_; }
    void SetDefaultRadius(float radius) { defaultRadius_ = radius; }

    void SetJointRadius(const std::string& jointName, float radius)
    {
        jointRadii_[jointName] = radius;
    }

    float GetJointRadius(const std::string& jointName) const
    {
        auto it = jointRadii_.find(jointName);
        if (it != jointRadii_.end())
        {
            return it->second;
        }
        return defaultRadius_;
    }

private:
    Object3d* object3d_ = nullptr;
    float defaultRadius_ = 0.25f;
    std::unordered_map<std::string, float> jointRadii_;
};
