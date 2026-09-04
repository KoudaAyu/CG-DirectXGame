#pragma once

#include <cstddef>
#include <string>

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

  // --- アプリ開発用 直感ショートカット API ---
  void ChangeScene(const std::string &sceneName);
  void RestartScene();

  SceneManager *GetSceneManager() const { return sceneManager_; }
  Camera *GetCamera() const { return camera_; }
  DirectXCom *GetDirectXCom() const { return dxCommon_; }

  // シーン間パラメータ共有（Scene Context）
  template <typename T>
  void SetSceneData(const std::string &key, const T &value);

  void SetSceneData(const std::string &key, const std::string &value);
  void SetSceneData(const std::string &key, const char* value);
  void SetSceneDataInt(const std::string &key, int value);
  void SetSceneDataFloat(const std::string &key, float value);
  void SetSceneDataDouble(const std::string &key, double value);
  void SetSceneDataBool(const std::string &key, bool value);
  void SetSceneDataString(const std::string &key, const std::string &value);

  template <typename T>
  T GetSceneData(const std::string &key, const T &defaultVal = T{}) const;

  std::string GetSceneData(const std::string &key, const std::string &defaultVal = "") const;
  int GetSceneDataInt(const std::string &key, int defaultVal = 0) const;
  float GetSceneDataFloat(const std::string &key, float defaultVal = 0.0f) const;
  double GetSceneDataDouble(const std::string &key, double defaultVal = 0.0) const;
  bool GetSceneDataBool(const std::string &key, bool defaultVal = false) const;
  std::string GetSceneDataString(const std::string &key, const std::string &defaultVal = "") const;

  bool HasSceneData(const std::string &key) const;
  void RemoveSceneData(const std::string &key);
  void ClearAllSceneData();

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
