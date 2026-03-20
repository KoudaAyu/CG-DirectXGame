#pragma once

#include <memory>

#include"AudioManager.h"
#include"Camera.h"
#include"CrashDump.h"
#include"DirectXCom.h"
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
#include"SceneManager.h"
#include"SceneRegistration.h"
#include"Sound.h"
#include"Sphere.h"
#include"Sprite.h"
#include"SpriteCom.h"
#include"SpriteManager.h"
#include"ResourceLeakCheek.h"
#include"TextureManager.h"
#include"WindowsAPI.h"

#include <vector>
#include <random>

class Game : public Framework
{
public:
	void Initialize() override;
	void Finalize() override;
	void Update() override;
	void Draw() override;

	bool IsQuitRequested() override;

public:
	std::ostream& logStream = log.GetLogStream();

	WindowAPI* GetWindowAPI() { return windowAPI.get(); }
	const WindowAPI* GetWindowAPI() const { return windowAPI.get(); }
	DirectXCom* GetDirectXCom() { return directXCom.get(); }
	const DirectXCom* GetDirectXCom() const { return directXCom.get(); }
	SpriteCom* GetSpriteCom() { return spriteCom.get(); }
	const SpriteCom* GetSpriteCom() const { return spriteCom.get(); }
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
	std::vector<std::unique_ptr<Sprite>>& GetSprites() { return spriteManager_->GetSprites(); }
	const std::vector<std::unique_ptr<Sprite>>& GetSprites() const { return spriteManager_->GetSprites(); }
	Object3d* GetObject3d() { return object3d_.get(); }
	const Object3d* GetObject3d() const { return object3d_.get(); }
	Object3dCom* GetObject3dCom() { return object3dCom.get(); }
	const Object3dCom* GetObject3dCom() const { return object3dCom.get(); }
	ParticleManager* GetParticleManager() { return particleManager.get(); }
	const ParticleManager* GetParticleManager() const { return particleManager.get(); }


private:


private:
	ResourceLeakCheek leakChecker; //リソースリークチェック用のオブジェクト
	CrashDump crashDump; //クラッシュダンプ生成用のオブジェクト
	Log log;
	
	std::unique_ptr<Camera> camera_;
	std::unique_ptr<DirectXCom> directXCom;
	std::unique_ptr<ImGuiManager> imguiManager;
	std::unique_ptr<Light> light;
	std::unique_ptr<Model> model_;
	std::unique_ptr<ModelCom>modelCom_;
	std::unique_ptr<Object3d> object3d_;
	std::unique_ptr<Object3dCom> object3dCom;
	std::unique_ptr<ParticleManager> particleManager;
	std::unique_ptr<SpriteCom> spriteCom;
	std::unique_ptr<SpriteManager> spriteManager_;
	std::unique_ptr<WindowAPI> windowAPI;//ウィンドウ関連のAPIをまとめたオブジェクト
	
	DebugCamera debugCamera_;
	
	SRVManager srvManager;
	std::list<ParticleManager::Particle> particles;
	ParticleEmitter particleEmitter;
	KeyInput inputManager;
	std::unique_ptr<AudioManager> audioManager_;
	std::unique_ptr<MaterialManager> materialManager_;
private:
	std::vector<std::unique_ptr<Sprite>>sprites;
	Sprite::Transform transformObject;
	//Sprite::Transform uvTransformSprite;
	
	Sprite::Transform cameraTransform;

	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU;
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2;

	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU;
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2;

	

	

	Object3d::ModelData modelData;

	

	const float kDeltaTime = 1.0f / 60.0f;

	//SRVの切り替え
	bool useMonsterBall = true;
	//Objectの描画切り替え
	bool drawObject = false;
	bool drawSprite = false;
	

	// Keep GPU resources alive beyond Initialize
	
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource2;
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource2;
	//Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource;


	

};

