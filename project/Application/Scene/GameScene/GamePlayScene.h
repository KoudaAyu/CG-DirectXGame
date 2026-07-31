#pragma once
#include <memory>
#include <vector>
#include <string>

#include "BaseScene.h"
#include "Object3d.h"
#include "Sprite.h"
#include "ParticleEmitter.h"
#include "Baziru3_Engine/Core/IO/Mouse/MouseInput.h"

class Camera;
class Object3dCom;
class Light;
class MaterialManager;
class ParticleManager;
class SpriteCom;
struct SceneRenderRequests;
struct ModelData;

/// <summary>
/// エンジン機能確認用デモシーン
/// Player/Enemy/Obstacle 等のゲームロジックを持たず、
/// エンジンに追加した各機能（3D描画、スプライト、パーティクル、ImGui等）を
/// 動作確認することを目的とするシーン。
/// </summary>
class GamePlayScene : public BaseScene
{
public:
    GamePlayScene();

    void InitializeScene() override;
    void Finalize()        override;
    ~GamePlayScene()       override;

    void Update()                              override;
    void Draw(SceneRenderRequests& renderRequests) override;

    const char* GetSceneType() const { return "GAMEPLAY"; }

private:
    // --- エンジンコンテキスト（外部参照） ---
    Camera*          camera_         = nullptr;
    Object3dCom*     object3dCom_    = nullptr;
    Light*           light_          = nullptr;
    MaterialManager* materialManager_= nullptr;
    ParticleManager* particleManager_= nullptr;
    SpriteCom*       spriteCom_      = nullptr;

    // --- デモ用オブジェクト ---
    std::unique_ptr<Object3d>  demoObject_;   // plane.obj などのシンプルな3Dモデル
    std::vector<std::unique_ptr<Sprite>> sprites_;

    // --- パーティクル ---
    Emitter        emitter_{};
    ParticleEmitter particleEmitter_{};

    // --- テクスチャ ---
    uint32_t uvCheckerIndex_ = 0;

    // --- デモ状態 ---
    float  rotY_          = 0.0f;   // デモオブジェクト自転角
    bool   showParticles_ = false;  // パーティクル表示フラグ
    MouseInput mouseInput_;

    // --- ヘルパー ---
    void UpdateImGuiPanel();
};
