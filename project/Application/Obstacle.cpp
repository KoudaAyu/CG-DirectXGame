#include "Obstacle.h"
#include "TextureManager.h"
#include "Baziru3_Engine/Framework/Collision/CollisionManager.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <unordered_map>

// 重複ロードを防ぎシーン遷移を爆速化するための静的モデル＆AABBキャッシュ
static std::unordered_map<std::string, Object3d::ModelData> sModelCache;
static std::unordered_map<std::string, std::pair<Vector3, Vector3>> sModelBoundsCache;

namespace
{
    // リソースパス定数
    constexpr const char* kResourceDir           = "Resources";
    constexpr const char* kProjectResourcePrefix = "project/Resources/";
    constexpr const char* kDefaultFenceModel     = "duckov_fence.obj";
    constexpr const char* kLegacyFenceModel      = "fence.obj";
    constexpr const char* kLegacyWatchtowerModel = "duckov_watchtower.obj";
    constexpr const char* kWatchtowerModel       = "watchtower.obj";
    constexpr const char* kFallbackCrateModel    = "duckov_crate.obj";
    constexpr const char* kPineTreeModel         = "duckov_pine_tree.obj";
    constexpr const char* kTallPineTreeModel     = "duckov_pine_tree_tall.obj";
    constexpr const char* kBirchTreeModel        = "duckov_birch_tree.obj";

    constexpr const char* kTexGrass              = "Resources/grass.png";
    constexpr const char* kTexRiverBed           = "Resources/duckov_river_bed.png";
    constexpr const char* kTexBirchTree          = "Resources/duckov_birch_tree.png";
    constexpr const char* kTexPineTree           = "Resources/duckov_pine_tree.png";
    constexpr const char* kTexBridge             = "Resources/duckov_bridge.png";
    constexpr const char* kTexCrate              = "Resources/duckov_crate.png";
    constexpr const char* kTexSandbag            = "Resources/duckov_sandbag.png";
    constexpr const char* kTexBarrel             = "Resources/duckov_barrel.png";
    constexpr const char* kTexContainer          = "Resources/container_military.png";
    constexpr const char* kTexFence              = "Resources/duckov_fence.png";
    constexpr const char* kTexWhiteFallback      = "Resources/CG4/human/white.png";

    // 空間座標ハッシュ＆混交林バリエーション定数
    constexpr float    kGridQuantizeScale        = 10.0f;
    constexpr int32_t  kHashPrimeX               = 73856093;
    constexpr int32_t  kHashPrimeZ               = 19349663;
    constexpr uint32_t kHashShift1               = 13u;
    constexpr uint32_t kHashMixPrime             = 1274126177u;
    constexpr uint32_t kHashShift2               = 16u;
    constexpr uint32_t kPercentModulus           = 100u;
    constexpr uint32_t kPineDefaultBucketMax     = 48u;
    constexpr uint32_t kPineTallBucketMax        = 75u;

    // バウンディング＆AABB算出定数
    constexpr float   kTerrainMinBoundingRadius  = 120.0f;
    constexpr float   kSmallPropRadiusThreshold  = 1.0f;
    constexpr float   kSmallPropMinBoundingRad   = 3.0f;
    constexpr float   kBoundsInitExtent          = 1e9f;
    constexpr Vector3 kFallbackBoundsMin         = { -1.0f, 0.0f, -1.0f };
    constexpr Vector3 kFallbackBoundsMax         = {  1.0f, 2.0f,  1.0f };
    constexpr float   kMinLocalBoxAxis           = 0.2f;

    // ライティング＆マテリアル色定数
    constexpr Vector3 kSunLightDirection         = { 0.36f, -0.82f, 0.44f };
    constexpr float   kLowSpecLightIntensity     = 0.04f;
    constexpr float   kMetalReflectionFactor     = 0.04f;
    constexpr float   kMetalFresnelF0            = 0.04f;
    constexpr Vector4 kTreeBaseLightColor        = { 24.5f, 24.5f, 24.0f, 6.0f };
    constexpr Vector4 kBridgeLightColor          = { 30.5f, 29.8f, 28.6f, 1.0f };
    constexpr Vector4 kCrateColor                = { 0.72f, 0.54f, 0.32f, 1.0f };
    constexpr Vector4 kBarrelColor               = { 0.84f, 0.28f, 0.18f, 1.0f };
    constexpr Vector4 kSandbagColor              = { 0.82f, 0.75f, 0.48f, 1.0f };
    constexpr Vector4 kWatchtowerColor           = { 0.52f, 0.36f, 0.22f, 1.0f };
    constexpr Vector4 kContainerColor            = { 0.20f, 0.52f, 0.32f, 1.0f };
    constexpr Vector4 kRiverBedColor             = { 0.96f, 0.99f, 1.0f,  1.0f };
    constexpr Vector4 kFenceColor                = { 0.98f, 0.98f, 0.98f, 1.0f };
    constexpr Vector4 kGroundColor               = { 0.96f, 0.98f, 0.94f, 1.0f };
    constexpr Vector4 kNeutralFallbackColor      = { 0.68f, 0.68f, 0.68f, 1.0f };

    // 樹木の葉緑素個体差定数
    constexpr uint32_t kTreeHashTintShift        = 8u;
    constexpr uint32_t kTreeHashTintMask         = 0xFFu;
    constexpr float    kByteMaxFloat             = 255.0f;
    constexpr float    kTreeTintRBase            = 0.93f;
    constexpr float    kTreeTintRRange           = 0.07f;
    constexpr float    kTreeTintGBase            = 0.95f;
    constexpr float    kTreeTintGRange           = 0.06f;
    constexpr float    kTreeTintBBase            = 0.91f;
    constexpr float    kTreeTintBRange           = 0.06f;

    // 樹木の風揺れスウェイ定数
    constexpr float kSwayPhaseFactorX            = 0.23f;
    constexpr float kSwayPhaseFactorZ            = 0.31f;
    constexpr float kSwayPrimaryAmpX             = 0.0055f;
    constexpr float kSwayPrimaryFreqX            = 1.35f;
    constexpr float kSwaySecondaryAmpX           = 0.0022f;
    constexpr float kSwaySecondaryFreqX          = 2.85f;
    constexpr float kSwaySecondaryPhaseMul       = 1.7f;
    constexpr float kSwayAmpZ                    = 0.0045f;
    constexpr float kSwayFreqZ                   = 1.15f;
    constexpr float kSwayZPhaseMul               = 0.8f;

    // 種別ごとOBBコライダー寸法定数
    constexpr Vector3 kFenceBaseSize             = { 5.20f, 2.58f, 0.68f };
    constexpr Vector3 kWatchtowerLegBaseSize     = { 0.35f, 4.5f,  0.35f };
    constexpr float   kWatchtowerLegOffsetXZ     = 1.15f;
    constexpr float   kWatchtowerLegCenterY      = 2.25f;
    constexpr Vector3 kWatchtowerRoofBaseSize    = { 3.2f,  1.8f,  3.2f  };
    constexpr float   kWatchtowerRoofCenterY     = 5.1f;
    constexpr float   kWatchtowerCornerSigns[4][2] = {
        { -1.0f, -1.0f }, {  1.0f, -1.0f }, { -1.0f,  1.0f }, {  1.0f,  1.0f }
    };

    constexpr Vector3 kBridgeRailingBaseSize     = { 0.20f, 1.0f,  8.0f  };
    constexpr float   kBridgeRailingOffsetX      = 1.40f;
    constexpr float   kBridgeRailingOffsetY      = 0.5f;

    constexpr float   kWestBridgeCenterX         = -14.0f;
    constexpr float   kEastBridgeCenterX         =  14.0f;
    constexpr float   kBridgeWalkwayHalfWidth    =  1.40f;
    constexpr float   kRiverBarrierHeight        =  0.56f;
    constexpr float   kRiverBarrierCenterY       =  0.18f;
    constexpr float   kMinRiverSegmentWidth      =  1.0f;

    constexpr Vector3 kTreeTrunkBaseSize         = { 0.65f, 4.2f,  0.65f };
    constexpr float   kTreeTrunkOffsetY          = 2.1f;
    constexpr float   kSnugColliderScaleXZ       = 0.90f;

    struct KeywordTextureEntry
    {
        const char* keyword;
        const char* texturePath;
    };

    constexpr KeywordTextureEntry kFallbackTextureTable[] = {
        { "crate",      kTexCrate     },
        { "sandbag",    kTexSandbag   },
        { "barrel",     kTexBarrel    },
        { "bridge",     kTexBridge    },
        { "container",  kTexContainer },
        { "fence",      kTexFence     },
        { "watchtower", kTexCrate     },
    };

    struct KeywordColorEntry
    {
        const char* keyword;
        Vector4     color;
    };

    constexpr KeywordColorEntry kPropColorTable[] = {
        { "crate",      kCrateColor      },
        { "barrel",     kBarrelColor     },
        { "sandbag",    kSandbagColor    },
        { "watchtower", kWatchtowerColor },
        { "container",  kContainerColor  },
    };

    bool HasSubstring(const std::string& str, const char* sub)
    {
        return str.find(sub) != std::string::npos;
    }

    bool HasModelResource(const std::string& filename)
    {
        return std::filesystem::exists(std::string(kResourceDir) + "/" + filename) ||
               std::filesystem::exists(std::string(kProjectResourcePrefix) + filename);
    }

    uint32_t SpatialTreeHash(const Vector3& p)
    {
        const int32_t ix = static_cast<int32_t>(std::round(p.x * kGridQuantizeScale));
        const int32_t iz = static_cast<int32_t>(std::round(p.z * kGridQuantizeScale));
        uint32_t h = static_cast<uint32_t>(ix * kHashPrimeX) ^ static_cast<uint32_t>(iz * kHashPrimeZ);
        h = (h ^ (h >> kHashShift1)) * kHashMixPrime;
        return h ^ (h >> kHashShift2);
    }

    std::string ResolveTextureForModel(const std::string& visualFilename, const std::string& mtlTexturePath)
    {
        if (HasSubstring(visualFilename, "ground") || HasSubstring(visualFilename, "plane")) return kTexGrass;
        if (HasSubstring(visualFilename, "river")  || HasSubstring(visualFilename, "water")) return kTexRiverBed;
        if (HasSubstring(visualFilename, "birch"))                                           return kTexBirchTree;
        if (HasSubstring(visualFilename, "tree")   || HasSubstring(visualFilename, "Tree"))  return kTexPineTree;
        if (HasSubstring(visualFilename, "bridge") || HasSubstring(visualFilename, "Bridge"))return kTexBridge;

        if (!mtlTexturePath.empty()) return mtlTexturePath;

        for (const auto& entry : kFallbackTextureTable)
        {
            if (HasSubstring(visualFilename, entry.keyword)) return entry.texturePath;
        }
        return kTexWhiteFallback;
    }
}

void Obstacle::Initialize(Object3dCom* object3dCom, Camera* camera, const Vector3& position, float radius, const std::string& modelFilename, const Vector3& scale, const Vector3& rotation)
{
    object3dCom_ = object3dCom;
    position_ = position;
    radius_ = radius;
    rotation_ = rotation;

    // 指定されたモデルファイルをロード (キャッシュがあれば即座に再利用)
    std::string filename = modelFilename.empty() ? kDefaultFenceModel : modelFilename;
    if (filename == kLegacyFenceModel)      filename = kDefaultFenceModel;
    if (filename == kLegacyWatchtowerModel) filename = kWatchtowerModel;
    if (!HasModelResource(filename))        filename = kFallbackCrateModel;

    modelFilename_ = filename;
    isTree_ = (HasSubstring(filename, "tree") || HasSubstring(filename, "Tree"));
    isRiverWalkBarrier_ = (HasSubstring(filename, "river") || HasSubstring(filename, "water"));
    treeSwayPhase_ = position_.x * kSwayPhaseFactorX + position_.z * kSwayPhaseFactorZ;

    // 森林の樹木（duckov_pine_tree.obj）は座標ハッシュに基づき、ヨーロッパトウヒ・高木アカマツ・シラカバの自然な混交林へ自動展開
    std::string visualFilename = filename;
    const uint32_t treeHash = SpatialTreeHash(position_);
    if (filename == kPineTreeModel)
    {
        const uint32_t bucket = treeHash % kPercentModulus;
        if (bucket < kPineDefaultBucketMax)      visualFilename = kPineTreeModel;
        else if (bucket < kPineTallBucketMax)    visualFilename = kTallPineTreeModel;
        else                                     visualFilename = kBirchTreeModel;

        if (!HasModelResource(visualFilename))   visualFilename = filename;
    }

    auto it = sModelCache.find(visualFilename);
    if (it == sModelCache.end())
    {
        Object3d::ModelData loaded = Object3d::LoadObjFile(kResourceDir, visualFilename);
        loaded.material.textureFilePath = ResolveTextureForModel(visualFilename, loaded.material.textureFilePath);
        loaded.material.textureIndex = TextureManager::GetInstance()->Load(loaded.material.textureFilePath);

        float maxDistSq = 0.0f;
        for (const auto& v : loaded.vertices)
        {
            const float distSq = v.position.x * v.position.x + v.position.y * v.position.y + v.position.z * v.position.z;
            if (distSq > maxDistSq) maxDistSq = distSq;
        }
        loaded.boundingRadius = std::sqrt(maxDistSq);
        if (HasSubstring(visualFilename, "ground") || HasSubstring(visualFilename, "plane") || HasSubstring(visualFilename, "river"))
        {
            loaded.boundingRadius = (std::max)(loaded.boundingRadius, kTerrainMinBoundingRadius);
        }
        else if (loaded.boundingRadius < kSmallPropRadiusThreshold)
        {
            loaded.boundingRadius = kSmallPropMinBoundingRad;
        }

        it = sModelCache.emplace(visualFilename, std::move(loaded)).first;
    }

    const Object3d::ModelData& model = it->second;

    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(object3dCom, model);
    object3d_->SetCamera(camera);
    object3d_->SetTranslate(position_);
    object3d_->SetRotate(rotation_);
    object3d_->SetScale({ scale.x * radius_, scale.y * radius_, scale.z * radius_ });

    const bool isRiverOrWater = HasSubstring(filename, "river") || HasSubstring(filename, "water");
    const bool isGroundOrPlane = HasSubstring(filename, "ground") || HasSubstring(filename, "plane");
    const bool isTreeVisual = isTree_ || HasSubstring(visualFilename, "tree") || HasSubstring(visualFilename, "Tree") || HasSubstring(visualFilename, "birch");
    const bool isBridge = HasSubstring(filename, "bridge") || HasSubstring(filename, "Bridge");
    const bool isFence = (filename == kLegacyFenceModel) || HasSubstring(filename, "fence");
    const bool isWatchtower = HasSubstring(filename, "watchtower") || HasSubstring(filename, "WatchTower");

    // マットな自然地表（芝生・水中川底）はsRGBスペキュラ白飛びを防ぎテクスチャ本来の陰影・AOを活かし、
    // 立体構造物（柵・コンテナ・木造橋・監視塔・土嚢・針葉樹等）にはHalf-Lambert太陽光ライティングを適用
    if (isRiverOrWater || isGroundOrPlane)
    {
        object3d_->SetReflectionFactor(0.0f);
        object3d_->SetEnableLighting(false);
        object3d_->SetAllowWireframeOverlay(false);
    }
    else if (isTreeVisual)
    {
        auto& treeLight = object3d_->GetLightData();
        treeLight.directionalLight.direction = kSunLightDirection;
        treeLight.directionalLight.intensity = kLowSpecLightIntensity;
        treeLight.pointLight.intensity = 0.0f;
        object3d_->SetColor(kTreeBaseLightColor);
        object3d_->SetReflectionFactor(0.0f);
        object3d_->SetEnableLighting(true);
        object3d_->SetAllowWireframeOverlay(false);
    }
    else if (isFence || HasSubstring(filename, "container") || HasSubstring(filename, "barrel"))
    {
        object3d_->SetReflectionFactor(kMetalReflectionFactor);
        object3d_->SetFresnelF0(kMetalFresnelF0);
        object3d_->SetEnableLighting(true);
    }
    else if (isBridge)
    {
        auto& bridgeLight = object3d_->GetLightData();
        bridgeLight.directionalLight.direction = kSunLightDirection;
        bridgeLight.directionalLight.intensity = kLowSpecLightIntensity;
        bridgeLight.pointLight.intensity = 0.0f;
        object3d_->SetColor(kBridgeLightColor);
        object3d_->SetReflectionFactor(0.0f);
        object3d_->SetEnableLighting(true);
    }
    else
    {
        object3d_->SetReflectionFactor(0.0f);
        object3d_->SetEnableLighting(true);
    }

    // 頂点配列からモデル本来の正確な最小・最大座標 (AABB) を自動計算（モデル単位でキャッシュ再利用）
    Vector3 minPos = {  kBoundsInitExtent,  kBoundsInitExtent,  kBoundsInitExtent };
    Vector3 maxPos = { -kBoundsInitExtent, -kBoundsInitExtent, -kBoundsInitExtent };

    auto boundsIt = sModelBoundsCache.find(visualFilename);
    if (boundsIt != sModelBoundsCache.end())
    {
        minPos = boundsIt->second.first;
        maxPos = boundsIt->second.second;
    }
    else
    {
        if (!model.vertices.empty())
        {
            for (const auto& v : model.vertices)
            {
                minPos.x = (std::min)(minPos.x, v.position.x);
                minPos.y = (std::min)(minPos.y, v.position.y);
                minPos.z = (std::min)(minPos.z, v.position.z);
                maxPos.x = (std::max)(maxPos.x, v.position.x);
                maxPos.y = (std::max)(maxPos.y, v.position.y);
                maxPos.z = (std::max)(maxPos.z, v.position.z);
            }
        }
        else
        {
            minPos = kFallbackBoundsMin;
            maxPos = kFallbackBoundsMax;
        }
        sModelBoundsCache.emplace(visualFilename, std::make_pair(minPos, maxPos));
    }

    const Vector3 localSize = {
        (std::max)(maxPos.x - minPos.x, kMinLocalBoxAxis),
        (std::max)(maxPos.y - minPos.y, kMinLocalBoxAxis),
        (std::max)(maxPos.z - minPos.z, kMinLocalBoxAxis)
    };
    const Vector3 worldScale = { scale.x * radius_, scale.y * radius_, scale.z * radius_ };
    const Vector3 boxSize = { localSize.x * worldScale.x, localSize.y * worldScale.y, localSize.z * worldScale.z };
    const Vector3 centerOffset = {
        ((minPos.x + maxPos.x) * 0.5f) * worldScale.x,
        ((minPos.y + maxPos.y) * 0.5f) * worldScale.y,
        ((minPos.z + maxPos.z) * 0.5f) * worldScale.z
    };

    rot1_ = rotation_;
    auto registerBoxCollider = [&](const Vector3& colSize, const Vector3& colOffset, Vector3* rotSource)
    {
        auto box = std::make_unique<BoxCollider>(colSize, &position_, rotSource, CollisionAttribute::Obstacle);
        box->SetPositionOffset(colOffset);
        CollisionManager::GetInstance()->RegisterCollider(box.get());
        if (!collider_) collider_ = std::move(box);
        else            extraColliders_.push_back(std::move(box));
    };

    if (isFence)
    {
        defaultTextureIndex_ = TextureManager::GetInstance()->Load(kTexFence);
        // コンクリート基礎＋鋼鉄支柱＋金網＋有刺鉄線を隙間なくカバーする直方体OBBコライダー
        const Vector3 fenceSize = { kFenceBaseSize.x * worldScale.x, kFenceBaseSize.y * worldScale.y, kFenceBaseSize.z * worldScale.z };
        registerBoxCollider(fenceSize, { 0.0f, fenceSize.y * 0.5f, 0.0f }, &rot1_);
    }
    else if (isWatchtower)
    {
        // 監視塔：4本の柱脚と最上部フロアに精密分割コライダーを配置
        const Vector3 legSize = { kWatchtowerLegBaseSize.x * worldScale.x, kWatchtowerLegBaseSize.y * worldScale.y, kWatchtowerLegBaseSize.z * worldScale.z };
        const float lx = kWatchtowerLegOffsetXZ * worldScale.x;
        const float ly = kWatchtowerLegCenterY  * worldScale.y;
        const float lz = kWatchtowerLegOffsetXZ * worldScale.z;

        for (const auto& sign : kWatchtowerCornerSigns)
        {
            registerBoxCollider(legSize, { sign[0] * lx, ly, sign[1] * lz }, &rot1_);
        }

        const Vector3 platformSize = { kWatchtowerRoofBaseSize.x * worldScale.x, kWatchtowerRoofBaseSize.y * worldScale.y, kWatchtowerRoofBaseSize.z * worldScale.z };
        registerBoxCollider(platformSize, { 0.0f, kWatchtowerRoofCenterY * worldScale.y, 0.0f }, &rot1_);
    }
    else if (isBridge)
    {
        defaultTextureIndex_ = TextureManager::GetInstance()->Load(kTexBridge);
        // 橋：中央通路を開放し、左右の欄干のみに薄いコライダーを配置
        const Vector3 railingSize = { kBridgeRailingBaseSize.x * worldScale.x, kBridgeRailingBaseSize.y * worldScale.y, kBridgeRailingBaseSize.z * worldScale.z };
        const float railX = kBridgeRailingOffsetX * worldScale.x;
        const float railY = kBridgeRailingOffsetY * worldScale.y;
        registerBoxCollider(railingSize, { -railX, railY, 0.0f }, &rot1_);
        registerBoxCollider(railingSize, {  railX, railY, 0.0f }, &rot1_);
    }
    else if (isRiverOrWater || HasSubstring(filename, "River"))
    {
        // 川：足元高さのみ侵入不可コライダーを配置し、水面上の弾丸や視線は通過可能にする
        const float riverHalfWidth = scale.x * 0.5f;
        const float leftOuterEdge = kWestBridgeCenterX - kBridgeWalkwayHalfWidth;
        const float leftBarrierWidth = (std::max)(kMinRiverSegmentWidth, riverHalfWidth + leftOuterEdge);
        registerBoxCollider({ leftBarrierWidth, kRiverBarrierHeight, scale.z }, { leftOuterEdge - leftBarrierWidth * 0.5f, kRiverBarrierCenterY, 0.0f }, &rot1_);

        const float centerMinX = kWestBridgeCenterX + kBridgeWalkwayHalfWidth;
        const float centerMaxX = kEastBridgeCenterX - kBridgeWalkwayHalfWidth;
        const float centerBarrierWidth = (std::max)(kMinRiverSegmentWidth, centerMaxX - centerMinX);
        registerBoxCollider({ centerBarrierWidth, kRiverBarrierHeight, scale.z }, { (centerMinX + centerMaxX) * 0.5f, kRiverBarrierCenterY, 0.0f }, &rot1_);

        const float rightOuterEdge = kEastBridgeCenterX + kBridgeWalkwayHalfWidth;
        const float rightBarrierWidth = (std::max)(kMinRiverSegmentWidth, riverHalfWidth - rightOuterEdge);
        registerBoxCollider({ rightBarrierWidth, kRiverBarrierHeight, scale.z }, { rightOuterEdge + rightBarrierWidth * 0.5f, kRiverBarrierCenterY, 0.0f }, &rot1_);
    }
    else if (isGroundOrPlane || HasSubstring(filename, "Shore"))
    {
        // 地面・床面：通行可能エリアのため、障害物コライダーは登録しない
    }
    else if (isTree_)
    {
        // 樹木：枝葉の下をくぐり抜けられるよう中央の幹にのみコライダーを配置
        const Vector3 trunkSize = { kTreeTrunkBaseSize.x * worldScale.x, kTreeTrunkBaseSize.y * worldScale.y, kTreeTrunkBaseSize.z * worldScale.z };
        registerBoxCollider(trunkSize, { 0.0f, kTreeTrunkOffsetY * worldScale.y, 0.0f }, &rotation_);
    }
    else
    {
        // 汎用モデル：通路での引っ掛かり防止のため実寸よりわずかにスリムなBoxColliderを生成
        const Vector3 snugBoxSize = { boxSize.x * kSnugColliderScaleXZ, boxSize.y, boxSize.z * kSnugColliderScaleXZ };
        registerBoxCollider(snugBoxSize, centerOffset, &rotation_);
    }

    // --- オブジェクトタイプ別の識別色（Half-Lambert太陽光と接地影に調和する自然なマテリアル色）---
    if (isTreeVisual)
    {
        const float tintVar = static_cast<float>((treeHash >> kTreeHashTintShift) & kTreeHashTintMask) / kByteMaxFloat;
        const float rTint = kTreeTintRBase + kTreeTintRRange * tintVar;
        const float gTint = kTreeTintGBase + kTreeTintGRange * (1.0f - std::abs(tintVar - 0.5f));
        const float bTint = kTreeTintBBase + kTreeTintBRange * (1.0f - tintVar);
        object3d_->SetColor({
            kTreeBaseLightColor.x * rTint,
            kTreeBaseLightColor.y * gTint,
            kTreeBaseLightColor.z * bTint,
            kTreeBaseLightColor.w
        });
    }
    else
    {
        bool matchedPropColor = false;
        for (const auto& entry : kPropColorTable)
        {
            if (HasSubstring(filename, entry.keyword))
            {
                object3d_->SetColor(entry.color);
                matchedPropColor = true;
                break;
            }
        }
        if (!matchedPropColor)
        {
            if (isRiverOrWater || HasSubstring(filename, "River") || HasSubstring(filename, "Shore")) object3d_->SetColor(kRiverBedColor);
            else if (isBridge)        object3d_->SetColor(kBridgeLightColor);
            else if (isFence)         object3d_->SetColor(kFenceColor);
            else if (isGroundOrPlane) object3d_->SetColor(kGroundColor);
            else                      object3d_->SetColor(kNeutralFallbackColor);
        }
    }

    object3d_->Update();
}

void Obstacle::Update()
{
    // カメラが動いても障害物が追従しないよう、毎フレームWVP行列を更新する
    // 樹木はコライダー座標を固定したまま、梢が微風にわずかに呼吸する自然なスウェイを付与
    if (!object3d_) return;

    if (isTree_)
    {
        static const auto sStart = std::chrono::steady_clock::now();
        const float t = std::chrono::duration<float>(std::chrono::steady_clock::now() - sStart).count();
        const float swayX = kSwayPrimaryAmpX * std::sin(t * kSwayPrimaryFreqX + treeSwayPhase_)
                          + kSwaySecondaryAmpX * std::sin(t * kSwaySecondaryFreqX + treeSwayPhase_ * kSwaySecondaryPhaseMul);
        const float swayZ = kSwayAmpZ * std::cos(t * kSwayFreqZ + treeSwayPhase_ * kSwayZPhaseMul);
        object3d_->SetRotate({ rotation_.x + swayX, rotation_.y, rotation_.z + swayZ });
    }
    object3d_->Update();
}

void Obstacle::Draw(const RenderContext& ctx)
{
    if (!object3d_) return;

    RenderContext obsCtx = ctx;
    const Object3d::ModelData& modelData = object3d_->GetModelData();
    const uint32_t texIdx = (modelData.material.textureIndex != TextureManager::kInvalidTextureIndex) ? modelData.material.textureIndex : defaultTextureIndex_;
    if (texIdx != TextureManager::kInvalidTextureIndex)
    {
        obsCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(texIdx);
    }

    object3dCom_->Draw(object3d_.get(), obsCtx, modelData, true);
}

void Obstacle::Finalize()
{
    if (meshCollider_)
    {
        CollisionManager::GetInstance()->UnregisterCollider(meshCollider_.get());
        meshCollider_.reset();
    }
    if (collider_)
    {
        CollisionManager::GetInstance()->UnregisterCollider(collider_.get());
        collider_.reset();
    }
    for (auto& col : extraColliders_)
    {
        if (col)
        {
            CollisionManager::GetInstance()->UnregisterCollider(col.get());
        }
    }
    extraColliders_.clear();
    object3d_.reset();
}
