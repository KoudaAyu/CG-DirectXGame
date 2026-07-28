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
#include <Xinput.h>
#include <cmath>
#ifdef USE_IMGUI
#include <imgui.h>
#endif
#pragma comment(lib, "xinput.lib")

namespace
{
	Matrix4x4 ExtractRotationAndTranslation(const Matrix4x4& mat)
	{
		Matrix4x4 result = MakeIdentity4x4();

		Vector3 xAxis = { mat.m[0][0], mat.m[0][1], mat.m[0][2] };
		Vector3 yAxis = { mat.m[1][0], mat.m[1][1], mat.m[1][2] };
		Vector3 zAxis = { mat.m[2][0], mat.m[2][1], mat.m[2][2] };

		float xLen = std::sqrt(xAxis.x * xAxis.x + xAxis.y * xAxis.y + xAxis.z * xAxis.z);
		float yLen = std::sqrt(yAxis.x * yAxis.x + yAxis.y * yAxis.y + yAxis.z * yAxis.z);
		float zLen = std::sqrt(zAxis.x * zAxis.x + zAxis.y * zAxis.y + zAxis.z * zAxis.z);

		if (xLen > 0.0001f) { xAxis.x /= xLen; xAxis.y /= xLen; xAxis.z /= xLen; }
		if (yLen > 0.0001f) { yAxis.x /= yLen; yAxis.y /= yLen; yAxis.z /= yLen; }
		if (zLen > 0.0001f) { zAxis.x /= zLen; zAxis.y /= zLen; zAxis.z /= zLen; }

		result.m[0][0] = xAxis.x; result.m[0][1] = xAxis.y; result.m[0][2] = xAxis.z;
		result.m[1][0] = yAxis.x; result.m[1][1] = yAxis.y; result.m[1][2] = yAxis.z;
		result.m[2][0] = zAxis.x; result.m[2][1] = zAxis.y; result.m[2][2] = zAxis.z;

		result.m[3][0] = mat.m[3][0];
		result.m[3][1] = mat.m[3][1];
		result.m[3][2] = mat.m[3][2];

		return result;
	}

	Object3d::ModelData CreateSwordModelData()
	{
		Object3d::ModelData data;
		// 剣の形の直方体 (幅 0.08m, 高さ 0.8m, 奥行き 0.08m)
		float w = 0.08f;
		float h = 0.8f;
		float d = 0.08f;

		Vector4 pos[8] = {
			{ -w, 0.0f, -d, 1.0f }, { -w,  h, -d, 1.0f }, {  w,  h, -d, 1.0f }, {  w, 0.0f, -d, 1.0f },
			{ -w, 0.0f,  d, 1.0f }, { -w,  h,  d, 1.0f }, {  w,  h,  d, 1.0f }, {  w, 0.0f,  d, 1.0f }
		};
		Vector2 uvs[4] = { {0,1}, {0,0}, {1,0}, {1,1} };

		auto addQuad = [&](int i0, int i1, int i2, int i3, Vector3 normal) {
			uint32_t base = static_cast<uint32_t>(data.vertices.size());
			data.vertices.push_back({ pos[i0], uvs[0], normal });
			data.vertices.push_back({ pos[i1], uvs[1], normal });
			data.vertices.push_back({ pos[i2], uvs[2], normal });
			data.vertices.push_back({ pos[i3], uvs[3], normal });

			data.indices.push_back(base + 0);
			data.indices.push_back(base + 1);
			data.indices.push_back(base + 2);
			data.indices.push_back(base + 0);
			data.indices.push_back(base + 2);
			data.indices.push_back(base + 3);
		};

		addQuad(0, 1, 2, 3, { 0, 0,-1 });
		addQuad(7, 6, 5, 4, { 0, 0, 1 });
		addQuad(4, 5, 1, 0, {-1, 0, 0 });
		addQuad(3, 2, 6, 7, { 1, 0, 0 });
		addQuad(1, 5, 6, 2, { 0, 1, 0 });
		addQuad(4, 0, 3, 7, { 0,-1, 0 });

		data.material.textureFilePath = "Resources/uvChecker.png";
		data.material.textureIndex = TextureManager::GetInstance()->Load("Resources/uvChecker.png");

		return data;
	}
}

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
		sneakWalkAnimation_ = LoadAnimationFile("Resources/CG4/human", "sneakWalk.gltf");
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

		// MultiMesh & MultiMaterial 検証モデル (multiMesh.obj: Plane + Cube の複数メッシュ構造)
		Model::ModelData multiModelData = Model::LoadModelFile("Resources", "multiMesh.obj");
		if (!multiModelData.vertices.empty())
		{
			Object3d::ModelData multiObjModelData;
			multiObjModelData.vertices = multiModelData.vertices;
			multiObjModelData.indices = multiModelData.indices;
			multiObjModelData.material.textureFilePath = multiModelData.material.textureFilePath;
			multiObjModelData.material.textureIndex = multiModelData.material.textureIndex;
			multiObjModelData.meshes = multiModelData.meshes;
			multiObjModelData.materials.resize(multiModelData.materials.size());
			for (size_t i = 0; i < multiModelData.materials.size(); ++i)
			{
				multiObjModelData.materials[i].textureFilePath = multiModelData.materials[i].textureFilePath;
				multiObjModelData.materials[i].textureIndex = multiModelData.materials[i].textureIndex;
			}

			multiMeshObject_ = std::make_unique<Object3d>();
			multiMeshObject_->Initialize(object3dCom, multiObjModelData);
			multiMeshObject_->SetTranslate({ -4.0f, 0.0f, 0.0f });
			multiMeshObject_->SetScale({ 1.0f, 1.0f, 1.0f });
			multiMeshObject_->SetEnableLighting(true);
			multiMeshObjectInitialized_ = true;
		}

		// 武器オブジェクトの初期化 (手にアタッチする用モデル)
		Object3d::ModelData weaponObjModelData = CreateSwordModelData();
		weaponObject_ = std::make_unique<Object3d>();
		weaponObject_->Initialize(object3dCom, weaponObjModelData);
		weaponObject_->SetEnableLighting(true);
		weaponObject_->SetColor({ 1.0f, 0.85f, 0.2f, 1.0f }); // 視認性の高いゴールドカラー
		weaponObjectInitialized_ = true;
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
		Vector3 moveDir = { 0.0f, 0.0f, 0.0f };

		// 1. パッド（XInput コントローラー）入力の取得
		XINPUT_STATE xinputState{};
		if (XInputGetState(0, &xinputState) == ERROR_SUCCESS)
		{
			float lx = static_cast<float>(xinputState.Gamepad.sThumbLX);
			float ly = static_cast<float>(xinputState.Gamepad.sThumbLY);
			if (std::abs(lx) > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)
			{
				moveDir.x += lx / 32767.0f;
			}
			if (std::abs(ly) > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)
			{
				moveDir.z += ly / 32767.0f;
			}
		}

		// 2. キーボード入力 (WASD / 矢印キー) の取得
		if ((GetAsyncKeyState('W') & 0x8000) || (GetAsyncKeyState(VK_UP) & 0x8000)) moveDir.z += 1.0f;
		if ((GetAsyncKeyState('S') & 0x8000) || (GetAsyncKeyState(VK_DOWN) & 0x8000)) moveDir.z -= 1.0f;
		if ((GetAsyncKeyState('A') & 0x8000) || (GetAsyncKeyState(VK_LEFT) & 0x8000)) moveDir.x -= 1.0f;
		if ((GetAsyncKeyState('D') & 0x8000) || (GetAsyncKeyState(VK_RIGHT) & 0x8000)) moveDir.x += 1.0f;

		// 3. 移動と回転の適用
		float lenSq = moveDir.x * moveDir.x + moveDir.z * moveDir.z;
		if (lenSq > 0.001f)
		{
			float len = std::sqrt(lenSq);
			moveDir.x /= len;
			moveDir.z /= len;

			float moveSpeed = 4.0f * kDeltaTime;
			Vector3 pos = animatedCube_->GetTranslate();
			pos.x += moveDir.x * moveSpeed;
			pos.z += moveDir.z * moveSpeed;
			animatedCube_->SetTranslate(pos);

			// 移動方向を向く（Y軸回転）
			float targetAngle = std::atan2(moveDir.x, moveDir.z);
			animatedCube_->SetRotate({ 0.0f, targetAngle, 0.0f });
		}

		// 4. アニメーション補間（クロスフェード）切り替え: Shiftキー / Spaceキー / パッドBボタン押下で「歩き (walk)」と「忍び足 (sneakWalk)」を0.5秒かけて滑らかに補間切替
		bool isSneakRequested = (GetAsyncKeyState(VK_SHIFT) & 0x8000) || (GetAsyncKeyState(VK_SPACE) & 0x8000);
		if (XInputGetState(0, &xinputState) == ERROR_SUCCESS)
		{
			if (xinputState.Gamepad.wButtons & (XINPUT_GAMEPAD_B | XINPUT_GAMEPAD_A))
			{
				isSneakRequested = true;
			}
		}

		if (isSneakRequested && sneakWalkAnimation_.duration > 0.0f)
		{
			animatedCube_->PlayAnimation(&sneakWalkAnimation_, 0.5f);
		}
		else if (animation_.duration > 0.0f)
		{
			animatedCube_->PlayAnimation(&animation_, 0.5f);
		}

		animatedCube_->Update(); // ステップ1〜4はエンジン層で実行される

		if (isHeadLookAtEnabled_ && headLookAtMode_ > 0)
		{
			Vector3 targetPos = headLookAtTargetPos_;
			if (headLookAtMode_ == 1 && camera_)
			{
				targetPos = camera_->GetTranslate();
			}
			animatedCube_->ApplyHeadLookAt(targetPos, headLookAtWeight_);
		}
	}

	if (multiMeshObjectInitialized_ && multiMeshObject_)
	{
		Vector3 rot = multiMeshObject_->GetRotate();
		rot.y += 0.01f;
		multiMeshObject_->SetRotate(rot);
		multiMeshObject_->Update();
	}

	if (animatedCube_)
	{
		const Matrix4x4 modelWorldMatrix = MakeAffineMatrix(
			animatedCube_->GetScale(),
			animatedCube_->GetRotate(),
			animatedCube_->GetTranslate());

		if (skeletonDebug_.IsInitialized())
		{
			skeletonDebug_.Sync(animatedCube_->GetSkeleton(), modelWorldMatrix);
		}

		// 手のボーン (RightHand / Hand_R) のワールド位置を取得して追従パーティクル発射
		static bool prevKeyE = false;
		bool curKeyE = (GetAsyncKeyState('E') & 0x8000) != 0 || (GetAsyncKeyState('e') & 0x8000) != 0;

		if (curKeyE && !prevKeyE)
		{
			isHandParticleEmitting_ = !isHandParticleEmitting_;
		}
		prevKeyE = curKeyE;

		if (particleManager)
		{
			if (auto* gpuEmitter = particleManager->GetGPUEmitter())
			{
				if (auto* data = gpuEmitter->GetEmitterData())
				{
					data->emit = isHandParticleEmitting_ ? 1 : 0;
				}
			}
		}

		if (isHandParticleEmitting_ && particleManager)
		{
			const Skeleton& skeleton = animatedCube_->GetSkeleton();
			int32_t handJointIndex = -1;

			// 1. スケルトン内のジョイントから「RightHand」「Hand_R」等を優先検索（腕や肩は除外）
			for (size_t i = 0; i < skeleton.joints.size(); ++i)
			{
				const std::string& jName = skeleton.joints[i].name;
				if (jName.find("ForeArm") != std::string::npos ||
					jName.find("Arm") != std::string::npos ||
					jName.find("Shoulder") != std::string::npos)
				{
					continue; // 腕や肩はスキップ！
				}

				if (jName.find("RightHand") != std::string::npos ||
					jName.find("Hand_R") != std::string::npos ||
					jName.find("Hand.R") != std::string::npos ||
					jName.find("mixamorig:RightHand") != std::string::npos ||
					jName.find("Right_Hand") != std::string::npos)
				{
					handJointIndex = static_cast<int32_t>(i);
					break;
				}
			}

			// フォールバック: 全ジョイントから「Hand」かつ「Arm」でないものを検索
			if (handJointIndex < 0)
			{
				for (size_t i = 0; i < skeleton.joints.size(); ++i)
				{
					const std::string& jName = skeleton.joints[i].name;
					if (jName.find("Arm") == std::string::npos && (jName.find("Hand") != std::string::npos || jName.find("hand") != std::string::npos))
					{
						handJointIndex = static_cast<int32_t>(i);
						break;
					}
				}
			}

			if (handJointIndex >= 0)
			{
				Vector3 handWorldPos = skeleton.GetJointWorldPosition(handJointIndex, modelWorldMatrix);
				handEmitter_.transform.SetTranslate(handWorldPos);
				handEmitter_.count = 12;
				handEmitter_.frequency = 0.016f;
				handEmitter_.frequencyTime += kDeltaTime;

				if (handEmitter_.frequencyTime >= handEmitter_.frequency)
				{
					auto handParticles = particleEmitter.Emit(handEmitter_, particleManager->GetRandomEngine(), *particleManager);
					for (auto& p : handParticles)
					{
						p.textureIndex = (particleTextureB != TextureManager::kInvalidTextureIndex) ? particleTextureB : 0;
						p.transform.SetScale({ 0.08f, 0.08f, 0.08f }); // 手の平サイズの繊細な魔法粒子
						p.color = { 1.0f, 0.65f, 0.15f, 1.0f }; // 発光感のある黄金〜火炎カラー
					}
					particleManager->AddParticles(handParticles);
					handEmitter_.frequencyTime -= handEmitter_.frequency;
				}

				// GPU エミッターも右手のワールド位置へ動的追従
				if (auto* gpuEmitter = particleManager->GetGPUEmitter())
				{
					if (auto* data = gpuEmitter->GetEmitterData())
					{
						data->translate = handWorldPos;
						data->radius = 0.03f; // 手の平サイズ (3cm) の高密度発生
						data->count = 8;
						data->emit = 1;
					}
				}
			}
		}

		// 武器オブジェクトの手ボーンアタッチ処理 (Item 7: 武器を手に持たせる - エンジンAPI AttachToJoint 1行で完結)
		if (weaponObjectInitialized_ && weaponObject_ && animatedCube_)
		{
			if (isWeaponAttached_)
			{
				weaponObject_->AttachToJoint(*animatedCube_, selectedJointIndex_, weaponOffsetScale_, weaponOffsetRotate_, weaponOffsetTranslate_);
			}
			else
			{
				weaponObject_->ClearCustomWorldMatrix();
			}
			weaponObject_->Update();
		}
	}

   if (hitEffectInitialized && hitEffect_)
	{
        hitEffect_->SetPlaneParticleCount(emitter.count);
       hitEffect_->Update(kDeltaTime);
	}

	// （静的な原点背景エミッターは手のパーティクルを明確に確認できるよう無効化）

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
			skinningObject3dCom->PreDraw();
			skinningObject3dCom->Draw(animatedCube_.get(), ctx, modelData, true);
		}
		else if (object3dCom)
		{
			object3dCom->PreDraw();
			object3dCom->Draw(animatedCube_.get(), ctx, modelData, true);
		}
	}

	if (multiMeshObjectInitialized_ && multiMeshObject_ && object3dCom)
	{
		const auto& modelData = multiMeshObject_->GetModelData();
		RenderContext multiCtx = ctx;
		if (modelData.material.textureIndex != TextureManager::kInvalidTextureIndex)
		{
			multiCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(modelData.material.textureIndex);
		}
		object3dCom->PreDraw();
		object3dCom->Draw(multiMeshObject_.get(), multiCtx, modelData, true);
	}

	if (weaponObjectInitialized_ && weaponObject_ && object3dCom)
	{
		const auto& modelData = weaponObject_->GetModelData();
		RenderContext weaponCtx = ctx;
		uint32_t texIdx = modelData.material.textureIndex;
		if (texIdx == TextureManager::kInvalidTextureIndex)
		{
			texIdx = TextureManager::GetInstance()->Load("Resources/uvChecker.png");
		}
		if (texIdx != TextureManager::kInvalidTextureIndex)
		{
			weaponCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(texIdx);
		}
		object3dCom->PreDraw();
		object3dCom->Draw(weaponObject_.get(), weaponCtx, modelData, true);
	}

}

void GamePlayScene::DrawUI()
{
	if (skeletonDebug_.IsInitialized() && showSkeletonDebug_)
	{
		skeletonDebug_.DrawUI(camera_);
	}

#ifdef USE_IMGUI
	if (ImGui::GetCurrentContext())
	{
		ImGui::Begin("Hand Particle Control (Item 6)");
		ImGui::Text("Item 6: 手からパーティクルを出す");
		ImGui::Checkbox("手から魔法パーティクル発生 (Eキーで切替)", &isHandParticleEmitting_);
		ImGui::Text("ステータス: %s", isHandParticleEmitting_ ? "ON (右手ボーンにリアルタイム追従発射中)" : "OFF (キー E を押してON)");
		ImGui::End();

		ImGui::Begin("Weapon Hand Attachment (Item 7)");
		ImGui::Text("Item 7: 武器を手に持たせる (10点)");
		ImGui::Checkbox("武器を手ボーンにアタッチ", &isWeaponAttached_);

		if (animatedCube_)
		{
			const Skeleton& skeleton = animatedCube_->GetSkeleton();
			if (!skeleton.joints.empty())
			{
				if (selectedJointIndex_ < 0 || selectedJointIndex_ >= static_cast<int32_t>(skeleton.joints.size()))
				{
					selectedJointIndex_ = 0;
				}

				std::string currentJointName = skeleton.joints[selectedJointIndex_].name;
				if (ImGui::BeginCombo("アタッチ対象ボーン", currentJointName.c_str()))
				{
					for (int32_t n = 0; n < static_cast<int32_t>(skeleton.joints.size()); n++)
					{
						const bool isSelected = (selectedJointIndex_ == n);
						std::string label = std::to_string(n) + ": " + skeleton.joints[n].name;
						if (ImGui::Selectable(label.c_str(), isSelected))
						{
							selectedJointIndex_ = n;
						}
						if (isSelected)
						{
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}
			}
		}

		ImGui::Separator();
		ImGui::Text("持ち手ローカルオフセット調整:");
		ImGui::DragFloat3("オフセット位置 (Translate)", &weaponOffsetTranslate_.x, 0.05f);
		ImGui::DragFloat3("オフセット回転 (Rotate)", &weaponOffsetRotate_.x, 0.05f);
		ImGui::DragFloat3("オフセットスケール (Scale)", &weaponOffsetScale_.x, 0.05f);
		ImGui::End();

		// エンジンカプセル化 API: GPU Particle Studio / Editor
		if (particleManager)
		{
			particleManager->DrawUI("GPU Particle Studio / Editor (Item 8: 20点)");
		}

		// エンジンカプセル化 API: Animation Timeline & Control
		if (animatedCube_)
		{
			animatedCube_->DrawAnimationUI("Animation Timeline & Control (Item 9: 30点)");
		}

		ImGui::Begin("Procedural Head LookAt Studio (Item 10: 30点)");
		ImGui::Text("Item 10: プロシージャル Head LookAt 視線・頭部動的追従");
		ImGui::Checkbox("Head LookAt 視線追従 有効", &isHeadLookAtEnabled_);

		const char* lookAtModeNames[] = { "0: Off (無効)", "1: LookAt Camera (カメラを自動凝視)", "2: Custom Target (カスタム座標指定)" };
		ImGui::Combo("LookAt モード", &headLookAtMode_, lookAtModeNames, IM_ARRAYSIZE(lookAtModeNames));

		ImGui::SliderFloat("追従ウェイト (Weight)", &headLookAtWeight_, 0.0f, 1.0f, "%.2f");

		if (headLookAtMode_ == 2)
		{
			ImGui::DragFloat3("カスタムターゲット座標", &headLookAtTargetPos_.x, 0.1f);
		}
		else if (headLookAtMode_ == 1 && camera_)
		{
			Vector3 camPos = camera_->GetTranslate();
			ImGui::Text("カメラ座標 (追従中): (%.2f, %.2f, %.2f)", camPos.x, camPos.y, camPos.z);
		}
		ImGui::End();
	}
#endif
}
