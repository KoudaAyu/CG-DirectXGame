#include "Game.h"
#include "Application/Scene/GameScene/GamePlayScene.h"
#include "Baziru3_Engine/Core/Base/Pipeline/PipelineStateManager.h"
#include "DebugUI.h"
#include <future>
#include <mutex>

#include <combaseapi.h>

#include <iomanip>
#include <sstream>

#include "Baziru3_Engine/Core/Base/SubsystemFactory.h"
#include "Baziru3_Engine\Graphics\Graphics\SceneRenderRequests.h"
#include "RenderContext.h"
#include "RootParam.h"

#include "Application/Config/AppDebugConfig.h"
#include "Application/Transition/StingerTransition.h"
#include "Application/Transition/DeathTransition.h"

#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#include <imgui_internal.h>
#endif

#if defined(USE_IMGUI) && !APP_DEBUG_UI
namespace {
// エンジン側が無条件で出す開発用ウィンドウ。
// Release では描画直前に非表示にして、画面にも入力にも関わらないようにする。
constexpr const char *kEngineDebugWindows[] = {
    "Engine Scene Manager",
    "Collision Debug Overlay Settings",
    "Engine Fallback Scene",
};

void HideEngineDebugWindows() {
  if (!ImGui::GetCurrentContext()) {
    return;
  }
  for (const char *name : kEngineDebugWindows) {
    if (ImGuiWindow *window = ImGui::FindWindowByName(name)) {
      window->Hidden = true;
    }
  }
}
} // namespace
#endif

void Game::Initialize() {

  Framework::Initialize();
  crashDump.Install();
  log.Initialize();

  // 1. エンジン全体の初期化（全サブシステム・DirectX12基盤・パイプライン・SceneManagerの一括初期化）
  // 【カプセル化の目的】
  // DirectX12やWindowAPI、音声、入力、カメラ等の生成責任をEngineContext内に隠蔽
  if (!InitializeEngine()) {
    return;
  }

  LogEngineDiagnostics();

#if defined(USE_IMGUI) && !APP_DEBUG_UI
  // Release ではウィンドウ配置の imgui.ini を書き出さない
  if (ImGui::GetCurrentContext()) {
    ImGui::GetIO().IniFilename = nullptr;
  }
#endif

  auto *spriteCom = engine_->GetSpriteCom();
  auto *window = engine_->GetWindowAPI();
  auto *camera = engine_->GetCamera();

  // 2. 描画に必要な共通リソース（サンプルのPlane等）を作る
  InitializeModelResources();

  // Transform初期化
  Sprite::Transform transform = {
      {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};

  transformObject = {
      {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};

  // スプライト生成（UVチェッカー）
  if (auto sp = Sprite::Create(spriteCom, transform, "Resources/uvChecker.png")) {
    sprites.emplace_back(std::move(sp));
  } else {
    Logger::Log(logStream, "Failed to create sprite: Resources/uvChecker.png\n");
  }

  // カーソル用スプライト生成
  Sprite::Transform tc = {
      {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
  if (auto cursor = Sprite::Create(spriteCom, tc, "Resources/CG4/circle2.png")) {
    cursor->SetSize({32.0f, 32.0f});
    cursor->SetAnchorPoint({0.0f, 0.0f});
    sprites.emplace_back(std::move(cursor));
    cursorSpriteIndex = static_cast<int>(sprites.size()) - 1;
  }

  // デバッグカメラ初期化
  debugCamera_.Initialize(window);

  // デバッグUIの初期化
  SpriteManager *uiSpriteManager = engine_->GetSpriteManager();
  debugUI = std::make_unique<DebugUI>(
      engine_->GetMaterialManager(), uiSpriteManager, camera, &transformObject,
      &useMonsterBall, &drawObject, &drawSprite);
  debugUI->SetOffScreenRendering(engine_->GetOffScreenRendering());
  debugUI->SetLight(engine_->GetLight());
  debugUI->Initialize();

  // シーン登録と初期シーン設定
  SceneRegistration::RegisterScenes();

  // アプリケーション層トランジション初期化（猫麦とろろ風スティンガー演出）
  if (engine_) {
    StingerTransition::GetInstance()->Initialize(engine_->GetSpriteCom(),
                                                 engine_->GetWindowAPI());
    StingerTransition::GetInstance()->SetOffScreenRendering(
        engine_->GetOffScreenRendering());

    // 死亡時専用の軍用メディバック・ヘビーシャッタートランジション初期化
    DeathTransition::GetInstance()->Initialize(engine_->GetSpriteCom(),
                                               engine_->GetWindowAPI());
    DeathTransition::GetInstance()->SetOffScreenRendering(
        engine_->GetOffScreenRendering());

    // エンジンのデフォルト黒フェードを無効化（即時切り替えにして、演出はアプリケーション層トランジションで完全制御）
    SceneManager::GetInstance()->SetFadeApplication(nullptr);
  }

  SceneManager::GetInstance()->ChangeScene("TITLE");

  // テクスチャ読み込み
  textureIndexUvChecker =
      TextureManager::GetInstance()->Load("Resources/uvChecker.png");
  textureIndexModelTex = TextureManager::GetInstance()->Load(
      modelData.material.textureFilePath);
  textureIndexSkybox_ = TextureManager::GetInstance()->Load(
      "Resources/CG4/dds/CG4_test.dds");
  SceneManager::GetInstance()->SetSkyboxTextureIndex(textureIndexSkybox_);

  // 遅延していたアップロードバッファの解放とGPU同期待ちを一括実行
  TextureManager::GetInstance()->ReleaseUploadBuffers();
}

void Game::Finalize() {
  // 1. デバッグUI終了
  if (debugUI) {
    debugUI.reset();
  }

  // 2. スプライト破棄
  for (auto &sp : sprites) {
    if (sp) {
      try {
        sp->Finalize();
      } catch (...) {
        Logger::Log(logStream, "sprite finalize failed\n");
      }
    }
  }
  sprites.clear();

  // 3. サンプルモデル破棄
  if (model_) {
    model_.reset();
  }
  if (modelCom_) {
    modelCom_.reset();
  }
  if (object3d_) {
    object3d_.reset();
  }

  // 3.5. トランジション破棄
  StingerTransition::GetInstance()->Finalize();
  DeathTransition::GetInstance()->Finalize();

  // 4. エンジンの一括破棄（SceneManager、サブシステム、DirectX12の逆順安全解放）
  // 【カプセル化】エンジン内部で依存関係を解決して安全にシャットダウン
  if (engine_) {
    try {
      engine_->Finalize();
    } catch (...) {
      Logger::Log(logStream, "engine finalize failed\n");
    }
    engine_.reset();
  }

  Framework::Finalize();
}

void Game::Update() {
  Framework::Update();

  // 1. エンジンサブシステムの一括更新（入力、音声、フェード、カメラ、ImGui等）
  // 【カプセル化】個別サブシステムの更新順序や依存関係をエンジン内部に隠蔽
  if (engine_) {
    engine_->Update();
  }

  // 2. シーンマネージャの更新（固定タイムステップ 1/60s）
  SceneManager::GetInstance()->Update(kDeltaTime);

  // 2.5. アプリケーション層トランジション更新
  StingerTransition::GetInstance()->Update(kDeltaTime);
  DeathTransition::GetInstance()->Update(kDeltaTime);
  DeathTransition::GetInstance()->DrawUI();

  // 3. デバッグカメラ・サンプルオブジェクトの更新
  debugCamera_.Update();

  if (object3d_) {
    object3d_->SetRotate(transformObject.rotate);
    object3d_->Update();
  }

#if APP_DEBUG_UI
  if (debugUI) {
    debugUI->Update();
  }
#endif

  // 4. マウス入力によるカーソル位置更新
  if (engine_ && engine_->GetMouseInput()) {
    auto *mouse = engine_->GetMouseInput();
    if (cursorSpriteIndex >= 0 &&
        cursorSpriteIndex < static_cast<int>(sprites.size())) {
      auto *cur = sprites[cursorSpriteIndex].get();
      if (cur) {
        Vector2 pos{static_cast<float>(mouse->GetX()),
                    static_cast<float>(mouse->GetY())};
        if (engine_->GetWindowAPI()) {
          RECT rc{};
          if (GetClientRect(engine_->GetWindowAPI()->GetHwnd(), &rc)) {
            float clientW = float(rc.right - rc.left);
            float clientH = float(rc.bottom - rc.top);
            if (clientW > 0.0f && clientH > 0.0f) {
              float sx =
                  float(engine_->GetWindowAPI()->GetClientWidth()) / clientW;
              float sy =
                  float(engine_->GetWindowAPI()->GetClientHeight()) / clientH;
              pos.x *= sx;
              pos.y *= sy;
            }
          }
        }

        cur->SetPosition(pos);
        cur->Update();
        if (mouse->PushButton(0)) {
          cur->SetColor({1.0f, 0.0f, 0.0f, 1.0f});
        } else {
          cur->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
        }
      }
    }
  }
}

void Game::Draw() {
  if (!engine_)
    return;

  // トランジション中およびタイトルシーン中は深度アウトラインポストプロセスを確実に抑制
  // （タイトルロゴ文字への輪郭線浸食やトランジション帯の輪郭線浮きを完全防止）
  if (engine_->GetOffScreenRendering()) {
    bool isTitle = (SceneManager::GetInstance() && SceneManager::GetInstance()->GetCurrentSceneName() == "TITLE");
    if (StingerTransition::GetInstance()->IsTransitioning() || DeathTransition::GetInstance()->IsTransitioning() || isTitle) {
      if (engine_->GetOffScreenRendering()->GetPostEffect() != OffScreenRendering::PostEffect::Normal) {
        engine_->GetOffScreenRendering()->SetPostEffect(OffScreenRendering::PostEffect::Normal);
      }
    } else {
      if (engine_->GetOffScreenRendering()->GetPostEffect() != OffScreenRendering::PostEffect::DepthBasedOutline) {
        engine_->GetOffScreenRendering()->SetPostEffect(OffScreenRendering::PostEffect::DepthBasedOutline);
      }
    }
  }

  // 1. フレーム描画の開始（バックバッファクリア、オフスクリーンレンダリング開始等）
  engine_->BeginFrame();

  // 2. 3Dシーンおよび2Dスプライト描画の実行
  // エンジン層の ConstantBufferAllocator がスレッドセーフ化されたため、3Dと2Dの安全な並列描画が可能
  engine_->RenderFrame(
      [this](const RenderContext &ctx) {
        // --- 3D描画パス ---
        if (SceneManager::GetInstance()) {
          SceneManager::GetInstance()->DrawSkybox(ctx.commandList);
          SceneRenderRequests renderRequests{};
          SceneManager::GetInstance()->Draw(renderRequests);
          sphereRenderer_.Draw(ctx, renderRequests);

          if (!renderRequests.sceneDrawn && drawObject) {
            if (engine_->GetObject3dCom() && object3d_) {
              engine_->GetObject3dCom()->Draw(object3d_.get(), ctx, modelData,
                                              drawObject);
            }
          }
        }

        DrawParticles(ctx);
      },
      [this](const RenderContext &workerCtx) {
        // --- 2Dスプライト並列描画パス ---
        DrawSprites(workerCtx);
      });

  // 3. フレーム描画の終了（ポストプロセスバッファ描画、フェード、ImGui、画面フリップ）
#if defined(USE_IMGUI) && !APP_DEBUG_UI
  HideEngineDebugWindows();
#endif
  engine_->EndFrame();
}

bool Game::ProcessMessage() {
  if (engine_) {
    return engine_->ProcessMessage();
  }
  return false;
}

bool Game::IsQuitRequested() {
  return ProcessMessage();
}

bool Game::InitializeEngine() {
  engine_ = std::make_unique<EngineContext>();

  if (!engine_->Initialize(logStream, InitConfig{})) {
    Logger::Log(logStream, "EngineContext initialization failed. Check "
                           "previous logs for details.\n");
    return false;
  }

  if (!engine_->GetDirectXCom()) {
    Logger::Log(logStream,
                "Error: EngineContext initialized but DirectXCom is null.\n");
    return false;
  }

  return true;
}

void Game::LogEngineDiagnostics() {
  auto *dx = engine_ ? engine_->GetDirectXCom() : nullptr;

  {
    std::ostringstream oss;
    oss << "Diagnostics: DirectXCom=" << std::hex << (uintptr_t)dx;
    oss << " device=" << (uintptr_t)(dx ? dx->GetDevice().Get() : nullptr);
    oss << " commandList="
        << (uintptr_t)(dx ? dx->GetCommandList().Get() : nullptr) << std::dec
        << "\n";
    Logger::Log(logStream, oss.str());
  }
}

void Game::InitializeModelResources() {
  auto *dx = engine_ ? engine_->GetDirectXCom() : nullptr;
  auto *object3dCom = engine_ ? engine_->GetObject3dCom() : nullptr;

  object3d_ = std::make_unique<Object3d>();
  object3d_->Initialize(object3dCom,
                        object3d_->LoadObjFile("Resources", "plane.obj"));

  // モデル読み込み
  modelData = object3d_->LoadObjFile("Resources", "plane.obj");

  // Model を作成して初期化（Model が自分で頂点リソースを作る）
  modelCom_ = std::make_unique<ModelCom>();
  modelCom_->Initialize(dx);
  model_ = std::make_unique<Model>();
  model_->Initialize(modelCom_.get(), "Resources", "plane.obj");
}

void Game::DrawObjects(const RenderContext &ctx) {
  if (ctx.textureHandle.ptr != 0) {
    ctx.commandList->SetGraphicsRootDescriptorTable(2, ctx.textureHandle);
  }

  if (ctx.light) {
    ctx.commandList->SetGraphicsRootConstantBufferView(
        3, ctx.light->GetDirectionalLightResource()->GetGPUVirtualAddress());
  } else {
    ctx.commandList->SetGraphicsRootConstantBufferView(3, 0);
  }

  if (ctx.camera && ctx.camera->GetCameraGpuAddress() != 0) {
    ctx.commandList->SetGraphicsRootConstantBufferView(
        4, ctx.camera->GetCameraGpuAddress());
  } else {
    Logger::Log(
        logStream,
        "Warning: camera GPU resource not available when drawing object.\n");
    return;
  }

  object3d_->Draw(ctx);

  if (drawObject) {
    ctx.commandList->DrawInstanced(UINT(modelData.vertices.size()), 1, 0, 0);
  }
}

void Game::DrawSprites(const RenderContext &ctx) {
  if (drawSprite) {
    SpriteManager *sm = engine_ ? engine_->GetSpriteManager() : nullptr;
    if (sm) {
      sm->DrawAll(ctx, &debugCamera_, &sprites);
    }
  }

  // アプリケーション層トランジション描画（最前面にトランジションを重ねる）
  StingerTransition::GetInstance()->Draw(ctx);
  DeathTransition::GetInstance()->Draw(ctx);
}

void Game::DrawParticles(const RenderContext &ctx) {
  auto *particleManager = engine_ ? engine_->GetParticleManager() : nullptr;
  particleRenderer_.Draw(ctx, particleManager, model_.get(),
                         UINT(modelData.vertices.size()));
}

RenderContext Game::PrepareRenderContext() {
  RenderContext ctx = engine_ ? engine_->GetRenderContext() : RenderContext{};

  uint32_t chosenIndex =
      useMonsterBall ? textureIndexModelTex : textureIndexUvChecker;
  if (chosenIndex != TextureManager::kInvalidTextureIndex) {
    ctx.textureHandle =
        TextureManager::GetInstance()->GetSrvHandleGPU(chosenIndex);
  } else {
    Logger::Log(logStream, "Warning: invalid texture index when preparing "
                           "render context for drawing.\n");
    ctx.textureHandle = {};
  }

  auto *materialManager = engine_ ? engine_->GetMaterialManager() : nullptr;
  ctx.materialGPUAddress =
      (materialManager && materialManager->GetMaterialResource())
          ? materialManager->GetMaterialResource()->GetGPUVirtualAddress()
          : 0;

  return ctx;
}
