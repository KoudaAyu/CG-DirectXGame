#include "Obstacle.h"
#include "TextureManager.h"
#include "Application/Audio/TacticalAudio.h"
#include "Application/Config/AppFastObjLoader.h"
#include "Application/Player/DuckPartRig.h"
#include "Baziru3_Engine/Framework/Collision/CollisionManager.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
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
    constexpr const char* kTexBreakableWoodCrate = "Resources/duckov_wood_crate.png";
    constexpr const char* kTexDamagedWoodCrate   = "Resources/duckov_wood_crate_damaged.png";
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
    camera_ = camera;
    position_ = position;
    radius_ = radius;
    rotation_ = rotation;
    isBreakableSupplyCrate_ = false;
    isExplosiveBarrel_ = false;
    isBarrelLeaking_ = false;
    barrelHp_ = 3;
    barrelFuseTimer_ = 2.6f;
    customTextureIndex_ = UINT32_MAX;
    debrisPieces_.clear();

    // 指定されたモデルファイルをロード (キャッシュがあれば即座に再利用)
    std::string filename = modelFilename.empty() ? kDefaultFenceModel : modelFilename;
    if (filename == kLegacyFenceModel)      filename = kDefaultFenceModel;
    if (filename == kLegacyWatchtowerModel) filename = kWatchtowerModel;
    if (!HasModelResource(filename))        filename = kFallbackCrateModel;

    modelFilename_ = filename;
    isTree_ = (HasSubstring(filename, "tree") || HasSubstring(filename, "Tree"));
    isExplosiveBarrel_ = HasSubstring(filename, "barrel");
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
    isBirchTree_ = HasSubstring(visualFilename, "birch");

    auto it = sModelCache.find(visualFilename);
    if (it == sModelCache.end())
    {
        Object3d::ModelData loaded = AppFastObjLoader::LoadObjCached(kResourceDir, visualFilename);
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

    baseScale_ = { scale.x * radius_, scale.y * radius_, scale.z * radius_ };
    isBroken_ = false;
    hitFlinchTimer_ = 0.0f;

    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(object3dCom, model);
    object3d_->SetCamera(camera);
    object3d_->SetTranslate(position_);
    object3d_->SetRotate(rotation_);
    object3d_->SetScale(baseScale_);

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
    else if (HasSubstring(filename, "crate"))
    {
        // 木箱天面のBlinn-Phong白色スペキュラ潰れを抑制し、天面・側面とも木目とステンシルが鮮明に見えるマット木材ライティング
        auto& crateLight = object3d_->GetLightData();
        crateLight.directionalLight.direction = kSunLightDirection;
        crateLight.directionalLight.intensity = kLowSpecLightIntensity;
        crateLight.pointLight.intensity = 0.0f;
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
        baseColor_ = {
            kTreeBaseLightColor.x * rTint,
            kTreeBaseLightColor.y * gTint,
            kTreeBaseLightColor.z * bTint,
            kTreeBaseLightColor.w
        };
    }
    else
    {
        bool matchedPropColor = false;
        if (HasSubstring(filename, "crate"))
        {
            baseColor_ = { 18.0f * kCrateColor.x, 18.0f * kCrateColor.y, 18.0f * kCrateColor.z, 1.0f };
            matchedPropColor = true;
        }
        else
        {
            for (const auto& entry : kPropColorTable)
            {
                if (HasSubstring(filename, entry.keyword))
                {
                    baseColor_ = entry.color;
                    matchedPropColor = true;
                    break;
                }
            }
        }
        if (!matchedPropColor)
        {
            if (isRiverOrWater || HasSubstring(filename, "River") || HasSubstring(filename, "Shore")) baseColor_ = kRiverBedColor;
            else if (isBridge)        baseColor_ = kBridgeLightColor;
            else if (isFence)         baseColor_ = kFenceColor;
            else if (isGroundOrPlane) baseColor_ = kGroundColor;
            else                      baseColor_ = kNeutralFallbackColor;
        }
    }

    object3d_->SetColor(baseColor_);
    object3d_->Update();
}

namespace
{
    float CrateRandRange(float a, float b)
    {
        return a + (b - a) * (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX));
    }

    const Object3d::ModelData& GetBreakablePlankModel(uint32_t woodTexIdx)
    {
        static Object3d::ModelData sPlankModel{};
        static bool sBuilt = false;
        if (!sBuilt)
        {
            sBuilt = true;
            // 木箱の木目＆黄色ステンシル帯を自然に切り出した立体木板メッシュ (0.98m x 0.065m x 0.24m)
            const Vector3 minP = { -0.49f, -0.032f, -0.12f };
            const Vector3 maxP = {  0.49f,  0.032f,  0.12f };
            const Vector3 corners[DuckPartRig::kBoxCornerCount] = {
                { minP.x, minP.y, minP.z }, { maxP.x, minP.y, minP.z }, { maxP.x, maxP.y, minP.z }, { minP.x, maxP.y, minP.z },
                { minP.x, minP.y, maxP.z }, { maxP.x, minP.y, maxP.z }, { maxP.x, maxP.y, maxP.z }, { minP.x, maxP.y, maxP.z },
            };
            const Vector2 plankUVs[DuckPartRig::kBoxFaceVertCount] = {
                { 0.12f, 0.28f }, { 0.88f, 0.28f }, { 0.88f, 0.52f }, { 0.12f, 0.52f }
            };
            for (int f = 0; f < DuckPartRig::kBoxFaceCount; ++f)
            {
                const uint32_t baseIdx = static_cast<uint32_t>(sPlankModel.vertices.size());
                for (int c = 0; c < DuckPartRig::kBoxFaceVertCount; ++c)
                {
                    const Vector3& cp = corners[DuckPartRig::kBoxFaces[f][c]];
                    Sprite::VertexData v{};
                    v.position = { cp.x, cp.y, cp.z, 1.0f };
                    v.texcoord = plankUVs[c];
                    v.normal   = DuckPartRig::kBoxNormals[f];
                    sPlankModel.vertices.push_back(v);
                }
                for (uint32_t triIdx : DuckPartRig::kQuadTriIndices)
                {
                    sPlankModel.indices.push_back(baseIdx + triIdx);
                }
            }
            sPlankModel.material.textureFilePath = kTexBreakableWoodCrate;
            sPlankModel.boundingRadius = 1.6f;
        }
        sPlankModel.material.textureIndex = woodTexIdx;
        return sPlankModel;
    }
}

void Obstacle::ConfigureAsBreakableSupplyCrate()
{
    isBreakableSupplyCrate_ = true;
    TextureManager* tm = TextureManager::GetInstance();
    woodCrateTexIndex_ = tm->Load(kTexBreakableWoodCrate);
    woodCrateDamagedTexIndex_ = tm->Load(kTexDamagedWoodCrate);
    if (woodCrateTexIndex_ != TextureManager::kInvalidTextureIndex)
    {
        customTextureIndex_ = woodCrateTexIndex_;
    }
    // 木橋と同じく directionalLight.intensity=0.04 で天面の白いスペキュラ膜を完全除去し、全方向から木目と弾薬ステンシルがくっきり見える設定
    baseColor_ = { 17.5f, 16.8f, 15.6f, 1.0f };
    if (object3d_)
    {
        auto& crateLight = object3d_->GetLightData();
        crateLight.directionalLight.direction = kSunLightDirection;
        crateLight.directionalLight.intensity = kLowSpecLightIntensity;
        crateLight.pointLight.intensity = 0.0f;
        object3d_->SetReflectionFactor(0.0f);
        object3d_->SetFresnelF0(0.0f);
        object3d_->SetColor(baseColor_);
        object3d_->Update();
    }
}

bool Obstacle::DamageBarrel(const Vector3& hitPos, const Vector3& leakNormal, float chainFuseOverride)
{
    if (isBroken_ || !isExplosiveBarrel_) return false;

    if (chainFuseOverride > 0.0f)
    {
        isBarrelLeaking_ = true;
        barrelLeakPos_ = position_ + Vector3{ 0.0f, 0.72f, 0.0f };
        barrelLeakDir_ = { 0.0f, 0.85f, 0.25f };
        barrelFuseTimer_ = (std::min)(barrelFuseTimer_, chainFuseOverride);
        barrelHp_ = (std::min)(barrelHp_, 1);
        TriggerHitFlinch(leakNormal);
        return false;
    }

    barrelHp_--;
    barrelLeakPos_ = hitPos;
    barrelLeakDir_ = { leakNormal.x, (std::max)(0.28f, leakNormal.y + 0.25f), leakNormal.z };
    isBarrelLeaking_ = true;
    TriggerHitFlinch({ -leakNormal.x, 0.0f, -leakNormal.z });
    return (barrelHp_ <= 0);
}

bool Obstacle::TickBarrelFuse(float dt)
{
    if (isBroken_ || !isExplosiveBarrel_ || !isBarrelLeaking_) return false;
    barrelFuseTimer_ -= dt;
    if (object3d_)
    {
        const float pulse = 0.5f + 0.5f * std::sin(barrelFuseTimer_ * 26.0f);
        object3d_->SetColor({
            baseColor_.x * (1.15f + 0.65f * pulse),
            baseColor_.y * (1.0f + 0.45f * pulse),
            baseColor_.z * (1.0f + 0.15f * pulse),
            1.0f
        });
    }
    return (barrelFuseTimer_ <= 0.0f);
}

void Obstacle::TriggerHitFlinch(const Vector3& hitDir)
{
    if (isBroken_) return;
    hitFlinchTimer_ = 0.22f;
    hitFlinchDir_ = hitDir;

    // 1発目被弾時にヒビ割れ・弾痕入りテクスチャへ切り替え、「あと少しで壊れる！」視覚手応えを付与
    if (isBreakableSupplyCrate_ && woodCrateDamagedTexIndex_ != UINT32_MAX &&
        woodCrateDamagedTexIndex_ != TextureManager::kInvalidTextureIndex)
    {
        customTextureIndex_ = woodCrateDamagedTexIndex_;
    }
}

void Obstacle::BreakObstacle(const Vector3& hitDir)
{
    if (isBroken_) return;
    isBroken_ = true;
    isBarrelLeaking_ = false;
    hitFlinchTimer_ = 0.0f;
    if (collider_) collider_->SetIsEnabled(false);
    for (auto& col : extraColliders_)
    {
        if (col) col->SetIsEnabled(false);
    }
    if (meshCollider_) meshCollider_->SetIsEnabled(false);

    // 3D木箱破砕演出：壁面・天板・斜め補強材を構成していた12枚の実体3D木板が物理演算で四方へ吹き飛び地面を跳ねる！
    if (object3dCom_ && camera_)
    {
        const uint32_t plankTex = (woodCrateTexIndex_ != UINT32_MAX && woodCrateTexIndex_ != TextureManager::kInvalidTextureIndex)
            ? woodCrateTexIndex_ : defaultTextureIndex_;
        const Object3d::ModelData& plankModel = GetBreakablePlankModel(plankTex);

        struct PlankSpawnSpec { Vector3 localOff; float yaw; Vector3 scale; bool isLid; };
        constexpr PlankSpawnSpec kPlankSpecs[12] = {
            { {  0.00f, 0.34f, -0.58f }, 0.0f,                { 1.05f, 1.0f, 1.00f }, false }, // 前面下段板
            { {  0.00f, 0.84f, -0.58f }, 0.0f,                { 1.05f, 1.0f, 1.00f }, false }, // 前面上段板
            { {  0.00f, 0.34f,  0.58f }, DuckPartRig::kPi,    { 1.05f, 1.0f, 1.00f }, false }, // 背面下段板
            { {  0.00f, 0.84f,  0.58f }, DuckPartRig::kPi,    { 1.05f, 1.0f, 1.00f }, false }, // 背面上段板
            { { -0.58f, 0.38f,  0.00f }, DuckPartRig::kPi*0.5f,{ 1.05f, 1.0f, 1.00f }, false }, // 左面下段板
            { { -0.58f, 0.86f,  0.00f }, DuckPartRig::kPi*0.5f,{ 1.05f, 1.0f, 1.00f }, false }, // 左面上段板
            { {  0.58f, 0.38f,  0.00f },-DuckPartRig::kPi*0.5f,{ 1.05f, 1.0f, 1.00f }, false }, // 右面下段板
            { {  0.58f, 0.86f,  0.00f },-DuckPartRig::kPi*0.5f,{ 1.05f, 1.0f, 1.00f }, false }, // 右面上段板
            { {  0.00f, 1.12f, -0.26f }, 0.15f,               { 1.12f, 1.0f, 1.15f }, true  }, // 天板前側
            { {  0.00f, 1.12f,  0.26f },-0.15f,               { 1.12f, 1.0f, 1.15f }, true  }, // 天板奥側
            { { -0.32f, 0.62f, -0.32f }, 0.78f,               { 0.92f, 1.1f, 0.58f }, false }, // 斜め角材A
            { {  0.32f, 0.62f,  0.32f },-0.78f,               { 0.92f, 1.1f, 0.58f }, false }, // 斜め角材B
        };

        debrisPieces_.clear();
        debrisPieces_.reserve(12);
        const float cosY = std::cos(rotation_.y);
        const float sinY = std::sin(rotation_.y);

        for (const auto& sp : kPlankSpecs)
        {
            CrateDebrisPiece p{};
            p.obj = std::make_unique<Object3d>();
            p.obj->Initialize(object3dCom_, plankModel);
            p.obj->SetCamera(camera_);
            p.obj->SetEnableLighting(true);
            p.obj->SetReflectionFactor(0.0f);
            auto& plankLight = p.obj->GetLightData();
            plankLight.directionalLight.direction = kSunLightDirection;
            plankLight.directionalLight.intensity = kLowSpecLightIntensity;
            plankLight.pointLight.intensity = 0.0f;

            const float wx = sp.localOff.x * cosY + sp.localOff.z * sinY;
            const float wz = -sp.localOff.x * sinY + sp.localOff.z * cosY;
            p.pos = { position_.x + wx * baseScale_.x, position_.y + sp.localOff.y * baseScale_.y, position_.z + wz * baseScale_.z };

            Vector3 outDir = { wx, 0.0f, wz };
            const float outLen = std::sqrt(outDir.x * outDir.x + outDir.z * outDir.z);
            if (outLen > 0.01f) { outDir.x /= outLen; outDir.z /= outLen; }
            else { const float a = CrateRandRange(0.0f, DuckPartRig::kTwoPi); outDir = { std::cos(a), 0.0f, std::sin(a) }; }

            const float radSpd = isExplosiveBarrel_
                ? CrateRandRange(4.8f, 9.6f)
                : (sp.isLid ? CrateRandRange(1.8f, 3.8f) : CrateRandRange(2.8f, 5.4f));
            p.vel = {
                outDir.x * radSpd + hitDir.x * CrateRandRange(1.2f, 2.5f) + CrateRandRange(-0.8f, 0.8f),
                isExplosiveBarrel_ ? CrateRandRange(4.5f, 9.2f) : (sp.isLid ? CrateRandRange(3.8f, 6.2f) : CrateRandRange(2.0f, 4.6f)),
                outDir.z * radSpd + hitDir.z * CrateRandRange(1.2f, 2.5f) + CrateRandRange(-0.8f, 0.8f)
            };
            p.rot = { sp.isLid ? 0.0f : CrateRandRange(-0.45f, 0.45f), rotation_.y + sp.yaw, CrateRandRange(-0.25f, 0.25f) };
            p.angVel = { CrateRandRange(-13.5f, 13.5f), CrateRandRange(-9.5f, 9.5f), CrateRandRange(-12.5f, 12.5f) };
            p.baseScale = { sp.scale.x * baseScale_.x, sp.scale.y * baseScale_.y, sp.scale.z * baseScale_.z };
            const float tone = CrateRandRange(0.90f, 1.08f);
            p.baseColor = isExplosiveBarrel_
                ? Vector4{ 8.5f * tone, 2.2f * tone, 1.6f * tone, 1.0f }
                : Vector4{ 17.5f * tone, 16.5f * tone, 15.2f * tone, 1.0f };
            p.age = 0.0f;
            p.lifeTime = CrateRandRange(2.3f, 2.9f);

            p.obj->SetTranslate(p.pos);
            p.obj->SetRotate(p.rot);
            p.obj->SetScale(p.baseScale);
            p.obj->SetColor(p.baseColor);
            p.obj->Update();
            debrisPieces_.push_back(std::move(p));
        }
    }
}

void Obstacle::Update()
{
    if (!object3d_) return;

    // 破壊後の3D木板破片の重力落下・回転・地面バウンド・消滅フェード更新
    if (isBroken_)
    {
        if (debrisPieces_.empty()) return;
        constexpr float dt = 1.0f / 60.0f;
        for (auto it = debrisPieces_.begin(); it != debrisPieces_.end(); )
        {
            it->age += dt;
            if (it->age >= it->lifeTime || !it->obj)
            {
                it = debrisPieces_.erase(it);
                continue;
            }

            if (!it->settled)
            {
                it->vel.y -= 17.5f * dt;
                it->vel.x *= (1.0f - 0.85f * dt);
                it->vel.z *= (1.0f - 0.85f * dt);
                it->pos = it->pos + it->vel * dt;
                it->rot.x += it->angVel.x * dt;
                it->rot.y += it->angVel.y * dt;
                it->rot.z += it->angVel.z * dt;

                if (it->pos.y <= 0.042f && it->vel.y < 0.0f)
                {
                    it->pos.y = 0.042f;
                    it->bounceCount++;
                    if (it->bounceCount == 1)
                    {
                        TacticalAudio::Play2D("Resources/Audio/SE_Impact_Wood.wav", 0.36f, 75, "crate_plank_bounce");
                    }

                    if (it->bounceCount >= 3 || std::abs(it->vel.y) < 0.95f)
                    {
                        it->settled = true;
                        it->vel = { 0.0f, 0.0f, 0.0f };
                        it->angVel = { 0.0f, 0.0f, 0.0f };
                        it->rot.x = 0.0f;
                        it->rot.z = 0.0f;
                    }
                    else
                    {
                        it->vel.y = -it->vel.y * 0.36f;
                        it->vel.x *= 0.60f;
                        it->vel.z *= 0.60f;
                        it->angVel.x *= 0.45f;
                        it->angVel.y *= 0.65f;
                        it->angVel.z *= 0.45f;
                        it->rot.x *= 0.45f;
                        it->rot.z *= 0.45f;
                    }
                }
            }

            const float remain = it->lifeTime - it->age;
            if (remain < 0.60f)
            {
                const float fade = (std::clamp)(remain / 0.60f, 0.0f, 1.0f);
                it->obj->SetScale({ it->baseScale.x * fade, it->baseScale.y * fade, it->baseScale.z * fade });
                it->obj->SetColor({ it->baseColor.x, it->baseColor.y, it->baseColor.z, fade });
            }
            it->obj->SetTranslate(it->pos);
            it->obj->SetRotate(it->rot);
            it->obj->Update();
            ++it;
        }
        return;
    }

    float flinchRotX = 0.0f;
    float flinchRotZ = 0.0f;
    if (hitFlinchTimer_ > 0.0f)
    {
        hitFlinchTimer_ = (std::max)(0.0f, hitFlinchTimer_ - (1.0f / 60.0f));
        const float ratio = hitFlinchTimer_ / 0.22f;
        if (ratio > 0.0f)
        {
            const float osc = std::sin((1.0f - ratio) * DuckPartRig::kPi * (isTree_ ? 4.5f : 3.0f)) * ratio;
            if (isTree_)
            {
                // 樹木被弾時は幹スケールを潰さず、樹冠がザワッと揺れる自然な角度振動のみを与える
                flinchRotX =  hitFlinchDir_.z * 0.048f * osc;
                flinchRotZ = -hitFlinchDir_.x * 0.048f * osc;
            }
            else
            {
                const float scaleMul = isBreakableSupplyCrate_ ? 1.0f : 0.35f;
                const float tiltMul  = isBreakableSupplyCrate_ ? 0.14f : 0.045f;
                object3d_->SetScale({
                    baseScale_.x * (1.0f + 0.11f * scaleMul * ratio),
                    baseScale_.y * (1.0f - 0.08f * scaleMul * ratio),
                    baseScale_.z * (1.0f + 0.11f * scaleMul * ratio)
                });
                object3d_->SetRotate({
                    rotation_.x + hitFlinchDir_.z * tiltMul * osc,
                    rotation_.y,
                    rotation_.z - hitFlinchDir_.x * tiltMul * osc
                });
                if (isBreakableSupplyCrate_)
                {
                    object3d_->SetColor({
                        baseColor_.x * (1.0f + 0.32f * ratio),
                        baseColor_.y * (1.0f + 0.24f * ratio),
                        baseColor_.z * (1.0f + 0.12f * ratio),
                        baseColor_.w
                    });
                }
            }
        }
        else
        {
            object3d_->SetScale(baseScale_);
            object3d_->SetRotate(rotation_);
            object3d_->SetColor(baseColor_);
        }
    }

    if (isTree_)
    {
        static const auto sStart = std::chrono::steady_clock::now();
        const float t = std::chrono::duration<float>(std::chrono::steady_clock::now() - sStart).count();
        const float swayX = kSwayPrimaryAmpX * std::sin(t * kSwayPrimaryFreqX + treeSwayPhase_)
                          + kSwaySecondaryAmpX * std::sin(t * kSwaySecondaryFreqX + treeSwayPhase_ * kSwaySecondaryPhaseMul);
        const float swayZ = kSwayAmpZ * std::cos(t * kSwayFreqZ + treeSwayPhase_ * kSwayZPhaseMul);
        object3d_->SetRotate({ rotation_.x + swayX + flinchRotX, rotation_.y, rotation_.z + swayZ + flinchRotZ });
    }
    object3d_->Update();
}

void Obstacle::Draw(const RenderContext& ctx)
{
    if (!object3d_) return;

    if (isBroken_)
    {
        if (debrisPieces_.empty()) return;
        RenderContext debCtx = ctx;
        const uint32_t plankTex = (woodCrateTexIndex_ != UINT32_MAX && woodCrateTexIndex_ != TextureManager::kInvalidTextureIndex)
            ? woodCrateTexIndex_ : defaultTextureIndex_;
        if (plankTex != TextureManager::kInvalidTextureIndex)
        {
            debCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(plankTex);
        }
        for (auto& p : debrisPieces_)
        {
            if (!p.obj) continue;
            if (ctx.camera)
            {
                p.obj->SetCamera(ctx.camera);
                p.obj->Update();
            }
            object3dCom_->Draw(p.obj.get(), debCtx, p.obj->GetModelData(), true);
        }
        return;
    }

    RenderContext obsCtx = ctx;
    const Object3d::ModelData& modelData = object3d_->GetModelData();
    uint32_t texIdx = (modelData.material.textureIndex != TextureManager::kInvalidTextureIndex) ? modelData.material.textureIndex : defaultTextureIndex_;
    if (customTextureIndex_ != UINT32_MAX && customTextureIndex_ != TextureManager::kInvalidTextureIndex)
    {
        texIdx = customTextureIndex_;
    }
    if (texIdx != TextureManager::kInvalidTextureIndex)
    {
        obsCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(texIdx);
    }

    object3dCom_->Draw(object3d_.get(), obsCtx, modelData, true);
}

void Obstacle::Finalize()
{
    debrisPieces_.clear();
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
