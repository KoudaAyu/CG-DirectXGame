#pragma once

#include <memory>

#include"AudioManager.h"
#include"Camera.h"
#include"CrashDump.h"
#include"DirectXCom.h"
#include"EngineContext.h"
#include"Framework.h"
#include"ImGuiManager.h"
#include"Light.h"
#include"Log.h"
#include"MaterialManager.h"
#include"Model.h"
#include"Object3d.h"
#include"Object3dCom.h"
#include"ParticleEmitter.h"
#include"ParticleManager.h"
#include"Baziru3_Engine\Base\OffScreenRendering\OffScreenRendering.h"
#include"Baziru3_Engine\Graphics\Particle\ParticleRenderer.h"
#include"Baziru3_Engine\Graphics\Sphere\SphereRenderer.h"
#include"SceneManager.h"
#include"SceneRegistration.h"
#include"SkyBox.h"
#include"SkyboxCom.h"
#include"Sound.h"
#include"Sphere.h"
#include"Sprite.h"
#include"SpriteCom.h"
#include"SpriteManager.h"
#include"ResourceLeakCheek.h"
#include"TextureManager.h"
#include"WindowsAPI.h"
#include "DebugUI.h"

#include <vector>
#include <random>

#include "RenderContext.h"

class Game : public Framework
{
public:
	void Initialize() override;
	void Finalize() override;
	void Update() override;
	void Draw() override;

	bool IsQuitRequested() override;

	//初期化関係
	bool InitializeEngine();

	/// <summary>
	/// DirectXComの診断Logを出す
	/// </summary>
	void LogEngineDiagnostics();

	/// <summary>
	/// SceneManagerに渡す基盤オブジェクトを用意する部分
	/// </summary>
	void InitializeSceneCore();


	/// <summary>
	/// 描画に必要な共通リソースを作る
	/// </summary>
	void InitializeModelResources();

	void InitializeSceneResources();

	/// <summary>
	/// 音声、入力系の初期化
	/// </summary>
	void InitializeAudioAndInput();

public:
	std::ostream& logStream = log.GetLogStream();
	DirectXCom* GetDirectXCom() { return engine_ ? engine_->GetDirectXCom() : nullptr; }
	const DirectXCom* GetDirectXCom() const { return engine_ ? engine_->GetDirectXCom() : nullptr; }
	Sprite* GetSprites(size_t index)
	{
		if (index < sprites.size())
		{
			return sprites[index].get();
		}
		return nullptr;
	}
	const Sprite* GetSprites(size_t index) const
	{
		if (index < sprites.size())
		{
			return sprites[index].get();
		}
		return nullptr;
	}
	std::vector<std::unique_ptr<Sprite>>& GetSprites()
	{
		if (engine_ && engine_->GetSpriteManager()) return engine_->GetSpriteManager()->GetSprites();
		static std::vector<std::unique_ptr<Sprite>> empty;
		return empty;
	}
	const std::vector<std::unique_ptr<Sprite>>& GetSprites() const
	{
		if (engine_ && engine_->GetSpriteManager()) return engine_->GetSpriteManager()->GetSprites();
		static const std::vector<std::unique_ptr<Sprite>> empty;
		return empty;
	}
	Object3d* GetObject3d() { return object3d_.get(); }
	const Object3d* GetObject3d() const { return object3d_.get(); }
	Object3dCom* GetObject3dCom() { return object3dCom.get(); }
	const Object3dCom* GetObject3dCom() const { return object3dCom.get(); }
	ParticleManager* GetParticleManager() { return particleManager.get(); }
	const ParticleManager* GetParticleManager() const { return particleManager.get(); }


private:
    void DrawObjects(const RenderContext& ctx);
    void DrawSprites(const RenderContext& ctx);
    void DrawParticles(const RenderContext& ctx);

private:
	ResourceLeakCheek leakChecker; //リソースリークチェック用のオブジェクト
	CrashDump crashDump; //クラッシュダンプ生成用のオブジェクト
	Log log;
	
	std::unique_ptr<Camera> camera_;
	std::unique_ptr<EngineContext> engine_;
	std::unique_ptr<ImGuiManager> imguiManager;
	std::unique_ptr<Light> light;
	std::unique_ptr<Model> model_;
	std::unique_ptr<ModelCom>modelCom_;
	std::unique_ptr<Object3d> object3d_;
	std::unique_ptr<Object3dCom> object3dCom;
   std::unique_ptr<OffScreenRendering> offScreenRendering_;
	std::unique_ptr<ParticleManager> particleManager;
    ParticleRenderer particleRenderer_;
    SphereRenderer sphereRenderer_;
	std::unique_ptr<SkyBox> skybox_;
	std::unique_ptr<SkyboxCom> skyboxCom_;
	
	DebugCamera debugCamera_;
	
	SRVManager srvManager;
	std::list<ParticleManager::Particle> particles;
	ParticleEmitter particleEmitter;
	KeyInput inputManager;
	std::unique_ptr<AudioManager> audioManager_;
	std::unique_ptr<MaterialManager> materialManager_;
	std::unique_ptr<DebugUI> debugUI; // debug UI
private:
	std::vector<std::unique_ptr<Sprite>>sprites;
	Sprite::Transform transformObject;
	Sprite::Transform cameraTransform;

	Object3d::ModelData modelData;

	RenderContext PrepareRenderContext();

	const float kDeltaTime = 1.0f / 60.0f;

	//SRVの切り替え
	bool useMonsterBall = true;
	//Objectの描画切り替え
	bool drawObject = false;
	bool drawSprite = false;

    uint32_t textureIndexUvChecker = TextureManager::kInvalidTextureIndex;
	uint32_t textureIndexModelTex = TextureManager::kInvalidTextureIndex;
	uint32_t textureIndexSkybox_ = TextureManager::kInvalidTextureIndex;
};

