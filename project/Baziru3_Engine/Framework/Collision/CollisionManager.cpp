#define NOMINMAX
#include "CollisionManager.h"
#include "SphereCollider.h"
#include "BoxCollider.h"
#include "CapsuleCollider.h"
#include "MeshCollider.h"
#include "SkeletonCollider.h"
#include "Matrix4x4.h"
#include "DirectXCom.h"
#include "Baziru3_Engine/Core/Base/Allocator/StackAllocator.h"
#include "Baziru3_Engine/Framework/Scene/Manager/SceneManager.h"
#include "Baziru3_Engine/Core/Camera/Camera.h"
#include "Baziru3_Engine/Graphics/Shapes/Sphere/Sphere.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3d.h"
#include <imgui.h>
#include <Windows.h>
#include <cmath>
#include <algorithm>
#include <chrono>

// =========================================================================
// ベクトル数学のインラインヘルパー関数
// =========================================================================

inline Vector3 Cross(const Vector3& a, const Vector3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

inline float Dot(const Vector3& a, const Vector3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline float LengthSq(const Vector3& v)
{
    return Dot(v, v);
}

inline float Length(const Vector3& v)
{
    return std::sqrt(LengthSq(v));
}

inline Vector3 Normalize(const Vector3& v)
{
    float len = Length(v);
    if (len > 1e-5f)
    {
        return v * (1.0f / len);
    }
    return { 0.0f, 0.0f, 0.0f };
}

inline float Clamp(float value, float min, float max)
{
    return (std::max)(min, (std::min)(value, max));
}

// =========================================================================
// CollisionManager メンバー関数実装
// =========================================================================

CollisionManager* CollisionManager::GetInstance()
{
    static CollisionManager instance;
    return &instance;
}

void CollisionManager::Initialize()
{
    colliders_.clear();
    previousTriggerPairs_.clear();
    currentTriggerPairs_.clear();

    // デフォルトの衝突フィルタの設定 (ビットマスクの初期化)
    collisionMasks_.clear();
    
    uint32_t maskPlayer   = (1 << static_cast<uint32_t>(CollisionAttribute::Enemy)) |
                            (1 << static_cast<uint32_t>(CollisionAttribute::Bullet)) |
                            (1 << static_cast<uint32_t>(CollisionAttribute::Obstacle));
    
    uint32_t maskEnemy    = (1 << static_cast<uint32_t>(CollisionAttribute::Player)) |
                            (1 << static_cast<uint32_t>(CollisionAttribute::Bullet)) |
                            (1 << static_cast<uint32_t>(CollisionAttribute::Obstacle));
    
    uint32_t maskBullet   = (1 << static_cast<uint32_t>(CollisionAttribute::Player)) |
                            (1 << static_cast<uint32_t>(CollisionAttribute::Enemy)) |
                            (1 << static_cast<uint32_t>(CollisionAttribute::Obstacle));
    
    uint32_t maskObstacle = (1 << static_cast<uint32_t>(CollisionAttribute::Player)) |
                            (1 << static_cast<uint32_t>(CollisionAttribute::Enemy)) |
                            (1 << static_cast<uint32_t>(CollisionAttribute::Bullet));

    collisionMasks_[CollisionAttribute::Player]   = maskPlayer;
    collisionMasks_[CollisionAttribute::Enemy]    = maskEnemy;
    collisionMasks_[CollisionAttribute::Bullet]   = maskBullet;
    collisionMasks_[CollisionAttribute::Obstacle] = maskObstacle;
}

void CollisionManager::Finalize()
{
    colliders_.clear();
    previousTriggerPairs_.clear();
    currentTriggerPairs_.clear();
}

void CollisionManager::RegisterCollider(Collider* collider)
{
    if (collider && std::find(colliders_.begin(), colliders_.end(), collider) == colliders_.end())
    {
        colliders_.push_back(collider);
    }
}

void CollisionManager::UnregisterCollider(Collider* collider)
{
    auto it = std::remove(colliders_.begin(), colliders_.end(), collider);
    if (it != colliders_.end())
    {
        colliders_.erase(it, colliders_.end());
    }
}

void CollisionManager::SetCollisionFilter(CollisionAttribute a, CollisionAttribute b, bool enable)
{
    uint32_t bitB = 1 << static_cast<uint32_t>(b);
    uint32_t bitA = 1 << static_cast<uint32_t>(a);

    if (enable)
    {
        collisionMasks_[a] |= bitB;
        collisionMasks_[b] |= bitA;
    }
    else
    {
        collisionMasks_[a] &= ~bitB;
        collisionMasks_[b] &= ~bitA;
    }
}

bool CollisionManager::ShouldCollide(CollisionAttribute a, CollisionAttribute b) const
{
    auto itA = collisionMasks_.find(a);
    auto itB = collisionMasks_.find(b);
    if (itA == collisionMasks_.end() || itB == collisionMasks_.end())
    {
        return true;
    }

    uint32_t bitA = 1 << static_cast<uint32_t>(a);
    uint32_t bitB = 1 << static_cast<uint32_t>(b);

    return (itA->second & bitB) && (itB->second & bitA);
}

void CollisionManager::Update()
{
    auto startTime = std::chrono::steady_clock::now();
    frameCount_++;

    if (colliders_.size() < 2)
    {
        lastUpdateDurationMs_ = 0.0f;
        return;
    }

    // 1. 各コライダーから衝突判定に必要なデータのみを抽出した連続配列を構築 (DOD化)
    std::vector<CollisionData> dataList;
    dataList.reserve(colliders_.size());

    for (Collider* col : colliders_)
    {
        if (!col || !col->IsEnabled()) continue;

        CollisionData data;
        data.originalCollider = col;
        data.type = col->GetType();
        data.attribute = col->GetAttribute();
        data.worldPosition = col->GetWorldPosition(); // 仮想関数とポインタ逆参照をここで一度だけ評価してフラット化
        data.isTrigger = col->IsTrigger();

        // 形状ごとの固有データを事前に取得して詰め込む
        if (data.type == ColliderType::Sphere)
        {
            SphereCollider* sphere = static_cast<SphereCollider*>(col);
            data.shape.radius = sphere->GetRadius();
        }
        else if (data.type == ColliderType::Box)
        {
            BoxCollider* box = static_cast<BoxCollider*>(col);
            data.shape.size = box->GetSize();
            data.shape.rotation = box->GetWorldRotation();
        }
        else if (data.type == ColliderType::Capsule)
        {
            CapsuleCollider* capsule = static_cast<CapsuleCollider*>(col);
            data.shape.radius = capsule->GetRadius();
            data.shape.height = capsule->GetHeight();
        }

        dataList.push_back(data);
    }

    if (dataList.size() < 2)
    {
        lastUpdateDurationMs_ = 0.0f;
        return;
    }

    // 2. 空間ハッシュテーブルの取得と初期化 (StackAllocator を使用して動的ヒープ確保を回避)
    DirectXCom* dxCommon = SceneManager::GetInstance()->GetDirectXCom();
    if (!dxCommon)
    {
        lastUpdateDurationMs_ = 0.0f;
        return;
    }
    StackAllocator* stackAllocator = dxCommon->GetStackAllocator();
    
    SpatialHashCell* gridTable = static_cast<SpatialHashCell*>(stackAllocator->Allocate(
        sizeof(SpatialHashCell) * kGridTableSize, alignof(SpatialHashCell)));
    std::memset(gridTable, 0, sizeof(SpatialHashCell) * kGridTableSize);

    // 各コライダーを該当するグリッドセルに登録
    for (uint32_t i = 0; i < static_cast<uint32_t>(dataList.size()); ++i)
    {
        const CollisionData& col = dataList[i];
        
        // 簡易AABB（境界箱）の算出
        Vector3 minPos = col.worldPosition;
        Vector3 maxPos = col.worldPosition;
        
        if (col.type == ColliderType::Sphere)
        {
            float r = col.shape.radius;
            minPos = minPos - Vector3{ r, r, r };
            maxPos = maxPos + Vector3{ r, r, r };
        }
        else if (col.type == ColliderType::Box)
        {
            // ボックスの各辺サイズからAABBを作成
            Vector3 size = col.shape.size;
            Vector3 halfSize = { size.x * 0.5f, size.y * 0.5f, size.z * 0.5f };
            minPos = minPos - halfSize;
            maxPos = maxPos + halfSize;
        }
        else if (col.type == ColliderType::Capsule)
        {
            float r = col.shape.radius;
            float halfH = col.shape.height * 0.5f;
            minPos = minPos - Vector3{ r, halfH + r, r };
            maxPos = maxPos + Vector3{ r, halfH + r, r };
        }

        // 重なるセル範囲のグリッドインデックスを計算
        int32_t minX = CalculateGridIndex(minPos.x);
        int32_t minY = CalculateGridIndex(minPos.y);
        int32_t minZ = CalculateGridIndex(minPos.z);
        
        int32_t maxX = CalculateGridIndex(maxPos.x);
        int32_t maxY = CalculateGridIndex(maxPos.y);
        int32_t maxZ = CalculateGridIndex(maxPos.z);

        // 巨大なオブジェクトが異常に広いセル範囲を覆って処理が遅延するのを防ぐクランプ処理
        if (maxX - minX > 2) maxX = minX + 2;
        if (maxY - minY > 2) maxY = minY + 2;
        if (maxZ - minZ > 2) maxZ = minZ + 2;

        // 全ての関連セルにインデックスを登録
        for (int32_t x = minX; x <= maxX; ++x)
        {
            for (int32_t y = minY; y <= maxY; ++y)
            {
                for (int32_t z = minZ; z <= maxZ; ++z)
                {
                    size_t hashKey = GetHashKey(x, y, z);
                    SpatialHashCell& cell = gridTable[hashKey];
                    if (cell.count < SpatialHashCell::kMaxColliders)
                    {
                        cell.colliderIndices[cell.count] = i;
                        cell.count++;
                    }
                }
            }
        }
    }

    // 3. 重複テスト防止用のフラグテーブルをアロケート (StackAllocator を利用)
    size_t numColliders = dataList.size();
    size_t flagTableSize = numColliders * numColliders;
    uint8_t* testedFlags = static_cast<uint8_t*>(stackAllocator->Allocate(
        flagTableSize, 1));
    std::memset(testedFlags, 0, flagTableSize);

    // 4. 各グリッドセル内の登録コライダーペアに対して交差判定を実行 (計算量は平均 O(N) に激減)
    for (size_t cellIdx = 0; cellIdx < kGridTableSize; ++cellIdx)
    {
        const SpatialHashCell& cell = gridTable[cellIdx];
        if (cell.count < 2) continue;

        for (uint32_t i = 0; i < cell.count; ++i)
        {
            uint32_t idxA = cell.colliderIndices[i];
            CollisionData& colA = dataList[idxA];

            for (uint32_t j = i + 1; j < cell.count; ++j)
            {
                uint32_t idxB = cell.colliderIndices[j];
                CollisionData& colB = dataList[idxB];

                // 重複排除のためにインデックスの小さい順にソートして判定キーを生成
                uint32_t lowIdx = idxA;
                uint32_t highIdx = idxB;
                if (lowIdx > highIdx) std::swap(lowIdx, highIdx);

                // すでに同フレーム内で判定済みのペアならスキップ
                size_t flagIdx = static_cast<size_t>(lowIdx) * numColliders + highIdx;
                if (testedFlags[flagIdx] != 0) continue;
                testedFlags[flagIdx] = 1;

                if (!ShouldCollide(colA.attribute, colB.attribute))
                {
                    continue;
                }

                Vector3 pushDir = { 0.0f, 0.0f, 0.0f };
                float pushLen = 0.0f;

                if (CheckCollision(colA, colB, pushDir, pushLen))
                {
                    if (colA.isTrigger || colB.isTrigger)
                    {
                        // トリガーイベント登録 (物理的押し出しは行わない)
                        TriggerPair pair{ colA.originalCollider, colB.originalCollider };
                        currentTriggerPairs_.push_back(pair);
                    }
                    else
                    {
                        // 物理的衝突（押し出し処理）
                        bool isAFixed = (colA.attribute == CollisionAttribute::Obstacle);
                        bool isBFixed = (colB.attribute == CollisionAttribute::Obstacle);

                        if (isAFixed && !isBFixed)
                        {
                            Vector3 newPos = colB.worldPosition + pushDir * pushLen;
                            colB.originalCollider->SetWorldPosition(newPos);
                            colB.worldPosition = newPos;
                        }
                        else if (!isAFixed && isBFixed)
                        {
                            Vector3 newPos = colA.worldPosition - pushDir * pushLen;
                            colA.originalCollider->SetWorldPosition(newPos);
                            colA.worldPosition = newPos;
                        }
                        else if (!isAFixed && !isBFixed)
                        {
                            Vector3 newPosA = colA.worldPosition - pushDir * (pushLen * 0.5f);
                            Vector3 newPosB = colB.worldPosition + pushDir * (pushLen * 0.5f);
                            colA.originalCollider->SetWorldPosition(newPosA);
                            colB.originalCollider->SetWorldPosition(newPosB);
                            colA.worldPosition = newPosA;
                            colB.worldPosition = newPosB;
                        }

                        // 物理的衝突のコールバックを発行 (OnCollision)
                        Vector3 contactPoint = (colA.worldPosition + colB.worldPosition) * 0.5f;

                        CollisionInfo infoToA;
                        infoToA.other = colB.originalCollider;
                        infoToA.contactPoint = contactPoint;
                        infoToA.normal = pushDir; // 相手から自分への方向
                        infoToA.depth = pushLen;
                        colA.originalCollider->OnCollision(infoToA);

                        CollisionInfo infoToB;
                        infoToB.other = colA.originalCollider;
                        infoToB.contactPoint = contactPoint;
                        infoToB.normal = pushDir * -1.0f; // 自分から相手への方向
                        infoToB.depth = pushLen;
                        colB.originalCollider->OnCollision(infoToB);
                    }
                }
            }
        }
    }

    // --- トリガーライフサイクルイベントの発行 ---
    for (const auto& current : currentTriggerPairs_)
    {
        auto it = std::find_if(previousTriggerPairs_.begin(), previousTriggerPairs_.end(), [&](const TriggerPair& p) {
            return (p.a == current.a && p.b == current.b) || (p.a == current.b && p.b == current.a);
        });

        if (it != previousTriggerPairs_.end())
        {
            // 前フレームにも存在した => Stay
            current.a->OnTriggerStay(current.b);
            current.b->OnTriggerStay(current.a);
        }
        else
        {
            // 新規衝突 => Enter
            current.a->OnTriggerEnter(current.b);
            current.b->OnTriggerEnter(current.a);
        }
    }

    // 前フレームにあったが、現フレームで衝突しなくなったペア => Exit
    for (const auto& prev : previousTriggerPairs_)
    {
        auto it = std::find_if(currentTriggerPairs_.begin(), currentTriggerPairs_.end(), [&](const TriggerPair& p) {
            return (p.a == prev.a && p.b == prev.b) || (p.a == prev.b && p.b == prev.a);
        });

        if (it == currentTriggerPairs_.end())
        {
            prev.a->OnTriggerExit(prev.b);
            prev.b->OnTriggerExit(prev.a);
        }
    }

    // トリガー履歴の更新
    previousTriggerPairs_ = std::move(currentTriggerPairs_);
    currentTriggerPairs_.clear();

    auto endTime = std::chrono::steady_clock::now();
    lastUpdateDurationMs_ = std::chrono::duration<float, std::milli>(endTime - startTime).count();
}

bool CollisionManager::CheckCollision(const CollisionData& a, const CollisionData& b, Vector3& outPushDir, float& outPushLen)
{
	// アーリーアウト: 境界球の距離比較による超高速カット
	float maxRadiusA = a.shape.radius;
	if (a.type == ColliderType::Box)
	{
		maxRadiusA = Length({ a.shape.size.x * 0.5f, a.shape.size.y * 0.5f, a.shape.size.z * 0.5f });
	}
	else if (a.type == ColliderType::Capsule)
	{
		maxRadiusA = a.shape.radius + a.shape.height * 0.5f;
	}

	float maxRadiusB = b.shape.radius;
	if (b.type == ColliderType::Box)
	{
		maxRadiusB = Length({ b.shape.size.x * 0.5f, b.shape.size.y * 0.5f, b.shape.size.z * 0.5f });
	}
	else if (b.type == ColliderType::Capsule)
	{
		maxRadiusB = b.shape.radius + b.shape.height * 0.5f;
	}

	Vector3 delta = b.worldPosition - a.worldPosition;
	float distSq = LengthSq(delta);
	float maxCombined = maxRadiusA + maxRadiusB;
	if (distSq > maxCombined * maxCombined)
	{
		return false;
	}

	ColliderType typeA = a.type;
	ColliderType typeB = b.type;


    // 1. 球 vs 球 (Sphere - Sphere)
    if (typeA == ColliderType::Sphere && typeB == ColliderType::Sphere)
    {
        return CheckSphereSphere(a, b, outPushDir, outPushLen);
    }
    // 2. 球 vs ボックス (Sphere - Box)
    if (typeA == ColliderType::Sphere && typeB == ColliderType::Box)
    {
        return CheckSphereBox(a, b, outPushDir, outPushLen);
    }
    if (typeA == ColliderType::Box && typeB == ColliderType::Sphere)
    {
        // 押し出し方向ベクトル(outPushDir)は「aからbへの方向」として算出されるため、判定後に反転します。
        bool hit = CheckSphereBox(b, a, outPushDir, outPushLen);
        outPushDir = outPushDir * -1.0f;
        return hit;
    }
    // 3. 球 vs カプセル (Sphere - Capsule)
    if (typeA == ColliderType::Sphere && typeB == ColliderType::Capsule)
    {
        return CheckSphereCapsule(a, b, outPushDir, outPushLen);
    }
    if (typeA == ColliderType::Capsule && typeB == ColliderType::Sphere)
    {
        bool hit = CheckSphereCapsule(b, a, outPushDir, outPushLen);
        outPushDir = outPushDir * -1.0f;
        return hit;
    }
    // 4. ボックス vs ボックス (Box - Box)
    if (typeA == ColliderType::Box && typeB == ColliderType::Box)
    {
        return CheckBoxBox(a, b, outPushDir, outPushLen);
    }
    // 5. ボックス vs カプセル (Box - Capsule)
    if (typeA == ColliderType::Box && typeB == ColliderType::Capsule)
    {
        return CheckBoxCapsule(a, b, outPushDir, outPushLen);
    }
    if (typeA == ColliderType::Capsule && typeB == ColliderType::Box)
    {
        bool hit = CheckBoxCapsule(b, a, outPushDir, outPushLen);
        outPushDir = outPushDir * -1.0f;
        return hit;
    }
    // 6. カプセル vs カプセル (Capsule - Capsule)
    if (typeA == ColliderType::Capsule && typeB == ColliderType::Capsule)
    {
        return CheckCapsuleCapsule(a, b, outPushDir, outPushLen);
    }

    return false;
}

// =========================================================================
// 各コライダー組み合わせに対する詳細な交差判定アルゴリズム
// =========================================================================

bool CollisionManager::CheckSphereSphere(const CollisionData& a, const CollisionData& b, Vector3& outPushDir, float& outPushLen)
{
    Vector3 posA = a.worldPosition;
    Vector3 posB = b.worldPosition;

    Vector3 dir = posB - posA;
    float dist = Length(dir);
    float minDist = a.shape.radius + b.shape.radius;

    if (dist < minDist)
    {
        outPushLen = minDist - dist;
        if (dist > 1e-4f)
        {
            outPushDir = Normalize(dir);
        }
        else
        {
            outPushDir = { 0.0f, 0.0f, 1.0f };
        }
        return true;
    }
    return false;
}

bool CollisionManager::CheckSphereBox(const CollisionData& sphere, const CollisionData& box, Vector3& outPushDir, float& outPushLen)
{
    Vector3 sPos = sphere.worldPosition;
    Vector3 bPos = box.worldPosition;
    Vector3 size = box.shape.size;
    Vector3 extents = { size.x * 0.5f, size.y * 0.5f, size.z * 0.5f };

    // 1. ボックスの回転行列 R を計算する
    Vector3 bRot = box.shape.rotation;
    Matrix4x4 R = Multiply(MakeRotateXMatrix(bRot.x), Multiply(MakeRotateYMatrix(bRot.y), MakeRotateZMatrix(bRot.z)));

    Vector3 axisX = { R.m[0][0], R.m[0][1], R.m[0][2] };
    Vector3 axisY = { R.m[1][0], R.m[1][1], R.m[1][2] };
    Vector3 axisZ = { R.m[2][0], R.m[2][1], R.m[2][2] };

    Vector3 offset = sPos - bPos;
    Vector3 localSphPos = {
        Dot(offset, axisX),
        Dot(offset, axisY),
        Dot(offset, axisZ)
    };

    Vector3 closestPointOnBox;
    closestPointOnBox.x = Clamp(localSphPos.x, -extents.x, extents.x);
    closestPointOnBox.y = Clamp(localSphPos.y, -extents.y, extents.y);
    closestPointOnBox.z = Clamp(localSphPos.z, -extents.z, extents.z);

    if (std::abs(localSphPos.x) <= extents.x &&
        std::abs(localSphPos.y) <= extents.y &&
        std::abs(localSphPos.z) <= extents.z)
    {
        float distL = extents.x + localSphPos.x; 
        float distR = extents.x - localSphPos.x; 
        float distB = extents.y + localSphPos.y; 
        float distT = extents.y - localSphPos.y; 
        float distF = extents.z + localSphPos.z; 
        float distN = extents.z - localSphPos.z; 

        float minDist = distL;
        Vector3 localPushDir = { 1.0f, 0.0f, 0.0f }; 

        if (distR < minDist) { minDist = distR; localPushDir = { -1.0f, 0.0f, 0.0f }; }
        if (distB < minDist) { minDist = distB; localPushDir = { 0.0f, 1.0f, 0.0f }; }
        if (distT < minDist) { minDist = distT; localPushDir = { 0.0f, -1.0f, 0.0f }; }
        if (distF < minDist) { minDist = distF; localPushDir = { 0.0f, 0.0f, 1.0f }; }
        if (distN < minDist) { minDist = distN; localPushDir = { 0.0f, 0.0f, -1.0f }; }

        outPushLen = sphere.shape.radius + minDist;
        outPushDir = axisX * localPushDir.x + axisY * localPushDir.y + axisZ * localPushDir.z;
        return true;
    }

    Vector3 localDir = closestPointOnBox - localSphPos;
    float dist = Length(localDir);

    if (dist < sphere.shape.radius)
    {
        outPushLen = sphere.shape.radius - dist;
        if (dist > 1e-4f)
        {
            Vector3 localPushDir = Normalize(localDir);
            outPushDir = axisX * localPushDir.x + axisY * localPushDir.y + axisZ * localPushDir.z;
        }
        else
        {
            outPushDir = axisZ * -1.0f;
        }
        return true;
    }

    return false;
}

bool CollisionManager::CheckSphereCapsule(const CollisionData& sphere, const CollisionData& capsule, Vector3& outPushDir, float& outPushLen)
{
    Vector3 sPos = sphere.worldPosition;
    Vector3 cPos = capsule.worldPosition;

    float halfH = capsule.shape.height * 0.5f;
    Vector3 segA = cPos - Vector3{ 0.0f, halfH, 0.0f };
    Vector3 segB = cPos + Vector3{ 0.0f, halfH, 0.0f };

    Vector3 ab = segB - segA;
    Vector3 as = sPos - segA;

    float abLenSq = Dot(ab, ab);
    float t = 0.0f;
    if (abLenSq > 1e-5f)
    {
        t = Dot(as, ab) / abLenSq;
    }
    
    t = Clamp(t, 0.0f, 1.0f);
    
    Vector3 closestPointOnSegment = segA + ab * t;

    Vector3 dir = sPos - closestPointOnSegment;
    float dist = Length(dir);
    float minDist = sphere.shape.radius + capsule.shape.radius;

    if (dist < minDist)
    {
        outPushLen = minDist - dist;
        if (dist > 1e-4f)
        {
            outPushDir = Normalize(dir);
        }
        else
        {
            outPushDir = { 0.0f, 0.0f, 1.0f };
        }
        return true;
    }

    return false;
}

bool CollisionManager::CheckBoxBox(const CollisionData& a, const CollisionData& b, Vector3& outPushDir, float& outPushLen)
{
    // OBB同士の交差判定 (分離軸定理: SAT)
    Vector3 rotA = a.shape.rotation;
    Matrix4x4 RA = Multiply(MakeRotateXMatrix(rotA.x), Multiply(MakeRotateYMatrix(rotA.y), MakeRotateZMatrix(rotA.z)));
    Vector3 uA[3] = {
        { RA.m[0][0], RA.m[0][1], RA.m[0][2] },
        { RA.m[1][0], RA.m[1][1], RA.m[1][2] },
        { RA.m[2][0], RA.m[2][1], RA.m[2][2] }
    };

    Vector3 rotB = b.shape.rotation;
    Matrix4x4 RB = Multiply(MakeRotateXMatrix(rotB.x), Multiply(MakeRotateYMatrix(rotB.y), MakeRotateZMatrix(rotB.z)));
    Vector3 uB[3] = {
        { RB.m[0][0], RB.m[0][1], RB.m[0][2] },
        { RB.m[1][0], RB.m[1][1], RB.m[1][2] },
        { RB.m[2][0], RB.m[2][1], RB.m[2][2] }
    };

    Vector3 T = b.worldPosition - a.worldPosition;
    Vector3 hA = a.shape.size * 0.5f;
    Vector3 hB = b.shape.size * 0.5f;

    Vector3 axes[15];
    int axisCount = 0;

    // Aのローカル軸
    for (int i = 0; i < 3; ++i) axes[axisCount++] = uA[i];
    // Bのローカル軸
    for (int i = 0; i < 3; ++i) axes[axisCount++] = uB[i];

    // 外積軸
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            Vector3 crossAxis = Cross(uA[i], uB[j]);
            if (LengthSq(crossAxis) > 1e-5f)
            {
                axes[axisCount++] = Normalize(crossAxis);
            }
        }
    }

    float minOverlap = 1e30f;
    Vector3 bestAxis = { 0.0f, 0.0f, 0.0f };

    for (int i = 0; i < axisCount; ++i)
    {
        Vector3 L = axes[i];
        if (LengthSq(L) < 1e-5f) continue;
        L = Normalize(L);

        // 投影半径の計算
        float rA = hA.x * std::abs(Dot(uA[0], L)) + hA.y * std::abs(Dot(uA[1], L)) + hA.z * std::abs(Dot(uA[2], L));
        float rB = hB.x * std::abs(Dot(uB[0], L)) + hB.y * std::abs(Dot(uB[1], L)) + hB.z * std::abs(Dot(uB[2], L));

        // 中心間距離の投影
        float distance = std::abs(Dot(T, L));

        // 重なり幅
        float overlap = (rA + rB) - distance;

        if (overlap < 0.0f)
        {
            return false; // 分離軸が見つかった
        }

        if (overlap < minOverlap)
        {
            minOverlap = overlap;
            bestAxis = L;
        }
    }

    outPushLen = minOverlap;
    if (Dot(T, bestAxis) < 0.0f)
    {
        outPushDir = bestAxis * -1.0f;
    }
    else
    {
        outPushDir = bestAxis;
    }

    return true;
}

bool CollisionManager::CheckBoxCapsule(const CollisionData& box, const CollisionData& capsule, Vector3& outPushDir, float& outPushLen)
{
    Vector3 bPos = box.worldPosition;
    Vector3 rot = box.shape.rotation;
    Matrix4x4 R = Multiply(MakeRotateXMatrix(rot.x), Multiply(MakeRotateYMatrix(rot.y), MakeRotateZMatrix(rot.z)));
    Vector3 uA[3] = {
        { R.m[0][0], R.m[0][1], R.m[0][2] },
        { R.m[1][0], R.m[1][1], R.m[1][2] },
        { R.m[2][0], R.m[2][1], R.m[2][2] }
    };

    Vector3 extents = box.shape.size * 0.5f;

    float halfH = capsule.shape.height * 0.5f;
    Vector3 P0 = capsule.worldPosition - Vector3{ 0.0f, halfH, 0.0f };
    Vector3 P1 = capsule.worldPosition + Vector3{ 0.0f, halfH, 0.0f };

    // 1. カプセル線分をBoxのローカル空間に変換
    Vector3 offset0 = P0 - bPos;
    Vector3 localP0 = { Dot(offset0, uA[0]), Dot(offset0, uA[1]), Dot(offset0, uA[2]) };
    Vector3 offset1 = P1 - bPos;
    Vector3 localP1 = { Dot(offset1, uA[0]), Dot(offset1, uA[1]), Dot(offset1, uA[2]) };

    // 2. AABBと線分の最短点を見つけるためのパラメータ t 候補
    std::vector<float> tCandidates;
    tCandidates.push_back(0.0f);
    tCandidates.push_back(1.0f);

    Vector3 segmentDir = localP1 - localP0;

    auto checkPlaneIntersection = [&](float value, float p0Val, float dirVal) {
        if (std::abs(dirVal) > 1e-5f)
        {
            float t = (value - p0Val) / dirVal;
            if (t >= 0.0f && t <= 1.0f)
            {
                tCandidates.push_back(t);
            }
        }
    };

    // AABB の各境界プレーンとの交点
    checkPlaneIntersection(extents.x, localP0.x, segmentDir.x);
    checkPlaneIntersection(-extents.x, localP0.x, segmentDir.x);
    checkPlaneIntersection(extents.y, localP0.y, segmentDir.y);
    checkPlaneIntersection(-extents.y, localP0.y, segmentDir.y);
    checkPlaneIntersection(extents.z, localP0.z, segmentDir.z);
    checkPlaneIntersection(-extents.z, localP0.z, segmentDir.z);

    float bestT = 0.0f;
    float minSqDist = 1e30f;

    for (float t : tCandidates)
    {
        Vector3 pt = localP0 + segmentDir * t;
        Vector3 closest = {
            Clamp(pt.x, -extents.x, extents.x),
            Clamp(pt.y, -extents.y, extents.y),
            Clamp(pt.z, -extents.z, extents.z)
        };
        float sqDist = LengthSq(pt - closest);
        if (sqDist < minSqDist)
        {
            minSqDist = sqDist;
            bestT = t;
        }
    }

    // 3. 特定した最短接近点 Q (ワールド座標)
    Vector3 Q = P0 + (P1 - P0) * bestT;

    // 4. Qを中心とする球とBoxの衝突判定へ帰着
    CollisionData sphereData;
    sphereData.originalCollider = capsule.originalCollider; // 押し戻し処理用にコライダーへの参照を保持
    sphereData.type = ColliderType::Sphere;
    sphereData.attribute = capsule.attribute;
    sphereData.worldPosition = Q;
    sphereData.shape.radius = capsule.shape.radius;

    // 球 vs Box 判定を呼び出す (押し戻し方向と量はそのまま使える)
    return CheckSphereBox(sphereData, box, outPushDir, outPushLen);
}

bool CollisionManager::CheckCapsuleCapsule(const CollisionData& a, const CollisionData& b, Vector3& outPushDir, float& outPushLen)
{
    float halfHA = a.shape.height * 0.5f;
    Vector3 P0 = a.worldPosition - Vector3{ 0.0f, halfHA, 0.0f };
    Vector3 P1 = a.worldPosition + Vector3{ 0.0f, halfHA, 0.0f };

    float halfHB = b.shape.height * 0.5f;
    Vector3 Q0 = b.worldPosition - Vector3{ 0.0f, halfHB, 0.0f };
    Vector3 Q1 = b.worldPosition + Vector3{ 0.0f, halfHB, 0.0f };

    Vector3 u = P1 - P0;
    Vector3 v = Q1 - Q0;
    Vector3 w = P0 - Q0;
    float a_val = Dot(u, u);
    float b_val = Dot(u, v);
    float c_val = Dot(v, v);
    float d_val = Dot(u, w);
    float e_val = Dot(v, w);
    float D = a_val * c_val - b_val * b_val;
    float sc, sN, sD = D;
    float tc, tN, tD = D;

    if (D < 1e-5f)
    {
        sN = 0.0f;
        sD = 1.0f;
        tN = e_val;
        tD = c_val;
    }
    else
    {
        sN = (b_val * e_val - c_val * d_val);
        tN = (a_val * e_val - b_val * d_val);
        if (sN < 0.0f)
        {
            sN = 0.0f;
            tN = e_val;
            tD = c_val;
        }
        else if (sN > sD)
        {
            sN = sD;
            tN = e_val + b_val;
            tD = c_val;
        }
    }

    if (tN < 0.0f)
    {
        tN = 0.0f;
        if (-d_val < 0.0f)
            sN = 0.0f;
        else if (-d_val > a_val)
            sN = sD;
        else {
            sN = -d_val;
            sD = a_val;
        }
    }
    else if (tN > tD)
    {
        tN = tD;
        if ((-d_val + b_val) < 0.0f)
            sN = 0.0f;
        else if ((-d_val + b_val) > a_val)
            sN = sD;
        else {
            sN = (-d_val + b_val);
            sD = a_val;
        }
    }

    sc = (std::abs(sN) < 1e-5f ? 0.0f : sN / sD);
    tc = (std::abs(tN) < 1e-5f ? 0.0f : tN / tD);

    Vector3 dP = w + (u * sc) - (v * tc);
    float dist = Length(dP);
    float minDist = a.shape.radius + b.shape.radius;

    if (dist < minDist)
    {
        outPushLen = minDist - dist;
        if (dist > 1e-4f)
        {
            outPushDir = Normalize(dP);
        }
        else
        {
            outPushDir = { 0.0f, 0.0f, 1.0f };
        }
        return true;
    }
    return false;
}

bool CollisionManager::Raycast(const Vector3& rayStart, const Vector3& rayDir, float maxDist, Collider*& outHitCollider, float& outHitDist)
{
    Collider* closestCollider = nullptr;
    float closestDist = maxDist;
    bool hit = false;

    std::vector<CollisionData> dataList;
    dataList.reserve(colliders_.size());
    for (Collider* col : colliders_)
    {
        if (!col || !col->IsEnabled()) continue;
        CollisionData data;
        data.originalCollider = col;
        data.type = col->GetType();
        data.attribute = col->GetAttribute();
        data.worldPosition = col->GetWorldPosition();
        data.isTrigger = col->IsTrigger();

        if (data.type == ColliderType::Sphere)
        {
            SphereCollider* sphere = static_cast<SphereCollider*>(col);
            data.shape.radius = sphere->GetRadius();
        }
        else if (data.type == ColliderType::Box)
        {
            BoxCollider* box = static_cast<BoxCollider*>(col);
            data.shape.size = box->GetSize();
            data.shape.rotation = box->GetWorldRotation();
        }
        dataList.push_back(data);
    }

    for (const auto& data : dataList)
    {
        float dist = 0.0f;
        if (CheckRayCollider(rayStart, rayDir, closestDist, data, dist))
        {
            closestDist = dist;
            closestCollider = data.originalCollider;
            hit = true;
        }
    }

    if (hit)
    {
        outHitCollider = closestCollider;
        outHitDist = closestDist;
    }

    // Save debug raycast coordinates
    lastRaycast_.exists = true;
    lastRaycast_.start = rayStart;
    lastRaycast_.end = rayStart + rayDir * maxDist;
    lastRaycast_.hit = hit;
    if (hit)
    {
        lastRaycast_.hitPoint = rayStart + rayDir * closestDist;
    }
    else
    {
        lastRaycast_.hitMesh = false;
    }

    return hit;
}

bool CollisionManager::CheckRayCollider(const Vector3& rayStart, const Vector3& rayDir, float maxDist, const CollisionData& collider, float& outDist)
{
    ColliderType type = collider.type;

    if (type == ColliderType::Sphere)
    {
        return CheckRaySphere(rayStart, rayDir, maxDist, collider, outDist);
    }
    else if (type == ColliderType::Box)
    {
        return CheckRayBox(rayStart, rayDir, maxDist, collider, outDist);
    }
    else if (type == ColliderType::Mesh)
    {
        return CheckRayMesh(rayStart, rayDir, maxDist, collider, outDist);
    }
    else if (type == ColliderType::Skeleton)
    {
        return CheckRaySkeleton(rayStart, rayDir, maxDist, collider, outDist);
    }
    return false;
}

bool CollisionManager::CheckRaySphere(const Vector3& rayStart, const Vector3& rayDir, float maxDist, const CollisionData& sphere, float& outDist)
{
    Vector3 sPos = sphere.worldPosition;
    float radius = sphere.shape.radius;

    Vector3 m = rayStart - sPos;
    float b = Dot(m, rayDir);
    float c = Dot(m, m) - radius * radius;

    if (c > 0.0f && b > 0.0f) return false;

    float discr = b * b - c;
    if (discr < 0.0f) return false;

    float t = -b - std::sqrt(discr);
    if (t < 0.0f) t = 0.0f;

    if (t > maxDist) return false;

    outDist = t;
    return true;
}

bool CollisionManager::CheckRayBox(const Vector3& rayStart, const Vector3& rayDir, float maxDist, const CollisionData& box, float& outDist)
{
    Vector3 bPos = box.worldPosition;
    Vector3 size = box.shape.size;
    Vector3 extents = { size.x * 0.5f, size.y * 0.5f, size.z * 0.5f };

    // 回転行列 R を計算する
    Vector3 bRot = box.shape.rotation;
    Matrix4x4 R = Multiply(MakeRotateXMatrix(bRot.x), Multiply(MakeRotateYMatrix(bRot.y), MakeRotateZMatrix(bRot.z)));

    // ボックスのローカル軸（X, Y, Z）
    Vector3 axisX = { R.m[0][0], R.m[0][1], R.m[0][2] };
    Vector3 axisY = { R.m[1][0], R.m[1][1], R.m[1][2] };
    Vector3 axisZ = { R.m[2][0], R.m[2][1], R.m[2][2] };

    // レイの始点と方向をローカル空間に変換する
    Vector3 offset = rayStart - bPos;
    Vector3 localStart = {
        Dot(offset, axisX),
        Dot(offset, axisY),
        Dot(offset, axisZ)
    };
    Vector3 localDir = {
        Dot(rayDir, axisX),
        Dot(rayDir, axisY),
        Dot(rayDir, axisZ)
    };

    float tmin = 0.0f;
    float tmax = maxDist;

    if (std::abs(localDir.x) < 1e-6f)
    {
        if (localStart.x < -extents.x || localStart.x > extents.x) return false;
    }
    else
    {
        float ood = 1.0f / localDir.x;
        float t1 = (-extents.x - localStart.x) * ood;
        float t2 = (extents.x - localStart.x) * ood;
        if (t1 > t2) std::swap(t1, t2);
        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
        if (tmin > tmax) return false;
    }

    if (std::abs(localDir.y) < 1e-6f)
    {
        if (localStart.y < -extents.y || localStart.y > extents.y) return false;
    }
    else
    {
        float ood = 1.0f / localDir.y;
        float t1 = (-extents.y - localStart.y) * ood;
        float t2 = (extents.y - localStart.y) * ood;
        if (t1 > t2) std::swap(t1, t2);
        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
        if (tmin > tmax) return false;
    }

    if (std::abs(localDir.z) < 1e-6f)
    {
        if (localStart.z < -extents.z || localStart.z > extents.z) return false;
    }
    else
    {
        float ood = 1.0f / localDir.z;
        float t1 = (-extents.z - localStart.z) * ood;
        float t2 = (extents.z - localStart.z) * ood;
        if (t1 > t2) std::swap(t1, t2);
        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
        if (tmin > tmax) return false;
    }

    outDist = tmin;
    return true;
}

bool CollisionManager::CheckRayMesh(const Vector3& rayStart, const Vector3& rayDir, float maxDist, const CollisionData& meshData, float& outDist)
{
    MeshCollider* meshCollider = static_cast<MeshCollider*>(meshData.originalCollider);
    if (!meshCollider || !meshCollider->GetObject3d()) return false;

    // CPU skinning update if animated
    meshCollider->Update();

    Object3d* obj = meshCollider->GetObject3d();
    const auto& aabbTree = meshCollider->GetAABBTree();

    Matrix4x4 world = obj->GetWorldMatrix();
    Matrix4x4 invWorld = Inverse(world);

    // Transform Ray start to local space
    Vector3 localStart = {
        rayStart.x * invWorld.m[0][0] + rayStart.y * invWorld.m[1][0] + rayStart.z * invWorld.m[2][0] + invWorld.m[3][0],
        rayStart.x * invWorld.m[0][1] + rayStart.y * invWorld.m[1][1] + rayStart.z * invWorld.m[2][1] + invWorld.m[3][1],
        rayStart.x * invWorld.m[0][2] + rayStart.y * invWorld.m[1][2] + rayStart.z * invWorld.m[2][2] + invWorld.m[3][2]
    };

    // Transform Ray direction to local space
    Vector3 localDir = {
        rayDir.x * invWorld.m[0][0] + rayDir.y * invWorld.m[1][0] + rayDir.z * invWorld.m[2][0],
        rayDir.x * invWorld.m[0][1] + rayDir.y * invWorld.m[1][1] + rayDir.z * invWorld.m[2][1],
        rayDir.x * invWorld.m[0][2] + rayDir.y * invWorld.m[1][2] + rayDir.z * invWorld.m[2][2]
    };

    float localDirLen = std::sqrt(localDir.x * localDir.x + localDir.y * localDir.y + localDir.z * localDir.z);
    if (localDirLen < 1e-6f) return false;

    Vector3 localDirNorm = { localDir.x / localDirLen, localDir.y / localDirLen, localDir.z / localDirLen };
    float localMaxDist = maxDist * localDirLen;

    float localHitDist = 0.0f;
    Vector3 localHitNormal = { 0, 1, 0 };
    Vector3 localV0, localV1, localV2;
    if (aabbTree.Raycast(localStart, localDirNorm, localMaxDist, localHitDist, localHitNormal, localV0, localV1, localV2))
    {
        outDist = localHitDist / localDirLen;

        auto transformPt = [&](const Vector3& pt) -> Vector3 {
            return {
                pt.x * world.m[0][0] + pt.y * world.m[1][0] + pt.z * world.m[2][0] + world.m[3][0],
                pt.x * world.m[0][1] + pt.y * world.m[1][1] + pt.z * world.m[2][1] + world.m[3][1],
                pt.x * world.m[0][2] + pt.y * world.m[1][2] + pt.z * world.m[2][2] + world.m[3][2]
            };
        };

        GetInstance()->lastRaycast_.hitMesh = true;
        GetInstance()->lastRaycast_.hitTriV0 = transformPt(localV0);
        GetInstance()->lastRaycast_.hitTriV1 = transformPt(localV1);
        GetInstance()->lastRaycast_.hitTriV2 = transformPt(localV2);

        return true;
    }
    return false;
}

bool CollisionManager::CheckRaySkeleton(const Vector3& rayStart, const Vector3& rayDir, float maxDist, const CollisionData& skeletonData, float& outDist)
{
    SkeletonCollider* skelCollider = static_cast<SkeletonCollider*>(skeletonData.originalCollider);
    if (!skelCollider || !skelCollider->GetObject3d()) return false;

    Object3d* obj = skelCollider->GetObject3d();
    const auto& skeleton = obj->GetSkeleton();
    if (skeleton.joints.empty()) return false;

    Matrix4x4 modelWorldMatrix = MakeAffineMatrix(obj->GetScale(), obj->GetRotate(), obj->GetTranslate());

    bool hit = false;
    float closestDist = maxDist;

    for (size_t i = 0; i < skeleton.joints.size(); ++i)
    {
        Vector3 jointPos = skeleton.GetJointWorldPosition(i, modelWorldMatrix);
        float radius = skelCollider->GetJointRadius(skeleton.joints[i].name);

        Vector3 m = rayStart - jointPos;
        float b = Dot(m, rayDir);
        float c = Dot(m, m) - radius * radius;

        if (c > 0.0f && b > 0.0f) continue;

        float discr = b * b - c;
        if (discr < 0.0f) continue;

        float t = -b - std::sqrt(discr);
        if (t < 0.0f) t = 0.0f;

        if (t < closestDist)
        {
            closestDist = t;
            hit = true;
        }
    }

    if (hit)
    {
        outDist = closestDist;
        return true;
    }
    return false;
}

void CollisionManager::DrawDebug(Camera* camera)
{
#ifdef USE_IMGUI
	static bool prevF1 = false;
	bool curF1 = (GetAsyncKeyState(VK_F1) & 0x8000) != 0;
	if (curF1 && !prevF1)
	{
		showDebugColliders_ = !showDebugColliders_;
	}
	prevF1 = curF1;

	// ImGui Debug Window to verify execution and states
	ImGui::Begin("Collision Debug Overlay Settings");
	ImGui::Checkbox("Show Debug Wireframes (F1 Toggle)", &showDebugColliders_);
	ImGui::Checkbox("Show Precise Mesh Wireframe", &showMeshWireframe_);
	ImGui::Text("Active Spheres: %zu", Sphere::GetInstances().size());
	ImGui::Text("Active Object3ds: %zu", Object3d::GetInstances().size());
	ImGui::Text("Camera: %s", camera ? "Valid" : "Null");
	ImGui::End();

	if (!showDebugColliders_ || !camera) return;

	ImGuiIO& io = ImGui::GetIO();
	float width = io.DisplaySize.x;
	float height = io.DisplaySize.y;

	if (width <= 0.0f || height <= 0.0f) return;

	const Matrix4x4& vp = camera->GetViewProjectionMatrix();
	ImDrawList* drawList = ImGui::GetForegroundDrawList();

	auto project3DTo2D = [&](const Vector3& pos3D, ImVec2& outPos) -> bool {
		float w = pos3D.x * vp.m[0][3] + pos3D.y * vp.m[1][3] + pos3D.z * vp.m[2][3] + vp.m[3][3];
		if (w <= 0.0f) return false;
		float x = (pos3D.x * vp.m[0][0] + pos3D.y * vp.m[1][0] + pos3D.z * vp.m[2][0] + vp.m[3][0]) / w;
		float y = (pos3D.x * vp.m[0][1] + pos3D.y * vp.m[1][1] + pos3D.z * vp.m[2][1] + vp.m[3][1]) / w;
		outPos.x = (x + 1.0f) * 0.5f * width;
		outPos.y = (1.0f - y) * 0.5f * height;
		return true;
	};

	for (Collider* col : colliders_)
	{
		if (!col || !col->IsEnabled()) continue;

		Vector3 worldPos = col->GetWorldPosition();

		// Apply distance culling to avoid drawing tens of thousands of lines on the CPU for distant objects
		if (camera && col->GetAttribute() != CollisionAttribute::Player)
		{
			Vector3 camPos = camera->GetTranslate();
			float dx = worldPos.x - camPos.x;
			float dy = worldPos.y - camPos.y;
			float dz = worldPos.z - camPos.z;
			float distSq = dx * dx + dy * dy + dz * dz;
			if (distSq > 40.0f * 40.0f)
			{
				continue;
			}
		}

		// Determine color by attribute
		ImU32 colColor = ImGui::ColorConvertFloat4ToU32({ 1.0f, 1.0f, 1.0f, 0.7f }); // default white
		if (col->GetAttribute() == CollisionAttribute::Player)
		{
			colColor = ImGui::ColorConvertFloat4ToU32({ 0.0f, 1.0f, 0.5f, 0.8f }); // Neon green
		}
		else if (col->GetAttribute() == CollisionAttribute::Enemy)
		{
			colColor = ImGui::ColorConvertFloat4ToU32({ 1.0f, 0.2f, 0.2f, 0.8f }); // Red
		}
		else if (col->GetAttribute() == CollisionAttribute::Obstacle)
		{
			colColor = ImGui::ColorConvertFloat4ToU32({ 0.3f, 0.7f, 1.0f, 0.7f }); // Light blue
		}
		else if (col->GetAttribute() == CollisionAttribute::Bullet)
		{
			colColor = ImGui::ColorConvertFloat4ToU32({ 1.0f, 0.8f, 0.0f, 0.8f }); // Yellow
		}

		if (col->GetType() == ColliderType::Sphere)
		{
			SphereCollider* sphere = static_cast<SphereCollider*>(col);
			float radius = sphere->GetRadius();

			// Draw horizontal circle
			const int numSegments = 16;
			std::vector<ImVec2> pts2D;
			for (int i = 0; i <= numSegments; ++i)
			{
				float angle = i * (6.2831853f / numSegments);
				Vector3 p3D = {
					worldPos.x + std::cos(angle) * radius,
					worldPos.y,
					worldPos.z + std::sin(angle) * radius
				};
				ImVec2 p2D;
				if (project3DTo2D(p3D, p2D))
				{
					pts2D.push_back(p2D);
				}
			}
			if (pts2D.size() > 1)
			{
				drawList->AddPolyline(pts2D.data(), (int)pts2D.size(), colColor, false, 2.0f);
			}

			// Draw vertical circle
			pts2D.clear();
			for (int i = 0; i <= numSegments; ++i)
			{
				float angle = i * (6.2831853f / numSegments);
				Vector3 p3D = {
					worldPos.x + std::cos(angle) * radius,
					worldPos.y + std::sin(angle) * radius,
					worldPos.z
				};
				ImVec2 p2D;
				if (project3DTo2D(p3D, p2D))
				{
					pts2D.push_back(p2D);
				}
			}
			if (pts2D.size() > 1)
			{
				drawList->AddPolyline(pts2D.data(), (int)pts2D.size(), colColor, false, 2.0f);
			}
		}
		else if (col->GetType() == ColliderType::Box)
		{
			BoxCollider* box = static_cast<BoxCollider*>(col);
			Vector3 ext = box->GetExtents();
			Vector3 rot = box->GetWorldRotation();

			// 8 local corners
			Vector3 localCorners[8] = {
				{ -ext.x, -ext.y, -ext.z },
				{  ext.x, -ext.y, -ext.z },
				{  ext.x, -ext.y,  ext.z },
				{ -ext.x, -ext.y,  ext.z },
				{ -ext.x,  ext.y, -ext.z },
				{  ext.x,  ext.y, -ext.z },
				{  ext.x,  ext.y,  ext.z },
				{ -ext.x,  ext.y,  ext.z }
			};

			// Rotate and translate to world space
			Vector3 worldCorners[8];
			for (int i = 0; i < 8; ++i)
			{
				// Rotate Euler Yaw-Pitch-Roll
				// Pitch (X)
				float cosX = std::cos(rot.x);
				float sinX = std::sin(rot.x);
				Vector3 pt1 = {
					localCorners[i].x,
					localCorners[i].y * cosX - localCorners[i].z * sinX,
					localCorners[i].y * sinX + localCorners[i].z * cosX
				};

				// Yaw (Y)
				float cosY = std::cos(rot.y);
				float sinY = std::sin(rot.y);
				Vector3 pt2 = {
					pt1.x * cosY + pt1.z * sinY,
					pt1.y,
					-pt1.x * sinY + pt1.z * cosY
				};

				// Roll (Z)
				float cosZ = std::cos(rot.z);
				float sinZ = std::sin(rot.z);
				Vector3 pt3 = {
					pt2.x * cosZ - pt2.y * sinZ,
					pt2.x * sinZ + pt2.y * cosZ,
					pt2.z
				};

				worldCorners[i] = pt3 + worldPos;
			}

			// Project to 2D
			ImVec2 screenCorners[8];
			bool projected[8];
			for (int i = 0; i < 8; ++i)
			{
				projected[i] = project3DTo2D(worldCorners[i], screenCorners[i]);
			}

			// Draw bottom face
			if (projected[0] && projected[1] && projected[2] && projected[3])
			{
				ImVec2 pts[5] = { screenCorners[0], screenCorners[1], screenCorners[2], screenCorners[3], screenCorners[0] };
				drawList->AddPolyline(pts, 5, colColor, false, 2.0f);
			}
			// Draw top face
			if (projected[4] && projected[5] && projected[6] && projected[7])
			{
				ImVec2 pts[5] = { screenCorners[4], screenCorners[5], screenCorners[6], screenCorners[7], screenCorners[4] };
				drawList->AddPolyline(pts, 5, colColor, false, 2.0f);
			}
			// Draw vertical edges
			for (int i = 0; i < 4; ++i)
			{
				if (projected[i] && projected[i + 4])
				{
					drawList->AddLine(screenCorners[i], screenCorners[i + 4], colColor, 2.0f);
				}
			}
		}
		else if (col->GetType() == ColliderType::Capsule)
		{
			CapsuleCollider* capsule = static_cast<CapsuleCollider*>(col);
			float radius = capsule->GetRadius();
			float halfH = capsule->GetHeight() * 0.5f;

			Vector3 bottomCenter = worldPos - Vector3{ 0.0f, halfH, 0.0f };
			Vector3 topCenter = worldPos + Vector3{ 0.0f, halfH, 0.0f };

			// Draw bottom circle
			const int numSegments = 16;
			std::vector<ImVec2> ptsBottom;
			std::vector<ImVec2> ptsTop;
			for (int i = 0; i <= numSegments; ++i)
			{
				float angle = i * (6.2831853f / numSegments);
				Vector3 pBottom = {
					bottomCenter.x + std::cos(angle) * radius,
					bottomCenter.y,
					bottomCenter.z + std::sin(angle) * radius
				};
				Vector3 pTop = {
					topCenter.x + std::cos(angle) * radius,
					topCenter.y,
					topCenter.z + std::sin(angle) * radius
				};

				ImVec2 pB2D, pT2D;
				if (project3DTo2D(pBottom, pB2D)) ptsBottom.push_back(pB2D);
				if (project3DTo2D(pTop, pT2D)) ptsTop.push_back(pT2D);
			}
			if (ptsBottom.size() > 1) drawList->AddPolyline(ptsBottom.data(), (int)ptsBottom.size(), colColor, false, 2.0f);
			if (ptsTop.size() > 1) drawList->AddPolyline(ptsTop.data(), (int)ptsTop.size(), colColor, false, 2.0f);

			// Draw vertical side lines
			Vector3 sides[4] = {
				{  radius, 0.0f, 0.0f },
				{ -radius, 0.0f, 0.0f },
				{ 0.0f, 0.0f,  radius },
				{ 0.0f, 0.0f, -radius }
			};
			for (int i = 0; i < 4; ++i)
			{
				ImVec2 pB2D, pT2D;
				if (project3DTo2D(bottomCenter + sides[i], pB2D) && project3DTo2D(topCenter + sides[i], pT2D))
				{
					drawList->AddLine(pB2D, pT2D, colColor, 2.0f);
				}
			}
		}
		else if (col->GetType() == ColliderType::Mesh)
		{
			MeshCollider* meshCollider = static_cast<MeshCollider*>(col);
			if (meshCollider && meshCollider->GetObject3d())
			{
				// Ensure skinned positions and AABBTree are updated (cached once per frame)
				meshCollider->Update();

				Object3d* obj = meshCollider->GetObject3d();
				Matrix4x4 world = obj->GetWorldMatrix();

				// 2. Draw hierarchical AABB tree bounds (Depth 3)
				std::vector<std::pair<Vector3, Vector3>> boundsList;
				meshCollider->GetAABBTree().GetNodesAtDepth(3, boundsList);

				for (const auto& bounds : boundsList)
				{
					Vector3 minB = bounds.first;
					Vector3 maxB = bounds.second;

					// 8 local corners
					Vector3 localCorners[8] = {
						{ minB.x, minB.y, minB.z },
						{ maxB.x, minB.y, minB.z },
						{ maxB.x, minB.y, maxB.z },
						{ minB.x, minB.y, maxB.z },
						{ minB.x, maxB.y, minB.z },
						{ maxB.x, maxB.y, minB.z },
						{ maxB.x, maxB.y, maxB.z },
						{ minB.x, maxB.y, maxB.z }
					};

					Vector3 worldCorners[8];
					for (int i = 0; i < 8; ++i)
					{
						worldCorners[i] = {
							localCorners[i].x * world.m[0][0] + localCorners[i].y * world.m[1][0] + localCorners[i].z * world.m[2][0] + world.m[3][0],
							localCorners[i].x * world.m[0][1] + localCorners[i].y * world.m[1][1] + localCorners[i].z * world.m[2][1] + world.m[3][1],
							localCorners[i].x * world.m[0][2] + localCorners[i].y * world.m[1][2] + localCorners[i].z * world.m[2][2] + world.m[3][2]
						};
					}

					ImVec2 screenCorners[8];
					bool projected[8];
					for (int i = 0; i < 8; ++i)
					{
						projected[i] = project3DTo2D(worldCorners[i], screenCorners[i]);
					}

					ImU32 hierarchyColor = ImGui::ColorConvertFloat4ToU32({ 0.0f, 1.0f, 0.5f, 0.25f }); // Semi-transparent green for boxes
					if (projected[0] && projected[1] && projected[2] && projected[3])
					{
						ImVec2 pts[5] = { screenCorners[0], screenCorners[1], screenCorners[2], screenCorners[3], screenCorners[0] };
						drawList->AddPolyline(pts, 5, hierarchyColor, false, 1.0f);
					}
					if (projected[4] && projected[5] && projected[6] && projected[7])
					{
						ImVec2 pts[5] = { screenCorners[4], screenCorners[5], screenCorners[6], screenCorners[7], screenCorners[4] };
						drawList->AddPolyline(pts, 5, hierarchyColor, false, 1.0f);
					}
					for (int i = 0; i < 4; ++i)
					{
						if (projected[i] && projected[i + 4])
						{
							drawList->AddLine(screenCorners[i], screenCorners[i + 4], hierarchyColor, 1.0f);
						}
					}
				}
			}
		}
		else if (col->GetType() == ColliderType::Skeleton)
		{
			SkeletonCollider* skelCollider = static_cast<SkeletonCollider*>(col);
			if (skelCollider && skelCollider->GetObject3d())
			{
				Object3d* obj = skelCollider->GetObject3d();
				const auto& skeleton = obj->GetSkeleton();
				if (!skeleton.joints.empty())
				{
					Matrix4x4 modelWorldMatrix = MakeAffineMatrix(obj->GetScale(), obj->GetRotate(), obj->GetTranslate());
					for (size_t i = 0; i < skeleton.joints.size(); ++i)
					{
						Vector3 jointPos = skeleton.GetJointWorldPosition(i, modelWorldMatrix);
						float radius = skelCollider->GetJointRadius(skeleton.joints[i].name);

						const int numSegments = 8;
						std::vector<ImVec2> pts2D;
						for (int j = 0; j <= numSegments; ++j)
						{
							float angle = j * (6.2831853f / numSegments);
							Vector3 p3D = {
								jointPos.x + std::cos(angle) * radius,
								jointPos.y,
								jointPos.z + std::sin(angle) * radius
							};
							ImVec2 p2D;
							if (project3DTo2D(p3D, p2D)) pts2D.push_back(p2D);
						}
						if (pts2D.size() > 1) drawList->AddPolyline(pts2D.data(), (int)pts2D.size(), colColor, false, 1.5f);

						pts2D.clear();
						for (int j = 0; j <= numSegments; ++j)
						{
							float angle = j * (6.2831853f / numSegments);
							Vector3 p3D = {
								jointPos.x + std::cos(angle) * radius,
								jointPos.y + std::sin(angle) * radius,
								jointPos.z
							};
							ImVec2 p2D;
							if (project3DTo2D(p3D, p2D)) pts2D.push_back(p2D);
						}
						if (pts2D.size() > 1) drawList->AddPolyline(pts2D.data(), (int)pts2D.size(), colColor, false, 1.5f);
					}
				}
			}
		}
	}

	// --- Draw Sphere Instances (Visual only) ---
	ImU32 sphereVisualColor = ImGui::ColorConvertFloat4ToU32({ 1.0f, 0.4f, 0.7f, 0.7f }); // Pink
	for (Sphere* s : Sphere::GetInstances())
	{
		if (!s || s->IsOverlayDraw() || !s->WasDrawnLastFrame()) continue;
		Vector3 worldPos = s->GetTransform().translate;
		float radius = 1.0f * s->GetTransform().scale.x;

		// Draw horizontal circle
		const int numSegments = 16;
		std::vector<ImVec2> pts2D;
		for (int i = 0; i <= numSegments; ++i)
		{
			float angle = i * (6.2831853f / numSegments);
			Vector3 p3D = {
				worldPos.x + std::cos(angle) * radius,
				worldPos.y,
				worldPos.z + std::sin(angle) * radius
			};
			ImVec2 p2D;
			if (project3DTo2D(p3D, p2D))
			{
				pts2D.push_back(p2D);
			}
		}
		if (pts2D.size() > 1)
		{
			drawList->AddPolyline(pts2D.data(), (int)pts2D.size(), sphereVisualColor, false, 2.0f);
		}

		// Draw vertical circle
		pts2D.clear();
		for (int i = 0; i <= numSegments; ++i)
		{
			float angle = i * (6.2831853f / numSegments);
			Vector3 p3D = {
				worldPos.x + std::cos(angle) * radius,
				worldPos.y + std::sin(angle) * radius,
				worldPos.z
			};
			ImVec2 p2D;
			if (project3DTo2D(p3D, p2D))
			{
				pts2D.push_back(p2D);
			}
		}
		if (pts2D.size() > 1)
		{
			drawList->AddPolyline(pts2D.data(), (int)pts2D.size(), sphereVisualColor, false, 2.0f);
		}
	}

	// --- Draw Object3d Instances (Visual only) ---
	ImU32 objVisualColor = ImGui::ColorConvertFloat4ToU32({ 0.0f, 1.0f, 0.5f, 0.7f }); // Neon Green
	for (Object3d* obj : Object3d::GetInstances())
	{
		if (!obj || !obj->WasDrawnLastFrame()) continue;
		Vector3 worldPos = obj->GetTranslate();

		// If skeleton joints are present, draw as a Box wrapping all joint world coordinates!
		if (!obj->GetSkeleton().joints.empty())
		{
			const auto& skeleton = obj->GetSkeleton();
			Matrix4x4 modelWorldMatrix = MakeAffineMatrix(obj->GetScale(), obj->GetRotate(), obj->GetTranslate());

			float minX = 1e9f, maxX = -1e9f;
			float minY = 1e9f, maxY = -1e9f;
			float minZ = 1e9f, maxZ = -1e9f;

			for (size_t jointIndex = 0; jointIndex < skeleton.joints.size(); ++jointIndex)
			{
				Vector3 pt = skeleton.GetJointWorldPosition(jointIndex, modelWorldMatrix);
				if (pt.x < minX) minX = pt.x;
				if (pt.x > maxX) maxX = pt.x;
				if (pt.y < minY) minY = pt.y;
				if (pt.y > maxY) maxY = pt.y;
				if (pt.z < minZ) minZ = pt.z;
				if (pt.z > maxZ) maxZ = pt.z;
			}

			if (minX < maxX && minY < maxY && minZ < maxZ)
			{
				// 8 world corner coordinates
				Vector3 worldCorners[8] = {
					{ minX, minY, minZ },
					{ maxX, minY, minZ },
					{ maxX, minY, maxZ },
					{ minX, minY, maxZ },
					{ minX, maxY, minZ },
					{ maxX, maxY, minZ },
					{ maxX, maxY, maxZ },
					{ minX, maxY, maxZ }
				};

				// Project to 2D
				ImVec2 screenCorners[8];
				bool projected[8];
				for (int i = 0; i < 8; ++i)
				{
					projected[i] = project3DTo2D(worldCorners[i], screenCorners[i]);
				}

				// Draw bottom face
				if (projected[0] && projected[1] && projected[2] && projected[3])
				{
					ImVec2 pts[5] = { screenCorners[0], screenCorners[1], screenCorners[2], screenCorners[3], screenCorners[0] };
					drawList->AddPolyline(pts, 5, objVisualColor, false, 2.0f);
				}
				// Draw top face
				if (projected[4] && projected[5] && projected[6] && projected[7])
				{
					ImVec2 pts[5] = { screenCorners[4], screenCorners[5], screenCorners[6], screenCorners[7], screenCorners[4] };
					drawList->AddPolyline(pts, 5, objVisualColor, false, 2.0f);
				}
				// Draw vertical edges
				for (int i = 0; i < 4; ++i)
				{
					if (projected[i] && projected[i + 4])
					{
						drawList->AddLine(screenCorners[i], screenCorners[i + 4], objVisualColor, 2.0f);
					}
				}
			}
		}
		else
		{
			// Box representation based on mesh vertices local bounding box
			const auto& modelData = obj->GetModelData();
			if (!modelData.vertices.empty())
			{
				float minX = 1e9f, maxX = -1e9f;
				float minY = 1e9f, maxY = -1e9f;
				float minZ = 1e9f, maxZ = -1e9f;
				for (const auto& v : modelData.vertices)
				{
					if (v.position.x < minX) minX = v.position.x;
					if (v.position.x > maxX) maxX = v.position.x;
					if (v.position.y < minY) minY = v.position.y;
					if (v.position.y > maxY) maxY = v.position.y;
					if (v.position.z < minZ) minZ = v.position.z;
					if (v.position.z > maxZ) maxZ = v.position.z;
				}

				if (minX < maxX && minY < maxY && minZ < maxZ)
				{
					Vector3 rot = obj->GetRotate();
					Vector3 scale = obj->GetScale();

					// 8 local corner coordinates
					Vector3 localCorners[8] = {
						{ minX * scale.x, minY * scale.y, minZ * scale.z },
						{ maxX * scale.x, minY * scale.y, minZ * scale.z },
						{ maxX * scale.x, minY * scale.y, maxZ * scale.z },
						{ minX * scale.x, minY * scale.y, maxZ * scale.z },
						{ minX * scale.x, maxY * scale.y, minZ * scale.z },
						{ maxX * scale.x, maxY * scale.y, minZ * scale.z },
						{ maxX * scale.x, maxY * scale.y, maxZ * scale.z },
						{ minX * scale.x, maxY * scale.y, maxZ * scale.z }
					};

					// Rotate and translate to world space
					Vector3 worldCorners[8];
					for (int i = 0; i < 8; ++i)
					{
						// Rotate Euler Yaw-Pitch-Roll
						// Pitch (X)
						float cosX = std::cos(rot.x);
						float sinX = std::sin(rot.x);
						Vector3 pt1 = {
							localCorners[i].x,
							localCorners[i].y * cosX - localCorners[i].z * sinX,
							localCorners[i].y * sinX + localCorners[i].z * cosX
						};

						// Yaw (Y)
						float cosY = std::cos(rot.y);
						float sinY = std::sin(rot.y);
						Vector3 pt2 = {
							pt1.x * cosY + pt1.z * sinY,
							pt1.y,
							-pt1.x * sinY + pt1.z * cosY
						};

						// Roll (Z)
						float cosZ = std::cos(rot.z);
						float sinZ = std::sin(rot.z);
						Vector3 pt3 = {
							pt2.x * cosZ - pt2.y * sinZ,
							pt2.x * sinZ + pt2.y * cosZ,
							pt2.z
						};

						worldCorners[i] = pt3 + worldPos;
					}

					// Project to 2D
					ImVec2 screenCorners[8];
					bool projected[8];
					for (int i = 0; i < 8; ++i)
					{
						projected[i] = project3DTo2D(worldCorners[i], screenCorners[i]);
					}

					// Draw bottom face
					if (projected[0] && projected[1] && projected[2] && projected[3])
					{
						ImVec2 pts[5] = { screenCorners[0], screenCorners[1], screenCorners[2], screenCorners[3], screenCorners[0] };
						drawList->AddPolyline(pts, 5, objVisualColor, false, 2.0f);
					}
					// Draw top face
					if (projected[4] && projected[5] && projected[6] && projected[7])
					{
						ImVec2 pts[5] = { screenCorners[4], screenCorners[5], screenCorners[6], screenCorners[7], screenCorners[4] };
						drawList->AddPolyline(pts, 5, objVisualColor, false, 2.0f);
					}
					// Draw vertical edges
					for (int i = 0; i < 4; ++i)
					{
						if (projected[i] && projected[i + 4])
						{
							drawList->AddLine(screenCorners[i], screenCorners[i + 4], objVisualColor, 2.0f);
						}
					}
				}
			}
		}
	}

	// Draw last raycast line for debugging
	if (lastRaycast_.exists)
	{
		ImVec2 s2D, e2D;
		if (project3DTo2D(lastRaycast_.start, s2D) && project3DTo2D(lastRaycast_.end, e2D))
		{
			ImU32 rayColor = lastRaycast_.hit ? ImGui::ColorConvertFloat4ToU32({ 1.0f, 0.0f, 0.0f, 0.9f })  // Red if hit
			                                  : ImGui::ColorConvertFloat4ToU32({ 1.0f, 0.8f, 0.0f, 0.6f }); // Gold if miss
			drawList->AddLine(s2D, e2D, rayColor, 3.0f);
		}

		if (lastRaycast_.hit)
		{
			ImVec2 h2D;
			if (project3DTo2D(lastRaycast_.hitPoint, h2D))
			{
				drawList->AddCircleFilled(h2D, 5.0f, ImGui::ColorConvertFloat4ToU32({ 0.0f, 1.0f, 1.0f, 1.0f }), 8); // Cyan point
			}

			if (lastRaycast_.hitMesh)
			{
				ImVec2 p0, p1, p2;
				if (project3DTo2D(lastRaycast_.hitTriV0, p0) &&
					project3DTo2D(lastRaycast_.hitTriV1, p1) &&
					project3DTo2D(lastRaycast_.hitTriV2, p2))
				{
					// Draw filled red triangle
					drawList->AddTriangleFilled(p0, p1, p2, ImGui::ColorConvertFloat4ToU32({ 1.0f, 0.0f, 0.0f, 0.5f }));
					// Draw bright yellow outline
					drawList->AddTriangle(p0, p1, p2, ImGui::ColorConvertFloat4ToU32({ 1.0f, 1.0f, 0.0f, 0.9f }), 2.0f);
				}
			}
		}
	}
#endif
}
