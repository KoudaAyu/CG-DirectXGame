#pragma once

#include "BaseScene.h"
class DirectXCom;
class KeyInput;
struct SceneRenderRequests;

class GameOverScene : public BaseScene
{
public:
	void InitializeScene() override;
	void Finalize() override;
	void Update() override;
	void Draw(SceneRenderRequests& renderRequests) override;

	const char* GetSceneType() const { return "GAMEOVER"; }

private:
	KeyInput* input_ = nullptr;
};
