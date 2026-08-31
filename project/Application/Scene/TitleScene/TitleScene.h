#pragma once

#include "BaseScene.h"
#include "Object3d.h"
#include <memory>
#include <vector>
#include <string>

class DirectXCom;
class KeyInput;
class Camera;
struct SceneRenderRequests;

class TitleScene : public BaseScene
{
public:
	enum class MenuState
	{
		Main,
		Briefing,
		Settings
	};

	void InitializeScene() override;
	void Finalize() override;
	void Update() override;
	void Draw(SceneRenderRequests& renderRequests) override;

	const char* GetSceneType() const { return "TITLE"; }

private:
	KeyInput* input_ = nullptr;
	std::unique_ptr<Object3d> duckModel_;
	std::unique_ptr<Object3d> enemyDuckModel_; // 背景の可愛い敵アヒル兵士
	MenuState currentMenu_ = MenuState::Main;
	float bgTimer_ = 0.0f;
	float startTransitionTimer_ = 0.0f;
	bool isStarting_ = false;

	// アヒルちゃんのインタラクティブ・リアクション用
	float duckJumpTimer_ = 0.0f;
	float duckSpinAngle_ = 0.0f;
	float duckCurrentYaw_ = -0.5f;
	float duckCurrentPitch_ = 0.1f;
	struct ClickSplash
	{
		float x, y;
		float timer;
		float maxTime;
		float scale;
	};
	std::vector<ClickSplash> splashes_;

	void DrawMainMenu(float screenW, float screenH, float deltaTime);
	void DrawBriefingModal(float screenW, float screenH);
	void DrawSettingsModal(float screenW, float screenH);
	void DrawCinematicBackground(float screenW, float screenH, float deltaTime);
};

