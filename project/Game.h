#pragma once

#include"GamePlayScene.h"

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
#include"Sound.h"
#include"Sphere.h"
#include"Sprite.h"
#include"SpriteCom.h"
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

	// Framework::Run will call this to check for WM_QUIT
	bool IsQuitRequested() override;

public:
	std::ostream& logStream = log.GetLogStream();

	WindowAPI* GetWindowAPI() { return windowAPI; }
	const WindowAPI* GetWindowAPI() const { return windowAPI; }
	DirectXCom* GetDirectXCom() { return directXCom; }
	const DirectXCom* GetDirectXCom() const { return directXCom; }
	SpriteCom* GetSpriteCom() { return spriteCom; }
	const SpriteCom* GetSpriteCom() const { return spriteCom; }
	Sprite* GetSprites(size_t index)
	{
		if (index < sprites.size())
		{
			return sprites[index];
		}
		return nullptr;
	}
	const Sprite* GetSprites(size_t index) const
	{
		if (index < sprites.size())
		{
			return sprites[index];
		}
		return nullptr;
	}
	std::vector<Sprite*>& GetSprites()
	{
		return sprites;
	}
	const std::vector<Sprite*>& GetSprites() const
	{
		return sprites;
	}
	Object3d* GetObject3d() { return object3d; }
	const Object3d* GetObject3d() const { return object3d; }
	Object3dCom* GetObject3dCom() { return object3dCom; }
	const Object3dCom* GetObject3dCom() const { return object3dCom; }
	ParticleManager* GetParticleManager() { return particleManager; }
	const ParticleManager* GetParticleManager() const { return particleManager; }
	Sphere* GetSphere() { return sphere; }
	const Sphere* GetSphere() const { return sphere; }

private:

	GamePlayScene* scene_ = nullptr;

private:
	ResourceLeakCheek leakChecker; //リソースリークチェック用のオブジェクト
	CrashDump crashDump; //クラッシュダンプ生成用のオブジェクト
	Log log;
	WindowAPI* windowAPI = nullptr; //ウィンドウ関連のAPIをまとめたオブジェクト
	DirectXCom* directXCom = nullptr;
	SpriteCom* spriteCom = nullptr;
	Model* model_ = nullptr;
	ModelCom* modelCom_ = nullptr;
	Object3d* object3d = nullptr;
	Object3dCom* object3dCom = nullptr;
	Light* light = nullptr;
	ParticleManager* particleManager = nullptr;
	Sphere* sphere = nullptr;
	ImGuiManager* imguiManager = nullptr;
	DebugCamera debugCamera_;
	Camera* camera = nullptr;
	SRVManager* srvManager = nullptr;
	std::list<ParticleManager::Particle> particles;
	ParticleEmitter particleEmitter;
	Emitter emitter;
	KeyInput inputManager;
	Sound* sound_ = nullptr;
	MaterialManager* materialManager = nullptr;
private:
	std::vector<Sprite*>sprites;
	Sprite::Transform transformObject;
	Sprite::Transform uvTransformSprite;
	Sprite::Transform transformSphere;
	Sprite::Transform cameraTransform;
	
	//D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};

	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU;
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2;

	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU;
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2;

	

	//D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU;

	Object3d::ModelData modelData;

	//const uint32_t kNumMaxInstances = 10;
	uint32_t numInstance = 0;

	const float kDeltaTime = 1.0f / 60.0f;

	//std::mt19937 randomEngine{ std::random_device{}() };

	//ParticleManager::ParticleForGPU* instanceData = nullptr;

	//SRVの切り替え
	bool useMonsterBall = true;
	//Objectの描画切り替え
	bool drawObject = false;
	bool drawSprite = false;
	bool drawSphere = false;

	// Keep GPU resources alive beyond Initialize
	
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource2;
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource2;
	//Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource;

	// UI controlled sprite position (initial 100,100)
	Vector2 uiSpritePosition = { 100.0f, 100.0f };

};

