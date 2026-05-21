#include "SpriteManager.h"
#include "DebugCamera.h"
#include "SpriteCom.h"
#include "WindowsAPI.h"
#include "Light.h"
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
        // 初期位置を指定された値に合わせる
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
    // If SpriteCom is not set we cannot render sprites here.
    if (!spriteCom_)
    {
        Logger::Log(std::cout, "SpriteManager::Draw called but spriteCom_ is null. Skipping draw.\n");
        return;
    }

    RenderContext ctx{};
    // 防御的チェック: dxCommon と command list が有効か確認
    if (!spriteCom_->GetDxCommon() || !spriteCom_->GetDxCommon()->GetCommandList())
    {
        Logger::Log(std::cout, "SpriteManager::Draw - dxCommon or commandList is null. Skipping draw.\n");
        return;
    }

    ctx.commandList = spriteCom_->GetDxCommon()->GetCommandList().Get();
    ctx.windowAPI = spriteCom_->GetDxCommon()->GetWindowAPI();
    DrawAll(ctx, nullptr, nullptr);
}

void SpriteManager::DrawAll(const RenderContext& ctx, DebugCamera* debugCamera, const std::vector<std::unique_ptr<Sprite>>* externalSprites, bool updateExternal)
{
    if (!ctx.commandList)
    {
        Logger::Log(std::cout, "SpriteManager::DrawAll called with null commandList. Skipping.\n");
        return;
    }
    spriteCom_->SetupDraw(ctx.commandList);

    for (auto& sp : sprites_)
    {
        if (!sp) continue;
        sp->Update(ctx.windowAPI, debugCamera);
        if (ctx.light) sp->SetDirectionalLightResource(ctx.light->GetDirectionalLightResource());
        sp->Draw();
    }

    // 外部スプライト群（Gameが持つやつ）も同じ処理
    if (externalSprites)
    {
        for (auto& sp : *externalSprites)
        {
            if (!sp) continue;
            if (updateExternal)
            {
                sp->Update(ctx.windowAPI, debugCamera);
            }
            // updateExternal が false の場合、呼び出し側が既に変換を更新している前提です
            if (ctx.light) sp->SetDirectionalLightResource(ctx.light->GetDirectionalLightResource());
            sp->Draw();
        }
    }
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