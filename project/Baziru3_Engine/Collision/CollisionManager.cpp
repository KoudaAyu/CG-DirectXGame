#include "CollisionManager.h"
#include "SphereCollider.h"
#include "BoxCollider.h"
#include "CapsuleCollider.h"
#include <cmath>
#include <algorithm>

// =========================================================================
// ベクトル数学のインラインヘルパー関数
// エンジン標準のVector3クラスに対して、衝突判定に必要な基本演算を提供します。
// =========================================================================



/// <summary>
/// ベクトルの内積 (ドット積)
/// 2つのベクトルの類似度（同じ方向を向いているか）や、ベクトル投影の計算に使用します。
/// 式: a.x * b.x + a.y * b.y + a.z * b.z
/// </summary>
inline float Dot(const Vector3& a, const Vector3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

/// <summary>
/// ベクトルの長さの二乗 (Length Squared)
/// 平方根(sqrt)の計算は処理負荷が高いため、距離の比較のみを行う場合は二乗のまま比較します。
/// </summary>
inline float LengthSq(const Vector3& v)
{
    return Dot(v, v);
}

/// <summary>
/// ベクトルの長さ (距離)
/// 実際にオブジェクトを移動・補正するための正確な距離を算出します。
/// </summary>
inline float Length(const Vector3& v)
{
    return std::sqrt(LengthSq(v));
}

/// <summary>
/// ベクトルの正規化 (単位ベクトル化)
/// ベクトルの長さを1に変換し、純粋な「方向」情報のみを取り出します。
/// ゼロ除算を防止するため、長さが極めてゼロに近い(1e-5f以下)場合はゼロベクトルを返します。
/// </summary>
inline Vector3 Normalize(const Vector3& v)
{
    float len = Length(v);
    if (len > 1e-5f)
    {
        return v * (1.0f / len);
    }
    return { 0.0f, 0.0f, 0.0f };
}

/// <summary>
/// クランプ関数
/// 指定した値を最小値(min)と最大値(max)の範囲内に制限します。
/// ボックスコライダー上の最寄点を探索する際などに重宝します。
/// </summary>
inline float Clamp(float value, float min, float max)
{
    return (std::max)(min, (std::min)(value, max));
}

// =========================================================================
// CollisionManager メンバー関数実装
// =========================================================================

/// <summary>
/// シングルトンインスタンスの取得
/// プログラム全体で唯一の衝突判定マネージャインスタンスを返します。
/// </summary>
CollisionManager* CollisionManager::GetInstance()
{
    static CollisionManager instance;
    return &instance;
}

/// <summary>
/// マネージャの初期化
/// 登録されているコライダーのリストをクリアします。
/// </summary>
void CollisionManager::Initialize()
{
    colliders_.clear();
}

/// <summary>
/// マネージャの終了処理
/// リソースのクリアを行います。
/// </summary>
void CollisionManager::Finalize()
{
    colliders_.clear();
}

/// <summary>
/// コライダーの登録
/// 重複登録を防ぎつつ、判定対象となるアクティブなコライダーをリストに追加します。
/// </summary>
void CollisionManager::RegisterCollider(Collider* collider)
{
    if (collider && std::find(colliders_.begin(), colliders_.end(), collider) == colliders_.end())
    {
        colliders_.push_back(collider);
    }
}

/// <summary>
/// コライダーの登録解除
/// オブジェクトが破棄される際などに呼び出され、判定対象リストから削除します。
/// </summary>
void CollisionManager::UnregisterCollider(Collider* collider)
{
    auto it = std::remove(colliders_.begin(), colliders_.end(), collider);
    if (it != colliders_.end())
    {
        colliders_.erase(it, colliders_.end());
    }
}

/// <summary>
/// 衝突フィルタリングルール
/// 特定のオブジェクトタイプ同士（例：敵の弾同士、背景の遮蔽物同士）の不要な判定をスキップします。
/// </summary>
bool CollisionManager::ShouldCollide(CollisionAttribute a, CollisionAttribute b) const
{
    // 弾丸同士は衝突しない
    if (a == CollisionAttribute::Bullet && b == CollisionAttribute::Bullet)
    {
        return false;
    }
    // 障害物（静的遮蔽物）同士も衝突しない
    if (a == CollisionAttribute::Obstacle && b == CollisionAttribute::Obstacle)
    {
        return false;
    }
    return true;
}

/// <summary>
/// 衝突判定および解決処理のメインループ
/// 登録された全コライダーに対して総当たり(O(N^2))で交差確認を行い、
/// 衝突検知イベントコールバックの発火、および物理的な押し戻し（めり込み補正）を実行します。
/// </summary>
void CollisionManager::Update()
{
    if (colliders_.size() < 2) return;

    // 二重ループによる総当たり組み合わせ判定
    for (size_t i = 0; i < colliders_.size(); ++i)
    {
        Collider* colA = colliders_[i];
        if (!colA || !colA->IsEnabled()) continue;

        for (size_t j = i + 1; j < colliders_.size(); ++j)
        {
            Collider* colB = colliders_[j];
            if (!colB || !colB->IsEnabled()) continue;

            // 衝突属性フィルタリングを適用して不要な組み合わせを除外
            if (!ShouldCollide(colA->GetAttribute(), colB->GetAttribute()))
            {
                continue;
            }

            Vector3 pushDir = { 0.0f, 0.0f, 0.0f }; // 衝突時の押し出し方向 (AからBを押し出すベクトル)
            float pushLen = 0.0f;                   // 衝突時のめり込み量 (押し出しに必要な距離)

            // 2つのコライダー間で詳細な交差判定を実行
            if (CheckCollision(colA, colB, pushDir, pushLen))
            {
                // 衝突コールバック関数を相互にトリガー
                colA->OnCollision(colB);
                colB->OnCollision(colA);

                // 物理衝突（押し出し）の解決
                // 両者ともに「トリガー判定専用(検知のみ)」ではない場合のみ、押し出し補正を適用します。
                if (!colA->IsTrigger() && !colB->IsTrigger())
                {
                    // 障害物(Obstacle)は固定壁や地形として扱い、位置補正によって動かないものとします。
                    bool isAFixed = (colA->GetAttribute() == CollisionAttribute::Obstacle);
                    bool isBFixed = (colB->GetAttribute() == CollisionAttribute::Obstacle);

                    if (isAFixed && !isBFixed)
                    {
                        // Aが固定オブジェクトで、Bが移動オブジェクトの場合:
                        // Bのみを「Aから離れる方向(pushDir)」へ「めり込み量(pushLen)」分だけ移動させて解決します。
                        Vector3 newPos = colB->GetWorldPosition() + pushDir * pushLen;
                        colB->SetWorldPosition(newPos);
                    }
                    else if (!isAFixed && isBFixed)
                    {
                        // Aが移動オブジェクトで、Bが固定オブジェクトの場合:
                        // Aのみを「Bから離れる方向(-pushDir)」へ「めり込み量(pushLen)」分だけ移動させて解決します。
                        Vector3 newPos = colA->GetWorldPosition() - pushDir * pushLen;
                        colA->SetWorldPosition(newPos);
                    }
                    else if (!isAFixed && !isBFixed)
                    {
                        // 両者とも移動可能な動的オブジェクト同士の場合:
                        // 公平に半々 (pushLen * 0.5f) ずつ逆方向に押し出して干渉を解決します。
                        Vector3 newPosA = colA->GetWorldPosition() - pushDir * (pushLen * 0.5f);
                        Vector3 newPosB = colB->GetWorldPosition() + pushDir * (pushLen * 0.5f);
                        colA->SetWorldPosition(newPosA);
                        colB->SetWorldPosition(newPosB);
                    }
                }
            }
        }
    }
}

/// <summary>
/// 2つのコライダー間の形状別判定振り分け
/// コライダーの形状タイプ(Sphere, Box, Capsule)の組み合わせに応じて適切な数学判定関数へブリッジします。
/// </summary>
bool CollisionManager::CheckCollision(const Collider* a, const Collider* b, Vector3& outPushDir, float& outPushLen)
{
    ColliderType typeA = a->GetType();
    ColliderType typeB = b->GetType();

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
        // 引数の順序を入れ替えて判定を行います。
        // 押し出し方向ベクトル(outPushDir)は「AからBへの方向」として算出されるため、
        // 判定後に結果の方向を反転 (-1.0f) させます。
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
        // 球 vs カプセルと同様に、順序を入れ替えて判定し、押し出し方向を反転します。
        bool hit = CheckSphereCapsule(b, a, outPushDir, outPushLen);
        outPushDir = outPushDir * -1.0f;
        return hit;
    }

    // 将来的に Box-Box, Box-Capsule などの判定ロジックが追加されたらここに分岐を追加します。
    return false;
}

// =========================================================================
// 各コライダー組み合わせに対する詳細な交差判定アルゴリズム
// =========================================================================

/// <summary>
/// 【球 vs 球】の衝突判定
/// 原理: 2つの球の中心点間の距離が、それぞれの半径の和未満であれば衝突していると判定します。
/// 計算式: |posB - posA| < radiusA + radiusB
/// </summary>
bool CollisionManager::CheckSphereSphere(const Collider* a, const Collider* b, Vector3& outPushDir, float& outPushLen)
{
    const SphereCollider* sA = dynamic_cast<const SphereCollider*>(a);
    const SphereCollider* sB = dynamic_cast<const SphereCollider*>(b);
    if (!sA || !sB) return false;

    Vector3 posA = sA->GetWorldPosition();
    Vector3 posB = sB->GetWorldPosition();

    // 2点間の相対ベクトルを算出
    Vector3 dir = posB - posA;
    float dist = Length(dir); // 中心点間の距離
    float minDist = sA->GetRadius() + sB->GetRadius(); // 衝突限界距離（半径の和）

    if (dist < minDist)
    {
        // めり込み量 = 限界距離 - 実際の距離
        outPushLen = minDist - dist;
        if (dist > 1e-4f)
        {
            // 押し出し方向はAからBへの方向ベクトル
            outPushDir = Normalize(dir);
        }
        else
        {
            // 中心座標が完全に一致してしまっている場合のフォールバック（デフォルトでZ軸方向に押し出す）
            outPushDir = { 0.0f, 0.0f, 1.0f };
        }
        return true;
    }
    return false;
}

/// <summary>
/// 【球 vs ボックス (AABB想定)】の衝突判定
/// 原理: ボックスのローカル空間（中心を原点とする空間）において、球の中心に最も近いボックス上の点（最寄点）を求め、
///       その最寄点と球の中心との距離が球の半径未満であるかを判定します。
/// </summary>
bool CollisionManager::CheckSphereBox(const Collider* sphere, const Collider* box, Vector3& outPushDir, float& outPushLen)
{
    const SphereCollider* s = dynamic_cast<const SphereCollider*>(sphere);
    const BoxCollider* b = dynamic_cast<const BoxCollider*>(box);
    if (!s || !b) return false;

    Vector3 sPos = s->GetWorldPosition();
    Vector3 bPos = b->GetWorldPosition();
    Vector3 extents = b->GetExtents(); // ボックスの各軸の半サイズ

    // ボックスの中心を原点とした空間における球の相対座標
    Vector3 localSphPos = sPos - bPos;

    // ボックスの境界（[-Extents, Extents]の範囲）に球の座標をクランプし、
    // ボックス表面または内部で「球に最も近い点」を求めます。
    Vector3 closestPointOnBox;
    closestPointOnBox.x = Clamp(localSphPos.x, -extents.x, extents.x);
    closestPointOnBox.y = Clamp(localSphPos.y, -extents.y, extents.y);
    closestPointOnBox.z = Clamp(localSphPos.z, -extents.z, extents.z);

    // ボックス上の最寄点から球の中心へ向かうベクトル
    Vector3 dir = localSphPos - closestPointOnBox;
    float dist = Length(dir); // その距離

    if (dist < s->GetRadius())
    {
        // めり込み量 = 球の半径 - 最寄点との距離
        outPushLen = s->GetRadius() - dist;
        if (dist > 1e-4f)
        {
            outPushDir = Normalize(dir);
        }
        else
        {
            // 球の中心がボックスの重心と完全に一致している場合の避難方向
            outPushDir = { 0.0f, 0.0f, 1.0f };
        }
        return true;
    }

    return false;
}

/// <summary>
/// 【球 vs カプセル】の衝突判定
/// 原理: カプセルを「中心を通る線分（シリンダー部の芯）」と「半径」として定義します。
///       カプセルの線分上で、球の中心に最も近い点（最寄点）をベクトル投影を用いて割り出し、
///       その点と球の中心との距離が「球の半径 ＋ カプセルの半径」未満であるかを判定します。
/// </summary>
bool CollisionManager::CheckSphereCapsule(const Collider* sphere, const Collider* capsule, Vector3& outPushDir, float& outPushLen)
{
    const SphereCollider* s = dynamic_cast<const SphereCollider*>(sphere);
    const CapsuleCollider* c = dynamic_cast<const CapsuleCollider*>(capsule);
    if (!s || !c) return false;

    Vector3 sPos = s->GetWorldPosition();
    Vector3 cPos = c->GetWorldPosition();

    // カプセルの中心軸となる線分の両端点（下端segA, 上端segB）を算出
    // ※ここでは簡易的にY軸方向（上方向）をカプセルの高さ方向として扱います。
    float halfH = c->GetHeight() * 0.5f;
    Vector3 segA = cPos - Vector3{ 0.0f, halfH, 0.0f };
    Vector3 segB = cPos + Vector3{ 0.0f, halfH, 0.0f };

    // 線分ABのベクトル
    Vector3 ab = segB - segA;
    // 線分の始点から球の中心へのベクトル
    Vector3 as = sPos - segA;

    // 射影比率 t の計算 (内積を利用して、点Sを直線ABへ下ろした垂線の足を求める)
    // t = (as・ab) / |ab|^2
    // 分母がゼロになることを防ぐ安全策を追加しています
    float abLenSq = Dot(ab, ab);
    float t = 0.0f;
    if (abLenSq > 1e-5f)
    {
        t = Dot(as, ab) / abLenSq;
    }
    
    // 線分の内側に収めるため、tを 0.0 から 1.0 の範囲にクランプします。
    // t = 0.0 の場合は下端点、t = 1.0 の場合は上端点、その間は線分上の点となります。
    t = Clamp(t, 0.0f, 1.0f);
    
    // カプセル芯の線分上の最寄点
    Vector3 closestPointOnSegment = segA + ab * t;

    // カプセル芯上の最寄点から、球の中心へ向かうベクトル
    Vector3 dir = sPos - closestPointOnSegment;
    float dist = Length(dir); // 実際の距離
    float minDist = s->GetRadius() + c->GetRadius(); // 衝突限界距離 (両形状の半径の合計)

    if (dist < minDist)
    {
        // めり込み量 = 衝突限界距離 - 実際の距離
        outPushLen = minDist - dist;
        if (dist > 1e-4f)
        {
            outPushDir = Normalize(dir);
        }
        else
        {
            // 球がカプセルの中心線と完全に重なっている場合のフォールバック方向
            outPushDir = { 0.0f, 0.0f, 1.0f };
        }
        return true;
    }

    return false;
}

// =========================================================================
// 以下、将来拡張のための判定スタブ関数群（Box-Box, Box-Capsule, Capsule-Capsule）
// アプリケーション側の要求に応じて、今後アルゴリズムを追加・実装可能です。
// =========================================================================

bool CollisionManager::CheckBoxBox(const Collider* a, const Collider* b, Vector3& outPushDir, float& outPushLen)
{
    return false;
}

bool CollisionManager::CheckBoxCapsule(const Collider* box, const Collider* capsule, Vector3& outPushDir, float& outPushLen)
{
    return false;
}

bool CollisionManager::CheckCapsuleCapsule(const Collider* a, const Collider* b, Vector3& outPushDir, float& outPushLen)
{
    return false;
}
