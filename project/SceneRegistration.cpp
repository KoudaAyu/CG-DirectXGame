#include "SceneRegistration.h"
#include "SceneFactory.h"
#include "SceneManager.h"

#include"TitleScene.h"
#include"GamePlayScene.h"

void SceneRegistration::RegisterScenes()
{
	//所有権をSceneManagerに移す
	SceneFactory* factory = new SceneFactory();

	factory->Register("TITLE", []() -> BaseScene* { return new TitleScene(); });
	factory->Register("GAMEPLAY", []() -> BaseScene* { return new GamePlayScene(); });

	SceneManager::GetInstance()->SetSceneFactory(factory);
}
