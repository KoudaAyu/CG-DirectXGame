#pragma once

#include <memory>

#include "AudioManager.h"
#include "Baziru3_Engine/Core/Base/EngineContext.h"
#include "Baziru3_Engine/Core/IO/Mouse/MouseInput.h"
#include "Baziru3_Engine/Framework/Scene/Fade.h"
#include "Baziru3_Engine\Core\Base\OffScreenRendering\OffScreenRendering.h"
#include "Baziru3_Engine\Graphics\Graphics\Particle\ParticleRenderer.h"
#include "Baziru3_Engine\Graphics\Graphics\Sphere\SphereRenderer.h"
#include "Camera.h"
#include "CrashDump.h"
#include "DebugUI.h"
#include "DirectXCom.h"
#include "Framework.h"
#include "ImGuiManager.h"
#include "Light.h"
#include "Log.h"
#include "MaterialManager.h"
#include "Model.h"
#include "Object3d.h"
#include "Object3dCom.h"
#include "ParticleEmitter.h"
#include "ParticleManager.h"
#include "ResourceLeakCheck.h"
#include "SceneManager.h"
#include "SceneRegistration.h"
#include "SkinningObject3dCom.h"
#include "SkyBox.h"
#include "SkyboxCom.h"
#include "Sound.h"
#include "Sphere.h"
#include "Sprite.h"
#include "SpriteCom.h"
#include "SpriteManager.h"
#include "TextureManager.h"
#include "WindowsAPI.h"

#include <random>
#include <vector>

#include "RenderContext.h"

class Game : public Framework {
public:
  void Initialize() override;
  void Finalize() override;
  void Update() override;
  void Draw() override;

  bool IsQuitRequested() override;

  static bool IsImGuiVisible() { return s_showImGui; }
  static void SetImGuiVisible(bool visible) { s_showImGui = visible; }
  static void ToggleImGuiVisible() { s_showImGui = !s_showImGui; }

private:
  static inline bool s_showImGui = true;

public:

  // 初期化関係
  bool InitializeEngine();

  /// <summary>
  /// DirectXComの診断Logを出す
  /// </summary>
  void LogEngineDiagnostics();

  /// <summary>
  /// SceneManagerに渡す基盤オブジェクトを用意する部分
  /// </summary>
  void InitializeSceneCore();

  /// <summary>
  /// 描画に必要な共通リソースを作る
  /// </summary>
  void InitializeModelResources();

  void InitializeSceneResources();

  /// <summary>
  /// 音声、入力系の初期化
  /// </summary>
  void InitializeAudioAndInput();

public:
  std::ostream &logStream = log.GetLogStream();
  DirectXCom *GetDirectXCom() {
    return engine_ ? engine_->GetDirectXCom() : nullptr;
  }
  const DirectXCom *GetDirectXCom() const {
    return engine_ ? engine_->GetDirectXCom() : nullptr;
  }

  Object3d *GetObject3d() { return object3d_.get(); }
  const Object3d *GetObject3d() const { return object3d_.get(); }
  Object3dCom *GetObject3dCom() { return object3dCom.get(); }
  const Object3dCom *GetObject3dCom() const { return object3dCom.get(); }
  ParticleManager *GetParticleManager() { return particleManager.get(); }
  const ParticleManager *GetParticleManager() const {
    return particleManager.get();
  }

private:
  void DrawObjects(const RenderContext &ctx);
  void DrawSprites(const RenderContext &ctx);
  void DrawParticles(const RenderContext &ctx);

private:
  ResourceLeakCheck leakChecker; // リソースリークチェック用のオブジェクト
  CrashDump crashDump; // クラッシュダンプ生成用のオブジェクト
  Log log;

  std::unique_ptr<Camera> camera_;
  std::unique_ptr<EngineContext> engine_;
  std::unique_ptr<ImGuiManager> imguiManager;
  std::unique_ptr<Light> light;
  std::unique_ptr<Model> model_;
  std::unique_ptr<ModelCom> modelCom_;
  std::unique_ptr<Object3d> object3d_;
  std::unique_ptr<Object3dCom> object3dCom;
  std::unique_ptr<SkinningObject3dCom> skinningObject3dCom;
  std::unique_ptr<OffScreenRendering> offScreenRendering_;
  std::unique_ptr<ParticleManager> particleManager;
  ParticleRenderer particleRenderer_;
  SphereRenderer sphereRenderer_;
  std::unique_ptr<SkyBox> skybox_;
  std::unique_ptr<SkyboxCom> skyboxCom_;

  DebugCamera debugCamera_;

  KeyInput inputManager;
  // Mouse input for cursor sprite
  MouseInput mouseInput;
  std::unique_ptr<AudioManager> audioManager_;
  std::unique_ptr<MaterialManager> materialManager_;
  std::unique_ptr<DebugUI> debugUI; // debug UI
  std::unique_ptr<Fade> fadeApplication_;

private:
  std::vector<std::unique_ptr<Sprite>> sprites;
  // index of cursor sprite in sprites vector, -1 if none
  int cursorSpriteIndex = -1;
  Sprite::Transform transformObject;

  Object3d::ModelData modelData;

  RenderContext PrepareRenderContext();

  const float kDeltaTime = 1.0f / 60.0f;

  // SRVの切り替え
  bool useMonsterBall = true;
  // Objectの描画切り替え
  bool drawObject = false;
  bool drawSprite = false;

  uint32_t textureIndexUvChecker = TextureManager::kInvalidTextureIndex;
  uint32_t textureIndexModelTex = TextureManager::kInvalidTextureIndex;
  uint32_t textureIndexSkybox_ = TextureManager::kInvalidTextureIndex;
};
