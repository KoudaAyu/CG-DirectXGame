#include "SceneRegistration.h"
#include "SceneFactory.h"
#include "SceneManager.h"

#include "TitleScene.h"
#include "GamePlayScene.h"
#include "Application/Scene/ClearScene/ClearScene.h"
#include "Application/Scene/GameOverScene/GameOverScene.h"

void SceneRegistration::RegisterScenes() {
  REGISTER_SCENE(TitleScene, "TITLE");
  REGISTER_SCENE(GamePlayScene, "GAMEPLAY");
  REGISTER_SCENE(ClearScene, "CLEAR");
  REGISTER_SCENE(GameOverScene, "GAMEOVER");
}
