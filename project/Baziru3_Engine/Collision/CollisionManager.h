#pragma once
#include "Collider.h"
#include <vector>
#include <memory>
#include <cmath>

struct CollisionData
{
    Collider* originalCollider = nullptr;
    ColliderType type;
    CollisionAttribute attribute;
    Vector3 worldPosition;
    bool isTrigger;

    struct ShapeData
    {
        float radius = 0.0f;
        float height = 0.0f;
        Vector3 size;      // Box用
        Vector3 rotation;  // Boxの回転（オイラー角）
    } shape;
};

struct SpatialHashCell
{
    static constexpr size_t kMaxColliders = 32;
    uint32_t colliderIndices[kMaxColliders];
    uint32_t count = 0;
};

/// <summary>
/// 衝突判定管理マネージャクラス
/// シーン内のすべてのコライダーを一括管理し、空間分割を用いて高速に衝突判定・押し出し解決を行います。
/// </summary>
class CollisionManager
{
public:
    static constexpr float kGridCellSize = 10.0f;
    static constexpr size_t kGridTableSize = 2048;

    /// <summary>
    /// シングルトンインスタンスを取得
    /// </summary>
    static CollisionManager* GetInstance();

    /// <summary>
    /// 初期化処理
    /// </summary>
    void Initialize();

    /// <summary>
    /// 終了処理。管理リストをクリアします。
    /// </summary>
    void Finalize();

    /// <summary>
    /// コライダーをマネージャに登録します。
    /// </summary>
    void RegisterCollider(Collider* collider);

    /// <summary>
    /// コライダーをマネージャから登録解除します。
    /// </summary>
    void UnregisterCollider(Collider* collider);

    /// <summary>
    /// すべての登録コライダーの組み合わせに対して衝突判定を実行し、
    /// 押し出し解決（めり込み補正）とコールバックのトリガーを行います。
    /// </summary>
    void Update();

    // --- 各形状ペアに対する当たり判定ヘルパー関数 ---
    
    static bool CheckSphereSphere(const CollisionData& a, const CollisionData& b, Vector3& outPushDir, float& outPushLen);
    static bool CheckSphereBox(const CollisionData& sphere, const CollisionData& box, Vector3& outPushDir, float& outPushLen);
    static bool CheckSphereCapsule(const CollisionData& sphere, const CollisionData& capsule, Vector3& outPushDir, float& outPushLen);

    static bool CheckBoxBox(const CollisionData& a, const CollisionData& b, Vector3& outPushDir, float& outPushLen);
    static bool CheckBoxCapsule(const CollisionData& box, const CollisionData& capsule, Vector3& outPushDir, float& outPushLen);

    static bool CheckCapsuleCapsule(const CollisionData& a, const CollisionData& b, Vector3& outPushDir, float& outPushLen);

    // --- レイキャスト (Raycast) 判定機能 ---
    
    /// <summary>
    /// レイ（光線）とすべてのコライダーとの交差判定を行い、最も近い衝突情報を取得します。
    /// </summary>
    bool Raycast(const Vector3& rayStart, const Vector3& rayDir, float maxDist, Collider*& outHitCollider, float& outHitDist);

    const std::vector<Collider*>& GetColliders() const { return colliders_; }

    /// <summary>
    /// すべての登録コライダーを画面上に3Dワイヤーフレーム表示します
    /// </summary>
    void DrawDebug(class Camera* camera);

    /// <summary>
    /// レイと個別コライダーの交差判定ブリッジ
    /// </summary>
    static bool CheckRayCollider(const Vector3& rayStart, const Vector3& rayDir, float maxDist, const CollisionData& collider, float& outDist);

    static bool CheckRaySphere(const Vector3& rayStart, const Vector3& rayDir, float maxDist, const CollisionData& sphere, float& outDist);
    static bool CheckRayBox(const Vector3& rayStart, const Vector3& rayDir, float maxDist, const CollisionData& box, float& outDist);
    static bool CheckRayMesh(const Vector3& rayStart, const Vector3& rayDir, float maxDist, const CollisionData& meshData, float& outDist);
    static bool CheckRaySkeleton(const Vector3& rayStart, const Vector3& rayDir, float maxDist, const CollisionData& skeletonData, float& outDist);

    uint32_t GetFrameCount() const { return frameCount_; }

private:
    CollisionManager() = default;
    ~CollisionManager() = default;
    CollisionManager(const CollisionManager&) = delete;
    CollisionManager& operator=(const CollisionManager&) = delete;

    /// <summary>
    /// 2つのコライダー間の衝突判定のブリッジ
    /// 形状タイプを解析して適切なヘルパーを呼び出します。
    /// </summary>
    bool CheckCollision(const CollisionData& a, const CollisionData& b, Vector3& outPushDir, float& outPushLen);

    /// <summary>
    /// 属性タグに基づいて、そもそも衝突判定を行うべきグループ同士かフィルタリングします。
    /// （例: 敵の弾丸同士は衝突させない等）
    /// </summary>
    bool ShouldCollide(CollisionAttribute a, CollisionAttribute b) const;

private:
    std::vector<Collider*> colliders_; // 登録された全アクティブコライダーのリスト (生ポインタ参照)
    uint32_t frameCount_ = 0;

    // 空間ハッシュ用ユーティリティ
    static int32_t CalculateGridIndex(float pos) {
        return static_cast<int32_t>(std::floor(pos / kGridCellSize));
    }
    static size_t GetHashKey(int32_t x, int32_t y, int32_t z) {
        uint32_t h = (static_cast<uint32_t>(x) * 73856093) ^
                     (static_cast<uint32_t>(y) * 19349663) ^
                     (static_cast<uint32_t>(z) * 83492791);
        return h % kGridTableSize;
    }

private:
    struct LastRaycastData {
        bool exists = false;
        Vector3 start;
        Vector3 end;
        bool hit = false;
        Vector3 hitPoint;
        bool hitMesh = false;
        Vector3 hitTriV0;
        Vector3 hitTriV1;
        Vector3 hitTriV2;
    } lastRaycast_;
};
