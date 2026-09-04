#pragma once
#include <memory>
#include "Vector.h"

class GamePlayScene;

/// <summary>
/// キャラクター演出コントローラー (CharacterEffectController)
/// プレイヤーの回避土煙・足元土埃、敵の警戒予兆オーラなど、
/// キャラクターのアクションに連動するエフェクト演出を管理します。
/// </summary>
class CharacterEffectController
{
public:
	/// <summary>
	/// コンストラクタ
	/// </summary>
	/// <param name="scene">対象となるGamePlayScene</param>
	explicit CharacterEffectController(GamePlayScene* scene);
	~CharacterEffectController() = default;

	/// <summary>
	/// 初期化処理
	/// </summary>
	void Initialize();

	/// <summary>
	/// 毎フレームのキャラクター演出更新
	/// </summary>
	/// <param name="deltaTime">フレーム経過時間</param>
	void Update(float deltaTime);

	/// <summary>
	/// プレイヤーのアクション連動エフェクト（回避土煙、足音土埃）の更新
	/// </summary>
	/// <param name="deltaTime">フレーム経過時間</param>
	void UpdatePlayerEffects(float deltaTime);

	/// <summary>
	/// 敵キャラクターの警戒予兆オーラ（Suspicion Aura）の更新
	/// </summary>
	/// <param name="deltaTime">フレーム経過時間</param>
	void UpdateEnemyEffects(float deltaTime);

private:
	GamePlayScene* scene_ = nullptr;

	// タイマー類（カプセル化）
	float dodgeDustTimer_ = 0.0f;
	float stepTimer_ = 0.0f;
};
