#include "Obstacle.h"
#include "TextureManager.h"
#include <unordered_map>

#include "Baziru3_Engine/Framework/Collision/CollisionManager.h"

// 重複ロードを防ぎシーン遷移を爆速化するための静的モデルキャッシュ
static std::unordered_map<std::string, Object3d::ModelData> sModelCache;

void Obstacle::Initialize(Object3dCom* object3dCom, Camera* camera, const Vector3& position, float radius, const std::string& modelFilename, const Vector3& scale, const Vector3& rotation)
{
    object3dCom_ = object3dCom;
    position_ = position;
    radius_ = radius;
    rotation_ = rotation;

    // 指定されたモデルファイルをロード (キャッシュがあれば即座に再利用)
    std::string filename = modelFilename.empty() ? "fence.obj" : modelFilename;
    
    auto it = sModelCache.find(filename);
    if (it == sModelCache.end())
    {
        Object3d::ModelData loaded = Object3d::LoadObjFile("Resources", filename);
        if (loaded.material.textureFilePath.empty())
        {
            if (filename.find("crate") != std::string::npos) loaded.material.textureFilePath = "Resources/duckov_crate.png";
            else if (filename.find("sandbag") != std::string::npos) loaded.material.textureFilePath = "Resources/duckov_sandbag.png";
            else if (filename.find("barrel") != std::string::npos) loaded.material.textureFilePath = "Resources/duckov_barrel.png";
            else if (filename.find("bridge") != std::string::npos) loaded.material.textureFilePath = "Resources/duckov_bridge.png";
            else if (filename.find("ground") != std::string::npos || filename.find("plane") != std::string::npos) loaded.material.textureFilePath = "Resources/grass.png";
            else if (filename.find("river") != std::string::npos || filename.find("water") != std::string::npos) loaded.material.textureFilePath = "Resources/water.png";
            else if (filename.find("container") != std::string::npos) loaded.material.textureFilePath = "Resources/container_military.png";
            else loaded.material.textureFilePath = "Resources/CG4/human/white.png";
        }
        uint32_t texIdx = TextureManager::GetInstance()->Load(loaded.material.textureFilePath);
        loaded.material.textureIndex = texIdx;

        float maxDistSq = 0.0f;
        for (const auto& v : loaded.vertices)
        {
            float distSq = v.position.x * v.position.x + v.position.y * v.position.y + v.position.z * v.position.z;
            if (distSq > maxDistSq) maxDistSq = distSq;
        }
        loaded.boundingRadius = std::sqrt(maxDistSq);
        if (filename.find("ground") != std::string::npos || filename.find("plane") != std::string::npos || filename.find("river") != std::string::npos)
        {
            loaded.boundingRadius = (std::max)(loaded.boundingRadius, 60.0f);
        }
        else if (loaded.boundingRadius < 1.0f)
        {
            loaded.boundingRadius = 3.0f;
        }

        sModelCache[filename] = loaded;
        it = sModelCache.find(filename);
    }

    const Object3d::ModelData& model = it->second;

    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(object3dCom, model);
    object3d_->SetCamera(camera);

    object3d_->SetTranslate(position_);
    object3d_->SetRotate(rotation_);
    object3d_->SetScale({ scale.x * radius_, scale.y * radius_, scale.z * radius_ });

    // 頂点配列からモデル本来の正確な最小・最大座標 (AABB) を自動計算
    Vector3 minPos = {  1e9f,  1e9f,  1e9f };
    Vector3 maxPos = { -1e9f, -1e9f, -1e9f };

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
        minPos = { -1.0f, 0.0f, -1.0f };
        maxPos = {  1.0f, 2.0f,  1.0f };
    }

    Vector3 localSize = { maxPos.x - minPos.x, maxPos.y - minPos.y, maxPos.z - minPos.z };
    localSize.x = (std::max)(localSize.x, 0.2f);
    localSize.y = (std::max)(localSize.y, 0.2f);
    localSize.z = (std::max)(localSize.z, 0.2f);

    Vector3 boxSize = {
        localSize.x * scale.x * radius_,
        localSize.y * scale.y * radius_,
        localSize.z * scale.z * radius_
    };

    Vector3 centerOffset = {
        ((minPos.x + maxPos.x) * 0.5f) * scale.x * radius_,
        ((minPos.y + maxPos.y) * 0.5f) * scale.y * radius_,
        ((minPos.z + maxPos.z) * 0.5f) * scale.z * radius_
    };

    if (filename == "fence.obj" || filename.find("fence") != std::string::npos)
    {
        defaultTextureIndex_ = TextureManager::GetInstance()->Load("Resources/fence.png");

        // X字のフェンスを構成する2枚の板の精密な角度 (±30.0度 = ±0.5236rad) と長さ (4.90m)、薄さ (0.15m)
        rot1_ = { rotation_.x, rotation_.y - 0.5235987f, rotation_.z };
        rot2_ = { rotation_.x, rotation_.y + 0.5235987f, rotation_.z };

        Vector3 fenceSize = { 4.90f * radius_, 2.0f * radius_, 0.15f * radius_ };
        
        collider_ = std::make_unique<BoxCollider>(fenceSize, &position_, &rot1_, CollisionAttribute::Obstacle);
        collider_->SetPositionOffset({ 0.0f, fenceSize.y * 0.5f, 0.0f });
        CollisionManager::GetInstance()->RegisterCollider(collider_.get());

        auto col2 = std::make_unique<BoxCollider>(fenceSize, &position_, &rot2_, CollisionAttribute::Obstacle);
        col2->SetPositionOffset({ 0.0f, fenceSize.y * 0.5f, 0.0f });
        CollisionManager::GetInstance()->RegisterCollider(col2.get());
        extraColliders_.push_back(std::move(col2));
    }
    else if (filename.find("watchtower") != std::string::npos || filename.find("WatchTower") != std::string::npos)
    {
        // 監視塔：4本の柱脚（±1.15m, ±1.15m）と最上部フロア（Y=5.0m）に精密分割コライダーを配置
        // これにより、柱の間をプレイヤーが通り抜けたり、弾丸が足元を突き抜ける精密な判定が可能になります
        rot1_ = rotation_;

        // 4本の柱脚コライダー
        Vector3 legSize = { 0.35f * scale.x * radius_, 4.5f * scale.y * radius_, 0.35f * scale.z * radius_ };
        float lx = 1.15f * scale.x * radius_;
        float lz = 1.15f * scale.z * radius_;
        float ly = 2.25f * scale.y * radius_;

        // 柱脚 1 (Front-Left)
        collider_ = std::make_unique<BoxCollider>(legSize, &position_, &rot1_, CollisionAttribute::Obstacle);
        collider_->SetPositionOffset({ -lx, ly, -lz });
        CollisionManager::GetInstance()->RegisterCollider(collider_.get());

        // 柱脚 2 (Front-Right)
        auto leg2 = std::make_unique<BoxCollider>(legSize, &position_, &rot1_, CollisionAttribute::Obstacle);
        leg2->SetPositionOffset({ lx, ly, -lz });
        CollisionManager::GetInstance()->RegisterCollider(leg2.get());
        extraColliders_.push_back(std::move(leg2));

        // 柱脚 3 (Back-Left)
        auto leg3 = std::make_unique<BoxCollider>(legSize, &position_, &rot1_, CollisionAttribute::Obstacle);
        leg3->SetPositionOffset({ -lx, ly, lz });
        CollisionManager::GetInstance()->RegisterCollider(leg3.get());
        extraColliders_.push_back(std::move(leg3));

        // 柱脚 4 (Back-Right)
        auto leg4 = std::make_unique<BoxCollider>(legSize, &position_, &rot1_, CollisionAttribute::Obstacle);
        leg4->SetPositionOffset({ lx, ly, lz });
        CollisionManager::GetInstance()->RegisterCollider(leg4.get());
        extraColliders_.push_back(std::move(leg4));

        // 最上階フロア・屋根コライダー
        Vector3 platformSize = { 3.2f * scale.x * radius_, 1.8f * scale.y * radius_, 3.2f * scale.z * radius_ };
        auto roof = std::make_unique<BoxCollider>(platformSize, &position_, &rot1_, CollisionAttribute::Obstacle);
        roof->SetPositionOffset({ 0.0f, 5.1f * scale.y * radius_, 0.0f });
        CollisionManager::GetInstance()->RegisterCollider(roof.get());
        extraColliders_.push_back(std::move(roof));
    }
    else if (filename.find("bridge") != std::string::npos || filename.find("Bridge") != std::string::npos)
    {
        // 橋：中央の通路（幅2.4m）はプレイヤーが自由に渡れるよう開放し、左右の欄干（X = -1.35 と +1.35）のみに薄いコライダーを配置
        rot1_ = rotation_;

        Vector3 railingSize = { 0.35f * scale.x * radius_, 1.0f * scale.y * radius_, 8.0f * scale.z * radius_ };
        Vector3 leftRailingOffset = { -1.35f * scale.x * radius_, 0.5f * scale.y * radius_, 0.0f };
        Vector3 rightRailingOffset = { 1.35f * scale.x * radius_, 0.5f * scale.y * radius_, 0.0f };

        collider_ = std::make_unique<BoxCollider>(railingSize, &position_, &rot1_, CollisionAttribute::Obstacle);
        collider_->SetPositionOffset(leftRailingOffset);
        CollisionManager::GetInstance()->RegisterCollider(collider_.get());

        auto col2 = std::make_unique<BoxCollider>(railingSize, &position_, &rot1_, CollisionAttribute::Obstacle);
        col2->SetPositionOffset(rightRailingOffset);
        CollisionManager::GetInstance()->RegisterCollider(col2.get());
        extraColliders_.push_back(std::move(col2));
    }
    else if (filename.find("river") != std::string::npos || filename.find("River") != std::string::npos ||
             filename.find("water") != std::string::npos)
    {
        // 川：プレイヤーおよび敵が川へ直接侵入できないよう、橋の通路部分（X: -1.2m 〜 +1.2m）を除いた東西の水面全域に侵入不可コライダーを配置
        rot1_ = rotation_;

        float riverHalfWidth = scale.x * 0.5f; // 例: 30.0m
        float bridgeWalkwayHalfWidth = 1.2f;    // 橋の開放通路幅
        float barrierWidth = (std::max)(1.0f, riverHalfWidth - bridgeWalkwayHalfWidth); // 28.8m
        float barrierOffset = bridgeWalkwayHalfWidth + barrierWidth * 0.5f;             // 15.6m

        Vector3 barrierSize = { barrierWidth, 2.0f, scale.z };
        Vector3 leftBarrierOffset = { -barrierOffset, 1.0f, 0.0f };
        Vector3 rightBarrierOffset = { barrierOffset, 1.0f, 0.0f };

        collider_ = std::make_unique<BoxCollider>(barrierSize, &position_, &rot1_, CollisionAttribute::Obstacle);
        collider_->SetPositionOffset(leftBarrierOffset);
        CollisionManager::GetInstance()->RegisterCollider(collider_.get());

        auto col2 = std::make_unique<BoxCollider>(barrierSize, &position_, &rot1_, CollisionAttribute::Obstacle);
        col2->SetPositionOffset(rightBarrierOffset);
        CollisionManager::GetInstance()->RegisterCollider(col2.get());
        extraColliders_.push_back(std::move(col2));
    }
    else if (filename.find("ground") != std::string::npos || filename.find("plane") != std::string::npos ||
             filename.find("Shore") != std::string::npos)
    {
        // 地面・床面：通行可能エリアのため、障害物コライダーは登録しない
        rot1_ = rotation_;
    }
    else
    {
        // 全モデル共通: 自動算出された精密なAABBサイズとオフセットでBoxColliderを生成
        rot1_ = rotation_;
        collider_ = std::make_unique<BoxCollider>(boxSize, &position_, &rotation_, CollisionAttribute::Obstacle);
        collider_->SetPositionOffset(centerOffset);
        CollisionManager::GetInstance()->RegisterCollider(collider_.get());
    }

    // --- オブジェクトタイプ別の識別色（ぱっと見で区別できるよう色分け）---
    if (filename.find("crate") != std::string::npos)
    {
        // 物資箱：軍用ブラウン
        object3d_->SetColor({ 0.55f, 0.38f, 0.18f, 1.0f });
    }
    else if (filename.find("barrel") != std::string::npos)
    {
        // ドラム缶：錆び赤
        object3d_->SetColor({ 0.75f, 0.20f, 0.12f, 1.0f });
    }
    else if (filename.find("sandbag") != std::string::npos)
        {
            // 土嚢：砂漠カーキ
            object3d_->SetColor({ 0.72f, 0.65f, 0.35f, 1.0f });
        }
        else if (filename.find("watchtower") != std::string::npos)
        {
            // 監視塔：ダークウッド
            object3d_->SetColor({ 0.35f, 0.22f, 0.10f, 1.0f });
        }
        else if (filename.find("container") != std::string::npos)
        {
            // コンテナ：海運グリーン
            object3d_->SetColor({ 0.12f, 0.45f, 0.25f, 1.0f });
        }
        else if (filename.find("river") != std::string::npos || filename.find("River") != std::string::npos || filename.find("water") != std::string::npos || filename.find("Shore") != std::string::npos)
        {
            // 川・水面パーツ：水テクスチャ
            object3d_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        }
        else if (filename.find("bridge") != std::string::npos)
        {
            // 橋：コンクリートグレー
            object3d_->SetColor({ 0.70f, 0.70f, 0.70f, 1.0f });
        }
        else if (filename.find("plane") != std::string::npos || filename.find("ground") != std::string::npos)
        {
            // 地面：青々とした鮮やかな芝生テクスチャ
            object3d_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        }
        else
        {
            // その他：ニュートラルグレー
            object3d_->SetColor({ 0.60f, 0.60f, 0.60f, 1.0f });
        }

    object3d_->Update();
}

void Obstacle::Update()
{
    // カメラが動いても障害物が追従しないよう、毎フレームWVP行列を更新する
    // ワールド行列は変化しないため、計算コストは軽微
    if (object3d_)
    {
        object3d_->Update();
    }
}


void Obstacle::Draw(const RenderContext& ctx)
{
    if (!object3d_) return;

    RenderContext obsCtx = ctx;
    const Object3d::ModelData& modelData = object3d_->GetModelData();
    uint32_t texIdx = (modelData.material.textureIndex != TextureManager::kInvalidTextureIndex) ? modelData.material.textureIndex : defaultTextureIndex_;
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
    if (object3d_)
    {
        object3d_.reset();
    }
}

