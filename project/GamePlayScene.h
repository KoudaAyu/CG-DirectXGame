#pragma once

#include "BaseScene.h"
#include"DirectXCom.h"
#include"ParticleEmitter.h"
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
	Emitter emitter;
	Sound* sound = nullptr;
	SpriteCom* spriteCom = nullptr;
	ParticleEmitter particleEmitter;
	std::unique_ptr<Sphere> sphere;
	std::unique_ptr<ParticleManager> particleManager;
	std::vector<std::unique_ptr<Sprite>> sprites;
	std::list<ParticleManager::Particle> particles;

private:
	const float kDeltaTime = 1.0f / 60.0f;
};