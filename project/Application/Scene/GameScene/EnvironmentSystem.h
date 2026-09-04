#pragma once
#include <memory>
#include "Vector.h"

class GamePlayScene;

/// <summary>
/// ステージ環境・脱出進行管理システム (EnvironmentSystem)
/// 脱出ヘリパッドの判定、カウントダウン、バリア演出、祝砲Confetti、
/// および川の水流さざ波・水滴等の環境エフェクトを統括します。
/// </summary>
class EnvironmentSystem
{
public:
	/// <summary>
	/// コンストラクタ
	/// </summary>
	/// <param name="scene">対象となるGamePlayScene</param>
	explicit EnvironmentSystem(GamePlayScene* scene);
	~EnvironmentSystem() = default;

	/// <summary>
	/// 初期化処理
	/// </summary>
	void Initialize();

	/// <summary>
	/// 毎フレームの環境・脱出演出の更新
	/// </summary>
	/// <param name="deltaTime">フレーム経過時間</param>
	void Update(float deltaTime);

	/// <summary>
	/// 脱出目標と生還判定・カウントダウン・祝砲演出の更新
	/// </summary>
	/// <param name="deltaTime">フレーム経過時間</param>
	void UpdateExtractionGoal(float deltaTime);

	/// <summary>
	/// 脱出バリア球の回転・パルス鼓動・非表示制御
	/// </summary>
	void UpdateBarrier();

	/// <summary>
	/// 川の水流さざ波および水滴パーティクルの周期的放出
	/// </summary>
	/// <param name="deltaTime">フレーム経過時間</param>
	void UpdateRiverEffects(float deltaTime);

	/// <summary>
	/// ヒットエフェクトの更新
	/// </summary>
	/// <param name="deltaTime">フレーム経過時間</param>
	void UpdateHitEffect(float deltaTime);

private:
	GamePlayScene* scene_ = nullptr;

	// タイマー類（GamePlayScene から移譲・カプセル化）
	float riverWaveTimer_ = 0.0f;
	float riverSplashTimer_ = 0.0f;
	float barrierTimer_ = 0.0f;
};
