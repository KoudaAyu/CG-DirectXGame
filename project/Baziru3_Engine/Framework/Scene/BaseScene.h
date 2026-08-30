#pragma once

#include <cstddef>

class DirectXCom;
class SceneManager;
class Camera;
class Object3dCom;
class SkinningObject3dCom;
class MaterialManager;
class Light;
class ParticleManager;
class AudioManager;
class SpriteCom;
struct SceneRenderRequests;

class BaseScene {
public:
  virtual ~BaseScene() = default;

  // エンジンから呼ばれるシーン共通初期化エントリーポイント
  void Initialize(DirectXCom *dxCommon, Camera *camera) {
    dxCommon_ = dxCommon;
    camera_ = camera;
    InitializeScene();
  }

  virtual void InitializeScene() = 0;
  virtual void Finalize() = 0;
  virtual void Update() = 0;
  virtual void Draw(SceneRenderRequests &renderRequests) = 0;

  virtual void SetSceneManager(SceneManager *sceneManager) {
    sceneManager_ = sceneManager;
  }

protected:
  // 各種マネージャへの簡単アクセス用ショートカットゲッター
  Object3dCom *GetObject3dCom() const;
  SkinningObject3dCom *GetSkinningObject3dCom() const;
  MaterialManager *GetMaterialManager() const;
  Light *GetLight() const;
  ParticleManager *GetParticleManager() const;
  AudioManager *GetAudioManager() const;
  SpriteCom *GetSpriteCom() const;

protected:
  SceneManager *sceneManager_ = nullptr;
  DirectXCom *dxCommon_ = nullptr;
  Camera *camera_ = nullptr;
};
