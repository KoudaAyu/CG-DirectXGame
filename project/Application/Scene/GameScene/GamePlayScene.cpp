#include"GamePlayScene.h"
#include"Camera.h"
#include"Object3dCom.h"
#include "SceneManager.h"
#include "MaterialManager.h"
#include "Light.h"
#include "ParticleManager.h"
#include "RenderContext.h"
#include "SpriteManager.h"
#include "AudioManager.h"
#include <cassert>
#include <Windows.h>



void GamePlayScene::Initialize(DirectXCom* dxCommon,Camera* camera)
{
	camera_ = camera;
     assert(dxCommon != nullptr);
    this->directXCom = dxCommon;


    object3dCom = SceneManager::GetInstance()->GetObject3dCom();
    materialManager = SceneManager::GetInstance()->GetMaterialManager();
    light = SceneManager::GetInstance()->GetLight();
    particleManager = SceneManager::GetInstance()->GetParticleManager();

  
    if (object3dCom && materialManager && light && particleManager)
    {
        sphere_ = std::make_unique<Sphere>();
        sphere_->Initialize(directXCom, object3dCom, materialManager, light, camera_);
        sphereInitialized = true;

        uvTransformSprite = {
            {1.0f, 1.0f, 1.0f},
            {0.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 0.0f}
        };
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

    // デバッグ用に2つのパーティクル用のテクスチャを読み込む
    particleTextureA = TextureManager::GetInstance()->Load("Resources/uvChecker.png");
    particleTextureB = TextureManager::GetInstance()->Load("Resources/CG4/circle2.png");
}

void GamePlayScene::Finalize()
{
}

void GamePlayScene::Update()
{
	
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
		for (auto& p : newParticles)
		{
			p.textureIndex = particleTextureA;
		}
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

    // 9キーで Hie エフェクトを発生させる
    {
        static bool prevF2 = false;
        bool curF2 = (GetAsyncKeyState('9') & 0x8000) != 0;
        if (curF2 && !prevF2)
        {
            if (particleManager)
            {
                std::list<ParticleManager::Particle> newParticles;
                for (uint32_t i = 0; i < emitter.count; ++i)
                {
					Vector3 effectTranslate = emitter.transform.GetTranslate();
					effectTranslate.y += 1.5f;
					auto p = particleManager->MakeHieEffect(particleManager->GetRandomEngine(), effectTranslate);
					p.textureIndex = particleTextureB;
					p.lifeTime = 0.35f;
                        newParticles.push_back(p);
                }
                particleManager->AddEffectParticles(newParticles);
            }
        }
        prevF2 = curF2;
    }
}

void GamePlayScene::Draw()
{
	RenderContext ctx{};
	if (directXCom)
	{
		ctx.commandList = directXCom->GetCommandList().Get();
		ctx.windowAPI = directXCom->GetWindowAPI();
		ctx.camera = camera_;
		ctx.light = SceneManager::GetInstance()->GetLight(); // or use member 'light' if set
	}

	if (spriteManager_)
	{
		spriteManager_->DrawAll(ctx, &debugCamera_, &sprites);
	}
	else
	{
	
		for (auto& sprite : sprites)
		{
			if (sprite)
			{
				sprite->Update(ctx.windowAPI, &debugCamera_);
				sprite->Draw();
			}
		}
	}

	if (sphereInitialized && sphere_)
	{
		if (drawSphere)
		{
			D3D12_GPU_DESCRIPTOR_HANDLE handle{}; 
			sphere_->Draw(handle);
		}
	}
}