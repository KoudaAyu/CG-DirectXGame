
#include "SceneManager.h"
#include "MaterialManager.h"
#include "Light.h"
#include "ParticleManager.h"
#include "RootParam.h"
#include "RenderContext.h"
#include "Baziru3_Engine\Graphics\Graphics\SceneRenderRequests.h"
#include "SpriteManager.h"
#include "TextureManager.h"
#include "AudioManager.h"
#include "../../Player/Player.h"
#include "../../Enemy/Enemy.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <Windows.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include "Baziru3_Engine/Framework/AI/NavMesh.h"

#include "GamePlayScene.h"

#include "imgui.h"
#include "CombatSystem.h"
#include "Bullet.h"
#include "CollisionSystem.h"
#include "Baziru3_Engine/Framework/Collision/CollisionManager.h"
#include "Baziru3_Engine/Framework/Collision/SphereCollider.h"
#include "Baziru3_Engine/Framework/Collision/BoxCollider.h"
#include "Baziru3_Engine/Framework/Collision/CapsuleCollider.h"

GamePlayScene::GamePlayScene()
{
}

GamePlayScene::~GamePlayScene()
{
}

void GamePlayScene::InitializeScene()
{
	directXCom = dxCommon_;
	camera_ = BaseScene::camera_;
	assert(directXCom != nullptr);

	lastTime_ = std::chrono::steady_clock::now();
	hitStopTimer_ = 0.0f;

	object3dCom = GetObject3dCom();
	skinningObject3dCom = GetSkinningObject3dCom();
	materialManager = GetMaterialManager();
	light = GetLight();
	particleManager = GetParticleManager();
	appParticleManager_ = std::make_unique<AppParticleManager>();
	appParticleManager_->Initialize(particleManager);

	InitializeEnvironment();
	InitializeCharacters();
	InitializeSprites();
	InitializeAudioAndParticles();
	InitializeObstacles();

	combatSystem_ = std::make_unique<CombatSystem>(this);
	collisionSystem_ = std::make_unique<CollisionSystem>(this);

	// --- チュートリアル用の的と看板の初期配置 ---
	targets_.clear();
	auto t1 = std::make_unique<Target>();
	t1->Initialize(object3dCom, camera_, { -3.0f, 0.0f, 16.0f }, 0.8f);
	targets_.push_back(std::move(t1));

	auto t2 = std::make_unique<Target>();
	t2->Initialize(object3dCom, camera_, { 3.0f, 0.0f, 16.0f }, 0.8f);
	targets_.push_back(std::move(t2));

	auto t3 = std::make_unique<Target>();
	t3->Initialize(object3dCom, camera_, { 0.0f, 0.0f, 26.0f }, 0.8f);
	targets_.push_back(std::move(t3));
	allTargetsDestroyed_ = false;
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

	goalRing_ = std::make_unique<Ring>();
	goalRing_->Initialize(directXCom, object3dCom, materialManager, light, camera_, 64, 1.5f, 1.2f);
	goalRingTransform_.rotate = { 1.570796f, 0.0f, 0.0f };
	goalRingTransform_.scale = { 1.0f, 1.0f, 1.0f };
	goalRingTransform_.translate = { 0.0f, 0.01f, 32.0f };
	isGameCleared_ = false;
	extractionTimer_ = 5.0f;

	// --- ✨ アプリケーション層で Resources/stage_layout.json (川・大自然) を全自動ロード ---
	levelEditor_ = std::make_unique<LevelEditor>();
	levelEditor_->Initialize(directXCom, object3dCom);
	levelEditor_->LoadFromFile("Resources/stage_layout.json");
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

	if (!movingEnemy_)
	{
		movingEnemy_ = std::make_unique<MovingEnemy>();
		movingEnemy_->Initialize(object3dCom, camera_);
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

	auto mobBg = Sprite::Create(sc, defaultTransform, "Resources/CG4/human/white.png");
	auto mobFg = Sprite::Create(sc, defaultTransform, "Resources/CG4/human/white.png");
	if (mobBg && mobFg)
	{
		mobBg->SetAnchorPoint({ 0.0f, 0.0f });
		mobFg->SetAnchorPoint({ 0.0f, 0.0f });
		sprites.emplace_back(std::move(mobBg));
		sprites.emplace_back(std::move(mobFg));
		if (movingEnemy_)
		{
			movingEnemy_->SetHPBarSprites(sprites[sprites.size() - 2].get(), sprites[sprites.size() - 1].get());
		}
	}

	auto alertBar = Sprite::Create(sc, defaultTransform, "Resources/CG4/human/white.png");
	auto alertDot = Sprite::Create(sc, defaultTransform, "Resources/CG4/human/white.png");
	if (alertBar && alertDot)
	{
		alertBar->SetAnchorPoint({ 0.5f, 1.0f }); // 下端中央
		alertDot->SetAnchorPoint({ 0.5f, 0.0f }); // 上端中央
		sprites.emplace_back(std::move(alertBar));
		sprites.emplace_back(std::move(alertDot));
		if (movingEnemy_)
		{
			movingEnemy_->SetAlertSprites(sprites[sprites.size() - 2].get(), sprites[sprites.size() - 1].get());
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

	auto prBg = Sprite::Create(sc, defaultTransform, "Resources/CG4/human/white.png");
	auto prFg = Sprite::Create(sc, defaultTransform, "Resources/CG4/human/white.png");
	if (prBg && prFg)
	{
		prBg->SetAnchorPoint({ 0.0f, 0.0f });
		prFg->SetAnchorPoint({ 0.0f, 0.0f });
		sprites.emplace_back(std::move(prBg));
		sprites.emplace_back(std::move(prFg));
		playerReloadBarBg_ = sprites[sprites.size() - 2].get();
		playerReloadBarFg_ = sprites[sprites.size() - 1].get();
	}

	// 速度線のスプライトを初期化 (8本)
	speedLines_.clear();
	speedLineAlpha_ = 0.0f;
	for (int i = 0; i < 8; ++i)
	{
		auto line = Sprite::Create(sc, defaultTransform, "Resources/CG4/gradationLine.png");
		if (line)
		{
			line->SetAnchorPoint({ 0.5f, 0.0f });
			line->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
			sprites.emplace_back(std::move(line));
			speedLines_.push_back(sprites.back().get());
		}
	}

	// ビネット用スプライトの初期化
	vignetteAlpha_ = 0.0f;
	if (auto vignette = Sprite::Create(sc, defaultTransform, "Resources/CG4/human/white.png"))
	{
		vignette->SetAnchorPoint({ 0.0f, 0.0f });
		vignette->SetPosition({ 0.0f, 0.0f });
		vignette->SetSize({ 1280.0f, 720.0f });
		vignette->SetColor({ 1.0f, 0.0f, 0.0f, 0.0f });
		sprites.emplace_back(std::move(vignette));
		vignetteSprite_ = sprites.back().get();
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
	starburstTextureIndex_ = TextureManager::GetInstance()->Load("Resources/starburst.png");
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

	if (combatSystem_) { combatSystem_.reset(); }
	if (collisionSystem_) { collisionSystem_.reset(); }

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

	if (movingEnemy_)
	{
		movingEnemy_->Finalize();
		movingEnemy_.reset();
	}

	for (auto& obs : obstacles_)
	{
		if (obs)
		{
			obs->Finalize();
		}
	}
	obstacles_.clear();

	for (auto& t : targets_)
	{
		if (t) t->Finalize();
	}
	targets_.clear();
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

	if (dist <= kExtractionRadius && allTargetsDestroyed_)
	{
		if (!isGameCleared_)
		{
			extractionTimer_ -= deltaTime;
			if (extractionTimer_ <= 0.0f)
			{
				extractionTimer_ = 0.0f;
				isGameCleared_ = true;
				clearCelebrateTimer_ = 0.0f;

				// Initial massive blast of confetti (150 particles)
				if (particleManager && appParticleManager_)
				{
					for (int i = 0; i < 150; ++i)
					{
						float angle = (static_cast<float>(rand()) / RAND_MAX) * 6.2831853f;
						float speedXZ = 1.5f + (static_cast<float>(rand()) / RAND_MAX) * 4.5f;
						Vector3 velocity = {
							std::cos(angle) * speedXZ,
							9.0f + (static_cast<float>(rand()) / RAND_MAX) * 11.0f,
							std::sin(angle) * speedXZ
						};
						
						Vector4 color;
						int colorType = rand() % 4;
						if (colorType == 0) color = { 1.0f, 0.85f, 0.15f, 1.0f };
						else if (colorType == 1) color = { 0.1f, 0.9f, 1.0f, 1.0f };
						else if (colorType == 2) color = { 1.0f, 0.2f, 0.8f, 1.0f };
						else color = { 0.2f, 1.0f, 0.5f, 1.0f };
						
						float scale = 0.2f + (static_cast<float>(rand()) / RAND_MAX) * 0.15f;
						float lifeTime = 1.2f + (static_cast<float>(rand()) / RAND_MAX) * 0.8f;

						appParticleManager_->EmitSparkWithVelocity(
							particleManager->GetRandomEngine(),
							goalPos,
							velocity,
							color,
							scale,
							lifeTime,
							particleTextureB
						);
					}
				}
			}
		}
	}
	else if (!isGameCleared_)
	{
		extractionTimer_ = 5.0f;
	}

	// Continuous fountain celebration after clear
	if (isGameCleared_ && particleManager && appParticleManager_)
	{
		clearCelebrateTimer_ += deltaTime;
		if (clearCelebrateTimer_ >= 0.009f)
		{
			// Spawn 10 celebratory confetti sparks shooting up every interval
			for (int i = 0; i < 10; ++i)
			{
				float angle = (static_cast<float>(rand()) / RAND_MAX) * 6.2831853f;
				float speedXZ = 1.0f + (static_cast<float>(rand()) / RAND_MAX) * 3.5f;
				Vector3 velocity = {
					std::cos(angle) * speedXZ,
					7.0f + (static_cast<float>(rand()) / RAND_MAX) * 9.0f,
					std::sin(angle) * speedXZ
				};
				
				Vector4 color;
				int colorType = rand() % 4;
				if (colorType == 0) color = { 1.0f, 0.85f, 0.15f, 1.0f };
				else if (colorType == 1) color = { 0.1f, 0.9f, 1.0f, 1.0f };
				else if (colorType == 2) color = { 1.0f, 0.2f, 0.8f, 1.0f };
				else color = { 0.2f, 1.0f, 0.5f, 1.0f };
				
				float scale = 0.18f + (static_cast<float>(rand()) / RAND_MAX) * 0.12f;
				float lifeTime = 1.0f + (static_cast<float>(rand()) / RAND_MAX) * 0.8f;

				appParticleManager_->EmitSparkWithVelocity(
					particleManager->GetRandomEngine(),
					goalPos,
					velocity,
					color,
					scale,
					lifeTime,
					particleTextureB
				);
			}
			clearCelebrateTimer_ = 0.0f;
		}
	}
}

void GamePlayScene::UpdateEnvironment()
{
	if (sphereInitialized && sphere_)
	{
		Sprite::Transform transformSphere = sphere_->GetTransform();
		transformSphere.rotate.y += 0.05f; // バリア感を出すために少し速めに回転
		
		if (player_ && player_->IsDodging())
		{
			// プレイヤーアヒルの位置にスナップ（頭部に合わせるためY軸方向に少しオフセット）
			transformSphere.translate = player_->GetPosition();
			transformSphere.translate.y += 0.4f;
			transformSphere.scale = { 1.3f, 1.3f, 1.3f }; // アヒルを覆うサイズ
			
			// ドッジタイマーに基づいてシールドサイズを少し脈動させる
			float pulse = 1.0f + 0.08f * std::sin(player_->GetDodgeTimer() * 35.0f);
			transformSphere.scale.x *= pulse;
			transformSphere.scale.y *= pulse;
			transformSphere.scale.z *= pulse;
		}
		else if (!allTargetsDestroyed_)
		{
			// 的が残っている間は脱出ゲートを保護する赤いバリアとして表示
			transformSphere.translate = goalRingTransform_.translate;
			transformSphere.translate.y = 0.5f;
			transformSphere.scale = { 1.8f, 1.8f, 1.8f };
			
			// バリアの鼓動
			static float barrierTimer = 0.0f;
			barrierTimer += kFixedDeltaTime * 4.0f;
			float pulse = 1.0f + 0.05f * std::sin(barrierTimer);
			transformSphere.scale.x *= pulse;
			transformSphere.scale.y *= pulse;
			transformSphere.scale.z *= pulse;
		}
		else
		{
			// 画面外の遠くか、あるいはスケール0に設定して非表示にする
			transformSphere.translate = { 0.0f, -100.0f, 0.0f };
			transformSphere.scale = { 0.0f, 0.0f, 0.0f };
		}
		
		sphere_->SetTransform(transformSphere);
		sphere_->Update();
	}



	if (hitEffectInitialized && hitEffect_)
	{
		hitEffect_->SetPlaneParticleCount(emitter.count);
		hitEffect_->Update(kFixedDeltaTime);
	}
}

void GamePlayScene::UpdateParticles(float deltaTime)
{
	// 初期状態からテスト用エミッターが連続でパーティクルを出し続けるのを停止
	/*
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
	*/

	// 脱出エリアの「もくもく」煙エフェクト (項目2)
	if (particleManager && appParticleManager_)
	{
		escapeSmokeTimer_ += deltaTime;
		if (escapeSmokeTimer_ >= 0.08f)
		{
			for (int i = 0; i < 3; ++i)
			{
				appParticleManager_->EmitDust(
					particleManager->GetRandomEngine(),
					goalRingTransform_.translate,
					1.8f,
					{ 0.2f, 0.8f, 0.6f, 0.4f },
					particleTextureB
				);
			}
			escapeSmokeTimer_ = 0.0f;
		}
	}

	// 敵の視野コーンの3Dパーティクルエフェクト描画 (F1デバッグ表示が有効な場合)
	if (showDebugGizmos_ && particleManager && appParticleManager_)
	{
		auto emitVisionCone3D = [&](const Vector3& pos, float yaw, float fov, float maxRange, const Vector4& color) {
			float halfFov = fov * 0.5f;
			float startAngle = yaw - halfFov;
			float endAngle = yaw + halfFov;

			// Left boundary (densely sampled, slowly rising)
			for (int i = 1; i <= 10; ++i)
			{
				float dist = maxRange * (i / 10.0f);
				Vector3 p = {
					pos.x + std::sin(startAngle) * dist,
					pos.y + 0.15f,
					pos.z + std::cos(startAngle) * dist
				};
				appParticleManager_->EmitSparkWithVelocity(
					particleManager->GetRandomEngine(),
					p,
					{ 0.0f, 0.0f, 0.0f }, // No rise, stay flat on the ground
					color,
					0.22f, // Larger size
					0.10f, // Extended lifetime to survive frame delta variations
					particleTextureB
				);
			}

			// Right boundary
			for (int i = 1; i <= 10; ++i)
			{
				float dist = maxRange * (i / 10.0f);
				Vector3 p = {
					pos.x + std::sin(endAngle) * dist,
					pos.y + 0.15f,
					pos.z + std::cos(endAngle) * dist
				};
				appParticleManager_->EmitSparkWithVelocity(
					particleManager->GetRandomEngine(),
					p,
					{ 0.0f, 0.0f, 0.0f },
					color,
					0.22f,
					0.10f,
					particleTextureB
				);
			}

			// Outer arc (densely sampled)
			for (int i = 0; i <= 16; ++i)
			{
				float angle = startAngle + i * (fov / 16.0f);
				Vector3 p = {
					pos.x + std::sin(angle) * maxRange,
					pos.y + 0.15f,
					pos.z + std::cos(angle) * maxRange
				};
				appParticleManager_->EmitSparkWithVelocity(
					particleManager->GetRandomEngine(),
					p,
					{ 0.0f, 0.0f, 0.0f },
					color,
					0.22f,
					0.10f,
					particleTextureB
				);
			}
		};

		if (enemy_ && !enemy_->IsDead())
		{
			Vector4 color = { 0.1f, 0.9f, 0.2f, 0.7f }; // Green
			if (enemy_->GetAIState() == Enemy::AIState::Investigate)
			{
				color = { 1.0f, 0.6f, 0.1f, 0.7f }; // Orange
			}
			else if (enemy_->GetAIState() == Enemy::AIState::Chase)
			{
				color = { 1.0f, 0.1f, 0.1f, 0.7f }; // Red
			}
			emitVisionCone3D(enemy_->GetPosition(), enemy_->GetYaw(), enemy_->GetFieldOfView(), enemy_->GetMaxSightRange(), color);
		}

		if (movingEnemy_ && !movingEnemy_->IsDead())
		{
			Vector4 color = { 0.1f, 0.9f, 0.2f, 0.7f }; // Green
			if (movingEnemy_->GetAIState() == MovingEnemy::AIState::Investigate)
			{
				color = { 1.0f, 0.6f, 0.1f, 0.7f }; // Orange
			}
			else if (movingEnemy_->GetAIState() == MovingEnemy::AIState::Chase)
			{
				color = { 1.0f, 0.1f, 0.1f, 0.7f }; // Red
			}
			emitVisionCone3D(movingEnemy_->GetPosition(), movingEnemy_->GetYaw(), movingEnemy_->GetFieldOfView(), movingEnemy_->GetMaxSightRange(), color);
		}
	}

	// 敵の警戒予兆エフェクト (Suspicion Aura)
	if (particleManager && appParticleManager_)
	{
		auto emitSuspicionAura = [&](const Vector3& pos, float meter) {
			// 警戒度に応じて発生頻度を高める (15% 〜 45%)
			int chance = static_cast<int>(15.0f + meter * 30.0f);
			if (rand() % 100 < chance)
			{
				std::uniform_real_distribution<float> offsetDist(-0.35f, 0.35f);
				Vector3 spawnPos = pos + Vector3{ offsetDist(particleManager->GetRandomEngine()), 1.3f, offsetDist(particleManager->GetRandomEngine()) };
				
				std::uniform_real_distribution<float> velY(0.5f, 1.3f);
				std::uniform_real_distribution<float> velXZ(-0.2f, 0.2f);
				Vector3 vel = { velXZ(particleManager->GetRandomEngine()), velY(particleManager->GetRandomEngine()), velXZ(particleManager->GetRandomEngine()) };

				// 警戒度に応じた色（黄色〜オレンジ）
				Vector4 color = { 1.0f, 0.9f - (meter * 0.3f), 0.15f, 0.75f };

				appParticleManager_->EmitDustWithVelocity(
					particleManager->GetRandomEngine(),
					spawnPos,
					0.85f, // はっきり見えるサイズに拡大
					color,
					vel,
					1.15f, // 寿命を長くして頭上へ立ち上らせる
					particleTextureB
				);
			}
		};

		if (enemy_ && !enemy_->IsDead() && enemy_->GetDetectionMeter() > 0.0f && enemy_->GetAIState() != Enemy::AIState::Chase)
		{
			emitSuspicionAura(enemy_->GetPosition(), enemy_->GetDetectionMeter());
		}

		if (movingEnemy_ && !movingEnemy_->IsDead() && movingEnemy_->GetDetectionMeter() > 0.0f && movingEnemy_->GetAIState() != MovingEnemy::AIState::Chase)
		{
			emitSuspicionAura(movingEnemy_->GetPosition(), movingEnemy_->GetDetectionMeter());
		}
	}

	if (appParticleManager_)
	{
		appParticleManager_->Update(deltaTime, player_ ? player_->GetPosition() : Vector3{ 0.0f, 0.0f, 0.0f });
	}
}

void GamePlayScene::UpdateSprites(float deltaTime)
{
	if (spriteManager_ && directXCom)
	{
		WindowAPI* windowAPI = directXCom->GetWindowAPI();
		if (windowAPI)
		{
			spriteManager_->Update();
		}
	}

	// 速度線の更新
	if (directXCom && directXCom->GetWindowAPI())
	{
		WindowAPI* windowAPI = directXCom->GetWindowAPI();
		float width = static_cast<float>(windowAPI->GetClientWidth());
		float height = static_cast<float>(windowAPI->GetClientHeight());
		Vector2 center = { width * 0.5f, height * 0.5f };

		// 回避中ならアルファ値を上げ、そうでなければ下げる
		if (player_ && player_->IsDodging())
		{
			speedLineAlpha_ += 10.0f * deltaTime; // 高速でフェードイン
			if (speedLineAlpha_ > 0.6f) speedLineAlpha_ = 0.6f;
		}
		else
		{
			speedLineAlpha_ -= 4.0f * deltaTime; // フェードアウト
			if (speedLineAlpha_ < 0.0f) speedLineAlpha_ = 0.0f;
		}

		// 回避タイマー割合を基準にスケールアニメーション
		float dodgeProgress = 1.0f;
		if (player_ && player_->IsDodging())
		{
			dodgeProgress = player_->GetDodgeTimer() / player_->GetDodgeDuration();
		}

		for (size_t i = 0; i < speedLines_.size(); ++i)
		{
			auto* line = speedLines_[i];
			if (!line) continue;

			if (speedLineAlpha_ <= 0.0f)
			{
				line->SetSize({ 0.0f, 0.0f });
				line->Update();
				continue;
			}

			// 放射状の角度
			float angle = i * (6.2831853f / speedLines_.size());
			
			// 中心から外側に向かうベクトル
			Vector2 dir = { std::cos(angle), std::sin(angle) };

			// 画面端から少し中心側にオフセット
			float maxDist = (std::max)(width, height) * 0.5f;
			// 回避進行度に合わせて、中央に向けて伸縮アニメーション
			float dist = maxDist * (0.8f + 0.2f * std::sin(dodgeProgress * 3.14159f));

			Vector2 pos = { center.x + dir.x * dist, center.y + dir.y * dist };

			line->SetPosition(pos);
			// 速度線の向きを中心に合わせる
			line->SetRotation(angle - 1.570796f); // gradationLineは縦方向なので90度ずらす
			
			// スピード感のあるサイズ (幅と長さ)
			float lineWidth = 8.0f + 4.0f * std::sin(static_cast<float>(i + dodgeProgress * 10.0f));
			float lineHeight = maxDist * 0.7f;
			line->SetSize({ lineWidth, lineHeight });
			
			line->SetColor({ 1.0f, 1.0f, 1.0f, speedLineAlpha_ });
			line->Update();
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
	cur->Update();

	if (mouseInput.PushButton(0))
	{
		// 左クリック（射撃中）は赤色
		cur->SetColor({ 1.0f, 0.0f, 0.0f, 1.0f });
	}
	else
	{
		// 通常時は白色
		cur->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
	}

	// ビネット（ダメージ赤フラッシュ ＆ 瀕死時赤鼓動ビネット）の更新
	if (vignetteSprite_ && directXCom && directXCom->GetWindowAPI())
	{
		WindowAPI* windowAPI = directXCom->GetWindowAPI();
		
		// 瀕死の赤鼓動の計算
		float lowHpVignette = 0.0f;
		if (player_ && !player_->IsDead())
		{
			float hpRatio = player_->GetHPRatio();
			if (hpRatio <= 0.3f)
			{
				// 心拍数の周期（HPが低ければ低いほど鼓動が速くなるようにする）
				static float heartTimer = 0.0f;
				// HP 30% -> スピード速め。1秒間に約1.5回〜3回のパルス
				float pulseSpeed = 6.0f + (1.0f - hpRatio / 0.3f) * 6.0f; 
				heartTimer += pulseSpeed * deltaTime;

				// 鼓動のようなドクン、ドクンという2拍子の波を作る
				float sinVal = std::sin(heartTimer);
				float pulse = sinVal > 0.0f ? std::pow(sinVal, 2.0f) : 0.0f; // 収縮期を強調
				
				// 最大の強さは残りHPに応じて0.2〜0.5の間で変化
				float maxIntensity = 0.2f + (1.0f - hpRatio / 0.3f) * 0.3f;
				lowHpVignette = pulse * maxIntensity;
			}
		}

		if (vignetteAlpha_ > 0.0f)
		{
			vignetteAlpha_ -= 2.0f * deltaTime; // 0.5sで完全にフェードアウトする速度
			if (vignetteAlpha_ < 0.0f) vignetteAlpha_ = 0.0f;
		}

		// 被弾赤フラッシュ（一時）と瀕死赤鼓動（持続）の強い方を適用
		float finalAlpha = (std::max)(vignetteAlpha_, lowHpVignette);
		vignetteSprite_->SetColor({ 1.0f, 0.0f, 0.0f, finalAlpha });
		vignetteSprite_->Update();
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
				auto p = particleManager->MakeNewParticles(particleManager->GetRandomEngine(), effectTranslate);
				p.textureIndex = particleTextureB;
				p.lifeTime = 0.35f;
				newParticles.push_back(p);
			}
			particleManager->AddParticles(newParticles);
		}
		prevKey8 = curKey8;
	}

	{
		static bool prevF1 = false;
		const bool curF1 = (GetAsyncKeyState(VK_F1) & 0x8000) != 0;
		if (curF1 && !prevF1)
		{
			showDebugGizmos_ = !showDebugGizmos_;
		}
		prevF1 = curF1;
	}
}

void GamePlayScene::UpdateCharacters(float deltaTime)
{
	if (player_)
	{
		player_->Update(deltaTime, &mouseInput);

		// Spawn dust particles for player movement and dodging
		if (particleManager && appParticleManager_ && !player_->IsDead())
		{
			Vector3 playerPos = player_->GetPosition();

			// リロード中の物理演出（マガジンの排出 ＆ 残弾ドロップ）
			static bool spawnedMagazine = false;
			if (player_->IsReloading() && !wasPlayerReloadingPrev_)
			{
				remainingAmmoOnReload_ = player_->GetMagazineAmmo();
				droppedCasingsCount_ = 0;
			}
			wasPlayerReloadingPrev_ = player_->IsReloading();

			if (player_->IsReloading())
			{
				playerReloadCasingTimer_ += deltaTime;

				// 1. リロード開始直後にマガジン（弾倉）をドロップする
				float progress = player_->GetReloadProgress();
				if (progress < 0.1f && !spawnedMagazine)
				{
					Vector3 ejectOrigin = playerPos;
					ejectOrigin.y += 0.4f; // 胴体下部あたりから落とす
					
					Vector3 forward = { std::sin(player_->GetRotation().y), 0.0f, std::cos(player_->GetRotation().y) };
					appParticleManager_->EmitShellCasing(
						particleManager->GetRandomEngine(),
						ejectOrigin,
						forward,
						{ 0.95f, 0.95f, 1.0f, 1.0f }, // Chrome silver
						{ 0.28f, 0.48f, 0.18f },       // Large magazine scale
						particleTextureB
					);
					
					spawnedMagazine = true;
				}

				// 2. 残弾数に応じた個数の実弾をバラバラと落とす
				if (remainingAmmoOnReload_ > 0 && droppedCasingsCount_ < remainingAmmoOnReload_)
				{
					if (playerReloadCasingTimer_ >= 0.15f)
					{
						Vector3 ejectOrigin = playerPos;
						ejectOrigin.y += 0.5f; // 手元あたりから落とす
						
						Vector3 forward = { std::sin(player_->GetRotation().y), 0.0f, std::cos(player_->GetRotation().y) };
						
						// 未使用の弾薬なので金色（ゴールド）
						Vector4 color = { 1.0f, 0.85f, 0.2f, 1.0f };
						Vector3 scale = { 0.08f, 0.22f, 1.0f };

						appParticleManager_->EmitShellCasing(
							particleManager->GetRandomEngine(),
							ejectOrigin,
							forward,
							color,
							scale,
							particleTextureB
						);
						
						droppedCasingsCount_++;
						playerReloadCasingTimer_ = 0.0f;
					}
				}
			}
			else
			{
				playerReloadCasingTimer_ = 0.0f;
				spawnedMagazine = false;
			}

			// リロード完了の検知
			if (wasPlayerReloading_ && !player_->IsReloading())
			{
				// 巨大な緑のマズルフレアを発生させて完了の瞬間を目立たせる
				if (particleManager && appParticleManager_)
				{
					appParticleManager_->EmitMuzzleFlare(
						particleManager->GetRandomEngine(),
						playerPos + Vector3{ 0.0f, 0.4f, 0.0f },
						0.75f,
						{ 0.3f, 1.0f, 0.5f, 1.0f }, // 鮮やかな緑色
						0.08f,
						particleTextureB
					);

					for (int i = 0; i < 60; ++i)
					{
						float angle = i * (6.2831853f / 60.0f);
						Vector3 vel = { std::cos(angle) * 6.5f, 0.3f, std::sin(angle) * 6.5f };
						appParticleManager_->EmitSparkPlayerRelative(
							particleManager->GetRandomEngine(),
							playerPos,
							Vector3{0.0f, 0.1f, 0.0f},
							vel,
							{ 0.2f, 1.0f, 0.4f, 1.0f },
							0.45f, // はっきり見えるサイズ
							0.6f,
							particleTextureB
						);
					}
				}
			}
			wasPlayerReloading_ = player_->IsReloading();

			static bool prevDodge = false;
			if (player_->IsDodging())
			{
				if (!prevDodge)
				{
					TriggerCameraShake(0.12f, 0.22f); // 回避開始のカメラブレ
					
					float maxRad = 10.0f;
					playerSoundMaxRadius_ = maxRad;
					playerSoundRadius_ = 0.0f;
					playerSoundTimer_ = 0.4f; // Ring lasts for 0.4s
					
					Vector3 playerPos = player_->GetPosition();
					if (enemy_ && !enemy_->IsDead())
					{
						float dx = enemy_->GetPosition().x - playerPos.x;
						float dz = enemy_->GetPosition().z - playerPos.z;
						float dist = std::sqrt(dx * dx + dz * dz);
						if (dist <= maxRad)
						{
							enemy_->HearNoise(playerPos);
						}
					}
					if (movingEnemy_ && !movingEnemy_->IsDead())
					{
						float dx = movingEnemy_->GetPosition().x - playerPos.x;
						float dz = movingEnemy_->GetPosition().z - playerPos.z;
						float dist = std::sqrt(dx * dx + dz * dz);
						if (dist <= maxRad)
						{
							movingEnemy_->HearNoise(playerPos);
						}
					}
					
					// 足元から全方位（8方向）に勢いよく広がる土煙の輪（キックオフ演出）
					for (int i = 0; i < 8; ++i)
					{
						float angle = i * (6.2831853f / 8.0f);
						Vector3 vel = { std::cos(angle) * 2.8f, 0.4f, std::sin(angle) * 2.8f };
						appParticleManager_->EmitDustWithVelocity(
							particleManager->GetRandomEngine(),
							playerPos,
							1.5f,
							{ 1.0f, 1.0f, 1.0f, 0.7f }, // Semi-transparent dust
							vel,
							0.35f,
							particleTextureB
						);
					}
					prevDodge = true;
				}

				// Spawn dense, larger dust during dodge (回避中の激しい土煙)
				for (int i = 0; i < 3; ++i)
				{
					appParticleManager_->EmitDust(
						particleManager->GetRandomEngine(),
						playerPos,
						1.8f,
						{ 1.0f, 1.0f, 1.0f, 0.5f },
						particleTextureB
					);
				}
			}
			else
			{
				if (prevDodge)
				{
					// 回避終了時の着地衝撃波（土煙の放射状バースト ＆ カメラブレ）
					TriggerCameraShake(0.15f, 0.35f);
					for (int i = 0; i < 12; ++i)
					{
						float angle = i * (6.2831853f / 12.0f);
						Vector3 vel = { std::cos(angle) * 3.2f, 0.5f, std::sin(angle) * 3.2f };
						appParticleManager_->EmitDustWithVelocity(
							particleManager->GetRandomEngine(),
							playerPos,
							2.0f,
							{ 1.0f, 1.0f, 1.0f, 0.8f },
							vel,
							0.45f,
							particleTextureB
						);
					}
				}
				prevDodge = false;
				if (player_->IsMoving())
				{
					playerDustTimer_ += deltaTime;
					if (playerDustTimer_ >= 0.15f)
					{
						appParticleManager_->EmitDust(
							particleManager->GetRandomEngine(),
							playerPos,
							0.8f,
							{ 1.0f, 1.0f, 1.0f, 0.5f },
							particleTextureB
						);

						playerDustTimer_ = 0.0f;
					}

					// Footstep sound simulation
					static float stepNoiseAccumulator = 0.0f;
					stepNoiseAccumulator += deltaTime;
					if (stepNoiseAccumulator >= 0.35f)
					{
						stepNoiseAccumulator = 0.0f;
						float maxRad = 6.0f;
						playerSoundMaxRadius_ = maxRad;
						playerSoundRadius_ = 0.0f;
						playerSoundTimer_ = 0.25f; // Ring lasts for 0.25s
						
						Vector3 playerPos = player_->GetPosition();
						if (enemy_ && !enemy_->IsDead())
						{
							float dx = enemy_->GetPosition().x - playerPos.x;
							float dz = enemy_->GetPosition().z - playerPos.z;
							float dist = std::sqrt(dx * dx + dz * dz);
							if (dist <= maxRad)
							{
								enemy_->HearNoise(playerPos);
							}
						}
						if (movingEnemy_ && !movingEnemy_->IsDead())
						{
							float dx = movingEnemy_->GetPosition().x - playerPos.x;
							float dz = movingEnemy_->GetPosition().z - playerPos.z;
							float dist = std::sqrt(dx * dx + dz * dz);
							if (dist <= maxRad)
							{
								movingEnemy_->HearNoise(playerPos);
							}
						}
					}
				}
				else
				{
					playerDustTimer_ = 0.0f;
				}
			}
		}
	}

	WindowAPI* windowAPI = directXCom ? directXCom->GetWindowAPI() : nullptr;
	if (!enemy_)
	{
		return;
	}

	const Vector3* target = nullptr;
	Vector3 playerPosTarget{};
	if (player_ && !player_->IsDead())
	{
		playerPosTarget = player_->GetPosition();
		target = &playerPosTarget;
	}

	enemy_->Update(windowAPI, target, obstacles_, deltaTime);

	if (movingEnemy_)
	{
		movingEnemy_->Update(windowAPI, target, obstacles_, deltaTime);
	}

	// 1体が発見したら周囲の仲間に無線で位置を伝達（グループ連携無線）
	if (enemy_ && movingEnemy_ && player_ && !player_->IsDead())
	{
		if (enemy_->GetAIState() == Enemy::AIState::Chase && !enemy_->IsDead())
		{
			movingEnemy_->AlertEnemy(player_->GetPosition());
		}
		if (movingEnemy_->GetAIState() == MovingEnemy::AIState::Chase && !movingEnemy_->IsDead())
		{
			enemy_->AlertEnemy(player_->GetPosition());
		}
	}

	// 敵の復活時演出 (Enemy Respawn Effects - 光の召喚ピラー)
	if (particleManager && appParticleManager_)
	{
		auto emitRespawnPortal = [&](const Vector3& spawnPos, const Vector4& portalColor, float scaleMult = 1.0f) {
			// 二重同心円状に配置した粒子を垂直上方向に打ち上げて光の柱を形成する
			
			// 外円 (半径 0.7f, 24個 of particles)
			for (int i = 0; i < 24; ++i)
			{
				float angle = i * (6.2831853f / 24.0f);
				Vector3 offset = { std::sin(angle) * 0.7f, 0.0f, std::cos(angle) * 0.7f };
				appParticleManager_->EmitDustWithVelocity(
					particleManager->GetRandomEngine(),
					spawnPos + offset,
					0.65f * scaleMult, // サイズスケーリング
					portalColor,
					{ 0.0f, 2.2f, 0.0f }, // 垂直方向のみに高速上昇
					0.8f, // 少し長めの寿命で高くまで届かせる
					particleTextureB
				);
			}

			// 内円 (半径 0.4f, 16個 of particles)
			for (int i = 0; i < 16; ++i)
			{
				float angle = i * (6.2831853f / 16.0f);
				Vector3 offset = { std::sin(angle) * 0.4f, 0.0f, std::cos(angle) * 0.4f };
				appParticleManager_->EmitDustWithVelocity(
					particleManager->GetRandomEngine(),
					spawnPos + offset,
					0.55f * scaleMult,
					portalColor,
					{ 0.0f, 2.8f, 0.0f }, // さらに速く上昇
					0.7f,
					particleTextureB
				);
			}

			// 中心ビームコア (中心から数粒子を立ち上らせる)
			for (int i = 0; i < 8; ++i)
			{
				std::uniform_real_distribution<float> offsetH(-0.1f, 0.1f);
				std::uniform_real_distribution<float> offsetV(0.0f, 0.4f);
				Vector3 spawnOffset = { offsetH(particleManager->GetRandomEngine()), offsetV(particleManager->GetRandomEngine()), offsetH(particleManager->GetRandomEngine()) };
				
				appParticleManager_->EmitDustWithVelocity(
					particleManager->GetRandomEngine(),
					spawnPos + spawnOffset,
					0.75f * scaleMult,
					{ 1.0f, 1.0f, 1.0f, 0.95f }, // 中心コアは高輝度な白光
					{ 0.0f, 3.2f, 0.0f },
					0.6f,
					particleTextureB
				);
			}
		};

		if (enemy_ && enemy_->GetJustRespawned())
		{
			// 通常の敵：視認性の高い「鮮やかなネオンゴールド（オレンジ黄）」のフューチャーゲートでサイズを1.35倍に拡大
			emitRespawnPortal(enemy_->GetPosition(), { 1.0f, 0.7f, 0.0f, 0.95f }, 1.35f);
			enemy_->ClearJustRespawned();
		}

		if (movingEnemy_ && movingEnemy_->GetJustRespawned())
		{
			// 動く敵：ホットマゼンタ（ピンク）のマジックポータル
			emitRespawnPortal(movingEnemy_->GetPosition(), { 1.0f, 0.1f, 0.75f, 0.85f }, 1.0f);
			movingEnemy_->ClearJustRespawned();
		}
	}

	// Update sound radius propagation
	if (playerSoundTimer_ > 0.0f)
	{
		playerSoundTimer_ -= deltaTime;
		playerSoundRadius_ += 30.0f * deltaTime;
		if (playerSoundRadius_ > playerSoundMaxRadius_)
		{
			playerSoundRadius_ = playerSoundMaxRadius_;
		}
		if (playerSoundTimer_ <= 0.0f)
		{
			playerSoundRadius_ = 0.0f;
			playerSoundMaxRadius_ = 0.0f;
		}
	}

	if (levelEditor_)
	{
		levelEditor_->Update(deltaTime);
	}
}

bool GamePlayScene::IsWithinRadius(const Vector3& a, const Vector3& b, float radius)
{
	const float dx = a.x - b.x;
	const float dy = a.y - b.y;
	const float dz = a.z - b.z;
	return (dx * dx + dy * dy + dz * dz) <= (radius * radius);
}

void GamePlayScene::UpdatePlayerHpBar()
{
	if (!player_ || !playerHpBarBg_ || !playerHpBarFg_ || !directXCom || !directXCom->GetWindowAPI())
	{
		return;
	}

	const float ratio = player_->GetHPRatio();
	playerHpBarFg_->SetSize({ 200.0f * ratio, 16.0f });
	playerHpBarBg_->Update();
	playerHpBarFg_->Update();

	// プレイヤーの頭上にフローティング表示するリロードプログレスバーの更新
	if (playerReloadBarBg_ && playerReloadBarFg_)
	{
		WindowAPI* windowAPI = directXCom->GetWindowAPI();
		if (player_->IsReloading() && camera_)
		{
			Vector3 playerPos = player_->GetPosition();
			Vector3 barPos3D = playerPos;
			barPos3D.y += 1.6f; // プレイヤーアヒルの頭上

			const Matrix4x4& vp = camera_->GetViewProjectionMatrix();
			float x = barPos3D.x * vp.m[0][0] + barPos3D.y * vp.m[1][0] + barPos3D.z * vp.m[2][0] + vp.m[3][0];
			float y = barPos3D.x * vp.m[0][1] + barPos3D.y * vp.m[1][1] + barPos3D.z * vp.m[2][1] + vp.m[3][1];
			float z = barPos3D.x * vp.m[0][2] + barPos3D.y * vp.m[1][2] + barPos3D.z * vp.m[2][2] + vp.m[3][2];
			float w = barPos3D.x * vp.m[0][3] + barPos3D.y * vp.m[1][3] + barPos3D.z * vp.m[2][3] + vp.m[3][3];

			if (w > 0.0f)
			{
				x /= w;
				y /= w;

				float width = static_cast<float>(windowAPI->GetClientWidth());
				float height = static_cast<float>(windowAPI->GetClientHeight());

				float screenX = (x + 1.0f) * 0.5f * width;
				float screenY = (1.0f - y) * 0.5f * height;

				float bgWidth = 60.0f;
				float bgHeight = 6.0f;

				float reloadRatio = player_->GetReloadProgress(); // 0.0f -> 1.0f

				// 背景バー (黒)
				playerReloadBarBg_->SetPosition({ screenX - bgWidth * 0.5f, screenY });
				playerReloadBarBg_->SetSize({ bgWidth, bgHeight });
				playerReloadBarBg_->SetColor({ 0.1f, 0.1f, 0.1f, 0.8f });
				playerReloadBarBg_->Update();

				// 前景バー (黄緑・若草色で被ダメージの赤点滅時でもハッキリ見分けがつく色)
				playerReloadBarFg_->SetPosition({ screenX - bgWidth * 0.5f, screenY });
				playerReloadBarFg_->SetSize({ bgWidth * reloadRatio, bgHeight });
				playerReloadBarFg_->SetColor({ 0.0f, 1.0f, 0.5f, 1.0f });
				playerReloadBarFg_->Update();
			}
			else
			{
				playerReloadBarBg_->SetSize({ 0.0f, 0.0f });
				playerReloadBarBg_->Update();
				playerReloadBarFg_->SetSize({ 0.0f, 0.0f });
				playerReloadBarFg_->Update();
			}
		}
		else
		{
			// 非表示
			playerReloadBarBg_->SetSize({ 0.0f, 0.0f });
			playerReloadBarBg_->Update();
			playerReloadBarFg_->SetSize({ 0.0f, 0.0f });
			playerReloadBarFg_->Update();
		}
	}
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
	float realDeltaTime = AdvanceDeltaTime();
	float deltaTime = realDeltaTime;

	// ヒットストップ処理の更新
	if (hitStopTimer_ > 0.0f)
	{
		hitStopTimer_ -= realDeltaTime;
		deltaTime *= 0.15f;
		if (hitStopTimer_ <= 0.0f)
		{
			hitStopTimer_ = 0.0f;
		}
	}

	// クリア演出中のスローモーション処理
	if (isGameCleared_)
	{
		deltaTime *= 0.15f; // 15%の速度にスローダウン
		
		clearSlowMoTimer_ -= realDeltaTime;
		if (clearSlowMoTimer_ <= 0.0f)
		{
			SceneManager::GetInstance()->ChangeScene("CLEAR");
			return;
		}
	}

	UpdateExtractionGoal(deltaTime);

	UpdateEnvironment();
	UpdateObstacles();

	// 的の更新
	for (auto& t : targets_)
	{
		if (t) t->Update(deltaTime);
	}

	// すべての的が破壊されたかチェック
	bool anyTargetAlive = false;
	for (const auto& t : targets_)
	{
		if (t && !t->IsDead())
		{
			anyTargetAlive = true;
			break;
		}
	}
	allTargetsDestroyed_ = !anyTargetAlive;

	UpdateSprites(deltaTime);
	UpdateDebugInput();
	UpdateCharacters(deltaTime);
	if (combatSystem_)
	{
		combatSystem_->Update(deltaTime);
	}
	UpdateParticles(deltaTime);
	if (collisionSystem_)
	{
		collisionSystem_->Update();
	}
	UpdatePlayerHpBar();
	CheckGameOver();

	// ライト点滅（マズルフラッシュ効果）の更新
	if (light)
	{
		float intensity = 1.0f;
		if (lightFlashTimer_ > 0.0f)
		{
			lightFlashTimer_ -= deltaTime;
			intensity = 5.5f; // 眩しく発光
		}
		else
		{
			intensity = 1.0f; // 通常の明るさ
		}

		auto resource = light->GetDirectionalLightResource();
		if (resource)
		{
			Object3d::DirectionalLight* data = nullptr;
			resource->Map(0, nullptr, reinterpret_cast<void**>(&data));
			if (data)
			{
				data->intensity = intensity;
			}
			resource->Unmap(0, nullptr);
		}
	}

	// カメラシェイクの更新
	if (camera_ && cameraShakeTime_ > 0.0f)
	{
		cameraShakeTime_ -= realDeltaTime;
		float progress = cameraShakeTime_ / cameraShakeDurationMax_;
		float currentIntensity = cameraShakeIntensity_ * progress * progress; // スムーズな減衰
		float rx = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * currentIntensity;
		float ry = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * currentIntensity;
		float rz = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * currentIntensity;
		camera_->SetTranslate(camera_->GetTranslate() + Vector3{ rx, ry, rz });
	}

	// クリア時のシネマティックズームイン (プレイヤーアヒルに徐々に寄る)
	if (isGameCleared_ && camera_ && player_)
	{
		Vector3 playerPos = player_->GetPosition();
		Vector3 targetOffset = { 0.0f, 6.0f, -7.0f }; // 通常 {0, 20, -20} から寄る
		Vector3 currentTranslate = camera_->GetTranslate();
		Vector3 targetTranslate = playerPos + targetOffset;
		
		// イージングによる滑らかなズーム
		currentTranslate.x += (targetTranslate.x - currentTranslate.x) * 0.08f;
		currentTranslate.y += (targetTranslate.y - currentTranslate.y) * 0.08f;
		currentTranslate.z += (targetTranslate.z - currentTranslate.z) * 0.08f;
		camera_->SetTranslate(currentTranslate);
	}

	// 浮遊テキストの更新
	auto it = floatingTexts_.begin();
	while (it != floatingTexts_.end())
	{
		it->lifeTime -= deltaTime;
		it->position.y += 1.5f * deltaTime; // 上昇させる
		if (it->isCritical)
		{
			it->position.x += std::sin(it->lifeTime * 20.0f) * 0.15f * deltaTime; // クリティカルはゆらゆら揺らす
		}

		if (it->lifeTime <= 0.0f)
		{
			it = floatingTexts_.erase(it);
		}
		else
		{
			++it;
		}
	}

#ifdef USE_IMGUI
	// 浮遊テキストのImGui描画
	if (camera_ && !floatingTexts_.empty())
	{
		ImGuiIO& io = ImGui::GetIO();
		float screenW = io.DisplaySize.x;
		float screenH = io.DisplaySize.y;

		if (screenW > 0.0f && screenH > 0.0f)
		{
			Matrix4x4 viewProj = camera_->GetViewProjectionMatrix();
			ImDrawList* drawList = ImGui::GetForegroundDrawList();

			for (const auto& ft : floatingTexts_)
			{
				// 3D座標をスクリーン座標に変換
				float x = ft.position.x * viewProj.m[0][0] + ft.position.y * viewProj.m[1][0] + ft.position.z * viewProj.m[2][0] + viewProj.m[3][0];
				float y = ft.position.x * viewProj.m[0][1] + ft.position.y * viewProj.m[1][1] + ft.position.z * viewProj.m[2][1] + viewProj.m[3][1];
				float z = ft.position.x * viewProj.m[0][2] + ft.position.y * viewProj.m[1][2] + ft.position.z * viewProj.m[2][2] + viewProj.m[3][2];
				float w = ft.position.x * viewProj.m[0][3] + ft.position.y * viewProj.m[1][3] + ft.position.z * viewProj.m[2][3] + viewProj.m[3][3];

				if (w <= 0.0f) continue;

				float ndcX = x / w;
				float ndcY = y / w;

				if (ndcX < -1.1f || ndcX > 1.1f || ndcY < -1.1f || ndcY > 1.1f) continue;

				float screenX = (ndcX + 1.0f) * 0.5f * screenW;
				float screenY = (1.0f - ndcY) * 0.5f * screenH;

				float alpha = ft.lifeTime / ft.maxLifeTime;
				alpha = (std::clamp)(alpha, 0.0f, 1.0f);
				
				ImU32 textColor = ImGui::ColorConvertFloat4ToU32({ ft.color.x, ft.color.y, ft.color.z, ft.color.w * alpha });
				ImU32 shadowColor = ImGui::ColorConvertFloat4ToU32({ 0.0f, 0.0f, 0.0f, 0.8f * alpha });

				if (ft.isCritical)
				{
					// クリティカルテキストは太く見せるために3方向シャドウ＋2重メイン描画
					drawList->AddText(ImVec2(screenX + 2.0f, screenY + 2.0f), shadowColor, ft.text.c_str());
					drawList->AddText(ImVec2(screenX + 1.0f, screenY + 2.0f), shadowColor, ft.text.c_str());
					drawList->AddText(ImVec2(screenX + 2.0f, screenY + 1.0f), shadowColor, ft.text.c_str());
					drawList->AddText(ImVec2(screenX, screenY), textColor, ft.text.c_str());
					drawList->AddText(ImVec2(screenX + 1.0f, screenY), textColor, ft.text.c_str());
				}
				else
				{
					drawList->AddText(ImVec2(screenX + 1.0f, screenY + 1.0f), shadowColor, ft.text.c_str());
					drawList->AddText(ImVec2(screenX, screenY), textColor, ft.text.c_str());
				}
			}
		}
	}
#endif
}

void GamePlayScene::InitializeObstacles()
{
	obstacles_.clear();

	bool success = false;
	std::string filepath = "Resources/stage_layout.json";
	std::ifstream file(filepath);
	
	if (file.is_open())
	{
		try
		{
			nlohmann::json j;
			file >> j;
			file.close();

			for (const auto& obj : j)
			{
				std::string name = obj["name"];
				// オブジェクト名が "Obstacle" で始まる場合のみ配置
				if (name.find("Obstacle") == 0)
				{
					Vector3 pos = {
						obj["position"]["x"].get<float>(),
						obj["position"]["y"].get<float>(),
						obj["position"]["z"].get<float>()
					};
					// スケールのXを半径（radius）として使用します
					float radius = obj["scale"]["x"].get<float>();
					
					auto obs = std::make_unique<Obstacle>();
					obs->Initialize(object3dCom, camera_, pos, radius);
					obstacles_.push_back(std::move(obs));
				}
			}
			success = true;
			OutputDebugStringA("GamePlayScene: Successfully loaded obstacles from JSON.\n");
		}
		catch (const std::exception& e)
		{
			char errorMsg[256];
			sprintf_s(errorMsg, "GamePlayScene: Failed to parse stage_layout.json: %s\n", e.what());
			OutputDebugStringA(errorMsg);
		}
	}

	// 読み込みに失敗した、またはデータが空だった場合はデフォルトの配置を使用（フォールバック）
	if (!success || obstacles_.empty())
	{
		obstacles_.clear();
		OutputDebugStringA("GamePlayScene: Using default obstacle placement (fallback).\n");

		// --- 左側グループ (X = -5.0f 付近の配置) ---
		auto obs1 = std::make_unique<Obstacle>();
		obs1->Initialize(object3dCom, camera_, { -5.0f, 0.0f, 8.0f }, 1.0f);
		obstacles_.push_back(std::move(obs1));

		auto obs2 = std::make_unique<Obstacle>();
		obs2->Initialize(object3dCom, camera_, { -5.0f, 0.0f, 18.0f }, 1.0f);
		obstacles_.push_back(std::move(obs2));

		// --- 右側グループ (X = 5.0f 付近の配置) ---
		auto obs3 = std::make_unique<Obstacle>();
		obs3->Initialize(object3dCom, camera_, { 5.0f, 0.0f, 8.0f }, 1.0f);
		obstacles_.push_back(std::move(obs3));

		auto obs4 = std::make_unique<Obstacle>();
		obs4->Initialize(object3dCom, camera_, { 5.0f, 0.0f, 18.0f }, 1.0f);
		obstacles_.push_back(std::move(obs4));

		// --- 奥・ゴール前グループ (脱出リング手前のカバー用の配置) ---
		auto obs5 = std::make_unique<Obstacle>();
		obs5->Initialize(object3dCom, camera_, { -2.0f, 0.0f, 29.0f }, 1.0f);
		obstacles_.push_back(std::move(obs5));

		auto obs6 = std::make_unique<Obstacle>();
		obs6->Initialize(object3dCom, camera_, { 2.0f, 0.0f, 29.0f }, 1.0f);
		obstacles_.push_back(std::move(obs6));
	}
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

	// --- ✨ レベルエディタで配置された川・小石・大樹・アシ水草など全アセットを自動描画 ---
	if (levelEditor_)
	{
		levelEditor_->Draw(renderRequests);
	}

	if (player_)
	{
		player_->Draw(ctx);
	}

	if (combatSystem_)
	{
		for (auto& bullet : combatSystem_->GetBullets())
		{
			if (bullet)
			{
				bullet->Draw(ctx);
			}
		}
	}

	if (enemy_)
	{
		enemy_->Draw(ctx);
	}

	if (movingEnemy_)
	{
		movingEnemy_->Draw(ctx);
	}

	for (auto& obs : obstacles_)
	{
		if (obs)
		{
			obs->Draw(ctx);
		}
	}

	// 的の描画
	for (auto& t : targets_)
	{
		if (t)
		{
			t->Draw(ctx);
		}
	}

	if (goalRing_)
	{
		// 的が残っている間は、警告色である monsterBall.png をテクスチャとして渡して赤く見せる
		uint32_t ringTex = allTargetsDestroyed_ ? cylinderTextureIndex_ : TextureManager::GetInstance()->Load("Resources/monsterBall.png");
		D3D12_GPU_DESCRIPTOR_HANDLE handle = TextureManager::GetInstance()->GetSrvHandleGPU(ringTex);
		if (handle.ptr != 0)
		{
			goalRing_->Draw(handle);
		}
	}

	if (hitEffect_)
	{
		hitEffect_->Draw();
	}

	if (sphere_ && sphereInitialized)
	{
		if (player_ && player_->IsDodging())
		{
			// 回避中は青く半透明に光るシールドとしてオーバーレイ描画
			sphere_->SetOverlayDraw(true);
			D3D12_GPU_DESCRIPTOR_HANDLE handle = TextureManager::GetInstance()->GetSrvHandleGPU(particleTextureB); // 円形のテクスチャ
			if (handle.ptr != 0)
			{
				sphere_->Draw(handle);
			}
		}
		else if (!allTargetsDestroyed_)
		{
			// 的が残っている間は赤い半透明バリアとしてオーバーレイ描画
			sphere_->SetOverlayDraw(true);
			D3D12_GPU_DESCRIPTOR_HANDLE handle = TextureManager::GetInstance()->GetSrvHandleGPU(particleTextureB);
			if (handle.ptr != 0)
			{
				sphere_->Draw(handle);
			}
		}
	}

	if (spriteManager_)
	{
		spriteManager_->DrawAll(ctx, &debugCamera_, &sprites);
	}

#ifdef USE_IMGUI
	// 画面下部に常時表示する操作方法案内 UI
	{
		ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f - 240.0f, ImGui::GetIO().DisplaySize.y * 0.82f), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(480.0f, 100.0f), ImGuiCond_Always);
		ImGui::Begin("Tutorial Guide", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
		
		ImDrawList* dl = ImGui::GetWindowDrawList();
		ImVec2 min = ImGui::GetWindowPos();
		ImVec2 max = ImVec2(min.x + 480.0f, min.y + 100.0f);
		dl->AddRectFilled(min, max, IM_COL32(15, 20, 30, 200), 8.0f);
		dl->AddRect(min, max, IM_COL32(0, 255, 128, 255), 8.0f, 0, 1.5f);
		
		ImGui::SetCursorPos(ImVec2(20.0f, 15.0f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 1.0f, 0.6f, 1.0f));
		ImGui::TextWrapped((const char*)u8"【基本操作】\n[W][A][S][D] : 移動  /  [マウス] : 照準  /  [SPACE] : 回避\n[左クリック] : 通常射撃  /  [右クリック] : 散弾  /  [R] : リロード\n※ 3つの的をすべて壊し、奥の脱出エリアへ向かえ！");
		ImGui::PopStyleColor();
		
		ImGui::End();
	}

	// --- タクティカルレーザーサイトの描画 ---
	if (player_ && !player_->IsDead() && camera_)
	{
		ImGuiIO& io = ImGui::GetIO();
		float width = io.DisplaySize.x;
		float height = io.DisplaySize.y;
		if (width > 0.0f && height > 0.0f)
		{
			const Matrix4x4& vp = camera_->GetViewProjectionMatrix();
			ImDrawList* drawList = ImGui::GetForegroundDrawList();

			auto project3DTo2D = [&](const Vector3& pos3D, ImVec2& outPos) -> bool {
				float w = pos3D.x * vp.m[0][3] + pos3D.y * vp.m[1][3] + pos3D.z * vp.m[2][3] + vp.m[3][3];
				if (w <= 0.0f) return false;
				float x = (pos3D.x * vp.m[0][0] + pos3D.y * vp.m[1][0] + pos3D.z * vp.m[2][0] + vp.m[3][0]) / w;
				float y = (pos3D.x * vp.m[0][1] + pos3D.y * vp.m[1][1] + pos3D.z * vp.m[2][1] + vp.m[3][1]) / w;
				outPos.x = (x + 1.0f) * 0.5f * width;
				outPos.y = (1.0f - y) * 0.5f * height;
				return true;
			};

			float rotY = player_->GetRotation().y;
			Vector3 dir = { std::sin(rotY), 0.0f, std::cos(rotY) };
			// 実際の弾丸のスポーン位置計算 (前方 0.5f、高さ 0.2f) と完全に同期させる
			Vector3 gunPos = Bullet::ComputeSpawnPosition(player_->GetPosition(), dir, Vector3{ 0.0f, 0.2f, 0.5f });

			// レイキャストで障害物のみ(Obstacle)との衝突距離を測定 (自キャラのコライダーへの即時衝突を防ぐため)
			float laserRange = 25.0f; // 最大長
			bool hitObstacle = false;
			float closestDist = laserRange;
			
			auto& colliders = CollisionManager::GetInstance()->GetColliders();
			for (Collider* col : colliders)
			{
				if (!col || !col->IsEnabled() || col->GetAttribute() != CollisionAttribute::Obstacle)
				{
					continue;
				}

				CollisionData data;
				data.originalCollider = col;
				data.type = col->GetType();
				data.attribute = col->GetAttribute();
				data.worldPosition = col->GetWorldPosition();
				data.isTrigger = col->IsTrigger();

				if (data.type == ColliderType::Sphere)
				{
					SphereCollider* sphere = static_cast<SphereCollider*>(col);
					data.shape.radius = sphere->GetRadius();
				}
				else if (data.type == ColliderType::Box)
				{
					// レーザーサイトはフェンスの隙間をすり抜けられるよう、簡易BoxではなくMeshColliderで判定を行います
					continue;
					
					BoxCollider* box = static_cast<BoxCollider*>(col);
					data.shape.size = box->GetSize();
					data.shape.rotation = box->GetWorldRotation();
				}
				else if (data.type == ColliderType::Capsule)
				{
					CapsuleCollider* capsule = static_cast<CapsuleCollider*>(col);
					data.shape.radius = capsule->GetRadius();
					data.shape.height = capsule->GetHeight();
				}
				else if (data.type == ColliderType::Mesh)
				{
					// メッシュコライダーは追加の形状パラメータ設定不要
				}

				float dist = 0.0f;
				if (CollisionManager::CheckRayCollider(gunPos, dir, closestDist, data, dist))
				{
					closestDist = dist;
					hitObstacle = true;
				}
			}

			if (hitObstacle)
			{
				laserRange = closestDist;
			}

			Vector3 endPos = gunPos + dir * laserRange;
			ImVec2 start2D, end2D;
			if (project3DTo2D(gunPos, start2D) && project3DTo2D(endPos, end2D))
			{
				// レーザー光線 (赤い半透明ライン)
				drawList->AddLine(start2D, end2D, IM_COL32(255, 30, 30, 160), 2.0f);
				// 照準ドット (衝突位置の赤い点)
				drawList->AddCircleFilled(end2D, 4.0f, IM_COL32(255, 80, 80, 220));
				drawList->AddCircle(end2D, 6.0f, IM_COL32(255, 0, 0, 120), 0, 1.0f);
			}
		}
	}

	// デバッグ用の的復活ボタン (画面右下に固定配置して重なりを防ぐ)
	ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 220.0f, ImGui::GetIO().DisplaySize.y - 120.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(200.0f, 90.0f), ImGuiCond_FirstUseEver);
	ImGui::Begin("Debug Controls");
	if (ImGui::Button("Reset Targets"))
	{
		for (auto& t : targets_)
		{
			if (t) t->Reset();
		}
		allTargetsDestroyed_ = false;
	}
	ImGui::End();

	if (showDebugGizmos_ && camera_)
	{
		ImGuiIO& io = ImGui::GetIO();
		float width = io.DisplaySize.x;
		float height = io.DisplaySize.y;

		if (width > 0.0f && height > 0.0f)
		{
			const Matrix4x4& vp = camera_->GetViewProjectionMatrix();
			ImDrawList* drawList = ImGui::GetForegroundDrawList();

			auto project3DTo2D = [&](const Vector3& pos3D, ImVec2& outPos) -> bool {
				float w = pos3D.x * vp.m[0][3] + pos3D.y * vp.m[1][3] + pos3D.z * vp.m[2][3] + vp.m[3][3];
				if (w <= 0.0f) return false;
				float x = (pos3D.x * vp.m[0][0] + pos3D.y * vp.m[1][0] + pos3D.z * vp.m[2][0] + vp.m[3][0]) / w;
				float y = (pos3D.x * vp.m[0][1] + pos3D.y * vp.m[1][1] + pos3D.z * vp.m[2][1] + vp.m[3][1]) / w;
				outPos.x = (x + 1.0f) * 0.5f * width;
				outPos.y = (1.0f - y) * 0.5f * height;
				return true;
			};

			// 1. Draw Player Noise Ring
			if (playerSoundRadius_ > 0.0f && player_)
			{
				Vector3 playerPos = player_->GetPosition();
				const int numSegments = 32;
				std::vector<ImVec2> points2D;
				for (int i = 0; i <= numSegments; ++i)
				{
					float angle = i * (6.2831853f / numSegments);
					Vector3 p3D = {
						playerPos.x + std::cos(angle) * playerSoundRadius_,
						playerPos.y,
						playerPos.z + std::sin(angle) * playerSoundRadius_
					};
					ImVec2 p2D;
					if (project3DTo2D(p3D, p2D))
					{
						points2D.push_back(p2D);
					}
				}
				if (points2D.size() > 1)
				{
					float alpha = 1.0f - (playerSoundRadius_ / playerSoundMaxRadius_);
					ImU32 color = ImGui::ColorConvertFloat4ToU32({ 0.0f, 0.8f, 1.0f, alpha * 0.5f });
					drawList->AddPolyline(points2D.data(), (int)points2D.size(), color, false, 2.5f);
				}
			}

			// Helper to draw vision cone
			auto drawVisionCone = [&](const Vector3& enemyPos, float yaw, float fov, float maxRange, ImU32 color) {
				const int numSegments = 16;
				std::vector<ImVec2> points2D;
				ImVec2 center2D;
				if (project3DTo2D(enemyPos, center2D))
				{
					points2D.push_back(center2D);
				}
				else
				{
					return;
				}

				float halfFov = fov * 0.5f;
				float startAngle = yaw - halfFov;

				for (int i = 0; i <= numSegments; ++i)
				{
					float angle = startAngle + i * (fov / numSegments);
					Vector3 p3D = {
						enemyPos.x + std::sin(angle) * maxRange,
						enemyPos.y,
						enemyPos.z + std::cos(angle) * maxRange
					};
					ImVec2 p2D;
					if (project3DTo2D(p3D, p2D))
					{
						points2D.push_back(p2D);
					}
				}

				if (points2D.size() > 2)
				{
					drawList->AddPolyline(points2D.data(), (int)points2D.size(), color, true, 2.0f);
					ImU32 fillColor = (color & 0x00FFFFFF) | 0x15000000;
					drawList->AddConvexPolyFilled(points2D.data(), (int)points2D.size(), fillColor);
				}
			};

			// 2. Draw Enemy Vision Cone & Stats
			if (enemy_ && !enemy_->IsDead())
			{
				Vector3 pos = enemy_->GetPosition();
				float yaw = enemy_->GetYaw();
				float fov = enemy_->GetFieldOfView();
				float range = enemy_->GetMaxSightRange();
				
				ImU32 color = ImGui::ColorConvertFloat4ToU32({ 0.0f, 1.0f, 0.0f, 0.8f });
				std::string stateName = "PATROL";
				if (enemy_->GetAIState() == Enemy::AIState::Investigate)
				{
					color = ImGui::ColorConvertFloat4ToU32({ 1.0f, 0.6f, 0.0f, 0.8f });
					stateName = "INVESTIGATE";
				}
				else if (enemy_->GetAIState() == Enemy::AIState::Chase)
				{
					color = ImGui::ColorConvertFloat4ToU32({ 1.0f, 0.1f, 0.1f, 0.8f });
					stateName = "CHASE";
				}

				drawVisionCone(pos, yaw, fov, range, color);

				ImVec2 head2D;
				if (project3DTo2D(pos + Vector3{ 0.0f, 1.8f, 0.0f }, head2D))
				{
					char buf[128];
					sprintf_s(buf, "%s (Alert: %.0f%%)", stateName.c_str(), enemy_->GetDetectionMeter() * 100.0f);
					drawList->AddText(ImVec2(head2D.x - 50.0f, head2D.y), color, buf);
				}
			}

			// 3. Draw MovingEnemy Vision Cone & Stats
			if (movingEnemy_ && !movingEnemy_->IsDead())
			{
				Vector3 pos = movingEnemy_->GetPosition();
				float yaw = movingEnemy_->GetYaw();
				float fov = movingEnemy_->GetFieldOfView();
				float range = movingEnemy_->GetMaxSightRange();
				
				ImU32 color = ImGui::ColorConvertFloat4ToU32({ 0.0f, 1.0f, 0.0f, 0.8f });
				std::string stateName = "PATROL";
				if (movingEnemy_->GetAIState() == MovingEnemy::AIState::Investigate)
				{
					color = ImGui::ColorConvertFloat4ToU32({ 1.0f, 0.6f, 0.0f, 0.8f });
					stateName = "INVESTIGATE";
				}
				else if (movingEnemy_->GetAIState() == MovingEnemy::AIState::Chase)
				{
					color = ImGui::ColorConvertFloat4ToU32({ 1.0f, 0.1f, 0.1f, 0.8f });
					stateName = "CHASE";
				}

				drawVisionCone(pos, yaw, fov, range, color);

				ImVec2 head2D;
				if (project3DTo2D(pos + Vector3{ 0.0f, 1.8f, 0.0f }, head2D))
				{
					char buf[128];
					sprintf_s(buf, "%s (Alert: %.0f%%)", stateName.c_str(), movingEnemy_->GetDetectionMeter() * 100.0f);
					drawList->AddText(ImVec2(head2D.x - 50.0f, head2D.y), color, buf);
				}
			}
		}
	}
#endif

	renderRequests.sceneDrawn = true;
}

void GamePlayScene::AddFloatingText(const Vector3& worldPos, const std::string& text, const Vector4& color, bool isCritical)
{
	FloatingText ft;
	ft.position = worldPos;
	// 敵などの位置から少しランダムにずらして被りを防ぐ
	ft.position.x += ((float)rand() / RAND_MAX * 2.0f - 1.0f) * 0.3f;
	ft.position.z += ((float)rand() / RAND_MAX * 2.0f - 1.0f) * 0.3f;
	ft.text = text;
	ft.color = color;
	ft.maxLifeTime = isCritical ? 1.0f : 0.75f;
	ft.lifeTime = ft.maxLifeTime;
	ft.isCritical = isCritical;
	floatingTexts_.push_back(ft);
}


