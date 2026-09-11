#pragma once

#include "BaseScene.h"
#include "../GameScene/RaidStats.h"

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

	// ゲームオーバー画面表示用戦績（Scene Context 経由で受け取った RaidStats）
	RaidStats raidStats_{};
};
