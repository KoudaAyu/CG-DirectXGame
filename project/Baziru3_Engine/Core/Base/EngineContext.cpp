#include "EngineContext.h"

#include <iostream>
#include <sstream>
#include <future>

#include "DirectXCom.h"
#include "Log.h"
#include "SpriteCom.h"
#include "SpriteManager.h"
#include "WindowsAPI.h"

#include "AudioManager.h"
#include "KeyInput.h"
#include "Baziru3_Engine/Core/IO/Mouse/MouseInput.h"
#include "OffScreenRendering.h"
#include "Fade.h"
#include "ParticleManager.h"
#include "ImGuiManager.h"
#include "Light.h"
#include "Camera.h"
#include "SkyboxCom.h"
#include "SkyBox.h"
#include "SceneManager.h"
#include "EngineAssert.h"

#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>
#endif

#if defined(_DEBUG)
#include <crtdbg.h>
#define new new(_NORMAL_BLOCK, __FILE__, __LINE__)
#endif

EngineContext::~EngineContext()
{
    Finalize();
}

bool EngineContext::Initialize(std::ostream& log, const InitConfig& cfg)
{
    logStream_ = &log;
    cfg_ = cfg;
    auto res = SubsystemFactory::InitializeAll(*logStream_, cfg_);
    if (!res.success)
    {
        Logger::Log(*logStream_, std::string("EngineContext: InitializeAll failed: ") + res.errorMessage + "\n");
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

    offScreenRendering_ = std::make_unique<OffScreenRendering>(*logStream_, res_.directXCom.get());
    offScreenRendering_->Initialize();

    fade_ = std::make_unique<Fade>();
    fade_->Initialize(res_.spriteCom.get(), res_.windowAPI.get());

    camera_ = std::make_unique<Camera>();
    camera_->Initialize(res_.directXCom.get());
    camera_->SetTranslate({ 0.0f, 20.0f, -20.0f });
    camera_->SetRotate({ 0.785f, 0.0f, 0.0f });

    light_ = std::make_unique<Light>();
    light_->Initialize(res_.directXCom.get());

    skyboxCom_ = std::make_unique<SkyboxCom>(*logStream_, res_.directXCom.get());
    skyboxCom_->Initialize();

    skybox_ = std::make_unique<SkyBox>();
    skybox_->Initialize(res_.directXCom.get(), camera_.get());

    particleManager_ = std::make_unique<ParticleManager>(*logStream_, res_.directXCom.get());
    particleManager_->Initialize(camera_.get());

    // ImGuiManager の初期化はアプリケーション層 (Game.cpp) で行われるためここでは行わない

    // SceneManager への参照設定
    SceneManager::GetInstance()->SetFadeApplication(fade_.get());
    SceneManager::GetInstance()->SetAudioManager(audioManager_.get());
    SceneManager::GetInstance()->SetParticleManager(particleManager_.get());
    SceneManager::GetInstance()->SetCamera(camera_.get());
    SceneManager::GetInstance()->SetLight(light_.get());
    SceneManager::GetInstance()->SetSkyboxCom(skyboxCom_.get());
    SceneManager::GetInstance()->SetSkyBox(skybox_.get());

    std::ostringstream oss;
    oss << "EngineContext: Initialized subsystems\n";
    oss << "  directXCom=0x" << std::hex << (uintptr_t)(res_.directXCom.get()) << std::dec << "\n";
    oss << "  windowAPI=0x" << std::hex << (uintptr_t)(res_.windowAPI.get()) << std::dec << "\n";
    oss << "  spriteCom=0x" << std::hex << (uintptr_t)(res_.spriteCom.get()) << std::dec << "\n";
    oss << "  spriteManager=0x" << std::hex << (uintptr_t)(res_.spriteManager.get()) << std::dec << "\n";
    Logger::Log(*logStream_, oss.str());

    return true;
}

void EngineContext::Finalize()
{
    if(finalized_)
    {
        return;
    }
    finalized_ = true;

    if (logStream_)
    {
        Logger::Log(*logStream_, "EngineContext: Finalizing subsystems\n");
    }

    // 他のサブシステム（パーティクルやカメラなど）が破棄される前に、SceneManager を安全に破棄する
    SceneManager::Destroy();

    // サブシステムの一括破棄 (imguiManager_ は使用しないため破棄もしない)
    if (particleManager_) { particleManager_->Finalize(); particleManager_.reset(); }
    if (skybox_) { skybox_.reset(); }
    if (skyboxCom_) { skyboxCom_.reset(); }
    if (light_) { light_.reset(); }
    if (camera_) { camera_->Finalize(); camera_.reset(); }
    if (fade_) { fade_->Finalize(); fade_.reset(); }
    if (offScreenRendering_) { offScreenRendering_->Finalize(); offScreenRendering_.reset(); }
    if (mouseInput_) { mouseInput_.reset(); }
    if (keyInput_) { keyInput_.reset(); }
    if (audioManager_) { audioManager_->Finalize(); audioManager_.reset(); }

    if (SceneManager::GetInstance())
    {
        SceneManager::GetInstance()->SetFadeApplication(nullptr);
        SceneManager::GetInstance()->SetAudioManager(nullptr);
        SceneManager::GetInstance()->SetParticleManager(nullptr);
        SceneManager::GetInstance()->SetCamera(nullptr);
        SceneManager::GetInstance()->SetLight(nullptr);
        SceneManager::GetInstance()->SetSkyboxCom(nullptr);
        SceneManager::GetInstance()->SetSkyBox(nullptr);
    }

    SubsystemFactory::FinalizeAll(res_, logStream_ ? *logStream_ : std::cout);

    res_ = {};
	logStream_ = nullptr;
}

void EngineContext::BeginFrame()
{
    if (!res_.directXCom) return;

    res_.directXCom->PreDraw();

    if (offScreenRendering_)
    {
        offScreenRendering_->Begin(res_.directXCom->GetCommandList().Get());
    }

    if (keyInput_) keyInput_->Update();
    if (mouseInput_) mouseInput_->Update();
    if (audioManager_) audioManager_->Update();
    if (fade_) fade_->Update();
    if (camera_) camera_->Update();
    if (skybox_) skybox_->Update();

    if (imguiManager_) imguiManager_->Update();
}

void EngineContext::EndFrame(std::function<void(const RenderContext&)> spriteDrawCallback)
{
    if (!res_.directXCom) return;
    auto* dx = res_.directXCom.get();

    dx->GetCommandList()->Close();
    ID3D12CommandList* mainLists1[] = { dx->GetCommandList().Get() };
    dx->GetCommandQueue()->ExecuteCommandLists(1, mainLists1);

    if (spriteDrawCallback)
    {
        RenderContext workerCtx = GetRenderContext();
        workerCtx.commandList = dx->GetWorkerCommandList().Get();

        auto spriteFuture = std::async(std::launch::async, [dx, workerCtx, spriteDrawCallback]() {
            dx->GetWorkerCommandAllocator()->Reset();
            dx->GetWorkerCommandList()->Reset(dx->GetWorkerCommandAllocator().Get(), nullptr);

            ID3D12DescriptorHeap* descriptorHeaps[] = { dx->GetSrvDescriptorHeap().Get() };
            workerCtx.commandList->SetDescriptorHeaps(1, descriptorHeaps);
            workerCtx.commandList->RSSetViewports(1, &dx->GetViewport());
            workerCtx.commandList->RSSetScissorRects(1, &dx->GetScissorRect());

            spriteDrawCallback(workerCtx);

            dx->GetWorkerCommandList()->Close();
        });

        spriteFuture.get();
        ID3D12CommandList* workerLists[] = { dx->GetWorkerCommandList().Get() };
        dx->GetCommandQueue()->ExecuteCommandLists(1, workerLists);
    }

    dx->GetCommandList()->Reset(dx->GetCommandAllocator().Get(), nullptr);
    ID3D12DescriptorHeap* descriptorHeaps[] = { dx->GetSrvDescriptorHeap().Get() };
    dx->GetCommandList()->SetDescriptorHeaps(1, descriptorHeaps);
    dx->GetCommandList()->RSSetViewports(1, &dx->GetViewport());
    dx->GetCommandList()->RSSetScissorRects(1, &dx->GetScissorRect());

    if (offScreenRendering_)
    {
        offScreenRendering_->End(dx->GetCommandList().Get());
        offScreenRendering_->SetMainRenderTarget(dx->GetCommandList().Get());
        if (camera_)
        {
            offScreenRendering_->SetProjectionInverse(Inverse(camera_->GetProjectionMatrix()));
        }
        offScreenRendering_->DrawToBackBuffer(dx->GetCommandList().Get());
    }

    if (fade_)
    {
        fade_->Draw();
    }

    if (imguiManager_) imguiManager_->Render();
#ifdef USE_IMGUI
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dx->GetCommandList().Get());
#endif

    dx->PostDraw();
}

RenderContext EngineContext::GetRenderContext() const
{
    RenderContext ctx{};
    ctx.commandList = res_.directXCom ? res_.directXCom->GetCommandList().Get() : nullptr;
    ctx.windowAPI = res_.windowAPI.get();
    ctx.camera = camera_.get();
    ctx.light = light_.get();
    return ctx;
}
