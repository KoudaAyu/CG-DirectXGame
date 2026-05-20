#include"GamePlayScene.h"
#include"Camera.h"
#include"Object3dCom.h"
#include "SceneManager.h"
#include "MaterialManager.h"
#include "Light.h"
#include "ParticleManager.h"
#include "RootParam.h"
#include "RenderContext.h"
#include "Baziru3_Engine\Graphics\SceneRenderRequests.h"
#include "SpriteManager.h"
#include "TextureManager.h"
#include "AudioManager.h"
#include "Player.h"
#include <cassert>
#include <Windows.h>

void GamePlayScene::Initialize(DirectXCom* dxCommon, Camera* camera)
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
        hitEffect_ = std::make_unique<HitEffect>();
		hitEffect_->Initialize(directXCom, object3dCom, materialManager, light, camera_, 64, 1.0f, 0.2f, 32, 1.0f, 1.0f, 3.0f);
       hitEffect_->SetParticleManager(particleManager);
		hitEffect_->SetCylinderEnabled(true);
      hitEffect_->SetRingEnabled(true);
       hitEffect_->SetEffectDuration(0.35f);
		Sprite::Transform transformCylinder = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
		hitEffect_->GetCylinderTransform() = transformCylinder;
       hitEffect_->Update(kDeltaTime);
		hitEffectInitialized = true;

		sphere_ = std::make_unique<Sphere>();
		sphere_->Initialize(directXCom, object3dCom, materialManager, light, camera_);
		sphereInitialized = true;

		uvTransformSprite = {
			{1.0f, 1.0f, 1.0f},
			{0.0f, 0.0f, 0.0f},
			{0.0f, 0.0f, 0.0f}
		};

		Object3d::ModelData animatedCubeModelData = Object3d::LoadModelFile("Resources/CG4/human", "walk.gltf");
		if (!animatedCubeModelData.material.textureFilePath.empty())
		{
			animatedCubeModelData.material.textureIndex = TextureManager::GetInstance()->Load(animatedCubeModelData.material.textureFilePath);
		}

		animatedCube_ = std::make_unique<Object3d>();
		animatedCube_->Initialize(object3dCom, animatedCubeModelData);
		animatedCube_->SetTranslate({ 2.0f, 0.0f, 0.0f });
		animatedCube_->SetScale({ 1.0f, 1.0f, 1.0f });
		animatedCubeInitialized_ = true;
		animation_ = LoadAnimationFile("Resources/CG4/human", "walk.gltf");
      if (animation_.duration > 0.0f && !animation_.nodeAnimations.empty())
		{
			animator_.SetAnimation(&animation_);
		}

      skeleton_ = SkeletonLoader{}.LoadSkeletonFile("Resources/CG4/human", "walk.gltf");
		if (!skeleton_.joints.empty())
		{
            skeleton_.Update();
			skeletonDebug_.Initialize(directXCom, object3dCom, materialManager, light, camera_, skeleton_);
		}
	}

    // プレイヤーの初期化は Player クラスへ移譲
	if (!player_)
	{
		player_ = std::make_unique<Player>();
		player_->Initialize(object3dCom, camera_);
	}

	//スプライト共通テクスチャ読み込み

	// スプライトマネージャが未設定なら SceneManager 経由で初期化を試みる
	if (!spriteManager_)
	{
		SpriteCom* sc = SceneManager::GetInstance()->GetSpriteCom();
		if (sc)
		{
			spriteManager_ = std::make_unique<SpriteManager>();
           spriteManager_->Initialize(sc, "Resources/uvChecker.png", 0);
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
  cylinderTextureIndex_ = TextureManager::GetInstance()->Load("Resources/CG4/gradationLine.png");
   particleTextureA = TextureManager::GetInstance()->Load("Resources/uvChecker.png");
   particleTextureB = TextureManager::GetInstance()->Load("Resources/CG4/circle2.png");
   if (hitEffect_)
   {
	   hitEffect_->SetPlaneParticleTextureIndex(particleTextureB);
	   hitEffect_->SetPlaneParticleCount(emitter.count);
	   hitEffect_->SetRingTextureIndex(particleTextureA);
   }
}

void GamePlayScene::Finalize()
{
   if (hitEffect_)
	{
      hitEffect_->Finalize();
		hitEffect_.reset();
	}
    hitEffectInitialized = false;

	// release player if created
	if (player_)
	{
        player_->Finalize();
		player_.reset();
	}
}

void GamePlayScene::Update()
{

	//球体の更新
	if (sphereInitialized && sphere_)
	{
		Sprite::Transform transformSphere = sphere_->GetTransform();
		transformSphere.rotate.y += 0.01f;
     transformSphere.translate.x = -2.0f;
		sphere_->SetTransform(transformSphere);
		sphere_->Update();
	}

	if (animatedCubeInitialized_ && animatedCube_)
	{
		Vector3 rotate = animatedCube_->GetRotate();
		rotate.y += 0.01f;
		animatedCube_->SetRotate(rotate);
		animatedCube_->Update();
	}

  if (skeletonDebug_.IsInitialized() && animatedCube_)
	{
     if (animator_.HasAnimation())
		{
           animator_.Update(kDeltaTime);
			animator_.ApplyTo(skeleton_);
		}

		skeleton_.Update();

		const Matrix4x4 modelWorldMatrix = MakeAffineMatrix(
			animatedCube_->GetScale(),
			animatedCube_->GetRotate(),
			animatedCube_->GetTranslate());
		skeletonDebug_.Sync(skeleton_, modelWorldMatrix);
	}

   if (hitEffectInitialized && hitEffect_)
	{
        hitEffect_->SetPlaneParticleCount(emitter.count);
       hitEffect_->Update(kDeltaTime);
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

	// 9キーで Ring を発生させる
	{
		static bool prevF2 = false;
		bool curF2 = (GetAsyncKeyState('9') & 0x8000) != 0;
		if (curF2 && !prevF2)
		{
            if (hitEffect_)
			{
              Vector3 effectTranslate = emitter.transform.GetTranslate();
				effectTranslate.y += 1.5f;
				hitEffect_->Play(effectTranslate);
			}
		}
		prevF2 = curF2;
	}

	// 8キーで HitEffect(Ringではないもの) を発生させる
	{
		static bool prevF3 = false;
		bool curF3 = (GetAsyncKeyState('8') & 0x8000) != 0;
		if (curF3 && !prevF3)
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
				particleManager->AddParticles(newParticles);
			}
		}
		prevF3 = curF3;
	}

    // ParticleManager is updated by the engine (SceneManager) after the scene Update.

	// Player update
	if (player_)
	{
		player_->Update();
	}
}

void GamePlayScene::Draw(SceneRenderRequests& renderRequests)
{
	RenderContext ctx{};
	if (directXCom)
	{
		ctx.commandList = directXCom->GetCommandList().Get();
		ctx.windowAPI = directXCom->GetWindowAPI();
		ctx.camera = camera_;
		ctx.light = SceneManager::GetInstance()->GetLight();
		ctx.materialGPUAddress = (materialManager && materialManager->GetMaterialResource()) ?
			materialManager->GetMaterialResource()->GetGPUVirtualAddress() : 0;
	}


    // Draw player if available
	if (player_)
	{
		player_->Draw(ctx);
	}

}