#pragma once
#include <cstdint>
#include <vector>
#include "Vector.h"

class GamePlayScene;
class Obstacle;

/// <summary>
/// 衝突判定・物理補正システムクラス
/// ゲーム内の弾丸、キャラクター（プレイヤー・敵）、障害物（Obstacle）間の衝突判定と、材質別着弾・跳弾演出および衝突解決を行います。
/// </summary>
class CollisionSystem
{
public:
	/// <summary>
	/// コンストラクタ
	/// </summary>
	/// <param name="scene">呼び出し元のゲームプレイシーンへのポインタ</param>
	CollisionSystem(GamePlayScene* scene);
	~CollisionSystem() = default;

	/// <summary>
	/// 各種衝突判定と補正処理をフレームごとに実行します
	/// </summary>
	void Update(float deltaTime = 1.0f / 60.0f);

private:
	/// <summary>
	/// 弾丸（Bullet）とキャラクター（Player, Enemy）の衝突判定とダメージ適用処理
	/// </summary>
	void ResolveBulletCollisions();

	/// <summary>
	/// キャラクターおよび弾丸と障害物（Obstacle）の衝突判定とめり込み補正・演出（火花・エフェクト）処理
	/// </summary>
	void ResolveObstacleCollisions();

	/// <summary>
	/// プレイヤーと敵キャラクターの接触によるダメージ適用処理
	/// </summary>
	void ResolveContactDamage();

	/// <summary>
	/// プレイヤーおよび移動敵と障害物(BoxCollider)との手動押し出し解決処理
	/// </summary>
	void ResolveCharacterObstacleCollisions();

	/// <summary>
	/// 障害物の材質（鋼鉄・コンクリート・土嚢・木材）と入射角に応じたリアルな着弾・跳弾エフェクトを発生させます
	/// </summary>
	void TriggerObstacleHitEffect(const Vector3& hitPos, const Vector3& bulletDir, const Vector3& surfaceNormal, const Obstacle* hitObstacle = nullptr);

	/// <summary>
	/// シーン内の全障害物（Obstacle）とレイの精密交差判定を行います（OBB法線・対象物取得対応）
	/// </summary>
	bool RaycastObstacles(const Vector3& rayStart, const Vector3& rayDir, float maxDist, float& outHitDist, Vector3& outHitPoint, Vector3* outHitNormal = nullptr, const Obstacle** outHitObstacle = nullptr);
	void UpdateBarrelHazards(float deltaTime);
	void DetonateFuelBarrel(Obstacle* barrelObs, const Vector3& blastDir);

private:
	struct ScorchedFirePatch
	{
		Vector3 pos{};
		float remainTime = 0.0f;
		float emitTimer = 0.0f;
	};

	GamePlayScene* scene_ = nullptr;    // ゲームプレイシーンへの参照
	std::vector<ScorchedFirePatch> activeFirePatches_;
	uint32_t impactFlashTexIndex_  = UINT32_MAX;
	uint32_t woodSplinterTexIndex_ = UINT32_MAX;
	uint32_t pineNeedleTexIndex_   = UINT32_MAX;
	uint32_t ricochetStarTexIndex_ = UINT32_MAX;
};
