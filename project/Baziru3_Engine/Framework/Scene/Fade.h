#pragma once

#include "Sprite.h"
#include <memory>

#include <chrono>

class SpriteCom;
class WindowAPI;

class Fade {
public:
  void Initialize(SpriteCom *spriteCom, WindowAPI *windowAPI);
  void Finalize();

  void Update();
  void Draw();

  void StartFadeOut(int frameCount = 30);
  void StartFadeIn(int frameCount = 30);

  bool IsAvailable() const;
  bool IsBusy() const;
  bool IsFadeOutFinished() const;

private:
  enum class State {
    None,
    FadeOut,
    FadeIn,
  };

  void ApplyColor();
  static float CalculateFadeSpeed(int frameCount);

private:
  SpriteCom *spriteCom_ = nullptr;
  WindowAPI *windowAPI_ = nullptr;
  std::unique_ptr<Sprite> fadeSprite_;
  State state_ = State::None;
  float alpha_ = 0.0f;
  float fadeSpeed_ = 0.0f;
  std::chrono::steady_clock::time_point lastTime_;
};
