#pragma once

#include "BaseScene.h"
#include"DirectXCom.h"
#include"Sound.h"
#include"Sphere.h"

#include<vector>

class SpriteCom;

class GamePlayScene : public BaseScene
{
public:
	
	void Initialize(DirectXCom* dxCommon) override;

	void Finalize() override;

	void Update() override;

	void Draw() override;

	void SetSpriteCom(SpriteCom* spriteCom) { this->spriteCom = spriteCom; }

private:

	DirectXCom* directXCom = nullptr;
	Sound* sound = nullptr;
	SpriteCom* spriteCom = nullptr;
	std::unique_ptr<Sphere> sphere;
	std::vector<std::unique_ptr<Sprite>> sprites;
};