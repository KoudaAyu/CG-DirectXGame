#include"TitleScene.h"
#include"KeyInput.h"
#include"SceneManager.h"

void TitleScene::Initialize(DirectXCom* dxCommon)
{
	dxCommon_ = dxCommon;

	SceneManager::GetInstance()->ChangeScene("GAMEPLAY");
}

void TitleScene::Finalize()
{
}

void TitleScene::Update()
{

}

void TitleScene::Draw()
{
}
