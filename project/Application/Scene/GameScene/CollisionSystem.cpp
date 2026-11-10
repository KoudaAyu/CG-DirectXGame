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
void CollisionSystem::Update()
{
	CollisionManager::GetInstance()->Update();
	ResolveObstacleCollisions();          // ① 弾丸と障害物の衝突を最優先判定
	ResolveBulletCollisions();            // ② 生き残った弾丸とキャラクター・標的の衝突
	ResolveCharacterObstacleCollisions(); // ③ キャラクターと障害物の衝突
	ResolveContactDamage();               // ④ プレイヤーと敵の直接接触によるダメージ
}

// 障害物の材質（鋼鉄・コンクリート・土嚢・木材）と実入射角に応じたリアル着弾＆跳弾演出
void CollisionSystem::TriggerObstacleHitEffect(const Vector3& hitPos, const Vector3& bulletDir, const Vector3& surfaceNormal, const Obstacle* hitObstacle)
{
	if (!scene_ || !scene_->particleManager || !scene_->appParticleManager_) return;

	if (impactFlashTexIndex_ == UINT32_MAX)
	{
		impactFlashTexIndex_ = TextureManager::GetInstance()->Load(Impact::kMuzzleFlashTexPath);
		if (impactFlashTexIndex_ == TextureManager::kInvalidTextureIndex)
		{
			impactFlashTexIndex_ = scene_->particleTextureB;
		}
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

	enum class SurfaceMat { Metal, Concrete, SandDirt, Wood };
	SurfaceMat mat = SurfaceMat::Metal;
	bool isTree = false;

	if (hitObstacle)
	{
		const std::string& mf = hitObstacle->GetModelFilename();
		if (mf.find("sandbag") != std::string::npos)
		{
			mat = SurfaceMat::SandDirt;
		}
		else if (mf.find("tree") != std::string::npos)
		{
			mat = SurfaceMat::Wood;
			isTree = true;
		}
		else if (mf.find("crate") != std::string::npos || mf.find("bridge") != std::string::npos ||
		         mf.find("watchtower") != std::string::npos)
		{
			mat = SurfaceMat::Wood;
		}
		else if ((mf.find("fence") != std::string::npos && hitPos.y < Geometry::kFenceConcreteMaxY) ||
		         mf.find("rock") != std::string::npos || mf.find("stone") != std::string::npos ||
		         mf.find("concrete") != std::string::npos)
		{
			mat = SurfaceMat::Concrete;
		}
		else
		{
			mat = SurfaceMat::Metal;
		}

		if (scene_->player_)
		{
			const char* impPath = Impact::kSeMetalPath;
			if (mat == SurfaceMat::Concrete)      impPath = Impact::kSeConcretePath;
			else if (mat == SurfaceMat::SandDirt) impPath = Impact::kSeDirtPath;
			else if (mat == SurfaceMat::Wood)     impPath = Impact::kSeWoodPath;

			TacticalAudio::Play3D(
				impPath, hitPos, scene_->player_->GetPosition(),
				Impact::kAudioVolume, Impact::kAudioMinDist, Impact::kAudioMaxDist,
				Impact::kAudioCooldownMs, "obs_impact");
		}
	}

	AppParticleManager* addMgr = scene_->appParticleManager_.get();
	AppParticleManager* smokeMgr = scene_->smokeParticleManager_ ? scene_->smokeParticleManager_.get() : nullptr;
	const uint32_t flashTex = (impactFlashTexIndex_ != TextureManager::kInvalidTextureIndex) ? impactFlashTexIndex_ : scene_->particleTextureB;
	const uint32_t sparkTex = (scene_->sparkStreakTextureIndex_ != TextureManager::kInvalidTextureIndex) ? scene_->sparkStreakTextureIndex_ : flashTex;
	const uint32_t dustTex  = (scene_->soilDustTextureIndex_ != TextureManager::kInvalidTextureIndex) ? scene_->soilDustTextureIndex_ : scene_->wispySmokeTextureIndex_;
	const uint32_t chipTex  = (scene_->debrisChipTextureIndex_ != TextureManager::kInvalidTextureIndex) ? scene_->debrisChipTextureIndex_ : scene_->particleTextureB;
	const uint32_t leafTex  = (scene_->leafTextureIndex_ != TextureManager::kInvalidTextureIndex) ? scene_->leafTextureIndex_ : chipTex;

	auto emitSmokePuff = [&](const Impact::SmokePuffPreset& preset)
	{
		if (!smokeMgr || dustTex == TextureManager::kInvalidTextureIndex) return;
		for (int i = 0; i < preset.count; ++i)
		{
			AppParticle sm{};
			sm.transform.Initialize();
			sm.transform.SetTranslate(hitPos + N * Impact::kSmokeNormalOffset);
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

	if (mat == SurfaceMat::Metal)
	{
		EmitImpactFragment(
			addMgr, hitPos + N * Impact::kFlashNormalOffset, { 0.0f, 0.0f, 0.0f },
			Impact::kMetalFlashStart, Impact::kMetalFlashEnd,
			Impact::kMetalFlashSize, Impact::kMetalFlashSize, Impact::kMetalFlashLife,
			0.0f, 0.0f, 0.0f, flashTex);

		for (int i = 0; i < Impact::kMetalSparkCount; ++i)
		{
			const Vector3 dir = {
				reflDir.x + RandRange(-Impact::kMetalSparkJitterXZ, Impact::kMetalSparkJitterXZ),
				reflDir.y + RandRange(Impact::kMetalSparkJitterYMin, Impact::kMetalSparkJitterYMax),
				reflDir.z + RandRange(-Impact::kMetalSparkJitterXZ, Impact::kMetalSparkJitterXZ)
			};
			const float spd = RandRange(Impact::kMetalSparkSpeedMin, Impact::kMetalSparkSpeedMax);
			const Vector4& startC = (i % Impact::kMetalBrightSparkPeriod == 0)
				? Impact::kMetalSparkBrightCol
				: Impact::kMetalSparkGoldCol;
			EmitImpactFragment(
				addMgr, hitPos + N * Impact::kSparkNormalOffset, dir * spd,
				startC, Impact::kMetalSparkEndCol,
				RandRange(Impact::kMetalSparkWMin, Impact::kMetalSparkWMax),
				RandRange(Impact::kMetalSparkLMin, Impact::kMetalSparkLMax),
				RandRange(Impact::kMetalSparkLifeMin, Impact::kMetalSparkLifeMax),
				Impact::kMetalSparkGravity, Impact::kMetalSparkDrag, Impact::kMetalSparkBounce,
				sparkTex, Impact::kMetalSparkEndScale, 2);
		}

		if (smokeMgr)
		{
			for (int i = 0; i < Impact::kMetalChipCount; ++i)
			{
				const Vector3 vel = N * RandRange(Impact::kMetalChipNormalSpdMin, Impact::kMetalChipNormalSpdMax)
					+ Vector3{
						RandRange(-Impact::kMetalChipHorizJitter, Impact::kMetalChipHorizJitter),
						RandRange(Impact::kMetalChipUpMin, Impact::kMetalChipUpMax),
						RandRange(-Impact::kMetalChipHorizJitter, Impact::kMetalChipHorizJitter)
					};
				EmitImpactFragment(
					smokeMgr, hitPos + N * Impact::kChipNormalOffset, vel,
					Impact::kMetalChipStartCol, Impact::kMetalChipEndCol,
					RandRange(Impact::kMetalChipWMin, Impact::kMetalChipWMax),
					RandRange(Impact::kMetalChipLMin, Impact::kMetalChipLMax),
					RandRange(Impact::kMetalChipLifeMin, Impact::kMetalChipLifeMax),
					Impact::kMetalChipGravity, 1.0f, Impact::kMetalChipBounce,
					chipTex, Impact::kMetalChipEndScale, 2);
			}
		}
		emitSmokePuff(Impact::kMetalSmoke);
	}
	else if (mat == SurfaceMat::Concrete)
	{
		EmitImpactFragment(
			addMgr, hitPos + N * Impact::kFlashNormalOffset, { 0.0f, 0.0f, 0.0f },
			Impact::kConcFlashStart, Impact::kConcFlashEnd,
			Impact::kConcFlashSize, Impact::kConcFlashSize, Impact::kConcFlashLife,
			0.0f, 0.0f, 0.0f, flashTex);

		for (int i = 0; i < Impact::kConcSparkCount; ++i)
		{
			const Vector3 vel = reflDir * RandRange(Impact::kConcSparkReflSpdMin, Impact::kConcSparkReflSpdMax)
				+ Vector3{
					RandRange(-Impact::kConcSparkHorizJitter, Impact::kConcSparkHorizJitter),
					RandRange(Impact::kConcSparkUpMin, Impact::kConcSparkUpMax),
					RandRange(-Impact::kConcSparkHorizJitter, Impact::kConcSparkHorizJitter)
				};
			EmitImpactFragment(
				addMgr, hitPos + N * Impact::kSparkNormalOffset, vel,
				Impact::kConcSparkStartCol, Impact::kConcSparkEndCol,
				Impact::kConcSparkWidth, Impact::kConcSparkLength,
				RandRange(Impact::kConcSparkLifeMin, Impact::kConcSparkLifeMax),
				Impact::kConcSparkGravity, Impact::kConcSparkDrag, Impact::kConcSparkBounce,
				sparkTex, Impact::kConcSparkEndScale, 2);
		}

		if (smokeMgr)
		{
			for (int i = 0; i < Impact::kConcChipCount; ++i)
			{
				const Vector3 vel = N * RandRange(Impact::kConcChipNormalSpdMin, Impact::kConcChipNormalSpdMax)
					+ Vector3{
						RandRange(-Impact::kConcChipHorizJitter, Impact::kConcChipHorizJitter),
						RandRange(Impact::kConcChipUpMin, Impact::kConcChipUpMax),
						RandRange(-Impact::kConcChipHorizJitter, Impact::kConcChipHorizJitter)
					};
				const float g = RandRange(Impact::kConcToneMin, Impact::kConcToneMax);
				EmitImpactFragment(
					smokeMgr, hitPos + N * Impact::kChipNormalOffset, vel,
					{ g, g * Impact::kConcGreenFactor, g * Impact::kConcBlueFactor, Impact::kConcStartAlpha },
					{ g * Impact::kConcEndRedDim, g * Impact::kConcEndGreenDim, g * Impact::kConcEndBlueDim, 0.0f },
					RandRange(Impact::kConcChipWMin, Impact::kConcChipWMax),
					RandRange(Impact::kConcChipLMin, Impact::kConcChipLMax),
					RandRange(Impact::kConcChipLifeMin, Impact::kConcChipLifeMax),
					Impact::kConcChipGravity, 1.0f, Impact::kConcChipBounce,
					chipTex, Impact::kConcChipEndScale, 2);
			}
		}
		emitSmokePuff(Impact::kConcreteSmoke);
	}
	else if (mat == SurfaceMat::SandDirt)
	{
		emitSmokePuff(Impact::kSandDirtSmoke);

		if (smokeMgr)
		{
			for (int i = 0; i < Impact::kSandClodCount; ++i)
			{
				const Vector3 vel = N * RandRange(Impact::kSandNormalSpdMin, Impact::kSandNormalSpdMax)
					+ Vector3{
						RandRange(-Impact::kSandHorizJitter, Impact::kSandHorizJitter),
						RandRange(Impact::kSandUpMin, Impact::kSandUpMax),
						RandRange(-Impact::kSandHorizJitter, Impact::kSandHorizJitter)
					};
				const float tone = RandRange(Impact::kSandToneMin, Impact::kSandToneMax);
				EmitImpactFragment(
					smokeMgr, hitPos + N * Impact::kChipNormalOffset, vel,
					{ Impact::kSandBaseStart.x * tone, Impact::kSandBaseStart.y * tone, Impact::kSandBaseStart.z * tone, Impact::kSandBaseStart.w },
					{ Impact::kSandBaseEndRGB.x * tone, Impact::kSandBaseEndRGB.y * tone, Impact::kSandBaseEndRGB.z * tone, 0.0f },
					RandRange(Impact::kSandWMin, Impact::kSandWMax),
					RandRange(Impact::kSandLMin, Impact::kSandLMax),
					RandRange(Impact::kSandLifeMin, Impact::kSandLifeMax),
					Impact::kSandGravity, Impact::kSandDrag, Impact::kSandBounce,
					chipTex, Impact::kSandEndScale, 2);
			}
		}
	}
	else // SurfaceMat::Wood
	{
		if (smokeMgr)
		{
			for (int i = 0; i < Impact::kWoodSplinterCount; ++i)
			{
				const Vector3 vel = N * RandRange(Impact::kWoodNormalSpdMin, Impact::kWoodNormalSpdMax)
					+ Vector3{
						RandRange(-Impact::kWoodHorizJitter, Impact::kWoodHorizJitter),
						RandRange(Impact::kWoodUpMin, Impact::kWoodUpMax),
						RandRange(-Impact::kWoodHorizJitter, Impact::kWoodHorizJitter)
					};
				const float tone = RandRange(Impact::kWoodToneMin, Impact::kWoodToneMax);
				const bool isBark = (isTree && (i % Impact::kWoodBarkPeriod == 0));
				const Vector4& baseC = isBark ? Impact::kWoodBarkBase : Impact::kWoodHeartBase;
				const Vector4 startC = { baseC.x * tone, baseC.y * tone, baseC.z * tone, baseC.w };
				EmitImpactFragment(
					smokeMgr, hitPos + N * Impact::kChipNormalOffset, vel,
					startC,
					{ startC.x * Impact::kWoodEndDim, startC.y * Impact::kWoodEndDim, startC.z * Impact::kWoodEndDim, 0.0f },
					RandRange(Impact::kWoodWMin, Impact::kWoodWMax),
					RandRange(Impact::kWoodLMin, Impact::kWoodLMax),
					RandRange(Impact::kWoodLifeMin, Impact::kWoodLifeMax),
					Impact::kWoodGravity, Impact::kWoodDrag, Impact::kWoodBounce,
					chipTex, Impact::kWoodEndScale, 2);
			}

			// 針葉樹・白樺への被弾時は衝撃で枝葉が舞い落ちる
			if (isTree && leafTex != TextureManager::kInvalidTextureIndex)
			{
				for (int i = 0; i < Impact::kTreeLeafCount; ++i)
				{
					AppParticle lf{};
					lf.transform.Initialize();
					lf.transform.SetTranslate(hitPos + Vector3{
						RandRange(-Impact::kLeafSpawnJitterXZ, Impact::kLeafSpawnJitterXZ),
						RandRange(Impact::kLeafSpawnOffsetYMin, Impact::kLeafSpawnOffsetYMax),
						RandRange(-Impact::kLeafSpawnJitterXZ, Impact::kLeafSpawnJitterXZ)
					});
					lf.transform.SetRotate({ 0.0f, 0.0f, RandRange(0.0f, kTwoPi) });
					const float sc = RandRange(Impact::kLeafScaleMin, Impact::kLeafScaleMax);
					lf.scaleAspect = Impact::kLeafAspect;
					lf.initialScale = sc;
					lf.targetScale = sc * Impact::kLeafTargetScaleRatio;
					lf.transform.SetScale({ sc * Impact::kLeafAspect.x, sc * Impact::kLeafAspect.y, 1.0f });
					lf.velocity = N * RandRange(Impact::kLeafNormalSpdMin, Impact::kLeafNormalSpdMax)
						+ Vector3{
							RandRange(-Impact::kLeafHorizJitter, Impact::kLeafHorizJitter),
							RandRange(Impact::kLeafUpMin, Impact::kLeafUpMax),
							RandRange(-Impact::kLeafHorizJitter, Impact::kLeafHorizJitter)
						};
					const float gTone = RandRange(Impact::kLeafToneMin, Impact::kLeafToneMax);
					lf.color = {
						Impact::kLeafStartBase.x * gTone,
						Impact::kLeafStartBase.y * gTone,
						Impact::kLeafStartBase.z * gTone,
						Impact::kLeafStartBase.w
					};
					lf.endColor = {
						Impact::kLeafEndBaseRGB.x * gTone,
						Impact::kLeafEndBaseRGB.y * gTone,
						Impact::kLeafEndBaseRGB.z * gTone,
						0.0f
					};
					lf.hasColorShift = true;
					lf.lifeTime = RandRange(Impact::kLeafLifeMin, Impact::kLeafLifeMax);
					lf.currentTime = 0.0f;
					lf.textureIndex = leafTex;
					lf.gravity = Impact::kLeafGravity;
					lf.drag = Impact::kLeafDrag;
					lf.wobbleFreq = Impact::kLeafWobbleFreq;
					lf.wobbleAmp = Impact::kLeafWobbleAmp;
					lf.wobbleAxis = Impact::kLeafWobbleAxis;
					lf.angularVelocity = RandRange(-Impact::kLeafAngularVel, Impact::kLeafAngularVel);
					lf.slotLayer = 2;
					smokeMgr->AddParticle(lf);
				}
			}
		}

		for (int i = 0; i < Impact::kWoodSparkCount; ++i)
		{
			const Vector3 vel = reflDir * RandRange(Impact::kWoodSparkReflSpdMin, Impact::kWoodSparkReflSpdMax)
				+ Vector3{
					RandRange(-Impact::kWoodSparkHorizJitter, Impact::kWoodSparkHorizJitter),
					RandRange(Impact::kWoodSparkUpMin, Impact::kWoodSparkUpMax),
					RandRange(-Impact::kWoodSparkHorizJitter, Impact::kWoodSparkHorizJitter)
				};
			EmitImpactFragment(
				addMgr, hitPos + N * Impact::kSparkNormalOffset, vel,
				Impact::kWoodSparkStartCol, Impact::kWoodSparkEndCol,
				Impact::kWoodSparkWidth, Impact::kWoodSparkLength,
				RandRange(Impact::kWoodSparkLifeMin, Impact::kWoodSparkLifeMax),
				Impact::kWoodSparkGravity, Impact::kWoodSparkDrag, Impact::kWoodSparkBounce,
				sparkTex, Impact::kWoodSparkEndScale, 2);
		}

		emitSmokePuff(Impact::kWoodSmoke);
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
