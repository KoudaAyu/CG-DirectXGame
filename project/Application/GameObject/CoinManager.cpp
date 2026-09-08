#include "Application/GameObject/CoinManager.h"

#include "Baziru3_Engine/Graphics/3D/Object/Object3dCom.h"
#include "Baziru3_Engine/Graphics/2D/Texture/TextureManager.h"
#include "Application/Player/PikminPlayer.h"

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace
{
    constexpr float kPi = 3.14159265358979323846f;

    CoinConfig gCoinConfig{};

    /**
     * @brief コイン用の円盤メッシュを生成する
     *
     * 板を Z 軸方向に薄く伸ばした円柱。XY 平面に円が立っているので、
     * Y 軸で自転させると表 -> 横 -> 裏 と見え方が変わって「コインらしく」なる。
     */
    Object3d::ModelData GenerateCoinMesh(float radius, float thickness, int segments)
    {
        Object3d::ModelData model;

        segments = (std::clamp)(segments, 6, 128);
        const float halfT = (std::max)(0.005f, thickness * 0.5f);

        auto PushVertex = [&model](const Vector3& pos, const Vector3& normal, const Vector2& uv) -> uint32_t {
            Sprite::VertexData v{};
            v.position = { pos.x, pos.y, pos.z, 1.0f };
            v.normal = normal;
            v.texcoord = uv;
            model.vertices.push_back(v);
            return static_cast<uint32_t>(model.vertices.size() - 1);
        };

        // --- 表の面（-Z 向き）---
        uint32_t frontCenter = PushVertex({ 0.0f, 0.0f, -halfT }, { 0.0f, 0.0f, -1.0f }, { 0.5f, 0.5f });
        uint32_t frontFirst = static_cast<uint32_t>(model.vertices.size());
        for (int i = 0; i < segments; ++i)
        {
            float t = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(segments);
            float c = std::cos(t);
            float s = std::sin(t);
            PushVertex({ radius * c, radius * s, -halfT }, { 0.0f, 0.0f, -1.0f },
                       { 0.5f + 0.5f * c, 0.5f - 0.5f * s });
        }

        // --- 裏の面（+Z 向き）---
        uint32_t backCenter = PushVertex({ 0.0f, 0.0f, halfT }, { 0.0f, 0.0f, 1.0f }, { 0.5f, 0.5f });
        uint32_t backFirst = static_cast<uint32_t>(model.vertices.size());
        for (int i = 0; i < segments; ++i)
        {
            float t = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(segments);
            float c = std::cos(t);
            float s = std::sin(t);
            PushVertex({ radius * c, radius * s, halfT }, { 0.0f, 0.0f, 1.0f },
                       { 0.5f + 0.5f * c, 0.5f - 0.5f * s });
        }

        // --- 側面（法線は放射方向）---
        uint32_t sideFirst = static_cast<uint32_t>(model.vertices.size());
        for (int i = 0; i <= segments; ++i)
        {
            int idx = i % segments;
            float t = 2.0f * kPi * static_cast<float>(idx) / static_cast<float>(segments);
            float c = std::cos(t);
            float s = std::sin(t);
            float u = static_cast<float>(i) / static_cast<float>(segments);
            PushVertex({ radius * c, radius * s, -halfT }, { c, s, 0.0f }, { u, 0.0f });
            PushVertex({ radius * c, radius * s,  halfT }, { c, s, 0.0f }, { u, 1.0f });
        }

        // --- インデックス ---
        for (int i = 0; i < segments; ++i)
        {
            uint32_t a = frontFirst + static_cast<uint32_t>(i);
            uint32_t b = frontFirst + static_cast<uint32_t>((i + 1) % segments);
            model.indices.push_back(frontCenter);
            model.indices.push_back(b);
            model.indices.push_back(a);
        }
        for (int i = 0; i < segments; ++i)
        {
            uint32_t a = backFirst + static_cast<uint32_t>(i);
            uint32_t b = backFirst + static_cast<uint32_t>((i + 1) % segments);
            model.indices.push_back(backCenter);
            model.indices.push_back(a);
            model.indices.push_back(b);
        }
        for (int i = 0; i < segments; ++i)
        {
            uint32_t v0 = sideFirst + static_cast<uint32_t>(i * 2);
            uint32_t v1 = v0 + 1;
            uint32_t v2 = v0 + 2;
            uint32_t v3 = v0 + 3;
            model.indices.push_back(v0);
            model.indices.push_back(v1);
            model.indices.push_back(v2);
            model.indices.push_back(v2);
            model.indices.push_back(v1);
            model.indices.push_back(v3);
        }

        model.boundingRadius = std::sqrt(radius * radius + halfT * halfT) * 1.4f;
        return model;
    }
}

CoinConfig& CoinManager::GetConfig()
{
    return gCoinConfig;
}

CoinManager::~CoinManager()
{
    Finalize();
}

void CoinManager::Initialize(Object3dCom* object3dCom, Camera* camera)
{
    object3dCom_ = object3dCom;
    camera_ = camera;
    collectedCount_ = 0;
    BuildMesh();
}

void CoinManager::BuildMesh()
{
    const CoinConfig& c = GetConfig();

    if (c.modelDirectory && c.modelDirectory[0] != '\0' &&
        c.modelFileName && c.modelFileName[0] != '\0')
    {
        // モデルファイルが指定されていればそちらを使う
        modelData_ = Object3d::LoadObjFile(c.modelDirectory, c.modelFileName);
        if (!modelData_.material.textureFilePath.empty())
        {
            textureIndex_ = TextureManager::GetInstance()->Load(modelData_.material.textureFilePath);
            modelData_.material.textureIndex = textureIndex_;
        }
    }
    else
    {
        modelData_ = GenerateCoinMesh(c.radius, c.thickness, c.segments);
        // テクスチャ無し。Object3dCom::Draw が白テクスチャにフォールバックしてくれるので、
        // 色は Object3d::SetColor だけで決まる
        modelData_.material.textureIndex = TextureManager::kInvalidTextureIndex;
        textureIndex_ = TextureManager::kInvalidTextureIndex;
    }

    meshReady_ = !modelData_.vertices.empty();
}

void CoinManager::Finalize()
{
    ClearAll();
    object3dCom_ = nullptr;
    camera_ = nullptr;
    meshReady_ = false;
}

Coin* CoinManager::Spawn(const Vector3& stageLocalPos)
{
    if (!object3dCom_ || !meshReady_) return nullptr;

    auto coin = std::make_unique<Coin>();
    coin->Initialize(object3dCom_, camera_, modelData_, stageLocalPos);
    if (Object3d* obj = coin->GetObject3d())
    {
        obj->SetColor(GetConfig().color);
    }

    Coin* raw = coin.get();
    coins_.push_back(std::move(coin));
    return raw;
}

void CoinManager::Remove(Coin* coin)
{
    if (!coin) return;

    auto it = std::find_if(coins_.begin(), coins_.end(),
                           [coin](const std::unique_ptr<Coin>& c) { return c.get() == coin; });
    if (it == coins_.end()) return;

    if ((*it)->IsCollected() && collectedCount_ > 0) --collectedCount_;
    (*it)->Finalize();
    coins_.erase(it);
}

void CoinManager::ClearAll()
{
    for (auto& c : coins_)
    {
        if (c) c->Finalize();
    }
    coins_.clear();
    collectedCount_ = 0;
}

void CoinManager::RefreshFromConfig()
{
    BuildMesh();
    if (!meshReady_) return;

    const CoinConfig& cfg = GetConfig();
    for (auto& c : coins_)
    {
        if (!c) continue;

        Vector3 anchor = c->GetStageLocalPosition();
        bool wasCollected = c->IsCollected();

        c->Finalize();
        c->Initialize(object3dCom_, camera_, modelData_, anchor);
        if (Object3d* obj = c->GetObject3d())
        {
            obj->SetColor(cfg.color);
        }
        if (wasCollected) c->Collect();
    }
}

void CoinManager::Update(float deltaTime, const Vector2& stageTilt, PikminPlayer* player)
{
    const CoinConfig& cfg = GetConfig();

    Vector3 playerPos = player ? player->GetPosition() : Vector3{ 0.0f, 0.0f, 0.0f };
    Vector2 pivot{ playerPos.x, playerPos.z };

    for (auto& c : coins_)
    {
        if (!c || c->IsCollected()) continue;
        c->Update(deltaTime, stageTilt, pivot, cfg.spinSpeed, cfg.bobHeight, cfg.bobSpeed, cfg.heightOffset);
    }

    // エディタ中は取得しない。置いた瞬間にプレイヤーの足元で消えると配置できない
    if (editorMode_ || !player) return;

    // 見た目半径の規約は SlimeCollision と同じ（scale * 0.78）
    float playerRadius = player->GetCurrentScale() * 0.78f;
    float hitDist = playerRadius + cfg.collectRadius;
    float hitDistSq = hitDist * hitDist;

    for (auto& c : coins_)
    {
        if (!c || c->IsCollected()) continue;

        const Vector3& cp = c->GetPosition();
        float dx = cp.x - playerPos.x;
        float dz = cp.z - playerPos.z;
        if (dx * dx + dz * dz > hitDistSq) continue;

        // 高さも見る。これが無いと、上下段が重なっているところで
        // 17m 下のコインを真上から取れてしまう
        if (std::abs(cp.y - playerPos.y) > cfg.collectHeight) continue;

        c->Collect();
        ++collectedCount_;
    }
}

void CoinManager::Draw(const RenderContext& ctx)
{
    if (!meshReady_) return;

    for (auto& c : coins_)
    {
        if (c) c->Draw(ctx);
    }
}

void CoinManager::DrawImGui()
{
#ifdef USE_IMGUI
    if (!ImGui::CollapsingHeader("Coin")) return;

    CoinConfig& cfg = GetConfig();

    ImGui::Text("Total: %d / Collected: %d / Remaining: %d",
                GetTotalCount(), GetCollectedCount(), GetRemainingCount());

    if (ImGui::Button("Reset Collected"))
    {
        for (auto& c : coins_) { if (c) c->Revive(); }
        collectedCount_ = 0;
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear All Coins"))
    {
        ClearAll();
    }

    ImGui::SeparatorText("Look");
    bool meshDirty = false;
    meshDirty |= ImGui::DragFloat("Radius", &cfg.radius, 0.01f, 0.05f, 3.0f);
    meshDirty |= ImGui::DragFloat("Thickness", &cfg.thickness, 0.01f, 0.01f, 1.0f);
    meshDirty |= ImGui::DragInt("Segments", &cfg.segments, 1.0f, 6, 64);

    if (ImGui::ColorEdit4("Color", &cfg.color.x))
    {
        for (auto& c : coins_)
        {
            if (c && c->GetObject3d()) c->GetObject3d()->SetColor(cfg.color);
        }
    }

    ImGui::DragFloat("Height Offset", &cfg.heightOffset, 0.01f, 0.0f, 5.0f);
    ImGui::DragFloat("Spin Speed", &cfg.spinSpeed, 0.05f, 0.0f, 12.0f);
    ImGui::DragFloat("Bob Height", &cfg.bobHeight, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Bob Speed", &cfg.bobSpeed, 0.05f, 0.0f, 12.0f);

    ImGui::SeparatorText("Collect");
    ImGui::DragFloat("Collect Radius", &cfg.collectRadius, 0.05f, 0.1f, 5.0f);
    ImGui::DragFloat("Collect Height", &cfg.collectHeight, 0.05f, 0.5f, 20.0f);

    if (meshDirty)
    {
        RefreshFromConfig();
    }
#endif
}
