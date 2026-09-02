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

#include "SoftBodyDeformer.h"
#include "CollisionShapes.h"
#include "CollisionDebugDraw.h"

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
    
    uint32_t maskPlayer   = (1 << static_cast<uint32_t>(CollisionAttribute::Player)) |
                            (1 << static_cast<uint32_t>(CollisionAttribute::Minion)) |
                            (1 << static_cast<uint32_t>(CollisionAttribute::Enemy)) |
                            (1 << static_cast<uint32_t>(CollisionAttribute::Bullet)) |
                            (1 << static_cast<uint32_t>(CollisionAttribute::Obstacle));

    uint32_t maskMinion   = (1 << static_cast<uint32_t>(CollisionAttribute::Player)) |
                            (1 << static_cast<uint32_t>(CollisionAttribute::Minion)) |
                            (1 << static_cast<uint32_t>(CollisionAttribute::Enemy)) |
                            (1 << static_cast<uint32_t>(CollisionAttribute::Obstacle));
    
    uint32_t maskEnemy    = (1 << static_cast<uint32_t>(CollisionAttribute::Player)) |
                            (1 << static_cast<uint32_t>(CollisionAttribute::Minion)) |
                            (1 << static_cast<uint32_t>(CollisionAttribute::Bullet)) |
                            (1 << static_cast<uint32_t>(CollisionAttribute::Obstacle));
    
    uint32_t maskBullet   = (1 << static_cast<uint32_t>(CollisionAttribute::Player)) |
                            (1 << static_cast<uint32_t>(CollisionAttribute::Enemy)) |
                            (1 << static_cast<uint32_t>(CollisionAttribute::Obstacle));
    
    uint32_t maskObstacle = (1 << static_cast<uint32_t>(CollisionAttribute::Player)) |
                            (1 << static_cast<uint32_t>(CollisionAttribute::Minion)) |
                            (1 << static_cast<uint32_t>(CollisionAttribute::Enemy)) |
                            (1 << static_cast<uint32_t>(CollisionAttribute::Bullet));

    collisionMasks_[CollisionAttribute::Player]   = maskPlayer;
    collisionMasks_[CollisionAttribute::Minion]   = maskMinion;
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
    // ソフトボディ変形用の内部アニメーション時間を 1/60 秒進める
    SoftBodyDeformer::AdvanceGlobalTime(0.0166667f);

    // =========================================================================
    // 【ステップ 0】3Dモデル(Object3d)のスケール・回転とコライダーを自動同期
    // =========================================================================
    const auto& objInstances = Object3d::GetInstances();
    for (Collider* col : colliders_)
    {
        if (!col || !col->IsEnabled() || col->GetType() != ColliderType::Sphere) continue;
        SphereCollider* sphere = static_cast<SphereCollider*>(col);
        Vector3 spherePos = sphere->GetWorldPosition();

        // コライダーの真下・中心にある最も近い 3D モデルを検索
        Object3d* bestObj = nullptr;
        float bestDistSq = 0.04f; // 距離 0.2m 以内

        for (Object3d* obj : objInstances)
        {
            if (!obj) continue;
            Vector3 objPos = obj->GetTranslate();
            float dx = spherePos.x - objPos.x;
            float dy = spherePos.y - objPos.y;
            float dz = spherePos.z - objPos.z;
            float distSq = dx * dx + dy * dy + dz * dz;
            if (distSq < bestDistSq)
            {
                float objScale = obj->GetScale().x;
                float radiusDiff = std::abs(sphere->GetRadius() - objScale);
                if (radiusDiff < 1.0f)
                {
                    bestDistSq = distSq;
                    bestObj = obj;
                }
            }
        }

        if (bestObj)
        {
            const Vector3& s = bestObj->GetScale();
            float maxS = (std::max)({ std::abs(s.x), std::abs(s.y), std::abs(s.z) });
            if (maxS > 0.001f)
            {
                float normY = s.y / maxS;
                float volumeCompXZ = 1.0f;
                if (std::abs(normY) > 0.01f)
                {
                    volumeCompXZ = 1.0f / std::sqrt(std::abs(normY));
                }

                // 軟体・液体（Player/Minion）メッシュ特有のSag・接地偏平係数を自動適用
                bool isSoftBody = (sphere->GetAttribute() == CollisionAttribute::Player || sphere->GetAttribute() == CollisionAttribute::Minion);
                if (isSoftBody)
                {
                    // シェーダー(Slime.VS.hlsl)のSagFactor + 接地偏平 + 黒いアウトライン外周に100%一致する包絡楕円体
                    Vector3 slimeScale = { 1.56f, 0.88f, 1.56f };
                    sphere->SetRadius(maxS);
                    sphere->SetScale(slimeScale);
                    sphere->SetPositionOffset({ 0.0f, 0.02f * maxS, 0.0f });
                }
                else
                {
                    Vector3 deformedRatio = {
                        (s.x / maxS) * volumeCompXZ,
                        normY,
                        (s.z / maxS) * volumeCompXZ
                    };
                    sphere->SetRadius(maxS);
                    sphere->SetScale(deformedRatio);
                    sphere->SetPositionOffset({ 0.0f, 0.0f, 0.0f });
                }
            }
            sphere->SetRotation(bestObj->GetRotate());
        }
    }

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

        // アプリ側で Player 属性として登録されている半径小のコライダーを Minion として自動分類
        if (data.attribute == CollisionAttribute::Player && data.type == ColliderType::Sphere)
        {
            SphereCollider* sphere = static_cast<SphereCollider*>(col);
            if (sphere->GetRadius() <= 0.45f)
            {
                data.attribute = CollisionAttribute::Minion;
            }
        }

        // 形状ごとの固有データを事前に取得して詰め込む
        if (data.type == ColliderType::Sphere)
        {
            SphereCollider* sphere = static_cast<SphereCollider*>(col);
            data.shape.radius = sphere->GetEffectiveRadius();
            data.shape.scale = sphere->GetScale();
            data.shape.rotation = sphere->GetRotation();
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

    // =========================================================================
    // 【ステップ 1】空間ハッシュグリッドによる高速ブロードフェーズ（広域判定）
    //  ※ 空間をグリッドに区切り、同じマスにいるコライダー同士だけを判定することで
    //     計算量を O(N^2) から 平均 O(N) へと爆速化します。
    // =========================================================================
    DirectXCom* dxCommon = SceneManager::GetInstance()->GetDirectXCom();
    if (!dxCommon)
    {
        lastUpdateDurationMs_ = 0.0f;
        return;
    }
    StackAllocator* stackAllocator = dxCommon->GetStackAllocator();
    
    // ヒープ確保を避けて StackAllocator で一時バッファを 0ms 確保
    SpatialHashCell* gridTable = static_cast<SpatialHashCell*>(stackAllocator->Allocate(
        sizeof(SpatialHashCell) * kGridTableSize, alignof(SpatialHashCell)));
    std::memset(gridTable, 0, sizeof(SpatialHashCell) * kGridTableSize);

    // 各コライダーの境界（AABB）が重なるグリッドセルにコライダー番号を登録
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

        // 巨大オブジェクトが異常に広いセルを覆って処理落ちするのを防ぐ安全クランプ
        if (maxX - minX > 2) maxX = minX + 2;
        if (maxY - minY > 2) maxY = minY + 2;
        if (maxZ - minZ > 2) maxZ = minZ + 2;

        // 該当する全グリッドセルにコライダーインデックスを登録
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

    // =========================================================================
    // 【ステップ 2】ナローフェーズ（詳細判定）と重複テスト防止
    // =========================================================================
    size_t numColliders = dataList.size();
    size_t flagTableSize = numColliders * numColliders;
    uint8_t* testedFlags = static_cast<uint8_t*>(stackAllocator->Allocate(
        flagTableSize, 1));
    std::memset(testedFlags, 0, flagTableSize);

    // 同じグリッドセル内にいるペアだけを対象に精密交差判定を実行
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

                // 重複排除のためにインデックスの小さい順にソート
                uint32_t lowIdx = idxA;
                uint32_t highIdx = idxB;
                if (lowIdx > highIdx) std::swap(lowIdx, highIdx);

                // すでに同フレーム内で判定済みのペアならスキップ
                size_t flagIdx = static_cast<size_t>(lowIdx) * numColliders + highIdx;
                if (testedFlags[flagIdx] != 0) continue;
                testedFlags[flagIdx] = 1;

                // 衝突マトリクスで無効化されている属性ペアならスキップ
                if (!ShouldCollide(colA.attribute, colB.attribute))
                {
                    continue;
                }

                Vector3 pushDir = { 0.0f, 0.0f, 0.0f };
                float pushLen = 0.0f;

                // 2つのコライダーが衝突しているか判定
                if (CheckCollision(colA, colB, pushDir, pushLen))
                {
                    if (colA.isTrigger || colB.isTrigger)
                    {
                        // トリガーイベント登録 (めり込みの物理的押し出しは行わない)
                        TriggerPair pair{ colA.originalCollider, colB.originalCollider };
                        currentTriggerPairs_.push_back(pair);
                    }
                    else
                    {
                        // =====================================================
                        // 物理的衝突（めり込み解消・押し出し処理）
                        // =====================================================
                        bool isAFixed = (colA.attribute == CollisionAttribute::Obstacle);
                        bool isBFixed = (colB.attribute == CollisionAttribute::Obstacle);

                        // コライダーの新しい位置を 3D オブジェクト座標へ自動反映
                        auto syncObjectPos = [&](CollisionData& cData, const Vector3& newPos) {
                            cData.originalCollider->SetWorldPosition(newPos);
                            cData.worldPosition = newPos;
                            for (Object3d* obj : objInstances) {
                                if (!obj) continue;
                                Vector3 curObjPos = obj->GetTranslate();
                                float dx = newPos.x - curObjPos.x;
                                float dy = newPos.y - curObjPos.y;
                                float dz = newPos.z - curObjPos.z;
                                if (dx * dx + dy * dy + dz * dz < 0.3f * 0.3f) {
                                    obj->SetTranslate(newPos);
                                    break;
                                }
                            }
                        };

                        if (isAFixed && !isBFixed)
                        {
                            // Aが静止物（壁/障害物）なら、Bのみを100%押し出す
                            Vector3 newPos = colB.worldPosition + pushDir * pushLen;
                            syncObjectPos(colB, newPos);
                        }
                        else if (!isAFixed && isBFixed)
                        {
                            // Bが静止物なら、Aのみを100%押し出す
                            Vector3 newPos = colA.worldPosition - pushDir * pushLen;
                            syncObjectPos(colA, newPos);
                        }
                        else if (!isAFixed && !isBFixed)
                        {
                            // 両方が動的オブジェクトの場合: 質量比・役割に基づく適切な分配
                            float weightA = 0.5f;
                            float weightB = 0.5f;

                            bool isAMinion = (colA.attribute == CollisionAttribute::Minion || (colA.attribute == CollisionAttribute::Player && colA.shape.radius <= 0.45f));
                            bool isBMinion = (colB.attribute == CollisionAttribute::Minion || (colB.attribute == CollisionAttribute::Player && colB.shape.radius <= 0.45f));
                            bool isAPlayer = (colA.attribute == CollisionAttribute::Player && !isAMinion);
                            bool isBPlayer = (colB.attribute == CollisionAttribute::Player && !isBMinion);

                            if (isAPlayer && isBMinion)
                            {
                                weightA = 0.0f;  // 巨大スライム（プレイヤー）はビクともせず直進
                                weightB = 1.0f;  // ミニオンがシッカリと外側へ100%押し出される
                            }
                            else if (isAMinion && isBPlayer)
                            {
                                weightA = 1.0f;  // ミニオンがシッカリと外側へ100%押し出される
                                weightB = 0.0f;
                            }
                            else if (colA.attribute == CollisionAttribute::Enemy && isBMinion)
                            {
                                weightA = 0.0f;
                                weightB = 1.0f;
                            }
                            else if (isAMinion && colB.attribute == CollisionAttribute::Enemy)
                            {
                                weightA = 1.0f;
                                weightB = 0.0f;
                            }

                            Vector3 newPosA = colA.worldPosition - pushDir * (pushLen * weightA);
                            Vector3 newPosB = colB.worldPosition + pushDir * (pushLen * weightB);
                            syncObjectPos(colA, newPosA);
                            syncObjectPos(colB, newPosB);
                        }

                        // 物理的衝突コールバックを発行 (OnCollisionEnter / OnCollisionStay)
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

    // =========================================================================
    // 【ステップ 3】トリガーのライフサイクル通知（Enter / Stay / Exit）
    // =========================================================================
    // 今フレーム接触しているペアに対して Enter または Stay を発行
    for (const auto& current : currentTriggerPairs_)
    {
        auto it = std::find_if(previousTriggerPairs_.begin(), previousTriggerPairs_.end(), [&](const TriggerPair& p) {
            return (p.a == current.a && p.b == current.b) || (p.a == current.b && p.b == current.a);
        });

        if (it != previousTriggerPairs_.end())
        {
            // 前フレームから継続して重なっている => Stay（接触中）
            current.a->OnTriggerStay(current.b);
            current.b->OnTriggerStay(current.a);
        }
        else
        {
            // 今フレームで初めて重なった => Enter（侵入開始）
            current.a->OnTriggerEnter(current.b);
            current.b->OnTriggerEnter(current.a);
        }
    }

    // 前フレームで接触していたが今フレーム離れたペア => Exit（離脱）
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

    // トリガー履歴の更新（次フレームの差分判定用）
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

    return CollisionShapes::CheckCollision(a, b, outPushDir, outPushLen);
}

// =========================================================================
// 各コライダー組み合わせに対する詳細な交差判定アルゴリズム (後方互換用ラッパー)
// =========================================================================

bool CollisionManager::CheckSphereSphere(const CollisionData& a, const CollisionData& b, Vector3& outPushDir, float& outPushLen)
{
    return CollisionShapes::CheckSphereSphere(a, b, outPushDir, outPushLen);
}

bool CollisionManager::CheckSphereBox(const CollisionData& sphere, const CollisionData& box, Vector3& outPushDir, float& outPushLen)
{
    return CollisionShapes::CheckSphereBox(sphere, box, outPushDir, outPushLen);
}

bool CollisionManager::CheckSphereCapsule(const CollisionData& sphere, const CollisionData& capsule, Vector3& outPushDir, float& outPushLen)
{
    return CollisionShapes::CheckSphereCapsule(sphere, capsule, outPushDir, outPushLen);
}

bool CollisionManager::CheckBoxBox(const CollisionData& a, const CollisionData& b, Vector3& outPushDir, float& outPushLen)
{
    return CollisionShapes::CheckBoxBox(a, b, outPushDir, outPushLen);
}

bool CollisionManager::CheckBoxCapsule(const CollisionData& box, const CollisionData& capsule, Vector3& outPushDir, float& outPushLen)
{
    return CollisionShapes::CheckBoxCapsule(box, capsule, outPushDir, outPushLen);
}

bool CollisionManager::CheckCapsuleCapsule(const CollisionData& a, const CollisionData& b, Vector3& outPushDir, float& outPushLen)
{
    return CollisionShapes::CheckCapsuleCapsule(a, b, outPushDir, outPushLen);
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

	CollisionDebugDraw::Draw(colliders_, camera);
#endif
}
