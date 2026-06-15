#include"GamePlayScene.h"
#include"Camera.h"
#include"Object3dCom.h"
#include"SkinningObject3dCom.h"
#include"Model.h"
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
#include <cassert>
#include <Windows.h>

void GamePlayScene::Initialize(DirectXCom* dxCommon, Camera* camera)
{
	camera_ = camera;
	assert(dxCommon != nullptr);
	this->directXCom = dxCommon;


	object3dCom = SceneManager::GetInstance()->GetObject3dCom();
	skinningObject3dCom = SceneManager::GetInstance()->GetSkinningObject3dCom();
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

		// Model::LoadModelFile でボーンウェイト込みデータを取得
		Model::ModelData skinModelData = Model::LoadModelFile("Resources/CG4/human", "walk.gltf");

		// Object3d 用にコピー（頂点・インデックス・マテリアル）
		Object3d::ModelData animatedCubeModelData;
		animatedCubeModelData.vertices = skinModelData.vertices;
		animatedCubeModelData.indices  = skinModelData.indices;
		animatedCubeModelData.material.textureFilePath = skinModelData.material.textureFilePath;
		if (!animatedCubeModelData.material.textureFilePath.empty())
		{
			animatedCubeModelData.material.textureIndex = TextureManager::GetInstance()->Load(animatedCubeModelData.material.textureFilePath);
			skinModelData.material.textureIndex = animatedCubeModelData.material.textureIndex;
		}

		animatedCube_ = std::make_unique<Object3d>();
		animatedCube_->Initialize(object3dCom, animatedCubeModelData);
		animatedCube_->SetTranslate({ 2.0f, 0.0f, 0.0f });
		animatedCube_->SetScale({ 1.0f, 1.0f, 1.0f });
		animatedCube_->SetEnableLighting(true);
		animatedCubeInitialized_ = true;

		animation_ = LoadAnimationFile("Resources/CG4/human", "walk.gltf");
		skeleton_ = SkeletonLoader{}.LoadSkeletonFile("Resources/CG4/human", "walk.gltf");

		if (animation_.duration > 0.0f && !animation_.nodeAnimations.empty() && !skeleton_.joints.empty())
		{
			// skinModelData にボーンウェイトが入っているので正しく渡す
			animatedCube_->SetupAnimation(&animation_, skeleton_, skinModelData);
		}

		if (!skeleton_.joints.empty())
		{
			skeletonDebug_.Initialize(directXCom, object3dCom, materialManager, light, camera_, skeleton_);
		}
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
		animatedCube_->Update(); // ステップ1〜4はエンジン層で実行される
	}

	if (skeletonDebug_.IsInitialized() && animatedCube_)
	{
		const Matrix4x4 modelWorldMatrix = MakeAffineMatrix(
			animatedCube_->GetScale(),
			animatedCube_->GetRotate(),
			animatedCube_->GetTranslate());
		skeletonDebug_.Sync(animatedCube_->GetSkeleton(), modelWorldMatrix);
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

	particleManager->Update(kDeltaTime);
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

   if (hitEffectInitialized && hitEffect_ && cylinderTextureIndex_ != TextureManager::kInvalidTextureIndex)
	{
     hitEffect_->SetTextureIndex(cylinderTextureIndex_);
		hitEffect_->Draw();
	}

	if (sphereInitialized && sphere_)
	{
		renderRequests.spheres.Request(sphere_.get());
	}

   if (skeletonDebug_.IsInitialized() && showSkeletonDebug_)
	{
		D3D12_GPU_DESCRIPTOR_HANDLE skeletonTexHandle = {};
		if (cylinderTextureIndex_ != TextureManager::kInvalidTextureIndex)
		{
			skeletonTexHandle = TextureManager::GetInstance()->GetSrvHandleGPU(cylinderTextureIndex_);
		}
	  skeletonDebug_.Draw(renderRequests, skeletonTexHandle);
	}

	if (animatedCubeInitialized_ && animatedCube_)
	{
		const auto& modelData = animatedCube_->GetModelData();
		if (modelData.material.textureIndex != TextureManager::kInvalidTextureIndex)
		{
			ctx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(modelData.material.textureIndex);
		}
		else
		{
			ctx.textureHandle = {};
		}

		// ジョイントがあればスキニングシェーダー、なければ通常シェーダー
		if (!animatedCube_->GetSkeleton().joints.empty() && skinningObject3dCom)
		{
			skinningObject3dCom->Draw(animatedCube_.get(), ctx, modelData, true);
		}
		else if (object3dCom)
		{
			object3dCom->Draw(animatedCube_.get(), ctx, modelData, true);
		}
	}

}
