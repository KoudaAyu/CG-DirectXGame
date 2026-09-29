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

/// <summary>
/// エンジン全サブシステムのライフサイクルとレンダリングパイプラインを統括管理するクラス。
/// 
/// 【カプセル化（Encapsulation）の設計方針】
/// 1. データ（状態）と操作（メソッド）をひとまとまりにする：
///    - DirectX12デバイス、スワップチェーン、コマンドリスト、各サブシステム（描画・入力・音声等）を内部に保持。
/// 2. 外部からは必要なインターフェースだけを公開し、内部の詳細は隠す：
///    - DirectX12の生記述子ヒープやコマンドアロケータ、マルチスレッド描画提出の詳細を隠蔽。
///    - アプリケーション側（チームメンバー）は、安全かつシンプルなAPI（Update, BeginFrame, RenderFrame, EndFrame）のみでゲームを制作可能。
/// 3. チーム制作における目的：
///    - 安全性：DirectX12リソースの破壊やGPU同期ハングを防止。
///    - 一貫性：描画順序やポストプロセス処理の適用を統一。
///    - 柔軟性：内部の描画パイプラインやDirectXの実装を変更しても、ゲーム側コードへの影響を最小化。
///    - 理解しやすさ：DirectX12の知識がなくても、直感的にシーンやスプライトを描画可能。
/// </summary>
class EngineContext
{
public:
    ~EngineContext();

    /// <summary>
    /// エンジン全体の初期化（全サブシステム・DirectX12リソースの生成と関連付け）
    /// </summary>
    bool Initialize(std::ostream& log, const InitConfig& cfg);

    /// <summary>
    /// 全サブシステムを依存関係の逆順で安全に解放
    /// </summary>
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

    // --- カプセル化されたメインループ・フレーム制御API ---

    /// <summary>
    /// Windowsメッセージを処理します。
    /// 【カプセル化】Win32のMSG構造体やPeekMessage処理を隠蔽し、終了要求の有無を返します。
    /// </summary>
    /// <returns>終了要求（WM_QUIT）を受信した場合は true</returns>
    bool ProcessMessage();

    /// <summary>
    /// 入力・音声・カメラ・ImGui等の全サブシステムを一括更新します。
    /// 【カプセル化】更新の順序依存性をエンジン側で保証し、呼び出し漏れを防止します。
    /// </summary>
    void Update();

    /// <summary>
    /// フレーム描画の開始処理（バックバッファのクリア、オフスクリーンレンダリングの開始など）
    /// 【カプセル化】DirectX12のPreDrawとリソースバリア遷移を隠蔽します。
    /// </summary>
    void BeginFrame();

    /// <summary>
    /// 3Dシーンおよび2Dスプライトの描画コマンド記録をカプセル化実行します。
    /// 【カプセル化】
    ///  ・メインスレッドでの3D描画と、ワーカースレッドでの2Dスプライト並列描画を自動制御。
    ///  ・DirectX 12のコマンドアロケータ/リストのリセット、記述子ヒープの設定、GPU提出を隠蔽。
    /// </summary>
    /// <param name="scene3dCallback">3Dシーン（モデル、スカイボックス、球体、パーティクル等）を描画する処理</param>
    /// <param name="sprite2dCallback">2Dスプライト（UI、カーソル等）を描画する処理（サブスレッドで並列記録）</param>
    void RenderFrame(
        std::function<void(const RenderContext&)> scene3dCallback,
        std::function<void(const RenderContext&)> sprite2dCallback = nullptr);

    /// <summary>
    /// フレーム描画の終了処理（ポストプロセス、フェード、ImGui描画、スワップチェーンフリップ）
    /// 【カプセル化】レンダーターゲットの復帰とPresent（画面表示）を隠蔽します。
    /// </summary>
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

