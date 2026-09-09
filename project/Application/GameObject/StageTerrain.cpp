#define NOMINMAX
#include "Application/GameObject/StageTerrain.h"

#include "Baziru3_Engine/Core/Base/Matrix4x4.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3dCom.h"
#include "Baziru3_Engine/Framework/Collision/CollisionManager.h"
#include "Baziru3_Engine/Graphics/2D/Texture/TextureManager.h"
#include "Application/GameObject/SlimePhysics.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>

namespace
{
    /// @brief 視錐台の誤カリングを完全に防ぐための巨大なバウンディング半径
    constexpr float kHugeBoundingRadius = 10000.0f;

    /// @brief mtl にもエントリの texture 指定にも何も無かったときの既定テクスチャ
    constexpr const char* kDefaultTexture = "Resources/10days/land.png";

    /// @brief カタログから外すファイル（地形ではないもの）
    bool IsExcludedMesh(const std::string& fileName)
    {
        // propeller は回転障害物。地形として置かれると当たり判定がおかしくなる
        return fileName == "propeller.obj";
    }

    /// @brief 行ベクトル規約（v * M）での回転適用
    inline Vector3 ApplyRotation(const Vector3& v, const Matrix4x4& m)
    {
        return {
            v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0],
            v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1],
            v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2]
        };
    }

    /// @brief GamePlayScene が地面に掛けていたのと同じ回転行列 R = Rx(pitch) * Rz(-roll)
    inline Matrix4x4 BuildStageRotation(const Vector2& stageTilt)
    {
        return Multiply(MakeRotateXMatrix(stageTilt.x), MakeRotateZMatrix(-stageTilt.y));
    }

    /**
     * @brief 3x3 回転行列からエンジンのオイラー角 (rx, ry, rz) を逆算する
     * @note engine の MakeAffineMatrix は R = Rx(x) * Ry(y) * Rz(z)（行ベクトル規約）。
     *       Object3d はオイラー角しか受け取れないので、合成した回転行列を
     *       ここで角度に戻してから SetRotate() する。
     *       GrowthCube.cpp が同じことをしている（あちらの実装を踏襲）
     */
    Vector3 MatrixToEulerXYZ(const Matrix4x4& R)
    {
        Vector3 euler;
        float sy = -R.m[0][2];
        sy = std::clamp(sy, -1.0f, 1.0f);
        euler.y = std::asin(sy);

        float cy = std::cos(euler.y);
        if (std::abs(cy) > 1e-4f)
        {
            euler.x = std::atan2(R.m[1][2], R.m[2][2]);
            euler.z = std::atan2(R.m[0][1], R.m[0][0]);
        }
        else
        {
            // ジンバルロック。x に寄せて z は 0 にする
            euler.x = std::atan2(-R.m[2][1], R.m[1][1]);
            euler.z = 0.0f;
        }
        return euler;
    }
}

float StageTerrain::Part::HandleRadius() const
{
    const float sx = (worldMax.x - worldMin.x) * 0.5f;
    const float sz = (worldMax.z - worldMin.z) * 0.5f;
    // 小さいパーツでも掴めるように下限を、大きい島でも画面を埋めないように上限を切る
    return std::clamp((std::min)(sx, sz) * 0.35f, 1.5f, 8.0f);
}

StageTerrain::~StageTerrain()
{
    Finalize();
}

void StageTerrain::Initialize(Object3dCom* object3dCom, Camera* camera)
{
    object3dCom_ = object3dCom;
    camera_ = camera;
}

void StageTerrain::Finalize()
{
    UnregisterAll();
    parts_.clear();
    object3dCom_ = nullptr;
    camera_ = nullptr;
}

// ===================================================================
// メッシュカタログ
// ===================================================================

namespace
{
    std::vector<std::string> g_meshCatalog;
    bool g_meshCatalogReady = false;
}

const std::vector<std::string>& StageTerrain::GetMeshCatalog()
{
    if (!g_meshCatalogReady)
    {
        RefreshMeshCatalog();
    }
    return g_meshCatalog;
}

void StageTerrain::RefreshMeshCatalog()
{
    g_meshCatalog.clear();
    g_meshCatalogReady = true;

    try
    {
        std::filesystem::path dir(StageLayout::kTerrainDirectory);
        if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir))
        {
            return;
        }

        for (const auto& entry : std::filesystem::directory_iterator(dir))
        {
            if (!entry.is_regular_file()) continue;

            std::string name = entry.path().filename().string();
            if (name.size() < 5) continue;

            // 拡張子 .obj だけを拾う（大文字小文字は無視）
            std::string ext = name.substr(name.size() - 4);
            std::transform(ext.begin(), ext.end(), ext.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (ext != ".obj") continue;

            if (IsExcludedMesh(name)) continue;

            g_meshCatalog.push_back(name);
        }
    }
    catch (...)
    {
        // ディレクトリが読めなくても落とさない。カタログが空になるだけ
    }

    std::sort(g_meshCatalog.begin(), g_meshCatalog.end());
}

// ===================================================================
// パーツの生成
// ===================================================================

std::unique_ptr<StageTerrain::Part> StageTerrain::CreatePart(const std::string& mesh,
                                                             const Vector3& position,
                                                             float rotationY, float scale,
                                                             bool bossTrigger,
                                                             const std::string& texture) const
{
    if (!object3dCom_ || mesh.empty()) return nullptr;

    auto part = std::make_unique<Part>();
    part->mesh = mesh;
    part->texture = texture;
    part->position = position;
    part->rotationY = rotationY;
    part->scale = (scale > 0.0001f) ? scale : 0.25f;
    part->bossTrigger = bossTrigger;

    part->modelData = Object3d::LoadObjFile(StageLayout::kTerrainDirectory, mesh);
    if (part->modelData.vertices.empty())
    {
        return nullptr; // 読めなかった
    }
    part->modelData.boundingRadius = kHugeBoundingRadius;

    // テクスチャの決め方: エントリ指定 -> mtl 指定 -> 既定
    std::string texPath = part->texture;
    if (texPath.empty()) texPath = part->modelData.material.textureFilePath;
    if (texPath.empty()) texPath = kDefaultTexture;

    part->textureIndex = TextureManager::GetInstance()->Load(texPath);
    part->modelData.material.textureIndex = part->textureIndex;

    // モデルローカルの AABB。ハンドル位置と選択枠に使う
    part->localMin = { 1e18f, 1e18f, 1e18f };
    part->localMax = { -1e18f, -1e18f, -1e18f };
    for (const auto& v : part->modelData.vertices)
    {
        part->localMin.x = (std::min)(part->localMin.x, v.position.x);
        part->localMin.y = (std::min)(part->localMin.y, v.position.y);
        part->localMin.z = (std::min)(part->localMin.z, v.position.z);
        part->localMax.x = (std::max)(part->localMax.x, v.position.x);
        part->localMax.y = (std::max)(part->localMax.y, v.position.y);
        part->localMax.z = (std::max)(part->localMax.z, v.position.z);
    }

    part->object = std::make_unique<Object3d>();
    part->object->Initialize(object3dCom_, part->modelData);
    part->object->SetCamera(camera_);
    part->object->SetTranslate(part->position);
    part->object->SetScale({ part->scale, part->scale, part->scale });
    part->object->SetRotate({ 0.0f, part->rotationY, 0.0f });
    part->object->SetColor(baseColor_);
    part->object->SetEnableLighting(true);
    part->object->Update();

    part->collider = std::make_unique<MeshCollider>(part->object.get(), CollisionAttribute::Obstacle);

    RecalculateBounds(*part);
    return part;
}

void StageTerrain::RecalculateBounds(Part& part)
{
    // 傾き0のときのワールド AABB。scale -> Ry(rotationY) -> translate の順に効かせる
    const float cs = std::cos(part.rotationY);
    const float sn = std::sin(part.rotationY);

    Vector3 outMin{ 1e18f, 1e18f, 1e18f };
    Vector3 outMax{ -1e18f, -1e18f, -1e18f };

    for (int i = 0; i < 8; ++i)
    {
        const Vector3 corner = {
            ((i & 1) ? part.localMax.x : part.localMin.x) * part.scale,
            ((i & 2) ? part.localMax.y : part.localMin.y) * part.scale,
            ((i & 4) ? part.localMax.z : part.localMin.z) * part.scale,
        };

        // engine の MakeRotateYMatrix（行ベクトル規約 v * M）に合わせる:
        //   m[0][0]= cos, m[0][2]= -sin
        //   m[2][0]= sin, m[2][2]=  cos
        //   -> x' = x*cos + z*sin / z' = -x*sin + z*cos
        const Vector3 rotated = {
            corner.x * cs + corner.z * sn,
            corner.y,
            -corner.x * sn + corner.z * cs,
        };

        const Vector3 world = { rotated.x + part.position.x,
                                rotated.y + part.position.y,
                                rotated.z + part.position.z };

        outMin.x = (std::min)(outMin.x, world.x);
        outMin.y = (std::min)(outMin.y, world.y);
        outMin.z = (std::min)(outMin.z, world.z);
        outMax.x = (std::max)(outMax.x, world.x);
        outMax.y = (std::max)(outMax.y, world.y);
        outMax.z = (std::max)(outMax.z, world.z);
    }

    part.worldMin = outMin;
    part.worldMax = outMax;
}

// ===================================================================
// 登録
// ===================================================================

void StageTerrain::UnregisterAll()
{
    if (!registered_) return;

    SlimePhysics::ClearGroundMeshes();
    for (auto& part : parts_)
    {
        if (part && part->collider)
        {
            CollisionManager::GetInstance()->UnregisterCollider(part->collider.get());
        }
        if (part) part->groundMeshIndex = -1;
    }
    registered_ = false;
}

void StageTerrain::RefreshRegistration()
{
    // SlimePhysics のメッシュ番号は「AddGroundMesh を呼んだ順」なので、
    // パーツを増減したら必ず全部を登録し直す。
    // GroundLayer::meshIndex と groundMeshIndex の対応がここで決まる
    UnregisterAll();

    int index = 0;
    for (auto& part : parts_)
    {
        if (!part || !part->object || !part->collider) continue;

        CollisionManager::GetInstance()->RegisterCollider(part->collider.get());
        SlimePhysics::AddGroundMesh(part->object.get(), part->collider.get());
        part->groundMeshIndex = index++;
    }
    registered_ = true;
}

// ===================================================================
// 配置データの適用・吸い出し
// ===================================================================

void StageTerrain::ApplyLayout(const std::vector<StageTerrainEntry>& entries)
{
    // すでに同じ構成なら何もしない。
    // シーン初期化では「フォールバック判定のために先に地形を適用」してから
    // PlacementEditor::SetLayout() でもう一度同じものを流し込むので、
    // ここで弾かないと obj を2回読むことになる
    if (parts_.size() == entries.size())
    {
        bool same = true;
        for (size_t i = 0; i < entries.size() && same; ++i)
        {
            const Part* p = parts_[i].get();
            const StageTerrainEntry& e = entries[i];
            if (!p) { same = false; break; }
            same = (p->mesh == e.mesh)
                && (p->texture == e.texture)
                && (std::abs(p->position.x - e.position.x) < 1e-4f)
                && (std::abs(p->position.y - e.position.y) < 1e-4f)
                && (std::abs(p->position.z - e.position.z) < 1e-4f)
                && (std::abs(p->rotationY - e.rotationY) < 1e-5f)
                && (std::abs(p->scale - e.scale) < 1e-5f)
                && (p->bossTrigger == e.bossTrigger);
        }
        if (same) return;
    }

    UnregisterAll();
    parts_.clear();

    for (const auto& e : entries)
    {
        auto part = CreatePart(e.mesh, e.position, e.rotationY, e.scale, e.bossTrigger, e.texture);
        if (part)
        {
            parts_.push_back(std::move(part));
        }
    }

    RefreshRegistration();
}

void StageTerrain::WriteLayout(std::vector<StageTerrainEntry>& out) const
{
    out.clear();
    out.reserve(parts_.size());

    for (const auto& part : parts_)
    {
        if (!part) continue;

        StageTerrainEntry e;
        e.mesh = part->mesh;
        e.texture = part->texture;
        e.position = part->position;
        e.rotationY = part->rotationY;
        e.scale = part->scale;
        e.bossTrigger = part->bossTrigger;
        out.push_back(e);
    }
}

// ===================================================================
// 編集
// ===================================================================

StageTerrain::Part* StageTerrain::AddPart(const std::string& mesh, const Vector3& position,
                                          float rotationY, float scale, bool bossTrigger,
                                          const std::string& texture)
{
    auto part = CreatePart(mesh, position, rotationY, scale, bossTrigger, texture);
    if (!part) return nullptr;

    Part* raw = part.get();
    parts_.push_back(std::move(part));
    RefreshRegistration();
    return raw;
}

StageTerrain::Part* StageTerrain::AddPartCenteredAt(const std::string& mesh, const Vector3& centerXZ,
                                                    float rotationY, float scale, bool bossTrigger,
                                                    const std::string& texture)
{
    // いったん原点を centerXZ に置いて作る。
    // CreatePart() の中で AABB が計算されるので、そこから
    // 「原点と見た目の中心のずれ」が分かる
    Part* part = AddPart(mesh, centerXZ, rotationY, scale, bossTrigger, texture);
    if (!part) return nullptr;

    SetPartCenterXZ(part, centerXZ.x, centerXZ.z);
    return part;
}

void StageTerrain::SetPartCenterXZ(Part* part, float centerX, float centerZ)
{
    if (!part) return;

    // ローカル原点はメッシュの中心とは限らない（この obj 群は数百単位でずれている）。
    // いまの中心とのずれぶんだけ原点を動かせば、見た目の中心が狙った場所に来る
    const Vector3 handle = part->HandlePosition();
    SetPartPositionXZ(part,
                      part->position.x + (centerX - handle.x),
                      part->position.z + (centerZ - handle.z));
}

void StageTerrain::RemovePart(Part* part)
{
    if (!part) return;

    auto it = std::find_if(parts_.begin(), parts_.end(),
                           [part](const std::unique_ptr<Part>& p) { return p.get() == part; });
    if (it == parts_.end()) return;

    // コライダーは先に外す。unique_ptr が消えたあとで参照されると落ちる
    if ((*it)->collider)
    {
        CollisionManager::GetInstance()->UnregisterCollider((*it)->collider.get());
    }
    parts_.erase(it);

    RefreshRegistration();
}

void StageTerrain::ClearParts()
{
    UnregisterAll();
    parts_.clear();
    registered_ = true; // 空の状態で登録済み扱い
}

void StageTerrain::SetPartPositionXZ(Part* part, float x, float z)
{
    if (!part) return;
    part->position.x = x;
    part->position.z = z;
    RecalculateBounds(*part);
}

void StageTerrain::SetPartRotationY(Part* part, float radian)
{
    if (!part) return;
    part->rotationY = radian;
    RecalculateBounds(*part);
}

void StageTerrain::SetPartScale(Part* part, float scale)
{
    if (!part) return;
    part->scale = (std::max)(0.01f, scale);
    RecalculateBounds(*part);
}

void StageTerrain::SetPartBossTrigger(Part* part, bool on)
{
    if (!part) return;
    part->bossTrigger = on;
}

// ===================================================================
// 毎フレーム
// ===================================================================

void StageTerrain::UpdateTransforms(const Vector2& stageTilt, const Vector2& pivot,
                                    float bounceOffsetY, const Vector2& shakeTilt)
{
    if (parts_.empty()) return;

    // ステージ傾斜＋揺らし。GamePlayScene が単一メッシュに掛けていたのと同じ式
    const Vector2 tilt = { stageTilt.x + shakeTilt.x, stageTilt.y - shakeTilt.y };
    const Matrix4x4 rTilt = BuildStageRotation(tilt);
    const Vector3 pivot3 = { pivot.x, 0.0f, pivot.y };

    for (auto& partPtr : parts_)
    {
        if (!partPtr || !partPtr->object) continue;
        Part& part = *partPtr;

        // 回転: 自分の Y 回転 -> ステージ傾斜 の順に掛ける
        const Matrix4x4 rSelf = MakeRotateYMatrix(part.rotationY);
        const Matrix4x4 rCombined = Multiply(rSelf, rTilt);

        // 平行移動: ピボット中心に自分の位置を回した先
        const Vector3 rel = part.position - pivot3;
        Vector3 translate = ApplyRotation(rel, rTilt) + pivot3;
        translate.y += bounceOffsetY;

        part.object->SetTranslate(translate);
        part.object->SetScale({ part.scale, part.scale, part.scale });
        part.object->SetRotate(MatrixToEulerXYZ(rCombined));
        part.object->Update();

        if (part.collider)
        {
            part.collider->Update();
        }
    }
}

void StageTerrain::SyncConstantBuffers()
{
    for (auto& part : parts_)
    {
        if (part && part->object)
        {
            part->object->Update();
        }
    }
}

// ===================================================================
// 参照
// ===================================================================

StageTerrain::Part* StageTerrain::GetPart(int index)
{
    if (index < 0 || index >= static_cast<int>(parts_.size())) return nullptr;
    return parts_[static_cast<size_t>(index)].get();
}

int StageTerrain::IndexOf(const Part* part) const
{
    for (size_t i = 0; i < parts_.size(); ++i)
    {
        if (parts_[i].get() == part) return static_cast<int>(i);
    }
    return -1;
}

StageTerrain::Part* StageTerrain::FindPartUnder(float x, float z)
{
    SlimePhysics::GroundLayer layers[8];
    const int count = SlimePhysics::QueryGroundLayers(x, z, layers, 8);
    if (count <= 0) return nullptr;

    // 一番上の床（QueryGroundLayers は Y 降順）がどのパーツのものか
    const int meshIndex = layers[0].meshIndex;
    if (meshIndex < 0) return nullptr;

    for (auto& part : parts_)
    {
        if (part && part->groundMeshIndex == meshIndex) return part.get();
    }
    return nullptr;
}

bool StageTerrain::IsBossTriggerAt(float x, float z)
{
    // 上下段が重なっているところでも「いま立っている床」を見たいので、
    // 最上面だけでなく全レイヤを見てトリガー付きが混じっていないかを調べる。
    // ただし床が複数ある場合、上のほうに立っているとは限らないので、
    // 呼び出し側でプレイヤーの Y に近いものを選びたくなったらここを直すこと
    SlimePhysics::GroundLayer layers[8];
    const int count = SlimePhysics::QueryGroundLayers(x, z, layers, 8);
    if (count <= 0) return false;

    const int meshIndex = layers[0].meshIndex;
    for (auto& part : parts_)
    {
        if (part && part->groundMeshIndex == meshIndex) return part->bossTrigger;
    }
    return false;
}

bool StageTerrain::HasBossTrigger() const
{
    for (const auto& part : parts_)
    {
        if (part && part->bossTrigger) return true;
    }
    return false;
}

void StageTerrain::SetBaseColor(const Vector4& color)
{
    baseColor_ = color;
    for (auto& part : parts_)
    {
        if (part && part->object) part->object->SetColor(color);
    }
}
