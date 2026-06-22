#pragma once

#include "BaseScene.h"

class DirectXCom;
class KeyInput;
class Camera;
struct SceneRenderRequests;

class TitleScene : public BaseScene
{
public:
	void InitializeScene() override;
	void Finalize() override;
	void Update() override;
   void Draw(SceneRenderRequests& renderRequests) override;

private:
	KeyInput* input_ = nullptr;
};

