#include"GamePlayScene.h"
#include"Camera.h"
#include"Object3dCom.h"
#include "SceneManager.h"
#include "MaterialManager.h"
#include "Light.h"
#include "ParticleManager.h"
#include "SpriteManager.h"
#include "AudioManager.h"
#include <cassert>
#include <Windows.h>

bool GamePlayScene::TryInitializeSphere()
{

	object3dCom = SceneManager::GetInstance()->GetObject3dCom();
	materialManager = SceneManager::GetInstance()->GetMaterialManager();
	light = SceneManager::GetInstance()->GetLight();
	particleManager = SceneManager::GetInstance()->GetParticleManager();
	
	// 必要な依存が揃っているか確認
	if (!object3dCom || !materialManager || !light || !particleManager) return false;


	sphere_ = std::make_unique<Sphere>();
	sphere_->Initialize(directXCom, object3dCom, materialManager, light, camera_);
	sphereInitialized = true;

	//uvTextureSpriteの座標
	uvTransformSprite = {
		{1.0f, 1.0f, 1.0f},
		{0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 0.0f}
	};

	return true;
}

void GamePlayScene::Initialize(DirectXCom* dxCommon,Camera* camera)
{
	camera_ = camera;
     assert(dxCommon != nullptr);
    this->directXCom = dxCommon;


	if (!TryInitializeSphere())
	{
		pendingSphereInit = true;
		return;
	}

	//スプライト共通テクスチャ読み込み

	// スプライトマネージャが未設定なら SceneManager 経由で初期化を試みる
	if (!spriteManager_)
	{
		SpriteCom* sc = SceneManager::GetInstance()->GetSpriteCom();
		if (sc)
		{
			spriteManager_ = std::make_unique<SpriteManager>();
			spriteManager_->Initialize(sc, "Resources/uvChecker.png", 5);
		}
	}

	//OBJからモデルデータを読み込む

	//3Dオブジェクトの生成

	//音声読み込み

	
	auto am = SceneManager::GetInstance()->GetAudioManager();
	if (am)
	{
		int32_t id = am->Load("Resources/Alarm01.wav");
		if (id >= 0)
		{
			am->Play(id);
		}
	}

	//パーティクルの初期化

	//エミッターの数値

	emitter.transform.SetTranslate({ 0.0f,0.0f,0.0f });
	emitter.transform.SetRotate({ 0.0f,0.0f,0.0f });
	emitter.transform.SetScale({ 1.0f,1.0f,1.0f });

	emitter.count = 3; // 初期値
	emitter.frequency = 0.5f;
	emitter.frequencyTime = 0.0f;
}

void GamePlayScene::Finalize()
{
}

void GamePlayScene::Update()
{
	
	if (pendingSphereInit && !sphereInitialized)
	{
		if (TryInitializeSphere()) pendingSphereInit = false;
	}

	//球体の更新
	if (sphereInitialized && sphere_)
	{
		Sprite::Transform transformSphere = sphere_->GetTransform();
		transformSphere.rotate.y += 0.01f;
		sphere_->SetTransform(transformSphere);
		sphere_->Update();
	}

	//パーティクルの更新
	emitter.frequencyTime += kDeltaTime;

	if (emitter.frequencyTime >= emitter.frequency)
	{
		auto newParticles = particleEmitter.Emit(emitter, particleManager->GetRandomEngine(), *particleManager);
		particleManager->AddParticles(newParticles);
		emitter.frequencyTime -= emitter.frequency;
	}

	particleManager->Update(kDeltaTime);

    // スプライトの毎フレーム更新はここで行う（必要な依存を持っている場合）
    if (spriteManager_ && directXCom)
    {
        WindowAPI* windowAPI = directXCom->GetWindowAPI();
        DebugCamera* debugCamera = &debugCamera_;
        if (windowAPI && debugCamera)
        {
            spriteManager_->Update(windowAPI, debugCamera);
        }
    }

 
    {
        static bool prevF1 = false;
        bool curF1 = (GetAsyncKeyState(VK_F1) & 0x8000) != 0;
        if (curF1 && !prevF1)
        {
            ToggleDrawSphere();
        }
        prevF1 = curF1;
    }
}

void GamePlayScene::Draw()
{
	for (auto& sprite : sprites)
	{
		sprite->Draw();
	}

    if (sphereInitialized && sphere_)
    {
        if (drawSphere)
        {
          
            D3D12_GPU_DESCRIPTOR_HANDLE handle{};
          
            sphere_->Draw(handle);
        }
		else
		{
			//sphere_->Draw(useMonsterBall ? textureSrvHandleGPU2 : textureSrvHandleGPU);
		}
    }
}