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
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <cmath>
#include <unordered_set>
#include <cassert>
#include <Windows.h>

namespace
{
  Vector3 Subtract(const Vector3& a, const Vector3& b)
	{
		return { a.x - b.x, a.y - b.y, a.z - b.z };
	}

	float Length(const Vector3& v)
	{
		return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
	}

	Vector3 Normalize(const Vector3& v)
	{
		const float length = Length(v);
		if (length <= 0.0001f)
		{
			return { 0.0f, 1.0f, 0.0f };
		}

		const float invLength = 1.0f / length;
		return { v.x * invLength, v.y * invLength, v.z * invLength };
	}

	Vector3 Cross(const Vector3& a, const Vector3& b)
	{
		return {
			a.y * b.z - a.z * b.y,
			a.z * b.x - a.x * b.z,
			a.x * b.y - a.y * b.x
		};
	}

	Matrix4x4 MakeBoneSegmentMatrix(const Vector3& start, const Vector3& end, float radius)
	{
		const Vector3 direction = Subtract(end, start);
		const float length = Length(direction);
		const Vector3 yAxis = Normalize(direction);

		Vector3 referenceAxis = { 0.0f, 0.0f, 1.0f };
		if (std::fabs(yAxis.z) > 0.99f)
		{
			referenceAxis = { 1.0f, 0.0f, 0.0f };
		}

		const Vector3 xAxis = Normalize(Cross(referenceAxis, yAxis));
		const Vector3 zAxis = Normalize(Cross(yAxis, xAxis));

		Matrix4x4 rotateMatrix = MakeIdentity4x4();
		rotateMatrix.m[0][0] = xAxis.x;
		rotateMatrix.m[0][1] = xAxis.y;
		rotateMatrix.m[0][2] = xAxis.z;
		rotateMatrix.m[1][0] = yAxis.x;
		rotateMatrix.m[1][1] = yAxis.y;
		rotateMatrix.m[1][2] = yAxis.z;
		rotateMatrix.m[2][0] = zAxis.x;
		rotateMatrix.m[2][1] = zAxis.y;
		rotateMatrix.m[2][2] = zAxis.z;

		return MakeAffineMatrix({ radius, length, radius }, rotateMatrix, start);
	}

    std::unordered_set<std::string> CollectBoneNames(const aiScene* scene)
	{
      std::unordered_set<std::string> boneNames;
		if (!scene)
		{
          return boneNames;
		}

     for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
		{
			const aiMesh* mesh = scene->mMeshes[meshIndex];
			if (!mesh)
			{
				continue;
			}

          for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex)
			{
				const aiBone* bone = mesh->mBones[boneIndex];
				if (bone)
				{
					boneNames.insert(bone->mName.C_Str());
				}
			}
		}

		return boneNames;
	}

	bool ConvertAssimpNodeToAnimNode(const aiNode* node, const std::unordered_set<std::string>& boneNames, AnimNode& outNode)
	{
		if (!node)
		{
			return false;
		}

		aiVector3D scale{};
		aiQuaternion rotate{};
		aiVector3D translate{};
		node->mTransformation.Decompose(scale, rotate, translate);

		AnimNode result{};
		result.name = node->mName.C_Str();
		result.transform.scale = { scale.x, scale.y, scale.z };
		result.transform.rotate = Quaternion(rotate.x, rotate.y, rotate.z, rotate.w);
		result.transform.translate = { translate.x, translate.y, translate.z };
		result.localMatrix = result.transform.MakeLocalMatrix();

		const bool isBoneNode = boneNames.contains(result.name);
		for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex)
		{
			AnimNode childNode{};
			if (ConvertAssimpNodeToAnimNode(node->mChildren[childIndex], boneNames, childNode))
			{
				result.children.push_back(std::move(childNode));
			}
		}

        if (!isBoneNode && result.children.empty())
		{
            return false;
		}

      outNode = std::move(result);
		return true;
	}
}



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
		animationTime_ = 0.0f;
		animationInitialized_ = animation_.duration > 0.0f && !animation_.nodeAnimations.empty();

		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile("Resources/CG4/human/walk.gltf", aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals);
		if (scene && scene->mRootNode)
		{
          const std::unordered_set<std::string> boneNames = CollectBoneNames(scene);
			SkeletonLoader skeletonLoader;
			AnimNode rootNode{};
			if (ConvertAssimpNodeToAnimNode(scene->mRootNode, boneNames, rootNode))
			{
				skeleton_ = skeletonLoader.CreateSkeleton(rootNode);
				skeleton_.Update();

             jointDebugSpheres_.clear();
                jointDebugCylinders_.clear();
				jointDebugSpheres_.reserve(skeleton_.joints.size());
             jointDebugCylinders_.reserve(skeleton_.joints.size());
				for (size_t jointIndex = 0; jointIndex < skeleton_.joints.size(); ++jointIndex)
				{
					auto jointSphere = std::make_unique<Sphere>();
					jointSphere->Initialize(directXCom, object3dCom, materialManager, light, camera_);
					jointSphere->SetOverlayDraw(true);
					Sprite::Transform jointTransform = jointSphere->GetTransform();
                 jointTransform.scale = { 0.06f, 0.06f, 0.06f };
					jointSphere->SetTransform(jointTransform);
					jointDebugSpheres_.push_back(std::move(jointSphere));

					auto jointCylinder = std::make_unique<Cylinder>();
					jointCylinder->Initialize(directXCom, object3dCom, materialManager, light, camera_, 12, 1.0f, 1.0f, 1.0f);
					jointCylinder->SetOverlayDraw(true);
					jointDebugCylinders_.push_back(std::move(jointCylinder));
				}

             skeletonInitialized_ = !jointDebugSpheres_.empty();
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
		animatedCube_->Update();
	}

	if (skeletonInitialized_ && animatedCube_)
	{
     if (animationInitialized_)
		{
			animationTime_ += kDeltaTime;
			if (animation_.duration > 0.0f)
			{
				while (animationTime_ >= animation_.duration)
				{
					animationTime_ -= animation_.duration;
				}
			}

			ApplyAnimation(skeleton_, animation_, animationTime_);
		}

		skeleton_.Update();

		const Matrix4x4 modelWorldMatrix = MakeAffineMatrix(
			animatedCube_->GetScale(),
			animatedCube_->GetRotate(),
			animatedCube_->GetTranslate());

		const size_t debugSphereCount = (std::min)(skeleton_.joints.size(), jointDebugSpheres_.size());
		for (size_t jointIndex = 0; jointIndex < debugSphereCount; ++jointIndex)
		{
			const Matrix4x4 jointWorldMatrix = Multiply(skeleton_.joints[jointIndex].skeletonMatrix, modelWorldMatrix);
          const Vector3 jointPosition = {
				jointWorldMatrix.m[3][0],
				jointWorldMatrix.m[3][1],
				jointWorldMatrix.m[3][2]
			};
			Sprite::Transform jointTransform = jointDebugSpheres_[jointIndex]->GetTransform();
            jointTransform.translate = jointPosition;
			jointDebugSpheres_[jointIndex]->SetTransform(jointTransform);
			jointDebugSpheres_[jointIndex]->Update();

			if (jointIndex < jointDebugCylinders_.size() && jointDebugCylinders_[jointIndex])
			{
				if (skeleton_.joints[jointIndex].parent)
				{
					const int32_t parentIndex = *skeleton_.joints[jointIndex].parent;
					const Matrix4x4 parentWorldMatrix = Multiply(skeleton_.joints[parentIndex].skeletonMatrix, modelWorldMatrix);
					const Vector3 parentPosition = {
						parentWorldMatrix.m[3][0],
						parentWorldMatrix.m[3][1],
						parentWorldMatrix.m[3][2]
					};
                 jointDebugCylinders_[jointIndex]->SetWorldMatrix(MakeBoneSegmentMatrix(parentPosition, jointPosition, 0.03f));
					jointDebugCylinders_[jointIndex]->Update();
				}
			}
		}
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

	if (skeletonInitialized_)
	{
      if (particleTextureA != TextureManager::kInvalidTextureIndex)
		{
			const D3D12_GPU_DESCRIPTOR_HANDLE debugTextureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(particleTextureA);
			for (size_t jointIndex = 0; jointIndex < jointDebugCylinders_.size(); ++jointIndex)
			{
				if (skeleton_.joints[jointIndex].parent && jointDebugCylinders_[jointIndex])
				{
					jointDebugCylinders_[jointIndex]->Draw(debugTextureHandle);
				}
			}
		}

		for (const auto& jointSphere : jointDebugSpheres_)
		{
			renderRequests.spheres.Request(jointSphere.get());
		}
	}

	if (animatedCubeInitialized_ && animatedCube_ && object3dCom)
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

		object3dCom->Draw(animatedCube_.get(), ctx, modelData, true);
	}

}