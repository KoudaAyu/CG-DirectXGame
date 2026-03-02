#pragma once

#include"GamePlayScene.h"

#include"Camera.h"
#include"CrashDump.h"
#include"DirectXCom.h"
#include"Framework.h"
#include"ImGuiManager.h"
#include"Log.h"
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
	Object3d* object3d = nullptr;
	Object3dCom* object3dCom = nullptr;
	ParticleManager* particleManager = nullptr;
	Sphere* sphere = nullptr;
	ImGuiManager* imguiManager = nullptr;
	DebugCamera debugCamera_;
	Camera* camera = nullptr;
	CameraForGPU* cameraData = nullptr;
	std::list<ParticleManager::Particle> particles;
	ParticleEmitter particleEmitter;
	Emitter emitter;
	KeyInput inputManager;
	Sound* sound_ = nullptr;
private:
	std::vector<Sprite*>sprites;
	Sprite::Transform transformObject;
	Sprite::Transform uvTransformSprite;
	Sprite::Transform transformSphere;
	Sprite::Transform cameraTransform;
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLight;
	Object3d::DirectionalLight* directionalLightData = nullptr;
	TransformationMatrix* transformationMatrixDataSphere = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResourceSphere;
	Sprite::Material* materialData = nullptr;

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;

	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU;
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2;

	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU;
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2;

	Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource;

	D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU;

	Object3d::ModelData modelData;

	D3D12_VIEWPORT viewport{};
	D3D12_RECT scissorRect{};

	
	
	

	const uint32_t kNumMaxInstances = 10;
	uint32_t numInstance = 0;

	const float kDeltaTime = 1.0f / 60.0f;

	std::mt19937 randomEngine{ std::random_device{}() };

	ParticleManager::ParticleForGPU* instanceData = nullptr;

	//SRVの切り替え
	bool useMonsterBall = true;
	//Objectの描画切り替え
	bool drawObject = false;
	bool drawSprite = false;
	bool drawSphere = false;

	// Keep GPU resources alive beyond Initialize
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceModel;
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource2;
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource2;
	Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource;

};

//#include<Windows.h>
//
////自作h
//#include"Camera.h"
//#include"DebugCamera.h"
//#include"DirectXCom.h"
//#include"KeyInput.h"
//#include"Matrix4x4.h"
//#include"Random.h"
//#include"ParticleEmitter.h"
//#include"Sound.h"
//#include"TextureManager.h"
//#include"Vector.h"
//#include"WindowsAPI.h"
//
//#include<chrono> //時間を扱うライブラリ
//#include<filesystem> //ファイルやディレクトリに関する操作を行うライブラリ
//#include<format> //文字列のフォーマットを行うライブラリ
//#include<fstream> //ファイルにかいたり読んだりするライブラリ
//#include<string> //文字列を扱うライブラリ
//#include<strsafe.h>
//
//#include<d3d12.h>
//#include<dxgi1_6.h>
//#include<cassert>
//
//
//
////Comptr
//#include<wrl.h>
//
////Debug用
//#include<dbghelp.h>
//#pragma comment(lib,"Dbghelp.lib")
//
////ファイル関係 / サウンド関係
//#include<sstream>
////#include <xaudio2.h>
////#pragma comment(lib, "xaudio2.lib")
//
//
////ReportLiveObjects
//#include <dxgidebug.h>
//#pragma comment(lib, "dxguid.lib")
//
////DXCの初期化
//#include<dxcapi.h>
//#pragma comment(lib, "dxcompiler.lib")
//
////Textureの転送
//#include"externals/DirectXTex/d3dx12.h"
//#include<vector>
//
//#include <DirectXMath.h>
//#include<cmath>
//#include "externals/DirectXTex/DirectXTex.h"
//
//#include<numbers>
//#include<list>
//
//
//#include"ImGuiManager.h"

