#include "SpriteManager.h"
#include "Log.h"
#include "DebugCamera.h"
#include "SpriteCom.h"
#include "WindowsAPI.h"
#include "Light.h"
#include "SceneManager.h"
#include <iostream>

void SpriteManager::Initialize(SpriteCom* spriteCom, const std::string& texturePath, size_t count)
{
    spriteCom_ = spriteCom;
    texturePath_ = texturePath;
    sprites_.clear();
    for (size_t i = 0; i < count; ++i)
    {
        auto s = std::make_unique<Sprite>();
        s->Initialize(spriteCom_, texturePath_);
        // Ensure initial position matches assignment requirement
        s->SetPosition({ 100.0f, 100.0f });
        sprites_.push_back(std::move(s));
    }
}

void SpriteManager::Update()
{
    for (auto& s : sprites_)
    {
        s->Update();
    }
}

void SpriteManager::Draw()
{
    // If SpriteCom is not set we cannot render sprites here.
    if (!spriteCom_)
    {
        Logger::Log(std::cout, "SpriteManager::Draw called but spriteCom_ is null. Skipping draw.\n");
        return;
    }

    RenderContext ctx{};
    // Defensive: ensure dxCommon and command list are valid
    if (!spriteCom_->GetDxCommon() || !spriteCom_->GetDxCommon()->GetCommandList())
    {
        Logger::Log(std::cout, "SpriteManager::Draw - dxCommon or commandList is null. Skipping draw.\n");
        return;
    }

    RenderContext buildCtx(
        spriteCom_->GetDxCommon()->GetCommandList().Get(),
        spriteCom_->GetDxCommon()->GetWindowAPI(),
        nullptr,
        nullptr
    );
    DrawAll(buildCtx, nullptr, nullptr);
}

void SpriteManager::DrawAll(const RenderContext& ctx, DebugCamera* debugCamera, const std::vector<std::unique_ptr<Sprite>>* externalSprites)
{
    if (!ctx.GetRawCommandList())
    {
        Logger::Log(std::cout, "SpriteManager::DrawAll called with null commandList. Skipping.\n");
        return;
    }
    spriteCom_->SetupDraw(ctx.GetRawCommandList());

    for (auto& sp : sprites_)
    {
        if (!sp) continue;
        if (ctx.GetLight()) sp->SetDirectionalLightResource(ctx.GetLight()->GetDirectionalLightResource());
        sp->Draw(ctx.GetRawCommandList());
    }

    // 外部スプライト群（Gameが持つやつ）も同じ処理
    if (externalSprites)
    {
        for (auto& sp : *externalSprites)
        {
            if (!sp) continue;
            if (ctx.GetLight()) sp->SetDirectionalLightResource(ctx.GetLight()->GetDirectionalLightResource());
            sp->Draw(ctx.GetRawCommandList());
        }
    }
}

void SpriteManager::DrawAll(DebugCamera* debugCamera, const std::vector<std::unique_ptr<Sprite>>* externalSprites)
{
    if (!spriteCom_) return;
    DirectXCom* dx = spriteCom_->GetDxCommon();
    if (!dx) return;

    RenderContext ctx{};
    ctx.commandList = dx->GetCommandList().Get();
    ctx.windowAPI = dx->GetWindowAPI();
    ctx.camera = SceneManager::GetInstance()->GetCamera();
    ctx.light = SceneManager::GetInstance()->GetLight();
    ctx.materialGPUAddress = 0;

    DrawAll(ctx, debugCamera, externalSprites);
}

std::vector<std::unique_ptr<Sprite>>& SpriteManager::GetSprites()
{
    return sprites_;
}

void SpriteManager::Finalize()
{
    for (auto& s : sprites_)
    {
        if (s)
        {
            s->Finalize();
        }
    }
    sprites_.clear();
    spriteCom_ = nullptr;
}