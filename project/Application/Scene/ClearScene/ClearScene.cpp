#include "ClearScene.h"
#include "KeyInput.h"
#include "DirectXCom.h"
#include "SceneManager.h"

void ClearScene::Initialize(DirectXCom* dxCommon, Camera* /*camera*/)
{
	dxCommon_ = dxCommon;

	if (dxCommon_)
	{
		input_ = new KeyInput();
		input_->Initialize(dxCommon_->GetWindowAPI());
	}
}

void ClearScene::Finalize()
{
	delete input_;
	input_ = nullptr;
}

void ClearScene::Update()
{
	if (!input_)
	{
		return;
	}

	input_->Update();

	if (input_->TriggerKey(DIK_SPACE))
	{
		SceneManager::GetInstance()->ChangeScene("TITLE");
	}
}

void ClearScene::Draw(SceneRenderRequests& /*renderRequests*/)
{
}
