#include"TitleScene.h"
#include"KeyInput.h"
#include"DirectXCom.h"
#include"SceneManager.h"

void TitleScene::InitializeScene()
{
	if (dxCommon_)
	{
		input_ = new KeyInput();
		input_->Initialize(dxCommon_->GetWindowAPI());
	}
}

void TitleScene::Finalize()
{
   delete input_;
	input_ = nullptr;
}

void TitleScene::Update()
{
	if (!input_)
	{
		return;
	}

	input_->Update();

	if (input_->TriggerKey(DIK_SPACE))
	{
		SceneManager::GetInstance()->ChangeScene("GAMEPLAY");
	}
}

void TitleScene::Draw(SceneRenderRequests& /*renderRequests*/)
{
}
