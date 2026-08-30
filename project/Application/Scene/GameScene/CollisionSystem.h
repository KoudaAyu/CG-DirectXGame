#pragma once

class GamePlayScene;

/// <summary>
/// 衝突判定・物理補正システムクラス
/// ゲーム内の弾丸、キャラクター（プレイヤー・敵）、障害物（Obstacle）間の衝突判定と、めり込み補正・ダメージ適用等の衝突解決を行います。
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
	void Update();

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

private:
	GamePlayScene* scene_ = nullptr;    // ゲームプレイシーンへの参照
};
