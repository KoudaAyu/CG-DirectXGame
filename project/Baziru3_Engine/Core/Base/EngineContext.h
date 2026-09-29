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
#include "Object3dCom.h"
#include "SkinningObject3dCom.h"
#include "MaterialManager.h"

// エンジンの初期化・更新・描画パイプラインを統括するクラス
class EngineContext
{
public:
    ~EngineContext();

    // エンジン全体の初期化
    bool Initialize(std::ostream& log, const InitConfig& cfg);

    // 全サブシステムの終了処理
    void Finalize();

    // --- Core リソース ---
    [[nodiscard]] DirectXCom*    GetDirectXCom()    const { return res_.directXCom.get();    }
    [[nodiscard]] SpriteCom*     GetSpriteCom()     const { return res_.spriteCom.get();     }
    [[nodiscard]] SpriteManager* GetSpriteManager() const { return res_.spriteManager.get(); }
    [[nodiscard]] WindowAPI*     GetWindowAPI()     const { return res_.windowAPI.get();     }

    // --- ゲームサブシステム ---
    [[nodiscard]] AudioManager*        GetAudioManager()        const { return audioManager_.get();        }
    [[nodiscard]] KeyInput*            GetKeyInput()            const { return keyInput_.get();            }
    [[nodiscard]] MouseInput*          GetMouseInput()          const { return mouseInput_.get();          }
    [[nodiscard]] OffScreenRendering*  GetOffScreenRendering()  const { return offScreenRendering_.get();  }
    [[nodiscard]] Fade*                GetFade()                const { return fade_.get();                }
    [[nodiscard]] ParticleManager*     GetParticleManager()     const { return particleManager_.get();     }
    [[nodiscard]] ImGuiManager*        GetImGuiManager()        const { return imguiManager_.get();        }
    [[nodiscard]] Light*               GetLight()               const { return light_.get();               }
    [[nodiscard]] Camera*              GetCamera()              const { return camera_.get();              }
    [[nodiscard]] SkyboxCom*           GetSkyboxCom()           const { return skyboxCom_.get();           }
    [[nodiscard]] SkyBox*              GetSkyBox()              const { return skybox_.get();              }
    [[nodiscard]] Object3dCom*         GetObject3dCom()         const { return object3dCom_.get();         }
    [[nodiscard]] SkinningObject3dCom* GetSkinningObject3dCom() const { return skinningObject3dCom_.get(); }
    [[nodiscard]] MaterialManager*     GetMaterialManager()     const { return materialManager_.get();     }

    // --- メインループ・フレーム制御API ---

    // ウィンドウメッセージ処理（終了要求があれば true）
    bool ProcessMessage();

    // 入力・カメラ・音声等の全サブシステム一括更新
    void Update();

    // フレーム描画開始（画面クリア・レンダーターゲット設定）
    void BeginFrame();

    // 3Dシーンおよび2Dスプライトの描画コマンド記録
    void RenderFrame(
        std::function<void(const RenderContext&)> scene3dCallback,
        std::function<void(const RenderContext&)> sprite2dCallback = nullptr);

    // フレーム描画終了（ポストプロセス・ImGui・画面表示）
    void EndFrame();

    /// <summary>
    /// 後方互換用：スプライト描画コールバックを受け取る旧形式のEndFrame
    /// </summary>
    void EndFrame(std::function<void(const RenderContext&)> spriteDrawCallback);

    /// <summary>
    /// 現在のコマンドリストやカメラ情報を含んだ描画コンテキストを生成して取得
    /// </summary>
    [[nodiscard]] RenderContext GetRenderContext() const;

private:
    SubsystemResult res_;
    std::ostream*   logStream_ = nullptr;
    InitConfig      cfg_;
    bool            finalized_ = false;

    std::unique_ptr<AudioManager>        audioManager_;
    std::unique_ptr<KeyInput>            keyInput_;
    std::unique_ptr<MouseInput>          mouseInput_;
    std::unique_ptr<OffScreenRendering>  offScreenRendering_;
    std::unique_ptr<Fade>                fade_;
    std::unique_ptr<ParticleManager>     particleManager_;
    std::unique_ptr<ImGuiManager>        imguiManager_;
    std::unique_ptr<Light>               light_;
    std::unique_ptr<Camera>              camera_;
    std::unique_ptr<SkyboxCom>           skyboxCom_;
    std::unique_ptr<SkyBox>              skybox_;
    std::unique_ptr<Object3dCom>         object3dCom_;
    std::unique_ptr<SkinningObject3dCom> skinningObject3dCom_;
    std::unique_ptr<MaterialManager>     materialManager_;
};

