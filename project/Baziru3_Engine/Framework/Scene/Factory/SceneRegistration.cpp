#include "SceneRegistration.h"
#include "SceneFactory.h"
#include "SceneManager.h"

void SceneRegistration::RegisterScenes() {
  SceneManager::GetInstance()->SetSceneFactory(std::make_unique<SceneFactory>());

  // 新規ゲーム開発時はここにアプリケーション層のシーンを登録してください
  // 例: REGISTER_SCENE(TitleScene, "TITLE");
}