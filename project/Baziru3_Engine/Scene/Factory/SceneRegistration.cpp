#include "SceneRegistration.h"
#include "SceneFactory.h"
#include "SceneManager.h"

#include "TitleScene.h"
#include "GamePlayScene.h"
#include "ClearScene/ClearScene.h"

void SceneRegistration::RegisterScenes()
{
  
    auto factory = std::make_unique<SceneFactory>();

    factory->Register("TITLE", []() -> std::unique_ptr<BaseScene> { return std::make_unique<TitleScene>(); });
    factory->Register("GAMEPLAY", []() -> std::unique_ptr<BaseScene> { return std::make_unique<GamePlayScene>(); });
    factory->Register("CLEAR", []() -> std::unique_ptr<BaseScene> { return std::make_unique<ClearScene>(); });

  
    SceneManager::GetInstance()->SetSceneFactory(std::move(factory));
}
