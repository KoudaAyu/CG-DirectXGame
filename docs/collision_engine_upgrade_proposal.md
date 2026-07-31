# コリジョンシステムのエンジン層移行に伴う機能拡張設計提案

## 1. 背景と目的

現在、アプリケーション層の [CollisionSystem.cpp](file:///c:/Users/k024g/OneDrive/デスクトップ/Engine_ver2026/project/Application/Scene/GameScene/CollisionSystem.cpp) には、エンジン側の `CollisionManager` のコライダー情報を直接取得し、コライダーの型（SphereやCapsule）ごとにキャストして泥臭いベクトル演算や線衝突判定を行うコードが数多く手書きされています。

これらは本来、ゲームエンジン（物理エンジン層）に隠蔽されるべき低レベルな幾何学計算です。これらをエンジン側の `CollisionManager` に移行し、共通APIとして抽象化することで、アプリケーション層を極限までシンプルにし、バグの温床を防ぎます。

---

## 2. 拡張設計案

### 2.1 レイキャスト機能の統合 (`CollisionManager::Raycast`)

現在、弾丸と障害物の衝突判定（`ResolveObstacleCollisions`）では、アプリ側で登録コライダーを全走査し、`ColliderType` に応じてパラメータを詰め替えて `CheckRayCollider` を手動ループで呼び出しています。

これを解決するため、エンジン側に光線衝突の判定用の共通構造体とAPIを追加します。

#### 1. 共通構造体の定義 (`CollisionManager.h`)
```cpp
struct Ray
{
    Vector3 origin;    // 始点
    Vector3 direction; // 方向（正規化されたベクトル）
};

struct RaycastHit
{
    Collider* collider = nullptr;      // 衝突したコライダー
    Vector3 position = {0.0f,0.0f,0.0f}; // 衝突した交点座標
    Vector3 normal = {0.0f,0.0f,0.0f};   // 衝突した面の法線
    float distance = 0.0f;             // 始点からの距離
};
```

#### 2. `CollisionManager` へのAPI追加
```cpp
class CollisionManager
{
public:
    // 登録されている全コライダーに対してレイを飛ばし、最も近い衝突情報を取得する
    bool Raycast(const Ray& ray, float maxDistance, RaycastHit* outHit);
};
```

#### 3. 実装のイメージ (`CollisionManager.cpp`)
```cpp
bool CollisionManager::Raycast(const Ray& ray, float maxDistance, RaycastHit* outHit)
{
    bool hitAny = false;
    float closestDist = maxDistance;
    RaycastHit tempHit;

    for (Collider* col : colliders_)
    {
        if (!col || !col->IsEnabled() || col->GetAttribute() != CollisionAttribute::Obstacle)
        {
            continue;
        }

        // コライダーのタイプに応じた形状情報の構築と、レイ判定
        CollisionData data;
        data.originalCollider = col;
        data.type = col->GetType();
        data.attribute = col->GetAttribute();
        data.worldPosition = col->GetWorldPosition();

        if (data.type == ColliderType::Sphere)
        {
            data.shape.radius = static_cast<SphereCollider*>(col)->GetRadius();
        }
        else if (data.type == ColliderType::Capsule)
        {
            data.shape.radius = static_cast<CapsuleCollider*>(col)->GetRadius();
            data.shape.height = static_cast<CapsuleCollider*>(col)->GetHeight();
        }
        // ※ その他の形状もここでバインド

        float dist = 0.0f;
        if (CheckRayCollider(ray.origin, ray.direction, closestDist, data, dist))
        {
            closestDist = dist;
            tempHit.collider = col;
            tempHit.position = ray.origin + ray.direction * dist;
            tempHit.distance = dist;
            hitAny = true;
        }
    }

    if (hitAny && outHit)
    {
        *outHit = tempHit;
    }
    return hitAny;
}
```

---

### 2.2 自動めり込み補正 (Penetration Resolution) の統合

現在、キャラクターと障害物の押し戻し処理（`ResolveCharacterObstacleCollisions`）は、アプリ側で「めり込んだ距離と押し出し方向」を毎フレーム手動で計算して座標を直書きで書き換えています。

これを解決するため、コライダー自体に物理的な動的属性を持たせ、エンジン側で自動解決するようにします。

#### 1. コライダーの静的・動的属性の追加 (`Collider.h`)
コライダーに、動かないオブジェクト（`Static`）か、動くキャラクター（`Dynamic`）かを判別する属性を追加します。
```cpp
enum class BodyType
{
    Static,  // 障害物、地形など（動かされない）
    Dynamic  // プレイヤー、敵など（衝突で押し戻される）
};
```

#### 2. `CollisionManager::Update()` での自動押し出し
コライダー同士の衝突を検知した際、双方が `Dynamic` 対 `Static` であれば、`Dynamic` 側の座標をめり込み分だけ自動で押し戻します。
```cpp
// CollisionManager::Update 内での処理イメージ
void CollisionManager::Update()
{
    // 全コライダーの総当たりチェック
    for (size_t i = 0; i < colliders_.size(); ++i) {
        for (size_t j = i + 1; j < colliders_.size(); ++j) {
            Collider* colA = colliders_[i];
            Collider* colB = colliders_[j];

            if (CheckCollision(colA, colB)) {
                // めり込み解決 (Penetration Resolution)
                ResolvePenetration(colA, colB);
            }
        }
    }
}

void CollisionManager::ResolvePenetration(Collider* colA, Collider* colB)
{
    if (colA->GetBodyType() == BodyType::Static && colB->GetBodyType() == BodyType::Static) return;

    // めり込み方向と深度の計算
    Vector3 pushDir;
    float depth;
    CalculatePenetrationDepth(colA, colB, &pushDir, &depth);

    // Dynamicな方を押し戻す
    if (colA->GetBodyType() == BodyType::Dynamic && colB->GetBodyType() == BodyType::Static) {
        colA->SetWorldPosition(colA->GetWorldPosition() - pushDir * depth);
    }
    else if (colB->GetBodyType() == BodyType::Dynamic && colA->GetBodyType() == BodyType::Static) {
        colB->SetWorldPosition(colB->GetWorldPosition() + pushDir * depth);
    }
}
```

---

## 3. アプリケーション層（`CollisionSystem.cpp`）のクリーンアップ対比

エンジン側への拡張が完了した際、アプリ側のコードがどれほどクリーンになるかの対比です。

### 3.1 弾丸と障害物の判定 (`ResolveObstacleCollisions`)

#### 【Before】現状の実装（アプリ層で手動でコライダーをループ解析）
```cpp
void CollisionSystem::ResolveObstacleCollisions()
{
    auto& bullets = scene_->combatSystem_->GetBullets();
    for (auto& bullet : bullets) {
        // ... (省略) ...
        auto& colliders = CollisionManager::GetInstance()->GetColliders();
        for (Collider* col : colliders) {
            if (col->GetAttribute() != CollisionAttribute::Obstacle) continue;
            
            CollisionData data;
            data.type = col->GetType();
            // コライダーの種類ごとに手動で半径や高さを詰め替え
            if (data.type == ColliderType::Sphere) {
                data.shape.radius = static_cast<SphereCollider*>(col)->GetRadius();
            } // ... (Capsule等の分岐) ...

            float dist = 0.0f;
            if (CollisionManager::CheckRayCollider(bPosPrev, dir, closestDist, data, dist)) {
                closestDist = dist;
                hitObstacle = true;
            }
        }
        if (hitObstacle) {
            // エフェクトの発生など...
            bullet->Finalize();
        }
    }
}
```

#### 【After】エンジン移行後の実装（エンジンのRaycastを呼ぶだけ）
```cpp
void CollisionSystem::ResolveObstacleCollisions()
{
    auto& bullets = scene_->combatSystem_->GetBullets();
    for (auto& bullet : bullets) {
        if (!bullet || bullet->IsDead()) continue;

        Ray ray = { bullet->GetPrevPosition(), bullet->GetDirection() };
        RaycastHit hit;
        if (CollisionManager::GetInstance()->Raycast(ray, bullet->GetSpeed() * deltaTime, &hit)) {
            // エフェクト発生
            scene_->appParticleManager_->EmitMuzzleFlare(..., hit.position);
            bullet->Finalize();
        }
    }
}
```

---

### 3.2 キャラクターと障害物の補正 (`ResolveCharacterObstacleCollisions`)

#### 【Before】現状の実装（アプリ側で押し戻しベクトルを直書き計算）
```cpp
void CollisionSystem::ResolveCharacterObstacleCollisions()
{
    // プレイヤーと障害物の押し戻し
    if (scene_->player_ && !scene_->player_->IsDead()) {
        Vector3 playerPos = scene_->player_->GetPosition();
        auto& colliders = CollisionManager::GetInstance()->GetColliders();
        for (Collider* col : colliders) {
            // めり込み距離を泥泥で計算し、
            // プレイヤー座標を直接 playerPos.x += push.x のように補正
        }
    }
}
```

#### 【After】エンジン移行後の実装
```cpp
void CollisionSystem::ResolveCharacterObstacleCollisions()
{
    // 完全に空（削除可能）
    // エンジン側の CollisionManager::Update 内で自動的に押し出しが行われるため、
    // アプリケーション側には一切のコード記述が不要になります。
}
```

---

## 4. この拡張によるメリット

1.  **3Dベクトル数学の隠蔽:**
    アプリ開発者は「交点の算出」や「押し出し方向」といった数式を一切記述する必要がなくなり、純粋なゲームイベントのコーディングに専念できます。
2.  **不具合の防止:**
    衝突判定や補正のロジックがエンジン側に一本化されるため、キャラクターが壁をすり抜けたり、当たり判定がズレたりといったバグのデバッグが極めて容易になります。
