#include "SpriteManager.h"
#include "DebugCamera.h"
#include "SpriteCom.h"
#include "WindowsAPI.h"
#include "Light.h"

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

void SpriteManager::Update(WindowAPI* windowAPI, DebugCamera* debugCamera)
{
    for (auto& s : sprites_)
    {
        s->Update(windowAPI, debugCamera);
    }
}

void SpriteManager::Draw()
{
     if (!spriteCom_)
    {
        for (auto& s : sprites_) s->Draw();
        return;
    }
    RenderContext ctx{};
    ctx.commandList = spriteCom_->GetDxCommon()->GetCommandList().Get();
    ctx.windowAPI = spriteCom_->GetDxCommon()->GetWindowAPI();
    DrawAll(ctx, nullptr, nullptr);
}

void SpriteManager::DrawAll(const RenderContext& ctx, DebugCamera* debugCamera, const std::vector<std::unique_ptr<Sprite>>* externalSprites)
{
    if (!ctx.commandList)
    {
        return;
    }
    spriteCom_->SetupDraw(ctx.commandList);

    for (auto& sp : sprites_)
    {
        if (!sp) continue;
        sp->Update(ctx.windowAPI, debugCamera);
        sp->SetEnvironmentTextureHandle(ctx.environmentTextureHandle);
        if (ctx.light) sp->SetDirectionalLightResource(ctx.light->GetDirectionalLightResource());
        sp->Draw();
    }

    // 外部スプライト群（Gameが持つやつ）も同じ処理
    if (externalSprites)
    {
        for (auto& sp : *externalSprites)
        {
            if (!sp) continue;
            sp->Update(ctx.windowAPI, debugCamera);
            sp->SetEnvironmentTextureHandle(ctx.environmentTextureHandle);
            if (ctx.light) sp->SetDirectionalLightResource(ctx.light->GetDirectionalLightResource());
            sp->Draw();
        }
    }
}

std::vector<std::unique_ptr<Sprite>>& SpriteManager::GetSprites()
{
    return sprites_;
}