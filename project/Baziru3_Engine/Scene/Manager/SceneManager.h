#pragma once
#include <iostream>
#include "AbstractSceneFactory.h"
#include "BaseScene.h"
#include <memory>
#include <cstdint>
// RenderContext is not required by SceneManager Draw; keep scene-only draw signature

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
class FadeApplication;
struct ID3D12GraphicsCommandList;
struct SceneRenderRequests;

class SceneManager
{

public:
    SceneManager(DirectXCom* dxCommon = nullptr, std::ostream& logStream = std::cerr)
        : dxCommon_(dxCommon), logStream_(logStream) {}
	~SceneManager();

	void Initialize(DirectXCom* dxCommon);

    // Update the scene manager and engine-level subsystems. deltaTime is in seconds.
	void Update(float deltaTime);

    void Draw(SceneRenderRequests& renderRequests);
	void DrawImGui();

	/// <summary>
	/// 次のシーン予約
	/// </summary>
	/// <param name="sceneName">シーン名</param>
	void ChangeScene(const std::string& sceneName);

   
    void ApplyPendingSceneChange();

	void SetDirectXCom(DirectXCom* dxCommon) { dxCommon_ = dxCommon; }
	DirectXCom* GetDirectXCom() const { return dxCommon_; }
	void SetCamera(Camera* camera) { camera_ = camera; }
	Camera* GetCamera() const { return camera_; }

	void SetSceneFactory(std::unique_ptr<AbstractSceneFactory> sceneFactory) { sceneFactory_ = std::move(sceneFactory); }
	BaseScene* GetCurrentScene() const { return scene_.get(); }

	static SceneManager* GetInstance();
	static void Destroy();


	void SetObject3dCom(Object3dCom* v) { object3dCom_ = v; }
	Object3dCom* GetObject3dCom() const { return object3dCom_; }
	void SetSkinningObject3dCom(SkinningObject3dCom* v) { skinningObject3dCom_ = v; }
	SkinningObject3dCom* GetSkinningObject3dCom() const { return skinningObject3dCom_; }
	void SetMaterialManager(MaterialManager* v) { materialManager_ = v; }
	MaterialManager* GetMaterialManager() const { return materialManager_; }
	void SetLight(Light* v) { light_ = v; }
	Light* GetLight() const { return light_; }
	void SetParticleManager(ParticleManager* v) { particleManager_ = v; }
	ParticleManager* GetParticleManager() const { return particleManager_; }
	void SetSpriteCom(SpriteCom* v) { spriteCom_ = v; }
	SpriteCom* GetSpriteCom() const { return spriteCom_; }
	void SetFadeApplication(FadeApplication* v) { fadeApplication_ = v; }
	FadeApplication* GetFadeApplication() const { return fadeApplication_; }

	
	void SetAudioManager(AudioManager* v) { audioManager_ = v; }
	AudioManager* GetAudioManager() const { return audioManager_; }
	void SetSkyBox(SkyBox* v) { skybox_ = v; }
	SkyBox* GetSkyBox() const { return skybox_; }
	void SetSkyboxCom(SkyboxCom* v) { skyboxCom_ = v; }
	SkyboxCom* GetSkyboxCom() const { return skyboxCom_; }
	void SetSkyboxTextureIndex(uint32_t v) { skyboxTextureIndex_ = v; }
	uint32_t GetSkyboxTextureIndex() const { return skyboxTextureIndex_; }
	void SetShowSkybox(bool show) { showSkybox_ = show; }
	bool GetShowSkybox() const { return showSkybox_; }
	bool* GetShowSkyboxPtr() { return &showSkybox_; }
	void DrawSkybox(ID3D12GraphicsCommandList* commandList) const;

private:
	std::unique_ptr<AbstractSceneFactory> sceneFactory_;
	std::unique_ptr<BaseScene> scene_ = nullptr;
	std::unique_ptr<BaseScene> nextScene_ = nullptr;
	DirectXCom* dxCommon_ = nullptr;
	Camera* camera_ = nullptr;

	
	Object3dCom* object3dCom_ = nullptr;
	SkinningObject3dCom* skinningObject3dCom_ = nullptr;
	MaterialManager* materialManager_ = nullptr;
	Light* light_ = nullptr;
	ParticleManager* particleManager_ = nullptr;
	SpriteCom* spriteCom_ = nullptr;


	AudioManager* audioManager_ = nullptr;
	SkyBox* skybox_ = nullptr;
	SkyboxCom* skyboxCom_ = nullptr;
	uint32_t skyboxTextureIndex_ = 0;
	bool showSkybox_ = true;
	FadeApplication* fadeApplication_ = nullptr;
	bool isSceneTransitioning_ = false;
	bool hasSwitchedSceneDuringFade_ = false;

    std::ostream& logStream_ = std::cerr;

	void CommitPendingSceneChange();

};

