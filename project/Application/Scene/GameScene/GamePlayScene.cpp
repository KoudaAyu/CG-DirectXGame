
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
#include "Application/Config/GameConfig.h"
#include "RaidStats.h"
#include "Baziru3_Engine/Core/Base/Allocator/ConstantBufferAllocator.h"
#include "imgui.h"
#include "imgui_internal.h"


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
	// 📷 カメラの初期化（Escape from Duckov 斜め45度トップダウン見下ろし視点）
	if (camera_)
	{
		camera_->SetTranslate(Vector3{ 0.0f, 20.0f, -20.0f });
		camera_->SetRotate({ 0.785f, 0.0f, 0.0f });
		camera_->Update();
	}

	// 標的リストの初期化（InitializeObstacles内でstage_layout.jsonから読み込まれます）
	targets_.clear();
	allTargetsDestroyed_ = false;
	isGameCleared_ = false;
	isPlayerInExtractionZone_ = false;
	extractionTimer_ = kExtractionMaxTime;
	goalRingTransform_.translate = GameConfig::Environment::kExtractionPadPosition;

	InitializeEnvironment();
	InitializeCharacters();
	InitializeSprites();
	InitializeAudioAndParticles();
	InitializeObstacles();

	// サブシステム初期化 (LootSystem & GamePlayHUD & Combat & Collision)
	lootSystem_ = std::make_unique<LootSystem>();
	lootSystem_->Initialize();
	hud_ = std::make_unique<GamePlayHUD>();
	hud_->Initialize();
	combatSystem_ = std::make_unique<CombatSystem>(this);
	collisionSystem_ = std::make_unique<CollisionSystem>(this);
	RaidStats::GetInstance().Reset();

	// 視野コーン（Vision Cone）やデバッグギズモはデフォルト非表示（F1でいつでも切替可能）
	showDebugGizmos_ = false;
	showPerformanceTracker_ = false;
	CollisionManager::GetInstance()->SetShowDebugColliders(false);
	CollisionManager::GetInstance()->SetShowMeshWireframe(false);
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

	sphere_ = std::make_unique<Sphere>();
	sphere_->Initialize(directXCom, object3dCom, materialManager, light, camera_);

	// 立体脱出ヘリパッド (extraction_pad.obj + extraction_pad.png)
	{
		Object3d::ModelData padModel = Object3d::LoadObjFile("Resources", "extraction_pad.obj");
		padModel.material.textureFilePath = "Resources/extraction_pad.png";
		extractionPadTextureIndex_ = TextureManager::GetInstance()->Load("Resources/extraction_pad.png");
		padModel.material.textureIndex = extractionPadTextureIndex_;

		extractionPadObject_ = std::make_unique<Object3d>();
		extractionPadObject_->Initialize(object3dCom, padModel);
		extractionPadObject_->SetCamera(camera_);
		extractionPadObject_->SetTranslate(GameConfig::Environment::kExtractionPadPosition);
		extractionPadObject_->SetScale({ 1.0f, 1.0f, 1.0f });
		extractionPadObject_->SetRotate({ 0.0f, 0.0f, 0.0f });
		extractionPadObject_->Update();
	}

	isGameCleared_ = false;
	extractionTimer_ = kExtractionMaxTime;

	// --- ✨ LevelEditorはImGui編集UI用に初期化のみ（DrawはInitializeObstacles側で一括処理するため二重ロード不要）---
	levelEditor_ = std::make_unique<LevelEditor>();
	levelEditor_->Initialize(directXCom, object3dCom);
	// levelEditor_->LoadFromFile("Resources/stage_layout.json"); // InitializeObstaclesと二重GPUバッファ作成になるため無効化
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

		// エンジン層のスプライン曲線等速移動システムを活用した戦術巡回ルート（敵陣地エリアを巡るCatmull-Rom曲線）
		std::vector<Vector3> patrolSpline = {
			{ -10.0f, 0.0f, 25.0f },
			{  -3.0f, 0.0f, 27.5f },
			{   3.0f, 0.0f, 25.5f },
			{  10.0f, 0.0f, 27.0f }
		};
		movingEnemy_->SetPatrolPath(patrolSpline, false, 2.0f); // 往復PingPong巡回
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

	// 回避スタミナバー（HPバーの下に配置）
	auto psBg = Sprite::Create(sc, defaultTransform, "Resources/CG4/human/white.png");
	auto psFg = Sprite::Create(sc, defaultTransform, "Resources/CG4/human/white.png");
	if (psBg && psFg)
	{
		psBg->SetAnchorPoint({ 0.0f, 0.0f });
		psFg->SetAnchorPoint({ 0.0f, 0.0f });
		psBg->SetPosition({ 20.0f, 40.0f });
		psBg->SetSize({ 200.0f, 10.0f });
		psBg->SetColor({ 0.1f, 0.1f, 0.1f, 0.8f });
		psFg->SetPosition({ 20.0f, 40.0f });
		psFg->SetSize({ 200.0f, 10.0f });
		psFg->SetColor({ 0.0f, 0.8f, 1.0f, 1.0f }); // エナジーシアン
		sprites.emplace_back(std::move(psBg));
		sprites.emplace_back(std::move(psFg));
		playerStaminaBarBg_ = sprites[sprites.size() - 2].get();
		playerStaminaBarFg_ = sprites[sprites.size() - 1].get();
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
	bloodTextureIndex_ = TextureManager::GetInstance()->Load("Resources/blood_splatter.png");
	smokeTextureIndex_ = TextureManager::GetInstance()->Load("Resources/smoke_dark.png");
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

	if (goalRing_)
	{
		goalRing_->Finalize();
		goalRing_.reset();
	}

	if (hud_) { hud_.reset(); }
	if (lootSystem_) { lootSystem_.reset(); }
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
	constexpr float kExtractionRadius = 2.5f;

	isPlayerInExtractionZone_ = (dist <= kExtractionRadius);

	if (isPlayerInExtractionZone_ && allTargetsDestroyed_)
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
		extractionTimer_ = kExtractionMaxTime;
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
	if (sphere_)
	{
		Sprite::Transform transformSphere = sphere_->GetTransform();
		transformSphere.rotate.y += 0.05f; // バリア感を出すために少し速めに回転
		
		if (!allTargetsDestroyed_)
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



	if (hitEffect_)
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

	// 1. プレイヤーのアクション連動GPUパーティクル (ローリング煙 ＆ 足元土埃)
	if (particleManager && appParticleManager_ && player_ && !player_->IsDead())
	{
		Vector3 pPos = player_->GetPosition();
		if (player_->IsDodging())
		{
			// ローリング回避中: 進行方向の逆へ足元から吹き出す土煙 (0.07秒おきに放出)
			static float dodgeDustTimer = 0.0f;
			dodgeDustTimer += deltaTime;
			if (dodgeDustTimer >= GameConfig::Player::kDodgeDustInterval)
			{
				Vector3 pRot = player_->GetRotation();
				Vector3 dodgeDir = { std::sin(pRot.y), 0.0f, std::cos(pRot.y) };
				appParticleManager_->EmitDodgeRollDust(particleManager->GetRandomEngine(), pPos, dodgeDir, smokeTextureIndex_);
				dodgeDustTimer = 0.0f;
			}
		}
		else if (player_->IsMoving())
		{
			// 走り移動中: 足元から舞い上がる土埃
			static float stepTimer = 0.0f;
			stepTimer += deltaTime;
			if (stepTimer >= GameConfig::Player::kStepDustInterval)
			{
				appParticleManager_->EmitFootstepDust(particleManager->GetRandomEngine(), pPos, smokeTextureIndex_);
				stepTimer = 0.0f;
			}
		}
	}

	// 2. 脱出ヘリパッド稼働時の上昇光粒子流 ＆ 風圧ダスト
	if (particleManager && appParticleManager_ && allTargetsDestroyed_)
	{
		appParticleManager_->EmitHelipadBeaconMotes(particleManager->GetRandomEngine(), goalRingTransform_.translate, particleTextureB);

		escapeSmokeTimer_ += deltaTime;
		if (escapeSmokeTimer_ >= 0.06f)
		{
			for (int i = 0; i < 2; ++i)
			{
				appParticleManager_->EmitDust(
					particleManager->GetRandomEngine(),
					goalRingTransform_.translate,
					1.6f,
					{ 0.1f, 0.9f, 0.5f, 0.45f },
					particleTextureB
				);
			}
			escapeSmokeTimer_ = 0.0f;
		}
	}

	// 3. 川 (River) エリアのさざ波・水しぶき
	if (particleManager && appParticleManager_)
	{
		// (A) 川の流れに沿って流れる上品なさざ波 (0.15秒おきに放出)
		static float riverWaveTimer = 0.0f;
		riverWaveTimer += deltaTime;
		if (riverWaveTimer >= GameConfig::Environment::kRiverWaveInterval)
		{
			appParticleManager_->EmitRiverWaveRipples(particleManager->GetRandomEngine(), particleTextureB);
			riverWaveTimer = 0.0f;
		}

		// (B) 川面のパチパチ跳ねる水滴 (0.25秒おき)
		static float riverSplashTimer = 0.0f;
		riverSplashTimer += deltaTime;
		if (riverSplashTimer >= GameConfig::Environment::kRiverSplashInterval)
		{
			std::uniform_real_distribution<float> rxDist(-18.0f, 18.0f);
			std::uniform_real_distribution<float> rzDist(GameConfig::Environment::kRiverZMin, GameConfig::Environment::kRiverZMax);
			Vector3 sPos = { rxDist(particleManager->GetRandomEngine()), GameConfig::Environment::kRiverSplashY, rzDist(particleManager->GetRandomEngine()) };
			appParticleManager_->EmitRiverSplashDroplet(particleManager->GetRandomEngine(), sPos, particleTextureB);
			riverSplashTimer = 0.0f;
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
				// リロード完了演出（控えめな薬莢排出とマズルフラッシュのみ）
				if (particleManager && appParticleManager_)
				{
					appParticleManager_->EmitMuzzleFlare(
						particleManager->GetRandomEngine(),
						playerPos + Vector3{ 0.0f, 0.4f, 0.0f },
						0.4f,
						{ 1.0f, 0.9f, 0.6f, 0.8f },
						0.05f,
						particleTextureB
					);
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
					
					// 足元から全方位（8方向）に広がる軽快な土煙の輪（キックオフ演出）
					for (int i = 0; i < 8; ++i)
					{
						float angle = i * (6.2831853f / 8.0f);
						Vector3 vel = { std::cos(angle) * 2.2f, 0.25f, std::sin(angle) * 2.2f };
						appParticleManager_->EmitDustWithVelocity(
							particleManager->GetRandomEngine(),
							playerPos + Vector3{ 0.0f, 0.05f, 0.0f },
							0.65f, // プレイヤーを隠さない適正スケール
							{ 0.85f, 0.80f, 0.75f, 0.5f },
							vel,
							0.30f,
							smokeTextureIndex_
						);
					}
					prevDodge = true;
				}
			}
			else
			{
				if (prevDodge)
				{
					// 回避終了時の着地衝撃波（土煙の放射状バースト ＆ カメラブレ）
					TriggerCameraShake(0.12f, 0.25f);
					for (int i = 0; i < 8; ++i)
					{
						float angle = i * (6.2831853f / 8.0f);
						Vector3 vel = { std::cos(angle) * 2.5f, 0.35f, std::sin(angle) * 2.5f };
						appParticleManager_->EmitDustWithVelocity(
							particleManager->GetRandomEngine(),
							playerPos + Vector3{ 0.0f, 0.05f, 0.0f },
							0.8f, // プレイヤーを隠さない適正スケール
							{ 0.85f, 0.80f, 0.75f, 0.5f },
							vel,
							0.35f,
							smokeTextureIndex_
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
	bool isPlayerInCover = false;
	if (player_ && !player_->IsDead())
	{
		playerPosTarget = player_->GetPosition();
		target = &playerPosTarget;

		// プレイヤーと遮蔽物（Obstacle）の近接遮蔽チェック（1.4m以内なら遮蔽中）
		for (const auto& obs : obstacles_)
		{
			if (!obs) continue;
			Vector3 obsPos = obs->GetPosition();
			float dx = playerPosTarget.x - obsPos.x;
			float dz = playerPosTarget.z - obsPos.z;
			float distSq = dx * dx + dz * dz;
			if (distSq <= 1.4f * 1.4f)
			{
				isPlayerInCover = true;
				break;
			}
		}
		player_->SetInCover(isPlayerInCover);
	}

	enemy_->Update(windowAPI, target, obstacles_, deltaTime, isPlayerInCover);

	if (movingEnemy_)
	{
		movingEnemy_->Update(windowAPI, target, obstacles_, deltaTime, isPlayerInCover);
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

	// 回避スタミナバーの更新
	if (playerStaminaBarBg_ && playerStaminaBarFg_)
	{
		const float sRatio = player_->GetStaminaRatio();
		playerStaminaBarFg_->SetSize({ 200.0f * sRatio, 10.0f });
		// スタミナ不足時（30%未満: 回避1回分未満）はオレンジ警告色
		if (sRatio < 0.30f)
		{
			playerStaminaBarFg_->SetColor({ 1.0f, 0.4f, 0.1f, 1.0f });
		}
		else
		{
			playerStaminaBarFg_->SetColor({ 0.0f, 0.85f, 1.0f, 1.0f });
		}
		playerStaminaBarBg_->Update();
		playerStaminaBarFg_->Update();
	}

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
		if (!isDeathSequenceActive_)
		{
			isDeathSequenceActive_ = true;
			deathSequenceTimer_ = kDeathSequenceDuration;
		}
	}
}

void GamePlayScene::Update()
{
	float realDeltaTime = AdvanceDeltaTime();
	float deltaTime = realDeltaTime;

	// 死亡シーケンス中: SPACEキーまたはENTERキーが押されるまで待機
	if (isDeathSequenceActive_)
	{
		deltaTime *= 0.15f; // ゲーム内時間をスローダウン

		// SPACEキーまたはENTERキーでフェード（GAMEOVER）へ移行
		bool proceedPressed = false;
		if ((GetAsyncKeyState(VK_SPACE) & 0x8000) != 0 || (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0)
		{
			proceedPressed = true;
		}
#ifdef USE_IMGUI
		if (ImGui::IsKeyPressed(ImGuiKey_Space) || ImGui::IsKeyPressed(ImGuiKey_Enter))
		{
			proceedPressed = true;
		}
#endif
		if (proceedPressed)
		{
			SceneManager::GetInstance()->ChangeScene("GAMEOVER");
			return;
		}
	}

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
			RaidStats::GetInstance().isSurvived = true;
			RaidStats::GetInstance().totalLootValue = player_ ? player_->GetLootValue() : 0;
			SceneManager::GetInstance()->ChangeScene("CLEAR");
			return;
		}
	}
	else if (!isDeathSequenceActive_)
	{
		// レイド時間の進行 ＆ MIA (時間切れロスト) 判定
		RaidStats::GetInstance().raidTime += realDeltaTime;
		if (RaidStats::GetInstance().GetRemainingTime() <= 0.0f && player_ && !player_->IsDead())
		{
			RaidStats::GetInstance().isMIA = true;
			player_->TakeDamage(9999.0f, "DUCKOV AIRSPACE LOCKDOWN", "MIA (MISSING IN ACTION) - RAID TIME EXPIRED");
		}
	}

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
	if (player_)
	{
		player_->PostCollisionUpdate();
	}

	// 📷 カメラのプレイヤー追従＆シネマティック更新（全オブジェクトの更新前に最新フレーム位置を反映）
	if (camera_ && player_)
	{
		Vector3 playerPos = player_->GetPosition();

		if (isGameCleared_)
		{
			// クリア時のシネマティックズームイン (プレイヤーアヒルに徐々に寄る)
			Vector3 targetOffset = { 0.0f, 6.0f, -7.0f };
			Vector3 currentTranslate = camera_->GetTranslate();
			Vector3 targetTranslate = playerPos + targetOffset;
			
			currentTranslate.x += (targetTranslate.x - currentTranslate.x) * 0.08f;
			currentTranslate.y += (targetTranslate.y - currentTranslate.y) * 0.08f;
			currentTranslate.z += (targetTranslate.z - currentTranslate.z) * 0.08f;
			camera_->SetTranslate(currentTranslate);
		}
		else
		{
			// 通常プレイ時の斜め見下ろしプレイヤー追従
			Vector3 targetCamPos = playerPos + Vector3{ 0.0f, 20.0f, -20.0f };
			camera_->SetTranslate(targetCamPos);
			camera_->SetRotate({ 0.785f, 0.0f, 0.0f });
		}

		// カメラシェイク（被弾・爆発時の画面揺れ）
		if (cameraShakeTime_ > 0.0f)
		{
			cameraShakeTime_ -= realDeltaTime;
			float progress = cameraShakeTime_ / cameraShakeDurationMax_;
			float currentIntensity = cameraShakeIntensity_ * progress * progress;
			float rx = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * currentIntensity;
			float ry = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * currentIntensity;
			float rz = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * currentIntensity;
			camera_->SetTranslate(camera_->GetTranslate() + Vector3{ rx, ry, rz });
		}

		camera_->Update();
	}

	UpdateExtractionGoal(deltaTime);
	if (lootSystem_)
	{
		lootSystem_->Update(deltaTime, player_.get(), appParticleManager_.get(), floatingTexts_);
	}

	UpdateEnvironment();
	UpdateObstacles();

	// 標的の更新
	for (auto& t : targets_)
	{
		if (t) t->Update(deltaTime);
	}

	// すべての的が破壊されたかチェック
	int totalTargets = static_cast<int>(targets_.size());
	int destroyedCount = 0;
	bool anyTargetAlive = false;
	for (const auto& t : targets_)
	{
		if (t && !t->IsDead())
		{
			anyTargetAlive = true;
		}
		else if (t && t->IsDead())
		{
			destroyedCount++;
		}
	}
	allTargetsDestroyed_ = (totalTargets > 0 && !anyTargetAlive && destroyedCount >= totalTargets);

	UpdatePlayerHpBar();
	CheckGameOver();
	if (sceneEntranceFadeTimer_ > 0.0f)
	{
		sceneEntranceFadeTimer_ -= realDeltaTime;
		if (sceneEntranceFadeTimer_ < 0.0f) sceneEntranceFadeTimer_ = 0.0f;
	}

	UpdateStressTestMode();

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

		light->SetDirectionalLightIntensity(intensity);
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
	std::vector<std::string> pathCandidates = {
		"Resources/stage_layout.json",
		"project/Resources/stage_layout.json",
		"../project/Resources/stage_layout.json"
	};

	for (const auto& filepath : pathCandidates)
	{
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
					std::string name = obj.value("name", "");
					std::string type = obj.value("type", "");
					if (!name.empty() && name != "GroundPlane" && type != "SpawnPoint" && type != "GoalRing" && name != "Player_Spawn_Point" && name != "Enemy_Spawn_Point" && name != "Goal_Extraction_Ring")
					{
						Vector3 pos = {
							obj["position"]["x"].get<float>(),
							obj["position"]["y"].get<float>(),
							obj["position"]["z"].get<float>()
						};
						Vector3 scl = { 1.0f, 1.0f, 1.0f };
						if (obj.contains("scale"))
						{
							scl = {
								obj["scale"]["x"].get<float>(),
								obj["scale"]["y"].get<float>(),
								obj["scale"]["z"].get<float>()
							};
						}
						Vector3 rot = { 0.0f, 0.0f, 0.0f };
						if (obj.contains("rotation"))
						{
							rot = {
								obj["rotation"]["x"].get<float>(),
								obj["rotation"]["y"].get<float>(),
								obj["rotation"]["z"].get<float>()
							};
						}
						std::string modelFile = obj.value("modelFilename", "fence.obj");
						if (type == "Target" || name.find("Target") != std::string::npos || name.find("target") != std::string::npos)
						{
							auto target = std::make_unique<Target>();
							float rad = obj.value("radius", 0.8f);
							target->Initialize(object3dCom, camera_, pos, rad);
							targets_.push_back(std::move(target));
							continue;
						}
						else if (type == "River" || name.find("River") != std::string::npos || name.find("river") != std::string::npos || name.find("Water") != std::string::npos || name.find("water") != std::string::npos)
						{
							modelFile = "river.obj";
						}
						else if (modelFile == "plane.obj" || name.find("Ground") != std::string::npos || name.find("Plane") != std::string::npos)
						{
							continue; // 不要な中央巨大板 plane.obj の生成をスキップ
						}
						
						auto obs = std::make_unique<Obstacle>();
						obs->Initialize(object3dCom, camera_, pos, 1.0f, modelFile, scl, rot);
						obstacles_.push_back(std::move(obs));
					}
				}
				// ステージ最下層に25x25mタイル16枚（4x4グリッド）で地面を敷く
				// ground.obj単体(100x100m)だとフラスタムカリングで端が消えるため分割する
				{
					const float tileSize = 25.0f; // タイル1枚のサイズ
					const int gridW = 4;
					const int gridH = 4;
					// グリッドの中心を (0, -0.01, 15) に合わせる
					const float startX = 0.0f - tileSize * gridW * 0.5f + tileSize * 0.5f;
					const float startZ = 15.0f - tileSize * gridH * 0.5f + tileSize * 0.5f;
					for (int gz = 0; gz < gridH; ++gz)
					{
						for (int gx = 0; gx < gridW; ++gx)
						{
							Vector3 tilePos = { startX + gx * tileSize, -0.01f, startZ + gz * tileSize };
							auto tile = std::make_unique<Obstacle>();
							tile->Initialize(object3dCom, camera_, tilePos, 1.0f, "ground_tile.obj", { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f });
							obstacles_.insert(obstacles_.begin(), std::move(tile));
						}
					}
				}

				// 橋の下を東西に横断する美しいクリアブルーの川水面を配置（幅60m x 奥行き5m）
				// ※ 地面とのZファイト防止のため Y=kRiverSurfaceY に配置
				auto river = std::make_unique<Obstacle>();
				river->Initialize(object3dCom, camera_, GameConfig::Environment::kRiverPosition, 1.0f, "river.obj", GameConfig::Environment::kRiverScale, GameConfig::Environment::kRiverRotation);
				obstacles_.insert(obstacles_.begin() + 1, std::move(river));


				success = true;
				OutputDebugStringA("GamePlayScene: Successfully loaded obstacles from JSON.\n");
				break;

			}
			catch (const std::exception& e)
			{
				char errorMsg[256];
				sprintf_s(errorMsg, "GamePlayScene: Failed to parse stage_layout.json: %s\n", e.what());
				OutputDebugStringA(errorMsg);
			}
		}
	}

	// 読み込みに失敗した、またはデータが不足していた場合はコンテナ＋直立フェンスの最新デフォルト配置を使用
	if (!success || obstacles_.size() <= 2)
	{
		OutputDebugStringA("GamePlayScene: Using updated default obstacle placement.\n");

		// ステージ最下層に25x25mタイル16枚（4x4グリッド）で地面を敷く
		{
			const float tileSize = 25.0f;
			const int gridW = 4;
			const int gridH = 4;
			const float startX = 0.0f - tileSize * gridW * 0.5f + tileSize * 0.5f;
			const float startZ = 15.0f - tileSize * gridH * 0.5f + tileSize * 0.5f;
			for (int gz = 0; gz < gridH; ++gz)
			{
				for (int gx = 0; gx < gridW; ++gx)
				{
					Vector3 tilePos = { startX + gx * tileSize, -0.01f, startZ + gz * tileSize };
					auto tile = std::make_unique<Obstacle>();
					tile->Initialize(object3dCom, camera_, tilePos, 1.0f, "ground_tile.obj", { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f });
					obstacles_.push_back(std::move(tile));
				}
			}
		}

		// コンテナ2個
		auto c1 = std::make_unique<Obstacle>();
		c1->Initialize(object3dCom, camera_, { -2.5f, 0.0f, 12.0f }, 1.0f, "container.obj", { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f });
		obstacles_.push_back(std::move(c1));

		auto c2 = std::make_unique<Obstacle>();
		c2->Initialize(object3dCom, camera_, { 2.5f, 0.0f, 12.0f }, 1.0f, "container.obj", { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f });
		obstacles_.push_back(std::move(c2));

		// 直立フェンス（川 Z=18.75m の手前と奥に配置）
		std::vector<Vector3> fencePositions = {
			{ -5.0f, 0.0f, 8.0f },
			{ 5.0f, 0.0f, 8.0f },
			{ -5.0f, 0.0f, 15.5f },
			{ 5.0f, 0.0f, 15.5f },
			{ -2.0f, 0.0f, 29.0f },
			{ 2.0f, 0.0f, 29.0f }
		};
		for (const auto& fpos : fencePositions)
		{
			auto fobs = std::make_unique<Obstacle>();
			fobs->Initialize(object3dCom, camera_, fpos, 1.0f, "fence.obj", { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f });
			obstacles_.push_back(std::move(fobs));
		}
	}

	if (targets_.empty())
	{
		auto t1 = std::make_unique<Target>();
		t1->Initialize(object3dCom, camera_, { -4.5f, 0.0f, 9.0f }, 0.8f);
		targets_.push_back(std::move(t1));

		auto t2 = std::make_unique<Target>();
		t2->Initialize(object3dCom, camera_, { 4.5f, 0.0f, 13.0f }, 0.8f);
		targets_.push_back(std::move(t2));

		auto t3 = std::make_unique<Target>();
		t3->Initialize(object3dCom, camera_, { -2.5f, 0.0f, 25.5f }, 0.8f);
		targets_.push_back(std::move(t3));
	}

	// --- 📜 8/31 チュートリアル用看板 (TutorialSign) の完全配置 ---
	tutorialSigns_.clear();
	{
		// 看板1: 初期位置（基本移動＆回避ローリング）
		auto sign1 = std::make_unique<TutorialSign>();
		sign1->Initialize(object3dCom, camera_, { -2.5f, 0.0f, 2.0f },
			(const char*)u8"【 基本操作訓練 】\n[ W ][ A ][ S ][ D ] : 移動\n[ SPACE ] : 回避ローリング（素早く前転・無敵時間あり！）", 3.2f);
		tutorialSigns_.push_back(std::move(sign1));

		// 看板2: 射撃練習エリア（照準・射撃・リロード）
		auto sign2 = std::make_unique<TutorialSign>();
		sign2->Initialize(object3dCom, camera_, { 2.5f, 0.0f, 7.5f },
			(const char*)u8"【 射撃・リロード訓練 】\n[ マウス ] : 照準  /  [ 左クリック ] : 射撃\n[ R ] : リロード（弾込め）\n※配置された ★ 標的（Target）をすべて破壊せよ！", 3.2f);
		tutorialSigns_.push_back(std::move(sign2));

		// 看板3: 遮蔽・戦術エリア（COVER＆ステルス）
		auto sign3 = std::make_unique<TutorialSign>();
		sign3->Initialize(object3dCom, camera_, { -3.0f, 0.0f, 15.0f },
			(const char*)u8"【 戦術遮蔽（COVER）訓練 】\nコンテナや土嚢のそばに行くと 自動的に ◆ COVER 状態！\n敵の視界が大幅に遮られ、見つかりにくくなるぞ！", 3.2f);
		tutorialSigns_.push_back(std::move(sign3));

		// 看板4: 脱出エリア（ヘリパッド脱出目標）
		auto sign4 = std::make_unique<TutorialSign>();
		sign4->Initialize(object3dCom, camera_, { 2.5f, 0.0f, 26.5f },
			(const char*)u8"【 レイド脱出訓練 】\nすべての標的を破壊後、最奥の ◆ 脱出パッド へ向かえ！\nパッド内でカウントダウン完了で生還（CLEAR）だ！", 3.2f);
		tutorialSigns_.push_back(std::move(sign4));
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

	if (player_)
	{
		Vector3 pPos = player_->GetPosition();
		for (auto& sign : tutorialSigns_)
		{
			if (sign)
			{
				sign->Update(pPos);
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
	// 描画直前にカメラを更新して最新フレームのGPU仮想アドレスを確定
	if (camera_)
	{
		camera_->Update();
	}

	renderRequests.sceneDrawn = true;
	const RenderContext ctx = BuildRenderContext();





	// Draw Skybox
	SceneManager::GetInstance()->DrawSkybox(ctx.commandList);

	// --- ✨ レベルエディタの重複描画を防止し、obstacles_側の本物モデル(コンテナ・フェンス)のみを描画 ---
	// if (levelEditor_)
	// {
	// 	levelEditor_->Draw(renderRequests);
	// }

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

	for (auto& obs : stressTestObstacles_)
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

	// 📜 チュートリアル看板の描画
	for (auto& sign : tutorialSigns_)
	{
		if (sign)
		{
			sign->Draw(ctx);
		}
	}

	// 立体脱出ヘリパッドの描画
	if (extractionPadObject_)
	{
		if (allTargetsDestroyed_)
		{
			// 的全滅時: 脱出許可（エメラルドグリーンのアクティブ点灯）
			extractionPadObject_->SetColor({ 1.2f, 1.2f, 1.2f, 1.0f });
		}
		else
		{
			// 的が残っている間: 警戒ロック状態（少し暗め）
			extractionPadObject_->SetColor({ 0.75f, 0.75f, 0.75f, 1.0f });
		}
		extractionPadObject_->Draw(ctx);
	}

	if (hitEffect_)
	{
		hitEffect_->Draw();
	}

	// パーティクル描画（AppParticleManager 自前パイプラインで直接レンダリング）
	// ※ ParticleRenderer::Draw() は GetNumInstance()==0 で早期リターンするため
	//   エンジン経由ではなく Draw(ctx) で自前のシェーダーパイプラインを使って描画する
	if (appParticleManager_ && particleManager)
	{
		RenderContext particleCtx = ctx;
		// スモーク用テクスチャをデフォルトのパーティクルテクスチャとして設定
		if (smokeTextureIndex_ != TextureManager::kInvalidTextureIndex)
		{
			particleCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(smokeTextureIndex_);
		}
		appParticleManager_->Draw(particleCtx);
	}

	if (spriteManager_)
	{
		spriteManager_->DrawAll(ctx, &debugCamera_, &sprites);
	}

#ifdef USE_IMGUI
	// --- タクティカルHUD統合描画 ---
	if (hud_)
	{
		GamePlayHUDContext hudCtx;
		hudCtx.camera = camera_;
		hudCtx.player = player_.get();
		hudCtx.enemy = enemy_.get();
		hudCtx.movingEnemy = movingEnemy_.get();
		hudCtx.obstacles = &obstacles_;
		hudCtx.targets = &targets_;
		hudCtx.tutorialSigns = &tutorialSigns_;
		hudCtx.lootProps = lootSystem_ ? &lootSystem_->GetProps() : nullptr;
		hudCtx.floatingTexts = &floatingTexts_;
		hudCtx.extractionGoalPos = goalRingTransform_.translate;
		hudCtx.isReadyToExtract = allTargetsDestroyed_;
		hudCtx.isGameCleared = isGameCleared_;
		hudCtx.isDeathSequenceActive = isDeathSequenceActive_;
		hudCtx.deathSequenceTimer = deathSequenceTimer_;
		hudCtx.hitIndicatorTimer = hitIndicatorTimer_;
		hudCtx.hitIndicatorAngle = hitIndicatorAngle_;
		hudCtx.playerSoundRadius = playerSoundRadius_;
		hudCtx.playerSoundMaxRadius = playerSoundMaxRadius_;
		hudCtx.playerSoundTimer = playerSoundTimer_;
		hudCtx.showDebugGizmos = showDebugGizmos_;
		hudCtx.showPerformanceTracker = showPerformanceTracker_;
		hudCtx.isStressTestActive = isStressTestActive_;
		hudCtx.stressTestCount = static_cast<int>(stressTestObstacles_.size());
		hudCtx.sceneEntranceFadeTimer = sceneEntranceFadeTimer_;

		hud_->Draw(hudCtx, AdvanceDeltaTime());
	}
#endif

	renderRequests.sceneDrawn = true;
}

void GamePlayScene::AddFloatingText(const Vector3& worldPos, const std::string& text, const Vector4& color, bool isCritical)
{
	FloatingText ft;
	ft.position = worldPos;
	ft.position.x += ((float)rand() / RAND_MAX * 2.0f - 1.0f) * 0.3f;
	ft.position.z += ((float)rand() / RAND_MAX * 2.0f - 1.0f) * 0.3f;
	ft.text = text;
	ft.color = color;
	ft.maxLifeTime = isCritical ? 1.0f : 0.75f;
	ft.lifeTime = ft.maxLifeTime;
	ft.isCritical = isCritical;
	floatingTexts_.push_back(ft);
}

void GamePlayScene::UpdateStressTestMode()
{
	if (!object3dCom || !camera_) return;

	if (isStressTestActive_ && stressTestObstacles_.empty())
	{
		const int kStressTestRows = 10;
		const int kStressTestCols = 10;
		const float kStressSpacing = 3.5f;

		for (int r = 0; r < kStressTestRows; ++r)
		{
			for (int c = 0; c < kStressTestCols; ++c)
			{
				Vector3 pos = {
					(c - kStressTestCols * 0.5f) * kStressSpacing,
					0.0f,
					(r - kStressTestRows * 0.5f) * kStressSpacing + 15.0f
				};
				auto obs = std::make_unique<Obstacle>();
				obs->Initialize(object3dCom, camera_, pos, 1.0f, "container.obj", { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f });
				stressTestObstacles_.push_back(std::move(obs));
			}
		}
	}
	else if (!isStressTestActive_ && !stressTestObstacles_.empty())
	{
		stressTestObstacles_.clear();
	}
}


