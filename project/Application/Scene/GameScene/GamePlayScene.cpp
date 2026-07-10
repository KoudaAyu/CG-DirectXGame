#include"GamePlayScene.h"
#include"Camera.h"
#include"Object3dCom.h"
#include"SkinningObject3dCom.h"
#include"Model.h"
#include "SceneManager.h"
#include "Baziru3_Engine/Collision/CollisionManager.h"
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

void GamePlayScene::InitializeScene()
{
	directXCom = dxCommon_;
	camera_ = BaseScene::camera_;

	object3dCom = GetObject3dCom();
	skinningObject3dCom = GetSkinningObject3dCom();
	materialManager = GetMaterialManager();
	light = GetLight();
	particleManager = GetParticleManager();


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

		// Register skinned Mesh Collider
		animatedCubeCollider_ = std::make_unique<MeshCollider>(animatedCube_.get(), CollisionAttribute::Enemy);
		CollisionManager::GetInstance()->RegisterCollider(animatedCubeCollider_.get());

		// Spawn animated characters in a grid (Disabled for 1 character mode)
		const int cols = 0;
		const int rows = 0;
		const float spacingX = 4.0f;
		const float spacingZ = 4.0f;
		const float startX = -((cols - 1) * spacingX) / 2.0f;
		const float startZ = 10.0f;

		for (int r = 0; r < rows; ++r)
		{
			for (int c = 0; c < cols; ++c)
			{
				auto obj = std::make_unique<Object3d>();
				obj->InitializeShared(object3dCom, animatedCube_.get());
				obj->SetTranslate({ startX + c * spacingX, 0.0f, startZ + r * spacingZ });
				obj->SetScale({ 1.0f, 1.0f, 1.0f });
				obj->SetEnableLighting(true);

				auto col = std::make_unique<MeshCollider>(obj.get(), CollisionAttribute::Enemy, animatedCubeCollider_.get());
				CollisionManager::GetInstance()->RegisterCollider(col.get());

				crowd_.push_back(std::move(obj));
				crowdColliders_.push_back(std::move(col));
			}
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
    if (animatedCubeCollider_)
    {
        CollisionManager::GetInstance()->UnregisterCollider(animatedCubeCollider_.get());
        animatedCubeCollider_.reset();
    }
    for (auto& col : crowdColliders_)
    {
        CollisionManager::GetInstance()->UnregisterCollider(col.get());
    }
    crowdColliders_.clear();
    crowd_.clear();
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
		if (materialManager)
		{
			animatedCube_->SetReflectionFactor(materialManager->GetMaterialReflectionFactor());
			animatedCube_->SetFresnelF0(materialManager->GetMaterialFresnelF0());
		}
		animatedCube_->Update(); // ステップ1〜4はエンジン層で実行される
	}

	for (auto& obj : crowd_)
	{
		Vector3 rotate = obj->GetRotate();
		rotate.y += 0.01f;
		obj->SetRotate(rotate);
		if (materialManager)
		{
			obj->SetReflectionFactor(materialManager->GetMaterialReflectionFactor());
			obj->SetFresnelF0(materialManager->GetMaterialFresnelF0());
		}
		obj->Update();
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

	// スプライトの毎フレーム更新
	if (spriteManager_)
	{
		spriteManager_->Update();
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
	if (spriteManager_)
	{
		spriteManager_->DrawAll(&debugCamera_, &sprites);
	}
	else
	{

		for (auto& sprite : sprites)
		{
			if (sprite)
			{
				sprite->Update();
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
		animatedCube_->Draw(object3dCom, skinningObject3dCom);
	}

	for (auto& obj : crowd_)
	{
		obj->Draw(object3dCom, skinningObject3dCom);
	}
}
