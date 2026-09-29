#include "EngineContext.h"

#include <future>
#include <iostream>
#include <sstream>

#include "DirectXCom.h"
#include "Log.h"
#include "SpriteCom.h"
#include "SpriteManager.h"
#include "WindowsAPI.h"

#include "AudioManager.h"
#include "Baziru3_Engine/Core/IO/Mouse/MouseInput.h"
#include "Camera.h"
#include "EngineAssert.h"
#include "Fade.h"
#include "ImGuiManager.h"
#include "KeyInput.h"
#include "Light.h"
#include "OffScreenRendering.h"
#include "ParticleManager.h"
#include "SceneManager.h"
#include "SkyBox.h"
#include "SkyboxCom.h"
#include "GpuProfiler.h"
#include "Baziru3_Engine/Core/Base/Pipeline/PipelineStateManager.h"

#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#endif

#if defined(_DEBUG)
#include <crtdbg.h>
#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif

EngineContext::~EngineContext() { Finalize(); }

bool EngineContext::Initialize(std::ostream &log, const InitConfig &cfg) {
  logStream_ = &log;
  cfg_ = cfg;
  auto res = SubsystemFactory::InitializeAll(*logStream_, cfg_);
  if (!res.success) {
    Logger::Log(*logStream_,
                std::string("EngineContext: InitializeAll failed: ") +
                    res.errorMessage + "\n");
    SubsystemFactory::FinalizeAll(res, *logStream_);
    return false;
  }
  res_ = std::move(res);

  // 各マネージャーの生成と一括初期化
  audioManager_ = std::make_unique<AudioManager>(*logStream_);
  audioManager_->Initialize();

  keyInput_ = std::make_unique<KeyInput>();
  keyInput_->Initialize(res_.windowAPI.get());

  mouseInput_ = std::make_unique<MouseInput>();
  mouseInput_->Initialize(res_.windowAPI.get());

  offScreenRendering_ =
      std::make_unique<OffScreenRendering>(*logStream_, res_.directXCom.get());
  offScreenRendering_->Initialize();

  fade_ = std::make_unique<Fade>();
  fade_->Initialize(res_.spriteCom.get(), res_.windowAPI.get());

  camera_ = std::make_unique<Camera>();
  camera_->Initialize(res_.directXCom.get());
  camera_->SetTranslate({0.0f, 20.0f, -20.0f});
  camera_->SetRotate({0.785f, 0.0f, 0.0f});

  light_ = std::make_unique<Light>();
  light_->Initialize(res_.directXCom.get());

  skyboxCom_ = std::make_unique<SkyboxCom>(*logStream_, res_.directXCom.get());
  skyboxCom_->Initialize();

  skybox_ = std::make_unique<SkyBox>();
  skybox_->Initialize(res_.directXCom.get(), camera_.get());

  particleManager_ =
      std::make_unique<ParticleManager>(*logStream_, res_.directXCom.get());
  particleManager_->Initialize(camera_.get());

  // 3Dモデル描画パイプライン基盤の初期化（Object3dCom, SkinningObject3dCom, MaterialManager）
  object3dCom_ = std::make_unique<Object3dCom>(*logStream_);
  object3dCom_->Initialize(res_.directXCom.get());
  object3dCom_->SetDefaultCamera(camera_.get());

  skinningObject3dCom_ = std::make_unique<SkinningObject3dCom>(*logStream_);
  skinningObject3dCom_->Initialize(res_.directXCom.get());

  materialManager_ = std::make_unique<MaterialManager>();
  materialManager_->Initialize(res_.directXCom.get());

  // ImGuiManager の初期化（デバッグUI用）
#ifdef USE_IMGUI
  if (cfg_.enableImGui) {
    imguiManager_ = std::make_unique<ImGuiManager>();
    imguiManager_->Initialize(res_.windowAPI.get(), res_.directXCom.get());
  }
#endif

  // SceneManager への参照設定（エンジン内部ですべて解決し、アプリケーション側の手動設定を不要にする）
  SceneManager::GetInstance()->Initialize(res_.directXCom.get());
  SceneManager::GetInstance()->SetFadeApplication(fade_.get());
  SceneManager::GetInstance()->SetAudioManager(audioManager_.get());
  SceneManager::GetInstance()->SetParticleManager(particleManager_.get());
  SceneManager::GetInstance()->SetCamera(camera_.get());
  SceneManager::GetInstance()->SetLight(light_.get());
  SceneManager::GetInstance()->SetSkyboxCom(skyboxCom_.get());
  SceneManager::GetInstance()->SetSkyBox(skybox_.get());
  SceneManager::GetInstance()->SetObject3dCom(object3dCom_.get());
  SceneManager::GetInstance()->SetSkinningObject3dCom(skinningObject3dCom_.get());
  SceneManager::GetInstance()->SetMaterialManager(materialManager_.get());

  std::ostringstream oss;
  oss << "EngineContext: Initialized subsystems\n";
  oss << "  directXCom=0x" << std::hex << (uintptr_t)(res_.directXCom.get())
      << std::dec << "\n";
  oss << "  windowAPI=0x" << std::hex << (uintptr_t)(res_.windowAPI.get())
      << std::dec << "\n";
  oss << "  spriteCom=0x" << std::hex << (uintptr_t)(res_.spriteCom.get())
      << std::dec << "\n";
  oss << "  spriteManager=0x" << std::hex
      << (uintptr_t)(res_.spriteManager.get()) << std::dec << "\n";
  Logger::Log(*logStream_, oss.str());

  return true;
}

void EngineContext::Finalize() {
  if (finalized_) {
    return;
  }
  finalized_ = true;

  if (logStream_) {
    Logger::Log(*logStream_, "EngineContext: Finalizing subsystems\n");
  }

  // 他のサブシステム（パーティクルやカメラなど）が破棄される前に、SceneManager を安全に破棄する
  SceneManager::Destroy();

  // サブシステムの一括破棄（生成と逆順で安全に解放）
#ifdef USE_IMGUI
  if (imguiManager_) {
    try {
      imguiManager_->Finalize();
    } catch (...) {}
    imguiManager_.reset();
  }
#endif

  if (materialManager_) {
    materialManager_->Finalize();
    materialManager_.reset();
  }
  if (skinningObject3dCom_) {
    skinningObject3dCom_.reset();
  }
  if (object3dCom_) {
    object3dCom_.reset();
  }
  if (particleManager_) {
    particleManager_->Finalize();
    particleManager_.reset();
  }
  if (skybox_) {
    skybox_.reset();
  }
  if (skyboxCom_) {
    skyboxCom_.reset();
  }
  if (light_) {
    light_.reset();
  }
  if (camera_) {
    camera_->Finalize();
    camera_.reset();
  }
  if (fade_) {
    fade_->Finalize();
    fade_.reset();
  }
  if (offScreenRendering_) {
    offScreenRendering_->Finalize();
    offScreenRendering_.reset();
  }
  if (mouseInput_) {
    mouseInput_.reset();
  }
  if (keyInput_) {
    keyInput_.reset();
  }
  if (audioManager_) {
    audioManager_->Finalize();
    audioManager_.reset();
  }

  if (SceneManager::GetInstance()) {
    SceneManager::GetInstance()->SetFadeApplication(nullptr);
    SceneManager::GetInstance()->SetAudioManager(nullptr);
    SceneManager::GetInstance()->SetParticleManager(nullptr);
    SceneManager::GetInstance()->SetCamera(nullptr);
    SceneManager::GetInstance()->SetLight(nullptr);
    SceneManager::GetInstance()->SetSkyboxCom(nullptr);
    SceneManager::GetInstance()->SetSkyBox(nullptr);
    SceneManager::GetInstance()->SetObject3dCom(nullptr);
    SceneManager::GetInstance()->SetSkinningObject3dCom(nullptr);
    SceneManager::GetInstance()->SetMaterialManager(nullptr);
  }

  SubsystemFactory::FinalizeAll(res_, logStream_ ? *logStream_ : std::cout);

  res_ = {};
  logStream_ = nullptr;
}

bool EngineContext::ProcessMessage() {
  if (res_.windowAPI) {
    return res_.windowAPI->ProcessMessage();
  }
  return false;
}

void EngineContext::Update() {
  // パイプラインステートのホットリロード・更新
  if (res_.directXCom) {
    PipelineStateManager::GetInstance()->Update(res_.directXCom.get());
  }

  // 入力・音声・フェード・カメラ・ImGuiの順次一括更新
  if (keyInput_)
    keyInput_->Update();
  if (mouseInput_)
    mouseInput_->Update();
  if (audioManager_)
    audioManager_->Update();
  if (fade_)
    fade_->Update();
  if (camera_)
    camera_->Update();
  if (skybox_)
    skybox_->Update();
#ifdef USE_IMGUI
  if (imguiManager_)
    imguiManager_->Update();
#endif
}

void EngineContext::BeginFrame() {
  if (!res_.directXCom)
    return;

  // 1. DirectX 12 バックバッファのクリアと描画前処理
  res_.directXCom->PreDraw();

  // 2. オフスクリーンレンダリングの開始
  if (offScreenRendering_) {
    offScreenRendering_->Begin(res_.directXCom->GetCommandList().Get());
  }

  // 3. Object3D描画前処理
  if (object3dCom_) {
    object3dCom_->PreDraw();
  }
}

void EngineContext::RenderFrame(
    std::function<void(const RenderContext &)> scene3dCallback,
    std::function<void(const RenderContext &)> sprite2dCallback) {
  if (!res_.directXCom)
    return;
  auto *dx = res_.directXCom.get();

  RenderContext ctx = GetRenderContext();

  // カメラ定数バッファのバインド（ルートパラメータ4番）
  if (camera_ && camera_->GetCameraGpuAddress() != 0) {
    dx->GetCommandList()->SetGraphicsRootConstantBufferView(
        4, camera_->GetCameraGpuAddress());
  }

  // スプライトの更新処理を一括でメインスレッドで行う
  if (res_.spriteManager) {
    res_.spriteManager->Update();
  }

  // === [サブスレッド] 2Dスプライト描画コマンドの並列記録 ===
  std::future<void> spriteFuture;
  if (sprite2dCallback) {
    RenderContext workerCtx = ctx;
    workerCtx.commandList = dx->GetWorkerCommandList().Get();

    spriteFuture = std::async(std::launch::async, [this, workerCtx, dx, sprite2dCallback]() {
      dx->GetWorkerCommandAllocator()->Reset();
      dx->GetWorkerCommandList()->Reset(
          dx->GetWorkerCommandAllocator().Get(), nullptr);

      D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle{};
      D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle{};
      if (offScreenRendering_) {
        dsvHandle = offScreenRendering_->GetDsvHandle();
        rtvHandle = offScreenRendering_->GetRtvHandle();
      } else {
        UINT backBufferIndex = dx->GetSwapChain()->GetCurrentBackBufferIndex();
        dsvHandle = dx->GetDsvHeap().GetCPUDescriptorHandle(0);
        rtvHandle = dx->GetRtvHandle(backBufferIndex);
      }
      workerCtx.commandList->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);

      ID3D12DescriptorHeap *descriptorHeaps[] = {
          dx->GetSrvDescriptorHeap().Get()};
      workerCtx.commandList->SetDescriptorHeaps(1, descriptorHeaps);
      workerCtx.commandList->RSSetViewports(1, &dx->GetViewport());
      workerCtx.commandList->RSSetScissorRects(1, &dx->GetScissorRect());

      GpuProfiler::GetInstance()->BeginProfile(workerCtx.commandList, "Sprite Draw");
      sprite2dCallback(workerCtx);
      GpuProfiler::GetInstance()->EndProfile(workerCtx.commandList, "Sprite Draw");

      dx->GetWorkerCommandList()->Close();
    });
  }

  // === [メインスレッド] 3Dオブジェクト等の描画コマンド記録 ===
  GpuProfiler::GetInstance()->BeginProfile(dx->GetCommandList().Get(), "Scene Draw");
  if (scene3dCallback) {
    scene3dCallback(ctx);
  }
  GpuProfiler::GetInstance()->EndProfile(dx->GetCommandList().Get(), "Scene Draw");

  // 前半のメインコマンドリスト記録を終了し、GPUに提出（オフスクリーン3D描画確定）
  dx->GetCommandList()->Close();
  ID3D12CommandList *mainLists1[] = {dx->GetCommandList().Get()};
  dx->GetCommandQueue()->ExecuteCommandLists(1, mainLists1);

  // [サブスレッド] Sprite 描画コマンド記録完了を同期的に待機し、提出
  if (spriteFuture.valid()) {
    spriteFuture.get();
    ID3D12CommandList *workerLists[] = {dx->GetWorkerCommandList().Get()};
    dx->GetCommandQueue()->ExecuteCommandLists(1, workerLists);
  }

  // 後半のコマンド記録（ポストプロセス以降）の開始
  dx->GetCommandList()->Reset(dx->GetCommandAllocator().Get(), nullptr);
  ID3D12DescriptorHeap *descriptorHeaps[] = {dx->GetSrvDescriptorHeap().Get()};
  dx->GetCommandList()->SetDescriptorHeaps(1, descriptorHeaps);
  dx->GetCommandList()->RSSetViewports(1, &dx->GetViewport());
  dx->GetCommandList()->RSSetScissorRects(1, &dx->GetScissorRect());
}

void EngineContext::EndFrame() {
  if (!res_.directXCom)
    return;
  auto *dx = res_.directXCom.get();

  // オフスクリーンレンダリングの終了とバックバッファへのポストプロセス描画
  if (offScreenRendering_) {
    offScreenRendering_->End(dx->GetCommandList().Get());
    offScreenRendering_->SetMainRenderTarget(dx->GetCommandList().Get());

    if (camera_) {
      offScreenRendering_->SetProjectionInverse(
          Inverse(camera_->GetProjectionMatrix()));
    }

    GpuProfiler::GetInstance()->BeginProfile(dx->GetCommandList().Get(), "PostProcess Draw");
    offScreenRendering_->DrawToBackBuffer(dx->GetCommandList().Get());
    GpuProfiler::GetInstance()->EndProfile(dx->GetCommandList().Get(), "PostProcess Draw");
  }

  // シーン遷移フェード描画
  if (fade_) {
    fade_->Draw();
  }

  // ImGui描画
#ifdef USE_IMGUI
  if (imguiManager_) {
    imguiManager_->Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dx->GetCommandList().Get());
  }
#endif

  // スワップチェーンのフリップ（画面表示）
  dx->PostDraw();
}

void EngineContext::EndFrame(
    std::function<void(const RenderContext &)> spriteDrawCallback) {
  RenderFrame(nullptr, spriteDrawCallback);
  EndFrame();
}

RenderContext EngineContext::GetRenderContext() const {
  RenderContext ctx{};
  ctx.commandList =
      res_.directXCom ? res_.directXCom->GetCommandList().Get() : nullptr;
  ctx.windowAPI = res_.windowAPI.get();
  ctx.camera = camera_.get();
  ctx.light = light_.get();
  return ctx;
}
