#pragma once
#include <iostream>
#include "AbstractSceneFactory.h"
#include "BaseScene.h"
#include <memory>
#include <cstdint>
#include <string>
#include <string_view>

class DirectXCom;
class Camera;
class Object3dCom;
class SkinningObject3dCom;
class MaterialManager;
class Light;
class ParticleManager;
class SpriteCom;
class AudioManager;
class SkyBox;
class SkyboxCom;
class Fade;
struct ID3D12GraphicsCommandList;
struct SceneRenderRequests;

// シーンの切り替えと、各シーンが必要とするサブシステムの仲介を担うクラス。
// 各シーンは SceneManager 経由でカメラ・ライト・パーティクル等を取得する。
class SceneManager
{
public:
    SceneManager(DirectXCom* dxCommon = nullptr, std::ostream& logStream = std::cerr)
        : dxCommon_(dxCommon), logStream_(logStream) {}

    ~SceneManager();

    // SceneManager を初期化する
    void Initialize(DirectXCom* dxCommon);

    // アクティブなシーンとエンジンサブシステムを更新する
    void Update(float deltaTime);

    // 現在のシーンを描画する
    void Draw(SceneRenderRequests& renderRequests);

    // スカイボックスを描画する
    void DrawSkybox(ID3D12GraphicsCommandList* commandList) const;

    // 次のシーンを名前で予約する（フェードアウト → 切り替え → フェードイン）
    void ChangeScene(const std::string& sceneName);

    // 保留中のシーン遷移を適用する
    void ApplyPendingSceneChange();

    [[nodiscard]] static SceneManager* GetInstance();
    static void Destroy();

    // サブシステムの登録・取得
    void SetDirectXCom(DirectXCom* dxCommon) { dxCommon_ = dxCommon; }
    [[nodiscard]] DirectXCom* GetDirectXCom() const { return dxCommon_; }

    void SetSceneFactory(std::unique_ptr<AbstractSceneFactory> sceneFactory)
        { sceneFactory_ = std::move(sceneFactory); }
    [[nodiscard]] BaseScene* GetCurrentScene() const { return scene_.get(); }

    void SetCamera(Camera* camera) { camera_ = camera; }
    [[nodiscard]] Camera* GetCamera() const { return camera_; }

    void SetObject3dCom(Object3dCom* object3dCom) { object3dCom_ = object3dCom; }
    [[nodiscard]] Object3dCom* GetObject3dCom() const { return object3dCom_; }

    void SetSkinningObject3dCom(SkinningObject3dCom* skinningObject3dCom) { skinningObject3dCom_ = skinningObject3dCom; }
    [[nodiscard]] SkinningObject3dCom* GetSkinningObject3dCom() const { return skinningObject3dCom_; }

    void SetMaterialManager(MaterialManager* materialManager) { materialManager_ = materialManager; }
    [[nodiscard]] MaterialManager* GetMaterialManager() const { return materialManager_; }

    void SetLight(Light* light) { light_ = light; }
    [[nodiscard]] Light* GetLight() const { return light_; }

    void SetParticleManager(ParticleManager* particleManager) { particleManager_ = particleManager; }
    [[nodiscard]] ParticleManager* GetParticleManager() const { return particleManager_; }

    void SetSpriteCom(SpriteCom* spriteCom) { spriteCom_ = spriteCom; }
    [[nodiscard]] SpriteCom* GetSpriteCom() const { return spriteCom_; }

    void SetFadeApplication(Fade* fade) { fadeApplication_ = fade; }
    [[nodiscard]] Fade* GetFadeApplication() const { return fadeApplication_; }

    void SetAudioManager(AudioManager* audioManager) { audioManager_ = audioManager; }
    [[nodiscard]] AudioManager* GetAudioManager() const { return audioManager_; }

    void SetSkyBox(SkyBox* skyBox) { skybox_ = skyBox; }
    [[nodiscard]] SkyBox* GetSkyBox() const { return skybox_; }

    void SetSkyboxCom(SkyboxCom* skyboxCom) { skyboxCom_ = skyboxCom; }
    [[nodiscard]] SkyboxCom* GetSkyboxCom() const { return skyboxCom_; }

    void SetSkyboxTextureIndex(uint32_t textureIndex) { skyboxTextureIndex_ = textureIndex; }
    [[nodiscard]] uint32_t GetSkyboxTextureIndex() const { return skyboxTextureIndex_; }

    void SetShowSkybox(bool show) { showSkybox_ = show; }
    [[nodiscard]] bool  GetShowSkybox()    const { return showSkybox_; }
    [[nodiscard]] bool* GetShowSkyboxPtr()       { return &showSkybox_; }

private:
    void CommitPendingSceneChange();

    std::unique_ptr<AbstractSceneFactory> sceneFactory_;
    std::unique_ptr<BaseScene>            scene_     = nullptr;
    std::unique_ptr<BaseScene>            nextScene_ = nullptr;

    DirectXCom* dxCommon_ = nullptr;

    Camera*              camera_              = nullptr;
    Object3dCom*         object3dCom_         = nullptr;
    SkinningObject3dCom* skinningObject3dCom_ = nullptr;
    MaterialManager*     materialManager_     = nullptr;
    Light*               light_               = nullptr;
    ParticleManager*     particleManager_     = nullptr;
    SpriteCom*           spriteCom_           = nullptr;
    AudioManager*        audioManager_        = nullptr;
    SkyBox*              skybox_              = nullptr;
    SkyboxCom*           skyboxCom_           = nullptr;
    uint32_t             skyboxTextureIndex_  = 0;
    bool                 showSkybox_          = true;
    Fade*                fadeApplication_     = nullptr;

    bool isSceneTransitioning_       = false;
    bool hasSwitchedSceneDuringFade_ = false;

    std::ostream& logStream_ = std::cerr;
};
