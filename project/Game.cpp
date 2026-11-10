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
#include "Application/Audio/TacticalAudio.h"
#include "Application/Transition/StingerTransition.h"
#include "Application/Transition/DeathTransition.h"
#include "Application/UI/TacticalUICanvas.h"
#include "Baziru3_Engine/Graphics/Graphics/GpuProfiler.h"
#include "SkyBox.h"

#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#include <imgui_internal.h>
#endif

#ifdef USE_IMGUI
namespace {
// エンジン側が無条件で出す開発用ウィンドウ。
// ImGuiオーバーレイ非表示時は描画直前に隠して、画面にも入力にも一切干渉しないようにする。
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

  // 全体のBGM・SE音量を耳に優しい適切なマスター音量へ設定
  if (engine_ && engine_->GetAudioManager()) {
    engine_->GetAudioManager()->SetMasterVolume(0.35f);
  }

  LogEngineDiagnostics();

#if defined(USE_IMGUI) && !APP_DEBUG_UI
  // Release (SHIPPING_BUILD) では ImGui を完全非表示・無効化し、imgui.ini の読み書きも行わない
  if (ImGui::GetCurrentContext()) {
    ImGui::GetIO().IniFilename = nullptr;
  }
#endif

  auto *spriteCom = engine_->GetSpriteCom();
  auto *window = engine_->GetWindowAPI();
  auto *camera = engine_->GetCamera();

  showDeveloperImGui_ = false;
  prevToggleImGuiKey_ = false;
  AppDebugState::SetDeveloperImGuiVisible(false);
  AppDebugState::SetImGuiCapturingMouse(false);
  AppDebugState::SetImGuiCapturingTextInput(false);
#if APP_DEBUG_UI
  (void)GetAsyncKeyState(VK_F12);
#endif

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

#if APP_DEBUG_UI
  // デバッグUIの初期化（Debug / Development のみ）
  SpriteManager *uiSpriteManager = engine_->GetSpriteManager();
  debugUI = std::make_unique<DebugUI>(
      engine_->GetMaterialManager(), uiSpriteManager, camera, &transformObject,
      &useMonsterBall, &drawObject, &drawSprite);
  debugUI->SetOffScreenRendering(engine_->GetOffScreenRendering());
  debugUI->SetLight(engine_->GetLight());
  debugUI->Initialize();
#else
  (void)camera;
#endif

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

    // DirectX 12 ネイティブ2D戦術UI描画基盤の初期化（ImGui非依存）
    TacticalUICanvas::GetInstance()->Initialize(engine_->GetDirectXCom(),
                                                engine_->GetWindowAPI());
  }

  SceneManager::GetInstance()->ChangeScene("TITLE");

  // テクスチャ読み込み
  textureIndexUvChecker =
      TextureManager::GetInstance()->Load("Resources/uvChecker.png");
  textureIndexModelTex = TextureManager::GetInstance()->Load(
      modelData.material.textureFilePath);
  textureIndexSkybox_ = TextureManager::GetInstance()->Load(
      "Resources/duckov_skybox.dds");
  if (textureIndexSkybox_ == TextureManager::kInvalidTextureIndex) {
    textureIndexSkybox_ = TextureManager::GetInstance()->Load(
        "Resources/CG4/dds/CG4_test.dds");
  }
  SceneManager::GetInstance()->SetSkyboxTextureIndex(textureIndexSkybox_);

  // アプリケーション層・リアルタイムアニメーション雲システムの初期化
  skyCloudSystem_ = std::make_unique<SkyCloudSystem>();
  skyCloudSystem_->Initialize(engine_->GetDirectXCom());

  // 遅延していたアップロードバッファの解放とGPU同期待ちを一括実行
  TextureManager::GetInstance()->ReleaseUploadBuffers();
}

void Game::Finalize() {
  AppDebugState::SetDeveloperImGuiVisible(false);
  AppDebugState::SetImGuiCapturingMouse(false);
  AppDebugState::SetImGuiCapturingTextInput(false);

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

  // 3.5. トランジション・アニメーション雲・ネイティブUIキャンバスの破棄
  if (skyCloudSystem_) {
    skyCloudSystem_->Finalize();
    skyCloudSystem_.reset();
  }
  TacticalUICanvas::GetInstance()->Finalize();
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

#if defined(USE_IMGUI) && APP_DEBUG_UI
  {
    const bool nowFocused = IsAppWindowFocused();
    const bool ctrlHeld = IsAppKeyDown(VK_CONTROL);
    const bool altHeld = IsAppKeyDown(VK_MENU);

    // F2 / F9 / Ctrl+F12: Visual Studio (F5) デバッグ中も OS 強制ブレークを一切起こさず即座に開閉
    // F12: Ctrl+F5（デバッグなしで開始）や exe 直接起動時にそのまま開閉可能
    const int asyncF12State = GetAsyncKeyState(VK_F12);
    const bool asyncF12Edge = nowFocused && !altHeld && ((asyncF12State & 0x0001) != 0);

    const bool curF2 = (!ctrlHeld && !altHeld) && IsAppKeyDown(VK_F2);
    const bool curF9 = (!ctrlHeld && !altHeld) && IsAppKeyDown(VK_F9);
    const bool curF12 = (!altHeld) && IsAppKeyDown(VK_F12);
    const bool curToggleKey = curF2 || curF9 || curF12;

    if ((curToggleKey && !prevToggleImGuiKey_) || (!prevToggleImGuiKey_ && asyncF12Edge)) {
      showDeveloperImGui_ = !showDeveloperImGui_;
      AppDebugState::SetDeveloperImGuiVisible(showDeveloperImGui_);
      TacticalAudio::Play2D(showDeveloperImGui_
                                ? "Resources/Audio/SE_UI_Map_Open.wav"
                                : "Resources/Audio/SE_UI_Map_Close.wav",
                            0.42f, 80);
    }
    prevToggleImGuiKey_ = curToggleKey;
  }

  if (ImGui::GetCurrentContext()) {
    ImGuiIO &io = ImGui::GetIO();
    // GamePlayScene 中は ShowCursor(FALSE) で OS カーソルが非表示になるため、
    // ImGui 表示中はソフトウェアカーソルを最前面に描画して確実なマウス操作を保証する
    io.MouseDrawCursor = showDeveloperImGui_;
    if (!showDeveloperImGui_) {
      io.WantCaptureMouse = false;
      io.WantCaptureKeyboard = false;
      io.WantTextInput = false;
    }
    AppDebugState::SetImGuiCapturingMouse(showDeveloperImGui_ && io.WantCaptureMouse);
    AppDebugState::SetImGuiCapturingTextInput(showDeveloperImGui_ && io.WantTextInput);
  } else {
    AppDebugState::SetImGuiCapturingMouse(false);
    AppDebugState::SetImGuiCapturingTextInput(false);
  }
#elif defined(USE_IMGUI)
  // Release (SHIPPING_BUILD) では F12 を含め ImGui の開閉を一切受け付けず常時非表示に固定
  showDeveloperImGui_ = false;
  if (ImGui::GetCurrentContext()) {
    ImGuiIO &io = ImGui::GetIO();
    io.MouseDrawCursor = false;
    io.WantCaptureMouse = false;
    io.WantCaptureKeyboard = false;
    io.WantTextInput = false;
  }
#endif

  // ネイティブ2D戦術UIキャンバスのフレーム開始（各Scene::Update / Draw内のUI記録を収集）
  TacticalUICanvas::GetInstance()->BeginFrame();

  // 2. シーンマネージャの更新（固定タイムステップ 1/60s）
  SceneManager::GetInstance()->Update(kDeltaTime);
  if (skyCloudSystem_) {
    skyCloudSystem_->Update(kDeltaTime);
  }

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

#if defined(USE_IMGUI) && APP_DEBUG_UI
  if (debugUI && showDeveloperImGui_) {
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
  // （開発者ImGuiが開いている間は、Settingsパネルからのポストエフェクト切替プレビューをそのまま許可する）
  if (engine_->GetOffScreenRendering()) {
    bool isTitle = (SceneManager::GetInstance() && SceneManager::GetInstance()->GetCurrentSceneName() == "TITLE");
    if (StingerTransition::GetInstance()->IsTransitioning() || DeathTransition::GetInstance()->IsTransitioning() || isTitle) {
      if (engine_->GetOffScreenRendering()->GetPostEffect() != OffScreenRendering::PostEffect::Normal) {
        engine_->GetOffScreenRendering()->SetPostEffect(OffScreenRendering::PostEffect::Normal);
      }
    } else if (!showDeveloperImGui_) {
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
          if (auto *skybox = SceneManager::GetInstance()->GetSkyBox()) {
            skybox->Update();
          }
          SceneManager::GetInstance()->DrawSkybox(ctx.commandList);
          if (SceneManager::GetInstance()->GetShowSkybox() && skyCloudSystem_) {
            skyCloudSystem_->Draw(ctx.commandList,
                                  SceneManager::GetInstance()->GetCamera());
          }
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

  // 3. フレーム描画の終了（オフスクリーン合成 → ネイティブDirectX12戦術UI → 画面フリップ）
  auto *dx = engine_->GetDirectXCom();
  if (dx) {
    auto *cmdList = dx->GetCommandList().Get();
    auto *offScreen = engine_->GetOffScreenRendering();
    if (offScreen) {
      offScreen->End(cmdList);
      offScreen->SetMainRenderTarget(cmdList);
      if (engine_->GetCamera()) {
        offScreen->SetProjectionInverse(
            Inverse(engine_->GetCamera()->GetProjectionMatrix()));
      }
      GpuProfiler::GetInstance()->BeginProfile(cmdList, "PostProcess Draw");
      offScreen->DrawToBackBuffer(cmdList);
      GpuProfiler::GetInstance()->EndProfile(cmdList, "PostProcess Draw");
    }

    // バックバッファへ直接 DirectX 12 ネイティブ戦術UI（TacticalUICanvas）を描画
    const UINT backBufferIdx = dx->GetSwapChain()->GetCurrentBackBufferIndex();
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = dx->GetRtvHandle(backBufferIdx);
    cmdList->OMSetRenderTargets(1, &rtvHandle, false, nullptr);
    TacticalUICanvas::GetInstance()->RenderAll(cmdList);

    if (auto *fade = engine_->GetFade()) {
      D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle =
          dx->GetDsvHeap().GetCPUDescriptorHandle(0);
      cmdList->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);
      fade->Draw();
    }

#ifdef USE_IMGUI
    if (!showDeveloperImGui_) {
      HideEngineDebugWindows();
    }
    if (auto *imguiMgr = engine_->GetImGuiManager()) {
      imguiMgr->Render();
      ImDrawData *drawData = ImGui::GetDrawData();
      // ImGui 1.92+ (IMGUI_HAS_TEXTURES) は非表示フレームでもフォントアトラスの
      // 生成・更新・破棄リクエスト (ImDrawData::Textures) を毎フレーム消化しておかないと、
      // 表示切替フレームで未アップロードの旧テクスチャ参照 (TexID == 0) アサートが発生する。
      if (drawData && drawData->Textures != nullptr) {
        for (ImTextureData *tex : *drawData->Textures) {
          if (tex && tex->Status != ImTextureStatus_OK) {
            ImGui_ImplDX12_UpdateTexture(tex);
          }
        }
      }
#if APP_DEBUG_UI
      if (showDeveloperImGui_ && drawData) {
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle =
            dx->GetDsvHeap().GetCPUDescriptorHandle(0);
        cmdList->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);
        cmdList->RSSetViewports(1, &dx->GetViewport());
        cmdList->RSSetScissorRects(1, &dx->GetScissorRect());
        ID3D12DescriptorHeap *heaps[] = {dx->GetSrvDescriptorHeap().Get()};
        cmdList->SetDescriptorHeaps(1, heaps);
        ImGui_ImplDX12_RenderDrawData(drawData, cmdList);
      }
#endif
    }
#endif

    dx->PostDraw();
  }
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
