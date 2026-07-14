#pragma once

#include "SubsystemFactory.h"
#include "RenderContext.h"
#include <ostream>
#include <sstream>
#include <memory>
#include <functional>

#include "AudioManager.h"
#include "KeyInput.h"
#include "Baziru3_Engine/IO/Mouse/MouseInput.h"
#include "OffScreenRendering.h"
#include "Fade.h"
#include "ParticleManager.h"
#include "ImGuiManager.h"
#include "Light.h"
#include "Camera.h"
#include "SkyboxCom.h"
#include "SkyBox.h"

class EngineContext
{
public:

    ~EngineContext();
 
    bool Initialize(std::ostream& log, const InitConfig& cfg);
    void Finalize();
  
    DirectXCom* GetDirectXCom() const { return res_.directXCom.get(); }
    SpriteCom* GetSpriteCom() const { return res_.spriteCom.get(); }
    SpriteManager* GetSpriteManager() const { return res_.spriteManager.get(); }
    WindowAPI* GetWindowAPI() const { return res_.windowAPI.get(); }

    AudioManager* GetAudioManager() const { return audioManager_.get(); }
    KeyInput* GetKeyInput() const { return keyInput_.get(); }
    MouseInput* GetMouseInput() const { return mouseInput_.get(); }
    OffScreenRendering* GetOffScreenRendering() const { return offScreenRendering_.get(); }
    Fade* GetFade() const { return fade_.get(); }
    ParticleManager* GetParticleManager() const { return particleManager_.get(); }
    ImGuiManager* GetImGuiManager() const { return imguiManager_.get(); }
    Light* GetLight() const { return light_.get(); }
    Camera* GetCamera() const { return camera_.get(); }
    SkyboxCom* GetSkyboxCom() const { return skyboxCom_.get(); }
    SkyBox* GetSkyBox() const { return skybox_.get(); }

    void BeginFrame();
    void EndFrame(std::function<void(const RenderContext&)> spriteDrawCallback);
    RenderContext GetRenderContext() const;

private:
   
    SubsystemResult res_;
    std::ostream* logStream_ = nullptr;

    // サブシステム
    std::unique_ptr<AudioManager> audioManager_;
    std::unique_ptr<KeyInput> keyInput_;
    std::unique_ptr<MouseInput> mouseInput_;
    std::unique_ptr<OffScreenRendering> offScreenRendering_;
    std::unique_ptr<Fade> fade_;
    std::unique_ptr<ParticleManager> particleManager_;
    std::unique_ptr<ImGuiManager> imguiManager_;
    std::unique_ptr<Light> light_;
    std::unique_ptr<Camera> camera_;
    std::unique_ptr<SkyboxCom> skyboxCom_;
    std::unique_ptr<SkyBox> skybox_;

    // 念のため
    InitConfig cfg_;

	bool finalized_ = false;
};
