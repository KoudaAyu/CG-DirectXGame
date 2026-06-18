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
#include "CustomObject3dRenderer.h"

void GamePlayScene::Initialize(DirectXCom* dxCommon, Camera* camera)
{
	camera_ = camera;
	assert(dxCommon != nullptr);
	directXCom = dxCommon;
	CustomObject3dRenderer::GetInstance()->Initialize(dxCommon);
	lastTime_ = std::chrono::steady_clock::now();

	object3dCom = SceneManager::GetInstance()->GetObject3dCom();
	materialManager = SceneManager::GetInstance()->GetMaterialManager();
	light = SceneManager::GetInstance()->GetLight();
	particleManager = SceneManager::GetInstance()->GetParticleManager();

	InitializeEnvironment();
	InitializeCharacters();
	InitializeSprites();
	InitializeAudioAndParticles();
	InitializeObstacles();
}

void GamePlayScene::InitializeEnvironment()
{
	if (!object3dCom || !materialManager || !light || !particleManager)
	{
		return;
	}

	hitEffect_ = std::make_unique<HitEffect>();
	hitEffect_->Initialize(directXCom, object3dCom, materialManager, light, camera_, 64, 1.0f, 0.2f, 32, 1.0f, 1.0f, 3.0f);
	hitEffect_->SetParticleManager(particleManager);
	hitEffect_->SetCylinderEnabled(true);
	hitEffect_->SetRingEnabled(true);
	hitEffect_->SetEffectDuration(0.35f);
	hitEffect_->GetCylinderTransform() = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
	hitEffect_->Update(kFixedDeltaTime);
	hitEffectInitialized = true;

	sphere_ = std::make_unique<Sphere>();
	sphere_->Initialize(directXCom, object3dCom, materialManager, light, camera_);
	sphereInitialized = true;

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

	goalRing_ = std::make_unique<Ring>();
	goalRing_->Initialize(directXCom, object3dCom, materialManager, light, camera_, 64, 1.5f, 1.2f);
	goalRingTransform_.rotate = { 1.570796f, 0.0f, 0.0f };
	goalRingTransform_.scale = { 1.0f, 1.0f, 1.0f };
	goalRingTransform_.translate = { 0.0f, 0.01f, 10.0f };
	isGameCleared_ = false;
	extractionTimer_ = 5.0f;
}

void GamePlayScene::InitializeCharacters()
{
	if (!player_)
	{
		player_ = std::make_unique<Player>();
		player_->Initialize(object3dCom, camera_);
	}

	if (!enemy_)
	{
		enemy_ = std::make_unique<Enemy>();
		enemy_->Initialize(object3dCom, camera_);
	}
}

void GamePlayScene::InitializeSprites()
{
	if (!spriteManager_)
	{
		SpriteCom* sc = SceneManager::GetInstance()->GetSpriteCom();
		if (sc)
		{
			spriteManager_ = std::make_unique<SpriteManager>();
			spriteManager_->Initialize(sc, "Resources/uvChecker.png", 0);
		}
	}

	if (!directXCom)
	{
		return;
	}

	mouseInput.Initialize(directXCom->GetWindowAPI());
	SpriteCom* sc = spriteCom ? spriteCom : SceneManager::GetInstance()->GetSpriteCom();
	if (!sc)
	{
		return;
	}

	if (!spriteCom)
	{
		spriteCom = sc;
	}

	Sprite::Transform defaultTransform = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
	if (auto cursor = Sprite::Create(sc, defaultTransform, "Resources/CG4/circle2.png"))
	{
		cursor->SetSize({ 24.0f, 24.0f });
		cursor->SetAnchorPoint({ 0.5f, 0.5f });
		sprites.emplace_back(std::move(cursor));
		cursorSpriteIndex = static_cast<int>(sprites.size()) - 1;
	}

	auto hpBg = Sprite::Create(sc, defaultTransform, "Resources/CG4/human/white.png");
	auto hpFg = Sprite::Create(sc, defaultTransform, "Resources/CG4/human/white.png");
	if (hpBg && hpFg)
	{
		hpBg->SetAnchorPoint({ 0.0f, 0.0f });
		hpFg->SetAnchorPoint({ 0.0f, 0.0f });
		sprites.emplace_back(std::move(hpBg));
		sprites.emplace_back(std::move(hpFg));
		if (enemy_)
		{
			enemy_->SetHPBarSprites(sprites[sprites.size() - 2].get(), sprites[sprites.size() - 1].get());
		}
	}

	auto pBg = Sprite::Create(sc, defaultTransform, "Resources/CG4/human/white.png");
	auto pFg = Sprite::Create(sc, defaultTransform, "Resources/CG4/human/white.png");
	if (pBg && pFg)
	{
		pBg->SetAnchorPoint({ 0.0f, 0.0f });
		pFg->SetAnchorPoint({ 0.0f, 0.0f });
		pBg->SetPosition({ 20.0f, 20.0f });
		pBg->SetSize({ 200.0f, 16.0f });
		pBg->SetColor({ 0.1f, 0.1f, 0.1f, 0.8f });
		pFg->SetPosition({ 20.0f, 20.0f });
		pFg->SetSize({ 200.0f, 16.0f });
		pFg->SetColor({ 1.0f, 0.0f, 0.0f, 1.0f });
		sprites.emplace_back(std::move(pBg));
		sprites.emplace_back(std::move(pFg));
		playerHpBarBg_ = sprites[sprites.size() - 2].get();
		playerHpBarFg_ = sprites[sprites.size() - 1].get();
	}
}

void GamePlayScene::InitializeAudioAndParticles()
{
	if (auto am = SceneManager::GetInstance()->GetAudioManager())
	{
		if (int32_t id = am->Load("Resources/Alarm01.wav"); id >= 0)
		{
			am->Play(id);
		}
	}

	emitter.transform.SetTranslate({ 0.0f, 0.0f, 0.0f });
	emitter.transform.SetRotate({ 0.0f, 0.0f, 0.0f });
	emitter.transform.SetScale({ 1.0f, 1.0f, 1.0f });
	emitter.count = 3;
	emitter.frequency = 0.5f;
	emitter.frequencyTime = 0.0f;

	cylinderTextureIndex_ = TextureManager::GetInstance()->Load("Resources/CG4/gradationLine.png");
	particleTextureA = TextureManager::GetInstance()->Load("Resources/uvChecker.png");
	particleTextureB = TextureManager::GetInstance()->Load("Resources/CG4/circle2.png");
	fenceTextureIndex_ = TextureManager::GetInstance()->Load("Resources/fence.png");
	if (hitEffect_)
	{
		hitEffect_->SetPlaneParticleTextureIndex(particleTextureB);
		hitEffect_->SetPlaneParticleCount(emitter.count);
		hitEffect_->SetRingTextureIndex(particleTextureA);
		hitEffect_->SetTextureIndex(cylinderTextureIndex_);
	}
}

void GamePlayScene::Finalize()
{
	CustomObject3dRenderer::GetInstance()->Finalize();
	if (hitEffect_)
	{
		hitEffect_->Finalize();
		hitEffect_.reset();
	}
	hitEffectInitialized = false;

	if (goalRing_)
	{
		goalRing_->Finalize();
		goalRing_.reset();
	}

	for (auto& bullet : bullets_)
	{
		if (bullet)
		{
			bullet->Finalize();
		}
	}
	bullets_.clear();

	if (player_)
	{
		player_->Finalize();
		player_.reset();
	}

	if (enemy_)
	{
		enemy_->Finalize();
		enemy_.reset();
	}

	for (auto& obs : obstacles_)
	{
		if (obs)
		{
			obs->Finalize();
		}
	}
	obstacles_.clear();
}

float GamePlayScene::AdvanceDeltaTime()
{
	const auto now = std::chrono::steady_clock::now();
	float deltaTime = std::chrono::duration<float>(now - lastTime_).count();
	lastTime_ = now;
	if (deltaTime > 0.1f)
	{
		deltaTime = 0.1f;
	}
	return deltaTime;
}

void GamePlayScene::UpdateExtractionGoal(float deltaTime)
{
	if (goalRing_)
	{
		goalRingTransform_.rotate.y += 0.02f;
		goalRing_->SetTransform(goalRingTransform_);
		goalRing_->Update();
	}

	if (!player_)
	{
		return;
	}

	const Vector3 playerPos = player_->GetPosition();
	const Vector3 goalPos = goalRingTransform_.translate;
	const float dx = playerPos.x - goalPos.x;
	const float dz = playerPos.z - goalPos.z;
	const float dist = std::sqrt(dx * dx + dz * dz);
	constexpr float kExtractionRadius = 1.5f;

	if (dist <= kExtractionRadius)
	{
		if (!isGameCleared_)
		{
			extractionTimer_ -= deltaTime;
			if (extractionTimer_ <= 0.0f)
			{
				extractionTimer_ = 0.0f;
				isGameCleared_ = true;
				SceneManager::GetInstance()->ChangeScene("CLEAR");
			}
		}
	}
	else if (!isGameCleared_)
	{
		extractionTimer_ = 5.0f;
	}
}

void GamePlayScene::UpdateEnvironment()
{
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
			animator_.Update(kFixedDeltaTime);
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
		hitEffect_->Update(kFixedDeltaTime);
	}
}

void GamePlayScene::UpdateParticles(float deltaTime)
{
	emitter.frequencyTime += deltaTime;
	if (emitter.frequencyTime >= emitter.frequency && particleManager)
	{
		auto newParticles = particleEmitter.Emit(emitter, particleManager->GetRandomEngine(), *particleManager);
		for (auto& p : newParticles)
		{
			p.textureIndex = particleTextureA;
		}
		particleManager->AddParticles(newParticles);
		emitter.frequencyTime -= emitter.frequency;
	}

	// 脱出エリアの「もくもく」煙エフェクト (項目2)
	if (particleManager)
	{
		escapeSmokeTimer_ += deltaTime;
		if (escapeSmokeTimer_ >= 0.08f)
		{
			std::list<ParticleManager::Particle> smokeParticles;
			for (int i = 0; i < 3; ++i)
			{
				auto p = particleManager->MakeDustParticle(particleManager->GetRandomEngine(), goalRingTransform_.translate, 1.8f);
				// エメラルドグリーン
				p.color = { 0.2f, 0.8f, 0.6f, 0.4f };
				p.textureIndex = particleTextureB; // 円形
				p.velocity.y = 1.5f + (static_cast<float>(rand()) / RAND_MAX) * 0.8f;
				p.lifeTime = 1.3f;
				smokeParticles.push_back(p);
			}
			particleManager->AddParticles(smokeParticles);
			escapeSmokeTimer_ = 0.0f;
		}
	}
}

void GamePlayScene::UpdateSprites()
{
	if (spriteManager_ && directXCom)
	{
		WindowAPI* windowAPI = directXCom->GetWindowAPI();
		if (windowAPI)
		{
			spriteManager_->Update(windowAPI, &debugCamera_);
		}
	}

	mouseInput.Update();
	if (cursorSpriteIndex < 0 || cursorSpriteIndex >= static_cast<int>(sprites.size()))
	{
		return;
	}

	auto* cur = sprites[cursorSpriteIndex].get();
	if (!cur)
	{
		return;
	}

	Vector2 pos = mouseInput.GetScaledPosition();

	if (player_)
	{
		float reticleSize = 24.0f + player_->GetCurrentSpread() * 400.0f;
		cur->SetSize({ reticleSize, reticleSize });
	}

	cur->SetPosition(pos);
	cur->UpdateTransformOnly(directXCom ? directXCom->GetWindowAPI() : nullptr);
	if (mouseInput.PushButton(0))
	{
		cur->SetColor({ 1.0f, 0.0f, 0.0f, 1.0f });
	}
	else
	{
		cur->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
	}
}

void GamePlayScene::UpdateDebugInput()
{
	{
		static bool prevKey9 = false;
		const bool curKey9 = (GetAsyncKeyState('9') & 0x8000) != 0;
		if (curKey9 && !prevKey9 && hitEffect_)
		{
			Vector3 effectTranslate = emitter.transform.GetTranslate();
			effectTranslate.y += 1.5f;
			hitEffect_->Play(effectTranslate);
		}
		prevKey9 = curKey9;
	}

	{
		static bool prevKey8 = false;
		const bool curKey8 = (GetAsyncKeyState('8') & 0x8000) != 0;
		if (curKey8 && !prevKey8 && particleManager)
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
		prevKey8 = curKey8;
	}
}

void GamePlayScene::UpdateCharacters(float deltaTime)
{
	if (player_)
	{
		player_->Update(deltaTime, &mouseInput);

		// Spawn dust particles for player movement and dodging
		if (particleManager && !player_->IsDead())
		{
			Vector3 playerPos = player_->GetPosition();
			if (player_->IsDodging())
			{
				// Spawn dense, larger dust during dodge
				std::list<ParticleManager::Particle> dodgeDust;
				for (int i = 0; i < 2; ++i)
				{
					auto p = particleManager->MakeDustParticle(particleManager->GetRandomEngine(), playerPos, 1.4f);
					p.textureIndex = particleTextureB;
					dodgeDust.push_back(p);
				}
				particleManager->AddParticles(dodgeDust);
			}
			else if (player_->IsMoving())
			{
				playerDustTimer_ += deltaTime;
				if (playerDustTimer_ >= 0.15f)
				{
					std::list<ParticleManager::Particle> walkDust;
					auto p = particleManager->MakeDustParticle(particleManager->GetRandomEngine(), playerPos, 0.8f);
					p.textureIndex = particleTextureB;
					walkDust.push_back(p);
					particleManager->AddParticles(walkDust);

					playerDustTimer_ = 0.0f;
				}
			}
			else
			{
				playerDustTimer_ = 0.0f;
			}
		}
	}

	WindowAPI* windowAPI = directXCom ? directXCom->GetWindowAPI() : nullptr;
	if (!enemy_)
	{
		return;
	}

	const Vector3* target = nullptr;
	Vector3 playerPos{};
	if (player_ && !player_->IsDead())
	{
		playerPos = player_->GetPosition();
		target = &playerPos;
	}

	enemy_->Update(windowAPI, target, deltaTime);
}

void GamePlayScene::AddBullet(std::unique_ptr<Bullet> bullet)
{
	if (bullet)
	{
		bullets_.emplace_back(std::move(bullet));
	}
}

void GamePlayScene::UpdateCombat(float deltaTime)
{
	if (player_)
	{
		std::unique_ptr<Bullet> bullet = player_->TryShoot(&mouseInput, deltaTime);
		if (bullet && particleManager)
		{
			std::list<ParticleManager::Particle> flashParticles;
			Vector3 bulletPos = bullet->GetPosition();
			Vector3 dir = bullet->GetDirection();
			Vector3 right = { dir.z, 0.0f, -dir.x };
			Vector3 up = { 0.0f, 1.0f, 0.0f };

			for (int i = 0; i < 8; ++i)
			{
				auto p = particleManager->MakeMuzzleFlashParticle(particleManager->GetRandomEngine(), bulletPos);
				
				// 前方への強い速度ベクトルに左右・上下のランダムブレを加える（円錐状の広がり）
				float forwardSpeed = 4.0f + (static_cast<float>(rand()) / RAND_MAX) * 4.0f;
				float rightSpeed = (static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f) * 1.8f;
				float upSpeed = (static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f) * 1.2f;

				p.velocity = dir * forwardSpeed + right * rightSpeed + up * upSpeed;
				p.textureIndex = particleTextureB;
				p.lifeTime = 0.05f + (static_cast<float>(rand()) / RAND_MAX) * 0.1f;
				flashParticles.push_back(p);
			}
			particleManager->AddParticles(flashParticles);
		}
		AddBullet(std::move(bullet));
	}

	if (enemy_ && player_ && !enemy_->IsDead() && !player_->IsDead())
	{
		std::unique_ptr<Bullet> bullet = enemy_->TryShoot(player_->GetPosition());
		if (bullet && particleManager)
		{
			std::list<ParticleManager::Particle> flashParticles;
			Vector3 bulletPos = bullet->GetPosition();
			Vector3 dir = bullet->GetDirection();
			Vector3 right = { dir.z, 0.0f, -dir.x };
			Vector3 up = { 0.0f, 1.0f, 0.0f };

			for (int i = 0; i < 6; ++i)
			{
				auto p = particleManager->MakeMuzzleFlashParticle(particleManager->GetRandomEngine(), bulletPos);
				
				// 敵も同様に円錐状に赤色マズルブラストを前方へ吹き出す
				float forwardSpeed = 3.5f + (static_cast<float>(rand()) / RAND_MAX) * 3.0f;
				float rightSpeed = (static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f) * 1.5f;
				float upSpeed = (static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f) * 1.0f;

				p.velocity = dir * forwardSpeed + right * rightSpeed + up * upSpeed;
				p.color = { 1.0f, 0.2f, 0.2f, 1.0f }; // 赤色
				p.textureIndex = particleTextureB;
				p.lifeTime = 0.05f + (static_cast<float>(rand()) / RAND_MAX) * 0.1f;
				flashParticles.push_back(p);
			}
			particleManager->AddParticles(flashParticles);
		}
		AddBullet(std::move(bullet));
	}

	UpdateBullets(deltaTime);
	ResolveBulletCollisions();
	RemoveDeadBullets();
	ResolveContactDamage();
}

void GamePlayScene::UpdateBullets(float deltaTime)
{
	for (auto& bullet : bullets_)
	{
		if (bullet)
		{
			bullet->Update(deltaTime);
		}
	}
}

void GamePlayScene::RemoveDeadBullets()
{
	bullets_.erase(
		std::remove_if(bullets_.begin(), bullets_.end(), [](const std::unique_ptr<Bullet>& bullet)
			{
				return !bullet || bullet->IsDead();
			}),
		bullets_.end());
}

bool GamePlayScene::IsWithinRadius(const Vector3& a, const Vector3& b, float radius)
{
	const float dx = a.x - b.x;
	const float dy = a.y - b.y;
	const float dz = a.z - b.z;
	return (dx * dx + dy * dy + dz * dz) <= (radius * radius);
}

void GamePlayScene::ResolveBulletCollisions()
{
	for (auto& bullet : bullets_)
	{
		if (!bullet || bullet->IsDead())
		{
			continue;
		}

		const Vector3 bulletPos = bullet->GetPosition();
		if (bullet->GetOwner() == BulletOwner::Player && enemy_ && !enemy_->IsDead())
		{
			const Vector3 enemyPos = enemy_->GetPosition();
			if (IsWithinRadius(bulletPos, enemyPos, bulletHitRadius_ + enemyHitRadius_))
			{
				enemy_->OnHit();

				// Spawn feather and spark particles on hit
				if (particleManager)
				{
					std::list<ParticleManager::Particle> hitParticles;
					// 15 feather particles (white)
					for (int i = 0; i < 15; ++i)
					{
						auto p = particleManager->MakeFeatherParticle(particleManager->GetRandomEngine(), bulletPos);
						p.textureIndex = particleTextureB;
						hitParticles.push_back(p);
					}
					// 6 spark particles
					for (int i = 0; i < 6; ++i)
					{
						auto p = particleManager->MakeSparkParticle(particleManager->GetRandomEngine(), bulletPos);
						p.textureIndex = particleTextureB;
						hitParticles.push_back(p);
					}
					particleManager->AddParticles(hitParticles);
				}

				if (hitEffect_ && enemy_->IsDead())
				{
					hitEffect_->SpawnPlaneParticles(enemyPos);
				}
				bullet->Finalize();
			}
		}
		else if (bullet->GetOwner() == BulletOwner::Enemy && player_ && !player_->IsDead())
		{
			const Vector3 playerPos = player_->GetPosition();
			if (IsWithinRadius(bulletPos, playerPos, bulletHitRadius_ + playerHitRadius_))
			{
				if (hitEffect_)
				{
					hitEffect_->Play(playerPos);
				}

				// Spawn feather and spark particles on hit
				if (particleManager)
				{
					std::list<ParticleManager::Particle> hitParticles;
					// 15 feather particles (white)
					for (int i = 0; i < 15; ++i)
					{
						auto p = particleManager->MakeFeatherParticle(particleManager->GetRandomEngine(), bulletPos);
						p.textureIndex = particleTextureB;
						hitParticles.push_back(p);
					}
					// 6 spark particles
					for (int i = 0; i < 6; ++i)
					{
						auto p = particleManager->MakeSparkParticle(particleManager->GetRandomEngine(), bulletPos);
						p.textureIndex = particleTextureB;
						hitParticles.push_back(p);
					}
					particleManager->AddParticles(hitParticles);
				}

				player_->TakeDamage(kEnemyBulletDamage);
				bullet->Finalize();
			}
		}
	}
}

void GamePlayScene::ResolveContactDamage()
{
	if (!player_ || !enemy_ || enemy_->IsDead() || player_->IsDead())
	{
		return;
	}

	if (IsWithinRadius(player_->GetPosition(), enemy_->GetPosition(), playerHitRadius_ + enemyHitRadius_))
	{
		player_->TakeDamage(kContactDamage);
	}
}

void GamePlayScene::UpdatePlayerHpBar()
{
	if (!player_ || !playerHpBarBg_ || !playerHpBarFg_ || !directXCom || !directXCom->GetWindowAPI())
	{
		return;
	}

	const float ratio = player_->GetHPRatio();
	playerHpBarFg_->SetSize({ 200.0f * ratio, 16.0f });
	playerHpBarBg_->UpdateTransformOnly(directXCom->GetWindowAPI());
	playerHpBarFg_->UpdateTransformOnly(directXCom->GetWindowAPI());
}

void GamePlayScene::CheckGameOver()
{
	if (player_ && player_->IsDead())
	{
		SceneManager::GetInstance()->ChangeScene("GAMEOVER");
	}
}

void GamePlayScene::Update()
{
	const float deltaTime = AdvanceDeltaTime();

	UpdateExtractionGoal(deltaTime);
	if (isGameCleared_)
	{
		return;
	}

	UpdateEnvironment();
	UpdateObstacles();
	UpdateParticles(deltaTime);
	UpdateSprites();
	UpdateDebugInput();
	UpdateCharacters(deltaTime);
	UpdateCombat(deltaTime);
	ResolveObstacleCollisions();
	UpdatePlayerHpBar();
	CheckGameOver();
}

void GamePlayScene::InitializeObstacles()
{
	obstacles_.clear();

	// 障害物を3つ仮配置 (エディタ作成前の仮置き)
	auto obs1 = std::make_unique<Obstacle>();
	obs1->Initialize(object3dCom, camera_, { 0.0f, 0.0f, 4.0f }, 1.0f);
	obstacles_.push_back(std::move(obs1));

	auto obs2 = std::make_unique<Obstacle>();
	obs2->Initialize(object3dCom, camera_, { -2.5f, 0.0f, 7.0f }, 1.0f);
	obstacles_.push_back(std::move(obs2));

	auto obs3 = std::make_unique<Obstacle>();
	obs3->Initialize(object3dCom, camera_, { 2.5f, 0.0f, 7.0f }, 1.0f);
	obstacles_.push_back(std::move(obs3));
}

void GamePlayScene::UpdateObstacles()
{
	for (auto& obs : obstacles_)
	{
		if (obs)
		{
			obs->Update();
		}
	}
}

void GamePlayScene::ResolveObstacleCollisions()
{
	// プレイヤーとの衝突判定・押し出し
	if (player_ && !player_->IsDead())
	{
		Vector3 pPos = player_->GetPosition();
		for (auto& obs : obstacles_)
		{
			if (!obs) continue;
			Vector3 oPos = obs->GetPosition();
			float dx = pPos.x - oPos.x;
			float dz = pPos.z - oPos.z;
			float dist = std::sqrt(dx * dx + dz * dz);
			float minDist = playerHitRadius_ + obs->GetRadius();
			if (dist < minDist)
			{
				float overlap = minDist - dist;
				if (dist > 1e-4f)
				{
					pPos.x += (dx / dist) * overlap;
					pPos.z += (dz / dist) * overlap;
				}
				else
				{
					pPos.x += minDist;
				}
				player_->SetPosition(pPos);
			}
		}
	}

	// 敵との衝突判定・押し出し
	if (enemy_ && !enemy_->IsDead())
	{
		Vector3 ePos = enemy_->GetPosition();
		for (auto& obs : obstacles_)
		{
			if (!obs) continue;
			Vector3 oPos = obs->GetPosition();
			float dx = ePos.x - oPos.x;
			float dz = ePos.z - oPos.z;
			float dist = std::sqrt(dx * dx + dz * dz);
			float minDist = enemyHitRadius_ + obs->GetRadius();
			if (dist < minDist)
			{
				float overlap = minDist - dist;
				if (dist > 1e-4f)
				{
					ePos.x += (dx / dist) * overlap;
					ePos.z += (dz / dist) * overlap;
				}
				else
				{
					ePos.x += minDist;
				}
				enemy_->SetPosition(ePos);
			}
		}
	}

	// 弾丸との衝突判定
	for (auto& bullet : bullets_)
	{
		if (!bullet || bullet->IsDead()) continue;
		Vector3 bPos = bullet->GetPosition();
		for (auto& obs : obstacles_)
		{
			if (!obs) continue;
			Vector3 oPos = obs->GetPosition();
			float dx = bPos.x - oPos.x;
			float dz = bPos.z - oPos.z;
			float dist = std::sqrt(dx * dx + dz * dz);
			float minDist = bulletHitRadius_ + obs->GetRadius();
			if (dist < minDist)
			{
				// 衝突時の木片・おがくずエフェクト
				if (particleManager)
				{
					std::list<ParticleManager::Particle> woodDebris;
					
					// 1. 木片 (Splinters)
					for (int i = 0; i < 8; ++i)
					{
						auto p = particleManager->MakeSparkParticle(particleManager->GetRandomEngine(), bPos);
						p.textureIndex = fenceTextureIndex_;
						// 茶色〜ベージュ系の木片の色に設定
						std::uniform_real_distribution<float> colorDist(0.0f, 0.15f);
						float r = 0.5f + colorDist(particleManager->GetRandomEngine());
						float g = 0.35f + colorDist(particleManager->GetRandomEngine());
						float b = 0.15f + colorDist(particleManager->GetRandomEngine());
						p.color = { r, g, b, 1.0f };
						
						// スケールを細長い針状から、やや横幅のある木片（チップ）の形状に調整
						std::uniform_real_distribution<float> chipScaleX(0.08f, 0.16f);
						std::uniform_real_distribution<float> chipScaleY(0.15f, 0.35f);
						p.transform.SetScale({ chipScaleX(particleManager->GetRandomEngine()), chipScaleY(particleManager->GetRandomEngine()), 1.0f });
						
						woodDebris.push_back(p);
					}
					
					// 2. おがくずの煙 (Sawdust Dust)
					for (int i = 0; i < 6; ++i)
					{
						auto p = particleManager->MakeDustParticle(particleManager->GetRandomEngine(), bPos, 0.6f);
						p.textureIndex = fenceTextureIndex_;
						// 明るい茶色（おがくず色）
						std::uniform_real_distribution<float> colorDist(0.0f, 0.1f);
						float r = 0.7f + colorDist(particleManager->GetRandomEngine());
						float g = 0.55f + colorDist(particleManager->GetRandomEngine());
						float b = 0.35f + colorDist(particleManager->GetRandomEngine());
						p.color = { r, g, b, 0.8f }; // やや半透明
						
						// 速度を少し横/上方向に広げる
						std::uniform_real_distribution<float> velDist(-2.0f, 2.0f);
						std::uniform_real_distribution<float> velUp(1.0f, 3.0f);
						p.velocity = { velDist(particleManager->GetRandomEngine()), velUp(particleManager->GetRandomEngine()), velDist(particleManager->GetRandomEngine()) };
						
						woodDebris.push_back(p);
					}
					
					particleManager->AddParticles(woodDebris);
				}

				bullet->Finalize();
				break;
			}
		}
	}
}

RenderContext GamePlayScene::BuildRenderContext() const
{
	RenderContext ctx{};
	if (!directXCom)
	{
		return ctx;
	}

	ctx.commandList = directXCom->GetCommandList().Get();
	ctx.windowAPI = directXCom->GetWindowAPI();
	ctx.camera = camera_;
	ctx.light = SceneManager::GetInstance()->GetLight();
	ctx.materialGPUAddress = (materialManager && materialManager->GetMaterialResource()) ?
		materialManager->GetMaterialResource()->GetGPUVirtualAddress() : 0;
	return ctx;
}

void GamePlayScene::Draw(SceneRenderRequests& renderRequests)
{
	const RenderContext ctx = BuildRenderContext();

	// Draw Skybox
	SceneManager::GetInstance()->DrawSkybox(ctx.commandList);

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

	for (auto& obs : obstacles_)
	{
		if (obs)
		{
			obs->Draw(ctx);
		}
	}

	if (goalRing_)
	{
		D3D12_GPU_DESCRIPTOR_HANDLE handle = TextureManager::GetInstance()->GetSrvHandleGPU(cylinderTextureIndex_);
		if (handle.ptr != 0)
		{
			goalRing_->Draw(handle);
		}
	}

	if (hitEffect_)
	{
		hitEffect_->Draw();
	}

	if (spriteManager_)
	{
		spriteManager_->DrawAll(ctx, &debugCamera_, &sprites, false);
	}

	renderRequests.sceneDrawn = true;
}
