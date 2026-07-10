#define NOMINMAX
#include "CollisionManager.h"
#include "SphereCollider.h"
#include "BoxCollider.h"
#include "CapsuleCollider.h"
#include "MeshCollider.h"
#include "SkeletonCollider.h"
#include "Matrix4x4.h"
#include "DirectXCom.h"
#include "Baziru3_Engine/Base/Allocator/StackAllocator.h"
#include "Baziru3_Engine/Scene/Manager/SceneManager.h"
#include "Baziru3_Engine/Camera/Camera.h"
#include "Baziru3_Engine/Shapes/Sphere/Sphere.h"
#include "Baziru3_Engine/3D/Object/Object3d.h"
#include <imgui.h>
#include <Windows.h>
#include <cmath>
#include <algorithm>

// =========================================================================
// ベクトル数学のインラインヘルパー関数
// =========================================================================

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
}

void CollisionManager::Finalize()
{
    colliders_.clear();
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

bool CollisionManager::ShouldCollide(CollisionAttribute a, CollisionAttribute b) const
{
    // 弾丸同士は衝突しない
    if (a == CollisionAttribute::Bullet && b == CollisionAttribute::Bullet)
    {
        return false;
    }
    // 障害物同士も衝突しない
    if (a == CollisionAttribute::Obstacle && b == CollisionAttribute::Obstacle)
    {
        return false;
    }
    return true;
}

void CollisionManager::Update()
{
    frameCount_++;

    if (colliders_.size() < 2) return;

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

    if (dataList.size() < 2) return;

    // 2. 空間ハッシュテーブルの取得と初期化 (StackAllocator を使用して動的ヒープ確保を回避)
    DirectXCom* dxCommon = SceneManager::GetInstance()->GetDirectXCom();
    if (!dxCommon) return;
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
                    colA.originalCollider->OnCollision(colB.originalCollider);
                    colB.originalCollider->OnCollision(colA.originalCollider);

                    if (!colA.isTrigger && !colB.isTrigger)
                    {
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
                    }
                }
            }
        }
    }
}

bool CollisionManager::CheckCollision(const CollisionData& a, const CollisionData& b, Vector3& outPushDir, float& outPushLen)
{
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
    return false;
}

bool CollisionManager::CheckBoxCapsule(const CollisionData& box, const CollisionData& capsule, Vector3& outPushDir, float& outPushLen)
{
    return false;
}

bool CollisionManager::CheckCapsuleCapsule(const CollisionData& a, const CollisionData& b, Vector3& outPushDir, float& outPushLen)
{
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
	static bool showDebugColliders = true;
	static bool prevF1 = false;
	bool curF1 = (GetAsyncKeyState(VK_F1) & 0x8000) != 0;
	if (curF1 && !prevF1)
	{
		showDebugColliders = !showDebugColliders;
	}
	prevF1 = curF1;

	// ImGui Debug Window to verify execution and states
	ImGui::Begin("Collision Debug Overlay Settings");
	ImGui::Checkbox("Show Debug Wireframes (F1 Toggle)", &showDebugColliders);
	ImGui::Checkbox("Show Hit-Area Mesh Scan (Wireframe)", &showMeshWireframe_);
	ImGui::Text("Active Spheres: %zu", Sphere::GetInstances().size());
	ImGui::Text("Active Object3ds: %zu", Object3d::GetInstances().size());
	ImGui::Text("Camera: %s", camera ? "Valid" : "Null");
	ImGui::End();

	if (!showDebugColliders || !camera) return;

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

		Vector3 worldPos = col->GetWorldPosition();

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
				const auto& skinnedPositions = meshCollider->GetSkinnedPositions();
				const auto& modelData = obj->GetModelData();

				// 1. Draw actual skinned mesh wireframe near the hit point (Local Scan Effect) if enabled
				if (showMeshWireframe_ && lastRaycast_.hit && lastRaycast_.hitMesh && !skinnedPositions.empty() && !modelData.indices.empty())
				{
					Matrix4x4 invWorld = Inverse(world);
					Vector3 localHitPoint = {
						lastRaycast_.hitPoint.x * invWorld.m[0][0] + lastRaycast_.hitPoint.y * invWorld.m[1][0] + lastRaycast_.hitPoint.z * invWorld.m[2][0] + invWorld.m[3][0],
						lastRaycast_.hitPoint.x * invWorld.m[0][1] + lastRaycast_.hitPoint.y * invWorld.m[1][1] + lastRaycast_.hitPoint.z * invWorld.m[2][1] + invWorld.m[3][1],
						lastRaycast_.hitPoint.x * invWorld.m[0][2] + lastRaycast_.hitPoint.y * invWorld.m[1][2] + lastRaycast_.hitPoint.z * invWorld.m[2][2] + invWorld.m[3][2]
					};

					float scanRadius = 0.5f; // Local space scan radius
					float scanRadiusSq = scanRadius * scanRadius;

					std::vector<ImVec2> projectedPts;
					projectedPts.resize(skinnedPositions.size());
					std::vector<bool> projectedValid;
					projectedValid.resize(skinnedPositions.size());

					// Only project vertices that are close to the hit point
					for (size_t i = 0; i < skinnedPositions.size(); ++i)
					{
						const auto& pt = skinnedPositions[i];
						float dx = pt.x - localHitPoint.x;
						float dy = pt.y - localHitPoint.y;
						float dz = pt.z - localHitPoint.z;
						if (dx * dx + dy * dy + dz * dz <= scanRadiusSq)
						{
							Vector3 worldPt = {
								pt.x * world.m[0][0] + pt.y * world.m[1][0] + pt.z * world.m[2][0] + world.m[3][0],
								pt.x * world.m[0][1] + pt.y * world.m[1][1] + pt.z * world.m[2][1] + world.m[3][1],
								pt.x * world.m[0][2] + pt.y * world.m[1][2] + pt.z * world.m[2][2] + world.m[3][2]
							};
							projectedValid[i] = project3DTo2D(worldPt, projectedPts[i]);
						}
						else
						{
							projectedValid[i] = false;
						}
					}

					ImU32 wireColor = ImGui::ColorConvertFloat4ToU32({ 0.0f, 1.0f, 0.5f, 0.6f }); // Glowing green wireframe
					for (size_t i = 0; i < modelData.indices.size(); i += 3)
					{
						uint32_t idx0 = modelData.indices[i];
						uint32_t idx1 = modelData.indices[i + 1];
						uint32_t idx2 = modelData.indices[i + 2];

						if (idx0 < projectedPts.size() && idx1 < projectedPts.size() && idx2 < projectedPts.size())
						{
							if (projectedValid[idx0] || projectedValid[idx1] || projectedValid[idx2])
							{
								auto ensureProjected = [&](uint32_t idx) {
									if (!projectedValid[idx])
									{
										const auto& pt = skinnedPositions[idx];
										Vector3 worldPt = {
											pt.x * world.m[0][0] + pt.y * world.m[1][0] + pt.z * world.m[2][0] + world.m[3][0],
											pt.x * world.m[0][1] + pt.y * world.m[1][1] + pt.z * world.m[2][1] + world.m[3][1],
											pt.x * world.m[0][2] + pt.y * world.m[1][2] + pt.z * world.m[2][2] + world.m[3][2]
										};
										projectedValid[idx] = project3DTo2D(worldPt, projectedPts[idx]);
									}
								};
								ensureProjected(idx0);
								ensureProjected(idx1);
								ensureProjected(idx2);

								if (projectedValid[idx0] && projectedValid[idx1])
									drawList->AddLine(projectedPts[idx0], projectedPts[idx1], wireColor, 1.2f);
								if (projectedValid[idx1] && projectedValid[idx2])
									drawList->AddLine(projectedPts[idx1], projectedPts[idx2], wireColor, 1.2f);
								if (projectedValid[idx2] && projectedValid[idx0])
									drawList->AddLine(projectedPts[idx2], projectedPts[idx0], wireColor, 1.2f);
							}
						}
					}
				}

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
