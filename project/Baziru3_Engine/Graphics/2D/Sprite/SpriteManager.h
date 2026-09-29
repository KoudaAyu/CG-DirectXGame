#pragma once
#include<string>
#include<vector>

class Camera;
class SpriteCom;
class WindowAPI;

#include"RenderContext.h"
#include"Sprite.h"
#include"Transform.h"


class SpriteManager
{
public:
	void Initialize(SpriteCom* spriteCom,const std::string& texturePath, size_t count);
	void Update();
	void Draw();
	void DrawAll(const RenderContext& ctx, Camera* camera = nullptr, const std::vector < std::unique_ptr<Sprite>>* externalSprites = nullptr );
	void DrawAll(Camera* camera = nullptr, const std::vector<std::unique_ptr<Sprite>>* externalSprites = nullptr);

	std::vector<std::unique_ptr<Sprite>>& GetSprites();

    void Finalize();

private:
	SpriteCom* spriteCom_;
	std::string texturePath_;
	std::vector<std::unique_ptr<Sprite>> sprites_;
};

