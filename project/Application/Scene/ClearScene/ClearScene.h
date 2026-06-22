#pragma once

#include "BaseScene.h"

class DirectXCom;
class KeyInput;
class Camera;
struct SceneRenderRequests;

class ClearScene : public BaseScene
{
public:
	void InitializeScene() override;
	void Finalize() override;
	void Update() override;
	void Draw(SceneRenderRequests& renderRequests) override;

	const char* GetSceneType() const override { return "CLEAR"; }

private:
	DirectXCom* dxCommon_ = nullptr;
	KeyInput* input_ = nullptr;
};
