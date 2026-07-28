#include "Collider.h"
#include "CollisionManager.h"

Collider::Collider(ColliderType type, CollisionAttribute attribute)
    : type_(type)
    , attribute_(attribute)
    , positionOffset_({ 0.0f, 0.0f, 0.0f })
    , isTrigger_(false)
    , isEnabled_(true)
{
    CollisionManager::GetInstance()->RegisterCollider(this);
}

Collider::~Collider()
{
    if (CollisionManager::GetInstance())
    {
        CollisionManager::GetInstance()->UnregisterCollider(this);
    }
}
