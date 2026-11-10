#include "CombatSystem.h"
#include "CombatTuning.h"
#include "GamePlayScene.h"
#include "../../Player/Player.h"
#include "../../Enemy/Enemy.h"
#include "../../Enemy/MovingEnemy.h"
#include "Application/Audio/TacticalAudio.h"
#include "Application/Particle/AppParticleManager.h"
#include "ParticleManager.h"
#include "TextureManager.h"
#include "DirectXCom.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

namespace
{
	using namespace CombatTuning;
	constexpr float   kTwoPi    = GameConfig::Math::kTwoPi;
	constexpr Vector3 kWorldUp  = { 0.0f, 1.0f, 0.0f };
	constexpr Vector3 kUnitVec3 = { 1.0f, 1.0f, 1.0f };
	constexpr Vector3 kZeroVec3 = { 0.0f, 0.0f, 0.0f };

	float Rand01() { return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX); }
	float RandRange(float a, float b) { return a + (b - a) * Rand01(); }
	float RandSym(float halfRange) { return RandRange(-halfRange, halfRange); }

	Vector3 RandomJitterVec(float horizRange, float upMin, float upMax)
	{
		return { RandSym(horizRange), RandRange(upMin, upMax), RandSym(horizRange) };
	}

	// 水平右方向ベクトルを安全に正規化して算出
	Vector3 ComputeHorizontalRight(const Vector3& forward)
	{
		Vector3 right = { forward.z, 0.0f, -forward.x };
		const float lenSq = right.x * right.x + right.z * right.z;
		if (lenSq > Spark::kDirNormalizeEps * Spark::kDirNormalizeEps)
		{
			const float invLen = 1.0f / std::sqrt(lenSq);
			right.x *= invLen;
			right.z *= invLen;
		}
		return right;
	}

	// 東西2つの木造トラス橋デッキ上かどうかを判定
	bool IsOnWoodenBridgeDeck(const Vector3& pos)
	{
		return pos.z >= BridgeDeck::kMinZ && pos.z <= BridgeDeck::kMaxZ &&
			(std::abs(pos.x + BridgeDeck::kBridgeCenterX) <= BridgeDeck::kDeckHalfWidth ||
			 std::abs(pos.x - BridgeDeck::kBridgeCenterX) <= BridgeDeck::kDeckHalfWidth);
	}

	bool HasBallisticResource(const char* filename)
	{
		return std::filesystem::exists(std::string(Assets::kResourceDir) + "/" + filename) ||
		       std::filesystem::exists(std::string(Assets::kProjectResourceDir) + filename);
	}

	uint32_t LoadTextureOrFallback(const char* path, uint32_t fallbackIdx)
	{
		const uint32_t idx = TextureManager::GetInstance()->Load(path);
		return (idx != TextureManager::kInvalidTextureIndex) ? idx : fallbackIdx;
	}

	uint32_t ResolveTexture3(uint32_t primary, uint32_t secondary, uint32_t fallback)
	{
		if (primary != UINT32_MAX && primary != TextureManager::kInvalidTextureIndex) return primary;
		if (secondary != UINT32_MAX && secondary != TextureManager::kInvalidTextureIndex) return secondary;
		return fallback;
	}

	std::unique_ptr<Object3d> CreateMasterPropObject(
		Object3dCom* objCom, Camera* camera, const char* modelFile, const char* texPath,
		uint32_t texIdx, float boundingRadius, float reflectionFactor, float fresnelF0, const Vector4& color)
	{
		const char* resolvedFile = HasBallisticResource(modelFile) ? modelFile : Assets::kFallbackSphereFile;
		Object3d::ModelData model = Object3d::LoadObjFile(Assets::kResourceDir, resolvedFile);
		model.material.textureFilePath = texPath;
		model.material.textureIndex = texIdx;
		model.boundingRadius = boundingRadius;

		auto master = std::make_unique<Object3d>();
		master->Initialize(objCom, model);
		master->SetCamera(camera);
		master->SetEnableLighting(true);
		master->SetReflectionFactor(reflectionFactor);
		master->SetFresnelF0(fresnelF0);
		master->SetColor(color);
		return master;
	}

	// 加算合成マズルフラッシュ粒子（コア閃光・前方ジェット・サイドベント共通）
	void EmitAdditiveFlash(
		AppParticleManager* mgr, const Vector3& pos, const Vector3& vel,
		const Vector2& aspect, float initScale, float targetScaleRatio,
		const Vector4& startCol, const Vector4& endCol, float drag, float lifeTime, uint32_t texIdx)
	{
		if (!mgr) return;
		AppParticle p{};
		p.transform.Initialize();
		p.transform.SetTranslate(pos);
		p.transform.SetRotate({ 0.0f, 0.0f, RandRange(0.0f, kTwoPi) });
		p.scaleAspect = aspect;
		p.transform.SetScale({ initScale * aspect.x, initScale * aspect.y, 1.0f });
		p.velocity = vel;
		p.color = startCol;
		p.endColor = endCol;
		p.hasColorShift = true;
		p.drag = drag;
		p.lifeTime = lifeTime;
		p.currentTime = 0.0f;
		p.textureIndex = texIdx;
		p.initialScale = initScale;
		p.targetScale = initScale * targetScaleRatio;
		mgr->AddParticle(p);
	}

	// GPUEmitterの白飛び煙を誘発せず、細長い実弾スパーク粒子のみを高精度に投入するヘルパー
	void EmitCleanBallisticSpark(
		AppParticleManager* mgr, const Vector3& pos, const Vector3& vel,
		const Vector4& startCol, const Vector4& endCol,
		float width, float length, float lifeTime, float gravity, float drag, uint32_t texIdx)
	{
		if (!mgr) return;
		AppParticle p{};
		p.transform.Initialize();
		p.transform.SetTranslate(pos);
		p.transform.SetRotate({ 0.0f, 0.0f, RandRange(0.0f, kTwoPi) });
		p.scaleAspect = { width, length };
		p.initialScale = 1.0f;
		p.targetScale = Spark::kTargetScaleRatio;
		p.transform.SetScale({ width, length, 1.0f });
		p.velocity = vel;
		p.color = startCol;
		p.endColor = endCol;
		p.hasColorShift = true;
		p.lifeTime = lifeTime;
		p.currentTime = 0.0f;
		p.textureIndex = texIdx;
		p.gravity = gravity;
		p.drag = drag;
		p.bounceElasticity = Spark::kBounceElasticity;
		p.angularVelocity = RandSym(Spark::kAngularVelocityRange);
		mgr->AddParticle(p);
	}

	struct SoftSmokeSpec
	{
		Vector3  pos{};
		Vector3  velocity{};
		Vector2  aspect{ 1.0f, 1.0f };
		float    initScale = 0.0f;
		float    targetScale = 0.0f;
		Vector4  startColor{};
		Vector4  endColor{};
		float    drag = 1.0f;
		float    lifeTime = 0.0f;
		float    fadeInRatio = 0.0f;
		float    curlFreq = 0.0f;
		float    curlAmp = 0.0f;
		float    angularVelRange = 0.0f;
		uint32_t textureIndex = UINT32_MAX;
		uint16_t slotLayer = 1;
	};

	// 砂塵パフ・コルダイト硝煙・衝撃波ベイパー共通のストレートアルファ粒子生成ヘルパー
	void EmitSoftSmokePuff(AppParticleManager* mgr, const SoftSmokeSpec& s)
	{
		if (!mgr || s.textureIndex == TextureManager::kInvalidTextureIndex) return;
		AppParticle p{};
		p.transform.Initialize();
		p.transform.SetTranslate(s.pos);
		p.transform.SetRotate({ 0.0f, 0.0f, RandRange(0.0f, kTwoPi) });
		p.scaleAspect = s.aspect;
		p.initialScale = s.initScale;
		p.targetScale = s.targetScale;
		p.transform.SetScale({ s.initScale * s.aspect.x, s.initScale * s.aspect.y, 1.0f });
		p.velocity = s.velocity;
		p.color = s.startColor;
		p.endColor = s.endColor;
		p.hasColorShift = true;
		p.drag = s.drag;
		p.curlFreq = s.curlFreq;
		p.curlAmp = s.curlAmp;
		p.lifeTime = s.lifeTime;
		p.currentTime = 0.0f;
		p.textureIndex = s.textureIndex;
		p.angularVelocity = (s.angularVelRange > 0.0f) ? RandSym(s.angularVelRange) : 0.0f;
		p.fadeInRatio = s.fadeInRatio;
		p.slotLayer = s.slotLayer;
		mgr->AddParticle(p);
	}

	void ApplyPropEndOfLifeFade(
		Object3d* obj, float remain, float fadeDuration, float minAlpha, const Vector4& baseRgb)
	{
		if (!obj || remain >= fadeDuration) return;
		const float ratio = remain / fadeDuration;
		const float shrink = std::clamp(ratio, 0.0f, 1.0f);
		const float fadeA = std::clamp(ratio, minAlpha, 1.0f);
		obj->SetScale({ shrink, shrink, shrink });
		obj->SetColor({ baseRgb.x, baseRgb.y, baseRgb.z, fadeA });
	}

	template <typename InstanceContainer>
	void DrawSharedPropInstances(
		InstanceContainer& instances, Object3dCom* objCom, const RenderContext& ctx, uint32_t texIdx)
	{
		if (instances.empty() || !objCom) return;

		RenderContext propCtx = ctx;
		if (texIdx != UINT32_MAX && texIdx != TextureManager::kInvalidTextureIndex)
		{
			propCtx.SetTextureHandle(TextureManager::GetInstance()->GetSrvHandleGPU(texIdx));
		}

		for (auto& inst : instances)
		{
			if (!inst.obj) continue;
			if (ctx.camera)
			{
				inst.obj->SetCamera(ctx.camera);
				inst.obj->Update();
			}
			objCom->Draw(inst.obj.get(), propCtx, inst.obj->GetModelData(), true);
		}
	}
}

CombatSystem::CombatSystem(GamePlayScene* scene)
	: scene_(scene)
{
	bullets_.reserve(Pool::kInitialBulletCapacity);
	casings_.reserve(Pool::kMaxActiveCasings);
	droppedMagazines_.reserve(Pool::kMaxDroppedMagazines);
}

void CombatSystem::Finalize()
{
	droppedMagazines_.clear();
	enemyMagDroppedState_.clear();
	magFullMasterObj_.reset();
	magEmptyMasterObj_.reset();
	casings_.clear();
	casingMasterObj_.reset();
	bullets_.clear();
	additiveTracerPSO_.Reset();
}

ID3D12PipelineState* CombatSystem::GetAdditiveTracerPSO()
{
	EnsureBallisticAssetsLoaded();
	return additiveTracerPSO_.Get();
}

void CombatSystem::EnsureBallisticAssetsLoaded()
{
	if (!scene_ || !scene_->object3dCom || !scene_->camera_) return;

	if (ballisticsAtlasTexIndex_ == UINT32_MAX)
	{
		ballisticsAtlasTexIndex_ = LoadTextureOrFallback(Assets::kBallisticsAtlasPath, scene_->particleTextureB);
	}
	if (muzzleFlashTexIndex_ == UINT32_MAX)
	{
		muzzleFlashTexIndex_ = LoadTextureOrFallback(Assets::kMuzzleFlashTexPath, scene_->particleTextureB);
	}
	if (magazineTexIndex_ == UINT32_MAX)
	{
		magazineTexIndex_ = LoadTextureOrFallback(Assets::kMagazineTexPath, ballisticsAtlasTexIndex_);
	}

	if (!casingMasterObj_)
	{
		casingMasterObj_ = CreateMasterPropObject(
			scene_->object3dCom, scene_->camera_, Assets::kCasingModelFile, Assets::kBallisticsAtlasPath,
			ballisticsAtlasTexIndex_, Assets::kCasingBoundingRadius, Assets::kCasingReflection,
			Assets::kCasingFresnelF0, Assets::kCasingMasterColor);
	}
	if (!magFullMasterObj_)
	{
		magFullMasterObj_ = CreateMasterPropObject(
			scene_->object3dCom, scene_->camera_, Assets::kMagFullModelFile, Assets::kMagazineTexPath,
			magazineTexIndex_, Assets::kMagazineBoundingRadius, Assets::kMagFullReflection,
			Assets::kMagFullFresnelF0, Assets::kWhiteColor);
	}
	if (!magEmptyMasterObj_)
	{
		magEmptyMasterObj_ = CreateMasterPropObject(
			scene_->object3dCom, scene_->camera_, Assets::kMagEmptyModelFile, Assets::kMagazineTexPath,
			magazineTexIndex_, Assets::kMagazineBoundingRadius, Assets::kMagEmptyReflection,
			Assets::kMagEmptyFresnelF0, Assets::kWhiteColor);
	}

	if (!additiveTracerPSO_ && scene_->object3dCom->GetDirectXCom())
	{
		DirectXCom* dx = scene_->object3dCom->GetDirectXCom();
		auto rootSig = scene_->object3dCom->GetRootSignature();
		if (dx->GetDevice() && rootSig)
		{
			auto vsBlob = dx->CompileShader(
				Assets::kObjectVSPath, Assets::kVSProfile,
				dx->GetDxcUtils().Get(), dx->GetDxcCompiler(), dx->GetIncludeHandler(), std::cout);
			auto psBlob = dx->CompileShader(
				Assets::kObjectPSPath, Assets::kPSProfile,
				dx->GetDxcUtils().Get(), dx->GetDxcCompiler(), dx->GetIncludeHandler(), std::cout);

			if (vsBlob && psBlob)
			{
				const D3D12_INPUT_ELEMENT_DESC kInputElements[] = {
					{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
					{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
					{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				};

				D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
				desc.pRootSignature = rootSig.Get();
				desc.InputLayout = { kInputElements, _countof(kInputElements) };
				desc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
				desc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };

				auto& rtBlend = desc.BlendState.RenderTarget[0];
				rtBlend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
				rtBlend.BlendEnable = TRUE;
				rtBlend.SrcBlend = D3D12_BLEND_ONE;
				rtBlend.BlendOp = D3D12_BLEND_OP_ADD;
				rtBlend.DestBlend = D3D12_BLEND_ONE;
				rtBlend.SrcBlendAlpha = D3D12_BLEND_ONE;
				rtBlend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
				rtBlend.DestBlendAlpha = D3D12_BLEND_ONE;

				desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
				desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
				desc.NumRenderTargets = 1;
				desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
				desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				desc.SampleDesc.Count = 1;
				desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

				desc.DepthStencilState.DepthEnable = TRUE;
				desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
				desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
				desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

				dx->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&additiveTracerPSO_));
			}
		}
	}
}

void CombatSystem::Update(float deltaTime)
{
	UpdateCombat(deltaTime);
	UpdateBullets(deltaTime);
	UpdateShellCasings(deltaTime);
	UpdateDroppedMagazines(deltaTime);
	RemoveDeadBullets();
}

void CombatSystem::AddBullet(std::unique_ptr<Bullet> bullet)
{
	if (bullet)
	{
		bullets_.emplace_back(std::move(bullet));
	}
}

void CombatSystem::SpawnShellCasing(const Vector3& breechPos, const Vector3& forward, bool isEnemy)
{
	EnsureBallisticAssetsLoaded();
	if (!casingMasterObj_ || !scene_ || !scene_->object3dCom) return;

	if (casings_.size() >= Pool::kMaxActiveCasings)
	{
		casings_.erase(casings_.begin());
	}

	const Vector3 right = ComputeHorizontalRight(forward);

	ShellCasingInstance c{};
	c.obj = std::make_unique<Object3d>();
	c.obj->InitializeShared(scene_->object3dCom, casingMasterObj_.get());
	c.obj->SetCamera(scene_->camera_);
	c.obj->SetEnableLighting(true);
	c.obj->SetReflectionFactor(Assets::kCasingReflection);
	c.obj->SetFresnelF0(Assets::kCasingFresnelF0);
	c.obj->SetColor(isEnemy ? Casing::kEnemyColor : Casing::kPlayerColor);

	c.pos = breechPos + right * Casing::kSpawnRightOffset + Vector3{ 0.0f, Casing::kSpawnUpOffset, 0.0f };
	c.vel = right * RandRange(Casing::kRightVelMin, Casing::kRightVelMax)
	      + Vector3{ 0.0f, RandRange(Casing::kUpVelMin, Casing::kUpVelMax), 0.0f }
	      - forward * RandRange(Casing::kBackVelMin, Casing::kBackVelMax);
	c.rot = {
		RandRange(0.0f, kTwoPi),
		std::atan2(forward.x, forward.z) + RandSym(Casing::kYawJitter),
		RandRange(0.0f, kTwoPi)
	};
	c.angVel = {
		RandSym(Casing::kAngVelPitchRange),
		RandSym(Casing::kAngVelYawRange),
		RandRange(Casing::kAngVelRollMin, Casing::kAngVelRollMax)
	};
	c.age = 0.0f;
	c.lifeTime = isEnemy ? Casing::kEnemyLifeTime : Casing::kPlayerLifeTime;

	c.obj->SetTranslate(c.pos);
	c.obj->SetRotate(c.rot);
	c.obj->SetScale(kUnitVec3);
	c.obj->Update();
	casings_.push_back(std::move(c));
}

void CombatSystem::UpdateShellCasings(float deltaTime)
{
	if (casings_.empty()) return;

	constexpr float kExtractionPadRadSq = GameConfig::Math::Square(BridgeDeck::kExtractionHardPadRad);
	constexpr float kOriginPadRadSq     = GameConfig::Math::Square(BridgeDeck::kOriginHardPadRad);

	for (auto it = casings_.begin(); it != casings_.end(); )
	{
		it->age += deltaTime;
		if (it->age >= it->lifeTime || !it->obj)
		{
			it = casings_.erase(it);
			continue;
		}

		const bool wasSettled = it->settled;
		if (!wasSettled)
		{
			it->vel.y -= Casing::kGravity * deltaTime;
			it->pos = it->pos + it->vel * deltaTime;
			it->rot = it->rot + it->angVel * deltaTime;

			const bool onBridgeDeck = IsOnWoodenBridgeDeck(it->pos);
			const float surfaceY = onBridgeDeck ? Casing::kBridgeSurfaceY : Casing::kGroundSurfaceY;

			if (it->pos.y <= surfaceY && it->vel.y < 0.0f)
			{
				it->pos.y = surfaceY;
				it->bounceCount++;

				if (it->bounceCount == 1 && scene_)
				{
					const Vector3 listenerPos = scene_->player_ ? scene_->player_->GetPosition() : it->pos;
					TacticalAudio::Play3D(
						Casing::kBounceSePath, it->pos, listenerPos,
						Casing::kBounceSeVolume, Casing::kBounceSeMinDist, Casing::kBounceSeMaxDist,
						Casing::kBounceSeCooldownMs, "casing_bounce");

					const float dxPad = it->pos.x - GameConfig::Environment::kExtractionPadPosition.x;
					const float dzPad = it->pos.z - GameConfig::Environment::kExtractionPadPosition.z;
					const bool onHardPad = (dxPad * dxPad + dzPad * dzPad <= kExtractionPadRadSq) ||
						(it->pos.x * it->pos.x + it->pos.z * it->pos.z <= kOriginPadRadSq);

					if ((onBridgeDeck || onHardPad) && scene_->appParticleManager_)
					{
						const uint32_t spkTex = ResolveTexture3(
							scene_->sparkStreakTextureIndex_, scene_->particleTextureB, scene_->particleTextureB);
						EmitCleanBallisticSpark(
							scene_->appParticleManager_.get(),
							it->pos + Vector3{ 0.0f, Casing::kBounceFxOffsetY, 0.0f },
							RandomJitterVec(Casing::kSparkHorizVel, Casing::kSparkUpVelMin, Casing::kSparkUpVelMax),
							Casing::kSparkStartColor, Casing::kSparkEndColor,
							Casing::kSparkWidth, Casing::kSparkLength,
							Casing::kSparkLifeTime, Casing::kSparkGravity, Casing::kSparkDrag, spkTex);
					}
					else if (scene_->smokeParticleManager_)
					{
						SoftSmokeSpec puff{};
						puff.pos          = it->pos + Vector3{ 0.0f, Casing::kBounceFxOffsetY, 0.0f };
						puff.velocity     = RandomJitterVec(Casing::kDustHorizVel, Casing::kDustUpVelMin, Casing::kDustUpVelMax);
						puff.aspect       = Casing::kDustAspect;
						puff.initScale    = Casing::kDustInitScale;
						puff.targetScale  = Casing::kDustTargetScale;
						puff.startColor   = Casing::kDustStartColor;
						puff.endColor     = Casing::kDustEndColor;
						puff.drag         = Casing::kDustDrag;
						puff.lifeTime     = Casing::kDustLifeTime;
						puff.fadeInRatio  = Casing::kDustFadeInRatio;
						puff.textureIndex = ResolveTexture3(scene_->soilDustTextureIndex_, scene_->wispySmokeTextureIndex_, scene_->smokeTextureIndex_);
						EmitSoftSmokePuff(scene_->smokeParticleManager_.get(), puff);
					}
				}

				if (it->bounceCount >= Casing::kMaxBounceCount || std::abs(it->vel.y) < Casing::kSettleVelY)
				{
					it->settled = true;
					it->vel = kZeroVec3;
					it->angVel = kZeroVec3;
					it->rot.x = Casing::kSettledRotX;
					it->rot.z = 0.0f;
				}
				else
				{
					it->vel.y = -it->vel.y * Casing::kRestitutionY;
					it->vel.x = it->vel.x * Casing::kFrictionXZ + RandSym(Casing::kScatterVelXZ);
					it->vel.z = it->vel.z * Casing::kFrictionXZ + RandSym(Casing::kScatterVelXZ);
					it->angVel.x *= Casing::kAngularDampingXZ;
					it->angVel.z *= Casing::kAngularDampingXZ;
				}
			}
		}

		ApplyPropEndOfLifeFade(
			it->obj.get(), it->lifeTime - it->age, Casing::kFadeDuration, Casing::kMinFadeAlpha, Casing::kFadeTint);

		if (!wasSettled)
		{
			it->obj->SetTranslate(it->pos);
			it->obj->SetRotate(it->rot);
		}
		++it;
	}
}

void CombatSystem::DrawShellCasings(const RenderContext& ctx)
{
	if (!scene_) return;
	DrawSharedPropInstances(casings_, scene_->object3dCom, ctx, ballisticsAtlasTexIndex_);
}

void CombatSystem::SpawnDroppedMagazine(const Vector3& magwellPos, const Vector3& forward, bool hasRemainingAmmo, bool isEnemy)
{
	EnsureBallisticAssetsLoaded();
	Object3d* master = hasRemainingAmmo ? magFullMasterObj_.get() : magEmptyMasterObj_.get();
	if (!master || !scene_ || !scene_->object3dCom) return;

	if (droppedMagazines_.size() >= Pool::kMaxDroppedMagazines)
	{
		droppedMagazines_.erase(droppedMagazines_.begin());
	}

	const Vector3 right = ComputeHorizontalRight(forward);

	DroppedMagazineInstance m{};
	m.obj = std::make_unique<Object3d>();
	m.obj->InitializeShared(scene_->object3dCom, master);
	m.obj->SetCamera(scene_->camera_);
	m.obj->SetEnableLighting(true);
	m.obj->SetReflectionFactor(hasRemainingAmmo ? Magazine::kFullDropReflection : Magazine::kEmptyDropReflection);
	m.obj->SetFresnelF0(Magazine::kDropFresnelF0);
	m.baseColor = isEnemy ? Magazine::kEnemyColor : Magazine::kPlayerColor;
	m.obj->SetColor(m.baseColor);

	m.pos = magwellPos;
	m.vel = right * RandRange(Magazine::kRightVelMin, Magazine::kRightVelMax)
	      + forward * RandRange(Magazine::kFwdVelMin, Magazine::kFwdVelMax)
	      + Vector3{ 0.0f, RandRange(Magazine::kDownVelMin, Magazine::kDownVelMax), 0.0f };
	m.rot = { Magazine::kInitialPitch, std::atan2(forward.x, forward.z) + RandSym(Magazine::kYawJitter), 0.0f };
	m.angVel = {
		RandRange(Magazine::kAngVelPitchMin, Magazine::kAngVelPitchMax),
		RandSym(Magazine::kAngVelYawRange),
		0.0f
	};
	m.age = 0.0f;
	m.lifeTime = isEnemy ? Magazine::kEnemyLifeTime : Magazine::kPlayerLifeTime;

	m.obj->SetTranslate(m.pos);
	m.obj->SetRotate(m.rot);
	m.obj->SetScale(isEnemy ? Magazine::kEnemyScale : Magazine::kPlayerScale);
	m.obj->Update();
	droppedMagazines_.push_back(std::move(m));
}

void CombatSystem::UpdateDroppedMagazines(float deltaTime)
{
	if (droppedMagazines_.empty()) return;

	for (auto it = droppedMagazines_.begin(); it != droppedMagazines_.end(); )
	{
		it->age += deltaTime;
		if (it->age >= it->lifeTime || !it->obj)
		{
			it = droppedMagazines_.erase(it);
			continue;
		}

		const bool wasSettled = it->settled;
		if (!wasSettled)
		{
			it->vel.y -= Magazine::kGravity * deltaTime;
			it->pos = it->pos + it->vel * deltaTime;
			it->rot.x += it->angVel.x * deltaTime;
			it->rot.y += it->angVel.y * deltaTime;
			it->rot.z = 0.0f;

			const bool onBridgeDeck = IsOnWoodenBridgeDeck(it->pos);
			const float surfaceY = onBridgeDeck ? Magazine::kBridgeSurfaceY : Magazine::kGroundSurfaceY;

			if (it->pos.y <= surfaceY && it->vel.y < 0.0f)
			{
				it->pos.y = surfaceY;
				it->bounceCount++;

				if (it->bounceCount == 1 && scene_)
				{
					const Vector3 listenerPos = scene_->player_ ? scene_->player_->GetPosition() : it->pos;
					TacticalAudio::Play3D(
						Magazine::kDropSePath, it->pos, listenerPos,
						Magazine::kDropSeVolume, Magazine::kDropSeMinDist, Magazine::kDropSeMaxDist,
						Magazine::kDropSeCooldownMs, "mag_drop");

					if (scene_->smokeParticleManager_)
					{
						const uint32_t dustTex = ResolveTexture3(
							scene_->soilDustTextureIndex_, scene_->wispySmokeTextureIndex_, scene_->smokeTextureIndex_);
						for (int k = 0; k < Magazine::kDustPuffCount; ++k)
						{
							SoftSmokeSpec puff{};
							puff.pos          = it->pos + Vector3{ RandSym(Magazine::kDustJitterXZ), Magazine::kDustOffsetY, RandSym(Magazine::kDustJitterXZ) };
							puff.velocity     = RandomJitterVec(Magazine::kDustHorizVel, Magazine::kDustUpVelMin, Magazine::kDustUpVelMax);
							puff.aspect       = Magazine::kDustAspect;
							puff.initScale    = Magazine::kDustInitScale;
							puff.targetScale  = Magazine::kDustTargetScale;
							puff.startColor   = onBridgeDeck ? Magazine::kBridgeDustColor : Magazine::kGroundDustColor;
							puff.endColor     = Magazine::kDustEndColor;
							puff.drag         = Magazine::kDustDrag;
							puff.lifeTime     = Magazine::kDustLifeTime;
							puff.fadeInRatio  = Magazine::kDustFadeInRatio;
							puff.textureIndex = dustTex;
							EmitSoftSmokePuff(scene_->smokeParticleManager_.get(), puff);
						}
					}
				}

				if (it->bounceCount >= Magazine::kMaxBounceCount || std::abs(it->vel.y) < Magazine::kSettleVelY)
				{
					it->settled = true;
					it->vel = kZeroVec3;
					it->angVel = kZeroVec3;
					it->rot.x = Magazine::kSettledRotX;
					it->rot.z = 0.0f;
				}
				else
				{
					it->vel.y = -it->vel.y * Magazine::kRestitutionY;
					it->vel.x = it->vel.x * Magazine::kFrictionXZ + RandSym(Magazine::kScatterVelXZ);
					it->vel.z = it->vel.z * Magazine::kFrictionXZ + RandSym(Magazine::kScatterVelXZ);
					it->angVel.x *= Magazine::kAngularDampingX;
					it->angVel.y *= Magazine::kAngularDampingY;
				}
			}
		}

		ApplyPropEndOfLifeFade(
			it->obj.get(), it->lifeTime - it->age, Magazine::kFadeDuration, Magazine::kMinFadeAlpha, it->baseColor);

		if (!wasSettled)
		{
			it->obj->SetTranslate(it->pos);
			it->obj->SetRotate(it->rot);
		}
		++it;
	}
}

void CombatSystem::DrawDroppedMagazines(const RenderContext& ctx)
{
	if (!scene_) return;
	DrawSharedPropInstances(droppedMagazines_, scene_->object3dCom, ctx, magazineTexIndex_);
}

void CombatSystem::EmitRealisticMuzzleBlast(const Vector3& muzzlePos, const Vector3& dir, bool isShotgun, bool isEnemy)
{
	if (!scene_ || !scene_->particleManager || !scene_->appParticleManager_) return;
	EnsureBallisticAssetsLoaded();

	AppParticleManager* flashMgr = scene_->appParticleManager_.get();
	const Vector3 right = ComputeHorizontalRight(dir);
	const uint32_t flashTex = ResolveTexture3(muzzleFlashTexIndex_, scene_->particleTextureB, scene_->particleTextureB);

	// 1. 銃口クラウン直前の白熱マッハディスク閃光
	const float coreScale = isShotgun
		? MuzzleBlast::kCoreScaleShotgun
		: (isEnemy ? MuzzleBlast::kCoreScaleEnemy : MuzzleBlast::kCoreScalePlayer);
	EmitAdditiveFlash(
		flashMgr, muzzlePos + dir * MuzzleBlast::kCoreFwdOffset, dir * MuzzleBlast::kCoreFwdSpeed,
		{ 1.0f, 1.0f }, coreScale, MuzzleBlast::kCoreTargetScaleRatio,
		isEnemy ? MuzzleBlast::kCoreColorEnemy : MuzzleBlast::kCoreColorPlayer, MuzzleBlast::kCoreEndColor,
		1.0f, isShotgun ? MuzzleBlast::kCoreLifeShotgun : MuzzleBlast::kCoreLifeRifle, flashTex);

	// 2. 前方軸方向ジェット炎
	const int jetCount = isShotgun ? MuzzleBlast::kJetCountShotgun : MuzzleBlast::kJetCountRifle;
	const float jetBaseSc = isShotgun ? MuzzleBlast::kJetBaseScaleShotgun : MuzzleBlast::kJetBaseScaleRifle;
	for (int i = 0; i < jetCount; ++i)
	{
		const float sc = jetBaseSc * RandRange(MuzzleBlast::kJetScaleJitterMin, MuzzleBlast::kJetScaleJitterMax);
		const Vector3 jetVel = dir * RandRange(MuzzleBlast::kJetFwdSpeedMin, MuzzleBlast::kJetFwdSpeedMax)
		                     + right * RandSym(MuzzleBlast::kJetRightJitter)
		                     + kWorldUp * RandSym(MuzzleBlast::kJetUpJitter);
		EmitAdditiveFlash(
			flashMgr,
			muzzlePos + dir * (MuzzleBlast::kJetBaseFwdOffset + static_cast<float>(i) * MuzzleBlast::kJetStepFwdOffset),
			jetVel, MuzzleBlast::kJetAspect, sc, MuzzleBlast::kJetTargetScaleRatio,
			isEnemy ? MuzzleBlast::kJetColorEnemy : MuzzleBlast::kJetColorPlayer, MuzzleBlast::kJetEndColor,
			MuzzleBlast::kJetDrag, RandRange(MuzzleBlast::kJetLifeMin, MuzzleBlast::kJetLifeMax), flashTex);
	}

	// 左右マズルコンペンセイターのサイドベント噴出炎
	const float ventSc = isShotgun ? MuzzleBlast::kVentScaleShotgun : MuzzleBlast::kVentScaleRifle;
	for (int side = -1; side <= 1; side += 2)
	{
		const float fSide = static_cast<float>(side);
		const Vector3 ventVel = right * (fSide * RandRange(MuzzleBlast::kVentSideSpeedMin, MuzzleBlast::kVentSideSpeedMax))
		                      + dir * MuzzleBlast::kVentFwdSpeed;
		EmitAdditiveFlash(
			flashMgr, muzzlePos + right * (MuzzleBlast::kVentSideOffset * fSide), ventVel,
			{ 1.0f, 1.0f }, ventSc, MuzzleBlast::kVentTargetScaleRatio,
			MuzzleBlast::kVentStartColor, MuzzleBlast::kVentEndColor,
			MuzzleBlast::kVentDrag, MuzzleBlast::kVentLifeTime, flashTex);
	}

	// 3. 未燃焼装薬の高速火の粉スパーク
	const uint32_t sparkStreakTex = ResolveTexture3(scene_->sparkStreakTextureIndex_, flashTex, flashTex);
	const int sparkCount = isShotgun ? MuzzleBlast::kSparkCountShotgun : MuzzleBlast::kSparkCountRifle;
	for (int i = 0; i < sparkCount; ++i)
	{
		const Vector3 spkVel = dir * RandRange(MuzzleBlast::kSparkFwdSpeedMin, MuzzleBlast::kSparkFwdSpeedMax)
		                     + right * RandSym(MuzzleBlast::kSparkRightJitter)
		                     + kWorldUp * RandRange(MuzzleBlast::kSparkUpMin, MuzzleBlast::kSparkUpMax);
		EmitCleanBallisticSpark(
			flashMgr, muzzlePos + dir * MuzzleBlast::kSparkFwdOffset, spkVel,
			(i % 2 == 0) ? MuzzleBlast::kSparkBrightColor : MuzzleBlast::kSparkAmberColor, MuzzleBlast::kSparkEndColor,
			RandRange(MuzzleBlast::kSparkWidthMin, MuzzleBlast::kSparkWidthMax),
			RandRange(MuzzleBlast::kSparkLenMin, MuzzleBlast::kSparkLenMax),
			RandRange(MuzzleBlast::kSparkLifeMin, MuzzleBlast::kSparkLifeMax),
			MuzzleBlast::kSparkGravity, MuzzleBlast::kSparkDrag, sparkStreakTex);
	}

	// 4. ストレートアルファ硝煙（銃口コルダイト煙 ＋ 薬室エジェクションポート薄煙）
	AppParticleManager* smokeMgr = scene_->smokeParticleManager_ ? scene_->smokeParticleManager_.get() : nullptr;
	const uint32_t corditeTex = ResolveTexture3(scene_->soilDustTextureIndex_, scene_->wispySmokeTextureIndex_, scene_->smokeTextureIndex_);
	const uint32_t wispyTex   = ResolveTexture3(scene_->wispySmokeTextureIndex_, corditeTex, corditeTex);

	if (smokeMgr && corditeTex != TextureManager::kInvalidTextureIndex)
	{
		const int puffCount = isShotgun ? MuzzleBlast::kSmokePuffShotgun : MuzzleBlast::kSmokePuffRifle;
		const float smokeAlpha = isShotgun ? MuzzleBlast::kSmokeAlphaShotgun : MuzzleBlast::kSmokeAlphaRifle;
		for (int i = 0; i < puffCount; ++i)
		{
			SoftSmokeSpec sm{};
			sm.pos = muzzlePos + dir * (MuzzleBlast::kSmokeBaseFwdOffset + static_cast<float>(i) * MuzzleBlast::kSmokeStepFwdOffset);
			sm.velocity = dir * RandRange(MuzzleBlast::kSmokeFwdSpeedMin, MuzzleBlast::kSmokeFwdSpeedMax)
			            + right * RandSym(MuzzleBlast::kSmokeRightJitter)
			            + kWorldUp * RandRange(MuzzleBlast::kSmokeUpMin, MuzzleBlast::kSmokeUpMax);
			sm.aspect = MuzzleBlast::kSmokeAspect;
			sm.initScale = isShotgun
				? RandRange(MuzzleBlast::kSmokeInitMinShotgun, MuzzleBlast::kSmokeInitMaxShotgun)
				: RandRange(MuzzleBlast::kSmokeInitMinRifle, MuzzleBlast::kSmokeInitMaxRifle);
			sm.targetScale = isShotgun
				? RandRange(MuzzleBlast::kSmokeEndMinShotgun, MuzzleBlast::kSmokeEndMaxShotgun)
				: RandRange(MuzzleBlast::kSmokeEndMinRifle, MuzzleBlast::kSmokeEndMaxRifle);
			sm.startColor      = { MuzzleBlast::kSmokeStartRGB.x, MuzzleBlast::kSmokeStartRGB.y, MuzzleBlast::kSmokeStartRGB.z, smokeAlpha };
			sm.endColor        = MuzzleBlast::kSmokeEndColor;
			sm.drag            = MuzzleBlast::kSmokeDrag;
			sm.curlFreq        = MuzzleBlast::kSmokeCurlFreq;
			sm.curlAmp         = MuzzleBlast::kSmokeCurlAmp;
			sm.lifeTime        = RandRange(MuzzleBlast::kSmokeLifeMin, MuzzleBlast::kSmokeLifeMax);
			sm.angularVelRange = MuzzleBlast::kSmokeAngularVel;
			sm.fadeInRatio     = MuzzleBlast::kSmokeFadeInRatio;
			sm.textureIndex    = (i == 0) ? corditeTex : wispyTex;
			EmitSoftSmokePuff(smokeMgr, sm);
		}

		SoftSmokeSpec ch{};
		ch.pos          = muzzlePos - dir * MuzzleBlast::kChamberBackOffset + right * MuzzleBlast::kChamberRightOffset;
		ch.velocity     = right * MuzzleBlast::kChamberRightSpeed + kWorldUp * MuzzleBlast::kChamberUpSpeed;
		ch.aspect       = MuzzleBlast::kChamberAspect;
		ch.initScale    = MuzzleBlast::kChamberInitScale;
		ch.targetScale  = MuzzleBlast::kChamberTargetScale;
		ch.startColor   = MuzzleBlast::kChamberStartColor;
		ch.endColor     = MuzzleBlast::kChamberEndColor;
		ch.drag         = MuzzleBlast::kChamberDrag;
		ch.curlFreq     = MuzzleBlast::kChamberCurlFreq;
		ch.curlAmp      = MuzzleBlast::kChamberCurlAmp;
		ch.lifeTime     = MuzzleBlast::kChamberLifeTime;
		ch.fadeInRatio  = MuzzleBlast::kChamberFadeInRatio;
		ch.textureIndex = wispyTex;
		EmitSoftSmokePuff(smokeMgr, ch);
	}
}

void CombatSystem::UpdateCombat(float deltaTime)
{
	// --- プレイヤーの射撃判定 ---
	if (scene_->player_)
	{
		std::vector<std::unique_ptr<Bullet>> fired = scene_->player_->TryShoot(&scene_->mouseInput, deltaTime);
		if (!fired.empty())
		{
			const bool isShotgun = (fired.size() > 1);
			const float maxRad = isShotgun ? Firing::kShotgunSoundRadius : Firing::kRifleSoundRadius;
			scene_->playerSoundMaxRadius_ = maxRad;
			scene_->playerSoundRadius_ = 0.0f;
			scene_->playerSoundTimer_ = Firing::kSoundRingDuration;

			// 周囲の敵への銃声通知（二乗距離比較で平方根演算を省略）
			const Vector3 playerPos = scene_->player_->GetPosition();
			const float maxRadSq = maxRad * maxRad;
			for (auto& enemy : scene_->GetEnemies())
			{
				if (enemy && !enemy->IsDead())
				{
					const float dx = enemy->GetPosition().x - playerPos.x;
					const float dz = enemy->GetPosition().z - playerPos.z;
					if (dx * dx + dz * dz <= maxRadSq)
					{
						enemy->HearNoise(playerPos);
					}
				}
			}

			scene_->lightFlashTimer_ = isShotgun ? Firing::kShotgunLightFlashTime : Firing::kRifleLightFlashTime;
			if (isShotgun)
			{
				scene_->TriggerCameraShake(Firing::kShotgunShakeDuration, Firing::kShotgunShakeAmplitude);
				TacticalAudio::Play2D(Firing::kShotgunSePath, Firing::kShotgunSeVol, Firing::kShotgunSeCooldownMs, "player_shot");
			}
			else
			{
				scene_->TriggerCameraShake(Firing::kRifleShakeDuration, Firing::kRifleShakeAmplitude);
				TacticalAudio::PlayRandom2D(
					{ Firing::kRifleSePath1, Firing::kRifleSePath2, Firing::kRifleSePath3 },
					Firing::kRifleSeVol, Firing::kRifleSeCooldownMs, "player_shot");
			}

			const Vector3 muzzlePos = fired[0]->GetPosition();
			const Vector3 dir = fired[0]->GetDirection();
			EmitRealisticMuzzleBlast(muzzlePos, dir, isShotgun, false);
			SpawnShellCasing(muzzlePos - dir * Casing::kPlayerBreechBackOffset, dir, false);

			for (auto& b : fired)
			{
				AddBullet(std::move(b));
			}
		}
	}

	// --- 全敵キャラクターの射撃判定 ＆ リロード時の空マガジン排出 ---
	if (scene_->player_ && !scene_->player_->IsDead())
	{
		const Vector3 playerPos = scene_->player_->GetPosition();
		for (auto& enemy : scene_->GetEnemies())
		{
			if (!enemy || enemy->IsDead()) continue;

			if (enemy->IsEnemyReloading())
			{
				if (enemy->GetReloadProgress() >= Magazine::kEnemyDropReloadRatio && !enemyMagDroppedState_[enemy.get()])
				{
					const float eyaw = enemy->GetYaw();
					const float sinYaw = std::sin(eyaw);
					const float cosYaw = std::cos(eyaw);
					const Vector3 efwd   = { sinYaw, 0.0f, cosYaw };
					const Vector3 eright = { cosYaw, 0.0f, -sinYaw };
					const Vector3 magPos = enemy->GetPosition()
					                     + eright * Magazine::kEnemyMagRightOffset
					                     + efwd * Magazine::kEnemyMagFwdOffset
					                     + Vector3{ 0.0f, Magazine::kEnemyMagUpOffset, 0.0f };
					SpawnDroppedMagazine(magPos, efwd, false, true);
					TacticalAudio::Play3D(
						Magazine::kEnemyMagOutSePath, magPos, playerPos,
						Magazine::kEnemyMagOutVol, Magazine::kEnemyMagOutMinDist, Magazine::kEnemyMagOutMaxDist,
						Magazine::kEnemyMagOutCooldownMs);
					enemyMagDroppedState_[enemy.get()] = true;
				}
			}
			else
			{
				enemyMagDroppedState_[enemy.get()] = false;
			}

			if (std::unique_ptr<Bullet> bullet = enemy->TryShoot(playerPos))
			{
				scene_->TriggerCameraShake(Firing::kEnemyShakeDuration, Firing::kEnemyShakeAmplitude);
				const Vector3 muzzlePos = bullet->GetPosition();
				const Vector3 dir = bullet->GetDirection();
				TacticalAudio::PlayRandom3D(
					{ Firing::kEnemyShotSePath1, Firing::kEnemyShotSePath2 },
					muzzlePos, playerPos,
					Firing::kEnemyShotSeVol, Firing::kEnemyShotMinDist, Firing::kEnemyShotMaxDist,
					Firing::kEnemyShotCooldownMs, "enemy_shot");
				EmitRealisticMuzzleBlast(muzzlePos, dir, false, true);
				SpawnShellCasing(muzzlePos - dir * Casing::kEnemyBreechBackOffset, dir, true);
				AddBullet(std::move(bullet));
			}
		}
	}
}

void CombatSystem::UpdateBullets(float deltaTime)
{
	AppParticleManager* smokeMgr = (scene_ && scene_->smokeParticleManager_) ? scene_->smokeParticleManager_.get() : nullptr;
	const uint32_t wispyTex = scene_
		? ResolveTexture3(scene_->wispySmokeTextureIndex_, scene_->smokeTextureIndex_, TextureManager::kInvalidTextureIndex)
		: TextureManager::kInvalidTextureIndex;

	constexpr float kMinHitDist     = GamePlayScene::kPlayerHitRadius + GamePlayScene::kBulletHitRadius;
	constexpr float kNearMissDist   = kMinHitDist + GrazeAndWake::kNearMissMargin;
	constexpr float kMinHitDistSq   = kMinHitDist * kMinHitDist;
	constexpr float kNearMissDistSq = kNearMissDist * kNearMissDist;

	for (auto& bullet : bullets_)
	{
		if (!bullet || bullet->IsDead()) continue;

		bullet->Update(deltaTime);

		// 敵の弾丸とプレイヤーアヒルの「かすり判定(Graze/NearMiss)」
		if (bullet->GetOwner() == BulletOwner::Enemy && scene_->player_ && !scene_->player_->IsDead() && !bullet->IsNearMissTriggered())
		{
			const Vector3 bPos = bullet->GetPosition();
			const Vector3 pPos = scene_->player_->GetPosition();
			const float dx = bPos.x - pPos.x;
			const float dz = bPos.z - pPos.z;
			const float distSq = dx * dx + dz * dz;

			if (distSq > kMinHitDistSq && distSq <= kNearMissDistSq)
			{
				bullet->TriggerNearMiss();
				TacticalAudio::Play2D(GrazeAndWake::kWhizSePath, GrazeAndWake::kWhizSeVol, GrazeAndWake::kWhizSeCooldownMs);

				if (scene_->particleManager && scene_->appParticleManager_)
				{
					EnsureBallisticAssetsLoaded();
					const uint32_t sparkTex = ResolveTexture3(
						scene_->sparkStreakTextureIndex_, muzzleFlashTexIndex_, scene_->particleTextureB);
					const Vector3 dir = bullet->GetDirection();
					for (int i = 0; i < GrazeAndWake::kGrazeSparkCount; ++i)
					{
						const float offset = (static_cast<float>(i) - GrazeAndWake::kGrazeSparkCenterIdx) * GrazeAndWake::kGrazeSparkSpacing;
						EmitCleanBallisticSpark(
							scene_->appParticleManager_.get(), bPos + dir * offset,
							dir * GrazeAndWake::kGrazeSparkFwdSpeed + RandomJitterVec(GrazeAndWake::kGrazeSparkHorizJitter, GrazeAndWake::kGrazeSparkUpMin, GrazeAndWake::kGrazeSparkUpMax),
							GrazeAndWake::kGrazeSparkStartColor, GrazeAndWake::kGrazeSparkEndColor,
							GrazeAndWake::kGrazeSparkWidth, GrazeAndWake::kGrazeSparkLength,
							GrazeAndWake::kGrazeSparkLife, 0.0f, GrazeAndWake::kGrazeSparkDrag, sparkTex);
					}
				}

				scene_->AddFloatingText(
					pPos + Vector3{ 0.0f, GrazeAndWake::kGrazeTextOffsetY, 0.0f },
					GrazeAndWake::kGrazeTextLabel, GrazeAndWake::kGrazeTextColor, false);
			}
		}

		// 超音速弾頭の背後に残る衝撃波凝縮ベイパー＆曳光剤微細火の粉
		if (bullet->ConsumeWakePulse(deltaTime))
		{
			const Vector3 pos = bullet->GetPosition();
			const Vector3 dir = bullet->GetDirection();
			const bool isEnemy = (bullet->GetOwner() == BulletOwner::Enemy);

			SoftSmokeSpec wake{};
			wake.pos          = pos - dir * GrazeAndWake::kWakeBackOffset;
			wake.velocity     = RandomJitterVec(GrazeAndWake::kWakeHorizJitter, GrazeAndWake::kWakeUpMin, GrazeAndWake::kWakeUpMax) - dir * GrazeAndWake::kWakeBackSpeed;
			wake.aspect       = GrazeAndWake::kWakeAspect;
			wake.initScale    = GrazeAndWake::kWakeInitScale;
			wake.targetScale  = GrazeAndWake::kWakeTargetScale;
			wake.startColor   = isEnemy ? GrazeAndWake::kWakeColorEnemy : GrazeAndWake::kWakeColorPlayer;
			wake.endColor     = GrazeAndWake::kWakeEndColor;
			wake.drag         = GrazeAndWake::kWakeDrag;
			wake.lifeTime     = GrazeAndWake::kWakeLifeTime;
			wake.fadeInRatio  = GrazeAndWake::kWakeFadeInRatio;
			wake.textureIndex = wispyTex;
			wake.slotLayer    = 0;
			EmitSoftSmokePuff(smokeMgr, wake);

			if (scene_->particleManager && scene_->appParticleManager_ && (std::rand() % GrazeAndWake::kEmberChanceModulus == 0))
			{
				EnsureBallisticAssetsLoaded();
				const uint32_t sparkTex = ResolveTexture3(
					scene_->sparkStreakTextureIndex_, muzzleFlashTexIndex_, scene_->particleTextureB);
				EmitCleanBallisticSpark(
					scene_->appParticleManager_.get(), pos - dir * GrazeAndWake::kEmberBackOffset,
					RandomJitterVec(GrazeAndWake::kEmberHorizJitter, GrazeAndWake::kEmberUpMin, GrazeAndWake::kEmberUpMax),
					isEnemy ? GrazeAndWake::kEmberColorEnemy : GrazeAndWake::kEmberColorPlayer, GrazeAndWake::kEmberEndColor,
					GrazeAndWake::kEmberWidth, GrazeAndWake::kEmberLength, GrazeAndWake::kEmberLifeTime,
					GrazeAndWake::kEmberGravity, GrazeAndWake::kEmberDrag, sparkTex);
			}
		}
	}
}

void CombatSystem::RemoveDeadBullets()
{
	bullets_.erase(
		std::remove_if(bullets_.begin(), bullets_.end(), [](const std::unique_ptr<Bullet>& bullet)
		{
			return !bullet || bullet->IsDead();
		}),
		bullets_.end());
}
