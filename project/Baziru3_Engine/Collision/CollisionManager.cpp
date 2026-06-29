#include "CollisionManager.h"
#include "SphereCollider.h"
#include "BoxCollider.h"
#include "CapsuleCollider.h"
#include "Matrix4x4.h"
#include <cmath>
#include <algorithm>

// =========================================================================
// 繝吶け繝医Ν謨ｰ蟄ｦ縺ｮ繧､繝ｳ繝ｩ繧､繝ｳ繝倥Ν繝代ｼ髢｢謨ｰ
// 繧ｨ繝ｳ繧ｸ繝ｳ讓呎ｺ悶ｮVector3繧ｯ繝ｩ繧ｹ縺ｫ蟇ｾ縺励※縲∬｡晉ｪ∝愛螳壹↓蠢隕√↑蝓ｺ譛ｬ貍皮ｮ励ｒ謠蝉ｾ帙＠縺ｾ縺吶
// =========================================================================



/// <summary>
/// 繝吶け繝医Ν縺ｮ蜀遨 (繝峨ャ繝育ｩ)
/// 2縺､縺ｮ繝吶け繝医Ν縺ｮ鬘樔ｼｼ蠎ｦｼ亥酔縺俶婿蜷代ｒ蜷代＞縺ｦ縺繧九°ｼ峨ｄ縲√吶け繝医Ν謚募ｽｱ縺ｮ險育ｮ励↓菴ｿ逕ｨ縺励∪縺吶
/// 蠑: a.x * b.x + a.y * b.y + a.z * b.z
/// </summary>
inline float Dot(const Vector3& a, const Vector3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

/// <summary>
/// 繝吶け繝医Ν縺ｮ髟ｷ縺輔ｮ莠御ｹ (Length Squared)
/// 蟷ｳ譁ｹ譬ｹ(sqrt)縺ｮ險育ｮ励ｯ蜃ｦ逅雋闕ｷ縺碁ｫ倥＞縺溘ａ縲∬ｷ晞屬縺ｮ豈碑ｼ縺ｮ縺ｿ繧定｡後≧蝣ｴ蜷医ｯ莠御ｹ励ｮ縺ｾ縺ｾ豈碑ｼ縺励∪縺吶
/// </summary>
inline float LengthSq(const Vector3& v)
{
    return Dot(v, v);
}

/// <summary>
/// 繝吶け繝医Ν縺ｮ髟ｷ縺 (霍晞屬)
/// 螳滄圀縺ｫ繧ｪ繝悶ず繧ｧ繧ｯ繝医ｒ遘ｻ蜍輔ｻ陬懈ｭ｣縺吶ｋ縺溘ａ縺ｮ豁｣遒ｺ縺ｪ霍晞屬繧堤ｮ怜ｺ縺励∪縺吶
/// </summary>
inline float Length(const Vector3& v)
{
    return std::sqrt(LengthSq(v));
}

/// <summary>
/// 繝吶け繝医Ν縺ｮ豁｣隕丞喧 (蜊倅ｽ阪吶け繝医Ν蛹)
/// 繝吶け繝医Ν縺ｮ髟ｷ縺輔ｒ1縺ｫ螟画鋤縺励∫ｴ皮ｲ九↑縲梧婿蜷代肴ュ蝣ｱ縺ｮ縺ｿ繧貞叙繧雁ｺ縺励∪縺吶
/// 繧ｼ繝ｭ髯､邂励ｒ髦ｲ豁｢縺吶ｋ縺溘ａ縲髟ｷ縺輔′讌ｵ繧√※繧ｼ繝ｭ縺ｫ霑代＞(1e-5f莉･荳)蝣ｴ蜷医ｯ繧ｼ繝ｭ繝吶け繝医Ν繧定ｿ斐＠縺ｾ縺吶
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
/// 繧ｯ繝ｩ繝ｳ繝鈴未謨ｰ
/// 謖螳壹＠縺溷､繧呈怙蟆丞､(min)縺ｨ譛螟ｧ蛟､(max)縺ｮ遽蝗ｲ蜀縺ｫ蛻ｶ髯舌＠縺ｾ縺吶
/// 繝懊ャ繧ｯ繧ｹ繧ｳ繝ｩ繧､繝繝ｼ荳翫ｮ譛蟇轤ｹ繧呈爾邏｢縺吶ｋ髫帙↑縺ｩ縺ｫ驥榊ｮ昴＠縺ｾ縺吶
/// </summary>
inline float Clamp(float value, float min, float max)
{
    return (std::max)(min, (std::min)(value, max));
}

// =========================================================================
// CollisionManager 繝｡繝ｳ繝舌ｼ髢｢謨ｰ螳溯｣
// =========================================================================

/// <summary>
/// 繧ｷ繝ｳ繧ｰ繝ｫ繝医Φ繧､繝ｳ繧ｹ繧ｿ繝ｳ繧ｹ縺ｮ蜿門ｾ
/// 繝励Ο繧ｰ繝ｩ繝蜈ｨ菴薙〒蜚ｯ荳縺ｮ陦晉ｪ∝愛螳壹槭ロ繝ｼ繧ｸ繝｣繧､繝ｳ繧ｹ繧ｿ繝ｳ繧ｹ繧定ｿ斐＠縺ｾ縺吶
/// </summary>
CollisionManager* CollisionManager::GetInstance()
{
    static CollisionManager instance;
    return &instance;
}

/// <summary>
/// 繝槭ロ繝ｼ繧ｸ繝｣縺ｮ蛻晄悄蛹
/// 逋ｻ骭ｲ縺輔ｌ縺ｦ縺繧九さ繝ｩ繧､繝繝ｼ縺ｮ繝ｪ繧ｹ繝医ｒ繧ｯ繝ｪ繧｢縺励∪縺吶
/// </summary>
void CollisionManager::Initialize()
{
    colliders_.clear();
}

/// <summary>
/// 繝槭ロ繝ｼ繧ｸ繝｣縺ｮ邨ゆｺ蜃ｦ逅
/// 繝ｪ繧ｽ繝ｼ繧ｹ縺ｮ繧ｯ繝ｪ繧｢繧定｡後＞縺ｾ縺吶
/// </summary>
void CollisionManager::Finalize()
{
    colliders_.clear();
}

/// <summary>
/// 繧ｳ繝ｩ繧､繝繝ｼ縺ｮ逋ｻ骭ｲ
/// 驥崎､逋ｻ骭ｲ繧帝亟縺弱▽縺､縲∝愛螳壼ｯｾ雎｡縺ｨ縺ｪ繧九い繧ｯ繝繧｣繝悶↑繧ｳ繝ｩ繧､繝繝ｼ繧偵Μ繧ｹ繝医↓霑ｽ蜉縺励∪縺吶
/// </summary>
void CollisionManager::RegisterCollider(Collider* collider)
{
    if (collider && std::find(colliders_.begin(), colliders_.end(), collider) == colliders_.end())
    {
        colliders_.push_back(collider);
    }
}

/// <summary>
/// 繧ｳ繝ｩ繧､繝繝ｼ縺ｮ逋ｻ骭ｲ隗｣髯､
/// 繧ｪ繝悶ず繧ｧ繧ｯ繝医′遐ｴ譽縺輔ｌ繧矩圀縺ｪ縺ｩ縺ｫ蜻ｼ縺ｳ蜃ｺ縺輔ｌ縲∝愛螳壼ｯｾ雎｡繝ｪ繧ｹ繝医°繧牙炎髯､縺励∪縺吶
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
/// 陦晉ｪ√ヵ繧｣繝ｫ繧ｿ繝ｪ繝ｳ繧ｰ繝ｫ繝ｼ繝ｫ
/// 迚ｹ螳壹ｮ繧ｪ繝悶ず繧ｧ繧ｯ繝医ち繧､繝怜酔螢ｫｼ井ｾ具ｼ壽雰縺ｮ蠑ｾ蜷悟｣ｫ縲∬レ譎ｯ縺ｮ驕ｮ阡ｽ迚ｩ蜷悟｣ｫｼ峨ｮ荳崎ｦ√↑蛻､螳壹ｒ繧ｹ繧ｭ繝繝励＠縺ｾ縺吶
/// </summary>
bool CollisionManager::ShouldCollide(CollisionAttribute a, CollisionAttribute b) const
{
    // 蠑ｾ荳ｸ蜷悟｣ｫ縺ｯ陦晉ｪ√＠縺ｪ縺
    if (a == CollisionAttribute::Bullet && b == CollisionAttribute::Bullet)
    {
        return false;
    }
    // 髫懷ｮｳ迚ｩｼ磯撕逧驕ｮ阡ｽ迚ｩｼ牙酔螢ｫ繧り｡晉ｪ√＠縺ｪ縺
    if (a == CollisionAttribute::Obstacle && b == CollisionAttribute::Obstacle)
    {
        return false;
    }
    return true;
}

/// <summary>
/// 陦晉ｪ∝愛螳壹♀繧医ｳ隗｣豎ｺ蜃ｦ逅縺ｮ繝｡繧､繝ｳ繝ｫ繝ｼ繝
/// 逋ｻ骭ｲ縺輔ｌ縺溷ｨ繧ｳ繝ｩ繧､繝繝ｼ縺ｫ蟇ｾ縺励※邱丞ｽ薙◆繧(O(N^2))縺ｧ莠､蟾ｮ遒ｺ隱阪ｒ陦後＞縲
/// 陦晉ｪ∵､懃衍繧､繝吶Φ繝医さ繝ｼ繝ｫ繝舌ャ繧ｯ縺ｮ逋ｺ轣ｫ縲√♀繧医ｳ迚ｩ逅逧縺ｪ謚ｼ縺玲綾縺暦ｼ医ａ繧願ｾｼ縺ｿ陬懈ｭ｣ｼ峨ｒ螳溯｡後＠縺ｾ縺吶
/// </summary>
void CollisionManager::Update()
{
    if (colliders_.size() < 2) return;

    // 莠碁阪Ν繝ｼ繝励↓繧医ｋ邱丞ｽ薙◆繧顔ｵ縺ｿ蜷医ｏ縺帛愛螳
    for (size_t i = 0; i < colliders_.size(); ++i)
    {
        Collider* colA = colliders_[i];
        if (!colA || !colA->IsEnabled()) continue;

        for (size_t j = i + 1; j < colliders_.size(); ++j)
        {
            Collider* colB = colliders_[j];
            if (!colB || !colB->IsEnabled()) continue;

            // 陦晉ｪ∝ｱ樊ｧ繝輔ぅ繝ｫ繧ｿ繝ｪ繝ｳ繧ｰ繧帝←逕ｨ縺励※荳崎ｦ√↑邨縺ｿ蜷医ｏ縺帙ｒ髯､螟
            if (!ShouldCollide(colA->GetAttribute(), colB->GetAttribute()))
            {
                continue;
            }

            Vector3 pushDir = { 0.0f, 0.0f, 0.0f }; // 陦晉ｪ∵凾縺ｮ謚ｼ縺怜ｺ縺玲婿蜷 (A縺九ｉB繧呈款縺怜ｺ縺吶吶け繝医Ν)
            float pushLen = 0.0f;                   // 陦晉ｪ∵凾縺ｮ繧√ｊ霎ｼ縺ｿ驥 (謚ｼ縺怜ｺ縺励↓蠢隕√↑霍晞屬)

            // 2縺､縺ｮ繧ｳ繝ｩ繧､繝繝ｼ髢薙〒隧ｳ邏ｰ縺ｪ莠､蟾ｮ蛻､螳壹ｒ螳溯｡
            if (CheckCollision(colA, colB, pushDir, pushLen))
            {
                // 陦晉ｪ√さ繝ｼ繝ｫ繝舌ャ繧ｯ髢｢謨ｰ繧堤嶌莠偵↓繝医Μ繧ｬ繝ｼ
                colA->OnCollision(colB);
                colB->OnCollision(colA);

                // 迚ｩ逅陦晉ｪｼ域款縺怜ｺ縺暦ｼ峨ｮ隗｣豎ｺ
                // 荳｡閠縺ｨ繧ゅ↓縲後ヨ繝ｪ繧ｬ繝ｼ蛻､螳壼ｰら畑(讀懃衍縺ｮ縺ｿ)縲阪〒縺ｯ縺ｪ縺蝣ｴ蜷医ｮ縺ｿ縲∵款縺怜ｺ縺苓｣懈ｭ｣繧帝←逕ｨ縺励∪縺吶
                if (!colA->IsTrigger() && !colB->IsTrigger())
                {
                    // 髫懷ｮｳ迚ｩ(Obstacle)縺ｯ蝗ｺ螳壼｣√ｄ蝨ｰ蠖｢縺ｨ縺励※謇ｱ縺縲∽ｽ咲ｽｮ陬懈ｭ｣縺ｫ繧医▲縺ｦ蜍輔°縺ｪ縺繧ゅｮ縺ｨ縺励∪縺吶
                    bool isAFixed = (colA->GetAttribute() == CollisionAttribute::Obstacle);
                    bool isBFixed = (colB->GetAttribute() == CollisionAttribute::Obstacle);

                    if (isAFixed && !isBFixed)
                    {
                        // A縺悟崋螳壹が繝悶ず繧ｧ繧ｯ繝医〒縲。縺檎ｧｻ蜍輔が繝悶ず繧ｧ繧ｯ繝医ｮ蝣ｴ蜷:
                        // B縺ｮ縺ｿ繧偵窟縺九ｉ髮｢繧後ｋ譁ｹ蜷(pushDir)縲阪∈縲後ａ繧願ｾｼ縺ｿ驥(pushLen)縲榊縺縺醍ｧｻ蜍輔＆縺帙※隗｣豎ｺ縺励∪縺吶
                        Vector3 newPos = colB->GetWorldPosition() + pushDir * pushLen;
                        colB->SetWorldPosition(newPos);
                    }
                    else if (!isAFixed && isBFixed)
                    {
                        // A縺檎ｧｻ蜍輔が繝悶ず繧ｧ繧ｯ繝医〒縲。縺悟崋螳壹が繝悶ず繧ｧ繧ｯ繝医ｮ蝣ｴ蜷:
                        // A縺ｮ縺ｿ繧偵沓縺九ｉ髮｢繧後ｋ譁ｹ蜷(-pushDir)縲阪∈縲後ａ繧願ｾｼ縺ｿ驥(pushLen)縲榊縺縺醍ｧｻ蜍輔＆縺帙※隗｣豎ｺ縺励∪縺吶
                        Vector3 newPos = colA->GetWorldPosition() - pushDir * pushLen;
                        colA->SetWorldPosition(newPos);
                    }
                    else if (!isAFixed && !isBFixed)
                    {
                        // 荳｡閠縺ｨ繧らｧｻ蜍募庄閭ｽ縺ｪ蜍慕噪繧ｪ繝悶ず繧ｧ繧ｯ繝亥酔螢ｫ縺ｮ蝣ｴ蜷:
                        // 蜈ｬ蟷ｳ縺ｫ蜊翫 (pushLen * 0.5f) 縺壹▽騾譁ｹ蜷代↓謚ｼ縺怜ｺ縺励※蟷ｲ貂峨ｒ隗｣豎ｺ縺励∪縺吶
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
/// 2縺､縺ｮ繧ｳ繝ｩ繧､繝繝ｼ髢薙ｮ蠖｢迥ｶ蛻･蛻､螳壽険繧雁縺
/// 繧ｳ繝ｩ繧､繝繝ｼ縺ｮ蠖｢迥ｶ繧ｿ繧､繝(Sphere, Box, Capsule)縺ｮ邨縺ｿ蜷医ｏ縺帙↓蠢懊§縺ｦ驕ｩ蛻縺ｪ謨ｰ蟄ｦ蛻､螳夐未謨ｰ縺ｸ繝悶Μ繝繧ｸ縺励∪縺吶
/// </summary>
bool CollisionManager::CheckCollision(const Collider* a, const Collider* b, Vector3& outPushDir, float& outPushLen)
{
    ColliderType typeA = a->GetType();
    ColliderType typeB = b->GetType();

    // 1. 逅 vs 逅 (Sphere - Sphere)
    if (typeA == ColliderType::Sphere && typeB == ColliderType::Sphere)
    {
        return CheckSphereSphere(a, b, outPushDir, outPushLen);
    }
    // 2. 逅 vs 繝懊ャ繧ｯ繧ｹ (Sphere - Box)
    if (typeA == ColliderType::Sphere && typeB == ColliderType::Box)
    {
        return CheckSphereBox(a, b, outPushDir, outPushLen);
    }
    if (typeA == ColliderType::Box && typeB == ColliderType::Sphere)
    {
        // 蠑墓焚縺ｮ鬆蠎上ｒ蜈･繧梧崛縺医※蛻､螳壹ｒ陦後＞縺ｾ縺吶
        // 謚ｼ縺怜ｺ縺玲婿蜷代吶け繝医Ν(outPushDir)縺ｯ縲窟縺九ｉB縺ｸ縺ｮ譁ｹ蜷代阪→縺励※邂怜ｺ縺輔ｌ繧九◆繧√
        // 蛻､螳壼ｾ後↓邨先棡縺ｮ譁ｹ蜷代ｒ蜿崎ｻ｢ (-1.0f) 縺輔○縺ｾ縺吶
        bool hit = CheckSphereBox(b, a, outPushDir, outPushLen);
        outPushDir = outPushDir * -1.0f;
        return hit;
    }
    // 3. 逅 vs 繧ｫ繝励そ繝ｫ (Sphere - Capsule)
    if (typeA == ColliderType::Sphere && typeB == ColliderType::Capsule)
    {
        return CheckSphereCapsule(a, b, outPushDir, outPushLen);
    }
    if (typeA == ColliderType::Capsule && typeB == ColliderType::Sphere)
    {
        // 逅 vs 繧ｫ繝励そ繝ｫ縺ｨ蜷梧ｧ倥↓縲鬆蠎上ｒ蜈･繧梧崛縺医※蛻､螳壹＠縲∵款縺怜ｺ縺玲婿蜷代ｒ蜿崎ｻ｢縺励∪縺吶
        bool hit = CheckSphereCapsule(b, a, outPushDir, outPushLen);
        outPushDir = outPushDir * -1.0f;
        return hit;
    }

    // 蟆譚･逧縺ｫ Box-Box, Box-Capsule 縺ｪ縺ｩ縺ｮ蛻､螳壹Ο繧ｸ繝繧ｯ縺瑚ｿｽ蜉縺輔ｌ縺溘ｉ縺薙％縺ｫ蛻蟯舌ｒ霑ｽ蜉縺励∪縺吶
    return false;
}

// =========================================================================
// 蜷繧ｳ繝ｩ繧､繝繝ｼ邨縺ｿ蜷医ｏ縺帙↓蟇ｾ縺吶ｋ隧ｳ邏ｰ縺ｪ莠､蟾ｮ蛻､螳壹い繝ｫ繧ｴ繝ｪ繧ｺ繝
// =========================================================================

/// <summary>
/// 縲千帥 vs 逅縲代ｮ陦晉ｪ∝愛螳
/// 蜴溽炊: 2縺､縺ｮ逅縺ｮ荳ｭ蠢轤ｹ髢薙ｮ霍晞屬縺後√◎繧後◇繧後ｮ蜊雁ｾ縺ｮ蜥梧悴貅縺ｧ縺ゅｌ縺ｰ陦晉ｪ√＠縺ｦ縺繧九→蛻､螳壹＠縺ｾ縺吶
/// 險育ｮ怜ｼ: |posB - posA| < radiusA + radiusB
/// </summary>
bool CollisionManager::CheckSphereSphere(const Collider* a, const Collider* b, Vector3& outPushDir, float& outPushLen)
{
    const SphereCollider* sA = dynamic_cast<const SphereCollider*>(a);
    const SphereCollider* sB = dynamic_cast<const SphereCollider*>(b);
    if (!sA || !sB) return false;

    Vector3 posA = sA->GetWorldPosition();
    Vector3 posB = sB->GetWorldPosition();

    // 2轤ｹ髢薙ｮ逶ｸ蟇ｾ繝吶け繝医Ν繧堤ｮ怜ｺ
    Vector3 dir = posB - posA;
    float dist = Length(dir); // 荳ｭ蠢轤ｹ髢薙ｮ霍晞屬
    float minDist = sA->GetRadius() + sB->GetRadius(); // 陦晉ｪ髯千阜霍晞屬ｼ亥濠蠕縺ｮ蜥鯉ｼ

    if (dist < minDist)
    {
        // 繧√ｊ霎ｼ縺ｿ驥 = 髯千阜霍晞屬 - 螳滄圀縺ｮ霍晞屬
        outPushLen = minDist - dist;
        if (dist > 1e-4f)
        {
            // 謚ｼ縺怜ｺ縺玲婿蜷代ｯA縺九ｉB縺ｸ縺ｮ譁ｹ蜷代吶け繝医Ν
            outPushDir = Normalize(dir);
        }
        else
        {
            // 荳ｭ蠢蠎ｧ讓吶′螳悟ｨ縺ｫ荳閾ｴ縺励※縺励∪縺｣縺ｦ縺繧句ｴ蜷医ｮ繝輔か繝ｼ繝ｫ繝舌ャ繧ｯｼ医ョ繝輔か繝ｫ繝医〒Z霆ｸ譁ｹ蜷代↓謚ｼ縺怜ｺ縺呻ｼ
            outPushDir = { 0.0f, 0.0f, 1.0f };
        }
        return true;
    }
    return false;
}

/// <summary>
/// 縲千帥 vs 繝懊ャ繧ｯ繧ｹ (AABB諠ｳ螳)縲代ｮ陦晉ｪ∝愛螳
/// 蜴溽炊: 繝懊ャ繧ｯ繧ｹ縺ｮ繝ｭ繝ｼ繧ｫ繝ｫ遨ｺ髢難ｼ井ｸｭ蠢繧貞次轤ｹ縺ｨ縺吶ｋ遨ｺ髢難ｼ峨↓縺翫＞縺ｦ縲∫帥縺ｮ荳ｭ蠢縺ｫ譛繧りｿ代＞繝懊ャ繧ｯ繧ｹ荳翫ｮ轤ｹｼ域怙蟇轤ｹｼ峨ｒ豎ゅａ縲
///       縺昴ｮ譛蟇轤ｹ縺ｨ逅縺ｮ荳ｭ蠢縺ｨ縺ｮ霍晞屬縺檎帥縺ｮ蜊雁ｾ譛ｪ貅縺ｧ縺ゅｋ縺九ｒ蛻､螳壹＠縺ｾ縺吶
/// </summary>
bool CollisionManager::CheckSphereBox(const Collider* sphere, const Collider* box, Vector3& outPushDir, float& outPushLen)
{
    const SphereCollider* s = dynamic_cast<const SphereCollider*>(sphere);
    const BoxCollider* b = dynamic_cast<const BoxCollider*>(box);
    if (!s || !b) return false;

    Vector3 sPos = s->GetWorldPosition();
    Vector3 bPos = b->GetWorldPosition();
    Vector3 extents = b->GetExtents(); // ボックスの各辺の半サイズ

    // 1. ボックスの回転行列 R を計算する
    Vector3 bRot = b->GetWorldRotation();
    Matrix4x4 R = Multiply(MakeRotateXMatrix(bRot.x), Multiply(MakeRotateYMatrix(bRot.y), MakeRotateZMatrix(bRot.z)));

    // ボックスのワールド空間でのローカル軸（X, Y, Z）を行列 R の行から取得する
    Vector3 axisX = { R.m[0][0], R.m[0][1], R.m[0][2] };
    Vector3 axisY = { R.m[1][0], R.m[1][1], R.m[1][2] };
    Vector3 axisZ = { R.m[2][0], R.m[2][1], R.m[2][2] };

    // 2. 球体のワールド座標系オフセットを、ボックスのローカル空間に変換する
    Vector3 offset = sPos - bPos;
    Vector3 localSphPos = {
        Dot(offset, axisX),
        Dot(offset, axisY),
        Dot(offset, axisZ)
    };

    // 3. ローカル空間上で、ボックスに最も近い点をクランプして求める
    Vector3 closestPointOnBox;
    closestPointOnBox.x = Clamp(localSphPos.x, -extents.x, extents.x);
    closestPointOnBox.y = Clamp(localSphPos.y, -extents.y, extents.y);
    closestPointOnBox.z = Clamp(localSphPos.z, -extents.z, extents.z);

    // 3.5. 球体の中心が完全にボックスの内部にある場合の処理
    if (std::abs(localSphPos.x) <= extents.x &&
        std::abs(localSphPos.y) <= extents.y &&
        std::abs(localSphPos.z) <= extents.z)
    {
        // 6つの面（左右・上下・前後）のうち、最も近い面を見つける
        float distL = extents.x + localSphPos.x; // -X面への距離
        float distR = extents.x - localSphPos.x; // +X面への距離
        float distB = extents.y + localSphPos.y; // -Y面への距離
        float distT = extents.y - localSphPos.y; // +Y面への距離
        float distF = extents.z + localSphPos.z; // -Z面への距離
        float distN = extents.z - localSphPos.z; // +Z面への距離

        float minDist = distL;
        // AからBへの方向（球からボックスへの方向）を設定するため、押し出し方向とは逆にする
        Vector3 localPushDir = { 1.0f, 0.0f, 0.0f }; // -X面が一番近い場合、ボックス内部方向は +X

        if (distR < minDist) { minDist = distR; localPushDir = { -1.0f, 0.0f, 0.0f }; }
        if (distB < minDist) { minDist = distB; localPushDir = { 0.0f, 1.0f, 0.0f }; }
        if (distT < minDist) { minDist = distT; localPushDir = { 0.0f, -1.0f, 0.0f }; }
        if (distF < minDist) { minDist = distF; localPushDir = { 0.0f, 0.0f, 1.0f }; }
        if (distN < minDist) { minDist = distN; localPushDir = { 0.0f, 0.0f, -1.0f }; }

        outPushLen = s->GetRadius() + minDist;
        outPushDir = axisX * localPushDir.x + axisY * localPushDir.y + axisZ * localPushDir.z;
        return true;
    }

    // 4. ローカル空間での押し出し方向と距離を計算する
    // AからBへの方向にするため、closestPointOnBox - localSphPos にする
    Vector3 localDir = closestPointOnBox - localSphPos;
    float dist = Length(localDir); // 距離

    if (dist < s->GetRadius())
    {
        // 押し込み量 = 半径 - 最も近い点との距離
        outPushLen = s->GetRadius() - dist;
        if (dist > 1e-4f)
        {
            Vector3 localPushDir = Normalize(localDir);
            // ローカル空間の方向をワールド空間に変換する
            outPushDir = axisX * localPushDir.x + axisY * localPushDir.y + axisZ * localPushDir.z;
        }
        else
        {
            // 球体の中心が完全にボックスの中心と重なっている場合のフォールバック（AからB＝-Z方向）
            outPushDir = axisZ * -1.0f;
        }
        return true;
    }

    return false;
}

/// <summary>
/// 縲千帥 vs 繧ｫ繝励そ繝ｫ縲代ｮ陦晉ｪ∝愛螳
/// 蜴溽炊: 繧ｫ繝励そ繝ｫ繧偵御ｸｭ蠢繧帝壹ｋ邱壼ｼ医す繝ｪ繝ｳ繝繝ｼ驛ｨ縺ｮ闃ｯｼ峨阪→縲悟濠蠕縲阪→縺励※螳夂ｾｩ縺励∪縺吶
///       繧ｫ繝励そ繝ｫ縺ｮ邱壼荳翫〒縲∫帥縺ｮ荳ｭ蠢縺ｫ譛繧りｿ代＞轤ｹｼ域怙蟇轤ｹｼ峨ｒ繝吶け繝医Ν謚募ｽｱ繧堤畑縺縺ｦ蜑ｲ繧雁ｺ縺励
///       縺昴ｮ轤ｹ縺ｨ逅縺ｮ荳ｭ蠢縺ｨ縺ｮ霍晞屬縺後檎帥縺ｮ蜊雁ｾ ｼ 繧ｫ繝励そ繝ｫ縺ｮ蜊雁ｾ縲肴悴貅縺ｧ縺ゅｋ縺九ｒ蛻､螳壹＠縺ｾ縺吶
/// </summary>
bool CollisionManager::CheckSphereCapsule(const Collider* sphere, const Collider* capsule, Vector3& outPushDir, float& outPushLen)
{
    const SphereCollider* s = dynamic_cast<const SphereCollider*>(sphere);
    const CapsuleCollider* c = dynamic_cast<const CapsuleCollider*>(capsule);
    if (!s || !c) return false;

    Vector3 sPos = s->GetWorldPosition();
    Vector3 cPos = c->GetWorldPosition();

    // 繧ｫ繝励そ繝ｫ縺ｮ荳ｭ蠢霆ｸ縺ｨ縺ｪ繧狗ｷ壼縺ｮ荳｡遶ｯ轤ｹｼ井ｸ狗ｫｯsegA, 荳顔ｫｯsegBｼ峨ｒ邂怜ｺ
    // 窶ｻ縺薙％縺ｧ縺ｯ邁｡譏鍋噪縺ｫY霆ｸ譁ｹ蜷托ｼ井ｸ頑婿蜷托ｼ峨ｒ繧ｫ繝励そ繝ｫ縺ｮ鬮倥＆譁ｹ蜷代→縺励※謇ｱ縺縺ｾ縺吶
    float halfH = c->GetHeight() * 0.5f;
    Vector3 segA = cPos - Vector3{ 0.0f, halfH, 0.0f };
    Vector3 segB = cPos + Vector3{ 0.0f, halfH, 0.0f };

    // 邱壼AB縺ｮ繝吶け繝医Ν
    Vector3 ab = segB - segA;
    // 邱壼縺ｮ蟋狗せ縺九ｉ逅縺ｮ荳ｭ蠢縺ｸ縺ｮ繝吶け繝医Ν
    Vector3 as = sPos - segA;

    // 蟆蠖ｱ豈皮紫 t 縺ｮ險育ｮ (蜀遨阪ｒ蛻ｩ逕ｨ縺励※縲∫せS繧堤峩邱哂B縺ｸ荳九ｍ縺励◆蝙らｷ壹ｮ雜ｳ繧呈ｱゅａ繧)
    // t = (as繝ｻab) / |ab|^2
    // 蛻豈阪′繧ｼ繝ｭ縺ｫ縺ｪ繧九％縺ｨ繧帝亟縺仙ｮ牙ｨ遲悶ｒ霑ｽ蜉縺励※縺縺ｾ縺
    float abLenSq = Dot(ab, ab);
    float t = 0.0f;
    if (abLenSq > 1e-5f)
    {
        t = Dot(as, ab) / abLenSq;
    }
    
    // 邱壼縺ｮ蜀蛛ｴ縺ｫ蜿弱ａ繧九◆繧√》繧 0.0 縺九ｉ 1.0 縺ｮ遽蝗ｲ縺ｫ繧ｯ繝ｩ繝ｳ繝励＠縺ｾ縺吶
    // t = 0.0 縺ｮ蝣ｴ蜷医ｯ荳狗ｫｯ轤ｹ縲》 = 1.0 縺ｮ蝣ｴ蜷医ｯ荳顔ｫｯ轤ｹ縲√◎縺ｮ髢薙ｯ邱壼荳翫ｮ轤ｹ縺ｨ縺ｪ繧翫∪縺吶
    t = Clamp(t, 0.0f, 1.0f);
    
    // 繧ｫ繝励そ繝ｫ闃ｯ縺ｮ邱壼荳翫ｮ譛蟇轤ｹ
    Vector3 closestPointOnSegment = segA + ab * t;

    // 繧ｫ繝励そ繝ｫ闃ｯ荳翫ｮ譛蟇轤ｹ縺九ｉ縲∫帥縺ｮ荳ｭ蠢縺ｸ蜷代°縺繝吶け繝医Ν
    Vector3 dir = sPos - closestPointOnSegment;
    float dist = Length(dir); // 螳滄圀縺ｮ霍晞屬
    float minDist = s->GetRadius() + c->GetRadius(); // 陦晉ｪ髯千阜霍晞屬 (荳｡蠖｢迥ｶ縺ｮ蜊雁ｾ縺ｮ蜷郁ｨ)

    if (dist < minDist)
    {
        // 繧√ｊ霎ｼ縺ｿ驥 = 陦晉ｪ髯千阜霍晞屬 - 螳滄圀縺ｮ霍晞屬
        outPushLen = minDist - dist;
        if (dist > 1e-4f)
        {
            outPushDir = Normalize(dir);
        }
        else
        {
            // 逅縺後き繝励そ繝ｫ縺ｮ荳ｭ蠢邱壹→螳悟ｨ縺ｫ驥阪↑縺｣縺ｦ縺繧句ｴ蜷医ｮ繝輔か繝ｼ繝ｫ繝舌ャ繧ｯ譁ｹ蜷
            outPushDir = { 0.0f, 0.0f, 1.0f };
        }
        return true;
    }

    return false;
}

// =========================================================================
// 莉･荳九∝ｰ譚･諡｡蠑ｵ縺ｮ縺溘ａ縺ｮ蛻､螳壹せ繧ｿ繝夜未謨ｰ鄒､ｼBox-Box, Box-Capsule, Capsule-Capsuleｼ
// 繧｢繝励Μ繧ｱ繝ｼ繧ｷ繝ｧ繝ｳ蛛ｴ縺ｮ隕∵ｱゅ↓蠢懊§縺ｦ縲∽ｻ雁ｾ後い繝ｫ繧ｴ繝ｪ繧ｺ繝繧定ｿｽ蜉繝ｻ螳溯｣蜿ｯ閭ｽ縺ｧ縺吶
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
