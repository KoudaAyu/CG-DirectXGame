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
#include "../../Player/Player.h"
#include "../../Enemy/Enemy.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <Windows.h>

#include "GamePlayScene.h"

void GamePlayScene::Initialize(DirectXCom* dxCommon, Camera* camera)
{
	camera_ = camera;
	assert(dxCommon != nullptr);
	this->directXCom = dxCommon;
	playerShotCooldownTimer_ = 0.0f;


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

	// Enemyの初期化
	if (!enemy_)
	{
		enemy_ = std::make_unique<Enemy>();
		enemy_->Initialize(object3dCom, camera_);
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

	// マウス入力初期化とカーソルスプライトの生成
	if (directXCom)
	{
		mouseInput.Initialize(directXCom->GetWindowAPI());
		// SpriteCom がメンバにセットされていない可能性があるため、SceneManager 経由で取得する
		SpriteCom* sc = spriteCom ? spriteCom : SceneManager::GetInstance()->GetSpriteCom();
		if (sc)
		{
			// 保持していなければメンバに設定
			if (!spriteCom) spriteCom = sc;
			// 小さなカーソル用スプライトを追加
			Sprite::Transform tc = { {1.0f,1.0f,1.0f}, {0.0f,0.0f,0.0f}, {0.0f,0.0f,0.0f} };
			if (auto cursor = Sprite::Create(sc, tc, "Resources/CG4/circle2.png"))
			{
				cursor->SetSize({ 24.0f, 24.0f });
                // カーソル用テクスチャの左上がマウス位置に対応するよう、アンカーポイントを左上に設定
				cursor->SetAnchorPoint({ 0.0f, 0.0f });
				sprites.emplace_back(std::move(cursor));
				cursorSpriteIndex = static_cast<int>(sprites.size()) - 1;
			}
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

	for (auto& bullet : bullets_)
	{
		if (bullet)
		{
			bullet->Finalize();
		}
	}
	bullets_.clear();

	// release player if created
	if (player_)
	{
		player_->Finalize();
		player_.reset();
	}

	// release enemy if created
	if (enemy_)
	{
		enemy_->Finalize();
		enemy_.reset();
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

	// マウス更新とカーソルスプライトの位置反映
	mouseInput.Update();
	if (cursorSpriteIndex >= 0 && cursorSpriteIndex < static_cast<int>(sprites.size()))
	{
		auto* cur = sprites[cursorSpriteIndex].get();
		if (cur)
		{
			Vector2 pos{ static_cast<float>(mouseInput.GetX()), static_cast<float>(mouseInput.GetY()) };
            // マウスのクライアント座標をスプライト投影空間（WindowAPIの定数）にスケーリング
			if (directXCom && directXCom->GetWindowAPI())
			{
				RECT rc{};
				if (GetClientRect(directXCom->GetWindowAPI()->GetHwnd(), &rc))
				{
					float clientW = float(rc.right - rc.left);
					float clientH = float(rc.bottom - rc.top);
					if (clientW > 0.0f && clientH > 0.0f)
					{
						float sx = float(directXCom->GetWindowAPI()->GetClientWidth()) / clientW;
						float sy = float(directXCom->GetWindowAPI()->GetClientHeight()) / clientH;
						pos.x *= sx;
						pos.y *= sy;
					}
				}
			}
			cur->SetPosition(pos);
            // 重いスプライトごとのメタデータ処理を避けるための軽量な変換更新
			cur->UpdateTransformOnly(directXCom ? directXCom->GetWindowAPI() : nullptr);
            // 左ボタン押下中は赤にする
			if (mouseInput.PushButton(0))
			{
				cur->SetColor({ 1.0f, 0.0f, 0.0f, 1.0f });
			}
			else
			{
				cur->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
			}
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

	// Player update (pass mouse input so player can face cursor)
	if (player_)
	{
		player_->Update(&mouseInput);
	}

	if (playerShotCooldownTimer_ > 0.0f)
	{
		playerShotCooldownTimer_ -= kDeltaTime;
		if (playerShotCooldownTimer_ < 0.0f)
		{
			playerShotCooldownTimer_ = 0.0f;
		}
	}

	if (player_ && mouseInput.PushButton(0) && playerShotCooldownTimer_ <= 0.0f)
	{
		Vector3 playerPos = player_->GetPosition();
		Vector3 playerRot = player_->GetRotation();
		const float yaw = playerRot.y;
		Vector3 forward = { std::sin(yaw), 0.0f, std::cos(yaw) };
		Vector3 right = { forward.z, 0.0f, -forward.x };
		Vector3 spawnPos = {
			playerPos.x + right.x * bulletSpawnOffset_.x + forward.x * bulletSpawnOffset_.z,
			playerPos.y + bulletSpawnOffset_.y,
			playerPos.z + right.z * bulletSpawnOffset_.x + forward.z * bulletSpawnOffset_.z
		};

		auto bullet = std::make_unique<Bullet>();
		bullet->Initialize(object3dCom, camera_, spawnPos, forward, bulletSpeed_, bulletLifeTime_, BulletOwner::Player);
		bullets_.emplace_back(std::move(bullet));
		playerShotCooldownTimer_ = playerShotCooldown_;
	}

	for (auto& bullet : bullets_)
	{
		if (bullet)
		{
			bullet->Update(kDeltaTime);
		}
	}

	for (auto& bullet : bullets_)
	{
		if (!bullet || bullet->IsDead())
		{
			continue;
		}

		const Vector3 bulletPos = bullet->GetPosition();
		if (bullet->GetOwner() == BulletOwner::Player && enemy_)
		{
			const Vector3 enemyPos = enemy_->GetPosition();
			const float dx = bulletPos.x - enemyPos.x;
			const float dy = bulletPos.y - enemyPos.y;
			const float dz = bulletPos.z - enemyPos.z;
			const float r = bulletHitRadius_ + enemyHitRadius_;
			if ((dx * dx + dy * dy + dz * dz) <= (r * r))
			{
				if (hitEffect_)
				{
					hitEffect_->Play(enemyPos);
				}
				enemy_->OnHit();
				bullet->Finalize();
			}
		}
		else if (bullet->GetOwner() == BulletOwner::Enemy && player_)
		{
			const Vector3 playerPos = player_->GetPosition();
			const float dx = bulletPos.x - playerPos.x;
			const float dy = bulletPos.y - playerPos.y;
			const float dz = bulletPos.z - playerPos.z;
			const float r = bulletHitRadius_ + playerHitRadius_;
			if ((dx * dx + dy * dy + dz * dz) <= (r * r))
			{
				if (hitEffect_)
				{
					hitEffect_->Play(playerPos);
				}
				bullet->Finalize();
			}
		}
	}

	bullets_.erase(
		std::remove_if(bullets_.begin(), bullets_.end(), [](std::unique_ptr<Bullet>& bullet)
			{
				return !bullet || bullet->IsDead();
			}),
		bullets_.end());

	// Enemy update
	if (enemy_)
	{
		enemy_->Update();
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

	if (player_)
	{
		player_->Draw(ctx);
	}

	for (auto& bullet : bullets_)
	{
		if (bullet)
		{
			bullet->Draw(ctx);
		}
	}

	if (enemy_)
	{
		enemy_->Draw(ctx);
	}

	if (spriteManager_)
	{
		spriteManager_->DrawAll(ctx, &debugCamera_, &sprites, false);
	}
}