#pragma once

#include "SubsystemFactory.h"
#include "RenderContext.h"
#include <ostream>
#include <sstream>
#include <memory>
#include <functional>

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

// エンジン全サブシステムのライフサイクルを管理するクラス。
// Game クラスがここに依存し、各シーンは SceneManager 経由でリソースを取得する。
class EngineContext
{
public:
    ~EngineContext();

    // 全サブシステムを初期化する
    bool Initialize(std::ostream& log, const InitConfig& cfg);

    // 全サブシステムを逆依存順で破棄する
    void Finalize();

    // --- Core リソース ---
    [[nodiscard]] DirectXCom*    GetDirectXCom()    const { return res_.directXCom.get();    }
    [[nodiscard]] SpriteCom*     GetSpriteCom()     const { return res_.spriteCom.get();     }
    [[nodiscard]] SpriteManager* GetSpriteManager() const { return res_.spriteManager.get(); }
    [[nodiscard]] WindowAPI*     GetWindowAPI()     const { return res_.windowAPI.get();     }

    // --- ゲームサブシステム ---
    [[nodiscard]] AudioManager*       GetAudioManager()       const { return audioManager_.get();       }
    [[nodiscard]] KeyInput*           GetKeyInput()           const { return keyInput_.get();           }
    [[nodiscard]] MouseInput*         GetMouseInput()         const { return mouseInput_.get();         }
    [[nodiscard]] OffScreenRendering* GetOffScreenRendering() const { return offScreenRendering_.get(); }
    [[nodiscard]] Fade*               GetFade()               const { return fade_.get();               }
    [[nodiscard]] ParticleManager*    GetParticleManager()    const { return particleManager_.get();    }
    [[nodiscard]] ImGuiManager*       GetImGuiManager()       const { return imguiManager_.get();       }
    [[nodiscard]] Light*              GetLight()              const { return light_.get();              }
    [[nodiscard]] Camera*             GetCamera()             const { return camera_.get();             }
    [[nodiscard]] SkyboxCom*          GetSkyboxCom()          const { return skyboxCom_.get();          }
    [[nodiscard]] SkyBox*             GetSkyBox()             const { return skybox_.get();             }

    // --- フレームループ補助 ---
    void BeginFrame();
    void EndFrame(std::function<void(const RenderContext&)> spriteDrawCallback);
    [[nodiscard]] RenderContext GetRenderContext() const;

private:
    SubsystemResult res_;
    std::ostream*   logStream_ = nullptr;
    InitConfig      cfg_;
    bool            finalized_ = false;

    std::unique_ptr<AudioManager>       audioManager_;
    std::unique_ptr<KeyInput>           keyInput_;
    std::unique_ptr<MouseInput>         mouseInput_;
    std::unique_ptr<OffScreenRendering> offScreenRendering_;
    std::unique_ptr<Fade>               fade_;
    std::unique_ptr<ParticleManager>    particleManager_;
    std::unique_ptr<ImGuiManager>       imguiManager_;
    std::unique_ptr<Light>              light_;
    std::unique_ptr<Camera>             camera_;
    std::unique_ptr<SkyboxCom>          skyboxCom_;
    std::unique_ptr<SkyBox>             skybox_;
};
