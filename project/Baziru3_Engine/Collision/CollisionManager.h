#pragma once
#include "Collider.h"
#include <vector>
#include <memory>

/// <summary>
/// 衝突判定管理マネージャクラス
/// シーン内のすべてのコライダーを一括管理し、総当たりで衝突判定・押し出し解決を行います。
/// </summary>
class CollisionManager
{
public:
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
    
    static bool CheckSphereSphere(const Collider* a, const Collider* b, Vector3& outPushDir, float& outPushLen);
    static bool CheckSphereBox(const Collider* sphere, const Collider* box, Vector3& outPushDir, float& outPushLen);
    static bool CheckSphereCapsule(const Collider* sphere, const Collider* capsule, Vector3& outPushDir, float& outPushLen);

    static bool CheckBoxBox(const Collider* a, const Collider* b, Vector3& outPushDir, float& outPushLen);
    static bool CheckBoxCapsule(const Collider* box, const Collider* capsule, Vector3& outPushDir, float& outPushLen);

    static bool CheckCapsuleCapsule(const Collider* a, const Collider* b, Vector3& outPushDir, float& outPushLen);

private:
    CollisionManager() = default;
    ~CollisionManager() = default;
    CollisionManager(const CollisionManager&) = delete;
    CollisionManager& operator=(const CollisionManager&) = delete;

    /// <summary>
    /// 2つのコライダー間の衝突判定のブリッジ
    /// 形状タイプを解析して適切なヘルパーを呼び出します。
    /// </summary>
    bool CheckCollision(const Collider* a, const Collider* b, Vector3& outPushDir, float& outPushLen);

    /// <summary>
    /// 属性タグに基づいて、そもそも衝突判定を行うべきグループ同士かフィルタリングします。
    /// （例: 敵の弾丸同士は衝突させない等）
    /// </summary>
    bool ShouldCollide(CollisionAttribute a, CollisionAttribute b) const;

private:
    std::vector<Collider*> colliders_; // 登録された全アクティブコライダーのリスト (生ポインタ参照)
};
