#pragma once
#include<string>
#include<vector>

class DebugCamera;
class SpriteCom;
class WindowAPI;

#include"Sprite.h"
#include"Transform.h"

class SpriteManager
{
public:
	void Initialize(SpriteCom* spriteCom,const std::string& texturePath, size_t count);
	// Update now only advances sprite state (position is controlled externally via DebugUI)
	void Update(WindowAPI* windowAPI, DebugCamera* debugCamera);
	void Draw();

	std::vector<std::unique_ptr<Sprite>>& GetSprites();

private:
	SpriteCom* spriteCom_;
	std::string texturePath_;
	std::vector<std::unique_ptr<Sprite>> sprites_;
};

