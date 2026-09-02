#pragma once
#include "BaseScene.h"
#include <string>

class EngineDefaultScene : public BaseScene {
public:
  EngineDefaultScene() = default;
  virtual ~EngineDefaultScene() override = default;

  virtual void InitializeScene() override;
  virtual void Finalize() override;
  virtual void Update() override;
  virtual void Draw(SceneRenderRequests &renderRequests) override;

private:
  float timer_ = 0.0f;
};
