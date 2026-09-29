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

  /// <summary>
  /// ウィンドウメッセージ処理（EngineContext経由でカプセル化）
  /// </summary>
  bool ProcessMessage() override;

  // 初期化関係
  bool InitializeEngine();

  /// <summary>
  /// DirectXComの診断Logを出す
  /// </summary>
  void LogEngineDiagnostics();

  /// <summary>
  /// 描画に必要な共通リソース（サンプルのPlane等）を作る
  /// </summary>
  void InitializeModelResources();

public:
  std::ostream &logStream = log.GetLogStream();

  /// <summary>
  /// 【カプセル化】EngineContextへのアクセサ
  /// </summary>
  EngineContext *GetEngineContext() { return engine_.get(); }
  const EngineContext *GetEngineContext() const { return engine_.get(); }

  // --- 後方互換アクセサ（EngineContextに委譲） ---
  DirectXCom *GetDirectXCom() {
    return engine_ ? engine_->GetDirectXCom() : nullptr;
  }
  const DirectXCom *GetDirectXCom() const {
    return engine_ ? engine_->GetDirectXCom() : nullptr;
  }

  Object3d *GetObject3d() { return object3d_.get(); }
  const Object3d *GetObject3d() const { return object3d_.get(); }
  Object3dCom *GetObject3dCom() {
    return engine_ ? engine_->GetObject3dCom() : nullptr;
  }
  const Object3dCom *GetObject3dCom() const {
    return engine_ ? engine_->GetObject3dCom() : nullptr;
  }
  ParticleManager *GetParticleManager() {
    return engine_ ? engine_->GetParticleManager() : nullptr;
  }
  const ParticleManager *GetParticleManager() const {
    return engine_ ? engine_->GetParticleManager() : nullptr;
  }

private:
  void DrawObjects(const RenderContext &ctx);
  void DrawSprites(const RenderContext &ctx);
  void DrawParticles(const RenderContext &ctx);

private:
  ResourceLeakCheck leakChecker; // リソースリークチェック用のオブジェクト
  CrashDump crashDump;           // クラッシュダンプ生成用のオブジェクト
  Log log;

  // 【カプセル化】エンジン全サブシステム（DirectX12, 入力, 音声, カメラ, 描画基盤）を保持する司令塔
  std::unique_ptr<EngineContext> engine_;

  // ゲーム固有・デバッグ固有オブジェクト
  DebugCamera debugCamera_;
  std::unique_ptr<DebugUI> debugUI;
  std::unique_ptr<Model> model_;
  std::unique_ptr<ModelCom> modelCom_;
  std::unique_ptr<Object3d> object3d_;

  ParticleRenderer particleRenderer_;
  SphereRenderer sphereRenderer_;

private:
  std::vector<std::unique_ptr<Sprite>> sprites;
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
