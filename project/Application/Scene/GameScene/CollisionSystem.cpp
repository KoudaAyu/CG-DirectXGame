#include "CollisionSystem.h"
#include "CollisionTuning.h"
#include "GamePlayScene.h"
#include "../../Player/Player.h"
#include "../../Enemy/Enemy.h"
#include "../../Enemy/MovingEnemy.h"
#include "Obstacle.h"

#include "Application/Audio/TacticalAudio.h"
#include "Application/Particle/AppParticleManager.h"
#include "ParticleManager.h"
#include "TextureManager.h"
#include "Baziru3_Engine/Framework/Effect/HitEffect.h"
#include "Baziru3_Engine/Framework/Collision/CollisionManager.h"
#include "Baziru3_Engine/Framework/Collision/SphereCollider.h"
#include "Baziru3_Engine/Framework/Collision/BoxCollider.h"
#include "Baziru3_Engine/Framework/Collision/CapsuleCollider.h"
#include "CombatSystem.h"
#include "LootSystem.h"
#include "RaidStats.h"
#include "Matrix4x4.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>

namespace
{
	using namespace CollisionTuning;
	constexpr float kTwoPi = GameConfig::Math::kTwoPi;

	float Rand01()
	{
		return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
	}

	float RandRange(float a, float b)
	{
		return a + (b - a) * Rand01();
	}

	Vector3 NormalizeOrFallback(const Vector3& v, const Vector3& fallback)
	{
		const float lenSq = v.x * v.x + v.y * v.y + v.z * v.z;
		if (lenSq > Geometry::kVecNormalizeEps * Geometry::kVecNormalizeEps)
		{
			const float invLen = 1.0f / std::sqrt(lenSq);
			return { v.x * invLen, v.y * invLen, v.z * invLen };
		}
		return fallback;
	}

	// GPUEmitterの白飛び煙を誘発せず、材質別のリアル着弾スパーク・破片のみを投入するヘルパー
	void EmitImpactFragment(
		AppParticleManager* mgr,
		const Vector3& pos,
		const Vector3& vel,
		const Vector4& startCol,
		const Vector4& endCol,
		float width,
		float length,
		float lifeTime,
		float gravity,
		float drag,
		float bounce,
		uint32_t texIdx,
		float endScaleRatio = Fragment::kDefaultEndScaleRatio,
		uint16_t slotLayer = Fragment::kDefaultSlotLayer)
	{
		if (!mgr) return;
		AppParticle p{};
		p.transform.Initialize();
		p.transform.SetTranslate(pos);
		p.transform.SetRotate({ 0.0f, 0.0f, RandRange(0.0f, kTwoPi) });
		p.scaleAspect = { width, length };
		p.initialScale = 1.0f;
		p.targetScale = endScaleRatio;
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
		p.bounceElasticity = bounce;
		p.angularVelocity = RandRange(-Fragment::kAngularVelocityRange, Fragment::kAngularVelocityRange);
		p.slotLayer = slotLayer;
		mgr->AddParticle(p);
	}

	// OBBの単一スラブ軸に対するレイ交差更新ヘルパー
	bool IntersectRaySlab(
		float localStart,
		float localDir,
		float extent,
		const Vector3& axisWorld,
		float& ioTMin,
		float& ioTMax,
		Vector3& ioHitNormal)
	{
		if (std::abs(localDir) < Geometry::kParallelSlabEps)
		{
			return (localStart >= -extent && localStart <= extent);
		}
		const float invDir = 1.0f / localDir;
		float t1 = (-extent - localStart) * invDir;
		float t2 = (extent - localStart) * invDir;
		Vector3 n1 = { -axisWorld.x, -axisWorld.y, -axisWorld.z };
		if (t1 > t2)
		{
			std::swap(t1, t2);
			n1 = axisWorld;
		}
		if (t1 > ioTMin)
		{
			ioTMin = t1;
			ioHitNormal = n1;
		}
		ioTMax = (std::min)(ioTMax, t2);
		return ioTMin <= ioTMax;
	}
}

// 線分/レイ [rayStart, rayDir * maxDist] と OBB (中心 boxCenter, サイズ boxSize, 回転 boxRot) との精密交差判定（面法線取得対応）
static bool CheckRayBoxIntersection(
	const Vector3& rayStart,
	const Vector3& rayDir,
	float maxDist,
	const Vector3& boxCenter,
	const Vector3& boxSize,
	const Vector3& boxRot,
	float& outDist,
	Vector3& outNormal)
{
	const Vector3 extents = { boxSize.x * 0.5f, boxSize.y * 0.5f, boxSize.z * 0.5f };

	// ブロードフェーズ棄却：回転に依存しない保守的AABB境界で早期棄却
	const float boundR = extents.x + extents.y + extents.z;
	const float rayEndX = rayStart.x + rayDir.x * maxDist;
	if (boxCenter.x + boundR < (std::min)(rayStart.x, rayEndX) || boxCenter.x - boundR > (std::max)(rayStart.x, rayEndX)) return false;
	const float rayEndZ = rayStart.z + rayDir.z * maxDist;
	if (boxCenter.z + boundR < (std::min)(rayStart.z, rayEndZ) || boxCenter.z - boundR > (std::max)(rayStart.z, rayEndZ)) return false;
	const float rayEndY = rayStart.y + rayDir.y * maxDist;
	if (boxCenter.y + boundR < (std::min)(rayStart.y, rayEndY) || boxCenter.y - boundR > (std::max)(rayStart.y, rayEndY)) return false;

	const Vector3 offset = { rayStart.x - boxCenter.x, rayStart.y - boxCenter.y, rayStart.z - boxCenter.z };
	Vector3 axisX{ 1.0f, 0.0f, 0.0f };
	Vector3 axisY{ 0.0f, 1.0f, 0.0f };
	Vector3 axisZ{ 0.0f, 0.0f, 1.0f };
	Vector3 localStart = offset;
	Vector3 localDir = rayDir;

	// 無回転オブジェクトは行列生成を省略、Y軸回転のみの場合も単一回転行列で高速化
	const bool hasRotX = std::abs(boxRot.x) >= Geometry::kRotationEps;
	const bool hasRotY = std::abs(boxRot.y) >= Geometry::kRotationEps;
	const bool hasRotZ = std::abs(boxRot.z) >= Geometry::kRotationEps;
	if (hasRotX || hasRotY || hasRotZ)
	{
		const Matrix4x4 R = (!hasRotX && !hasRotZ)
			? MakeRotateYMatrix(boxRot.y)
			: Multiply(MakeRotateXMatrix(boxRot.x), Multiply(MakeRotateYMatrix(boxRot.y), MakeRotateZMatrix(boxRot.z)));

		axisX = { R.m[0][0], R.m[0][1], R.m[0][2] };
		axisY = { R.m[1][0], R.m[1][1], R.m[1][2] };
		axisZ = { R.m[2][0], R.m[2][1], R.m[2][2] };

		localStart = {
			offset.x * axisX.x + offset.y * axisX.y + offset.z * axisX.z,
			offset.x * axisY.x + offset.y * axisY.y + offset.z * axisY.z,
			offset.x * axisZ.x + offset.y * axisZ.y + offset.z * axisZ.z
		};
		localDir = {
			rayDir.x * axisX.x + rayDir.y * axisX.y + rayDir.z * axisX.z,
			rayDir.x * axisY.x + rayDir.y * axisY.y + rayDir.z * axisY.z,
			rayDir.x * axisZ.x + rayDir.y * axisZ.y + rayDir.z * axisZ.z
		};
	}

	float tmin = 0.0f;
	float tmax = maxDist;
	Vector3 hitNormal = { -rayDir.x, Geometry::kFallbackSlabNormalY, -rayDir.z };

	if (!IntersectRaySlab(localStart.x, localDir.x, extents.x, axisX, tmin, tmax, hitNormal)) return false;
	if (!IntersectRaySlab(localStart.y, localDir.y, extents.y, axisY, tmin, tmax, hitNormal)) return false;
	if (!IntersectRaySlab(localStart.z, localDir.z, extents.z, axisZ, tmin, tmax, hitNormal)) return false;

	outDist = tmin;
	outNormal = hitNormal;
	return true;
}

// 弾丸の移動線分 (start -> end, 半径 bulletRadius) とキャラクターの直立円柱 (center, height, radius) との精密連続衝突判定 (CCD)
static bool CheckBulletCapsuleHit(const Vector3& start, const Vector3& end, float bulletRadius, const Vector3& charPos, float charHeight, float charRadius, Vector3& outHitPoint)
{
	const float dx = end.x - start.x;
	const float dz = end.z - start.z;
	const float lenSq = dx * dx + dz * dz;

	float t = 0.0f;
	if (lenSq > Geometry::kParallelSlabEps)
	{
		const float vx = charPos.x - start.x;
		const float vz = charPos.z - start.z;
		t = std::clamp((vx * dx + vz * dz) / lenSq, 0.0f, 1.0f);
	}

	const Vector3 closestPoint = {
		start.x + dx * t,
		start.y + (end.y - start.y) * t,
		start.z + dz * t
	};

	if (closestPoint.y < charPos.y - bulletRadius || closestPoint.y > charPos.y + charHeight + bulletRadius)
	{
		return false;
	}

	const float distX = closestPoint.x - charPos.x;
	const float distZ = closestPoint.z - charPos.z;
	const float totalRadius = charRadius + bulletRadius;
	if (distX * distX + distZ * distZ <= totalRadius * totalRadius)
	{
		outHitPoint = closestPoint;
		return true;
	}
	return false;
}

// コンストラクタ
CollisionSystem::CollisionSystem(GamePlayScene* scene)
	: scene_(scene)
{
}

// 衝突判定とキャラクターの位置補正を毎フレーム実行
void CollisionSystem::Update(float deltaTime)
{
	CollisionManager::GetInstance()->Update();
	ResolveObstacleCollisions();          // ① 弾丸と障害物の衝突を最優先判定
	ResolveBulletCollisions();            // ② 生き残った弾丸とキャラクター・標的の衝突
	UpdateBarrelHazards(deltaTime);       // ③ 引火ドラム缶の火炎噴出・時限起爆・爆発後の地面炎上更新
	ResolveCharacterObstacleCollisions(); // ④ キャラクターと障害物の衝突
	ResolveContactDamage();               // ⑤ プレイヤーと敵の直接接触によるダメージ
}

// 障害物の材質（鋼鉄・コンクリート・土嚢・木材）と実入射角に応じたリアル着弾＆跳弾演出
void CollisionSystem::TriggerObstacleHitEffect(const Vector3& hitPos, const Vector3& bulletDir, const Vector3& surfaceNormal, const Obstacle* hitObstacle)
{
	if (!scene_ || !scene_->particleManager || !scene_->appParticleManager_) return;

	if (impactFlashTexIndex_ == UINT32_MAX)
	{
		impactFlashTexIndex_ = TextureManager::GetInstance()->Load(Impact::kMuzzleFlashTexPath);
		if (impactFlashTexIndex_ == TextureManager::kInvalidTextureIndex) impactFlashTexIndex_ = scene_->particleTextureB;
	}
	if (woodSplinterTexIndex_ == UINT32_MAX)
	{
		woodSplinterTexIndex_ = TextureManager::GetInstance()->Load("Resources/particle_wood_splinter.png");
		if (woodSplinterTexIndex_ == TextureManager::kInvalidTextureIndex) woodSplinterTexIndex_ = scene_->debrisChipTextureIndex_;
	}
	if (pineNeedleTexIndex_ == UINT32_MAX)
	{
		pineNeedleTexIndex_ = TextureManager::GetInstance()->Load("Resources/particle_pine_needle_cluster.png");
		if (pineNeedleTexIndex_ == TextureManager::kInvalidTextureIndex) pineNeedleTexIndex_ = scene_->leafTextureIndex_;
	}
	if (ricochetStarTexIndex_ == UINT32_MAX)
	{
		ricochetStarTexIndex_ = TextureManager::GetInstance()->Load("Resources/particle_ricochet_star.png");
		if (ricochetStarTexIndex_ == TextureManager::kInvalidTextureIndex) ricochetStarTexIndex_ = scene_->starburstTextureIndex_;
	}

	const Vector3 bDir = NormalizeOrFallback(bulletDir, { 0.0f, 0.0f, 1.0f });
	const Vector3 N    = NormalizeOrFallback(surfaceNormal, { -bDir.x, Geometry::kFallbackImpactNormalY, -bDir.z });

	// 実弾の鏡面跳弾反射ベクトル R = bDir - 2 * dot(bDir, N) * N
	const float bDotN = bDir.x * N.x + bDir.y * N.y + bDir.z * N.z;
	const Vector3 rawRefl = {
		bDir.x - 2.0f * bDotN * N.x,
		(std::max)(Geometry::kMinReflectedDirY, bDir.y - 2.0f * bDotN * N.y + Geometry::kReflectedUpBiasY),
		bDir.z - 2.0f * bDotN * N.z
	};
	const Vector3 reflDir = NormalizeOrFallback(rawRefl, rawRefl);

	// 6材質以上に細分化し、一目で何に当たったか分かるレベルで形状・色彩・物理挙動を切り替える
	enum class SurfaceMat { SteelContainer, FuelBarrel, ChainlinkWire, Concrete, Sandbag, Tree, TimberWood };
	SurfaceMat mat = SurfaceMat::SteelContainer;
	bool isBirchTree = false;

	if (hitObstacle)
	{
		const std::string& mf = hitObstacle->GetModelFilename();
		if (hitObstacle->IsTree() || mf.find("tree") != std::string::npos || mf.find("birch") != std::string::npos)
		{
			mat = SurfaceMat::Tree;
			isBirchTree = hitObstacle->IsBirchTree();
			const_cast<Obstacle*>(hitObstacle)->TriggerHitFlinch(bDir);
		}
		else if (mf.find("sandbag") != std::string::npos)
		{
			mat = SurfaceMat::Sandbag;
		}
		else if (mf.find("barrel") != std::string::npos)
		{
			mat = SurfaceMat::FuelBarrel;
			const_cast<Obstacle*>(hitObstacle)->TriggerHitFlinch(bDir);
		}
		else if (mf.find("crate") != std::string::npos || mf.find("bridge") != std::string::npos ||
		         mf.find("watchtower") != std::string::npos)
		{
			mat = SurfaceMat::TimberWood;
		}
		else if (mf.find("fence") != std::string::npos)
		{
			if (hitPos.y < Geometry::kFenceConcreteMaxY)
			{
				mat = SurfaceMat::Concrete;
			}
			else
			{
				mat = SurfaceMat::ChainlinkWire;
				const_cast<Obstacle*>(hitObstacle)->TriggerHitFlinch(bDir);
			}
		}
		else if (mf.find("rock") != std::string::npos || mf.find("stone") != std::string::npos ||
		         mf.find("concrete") != std::string::npos)
		{
			mat = SurfaceMat::Concrete;
		}
		else
		{
			mat = SurfaceMat::SteelContainer;
		}

		if (scene_->player_)
		{
			const char* impPath = Impact::kSeMetalPath;
			if (mat == SurfaceMat::Concrete)                                  impPath = Impact::kSeConcretePath;
			else if (mat == SurfaceMat::Sandbag)                              impPath = Impact::kSeDirtPath;
			else if (mat == SurfaceMat::Tree || mat == SurfaceMat::TimberWood) impPath = Impact::kSeWoodPath;

			TacticalAudio::Play3D(
				impPath, hitPos, scene_->player_->GetPosition(),
				Impact::kAudioVolume, Impact::kAudioMinDist, Impact::kAudioMaxDist,
				Impact::kAudioCooldownMs, "obs_impact");
		}
	}

	AppParticleManager* addMgr = scene_->appParticleManager_.get();
	AppParticleManager* smokeMgr = scene_->smokeParticleManager_ ? scene_->smokeParticleManager_.get() : nullptr;
	const uint32_t flashTex    = (impactFlashTexIndex_ != TextureManager::kInvalidTextureIndex) ? impactFlashTexIndex_ : scene_->particleTextureB;
	const uint32_t starTex     = (ricochetStarTexIndex_ != TextureManager::kInvalidTextureIndex) ? ricochetStarTexIndex_ : flashTex;
	const uint32_t sparkTex    = (scene_->sparkStreakTextureIndex_ != TextureManager::kInvalidTextureIndex) ? scene_->sparkStreakTextureIndex_ : flashTex;
	const uint32_t dustTex     = (scene_->soilDustTextureIndex_ != TextureManager::kInvalidTextureIndex) ? scene_->soilDustTextureIndex_ : scene_->wispySmokeTextureIndex_;
	const uint32_t chipTex     = (scene_->debrisChipTextureIndex_ != TextureManager::kInvalidTextureIndex) ? scene_->debrisChipTextureIndex_ : scene_->particleTextureB;
	const uint32_t splinterTex = (woodSplinterTexIndex_ != TextureManager::kInvalidTextureIndex) ? woodSplinterTexIndex_ : chipTex;
	const uint32_t leafTex     = (scene_->leafTextureIndex_ != TextureManager::kInvalidTextureIndex) ? scene_->leafTextureIndex_ : chipTex;
	const uint32_t pineTex     = (pineNeedleTexIndex_ != TextureManager::kInvalidTextureIndex) ? pineNeedleTexIndex_ : leafTex;

	auto emitSmokePuff = [&](const Impact::SmokePuffPreset& preset, const Vector3& posOverride)
	{
		if (!smokeMgr || dustTex == TextureManager::kInvalidTextureIndex) return;
		for (int i = 0; i < preset.count; ++i)
		{
			AppParticle sm{};
			sm.transform.Initialize();
			sm.transform.SetTranslate(posOverride + N * Impact::kSmokeNormalOffset);
			sm.transform.SetRotate({ 0.0f, 0.0f, RandRange(0.0f, kTwoPi) });
			const float initSc = RandRange(preset.minScale * Impact::kSmokeInitScaleMinRatio, preset.minScale * Impact::kSmokeInitScaleMaxRatio);
			const float endSc  = RandRange(preset.minScale, preset.maxScale);
			sm.scaleAspect = {
				RandRange(Impact::kSmokeAspectXMin, Impact::kSmokeAspectXMax),
				RandRange(Impact::kSmokeAspectYMin, Impact::kSmokeAspectYMax)
			};
			sm.transform.SetScale({ initSc * sm.scaleAspect.x, initSc * sm.scaleAspect.y, 1.0f });
			sm.velocity = N * RandRange(preset.speed * Impact::kSmokeNormalSpeedMinRatio, preset.speed)
			            + Vector3{
			            	RandRange(-Impact::kSmokeHorizJitter, Impact::kSmokeHorizJitter),
			            	RandRange(Impact::kSmokeUpMin, Impact::kSmokeUpMax),
			            	RandRange(-Impact::kSmokeHorizJitter, Impact::kSmokeHorizJitter)
			            };
			sm.color = preset.color;
			sm.endColor = {
				preset.color.x * Impact::kSmokeEndColorDim,
				preset.color.y * Impact::kSmokeEndColorDim,
				preset.color.z * Impact::kSmokeEndColorDim,
				0.0f
			};
			sm.hasColorShift = true;
			sm.drag = Impact::kSmokeDrag;
			sm.curlFreq = Impact::kSmokeCurlFreq;
			sm.curlAmp = Impact::kSmokeCurlAmp;
			sm.lifeTime = RandRange(preset.lifeTime * Impact::kSmokeLifeMinRatio, preset.lifeTime * Impact::kSmokeLifeMaxRatio);
			sm.currentTime = 0.0f;
			sm.textureIndex = dustTex;
			sm.initialScale = initSc;
			sm.targetScale = endSc;
			sm.angularVelocity = RandRange(-Impact::kSmokeAngularVel, Impact::kSmokeAngularVel);
			sm.fadeInRatio = Impact::kSmokeFadeInRatio;
			sm.slotLayer = 1;
			smokeMgr->AddParticle(sm);
		}
	};

	// 鏡面反射方向へ連なって飛び地面で跳ねる「跳弾レーザーコメット火花列」ヘルパー（鋼鉄専用の象徴的シルエット）
	auto emitRicochetComet = [&](const Vector3& rayVel, const Vector4& headCol, const Vector4& tailCol, int tailSteps)
	{
		for (int s = 0; s < tailSteps; ++s)
		{
			const float velFactor = 1.0f - static_cast<float>(s) * 0.055f;
			const float sc = (s == 0) ? 0.135f : (0.095f - static_cast<float>(s) * 0.012f);
			EmitImpactFragment(
				addMgr, hitPos + N * 0.03f, rayVel * velFactor,
				(s == 0) ? headCol : tailCol, { 1.0f, 0.25f, 0.04f, 0.0f },
				sc, sc, 0.36f + static_cast<float>(tailSteps - s) * 0.045f,
				12.2f, 0.85f, 0.72f, (s == 0) ? starTex : sparkTex, 0.18f, 2);
		}
	};

	if (mat == SurfaceMat::SteelContainer)
	{
		// 【鋼鉄コンテナ】：砂埃ゼロ！ 青白いアーク溶接スター閃光 ＋ 地面を跳ねる3条の跳弾コメット火花 ＋ オリーブ塗装片
		EmitImpactFragment(
			addMgr, hitPos + N * 0.04f, { 0.0f, 0.0f, 0.0f },
			{ 0.78f, 0.95f, 1.0f, 1.0f }, { 1.0f, 0.55f, 0.12f, 0.0f },
			0.82f, 0.82f, 0.068f, 0.0f, 0.0f, 0.0f, starTex, 0.30f, 2);

		for (int c = 0; c < 3; ++c)
		{
			const Vector3 cDir = NormalizeOrFallback({
				reflDir.x + RandRange(-0.28f, 0.28f),
				reflDir.y + RandRange(0.05f, 0.38f),
				reflDir.z + RandRange(-0.28f, 0.28f)
			}, reflDir);
			emitRicochetComet(cDir * RandRange(11.5f, 17.5f), { 0.85f, 0.97f, 1.0f, 1.0f }, { 1.0f, 0.80f, 0.32f, 0.98f }, 5);
		}

		for (int i = 0; i < 44; ++i)
		{
			const Vector3 dir = {
				reflDir.x + RandRange(-0.68f, 0.68f),
				reflDir.y + RandRange(-0.18f, 0.85f),
				reflDir.z + RandRange(-0.68f, 0.68f)
			};
			const float spd = RandRange(6.0f, 16.5f);
			const bool isBlueHot = (i % 3 == 0);
			const float sz = RandRange(0.055f, 0.115f);
			EmitImpactFragment(
				addMgr, hitPos + N * 0.03f, dir * spd,
				isBlueHot ? Vector4{ 0.82f, 0.96f, 1.0f, 1.0f } : Vector4{ 1.0f, 0.78f, 0.26f, 0.98f },
				{ 1.0f, 0.22f, 0.03f, 0.0f },
				sz * 0.55f, sz * 2.2f, RandRange(0.24f, 0.68f),
				12.4f, 1.35f, 0.66f, isBlueHot ? starTex : sparkTex, 0.20f, 2);
		}

		if (smokeMgr)
		{
			// 軍用コンテナのオリーブグリーン塗装剥がれ片
			for (int i = 0; i < 10; ++i)
			{
				const Vector3 vel = N * RandRange(2.2f, 5.4f) + Vector3{ RandRange(-1.8f, 1.8f), RandRange(1.2f, 3.4f), RandRange(-1.8f, 1.8f) };
				const bool darkSteel = (i % 3 == 0);
				EmitImpactFragment(
					smokeMgr, hitPos + N * 0.04f, vel,
					darkSteel ? Vector4{ 0.26f, 0.30f, 0.34f, 0.96f } : Vector4{ 0.18f, 0.46f, 0.27f, 0.96f },
					{ 0.14f, 0.22f, 0.16f, 0.0f },
					RandRange(0.065f, 0.125f), RandRange(0.075f, 0.145f),
					RandRange(0.38f, 0.68f), 15.5f, 1.0f, 0.50f, chipTex, 0.82f, 2);
			}
		}
		emitSmokePuff({ 2, { 0.34f, 0.38f, 0.44f, 0.20f }, 0.26f, 0.48f, 1.8f, 0.38f }, hitPos);
	}
	else if (mat == SurfaceMat::FuelBarrel)
	{
		// 【赤ドラム缶】：鮮烈なオレンジ炎閃光 ＋ 気化燃料ミスト噴出 ＋ 真紅の塗装片 ＋ 火花
		EmitImpactFragment(
			addMgr, hitPos + N * 0.04f, { 0.0f, 0.0f, 0.0f },
			{ 1.0f, 0.68f, 0.20f, 1.0f }, { 1.0f, 0.22f, 0.04f, 0.0f },
			0.76f, 0.76f, 0.072f, 0.0f, 0.0f, 0.0f, flashTex, 0.35f, 2);
		emitRicochetComet(reflDir * 13.5f, { 1.0f, 0.92f, 0.65f, 1.0f }, { 1.0f, 0.52f, 0.14f, 0.96f }, 5);

		for (int i = 0; i < 28; ++i)
		{
			const Vector3 vel = reflDir * RandRange(5.0f, 13.5f) + Vector3{ RandRange(-2.4f, 2.4f), RandRange(0.6f, 3.6f), RandRange(-2.4f, 2.4f) };
			const float sz = RandRange(0.055f, 0.110f);
			EmitImpactFragment(
				addMgr, hitPos + N * 0.03f, vel,
				{ 1.0f, 0.72f, 0.22f, 0.98f }, { 1.0f, 0.18f, 0.02f, 0.0f },
				sz * 0.55f, sz * 2.1f, RandRange(0.22f, 0.58f),
				12.0f, 1.5f, 0.62f, sparkTex, 0.22f, 2);
		}

		if (smokeMgr)
		{
			const uint32_t mistTex = (scene_->waterFoamTextureIndex_ != TextureManager::kInvalidTextureIndex) ? scene_->waterFoamTextureIndex_ : dustTex;
			for (int i = 0; i < 6; ++i)
			{
				AppParticle fm{};
				fm.transform.Initialize();
				fm.transform.SetTranslate(hitPos + N * 0.05f);
				const float sc = RandRange(0.14f, 0.24f);
				fm.scaleAspect = { 1.45f, 0.82f };
				fm.initialScale = sc;
				fm.targetScale = sc * 2.6f;
				fm.transform.SetScale({ sc * 1.45f, sc * 0.82f, 1.0f });
				fm.velocity = N * RandRange(3.8f, 7.2f) + Vector3{ RandRange(-0.9f, 0.9f), RandRange(0.3f, 1.4f), RandRange(-0.9f, 0.9f) };
				fm.color = { 0.82f, 0.90f, 0.98f, 0.42f };
				fm.endColor = { 0.60f, 0.68f, 0.76f, 0.0f };
				fm.hasColorShift = true;
				fm.lifeTime = RandRange(0.32f, 0.55f);
				fm.textureIndex = mistTex;
				fm.drag = 3.6f;
				fm.fadeInRatio = 0.08f;
				fm.slotLayer = 1;
				smokeMgr->AddParticle(fm);
			}

			for (int i = 0; i < 12; ++i)
			{
				const Vector3 vel = N * RandRange(2.4f, 5.6f) + Vector3{ RandRange(-1.9f, 1.9f), RandRange(1.3f, 3.6f), RandRange(-1.9f, 1.9f) };
				EmitImpactFragment(
					smokeMgr, hitPos + N * 0.04f, vel,
					{ 0.92f, 0.22f, 0.15f, 0.98f }, { 0.55f, 0.12f, 0.08f, 0.0f },
					RandRange(0.065f, 0.130f), RandRange(0.075f, 0.150f),
					RandRange(0.40f, 0.72f), 15.2f, 1.0f, 0.48f, chipTex, 0.84f, 2);
			}
		}
	}
	else if (mat == SurfaceMat::ChainlinkWire)
	{
		// 【金網フェンス上部】：煙ゼロ！ 鋭い銀白色ワイヤースパークのみが瞬く軽快な金属反応
		EmitImpactFragment(
			addMgr, hitPos + N * 0.02f, { 0.0f, 0.0f, 0.0f },
			{ 0.85f, 0.96f, 1.0f, 0.96f }, { 0.45f, 0.75f, 1.0f, 0.0f },
			0.54f, 0.54f, 0.048f, 0.0f, 0.0f, 0.0f, starTex, 0.25f, 2);

		for (int i = 0; i < 22; ++i)
		{
			const Vector3 vel = reflDir * RandRange(4.5f, 12.0f) + Vector3{ RandRange(-3.2f, 3.2f), RandRange(-0.5f, 3.2f), RandRange(-3.2f, 3.2f) };
			const float sz = RandRange(0.045f, 0.092f);
			EmitImpactFragment(
				addMgr, hitPos, vel,
				{ 0.88f, 0.97f, 1.0f, 1.0f }, { 0.55f, 0.80f, 1.0f, 0.0f },
				sz * 0.55f, sz * 1.9f, RandRange(0.14f, 0.34f),
				11.5f, 1.8f, 0.58f, (i % 2 == 0) ? starTex : sparkTex, 0.20f, 2);
		}
	}
	else if (mat == SurfaceMat::Concrete)
	{
		// 【コンクリート防壁・岩】：火花を極小に抑え、真っ白なセメント粉塵雲 ＋ ゴロゴロ跳ねる灰色の石塊破片！
		for (int i = 0; i < 3; ++i)
		{
			const Vector3 vel = reflDir * RandRange(3.5f, 7.2f) + Vector3{ RandRange(-1.4f, 1.4f), RandRange(0.8f, 2.2f), RandRange(-1.4f, 1.4f) };
			EmitImpactFragment(
				addMgr, hitPos + N * 0.02f, vel,
				{ 1.0f, 0.92f, 0.72f, 0.80f }, { 0.9f, 0.5f, 0.2f, 0.0f },
				0.032f, 0.095f, RandRange(0.10f, 0.18f), 13.0f, 2.2f, 0.45f, sparkTex, 0.24f, 2);
		}

		if (smokeMgr)
		{
			for (int i = 0; i < 22; ++i)
			{
				const Vector3 vel = N * RandRange(2.8f, 7.2f)
					+ Vector3{ RandRange(-2.4f, 2.4f), RandRange(1.5f, 4.6f), RandRange(-2.4f, 2.4f) };
				const float g = RandRange(0.68f, 0.90f);
				EmitImpactFragment(
					smokeMgr, hitPos + N * 0.04f, vel,
					{ g, g * 0.99f, g * 0.97f, 0.98f },
					{ g * 0.78f, g * 0.78f, g * 0.76f, 0.0f },
					RandRange(0.075f, 0.155f), RandRange(0.085f, 0.175f),
					RandRange(0.45f, 0.88f), 16.5f, 0.95f, 0.48f, chipTex, 0.85f, 2);
			}
		}
		emitSmokePuff({ 12, { 0.92f, 0.92f, 0.90f, 0.58f }, 0.52f, 1.18f, 3.6f, 0.88f }, hitPos);
	}
	else if (mat == SurfaceMat::Sandbag)
	{
		// 【土嚢】：火花ゼロ！ 弾孔から勢いよく噴き出して地面に落ちる黄金色の砂ジェット ＋ 麻袋の布切れ ＋ 砂嵐雲
		emitSmokePuff({ 12, { 0.78f, 0.66f, 0.44f, 0.58f }, 0.56f, 1.34f, 4.0f, 0.94f }, hitPos);

		if (smokeMgr)
		{
			for (int i = 0; i < 32; ++i)
			{
				const Vector3 vel = N * RandRange(2.8f, 7.4f)
					+ Vector3{ RandRange(-1.5f, 1.5f), RandRange(1.8f, 5.4f), RandRange(-1.5f, 1.5f) };
				const float tone = RandRange(0.88f, 1.15f);
				EmitImpactFragment(
					smokeMgr, hitPos + N * 0.04f, vel,
					{ 0.76f * tone, 0.64f * tone, 0.38f * tone, 0.96f },
					{ 0.58f * tone, 0.48f * tone, 0.28f * tone, 0.0f },
					RandRange(0.055f, 0.115f), RandRange(0.065f, 0.135f),
					RandRange(0.42f, 0.82f), 18.8f, 1.05f, 0.08f, chipTex, 0.75f, 2);
			}

			for (int i = 0; i < 7; ++i)
			{
				AppParticle cloth{};
				cloth.transform.Initialize();
				cloth.transform.SetTranslate(hitPos + N * 0.05f);
				const float sc = RandRange(0.11f, 0.19f);
				cloth.scaleAspect = { 0.75f, 1.35f };
				cloth.initialScale = sc;
				cloth.targetScale = sc * 0.90f;
				cloth.transform.SetScale({ sc * 0.75f, sc * 1.35f, 1.0f });
				cloth.velocity = N * RandRange(1.6f, 3.8f) + Vector3{ RandRange(-1.6f, 1.6f), RandRange(1.4f, 3.2f), RandRange(-1.6f, 1.6f) };
				cloth.color = { 0.72f, 0.63f, 0.40f, 0.95f };
				cloth.endColor = { 0.56f, 0.48f, 0.30f, 0.0f };
				cloth.hasColorShift = true;
				cloth.lifeTime = RandRange(0.65f, 1.15f);
				cloth.textureIndex = splinterTex;
				cloth.gravity = 5.2f;
				cloth.drag = 2.6f;
				cloth.wobbleFreq = 8.5f;
				cloth.wobbleAmp = 0.46f;
				cloth.wobbleAxis = { 1.0f, 0.2f, 0.7f };
				cloth.angularVelocity = RandRange(-9.0f, 9.0f);
				cloth.slotLayer = 2;
				smokeMgr->AddParticle(cloth);
			}
		}
	}
	else if (mat == SurfaceMat::Tree)
	{
		// 【樹木（松・トウヒ／白樺）】：火花ゼロ！ 樹冠全体から大量の大きな木の葉・松葉房がハラハラ降り注ぎ、樹皮と木片が飛び散る！
		if (smokeMgr)
		{
			const Vector3 treeBase = hitObstacle ? hitObstacle->GetPosition() : hitPos;

			for (int i = 0; i < 22; ++i)
			{
				const Vector3 vel = N * RandRange(2.6f, 6.6f)
					+ Vector3{ RandRange(-2.2f, 2.2f), RandRange(1.4f, 4.4f), RandRange(-2.2f, 2.2f) };
				const float tone = RandRange(0.88f, 1.12f);
				const bool isBark = (i % 2 == 0);
				Vector4 startC{};
				if (isBark)
				{
					startC = isBirchTree
						? Vector4{ 0.95f * tone, 0.93f * tone, 0.89f * tone, 0.98f }
						: Vector4{ 0.38f * tone, 0.23f * tone, 0.14f * tone, 0.98f };
				}
				else
				{
					startC = { 0.90f * tone, 0.74f * tone, 0.44f * tone, 0.98f };
				}
				EmitImpactFragment(
					smokeMgr, hitPos + N * 0.05f, vel,
					startC, { startC.x * 0.80f, startC.y * 0.80f, startC.z * 0.80f, 0.0f },
					RandRange(0.085f, 0.155f), RandRange(0.160f, 0.310f),
					RandRange(0.45f, 0.92f), 14.2f, 1.2f, 0.38f, splinterTex, 0.86f, 2);
			}

			// 頭上の樹冠外周（y=1.8m〜3.8m, 半径0.7m〜1.95m）から一斉に舞い落ちる大きな木の葉・松葉房シャワー！
			for (int i = 0; i < 26; ++i)
			{
				const float ang = static_cast<float>(i) * (kTwoPi / 26.0f) + RandRange(-0.22f, 0.22f);
				const float rad = RandRange(0.68f, 1.95f);
				const bool usePineCluster = (!isBirchTree && (i % 3 != 0));

				AppParticle lf{};
				lf.transform.Initialize();
				lf.transform.SetTranslate({
					treeBase.x + std::cos(ang) * rad,
					RandRange(1.85f, 3.75f),
					treeBase.z + std::sin(ang) * rad
				});
				const float sc = usePineCluster ? RandRange(0.20f, 0.34f) : RandRange(0.22f, 0.38f);
				lf.scaleAspect = usePineCluster ? Vector2{ 1.0f, 1.0f } : Vector2{ 0.88f, 1.22f };
				lf.initialScale = sc;
				lf.targetScale = sc * 0.92f;
				lf.transform.SetScale({ sc * lf.scaleAspect.x, sc * lf.scaleAspect.y, 1.0f });
				lf.velocity = {
					std::cos(ang) * RandRange(0.6f, 1.9f) + bDir.x * 0.55f,
					RandRange(-0.85f, 0.65f),
					std::sin(ang) * RandRange(0.6f, 1.9f) + bDir.z * 0.55f
				};

				const float gTone = RandRange(0.88f, 1.14f);
				if (isBirchTree)
				{
					lf.color = (i % 3 == 0)
						? Vector4{ 0.95f * gTone, 0.84f * gTone, 0.22f * gTone, 0.96f }
						: Vector4{ 0.48f * gTone, 0.86f * gTone, 0.28f * gTone, 0.96f };
				}
				else if (usePineCluster)
				{
					lf.color = { 0.92f * gTone, 1.02f * gTone, 0.90f * gTone, 0.96f };
				}
				else
				{
					lf.color = { 0.36f * gTone, 0.72f * gTone, 0.28f * gTone, 0.96f };
				}
				lf.endColor = { lf.color.x * 0.82f, lf.color.y * 0.82f, lf.color.z * 0.82f, 0.0f };
				lf.hasColorShift = true;
				lf.lifeTime = RandRange(1.55f, 2.65f);
				lf.textureIndex = usePineCluster ? pineTex : leafTex;
				lf.gravity = 1.45f;
				lf.drag = 2.15f;
				lf.wobbleFreq = RandRange(6.2f, 8.8f);
				lf.wobbleAmp = RandRange(0.58f, 0.88f);
				lf.wobbleAxis = { 1.0f, 0.18f, 0.72f };
				lf.angularVelocity = RandRange(-5.2f, 5.2f);
				lf.slotLayer = 2;
				smokeMgr->AddParticle(lf);
			}
		}
		emitSmokePuff({ 4, { 0.78f, 0.64f, 0.38f, 0.38f }, 0.34f, 0.68f, 2.2f, 0.58f }, hitPos);
	}
	else // SurfaceMat::TimberWood（木箱・木橋・監視塔）
	{
		// 【木材構造物】：火花ゼロ！ 大きく鋭い黄金松材スプリンター破片が四方へ弾け飛び、琥珀色のおが屑煙が舞う
		if (smokeMgr)
		{
			for (int i = 0; i < 28; ++i)
			{
				const Vector3 vel = N * RandRange(2.8f, 7.4f)
					+ Vector3{ RandRange(-2.5f, 2.5f), RandRange(1.6f, 5.0f), RandRange(-2.5f, 2.5f) };
				const float tone = RandRange(0.86f, 1.15f);
				const Vector4 startC = (i % 4 == 0)
					? Vector4{ 0.56f * tone, 0.40f * tone, 0.22f * tone, 0.98f }
					: Vector4{ 0.92f * tone, 0.72f * tone, 0.40f * tone, 0.98f };
				EmitImpactFragment(
					smokeMgr, hitPos + N * 0.04f, vel,
					startC, { startC.x * 0.78f, startC.y * 0.78f, startC.z * 0.78f, 0.0f },
					RandRange(0.090f, 0.165f), RandRange(0.175f, 0.340f),
					RandRange(0.45f, 0.92f), 14.6f, 1.2f, 0.44f, splinterTex, 0.86f, 2);
			}
		}
		emitSmokePuff({ 8, { 0.86f, 0.71f, 0.42f, 0.48f }, 0.44f, 0.92f, 2.8f, 0.72f }, hitPos);
	}
}

// シーン内の全障害物（Obstacle）とレイの精密交差判定（OBB直接法線・対象物取得対応）
bool CollisionSystem::RaycastObstacles(const Vector3& rayStart, const Vector3& rayDir, float maxDist, float& outHitDist, Vector3& outHitPoint, Vector3* outHitNormal, const Obstacle** outHitObstacle)
{
	if (!scene_) return false;

	bool hit = false;
	float closestDist = maxDist;
	Vector3 closestNormal = { -rayDir.x, Geometry::kFallbackSlabNormalY, -rayDir.z };
	const Obstacle* closestObs = nullptr;

	for (const auto& obs : scene_->GetObstacles())
	{
		if (!obs) continue;

		BoxCollider* col = obs->GetCollider();
		const auto& extraCols = obs->GetExtraColliders();
		if (!col && extraCols.empty()) continue;

		// 水面上の空間を水平飛翔する銃弾が、徒歩入水防止用の川コライダーに空中衝突しないよう除外
		if (obs->IsRiverWalkBarrier() && rayStart.y > Geometry::kRiverSkipMinStartY && rayDir.y > Geometry::kRiverSkipMinDirY)
		{
			continue;
		}

		auto testCollider = [&](const BoxCollider* box)
		{
			if (!box || !box->IsEnabled()) return;
			float dist = 0.0f;
			Vector3 norm{};
			if (CheckRayBoxIntersection(rayStart, rayDir, closestDist, box->GetWorldPosition(), box->GetSize(), box->GetWorldRotation(), dist, norm))
			{
				closestDist = dist;
				closestNormal = norm;
				closestObs = obs.get();
				hit = true;
			}
		};

		testCollider(col);
		for (const auto& extra : extraCols)
		{
			testCollider(extra.get());
		}
	}

	if (hit)
	{
		outHitDist = closestDist;
		outHitPoint = {
			rayStart.x + rayDir.x * closestDist,
			rayStart.y + rayDir.y * closestDist,
			rayStart.z + rayDir.z * closestDist
		};
		if (outHitNormal) *outHitNormal = closestNormal;
		if (outHitObstacle) *outHitObstacle = closestObs;
	}
	return hit;
}

// 弾丸とプレイヤー・敵・標的の精密衝突判定 (CCD + 直立円柱ヒットボックス)
void CollisionSystem::ResolveBulletCollisions()
{
	if (!scene_->combatSystem_) return;

	auto& bullets = scene_->combatSystem_->GetBullets();

	// 弾丸の移動軌跡上に遮蔽障害物があるか検証し、遮蔽されていれば着弾演出を発火して true を返す共通ラムダ
	auto isBlockedByObstacle = [&](const Vector3& segStart, const Vector3& hitPoint, const Vector3& bulletDir) -> bool
	{
		const Vector3 toHit = { hitPoint.x - segStart.x, hitPoint.y - segStart.y, hitPoint.z - segStart.z };
		const float hitDistSq = toHit.x * toHit.x + toHit.y * toHit.y + toHit.z * toHit.z;
		if (hitDistSq <= Geometry::kVecNormalizeEps * Geometry::kVecNormalizeEps) return false;

		const float hitDist = std::sqrt(hitDistSq);
		const Vector3 hitDir = { toHit.x / hitDist, toHit.y / hitDist, toHit.z / hitDist };
		float obsDist = hitDist;
		Vector3 obsHitPos{};
		Vector3 obsNormal{};
		const Obstacle* obsPtr = nullptr;
		if (RaycastObstacles(segStart, hitDir, hitDist, obsDist, obsHitPos, &obsNormal, &obsPtr))
		{
			TriggerObstacleHitEffect(obsHitPos, bulletDir, obsNormal, obsPtr);
			return true;
		}
		return false;
	};

	for (auto& bullet : bullets)
	{
		if (!bullet || bullet->IsDead()) continue;

		const Vector3 bPosPrev = bullet->GetPrevPosition();
		const Vector3 bPosCurrent = bullet->GetPosition();

		if (bullet->GetOwner() == BulletOwner::Player)
		{
			// --- プレイヤーの弾丸と全敵キャラクター（Enemy）の精密衝突判定 ---
			bool hitAnyEnemy = false;
			for (auto& enemy : scene_->GetEnemies())
			{
				if (!enemy || enemy->IsDead()) continue;

				const Vector3 enemyPos = enemy->GetPosition();
				Vector3 hitPoint{};
				const float roleScale = enemy->GetRoleScale();
				if (!CheckBulletCapsuleHit(
					bPosPrev, bPosCurrent, Geometry::kBulletRadius,
					enemyPos, Geometry::kDuckHeight * roleScale, Geometry::kDuckRadius * roleScale, hitPoint))
				{
					continue;
				}

				if (isBlockedByObstacle(bPosPrev, hitPoint, bullet->GetDirection()))
				{
					bullet->Finalize();
					hitAnyEnemy = true;
					break;
				}

				const int prevHp = enemy->GetHP();
				enemy->OnHit(scene_->player_ ? scene_->player_->GetPosition() : Vector3{ 0.0f, 0.0f, 0.0f });
				const int dmg = prevHp - enemy->GetHP();
				RaidStats::GetInstance().shotsHit++;

				if (dmg > 0)
				{
					const bool isCritical = (std::rand() % CombatHit::kPercentModulus < CombatHit::kCritChancePercent);
					std::string text = std::to_string(dmg);
					if (isCritical) text += "!";
					scene_->AddFloatingText(
						hitPoint + Vector3{ 0.0f, CombatHit::kDamagePopupOffsetY, 0.0f },
						text,
						isCritical ? CombatHit::kCritPopupColor : CombatHit::kNormalPopupColor,
						isCritical);

					if (scene_->particleManager && scene_->appParticleManager_)
					{
						const Vector3 bulletDir = NormalizeOrFallback(bPosCurrent - bPosPrev, bullet->GetDirection());
						scene_->appParticleManager_->EmitViolentBloodSpray(
							scene_->particleManager->GetRandomEngine(),
							hitPoint,
							bulletDir,
							isCritical,
							scene_->bloodTextureIndex_,
							scene_->smokeTextureIndex_);

						// 防弾プレート着弾スパーク＆舞い散るアヒルの羽毛（ストレートアルファで白飛びゼロ）
						const uint32_t spkTex = (scene_->sparkStreakTextureIndex_ != TextureManager::kInvalidTextureIndex)
							? scene_->sparkStreakTextureIndex_ : scene_->particleTextureB;
						const int armorSparks = isCritical ? 12 : 6;
						for (int s = 0; s < armorSparks; ++s)
						{
							const Vector3 sVel = {
								-bulletDir.x * RandRange(2.2f, 5.8f) + RandRange(-2.6f, 2.6f),
								RandRange(1.0f, 3.6f),
								-bulletDir.z * RandRange(2.2f, 5.8f) + RandRange(-2.6f, 2.6f)
							};
							EmitImpactFragment(
								scene_->appParticleManager_.get(), hitPoint, sVel,
								isCritical ? CombatHit::kTargetRedPaintCol : CombatHit::kVestSparkStartCol,
								CombatHit::kVestSparkEndCol,
								0.020f, 0.13f, RandRange(0.14f, 0.30f),
								11.8f, 1.9f, 0.48f, spkTex, 0.25f, 2);
						}
						if (scene_->smokeParticleManager_ && scene_->featherTextureIndex_ != TextureManager::kInvalidTextureIndex)
						{
							const int hitFeathers = isCritical ? 7 : 4;
							for (int f = 0; f < hitFeathers; ++f)
							{
								AppParticle fp{};
								fp.transform.Initialize();
								fp.transform.SetTranslate(hitPoint + Vector3{ RandRange(-0.14f, 0.14f), RandRange(-0.08f, 0.18f), RandRange(-0.14f, 0.14f) });
								fp.transform.SetRotate({ 0.0f, 0.0f, RandRange(0.0f, kTwoPi) });
								const float sc = RandRange(0.085f, 0.145f);
								fp.scaleAspect = CombatHit::kFeatherAspect;
								fp.initialScale = sc;
								fp.targetScale = sc * 0.90f;
								fp.transform.SetScale({ sc * CombatHit::kFeatherAspect.x, sc * CombatHit::kFeatherAspect.y, 1.0f });
								fp.velocity = bulletDir * RandRange(0.8f, 2.4f) + Vector3{ RandRange(-2.0f, 2.0f), RandRange(1.3f, 3.2f), RandRange(-2.0f, 2.0f) };
								fp.color = (f % 3 == 0) ? CombatHit::kFeatherWhiteCol : CombatHit::kFeatherYellowCol;
								fp.endColor = { fp.color.x, fp.color.y, fp.color.z, 0.0f };
								fp.hasColorShift = true;
								fp.lifeTime = RandRange(1.05f, 1.75f);
								fp.textureIndex = scene_->featherTextureIndex_;
								fp.gravity = CombatHit::kFeatherGravity;
								fp.drag = CombatHit::kFeatherDrag;
								fp.wobbleFreq = CombatHit::kFeatherWobbleFreq;
								fp.wobbleAmp = CombatHit::kFeatherWobbleAmp;
								fp.wobbleAxis = CombatHit::kFeatherWobbleAxis;
								fp.angularVelocity = RandRange(-5.0f, 5.0f);
								fp.slotLayer = 2;
								scene_->smokeParticleManager_->AddParticle(fp);
							}
						}
					}

					if (enemy->IsDead())
					{
						TacticalAudio::Play2D(CombatHit::kSeKillConfirm, CombatHit::kKillConfirmVol, CombatHit::kKillConfirmCooldownMs);
						if (scene_->player_)
						{
							TacticalAudio::Play3D(
								CombatHit::kSeDuckQuack, enemyPos, scene_->player_->GetPosition(),
								CombatHit::kEnemyQuackVol, CombatHit::kEnemyQuackMinDist, CombatHit::kEnemyQuackMaxDist,
								CombatHit::kEnemyQuackCooldownMs, "enemy_quack");
						}
						RaidStats::GetInstance().enemiesKilled++;
						const bool isWarden = (enemy->GetTacticalRole() == Enemy::TacticalRole::Warden);
						scene_->TriggerHitStop(isWarden ? CombatHit::kWardenKillHitStop : CombatHit::kEnemyKillHitStop);
						scene_->AddFloatingText(
							enemyPos + Vector3{ 0.0f, CombatHit::kKillBannerOffsetY * roleScale, 0.0f },
							isWarden ? "WARDEN ELIMINATED" : "HOSTILE DOWN",
							isWarden ? CombatHit::kWardenBannerColor : CombatHit::kHostileBannerColor,
							true);

						if (scene_->lootSystem_)
						{
							const char* corpseTag = "SCAV GUARD";
							if (isWarden) corpseTag = "THE WARDEN";
							else if (enemy->GetTacticalRole() == Enemy::TacticalRole::Rookie)    corpseTag = "SCAV ROOKIE";
							else if (enemy->GetTacticalRole() == Enemy::TacticalRole::Overwatch) corpseTag = "MARKSMAN";
							scene_->lootSystem_->SpawnCorpseLoot(enemyPos, corpseTag);
						}
					}
					else if (isCritical)
					{
						TacticalAudio::Play2D(CombatHit::kSeHitCrit, CombatHit::kCritHitVol, CombatHit::kHitSoundCooldownMs);
						scene_->TriggerHitStop(CombatHit::kCritHitStop);
					}
					else
					{
						TacticalAudio::Play2D(CombatHit::kSeHitEnemy, CombatHit::kNormalHitVol, CombatHit::kHitSoundCooldownMs);
					}
				}

				if (enemy->IsDead())
				{
					scene_->TriggerCameraShake(CombatHit::kEnemyDeathShakeDur, CombatHit::kEnemyDeathShakeAmp);
					if (scene_->particleManager && scene_->appParticleManager_)
					{
						const Vector3 hitDir = NormalizeOrFallback(bPosCurrent - bPosPrev, bullet->GetDirection());
						scene_->appParticleManager_->EmitEnemyDestroyGPUBurst(
							scene_->particleManager->GetRandomEngine(),
							enemyPos,
							hitDir,
							scene_->particleTextureB,
							scene_->starburstTextureIndex_,
							scene_->smokeTextureIndex_);

						// 撃破クライマックス：ストレートアルファでふわりと宙に舞い落ちる大量の羽毛＆地表衝撃波リング
						if (scene_->smokeParticleManager_)
						{
							AppParticleManager* smMgr = scene_->smokeParticleManager_.get();
							if (scene_->featherTextureIndex_ != TextureManager::kInvalidTextureIndex)
							{
								for (int f = 0; f < 22; ++f)
								{
									const float a = RandRange(0.0f, kTwoPi);
									const float spd = RandRange(1.4f, 4.6f);
									AppParticle fp{};
									fp.transform.Initialize();
									fp.transform.SetTranslate(enemyPos + Vector3{ RandRange(-0.22f, 0.22f), RandRange(0.35f, 1.05f), RandRange(-0.22f, 0.22f) });
									fp.transform.SetRotate({ 0.0f, 0.0f, RandRange(0.0f, kTwoPi) });
									const float sc = RandRange(0.095f, 0.175f);
									fp.scaleAspect = CombatHit::kFeatherAspect;
									fp.initialScale = sc;
									fp.targetScale = sc * 0.88f;
									fp.transform.SetScale({ sc * CombatHit::kFeatherAspect.x, sc * CombatHit::kFeatherAspect.y, 1.0f });
									fp.velocity = { std::cos(a) * spd + hitDir.x * 1.4f, RandRange(1.8f, 4.8f), std::sin(a) * spd + hitDir.z * 1.4f };
									fp.color = (f % 3 == 0) ? CombatHit::kFeatherWhiteCol : CombatHit::kFeatherYellowCol;
									fp.endColor = { fp.color.x, fp.color.y, fp.color.z, 0.0f };
									fp.hasColorShift = true;
									fp.lifeTime = RandRange(1.35f, 2.35f);
									fp.textureIndex = scene_->featherTextureIndex_;
									fp.gravity = 1.55f;
									fp.drag = 2.4f;
									fp.wobbleFreq = 8.2f;
									fp.wobbleAmp = 0.58f;
									fp.wobbleAxis = CombatHit::kFeatherWobbleAxis;
									fp.angularVelocity = RandRange(-5.8f, 5.8f);
									fp.slotLayer = 2;
									smMgr->AddParticle(fp);
								}
							}

							const uint32_t dustTex = (scene_->soilDustTextureIndex_ != TextureManager::kInvalidTextureIndex)
								? scene_->soilDustTextureIndex_ : scene_->wispySmokeTextureIndex_;
							if (dustTex != TextureManager::kInvalidTextureIndex)
							{
								for (int d = 0; d < 12; ++d)
								{
									const float a = static_cast<float>(d) * (kTwoPi / 12.0f) + RandRange(-0.12f, 0.12f);
									const float spd = RandRange(2.0f, 4.2f);
									const float initSc = RandRange(0.22f, 0.36f);
									AppParticle dp{};
									dp.transform.Initialize();
									dp.transform.SetTranslate(enemyPos + Vector3{ std::cos(a) * 0.22f, 0.14f, std::sin(a) * 0.22f });
									dp.transform.SetRotate({ 0.0f, 0.0f, RandRange(0.0f, kTwoPi) });
									dp.scaleAspect = { 1.35f, 0.85f };
									dp.initialScale = initSc;
									dp.targetScale = initSc * 2.9f;
									dp.transform.SetScale({ initSc * 1.35f, initSc * 0.85f, 1.0f });
									dp.velocity = { std::cos(a) * spd, RandRange(0.25f, 0.95f), std::sin(a) * spd };
									dp.color = { 0.70f, 0.62f, 0.50f, 0.44f };
									dp.endColor = { 0.66f, 0.60f, 0.52f, 0.0f };
									dp.hasColorShift = true;
									dp.lifeTime = RandRange(0.55f, 0.92f);
									dp.textureIndex = dustTex;
									dp.drag = 2.9f;
									dp.curlFreq = 1.6f;
									dp.curlAmp = 0.24f;
									dp.fadeInRatio = 0.09f;
									dp.slotLayer = 1;
									smMgr->AddParticle(dp);
								}
							}
						}
					}
				}

				bullet->Finalize();
				hitAnyEnemy = true;
				break;
			}
			if (hitAnyEnemy || bullet->IsDead()) continue;

			// --- プレイヤーの弾丸と射撃標的（Target）の精密衝突判定 ---
			for (auto& target : scene_->GetTargets())
			{
				if (!target || target->IsDead()) continue;

				const Vector3 targetPos = target->GetPosition();
				Vector3 hitPoint{};
				if (!CheckBulletCapsuleHit(
					bPosPrev, bPosCurrent, Geometry::kBulletRadius,
					targetPos, Geometry::kTargetHeight, target->GetRadius(), hitPoint))
				{
					continue;
				}

				if (isBlockedByObstacle(bPosPrev, hitPoint, bullet->GetDirection()))
				{
					bullet->Finalize();
					break;
				}

				const int prevHp = target->GetHP();
				target->OnHit(CombatHit::kTargetHitDamage);
				const int dmg = prevHp - target->GetHP();
				RaidStats::GetInstance().shotsHit++;

				if (dmg > 0)
				{
					const bool isCritical = (std::rand() % CombatHit::kPercentModulus < CombatHit::kCritChancePercent);
					std::string text = std::to_string(dmg);
					if (isCritical) text += "!";
					scene_->AddFloatingText(
						hitPoint + Vector3{ 0.0f, CombatHit::kDamagePopupOffsetY, 0.0f },
						text,
						isCritical ? CombatHit::kCritPopupColor : CombatHit::kNormalPopupColor,
						isCritical);

					const Vector3 bDir = bullet->GetDirection();
					TriggerObstacleHitEffect(hitPoint, bDir, { -bDir.x, Geometry::kFallbackSlabNormalY, -bDir.z }, nullptr);
					if (scene_->smokeParticleManager_)
					{
						const uint32_t chipTex = (scene_->debrisChipTextureIndex_ != TextureManager::kInvalidTextureIndex)
							? scene_->debrisChipTextureIndex_ : scene_->particleTextureB;
						for (int k = 0; k < CombatHit::kTargetPaintChipCount; ++k)
						{
							const Vector3 cVel = Vector3{ -bDir.x, CombatHit::kTargetPaintBackUpY, -bDir.z }
								* RandRange(CombatHit::kTargetPaintSpdMin, CombatHit::kTargetPaintSpdMax)
								+ Vector3{
									RandRange(-CombatHit::kTargetPaintHorizJit, CombatHit::kTargetPaintHorizJit),
									RandRange(CombatHit::kTargetPaintUpMin, CombatHit::kTargetPaintUpMax),
									RandRange(-CombatHit::kTargetPaintHorizJit, CombatHit::kTargetPaintHorizJit)
								};
							const Vector4& paintCol = (k % 2 == 0) ? CombatHit::kTargetRedPaintCol : CombatHit::kTargetWhitePaintCol;
							EmitImpactFragment(
								scene_->smokeParticleManager_.get(),
								hitPoint, cVel, paintCol,
								{ paintCol.x * CombatHit::kTargetPaintEndDim, paintCol.y * CombatHit::kTargetPaintEndDim, paintCol.z * CombatHit::kTargetPaintEndDim, 0.0f },
								RandRange(CombatHit::kTargetPaintWMin, CombatHit::kTargetPaintWMax),
								RandRange(CombatHit::kTargetPaintLMin, CombatHit::kTargetPaintLMax),
								RandRange(CombatHit::kTargetPaintLifeMin, CombatHit::kTargetPaintLifeMax),
								CombatHit::kTargetPaintGravity, CombatHit::kTargetPaintDrag, CombatHit::kTargetPaintBounce,
								chipTex, CombatHit::kTargetPaintEndScale, 2);
						}
					}
					scene_->TriggerHitStop(CombatHit::kCritHitStop);
					if (!target->IsDead())
					{
						TacticalAudio::Play2D(CombatHit::kSeTargetHit, CombatHit::kTargetHitVol, CombatHit::kTargetHitCooldownMs);
					}
				}

				if (target->IsDead())
				{
					TacticalAudio::Play2D(CombatHit::kSeTargetBreak, CombatHit::kTargetBreakVol, CombatHit::kTargetBreakCooldownMs);
					RaidStats::GetInstance().targetsDestroyed++;
					scene_->TriggerCameraShake(CombatHit::kTargetBreakShakeDur, CombatHit::kTargetBreakShakeAmp);

					const Vector3 burstCenter = targetPos + Vector3{ 0.0f, CombatHit::kTargetBurstOffsetY, 0.0f };
					if (scene_->particleManager && scene_->appParticleManager_)
					{
						const uint32_t spkTex = (scene_->sparkStreakTextureIndex_ != TextureManager::kInvalidTextureIndex)
							? scene_->sparkStreakTextureIndex_ : scene_->particleTextureB;
						const uint32_t flTex = (impactFlashTexIndex_ != UINT32_MAX && impactFlashTexIndex_ != TextureManager::kInvalidTextureIndex)
							? impactFlashTexIndex_ : scene_->particleTextureB;
						scene_->appParticleManager_->EmitTargetDestroyGPUBurst(
							scene_->particleManager->GetRandomEngine(),
							burstCenter, spkTex, flTex, scene_->soilDustTextureIndex_);
					}

					if (scene_->smokeParticleManager_)
					{
						AppParticleManager* smMgr = scene_->smokeParticleManager_.get();
						const uint32_t chipTex = (scene_->debrisChipTextureIndex_ != TextureManager::kInvalidTextureIndex)
							? scene_->debrisChipTextureIndex_ : scene_->particleTextureB;
						const uint32_t dustTex = (scene_->soilDustTextureIndex_ != TextureManager::kInvalidTextureIndex)
							? scene_->soilDustTextureIndex_ : scene_->wispySmokeTextureIndex_;

						for (int k = 0; k < CombatHit::kTargetDebrisCount; ++k)
						{
							const float a = RandRange(0.0f, kTwoPi);
							const float spd = RandRange(CombatHit::kTargetDebrisSpdMin, CombatHit::kTargetDebrisSpdMax);
							const Vector3 vel = {
								std::cos(a) * spd,
								RandRange(CombatHit::kTargetDebrisUpMin, CombatHit::kTargetDebrisUpMax),
								std::sin(a) * spd
							};
							Vector4 cStart{};
							float w = RandRange(CombatHit::kTargetPlateWMin, CombatHit::kTargetPlateWMax);
							float l = RandRange(CombatHit::kTargetPlateLMin, CombatHit::kTargetPlateLMax);
							const int kind = k % CombatHit::kTargetDebrisKindMod;
							if (kind == 0)
							{
								cStart = CombatHit::kTargetBullseyeRed;
							}
							else if (kind == 1)
							{
								cStart = CombatHit::kTargetRingWhite;
							}
							else
							{
								cStart = CombatHit::kTargetStandWood;
								w = RandRange(CombatHit::kTargetWoodWMin, CombatHit::kTargetWoodWMax);
								l = RandRange(CombatHit::kTargetWoodLMin, CombatHit::kTargetWoodLMax);
							}
							EmitImpactFragment(
								smMgr,
								burstCenter + Vector3{
									RandRange(-CombatHit::kTargetDebrisJitXZ, CombatHit::kTargetDebrisJitXZ),
									RandRange(-CombatHit::kTargetDebrisJitY, CombatHit::kTargetDebrisJitY),
									RandRange(-CombatHit::kTargetDebrisJitXZ, CombatHit::kTargetDebrisJitXZ)
								},
								vel, cStart,
								{ cStart.x * CombatHit::kTargetDebrisEndDim, cStart.y * CombatHit::kTargetDebrisEndDim, cStart.z * CombatHit::kTargetDebrisEndDim, 0.0f },
								w, l,
								RandRange(CombatHit::kTargetDebrisLifeMin, CombatHit::kTargetDebrisLifeMax),
								CombatHit::kTargetDebrisGravity, CombatHit::kTargetDebrisDrag, CombatHit::kTargetDebrisBounce,
								chipTex, CombatHit::kTargetDebrisEndScale, 2);
						}

						constexpr float kDustAngleStep = kTwoPi / static_cast<float>(CombatHit::kTargetDustCount);
						for (int k = 0; k < CombatHit::kTargetDustCount; ++k)
						{
							const float a = static_cast<float>(k) * kDustAngleStep + RandRange(-CombatHit::kTargetDustAngleJit, CombatHit::kTargetDustAngleJit);
							const float cosA = std::cos(a);
							const float sinA = std::sin(a);
							const float spd = RandRange(CombatHit::kTargetDustSpdMin, CombatHit::kTargetDustSpdMax);
							AppParticle d{};
							d.transform.Initialize();
							d.transform.SetTranslate(burstCenter + Vector3{
								cosA * CombatHit::kTargetDustRingRad,
								RandRange(-CombatHit::kTargetDustJitY, CombatHit::kTargetDustJitY),
								sinA * CombatHit::kTargetDustRingRad
							});
							d.transform.SetRotate({ 0.0f, 0.0f, RandRange(0.0f, kTwoPi) });
							const float initSc = RandRange(CombatHit::kTargetDustInitScMin, CombatHit::kTargetDustInitScMax);
							d.scaleAspect = CombatHit::kTargetDustAspect;
							d.initialScale = initSc;
							d.targetScale = initSc * CombatHit::kTargetDustExpandRatio;
							d.transform.SetScale({ initSc * CombatHit::kTargetDustAspect.x, initSc * CombatHit::kTargetDustAspect.y, 1.0f });
							d.velocity = {
								cosA * spd,
								RandRange(CombatHit::kTargetDustUpMin, CombatHit::kTargetDustUpMax),
								sinA * spd
							};
							d.color = CombatHit::kTargetDustStartCol;
							d.endColor = CombatHit::kTargetDustEndCol;
							d.hasColorShift = true;
							d.lifeTime = RandRange(CombatHit::kTargetDustLifeMin, CombatHit::kTargetDustLifeMax);
							d.currentTime = 0.0f;
							d.textureIndex = dustTex;
							d.drag = CombatHit::kTargetDustDrag;
							d.curlFreq = CombatHit::kTargetDustCurlFreq;
							d.curlAmp = CombatHit::kTargetDustCurlAmp;
							d.fadeInRatio = CombatHit::kTargetDustFadeIn;
							d.slotLayer = 1;
							smMgr->AddParticle(d);
						}
					}
					scene_->AddFloatingText(
						targetPos + Vector3{ 0.0f, CombatHit::kTargetBannerOffsetY, 0.0f },
						"TARGET DESTROYED",
						CombatHit::kTargetBannerColor,
						true);
				}

				bullet->Finalize();
				break;
			}
			if (bullet->IsDead()) continue;
		}
		else if (bullet->GetOwner() == BulletOwner::Enemy)
		{
			// --- 敵の弾丸とプレイヤーの精密衝突判定 ---
			if (!scene_->player_ || scene_->player_->IsDead()) continue;

			const Vector3 playerPos = scene_->player_->GetPosition();
			Vector3 hitPoint{};
			if (!CheckBulletCapsuleHit(
				bPosPrev, bPosCurrent, Geometry::kBulletRadius,
				playerPos, Geometry::kDuckHeight, Geometry::kDuckRadius, hitPoint))
			{
				continue;
			}

			if (isBlockedByObstacle(bPosPrev, hitPoint, bullet->GetDirection()))
			{
				bullet->Finalize();
				continue;
			}

			// 弾丸の飛来方向からキラー（射撃を行った敵）の位置を推定・特定
			const Vector3 bDir = bullet->GetDirection();
			Vector3 attackerPos = playerPos - bDir * CombatHit::kFallbackAttackerDist;
			float bestDot = CombatHit::kMinAttackerDot;
			for (const auto& e : scene_->GetEnemies())
			{
				if (!e || e->IsDead()) continue;
				const Vector3 toE = e->GetPosition() - playerPos;
				const float distSq = toE.x * toE.x + toE.z * toE.z;
				if (distSq > CombatHit::kMinEnemySearchDist * CombatHit::kMinEnemySearchDist)
				{
					const float invDist = 1.0f / std::sqrt(distSq);
					const float dot = (-bDir.x * toE.x - bDir.z * toE.z) * invDist;
					if (dot > bestDot)
					{
						bestDot = dot;
						attackerPos = e->GetPosition();
					}
				}
			}

			const float prevHp = scene_->player_->GetHP();
			scene_->player_->TakeDamage(
				CombatHit::kEnemyRifleBulletDmg,
				"HOSTILE SENTRY (AK-74M)",
				"5.45x39mm PS CARTRIDGE (CHEST PENETRATION)",
				&attackerPos);

			if (scene_->player_->GetHP() < prevHp)
			{
				TacticalAudio::Play2D(CombatHit::kSePlayerHit, CombatHit::kPlayerHitVol, CombatHit::kPlayerHitCooldownMs);
				TacticalAudio::Play2D(CombatHit::kSeDuckQuack, CombatHit::kPlayerQuackVol, CombatHit::kPlayerQuackCooldownMs);
				scene_->vignetteAlpha_ = CombatHit::kPlayerHitVignette;
				scene_->TriggerCameraShake(CombatHit::kPlayerHitShakeDur, CombatHit::kPlayerHitShakeAmp);
				scene_->TriggerHitStop(CombatHit::kPlayerHitStop);
				scene_->TriggerDamageIndicator(std::atan2(-bDir.x, -bDir.z));
				scene_->AddFloatingText(
					playerPos + Vector3{ 0.0f, CombatHit::kPlayerPopupOffsetY, 0.0f },
					"-20",
					CombatHit::kPlayerPopupColor,
					true);

				if (scene_->particleManager && scene_->appParticleManager_)
				{
					const uint32_t spkTex = (scene_->sparkStreakTextureIndex_ != TextureManager::kInvalidTextureIndex)
						? scene_->sparkStreakTextureIndex_ : scene_->particleTextureB;
					for (int i = 0; i < CombatHit::kVestSparkCount; ++i)
					{
						const Vector3 sVel = bDir * RandRange(CombatHit::kVestSparkDirMin, CombatHit::kVestSparkDirMax)
							+ Vector3{
								RandRange(-CombatHit::kVestSparkHorizJit, CombatHit::kVestSparkHorizJit),
								RandRange(CombatHit::kVestSparkUpMin, CombatHit::kVestSparkUpMax),
								RandRange(-CombatHit::kVestSparkHorizJit, CombatHit::kVestSparkHorizJit)
							};
						EmitImpactFragment(
							scene_->appParticleManager_.get(),
							hitPoint, sVel,
							CombatHit::kVestSparkStartCol, CombatHit::kVestSparkEndCol,
							CombatHit::kVestSparkWidth, CombatHit::kVestSparkLength,
							RandRange(CombatHit::kVestSparkLifeMin, CombatHit::kVestSparkLifeMax),
							CombatHit::kVestSparkGravity, CombatHit::kVestSparkDrag, CombatHit::kVestSparkBounce,
							spkTex, CombatHit::kVestSparkEndScale, 2);
					}

					AppParticleManager* fMgr = scene_->smokeParticleManager_ ? scene_->smokeParticleManager_.get() : scene_->appParticleManager_.get();
					const uint32_t fTex = (scene_->featherTextureIndex_ != TextureManager::kInvalidTextureIndex)
						? scene_->featherTextureIndex_ : scene_->particleTextureB;
					const uint32_t dTex = (scene_->soilDustTextureIndex_ != TextureManager::kInvalidTextureIndex)
						? scene_->soilDustTextureIndex_ : scene_->wispySmokeTextureIndex_;
					if (fMgr)
					{
						for (int i = 0; i < CombatHit::kFeatherCount; ++i)
						{
							AppParticle f{};
							f.transform.Initialize();
							f.transform.SetTranslate(hitPoint + Vector3{
								RandRange(-CombatHit::kFeatherJitXZ, CombatHit::kFeatherJitXZ),
								RandRange(CombatHit::kFeatherJitYMin, CombatHit::kFeatherJitYMax),
								RandRange(-CombatHit::kFeatherJitXZ, CombatHit::kFeatherJitXZ)
							});
							f.transform.SetRotate({ 0.0f, 0.0f, RandRange(0.0f, kTwoPi) });
							const float sc = RandRange(CombatHit::kFeatherScaleMin, CombatHit::kFeatherScaleMax);
							f.scaleAspect = CombatHit::kFeatherAspect;
							f.initialScale = sc;
							f.targetScale = sc * CombatHit::kFeatherEndScaleRatio;
							f.transform.SetScale({ sc * CombatHit::kFeatherAspect.x, sc * CombatHit::kFeatherAspect.y, 1.0f });
							f.velocity = bDir * RandRange(CombatHit::kFeatherFwdSpdMin, CombatHit::kFeatherFwdSpdMax)
								+ Vector3{
									RandRange(-CombatHit::kFeatherHorizJit, CombatHit::kFeatherHorizJit),
									RandRange(CombatHit::kFeatherUpMin, CombatHit::kFeatherUpMax),
									RandRange(-CombatHit::kFeatherHorizJit, CombatHit::kFeatherHorizJit)
								};
							f.color = (i % CombatHit::kFeatherWhitePeriod != 0)
								? CombatHit::kFeatherYellowCol
								: CombatHit::kFeatherWhiteCol;
							f.endColor = { f.color.x, f.color.y, f.color.z, 0.0f };
							f.hasColorShift = true;
							f.lifeTime = RandRange(CombatHit::kFeatherLifeMin, CombatHit::kFeatherLifeMax);
							f.currentTime = 0.0f;
							f.textureIndex = fTex;
							f.gravity = CombatHit::kFeatherGravity;
							f.drag = CombatHit::kFeatherDrag;
							f.wobbleFreq = CombatHit::kFeatherWobbleFreq;
							f.wobbleAmp = CombatHit::kFeatherWobbleAmp;
							f.wobbleAxis = CombatHit::kFeatherWobbleAxis;
							f.angularVelocity = RandRange(-CombatHit::kFeatherAngularVel, CombatHit::kFeatherAngularVel);
							f.slotLayer = 2;
							fMgr->AddParticle(f);
						}

						for (int i = 0; i < CombatHit::kImpactDustCount; ++i)
						{
							AppParticle d{};
							d.transform.Initialize();
							d.transform.SetTranslate(hitPoint);
							d.transform.SetRotate({ 0.0f, 0.0f, RandRange(0.0f, kTwoPi) });
							d.initialScale = CombatHit::kImpactDustInitSc;
							d.targetScale = CombatHit::kImpactDustEndSc;
							d.transform.SetScale({ CombatHit::kImpactDustInitSc, CombatHit::kImpactDustInitSc, 1.0f });
							d.velocity = bDir * RandRange(CombatHit::kImpactDustFwdSpdMin, CombatHit::kImpactDustFwdSpdMax)
								+ Vector3{
									RandRange(-CombatHit::kImpactDustHorizJit, CombatHit::kImpactDustHorizJit),
									RandRange(CombatHit::kImpactDustUpMin, CombatHit::kImpactDustUpMax),
									RandRange(-CombatHit::kImpactDustHorizJit, CombatHit::kImpactDustHorizJit)
								};
							d.color = CombatHit::kImpactDustStartCol;
							d.endColor = CombatHit::kImpactDustEndCol;
							d.hasColorShift = true;
							d.lifeTime = CombatHit::kImpactDustLife;
							d.currentTime = 0.0f;
							d.textureIndex = dTex;
							d.drag = CombatHit::kImpactDustDrag;
							d.fadeInRatio = CombatHit::kImpactDustFadeIn;
							d.slotLayer = 1;
							fMgr->AddParticle(d);
						}
					}
				}
			}

			bullet->Finalize();
		}
	}
}

// 弾丸と障害物（Obstacle）の精密衝突判定 (RaycastObstaclesによる確実な判定)
void CollisionSystem::ResolveObstacleCollisions()
{
	if (!scene_->combatSystem_) return;
	auto& bullets = scene_->combatSystem_->GetBullets();

	for (auto& bullet : bullets)
	{
		if (!bullet || bullet->IsDead()) continue;
		const Vector3 bPosPrev = bullet->GetPrevPosition();
		const Vector3 bPos = bullet->GetPosition();
		const Vector3 diff = { bPos.x - bPosPrev.x, bPos.y - bPosPrev.y, bPos.z - bPosPrev.z };
		const float lenSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
		if (lenSq < Geometry::kVecNormalizeEps * Geometry::kVecNormalizeEps) continue;

		const float len = std::sqrt(lenSq);
		const Vector3 dir = { diff.x / len, diff.y / len, diff.z / len };
		const float checkLen = len + Geometry::kBulletRadius;

		float hitDist = checkLen;
		Vector3 hitPos{};
		Vector3 hitNormal = { -dir.x, Geometry::kFallbackSlabNormalY, -dir.z };
		const Obstacle* hitObs = nullptr;
		bool hasHit = false;

		// 1. 中心線レイキャスト
		if (RaycastObstacles(bPosPrev, dir, checkLen, hitDist, hitPos, &hitNormal, &hitObs))
		{
			hasHit = true;
		}
		else
		{
			// 2. 弾丸半径を考慮した上下左右オフセットレイキャスト
			Vector3 right = { dir.z, 0.0f, -dir.x };
			const float rLenSq = right.x * right.x + right.z * right.z;
			if (rLenSq > Geometry::kVecNormalizeEps * Geometry::kVecNormalizeEps)
			{
				const float invRLen = 1.0f / std::sqrt(rLenSq);
				right.x *= invRLen;
				right.z *= invRLen;
			}

			constexpr float kRad = Geometry::kBulletRadius;
			const Vector3 kOffsets[] = {
				{  right.x * kRad, 0.0f,  right.z * kRad },
				{ -right.x * kRad, 0.0f, -right.z * kRad },
				{  0.0f,           kRad,  0.0f           },
				{  0.0f,          -kRad,  0.0f           },
			};

			for (const auto& off : kOffsets)
			{
				float subDist = checkLen;
				Vector3 subHitPos{};
				Vector3 subNormal{};
				const Obstacle* subObs = nullptr;
				const Vector3 startOff = { bPosPrev.x + off.x, bPosPrev.y + off.y, bPosPrev.z + off.z };
				if (RaycastObstacles(startOff, dir, checkLen, subDist, subHitPos, &subNormal, &subObs))
				{
					hasHit = true;
					hitDist = subDist;
					hitPos = subHitPos;
					hitNormal = subNormal;
					hitObs = subObs;
					break;
				}
			}
		}

		if (hasHit)
		{
			TriggerObstacleHitEffect(hitPos, bullet->GetDirection(), hitNormal, hitObs);

			// 赤ドラム缶への着弾時：1〜2発目で弾孔から高圧火炎ジェット噴出、3発目で大爆発！
			if (hitObs && hitObs->IsExplosiveBarrel())
			{
				if (bullet->GetOwner() == BulletOwner::Player) RaidStats::GetInstance().shotsHit++;
				Obstacle* mutableBarrel = const_cast<Obstacle*>(hitObs);
				if (mutableBarrel->DamageBarrel(hitPos, hitNormal))
				{
					DetonateFuelBarrel(mutableBarrel, bullet->GetDirection());
				}
				else
				{
					TacticalAudio::Play3D(
						"Resources/Audio/SE_Flare_Hiss_Loop.wav", hitPos,
						scene_->player_ ? scene_->player_->GetPosition() : hitPos,
						0.58f, 4.0f, 30.0f, 120, "barrel_leak_ignite");
					scene_->TriggerCameraShake(0.08f, 0.11f);
				}
			}

			// プレイヤー弾が未回収の物資木箱（最初の弾薬箱など）に当たった場合は木箱へダメージを与え、破壊で即座に物資支給
			if (bullet->GetOwner() == BulletOwner::Player && hitObs && scene_->lootSystem_ &&
			    hitObs->GetModelFilename().find("crate") != std::string::npos)
			{
				bool crateBroken = false;
				if (scene_->lootSystem_->TryDamageBreakableCrate(
					hitObs->GetPosition(),
					scene_->player_.get(),
					scene_->appParticleManager_.get(),
					scene_->floatingTexts_,
					crateBroken,
					scene_->smokeParticleManager_.get(),
					scene_->soilDustTextureIndex_,
					scene_->debrisChipTextureIndex_,
					scene_->sparkStreakTextureIndex_))
				{
					RaidStats::GetInstance().shotsHit++;
					Obstacle* mutableObs = const_cast<Obstacle*>(hitObs);
					if (crateBroken)
					{
						mutableObs->BreakObstacle(bullet->GetDirection());
						scene_->TriggerCameraShake(0.16f, 0.19f);
						scene_->TriggerHitStop(0.032f);
					}
					else
					{
						mutableObs->TriggerHitFlinch(bullet->GetDirection());
						scene_->TriggerCameraShake(0.055f, 0.065f);
					}
				}
			}

			bullet->Finalize();
		}
	}
}

// プレイヤーと敵の直接接触時の接触ダメージ判定と適用
void CollisionSystem::ResolveContactDamage()
{
	if (!scene_->player_ || scene_->player_->IsDead()) return;

	constexpr float kContactRadius = GamePlayScene::kPlayerHitRadius + GamePlayScene::kEnemyHitRadius;
	for (auto& enemy : scene_->GetEnemies())
	{
		if (!enemy || enemy->IsDead()) continue;

		if (!scene_->IsWithinRadius(scene_->player_->GetPosition(), enemy->GetPosition(), kContactRadius)) continue;

		const float prevHp = scene_->player_->GetHP();
		Vector3 enemyPos = enemy->GetPosition();
		scene_->player_->TakeDamage(GamePlayScene::kContactDamage, "HOSTILE GUARD", "CQC CLOSE COMBAT (BLUNT TRAUMA)", &enemyPos);
		if (scene_->player_->GetHP() < prevHp)
		{
			TacticalAudio::Play2D(CombatHit::kSePlayerHit, CombatHit::kContactHitVol, CombatHit::kPlayerHitCooldownMs);
			TacticalAudio::Play2D(CombatHit::kSeDuckQuack, CombatHit::kContactQuackVol, CombatHit::kPlayerQuackCooldownMs);
			scene_->vignetteAlpha_ = CombatHit::kContactVignette;
			scene_->TriggerCameraShake(CombatHit::kContactShakeDur, CombatHit::kContactShakeAmp);
			scene_->TriggerHitStop(CombatHit::kContactHitStop);

			const Vector3 playerPos = scene_->player_->GetPosition();
			const Vector3 toEnemy = enemyPos - playerPos;
			scene_->TriggerDamageIndicator(std::atan2(toEnemy.x, toEnemy.z));
			scene_->AddFloatingText(
				playerPos + Vector3{ 0.0f, CombatHit::kContactPopupOffsetY, 0.0f },
				"WARNING -20",
				CombatHit::kContactPopupColor,
				true);
		}
	}
}

// プレイヤーおよび移動敵と障害物(BoxCollider)との手動めり込み解決（押し出し補正）
void CollisionSystem::ResolveCharacterObstacleCollisions()
{
	// キャラクターと障害物の押し出し解決はエンジンの CollisionManager::Update に統合済み
}

void CollisionSystem::DetonateFuelBarrel(Obstacle* barrelObs, const Vector3& blastDir)
{
	if (!barrelObs || barrelObs->IsBroken() || !scene_) return;

	const Vector3 bPos = barrelObs->GetPosition();
	barrelObs->BreakObstacle(blastDir);

	const Vector3 listenerPos = scene_->player_ ? scene_->player_->GetPosition() : bPos;
	TacticalAudio::Play3D("Resources/Audio/SE_Barrel_Explode.wav", bPos, listenerPos, 1.0f, 10.0f, 65.0f, 40, "barrel_boom");
	scene_->TriggerCameraShake(0.48f, 0.96f);
	scene_->TriggerHitStop(0.068f);
	scene_->AddFloatingText(bPos + Vector3{ 0.0f, 1.85f, 0.0f }, "FUEL DETONATION!!", { 1.0f, 0.58f, 0.14f, 1.0f }, true);

	AppParticleManager* addMgr   = scene_->appParticleManager_.get();
	AppParticleManager* smokeMgr = scene_->smokeParticleManager_ ? scene_->smokeParticleManager_.get() : nullptr;
	const uint32_t flashTex = (impactFlashTexIndex_ != UINT32_MAX && impactFlashTexIndex_ != TextureManager::kInvalidTextureIndex)
		? impactFlashTexIndex_ : scene_->particleTextureB;
	const uint32_t starTex  = (ricochetStarTexIndex_ != UINT32_MAX && ricochetStarTexIndex_ != TextureManager::kInvalidTextureIndex)
		? ricochetStarTexIndex_ : scene_->starburstTextureIndex_;
	const uint32_t sparkTex = (scene_->sparkStreakTextureIndex_ != TextureManager::kInvalidTextureIndex) ? scene_->sparkStreakTextureIndex_ : flashTex;
	const uint32_t dustTex  = (scene_->soilDustTextureIndex_ != TextureManager::kInvalidTextureIndex) ? scene_->soilDustTextureIndex_ : scene_->wispySmokeTextureIndex_;
	const uint32_t smokeTex = (scene_->wispySmokeTextureIndex_ != TextureManager::kInvalidTextureIndex) ? scene_->wispySmokeTextureIndex_ : dustTex;

	if (addMgr)
	{
		// 1. 中心部の強烈な白熱爆轟スター閃光
		EmitImpactFragment(addMgr, bPos + Vector3{ 0.0f, 0.85f, 0.0f }, { 0.0f, 0.0f, 0.0f },
			{ 1.0f, 0.98f, 0.86f, 1.0f }, { 1.0f, 0.45f, 0.08f, 0.0f }, 3.8f, 3.8f, 0.11f, 0.0f, 0.0f, 0.0f, starTex, 0.35f, 2);
		EmitImpactFragment(addMgr, bPos + Vector3{ 0.0f, 0.75f, 0.0f }, { 0.0f, 0.0f, 0.0f },
			{ 1.0f, 0.78f, 0.32f, 1.0f }, { 1.0f, 0.18f, 0.02f, 0.0f }, 4.6f, 4.6f, 0.22f, 0.0f, 0.0f, 0.0f, flashTex, 1.45f, 2);

		// 2. 膨張しながら立ち昇る紅蓮のマッシュルームファイアボール（34発）
		for (int i = 0; i < 34; ++i)
		{
			const float a = RandRange(0.0f, kTwoPi);
			const float rSpd = RandRange(1.2f, 5.8f);
			const float initSc = RandRange(0.55f, 1.05f);
			AppParticle fb{};
			fb.transform.Initialize();
			fb.transform.SetTranslate(bPos + Vector3{ RandRange(-0.45f, 0.45f), RandRange(0.25f, 1.15f), RandRange(-0.45f, 0.45f) });
			fb.scaleAspect = { RandRange(0.9f, 1.25f), RandRange(0.9f, 1.25f) };
			fb.initialScale = initSc;
			fb.targetScale = initSc * RandRange(2.1f, 3.0f);
			fb.transform.SetScale({ initSc, initSc, 1.0f });
			fb.velocity = { std::cos(a) * rSpd, RandRange(2.2f, 6.8f), std::sin(a) * rSpd };
			fb.color = (i % 3 == 0) ? Vector4{ 1.0f, 0.94f, 0.62f, 0.96f } : Vector4{ 1.0f, 0.56f, 0.14f, 0.92f };
			fb.endColor = { 0.85f, 0.10f, 0.02f, 0.0f };
			fb.hasColorShift = true;
			fb.lifeTime = RandRange(0.38f, 0.78f);
			fb.textureIndex = flashTex;
			fb.drag = 2.4f;
			fb.curlFreq = 1.8f;
			fb.curlAmp = 0.45f;
			fb.slotLayer = 2;
			addMgr->AddParticle(fb);
		}

		// 3. 四方八方の地面で跳ね回る90発の爆裂火の粉＆破片スパークシャワー
		for (int i = 0; i < 90; ++i)
		{
			const float a = RandRange(0.0f, kTwoPi);
			const float hSpd = RandRange(4.5f, 18.5f);
			const Vector3 vel = { std::cos(a) * hSpd, RandRange(2.4f, 10.5f), std::sin(a) * hSpd };
			const float sz = RandRange(0.065f, 0.140f);
			const bool bright = (i % 4 == 0);
			EmitImpactFragment(
				addMgr, bPos + Vector3{ 0.0f, 0.55f, 0.0f }, vel,
				bright ? Vector4{ 1.0f, 0.97f, 0.82f, 1.0f } : Vector4{ 1.0f, 0.68f, 0.18f, 0.98f },
				{ 1.0f, 0.18f, 0.02f, 0.0f },
				sz * 0.6f, sz * 2.4f, RandRange(0.55f, 1.45f),
				12.2f, 1.05f, 0.66f, bright ? starTex : sparkTex, 0.22f, 2);
		}
	}

	if (smokeMgr)
	{
		// 4. 地表を高速で押し広がる衝撃波ダストリング（28発）
		for (int i = 0; i < 28; ++i)
		{
			const float a = static_cast<float>(i) * (kTwoPi / 28.0f) + RandRange(-0.08f, 0.08f);
			const float spd = RandRange(6.5f, 11.5f);
			const float initSc = RandRange(0.36f, 0.58f);
			AppParticle sw{};
			sw.transform.Initialize();
			sw.transform.SetTranslate(bPos + Vector3{ std::cos(a) * 0.45f, 0.16f, std::sin(a) * 0.45f });
			sw.scaleAspect = { 1.55f, 0.72f };
			sw.initialScale = initSc;
			sw.targetScale = initSc * 3.4f;
			sw.transform.SetScale({ initSc * 1.55f, initSc * 0.72f, 1.0f });
			sw.velocity = { std::cos(a) * spd, RandRange(0.18f, 0.75f), std::sin(a) * spd };
			sw.color = { 0.76f, 0.66f, 0.52f, 0.58f };
			sw.endColor = { 0.62f, 0.56f, 0.48f, 0.0f };
			sw.hasColorShift = true;
			sw.lifeTime = RandRange(0.65f, 1.15f);
			sw.textureIndex = dustTex;
			sw.drag = 2.8f;
			sw.fadeInRatio = 0.06f;
			sw.slotLayer = 1;
			smokeMgr->AddParticle(sw);
		}

		// 5. 上空5mまでモクモクと立ち昇る重油黒煙柱（26発）
		for (int i = 0; i < 26; ++i)
		{
			const float a = RandRange(0.0f, kTwoPi);
			const float rSpd = RandRange(0.4f, 2.2f);
			const float initSc = RandRange(0.45f, 0.78f);
			AppParticle bs{};
			bs.transform.Initialize();
			bs.transform.SetTranslate(bPos + Vector3{ RandRange(-0.55f, 0.55f), RandRange(0.55f, 1.45f), RandRange(-0.55f, 0.55f) });
			bs.scaleAspect = { RandRange(0.92f, 1.28f), RandRange(1.05f, 1.38f) };
			bs.initialScale = initSc;
			bs.targetScale = initSc * RandRange(3.0f, 4.4f);
			bs.transform.SetScale({ initSc, initSc, 1.0f });
			bs.velocity = { std::cos(a) * rSpd, RandRange(1.8f, 4.2f), std::sin(a) * rSpd };
			const float dark = RandRange(0.12f, 0.24f);
			bs.color = { dark, dark * 0.96f, dark * 0.94f, RandRange(0.62f, 0.80f) };
			bs.endColor = { 0.22f, 0.22f, 0.24f, 0.0f };
			bs.hasColorShift = true;
			bs.lifeTime = RandRange(2.2f, 3.8f);
			bs.textureIndex = smokeTex;
			bs.drag = 0.95f;
			bs.curlFreq = 1.5f;
			bs.curlAmp = 0.48f;
			bs.angularVelocity = RandRange(-1.4f, 1.4f);
			bs.fadeInRatio = 0.12f;
			bs.slotLayer = 1;
			smokeMgr->AddParticle(bs);
		}
	}

	// 6. 爆発跡地に5.5秒間燃え続ける残り火ゾーンを登録
	activeFirePatches_.push_back({ bPos, 5.5f, 0.0f });

	// 7. 爆風範囲ダメージ：周囲8mの敵を一掃、標的＆木箱を破壊、8.8m以内の別ドラム缶を連鎖誘爆！
	constexpr float kEnemyBlastRadSq  = 8.2f * 8.2f;
	constexpr float kTargetBlastRadSq = 6.5f * 6.5f;
	constexpr float kCrateBlastRadSq  = 6.0f * 6.0f;
	constexpr float kChainBarrelRadSq = 8.8f * 8.8f;

	for (auto& enemy : scene_->GetEnemies())
	{
		if (!enemy || enemy->IsDead()) continue;
		const Vector3 ePos = enemy->GetPosition();
		const float dx = ePos.x - bPos.x, dz = ePos.z - bPos.z;
		if (dx * dx + dz * dz <= kEnemyBlastRadSq)
		{
			for (int h = 0; h < 8 && !enemy->IsDead(); ++h) enemy->OnHit(bPos);
			if (enemy->IsDead())
			{
				RaidStats::GetInstance().enemiesKilled++;
				scene_->AddFloatingText(ePos + Vector3{ 0.0f, 1.55f, 0.0f }, "BARREL BLAST!", { 1.0f, 0.65f, 0.15f, 1.0f }, true);
				if (scene_->lootSystem_) scene_->lootSystem_->SpawnCorpseLoot(ePos, "BLASTED SCAV");
				if (scene_->particleManager && addMgr)
				{
					const Vector3 outDir = NormalizeOrFallback(ePos - bPos, { 0.0f, 0.0f, 1.0f });
					addMgr->EmitEnemyDestroyGPUBurst(scene_->particleManager->GetRandomEngine(), ePos, outDir, scene_->particleTextureB, scene_->starburstTextureIndex_, scene_->smokeTextureIndex_);
				}
			}
		}
	}

	for (auto& target : scene_->GetTargets())
	{
		if (!target || target->IsDead()) continue;
		const Vector3 tPos = target->GetPosition();
		const float dx = tPos.x - bPos.x, dz = tPos.z - bPos.z;
		if (dx * dx + dz * dz <= kTargetBlastRadSq)
		{
			target->OnHit(99);
			if (target->IsDead()) RaidStats::GetInstance().targetsDestroyed++;
		}
	}

	for (auto& obs : scene_->GetObstacles())
	{
		if (!obs || obs.get() == barrelObs || obs->IsBroken()) continue;
		const Vector3 oPos = obs->GetPosition();
		const float dx = oPos.x - bPos.x, dz = oPos.z - bPos.z;
		const float distSq = dx * dx + dz * dz;

		if (obs->IsExplosiveBarrel() && distSq <= kChainBarrelRadSq)
		{
			obs->DamageBarrel(oPos + Vector3{ 0.0f, 0.7f, 0.0f }, NormalizeOrFallback(oPos - bPos, { 0.0f, 1.0f, 0.0f }), 0.28f);
		}
		else if (obs->IsBreakableSupplyCrate() && distSq <= kCrateBlastRadSq && scene_->lootSystem_ && scene_->player_)
		{
			bool crateBroken = false;
			for (int hit = 0; hit < 4 && !crateBroken; ++hit)
			{
				if (!scene_->lootSystem_->TryDamageBreakableCrate(
					oPos, scene_->player_.get(), addMgr, scene_->floatingTexts_, crateBroken,
					smokeMgr, scene_->soilDustTextureIndex_, scene_->debrisChipTextureIndex_, scene_->sparkStreakTextureIndex_))
				{
					break;
				}
			}
			if (crateBroken) obs->BreakObstacle(NormalizeOrFallback(oPos - bPos, { 0.0f, 0.0f, 1.0f }));
		}
	}
}

void CollisionSystem::UpdateBarrelHazards(float deltaTime)
{
	if (!scene_ || !scene_->appParticleManager_) return;

	AppParticleManager* addMgr   = scene_->appParticleManager_.get();
	AppParticleManager* smokeMgr = scene_->smokeParticleManager_ ? scene_->smokeParticleManager_.get() : nullptr;
	const uint32_t flashTex = (impactFlashTexIndex_ != UINT32_MAX && impactFlashTexIndex_ != TextureManager::kInvalidTextureIndex)
		? impactFlashTexIndex_ : scene_->particleTextureB;
	const uint32_t sparkTex = (scene_->sparkStreakTextureIndex_ != TextureManager::kInvalidTextureIndex) ? scene_->sparkStreakTextureIndex_ : flashTex;
	const uint32_t smokeTex = (scene_->wispySmokeTextureIndex_ != TextureManager::kInvalidTextureIndex) ? scene_->wispySmokeTextureIndex_ : scene_->smokeTextureIndex_;

	// 1. 被弾して引火した赤ドラム缶の「高圧トーチ火炎ジェット噴出」＆時限爆発カウントダウン
	std::vector<Obstacle*> toDetonate;
	for (auto& obs : scene_->GetObstacles())
	{
		if (!obs || !obs->IsBarrelLeaking()) continue;

		const Vector3 lPos = obs->GetBarrelLeakPos();
		const Vector3 lDir = NormalizeOrFallback(obs->GetBarrelLeakDir(), { 0.0f, 0.5f, 0.86f });

		for (int j = 0; j < 2; ++j)
		{
			AppParticle fj{};
			fj.transform.Initialize();
			fj.transform.SetTranslate(lPos + lDir * (0.04f + static_cast<float>(j) * 0.08f));
			const float sc = RandRange(0.15f, 0.28f);
			fj.scaleAspect = { 1.15f, 1.15f };
			fj.initialScale = sc;
			fj.targetScale = sc * 2.2f;
			fj.transform.SetScale({ sc * 1.15f, sc * 1.15f, 1.0f });
			fj.velocity = lDir * RandRange(4.8f, 8.8f) + Vector3{ RandRange(-0.7f, 0.7f), RandRange(0.5f, 1.8f), RandRange(-0.7f, 0.7f) };
			fj.color = (j == 0) ? Vector4{ 1.0f, 0.94f, 0.58f, 0.95f } : Vector4{ 1.0f, 0.52f, 0.12f, 0.90f };
			fj.endColor = { 0.90f, 0.12f, 0.02f, 0.0f };
			fj.hasColorShift = true;
			fj.lifeTime = RandRange(0.14f, 0.26f);
			fj.textureIndex = flashTex;
			fj.drag = 3.2f;
			fj.slotLayer = 2;
			addMgr->AddParticle(fj);
		}

		if (std::rand() % 2 == 0)
		{
			EmitImpactFragment(addMgr, lPos, lDir * RandRange(5.5f, 11.0f) + Vector3{ RandRange(-1.8f, 1.8f), RandRange(0.8f, 3.0f), RandRange(-1.8f, 1.8f) },
				{ 1.0f, 0.86f, 0.35f, 0.98f }, { 1.0f, 0.20f, 0.02f, 0.0f }, 0.045f, 0.14f, RandRange(0.20f, 0.42f), 11.5f, 1.6f, 0.55f, sparkTex, 0.22f, 2);
		}

		if (smokeMgr && (std::rand() % 2 == 0) && smokeTex != TextureManager::kInvalidTextureIndex)
		{
			AppParticle ls{};
			ls.transform.Initialize();
			ls.transform.SetTranslate(lPos + lDir * 0.28f);
			const float sc = RandRange(0.18f, 0.30f);
			ls.scaleAspect = { 1.0f, 1.2f };
			ls.initialScale = sc;
			ls.targetScale = sc * 2.8f;
			ls.transform.SetScale({ sc, sc * 1.2f, 1.0f });
			ls.velocity = { lDir.x * 1.2f + RandRange(-0.4f, 0.4f), RandRange(1.5f, 2.8f), lDir.z * 1.2f + RandRange(-0.4f, 0.4f) };
			ls.color = { 0.18f, 0.18f, 0.20f, 0.52f };
			ls.endColor = { 0.24f, 0.24f, 0.26f, 0.0f };
			ls.hasColorShift = true;
			ls.lifeTime = RandRange(0.75f, 1.35f);
			ls.textureIndex = smokeTex;
			ls.drag = 1.4f;
			ls.curlFreq = 2.0f;
			ls.curlAmp = 0.35f;
			ls.fadeInRatio = 0.12f;
			ls.slotLayer = 1;
			smokeMgr->AddParticle(ls);
		}

		if (obs->TickBarrelFuse(deltaTime))
		{
			toDetonate.push_back(obs.get());
		}
	}

	for (Obstacle* b : toDetonate)
	{
		DetonateFuelBarrel(b, { 0.0f, 0.0f, 1.0f });
	}

	// 2. 爆発後の地面に残る炎上燃料プール＆舞い上がる残り火（5.5秒間持続）
	for (auto it = activeFirePatches_.begin(); it != activeFirePatches_.end(); )
	{
		it->remainTime -= deltaTime;
		if (it->remainTime <= 0.0f)
		{
			it = activeFirePatches_.erase(it);
			continue;
		}

		it->emitTimer += deltaTime;
		if (it->emitTimer >= 0.045f)
		{
			it->emitTimer = 0.0f;
			const float intensity = std::clamp(it->remainTime / 1.8f, 0.20f, 1.0f);

			for (int f = 0; f < 2; ++f)
			{
				const float a = RandRange(0.0f, kTwoPi);
				const float r = RandRange(0.15f, 2.35f * intensity);
				const Vector3 fPos = it->pos + Vector3{ std::cos(a) * r, 0.08f, std::sin(a) * r };
				const float sc = RandRange(0.22f, 0.44f) * intensity;
				AppParticle gf{};
				gf.transform.Initialize();
				gf.transform.SetTranslate(fPos);
				gf.scaleAspect = { 0.88f, 1.35f };
				gf.initialScale = sc;
				gf.targetScale = sc * 1.65f;
				gf.transform.SetScale({ sc * 0.88f, sc * 1.35f, 1.0f });
				gf.velocity = { RandRange(-0.35f, 0.35f), RandRange(0.95f, 2.15f), RandRange(-0.35f, 0.35f) };
				gf.color = { 1.0f, 0.65f, 0.16f, 0.82f * intensity };
				gf.endColor = { 0.88f, 0.12f, 0.02f, 0.0f };
				gf.hasColorShift = true;
				gf.lifeTime = RandRange(0.26f, 0.48f);
				gf.textureIndex = flashTex;
				gf.curlFreq = 2.6f;
				gf.curlAmp = 0.32f;
				gf.slotLayer = 2;
				addMgr->AddParticle(gf);
			}

			// 空へ舞い上がるオレンジ色の火の粉エンバー
			{
				const float a = RandRange(0.0f, kTwoPi);
				const float r = RandRange(0.1f, 2.1f);
				AppParticle em{};
				em.transform.Initialize();
				em.transform.SetTranslate(it->pos + Vector3{ std::cos(a) * r, 0.18f, std::sin(a) * r });
				const float sc = RandRange(0.032f, 0.062f);
				em.scaleAspect = { 0.8f, 1.3f };
				em.initialScale = sc;
				em.targetScale = sc * 0.35f;
				em.transform.SetScale({ sc * 0.8f, sc * 1.3f, 1.0f });
				em.velocity = { RandRange(-0.6f, 0.6f), RandRange(1.6f, 3.4f), RandRange(-0.6f, 0.6f) };
				em.color = { 1.0f, 0.82f, 0.28f, 0.95f * intensity };
				em.endColor = { 1.0f, 0.20f, 0.02f, 0.0f };
				em.hasColorShift = true;
				em.lifeTime = RandRange(0.85f, 1.65f);
				em.textureIndex = sparkTex;
				em.curlFreq = 3.2f;
				em.curlAmp = 0.65f;
				em.twinkleSpeed = 18.0f;
				em.twinklePhase = a;
				em.slotLayer = 2;
				addMgr->AddParticle(em);
			}

			if (smokeMgr && smokeTex != TextureManager::kInvalidTextureIndex && (std::rand() % 2 == 0))
			{
				const float a = RandRange(0.0f, kTwoPi);
				const float r = RandRange(0.2f, 1.8f);
				const float sc = RandRange(0.24f, 0.42f);
				AppParticle ps{};
				ps.transform.Initialize();
				ps.transform.SetTranslate(it->pos + Vector3{ std::cos(a) * r, 0.32f, std::sin(a) * r });
				ps.scaleAspect = { 1.0f, 1.25f };
				ps.initialScale = sc;
				ps.targetScale = sc * 2.9f;
				ps.transform.SetScale({ sc, sc * 1.25f, 1.0f });
				ps.velocity = { RandRange(-0.35f, 0.35f), RandRange(1.1f, 2.2f), RandRange(-0.35f, 0.35f) };
				ps.color = { 0.16f, 0.16f, 0.18f, 0.42f * intensity };
				ps.endColor = { 0.24f, 0.24f, 0.26f, 0.0f };
				ps.hasColorShift = true;
				ps.lifeTime = RandRange(1.1f, 1.9f);
				ps.textureIndex = smokeTex;
				ps.curlFreq = 1.8f;
				ps.curlAmp = 0.38f;
				ps.fadeInRatio = 0.15f;
				ps.slotLayer = 1;
				smokeMgr->AddParticle(ps);
			}
		}
		++it;
	}
}
