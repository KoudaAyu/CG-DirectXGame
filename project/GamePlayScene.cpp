#include"GamePlayScene.h"
#include"Camera.h"
#include"Object3dCom.h"
#include "SceneManager.h"
#include "MaterialManager.h"
#include "Light.h"
#include "ParticleManager.h"

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
	return true;
}

void GamePlayScene::Initialize(DirectXCom* dxCommon,Camera* camera)
{
	camera_ = camera;
	directXCom = dxCommon;


	if (!TryInitializeSphere())
	{
		pendingSphereInit = true;
		return;
	}

	//スプライト共通テクスチャ読み込み

	//スプライトの生成	
	//OBJからモデルデータを読み込む

	//3Dオブジェクトの生成

	//音声読み込み

	sound = Sound::GetInstance();
	sound->SoundLoadFile("Resources/Alarm01.wav");

	//必要なら音声生成
	sound->SoundPlayWave();

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
}

void GamePlayScene::Draw()
{
	for (auto& sprite : sprites)
	{
		sprite->Draw();
	}

	if (drawSphere && sphereInitialized && sphere_)
	{
		//sphere_->Draw(useMonsterBall ? textureSrvHandleGPU2 : textureSrvHandleGPU);
	}
}