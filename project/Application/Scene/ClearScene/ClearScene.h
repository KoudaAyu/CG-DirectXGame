#pragma once

#include "BaseScene.h"
#include "../GameScene/RaidStats.h"

class KeyInput;
struct SceneRenderRequests;

class ClearScene : public BaseScene
{
public:
	void InitializeScene() override;
	void Finalize() override;
	void Update() override;
	void Draw(SceneRenderRequests& renderRequests) override;

	const char* GetSceneType() const { return "CLEAR"; }

private:
	KeyInput* input_ = nullptr;

	// クリア画面表示用戦績（Scene Context 経由で受け取った RaidStats）
	RaidStats raidStats_{};
};
