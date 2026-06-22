#pragma once
#include<string>
#include<vector>

class DebugCamera;
class SpriteCom;
class WindowAPI;

#include"RenderContext.h"
#include"Sprite.h"
#include"Transform.h"


class SpriteManager
{
public:
	void Initialize(SpriteCom* spriteCom,const std::string& texturePath, size_t count);
	void Update(WindowAPI* windowAPI = nullptr, DebugCamera* debugCamera = nullptr);
	void Draw();
    void DrawAll(const RenderContext& ctx, DebugCamera* debugCamera, const std::vector < std::unique_ptr<Sprite>>* externalSprites = nullptr, bool updateExternal = true );
	void DrawAll(DebugCamera* debugCamera = nullptr, const std::vector<std::unique_ptr<Sprite>>* externalSprites = nullptr);

	std::vector<std::unique_ptr<Sprite>>& GetSprites();

    void Finalize();

private:
	SpriteCom* spriteCom_;
	std::string texturePath_;
	std::vector<std::unique_ptr<Sprite>> sprites_;
};

