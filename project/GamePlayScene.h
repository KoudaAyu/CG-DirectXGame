#pragma once
#include"Sound.h"

class GamePlayScene
{
public:
	void Initialize();

	void Finalize();

	void Update();

	void Draw();

private:
	Sound* sound = nullptr;
};