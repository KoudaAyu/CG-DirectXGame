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
	directXCom = dxCommon;
	lastTime_ = std::chrono::steady_clock::now();

	object3dCom = SceneManager::GetInstance()->GetObject3dCom();
	materialManager = SceneManager::GetInstance()->GetMaterialManager();
	light = SceneManager::GetInstance()->GetLight();
	particleManager = SceneManager::GetInstance()->GetParticleManager();

	InitializeEnvironment();
	InitializeCharacters();
	InitializeSprites();
	InitializeAudioAndParticles();
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
		cursor->SetAnchorPoint({ 0.0f, 0.0f });
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

void GamePlayScene::UpdateParticles()
{
	emitter.frequencyTime += kFixedDeltaTime;
	if (emitter.frequencyTime < emitter.frequency || !particleManager)
	{
		return;
	}

	auto newParticles = particleEmitter.Emit(emitter, particleManager->GetRandomEngine(), *particleManager);
	for (auto& p : newParticles)
	{
		p.textureIndex = particleTextureA;
	}
	particleManager->AddParticles(newParticles);
	emitter.frequencyTime -= emitter.frequency;
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

	Vector2 pos{ static_cast<float>(mouseInput.GetX()), static_cast<float>(mouseInput.GetY()) };
	if (directXCom && directXCom->GetWindowAPI())
	{
		RECT rc{};
		if (GetClientRect(directXCom->GetWindowAPI()->GetHwnd(), &rc))
		{
			const float clientW = float(rc.right - rc.left);
			const float clientH = float(rc.bottom - rc.top);
			if (clientW > 0.0f && clientH > 0.0f)
			{
				const float sx = float(directXCom->GetWindowAPI()->GetClientWidth()) / clientW;
				const float sy = float(directXCom->GetWindowAPI()->GetClientHeight()) / clientH;
				pos.x *= sx;
				pos.y *= sy;
			}
		}
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
		player_->Update(&mouseInput);
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
		AddBullet(player_->TryShoot(&mouseInput, deltaTime));
	}

	if (enemy_ && player_ && !enemy_->IsDead() && !player_->IsDead())
	{
		AddBullet(enemy_->TryShoot(player_->GetPosition()));
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
	UpdateParticles();
	UpdateSprites();
	UpdateDebugInput();
	UpdateCharacters(kFixedDeltaTime);
	UpdateCombat(kFixedDeltaTime);
	UpdatePlayerHpBar();
	CheckGameOver();
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
