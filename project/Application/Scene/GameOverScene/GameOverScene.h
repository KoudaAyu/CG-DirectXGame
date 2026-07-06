#pragma once

#include "BaseScene.h"
#include "KeyInput.h"
#include "DirectXCom.h"
#include "SceneManager.h"

class GameOverScene : public BaseScene
{
public:
	void InitializeScene() override
	{
		if (dxCommon_)
		{
			input_ = new KeyInput();
			input_->Initialize(dxCommon_->GetWindowAPI());
		}
	}

	void Finalize() override
	{
		delete input_;
		input_ = nullptr;
	}

	void Update() override
	{
		if (input_)
		{
			input_->Update();
			if (input_->TriggerKey(DIK_SPACE))
			{
				SceneManager::GetInstance()->ChangeScene("TITLE");
			}
		}
	}

	void Draw(SceneRenderRequests& /*renderRequests*/) override
	{
	}

	const char* GetSceneType() const { return "GAMEOVER"; }

private:
	KeyInput* input_ = nullptr;
};
