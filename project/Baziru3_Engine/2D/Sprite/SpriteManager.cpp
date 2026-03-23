#include "SpriteManager.h"
#include "DebugCamera.h"
#include "SpriteCom.h"
#include "WindowsAPI.h"

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
    for (auto& s : sprites_) s->Draw();
}

std::vector<std::unique_ptr<Sprite>>& SpriteManager::GetSprites()
{
    return sprites_;
}